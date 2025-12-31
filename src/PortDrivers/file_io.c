/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "file_io.h"
#include "dx_utilities.h"
#include <curl/curl.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef AZURE_SPHERE
#include <applibs/storage.h>
#endif // AZURE_SPHERE

#define APP_SAMPLES_DIRECTORY "AppSamples"

#define ENDPOINT_LEN 128
#define CHUNK_SIZE   256

static int copy_web(char *url);
static char devget_path_and_filename[50];

enum WEBGET_STATUS
{
    WEBGET_EOF,
    WEBGET_WAITING,
    WEBGET_DATA_READY,
    WEBGET_FAILED
};

typedef struct
{
    char chunk_buffer[CHUNK_SIZE]; // Buffer to hold current chunk
    size_t chunk_bytes_available;  // Number of bytes available in current chunk
    size_t chunk_position;         // Current read position within chunk
    bool transfer_complete;        // True when curl transfer is complete
    char personal_endpoint[ENDPOINT_LEN];
    char filename[128];
    char url[150];
    enum WEBGET_STATUS status;
    int index;
} WEBGET_T;

typedef struct
{
    int fd;
    char filename[128];
    bool file_opened;
    bool enabled;
    bool end_of_file;
    int index;
    int ch;
} DEVGET_T;

static WEBGET_T webget;
static DEVGET_T devget;

pthread_mutex_t webget_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t webget_cond   = PTHREAD_COND_INITIALIZER;

// Thread management for web transfers
static pthread_t transfer_thread   = 0;
static bool transfer_thread_active = false;

/// <summary>
/// Clean up webget resources and reset state
/// Must be called with webget_mutex held
/// </summary>
static void cleanup_webget_resources(void)
{
    memset(webget.chunk_buffer, 0, CHUNK_SIZE);
    webget.chunk_bytes_available = 0;
    webget.chunk_position        = 0;
    webget.transfer_complete     = true;
    webget.status                = WEBGET_FAILED;

    // Wake up any waiting curl callback so it can detect the transfer is no longer active
    pthread_cond_broadcast(&webget_cond);
}

/// <summary>
/// Thread function to handle web transfer
/// </summary>
static void *transfer_thread_func(void *arg)
{
    char *url = (char *)arg;

    // Enable thread cancellation
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    copy_web(url);

    pthread_mutex_lock(&webget_mutex);
    transfer_thread_active = false;
    transfer_thread        = 0;
    pthread_mutex_unlock(&webget_mutex);

    free(url);
    return NULL;
}

/// <summary>
/// Start a new web transfer, cancelling any active transfer first
/// Must be called with webget_mutex held
/// Returns 0 on success, -1 on failure
/// </summary>
static int start_web_transfer(const char *url)
{
    // Check if there's an active transfer thread and kill it
    if (transfer_thread_active && transfer_thread != 0)
    {
        printf("DEBUG: Active transfer detected (thread: %lu), cancelling thread\n", (unsigned long)transfer_thread);
        cleanup_webget_resources();

        pthread_t old_thread   = transfer_thread;
        transfer_thread        = 0;
        transfer_thread_active = false;

        pthread_mutex_unlock(&webget_mutex);

        // Cancel and wait for the old thread to finish
        pthread_cancel(old_thread);
        pthread_join(old_thread, NULL);

        pthread_mutex_lock(&webget_mutex);
    }

    // Initialize new transfer state
    webget.index  = 0;
    webget.status = WEBGET_WAITING;

    // Initialize chunk buffer fields
    memset(webget.chunk_buffer, 0, CHUNK_SIZE);
    webget.chunk_bytes_available = 0;
    webget.chunk_position        = 0;
    webget.transfer_complete     = false;

    // Create a copy of the URL for the thread
    char *url_copy = strdup(url);
    if (url_copy == NULL)
    {
        printf("ERROR: Failed to allocate memory for URL\n");
        webget.status = WEBGET_FAILED;
        return -1;
    }

    // Start the transfer thread
    transfer_thread_active = true;
    if (pthread_create(&transfer_thread, NULL, transfer_thread_func, url_copy) != 0)
    {
        printf("ERROR: Failed to create transfer thread\n");
        webget.status          = WEBGET_FAILED;
        transfer_thread_active = false;
        transfer_thread        = 0;
        free(url_copy);
        return -1;
    }

    return 0;
}

size_t file_output(int port, uint8_t data, char *buffer, size_t buffer_length)
{
    size_t len = 0;
    switch (port)
    {
#ifdef AZURE_SPHERE
        case 68: // set devget filename
            if (devget.index == 0)
            {
                if (devget.file_opened && devget.fd != -1)
                {
                    close(devget.fd);
                }
                memset(&devget, 0x00, sizeof(DEVGET_T));
                devget.fd = -1;
            }

            if (data != 0 && devget.index < sizeof(devget.filename) - 1)
            {
                devget.filename[devget.index] = data;
                devget.index++;
            }

            if (data == 0) // NULL TERMINATION
            {
                devget.index = 0;
            }

            break;

#endif // AZURE SPHERE
        case 109:
            webget.index = 0;
            break;
        case 110: // Set getfile custom endpoint url
            if (webget.index == 0)
            {
                memset(webget.personal_endpoint, 0x00, ENDPOINT_LEN);
            }

            if (data != 0 && webget.index < ENDPOINT_LEN - 1)
            {
                webget.personal_endpoint[webget.index++] = data;
            }

            if (data == 0) // NULL TERMINATION
            {
                if (webget.index < ENDPOINT_LEN)
                {
                    webget.personal_endpoint[webget.index] = 0x00;
                }
                webget.index = 0;
            }
            break;
        case 111: // Load getfile (gf) custom endpoint url
            len = (size_t)snprintf(buffer, buffer_length, "%s", webget.personal_endpoint);
            break;
        case 114: // copy file from web server to mutable storage
            if (webget.index == 0)
            {
                memset(webget.filename, 0x00, sizeof(webget.filename));
            }

            if (data != 0 && webget.index < sizeof(webget.filename) - 1)
            {
                webget.filename[webget.index] = data;
                webget.index++;
            }

            if (data == 0) // NULL TERMINATION
            {
                pthread_mutex_lock(&webget_mutex);

                // Build the URL
                memset(webget.url, 0x00, sizeof(webget.url));
                snprintf(webget.url, sizeof(webget.url), "%s/%s", webget.personal_endpoint, webget.filename);

                // Start the new transfer (cancelling any active transfer first)
                start_web_transfer(webget.url);

                pthread_mutex_unlock(&webget_mutex);
            }
            break;
    }

    return len;
}

uint8_t file_input(uint8_t port)
{
    uint8_t retVal = 0;

    switch (port)
    {
        case 33: // has copyx file need copied and loaded
            retVal = webget.status;
            break;
        case 68: // has devget eof
            retVal = devget.end_of_file;
            break;
        case 201: // Read file from http(s) web server
            pthread_mutex_lock(&webget_mutex);

            if (webget.chunk_bytes_available > 0 && webget.chunk_position < webget.chunk_bytes_available)
            {
                // Return byte from current chunk
                retVal = webget.chunk_buffer[webget.chunk_position];
                webget.chunk_position++;

                // Check if we've consumed the entire chunk
                if (webget.chunk_position >= webget.chunk_bytes_available)
                {
                    // Reset chunk for next fill
                    webget.chunk_bytes_available = 0;
                    webget.chunk_position        = 0;

                    // Signal that chunk buffer is now available for new data
                    pthread_cond_signal(&webget_cond);

                    // Set status based on whether more data is expected
                    webget.status = webget.transfer_complete ? WEBGET_EOF : WEBGET_WAITING;
                }
                else
                {
                    // More bytes available in current chunk
                    webget.status = WEBGET_DATA_READY;
                }
            }
            else
            {
                // No data in buffer - check if transfer is complete
                if (webget.transfer_complete)
                {
                    webget.status = WEBGET_EOF;
                }
                else
                {
                    webget.status = WEBGET_WAITING;
                }
                retVal = 0x00;
            }

            pthread_mutex_unlock(&webget_mutex);
            break;
#ifdef AZURE_SPHERE

        case 202: // READ DEVGET file from immutable storage
            if (devget.end_of_file)
            {
                retVal = 0x00;
            }
            else
            {
                if (!devget.file_opened)
                {
                    /* open the file */
                    snprintf(devget_path_and_filename, sizeof(devget_path_and_filename), "%s/%s", APP_SAMPLES_DIRECTORY, devget.filename);

                    if ((devget.fd = Storage_OpenFileInImagePackage(devget_path_and_filename)) != -1)
                    {
                        devget.file_opened = true;
                    }
                    else
                    {
                        devget.end_of_file = true;
                    }
                }

                if (devget.file_opened)
                {
                    if (read(devget.fd, &retVal, 1) == 0)
                    {
                        close(devget.fd);

                        devget.file_opened = false;
                        devget.end_of_file = true;
                        devget.fd          = -1;
                        retVal             = 0x00;
                    }
                }
                else
                {
                    retVal = 0x00;
                }
            }
            break;

#endif // AZURE SPHERE
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
        printf(".");
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

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *webget)
{
    size_t realsize        = size * nmemb;
    WEBGET_T *wg           = (WEBGET_T *)webget;
    size_t bytes_processed = 0;

    struct timespec timeoutTime;
    memset(&timeoutTime, 0, sizeof(struct timespec));

#ifdef __APPLE__
    // On macOS, use a simple timeout from current time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timeoutTime.tv_sec  = tv.tv_sec + 10;
    timeoutTime.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &timeoutTime);
    timeoutTime.tv_sec += 10;
#endif // __APPLE__

    if (pthread_mutex_timedlock(&webget_mutex, &timeoutTime) != 0)
    {
        wg->status = WEBGET_FAILED;
        return 0; // Return 0 to indicate error to curl
    }

    // Check if transfer is still active
    if (wg->status == WEBGET_FAILED)
    {
        printf("DEBUG: Transfer no longer active, aborting callback\n");
        pthread_mutex_unlock(&webget_mutex);
        return 0; // Abort transfer
    }

    // Process data in chunks, waiting for current chunk to be consumed before adding more
    while (bytes_processed < realsize)
    {
        // Check if this transfer is still active
        if (wg->status == WEBGET_FAILED)
        {
            printf("DEBUG: Transfer cancelled, aborting callback\n");
            pthread_mutex_unlock(&webget_mutex);
            return 0; // Abort this transfer
        }

        // Wait for the current chunk to be consumed by the client using condition variable with timeout
        while (wg->chunk_bytes_available > 0 && wg->status != WEBGET_FAILED)
        {
            // Create timeout for client crash detection (5 seconds)
            struct timespec client_timeout;
#ifdef __APPLE__
            struct timeval tv;
            gettimeofday(&tv, NULL);
            client_timeout.tv_sec  = tv.tv_sec + 5; // 5 second timeout for client response
            client_timeout.tv_nsec = tv.tv_usec * 1000;
#else
            clock_gettime(CLOCK_REALTIME, &client_timeout);
            client_timeout.tv_sec += 5; // 5 second timeout for client response
#endif

            // Efficient wait using condition variable with timeout
            if (pthread_cond_timedwait(&webget_cond, &webget_mutex, &client_timeout) != 0)
            {
                // Timeout occurred - likely client crashed or stopped reading
                if (wg->status != WEBGET_FAILED)
                {
                    printf("DEBUG: Client timeout - assuming client crashed or stopped reading\n");
                    wg->status = WEBGET_FAILED;
                }
                pthread_mutex_unlock(&webget_mutex);
                return bytes_processed;
            }
        }

        if (wg->status == WEBGET_FAILED)
        {
            break;
        }

        // Calculate how much data we can put into the chunk buffer
        size_t remaining_input = realsize - bytes_processed;
        size_t bytes_to_copy   = (remaining_input > CHUNK_SIZE) ? CHUNK_SIZE : remaining_input;

        // Final check before modifying buffer
        if (wg->status != WEBGET_FAILED)
        {
            // Copy data to chunk buffer
            memcpy(wg->chunk_buffer, (char *)ptr + bytes_processed, bytes_to_copy);
            wg->chunk_bytes_available = bytes_to_copy;
            wg->chunk_position        = 0;
            wg->status                = WEBGET_DATA_READY;

            bytes_processed += bytes_to_copy;
        }
        else
        {
            // Transfer was cancelled while we were waiting
            break;
        }
    }

    pthread_mutex_unlock(&webget_mutex);

    // Return 0 if transfer was cancelled, otherwise return what we processed
    if (wg->status == WEBGET_FAILED)
    {
        return 0; // Signal curl to abort
    }

    return realsize;
}

static int copy_web(char *url)
{
    CURL *curl_handle;

    // Validate input parameters
    if (url == NULL || strlen(url) == 0)
    {
        printf("ERROR: Invalid URL provided to copy_web\n");
        webget.status = WEBGET_FAILED;
        return -1;
    }

    /* init the curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL)
    {
        printf("ERROR: Failed to initialize curl handle\n");
        webget.status = WEBGET_FAILED;
        return -1;
    }

    /* set URL to get here */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    // Enable SSL verification for security (comment out the next line to disable if needed for testing)
    // For production use, SSL verification should be enabled
    // curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);

    // Enable SSL verification and certificate validation
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);

    // https://curl.se/libcurl/c/CURLOPT_NOSIGNAL.html
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);

    // https://curl.se/libcurl/c/CURLOPT_TIMEOUT.html - increased for larger files
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 20L);

    /* Disable debug output for production */
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);

    /* disable progress meter, set to 0L to enable it */
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

    // Remove artificial buffer size limit to let curl use natural chunk sizes
    // curl_easy_setopt(curl_handle, CURLOPT_BUFFERSIZE, 256L);

    // Limit transfer speed to prevent buffer overruns
    // Set low speed limit to 100 bytes/sec for 30 seconds before timeout
    curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT, 100L);
    curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME, 30L);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

    // A long parameter set to 1 tells the library to fail the request if the HTTP code returned is equal to
    // or larger than 400
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);

    webget.status = WEBGET_WAITING;

    /* write the page body to this file handle */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &webget);

    /* get it! */
    CURLcode res = curl_easy_perform(curl_handle);

    // Lock mutex to safely update webget state
    pthread_mutex_lock(&webget_mutex);

    if (res != CURLE_OK)
    {
        printf("CURL ERROR: %s\n", curl_easy_strerror(res));
        cleanup_webget_resources(); // This sets status to WEBGET_FAILED via cleanup
        webget.status = WEBGET_FAILED;
    }
    else
    {
        // Transfer completed successfully
        webget.transfer_complete = true;

        // Check if we have data ready in current chunk
        if (webget.chunk_bytes_available > 0)
        {
            webget.status = WEBGET_DATA_READY;
        }
        else
        {
            // Empty file or no data
            webget.status = WEBGET_EOF;
        }
    }

    // Mark transfer as complete and inactive
    webget.transfer_complete = true;

    pthread_mutex_unlock(&webget_mutex);

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);

    // Note: Don't call curl_global_cleanup() here as it should only be called
    // once when the application terminates, not after each transfer

    return (res == CURLE_OK) ? 0 : -1;
}
