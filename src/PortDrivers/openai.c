/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "openai.h"
#include "dx_utilities.h"
#include "parson.h"
#include <curl/curl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef AZURE_SPHERE
#include <applibs/storage.h>
#endif // AZURE_SPHERE

// 30k buffer for OpenAI message payload
#define OPENAI_BUFFER_SIZE 30720
static char openai_buffer[OPENAI_BUFFER_SIZE];
static int buffer_index = 0;

// Response buffer system (exactly like file_io.c)
#define RESPONSE_CHUNK_SIZE 256

typedef struct {
    char chunk_buffer[RESPONSE_CHUNK_SIZE];
    size_t chunk_bytes_available;
    size_t chunk_position;
    bool response_complete;
    bool has_data;
} OPENAI_RESPONSE_T;

static OPENAI_RESPONSE_T openai_response;
pthread_mutex_t openai_response_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t openai_data_available_cond = PTHREAD_COND_INITIALIZER;  // Consumer waits for data
pthread_cond_t openai_data_consumed_cond = PTHREAD_COND_INITIALIZER;   // Producer waits for consumption

static int stream_openai(struct curl_slist *headers, const char *postData, long timeout);
static void *openai_thread(void *arg);
static bool send_openai_message(void);

static const char *openai_endpoint = "https://api.openai.com/v1/chat/completions";
static struct curl_slist *headers  = NULL;
static bool streaming              = false;

enum OPENAI_STATUS
{
    OPENAI_END_OF_STREAM,
    OPENAI_WAITING,
    OPENAI_DATA_READY,
    // OPENAI_FAILED
};

typedef struct
{
    // bool end_of_stream;
    char content[128];
    int content_length;
    int content_index;
    char last_finish_reason[10];
    int last_finish_reason_length;
    char system_message[1024];
    enum OPENAI_STATUS status;
} OPENAI_T;

static OPENAI_T openai;

pthread_mutex_t openai_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_openai(const char *openai_api_key)
{
    if (!headers)
    {
        char auth[256];  // Increased buffer size for longer API keys
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", openai_api_key);

        headers = curl_slist_append(headers, auth);
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        // Debug: Print the full auth header to verify it's complete
        printf("=== OpenAI Auth Header ===\n");
        printf("%s\n", auth);
        printf("=== End Auth Header ===\n");
        fflush(stdout);
    }
}

// Function to reset the buffer
void reset_openai_buffer(void)
{
    memset(openai_buffer, 0, OPENAI_BUFFER_SIZE);
    buffer_index = 0;
}

// Function to reset the response buffer
void reset_openai_response(void)
{
    pthread_mutex_lock(&openai_response_mutex);
    
    // Clear entire buffer
    memset(openai_response.chunk_buffer, 0, RESPONSE_CHUNK_SIZE);
    
    openai_response.chunk_bytes_available = 0;
    openai_response.chunk_position = 0;
    openai_response.response_complete = false;
    openai_response.has_data = false;
    
    pthread_mutex_unlock(&openai_response_mutex);
}

// Function to add response data to buffer (called from callback - blocks like file_io.c)
void add_response_data(const char* content)
{
    if (!content || strlen(content) == 0) {
        return;
    }

    pthread_mutex_lock(&openai_response_mutex);
    
    // Wait for current chunk to be consumed (exactly like file_io.c)
    while (openai_response.chunk_bytes_available > 0) {
        // printf("add_response_data: Waiting for chunk to be consumed...\n");
        pthread_cond_wait(&openai_data_consumed_cond, &openai_response_mutex);
    }
    
    size_t content_len = strlen(content);
    size_t bytes_to_copy = (content_len >= RESPONSE_CHUNK_SIZE) ? RESPONSE_CHUNK_SIZE - 1 : content_len;
    
    // printf("add_response_data: Adding \"%s\" (len=%zu)\n", content, bytes_to_copy);
    
    // Copy content to buffer (REPLACE not append)
    memcpy(openai_response.chunk_buffer, content, bytes_to_copy);
    openai_response.chunk_buffer[bytes_to_copy] = '\0';
    
    // Set new chunk parameters
    openai_response.chunk_bytes_available = bytes_to_copy;
    openai_response.chunk_position = 0;
    openai_response.has_data = true;
    
    // Signal that new data is available
    pthread_cond_signal(&openai_data_available_cond);
    pthread_mutex_unlock(&openai_response_mutex);
}





// Function to mark response as complete
void mark_response_complete(void)
{
    pthread_mutex_lock(&openai_response_mutex);
    openai_response.response_complete = true;
    pthread_cond_signal(&openai_data_available_cond);
    pthread_mutex_unlock(&openai_response_mutex);
}

// Function to add byte to buffer and process when null terminator is received
void add_byte_to_buffer(uint8_t byte)
{
    if (buffer_index < OPENAI_BUFFER_SIZE - 1)
    {
        openai_buffer[buffer_index] = byte;
        buffer_index++;
    }
    
    // If null terminator received, process the complete payload
    if (byte == 0x00)
    {
        openai_buffer[buffer_index - 1] = '\0'; // Ensure null termination
        printf("OpenAI payload received (%d bytes):\n%s\n", buffer_index - 1, openai_buffer);
        
        // Call OpenAI API with the complete payload
        if (headers && !streaming)
        {
            openai.status = OPENAI_WAITING;
            // pthread_mutex_unlock(&openai_mutex);  // Commented out to match callback changes
            dx_startThreadDetached(openai_thread, NULL, "OpenAI Thread");
        }
    }
}

// Function to send OpenAI message with current buffer content
bool send_openai_message(void)
{
    if (headers && !streaming && buffer_index > 0)
    {
        openai.status = OPENAI_WAITING;
        dx_startThreadDetached(openai_thread, NULL, "OpenAI Thread");
        return true;
    }
    return false;
}

size_t openai_output(int port, uint8_t data, char *buffer, size_t buffer_length)
{
    size_t len = 0;
    switch (port)
    {
        case 120: // Reset buffer index to 0
            reset_openai_buffer();
            break;
            
        case 121: // Add byte to buffer until null received
            add_byte_to_buffer(data);
            break;

        case 122: // Reset response buffer
            reset_openai_response();
            break;
            
        case 123: // Reserved for future use
            break;
            
        case 124: // Reserved for future use
            break;

        case 125: // Cancel ChatGPT stream
            break;
    }

    return len;
}

uint8_t openai_input(uint8_t port)
{
    uint8_t retVal = 0;

    switch (port)
    {
        case 120: // Send ChatGPT message and get initial status
            if (send_openai_message())
            {
                retVal = 1; // Success
            }
            else
            {
                retVal = 0; // Error
            }
            break;
            
        case 121: // Get buffer length low byte
            retVal = (uint8_t)(buffer_index & 0xFF);
            break;
            
        case 122: // Get buffer length high byte  
            retVal = (uint8_t)((buffer_index >> 8) & 0xFF);
            break;

        case 123: // Get OpenAI response status (exactly like port 33 in file_io.c)
            pthread_mutex_lock(&openai_response_mutex);
            if (openai_response.chunk_bytes_available > 0 && openai_response.chunk_position < openai_response.chunk_bytes_available)
            {
                retVal = 2; // OPENAI_DATA_READY (matches WEBGET_DATA_READY)
            }
            else if (openai_response.response_complete)
            {
                retVal = 0; // OPENAI_EOF (matches WEBGET_EOF)
            }
            else
            {
                retVal = 1; // OPENAI_WAITING (matches WEBGET_WAITING)
            }
            pthread_mutex_unlock(&openai_response_mutex);
            break;
            
        case 124: // Read one byte from response buffer (exactly like file_io.c)
            pthread_mutex_lock(&openai_response_mutex);
            if (openai_response.chunk_bytes_available > 0 && openai_response.chunk_position < openai_response.chunk_bytes_available)
            {
                // Return byte from current chunk
                retVal = openai_response.chunk_buffer[openai_response.chunk_position];
                // printf("Port 124: Read '%c' (0x%02x) pos=%zu/%zu\n", retVal, retVal, 
                //        openai_response.chunk_position, openai_response.chunk_bytes_available);
                openai_response.chunk_position++;
                
                // Check if we've consumed the entire chunk
                if (openai_response.chunk_position >= openai_response.chunk_bytes_available)
                {
                    // printf("Port 124: Chunk consumed, signaling for more data\n");
                    // Reset chunk for next fill (exactly like file_io.c)
                    openai_response.chunk_bytes_available = 0;
                    openai_response.chunk_position = 0;
                    openai_response.has_data = false;
                    
                    // Signal that chunk buffer is now available for new data
                    pthread_cond_signal(&openai_data_consumed_cond);
                }
            }
            else
            {
                // No data in buffer
                retVal = 0x00;
                printf("Port 124: No data available, returning 0\n");
            }
            pthread_mutex_unlock(&openai_response_mutex);
            break;

        case 125: // Check if response stream is complete (1 = complete, 0 = ongoing)
            pthread_mutex_lock(&openai_response_mutex);
            retVal = openai_response.response_complete ? 1 : 0;
            pthread_mutex_unlock(&openai_response_mutex);
            break;
            
        case 126: // Get finish reason length
            retVal = (uint8_t)openai.last_finish_reason_length;
            break;
            
        case 127: // Get finish reason character by character
            {
                static int finish_reason_pos = 0;
                if (finish_reason_pos < openai.last_finish_reason_length) {
                    retVal = (uint8_t)openai.last_finish_reason[finish_reason_pos];
                    finish_reason_pos++;
                } else {
                    retVal = 0;
                    finish_reason_pos = 0; // Reset for next call
                }
            }
            break;
    }

    return retVal;
}

#ifdef __APPLE__

/*
 * A pthread_mutex_timedlock() impl for OSX/macOS, which lacks the real thing.
 * NOTE: Unlike the real McCoy, won't return EOWNERDEAD, EDEADLK, or EOWNERDEAD
 */
static int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abs_timeout)
{
    int rv;
    struct timespec remaining, slept, ts;
    remaining = *abs_timeout;

    long nsecs = remaining.tv_sec * 1000 * ONE_MS;
    nsecs += remaining.tv_nsec;

    while ((rv = pthread_mutex_trylock(mutex)) == EBUSY)
    {
        if (nsecs > 10 * ONE_MS)
        {
            nanosleep(&(struct timespec){0, 10 * ONE_MS}, NULL);
            nsecs -= 10 * ONE_MS;
        }
        else
        {
            nanosleep(&(struct timespec){0, nsecs}, NULL);
            nsecs = 0;
        }

        if (nsecs <= 0)
        {
            return ETIMEDOUT;
        }
    }

    return rv;
}

#endif // __APPLE__

/// @brief callback for OpenAI Streaming API
/// @param contents
/// @param size
/// @param nmemb
/// @param userp
/// @return
static size_t StreamOpenAICallback(void *contents, size_t size, size_t nmemb, void *openai)
{
    JSON_Array *choices    = NULL;
    JSON_Object *choice    = NULL;
    JSON_Object *messages  = NULL;
    JSON_Value *root_value = NULL;
    const char *content    = NULL;

    size_t realsize = size * nmemb;
    OPENAI_T *chat  = (OPENAI_T *)openai;

    struct timespec timeoutTime;
    memset(&timeoutTime, 0, sizeof(struct timespec));

#ifndef __APPLE__

    clock_gettime(CLOCK_REALTIME, &timeoutTime);
    timeoutTime.tv_sec += 3;
    timeoutTime.tv_nsec = 0;

#else // __APPLE__

    // Max wait is 500ms
    timeoutTime.tv_sec  = 2;
    timeoutTime.tv_nsec = 0;

#endif // __APPLE__

    // No need to wait here - add_response_data() handles blocking for each chunk
    // The consumer signals openai_data_consumed_cond when each chunk is fully read

    chat->content_index  = 0;
    chat->content_length = 0;
    char *ptr            = (char *)contents;
    char *end_ptr        = NULL;

    while (ptr != NULL)
    {
        ptr = strstr(ptr, "data: ");
        if (ptr != NULL)
        {
            // skip past the word data:
            ptr += 6;
            
            // Find the end of this data chunk (next newline)
            end_ptr = strchr(ptr, '\n');
            if (end_ptr) {
                *end_ptr = '\0'; // Temporarily null terminate for parsing
            }
            
            // printf("RAW SSE DATA: \"%s\"\n", ptr);

            root_value = json_parse_string(ptr);
            if (root_value == NULL)
            {
                // This is expected for "[DONE]" marker
                if (strncmp(ptr, "[DONE]", 6) != 0) {
                    printf("StreamOpenAICallback: json_parse_string failed for: %s\n", ptr);
                }
                // Restore the newline if we modified it
                if (end_ptr) {
                    *end_ptr = '\n';
                    ptr = end_ptr + 1;
                }
                continue;
            }
            
            // Restore the newline if we modified it
            if (end_ptr) {
                *end_ptr = '\n';
            }

            JSON_Object *root_object = json_value_get_object(root_value);
            if (root_object == NULL)
            {
                // printf("StreamOpenAICallback: json_parse_string failed\n");
                goto cleanup;
            }

            choices = json_object_get_array(root_object, "choices");
            if (choices == NULL)
            {
                // printf("StreamOpenAICallback: json_object_get_array failed\n");
                goto cleanup;
            }

            JSON_Object *choice = json_array_get_object(choices, 0);
            if (choice == NULL)
            {
                goto cleanup;
            }

            const char *finish_reason = json_object_get_string(choice, "finish_reason");

            if (!finish_reason)
            {
                JSON_Object *choice = json_array_get_object(choices, 0);

                if ((messages = json_object_get_object(choice, "delta")) != NULL)
                {
                    content = json_object_get_string(messages, "content");
                }
            }
            else
            {
                strcpy(chat->last_finish_reason, finish_reason);
                chat->last_finish_reason_length = (int)strlen(finish_reason);
                // Print newline when stream ends
                printf("\n[OpenAI stream ended: %s]\n", finish_reason);
                fflush(stdout);
                
                // Mark response as complete and STOP processing any more deltas
                mark_response_complete();
                
                // Important: Return immediately to stop processing any more data
                return realsize;
            }

            if (content != NULL)
            {
                // Block until previous data consumed (exactly like file_io.c)
                // printf("CALLBACK: Raw delta received: \"%s\" (len=%zu)\n", content, strlen(content));
                printf("%s", content);
                add_response_data(content);
                
                int content_length = strlen(content);
                if (content_length > 0 && chat->content_length + content_length < sizeof(chat->content))
                {
                    memcpy(chat->content + chat->content_length, content, content_length);
                    chat->content_length += content_length;
                }
            }

        cleanup:

            if (root_value != NULL)
            {
                json_value_free(root_value);
                root_value = NULL;
            }
            
            // Move to next data chunk
            if (end_ptr) {
                ptr = end_ptr + 1;
            } else {
                ptr = NULL;
            }
        }
    }

    chat->status = OPENAI_DATA_READY;

    return realsize;
}

static int stream_openai(struct curl_slist *headers, const char *postData, long timeout)
{
    CURL *curl_handle;

    // Debug: Show what payload we're about to send
    printf("=== Sending to OpenAI API ===\n");
    printf("Payload length: %zu bytes\n", strlen(postData));
    printf("Payload content:\n%s\n", postData);
    printf("=== End Payload ===\n");
    fflush(stdout);

    strcpy(openai.last_finish_reason, "failed");
    openai.last_finish_reason_length = 0;

    /* init the curl session */
    curl_handle = curl_easy_init();
    if (curl_handle)
    {
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

        /* set URL to get here */
        curl_easy_setopt(curl_handle, CURLOPT_URL, openai_endpoint);

        /* use a CURLOPT_POSTFIELDS to fetch data */
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postData);

        // https://curl.se/libcurl/c/CURLOPT_TIMEOUT.html
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 40L);

        /* send all data to this function  */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, StreamOpenAICallback);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&openai);
        
        /* Enable verbose output for debugging */
        curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
        
        /* Don't fail on HTTP error codes so we can see the response */
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 0L);

        /* some servers do not like requests that are made without a user-agent
           field, so we provide one */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        // based on the libcurl sample - https://curl.se/libcurl/c/https.html
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);

        // https://curl.se/libcurl/c/CURLOPT_NOSIGNAL.html
        curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);

        /* Switch on full protocol/debug output while testing */
        // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

        /* disable progress meter, set to 0L to enable it */
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

        openai.status = OPENAI_WAITING;

        /* write the page body to this file handle */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &openai);

        /* get it! */
        CURLcode res = curl_easy_perform(curl_handle);

        // Debug: Check curl result and HTTP response code
        long response_code;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        
        printf("=== Curl Request Completed ===\n");
        printf("Curl result: %d (%s)\n", res, curl_easy_strerror(res));
        printf("HTTP response code: %ld\n", response_code);
        
        if (res != CURLE_OK) {
            printf("Curl error: %s\n", curl_easy_strerror(res));
        }
        
        if (response_code >= 400) {
            printf("HTTP error: %ld\n", response_code);
        }
        
        printf("=== End Curl Debug ===\n");
        fflush(stdout);

        openai.status = OPENAI_END_OF_STREAM;

        /* cleanup curl stuff */
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
    }

    return 0;
}

static void *openai_thread(void *arg)
{
    if (!streaming)
    {
        streaming = true;
        // pthread_mutex_unlock(&openai_mutex);  // Commented out to match callback changes
        
        // Debug: Show what we're passing to stream_openai
        printf("=== OpenAI Thread Starting ===\n");
        printf("Buffer index: %d\n", buffer_index);
        printf("Buffer content length: %zu\n", strlen(openai_buffer));
        printf("Buffer first 100 chars: %.100s\n", openai_buffer);
        printf("=== Calling stream_openai ===\n");
        fflush(stdout);
        
        stream_openai(headers, openai_buffer, 5);
        printf("openai_thread: stream_openai returned\n");
        streaming = false;
    }
    return NULL;
}