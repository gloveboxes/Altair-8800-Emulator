/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "web_console.h"
#include "cpu_monitor.h" // For CPU_OPERATING_MODE, process_virtual_input, mode accessors, and intel8080_t

#include <ctype.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// External function declarations (defined in main.c)
extern CPU_OPERATING_MODE toggle_cpu_operating_mode(void);
extern CPU_OPERATING_MODE get_cpu_operating_mode_fast(void);
extern uint16_t bus_switches;
extern intel8080_t cpu;

// =============================================================================
// Constants and Configuration
// =============================================================================

#define OUTPUT_BUFFER_SIZE         4096
#define FLUSH_INTERVAL_MS          20
#define FLUSH_THRESHOLD_PERCENT    75
#define TERMINAL_INPUT_BUFFER_SIZE 256

// =============================================================================
// Type Definitions
// =============================================================================

// Output buffer structure
typedef struct
{
    char buffer[OUTPUT_BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t mutex;
} output_buffer_t;

// Terminal input queue structure
typedef struct
{
    char buffer[TERMINAL_INPUT_BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t mutex;
} terminal_input_queue_t;

// =============================================================================
// Static Variables
// =============================================================================

// Output buffer
static output_buffer_t output_buffer = {
    .head  = 0,
    .tail  = 0,
    .count = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
};

// Terminal input queue
static terminal_input_queue_t terminal_input_queue = {
    .buffer = {0},
    .head   = 0,
    .tail   = 0,
    .count  = 0,
    .mutex  = PTHREAD_MUTEX_INITIALIZER,
};

// WebSocket client management
static atomic_uintptr_t current_client = 0;
static void (*_client_connected_cb)(void);

// Session management
static bool cleanup_required     = false;
static const int session_minutes = 1 * 60 * 30; // 30 minutes

#ifdef ALTAIR_CLOUD
static struct timeval ws_timeout = {0, 250 * 1000};
#endif

// =============================================================================
// Forward Declarations
// =============================================================================

static DX_DECLARE_TIMER_HANDLER(expire_session_handler);
static DX_DECLARE_TIMER_HANDLER(flush_output_buffer_handler);
static void cleanup_session(void);
static void flush_output_buffer(void);
static void reset_output_buffer(void);
static bool output_buffer_write(const void *data, size_t length, size_t *resulting_count);
static inline bool output_buffer_write_bytes(const void *data, size_t length)
{
    return output_buffer_write(data, length, NULL);
}
static inline size_t terminal_queue_capacity(void);
static void handle_websocket_error(ws_cli_conn_t client, const char *error_msg);
static bool send_direct_websocket_message(ws_cli_conn_t client, const void *message, size_t length, const char *error_msg);

// =============================================================================
// Timer Bindings
// =============================================================================

static DX_TIMER_BINDING tmr_expire_session = {
    .name    = "tmr_expire_session",
    .handler = expire_session_handler,
};

static DX_TIMER_BINDING tmr_flush_output = {
    .name    = "tmr_flush_output",
    .handler = flush_output_buffer_handler,
    .repeat  = &(struct timespec){0, FLUSH_INTERVAL_MS *ONE_MS},
};

// =============================================================================
// Session Management Functions
// =============================================================================

/// <summary>
/// Clean up session resources
/// </summary>
static void cleanup_session(void)
{
#ifdef ALTAIR_CLOUD
    set_cpu_operating_mode(CPU_STOPPED);

    // Sleep this thread so the Altair CPU thread can complete current instruction
    nanosleep(&(struct timespec){0, 250 * ONE_MS}, NULL);

    load_boot_disk();
    clear_difference_disk();
#endif
    cleanup_required = false;
}

/// <summary>
/// Centralized WebSocket error handling
/// </summary>
/// <param name="client">WebSocket client connection</param>
/// <param name="error_msg">Error message to log</param>
static void handle_websocket_error(ws_cli_conn_t client, const char *error_msg)
{
    printf("%s\n", error_msg);
    ws_close_client(client);
    atomic_store(&current_client, 0);
    if (cleanup_required)
    {
        cleanup_session();
    }
}

// =============================================================================
// Event Handlers
// =============================================================================

/// <summary>
/// Timer handler for session expiration
/// </summary>
static DX_TIMER_HANDLER(expire_session_handler)
{
    ws_cli_conn_t client = (ws_cli_conn_t)atomic_load(&current_client);
    if (client != 0)
    {
        ws_close_client(client);
    }
    cleanup_session();
}
DX_TIMER_HANDLER_END

/// <summary>
/// Timer handler for WebSocket ping/pong
/// </summary>
DX_TIMER_HANDLER(ws_ping_pong_handler)
{
    ws_cli_conn_t client = (ws_cli_conn_t)atomic_load(&current_client);
    if (client != 0)
    {
        // Allow for up to 60 seconds (6 missed pings × 10 sec interval) before closing
        // Note: Browsers should auto-respond to PING with PONG at protocol level
        ws_ping(client, 6);
    }
}
DX_TIMER_HANDLER_END

/// <summary>
/// Async handler for session expiration
/// </summary>
DX_ASYNC_HANDLER(async_expire_session_handler, handle)
{
    dx_timerOneShotSet(&tmr_expire_session, &(struct timespec){session_minutes, 0});
}
DX_ASYNC_HANDLER_END

/// <summary>
/// Timer handler to periodically flush output buffer
/// </summary>
static DX_TIMER_HANDLER(flush_output_buffer_handler)
{
    flush_output_buffer();
}
DX_TIMER_HANDLER_END

// =============================================================================
// Output Buffer Functions
// =============================================================================

/// <summary>
/// Reset the output buffer contents and indices
/// </summary>
static void reset_output_buffer(void)
{
    pthread_mutex_lock(&output_buffer.mutex);
    output_buffer.head  = 0;
    output_buffer.tail  = 0;
    output_buffer.count = 0;
    pthread_mutex_unlock(&output_buffer.mutex);
}

/// <summary>
/// Add data to the output buffer
/// </summary>
/// <param name="data">Data to add</param>
/// <param name="length">Length of data</param>
/// <param name="resulting_count">Optional pointer to receive the buffer fill level after the write</param>
/// <returns>true if added successfully, false if buffer full</returns>
static bool output_buffer_write(const void *data, size_t length, size_t *resulting_count)
{
    if (data == NULL || length == 0)
    {
        return false;
    }

    pthread_mutex_lock(&output_buffer.mutex);

    // Check if buffer has enough space
    if (output_buffer.count + length > OUTPUT_BUFFER_SIZE)
    {
        pthread_mutex_unlock(&output_buffer.mutex);
        return false;
    }

    const size_t capacity = OUTPUT_BUFFER_SIZE;
    const char *bytes     = (const char *)data;
    size_t head           = output_buffer.head;
    size_t first_chunk    = length;

    // Ensure we don't write beyond buffer bounds
    if (head >= capacity) {
        head = head % capacity;  // Normalize head position
        output_buffer.head = head;
    }

    if (head + first_chunk > capacity)
    {
        first_chunk = capacity - head;
    }

    // Additional safety check to prevent overflow
    if (first_chunk > 0 && head + first_chunk <= capacity)
    {
        memcpy(&output_buffer.buffer[head], bytes, first_chunk);
    }

    size_t remaining = length - first_chunk;
    if (remaining > 0 && remaining <= capacity)
    {
        memcpy(&output_buffer.buffer[0], bytes + first_chunk, remaining);
    }

    output_buffer.head = (head + length) % capacity;
    output_buffer.count += length;

    if (resulting_count != NULL)
    {
        *resulting_count = output_buffer.count;
    }

    pthread_mutex_unlock(&output_buffer.mutex);
    return true;
}

/// <summary>
/// Flush the output buffer to the WebSocket client
/// </summary>
static void flush_output_buffer(void)
{
    ws_cli_conn_t client = (ws_cli_conn_t)atomic_load(&current_client);
    if (client == 0)
    {
        // No client connected, clear the buffer
        reset_output_buffer();
        return;
    }

    pthread_mutex_lock(&output_buffer.mutex);

    if (output_buffer.count == 0)
    {
        pthread_mutex_unlock(&output_buffer.mutex);
        return;
    }

    // Prepare data to send
    char temp_buffer[OUTPUT_BUFFER_SIZE];
    size_t bytes_to_send = output_buffer.count;

    // Efficient copy from circular buffer using memcpy
    // Handle potential wraparound in circular buffer
    if (output_buffer.tail + bytes_to_send <= OUTPUT_BUFFER_SIZE)
    {
        // Contiguous data - single memcpy
        memcpy(temp_buffer, &output_buffer.buffer[output_buffer.tail], bytes_to_send);
    }
    else
    {
        // Data wraps around - two memcpy operations
        size_t first_chunk  = OUTPUT_BUFFER_SIZE - output_buffer.tail;
        size_t second_chunk = bytes_to_send - first_chunk;

        memcpy(temp_buffer, &output_buffer.buffer[output_buffer.tail], first_chunk);
        memcpy(temp_buffer + first_chunk, &output_buffer.buffer[0], second_chunk);
    }

    // Update buffer state
    output_buffer.tail  = (output_buffer.tail + bytes_to_send) % OUTPUT_BUFFER_SIZE;
    output_buffer.count = 0;

    pthread_mutex_unlock(&output_buffer.mutex);

    // Send the buffered data
    if (bytes_to_send > 0)
    {
        // printf("Flushing output buffer: %zu bytes\n", bytes_to_send);
        if (ws_sendframe(client, temp_buffer, bytes_to_send, WS_FR_OP_TXT) == -1)
        {
            handle_websocket_error(client, "ws_sendframe failed - connection may be broken");
        }
    }
}

// =============================================================================
// Output Functions
// =============================================================================

/// <summary>
/// Publish a message to the WebSocket client
/// </summary>
/// <param name="message">Message data to send</param>
/// <param name="message_length">Length of the message</param>
void publish_message(const void *message, size_t message_length)
{
    // Validate input parameters
    if (message == NULL || message_length == 0)
    {
        printf("publish_message: Invalid message parameters\n");
        return;
    }

    ws_cli_conn_t client = (ws_cli_conn_t)atomic_load(&current_client);
    if (client == 0)
    {
        return;
    }

    // Check if message is too large for buffer
    if (message_length > OUTPUT_BUFFER_SIZE)
    {
        // Message too large, flush buffer first to maintain order, then send directly
        flush_output_buffer();
        send_direct_websocket_message(client, message, message_length, "ws_sendframe failed for large message - connection may be broken");
        return;
    }

    // Try to add data to buffer
    size_t current_count = 0;
    bool added           = output_buffer_write(message, message_length, &current_count);

    if (!added)
    {
        // Buffer is full, flush it first then add the message
        flush_output_buffer();

        // Try adding again (should succeed now since buffer was flushed)
        added = output_buffer_write(message, message_length, &current_count);

        if (!added)
        {
            // This shouldn't happen since we just flushed, but handle it
            printf("Failed to buffer message after flush - this shouldn't happen\n");
            send_direct_websocket_message(client, message, message_length, "ws_sendframe failed for fallback message - connection may be broken");
            return;
        }
    }

    // Check if we should flush immediately (only on threshold, not newlines)
    size_t threshold = (OUTPUT_BUFFER_SIZE * FLUSH_THRESHOLD_PERCENT) / 100;

    // Flush only if buffer is above threshold
    // Timer will handle periodic flushing (every 20ms)
    if (current_count >= threshold)
    {
        flush_output_buffer();
    }
}

/// <summary>
/// Send message directly via WebSocket (bypassing buffer)
/// </summary>
/// <param name="client">WebSocket client</param>
/// <param name="message">Message to send</param>
/// <param name="length">Message length</param>
/// <param name="error_msg">Error message for logging</param>
/// <returns>true if sent successfully</returns>
static bool send_direct_websocket_message(ws_cli_conn_t client, const void *message, size_t length, const char *error_msg)
{
    if (ws_sendframe(client, message, length, WS_FR_OP_TXT) == -1)
    {
        handle_websocket_error(client, error_msg);
        return false;
    }
    return true;
}

/// <summary>
/// Publish a single character to the WebSocket client
/// </summary>
/// <param name="character">Character to send</param>
inline void publish_character(char character)
{
    // print to stdout for debugging
    // the text is not appearing in vs code terminal?
    fflush(stdout);
    dx_Log_Debug("%c", character);
    publish_message(&character, 1);
}

// =============================================================================
// Terminal Input Queue Functions
// =============================================================================

/// <summary>
/// Get terminal queue capacity
/// </summary>
/// <returns>Size of the terminal input buffer</returns>
static inline size_t terminal_queue_capacity(void)
{
    return sizeof(terminal_input_queue.buffer);
}

/// <summary>
/// Add character to terminal input queue
/// </summary>
/// <param name="character">Character to enqueue</param>
void enqueue_terminal_input_character(char character)
{
    pthread_mutex_lock(&terminal_input_queue.mutex);

    size_t capacity = terminal_queue_capacity();
    if (terminal_input_queue.count >= capacity)
    {
        pthread_mutex_unlock(&terminal_input_queue.mutex);
        return; // drop character if buffer full
    }

    terminal_input_queue.buffer[terminal_input_queue.tail] = character;
    terminal_input_queue.tail                              = (terminal_input_queue.tail + 1) % capacity;
    terminal_input_queue.count++;

    pthread_mutex_unlock(&terminal_input_queue.mutex);
}

/// <summary>
/// Remove and return character from terminal input queue
/// </summary>
/// <returns>Character from queue, or 0 if queue is empty</returns>
char dequeue_terminal_input_character(void)
{
    char c = 0;

    pthread_mutex_lock(&terminal_input_queue.mutex);

    size_t capacity = terminal_queue_capacity();

    if (terminal_input_queue.count > 0)
    {
        c                         = terminal_input_queue.buffer[terminal_input_queue.head];
        terminal_input_queue.head = (terminal_input_queue.head + 1) % capacity;
        terminal_input_queue.count--;

        if (terminal_input_queue.count == 0)
        {
            terminal_input_queue.head = 0;
            terminal_input_queue.tail = 0;
        }
    }

    pthread_mutex_unlock(&terminal_input_queue.mutex);
    return c;
}

/// <summary>
/// Clear the terminal input queue
/// </summary>
void clear_terminal_input_queue(void)
{
    pthread_mutex_lock(&terminal_input_queue.mutex);
    terminal_input_queue.head  = 0;
    terminal_input_queue.tail  = 0;
    terminal_input_queue.count = 0;
    pthread_mutex_unlock(&terminal_input_queue.mutex);
}



/// <summary>
/// Enqueue characters for the CPU
/// </summary>
/// <param name="characters">Characters to queue</param>
/// <param name="length">Number of characters</param>
/// <returns>true when all characters were queued</returns>
bool terminal_enqueue_input_command(const char *characters, size_t length)
{
    if (characters == NULL || length == 0)
    {
        return false;
    }

    pthread_mutex_lock(&terminal_input_queue.mutex);

    size_t capacity        = terminal_queue_capacity();
    size_t available_space = capacity - terminal_input_queue.count;
    size_t to_enqueue      = (length <= available_space) ? length : available_space;

    // Enqueue the characters
    for (size_t i = 0; i < to_enqueue; i++)
    {
        terminal_input_queue.buffer[terminal_input_queue.tail] = characters[i];
        terminal_input_queue.tail                              = (terminal_input_queue.tail + 1) % capacity;
    }
    terminal_input_queue.count += to_enqueue;

    pthread_mutex_unlock(&terminal_input_queue.mutex);

    return to_enqueue == length;
}

// =============================================================================
// Terminal Input Processing Functions (consolidated from main.c)
// =============================================================================

/// <summary>
/// Handle enter key press in terminal
/// </summary>
/// <param name="data">Input data buffer</param>
/// <param name="cpu_mode">Current CPU operating mode</param>
/// <returns>true if handled and should cleanup, false to continue processing</returns>
static bool handle_enter_key(char *data, CPU_OPERATING_MODE cpu_mode)
{
    switch (cpu_mode)
    {
        case CPU_RUNNING:
            enqueue_terminal_input_character(0x0d);
            break;
        case CPU_STOPPED:
            data[0] = 0x00;
            process_virtual_input(data);
            break;
        default:
            break;
    }
    return true;
}

/// <summary>
/// Handle control character input
/// </summary>
/// <param name="data">Input character</param>
/// <param name="application_message_size">Size of the message</param>
/// <returns>true if handled and should cleanup, false to continue processing</returns>
static bool handle_ctrl_character(char *data, size_t application_message_size)
{
    char c = data[0];

    if (application_message_size == 1 && c > 0 && c < 29) // ASCII_CTRL_MAX
    {
        if (c == 28) // CTRL_M_MAPPED_VALUE - ctrl-m mapped to ASCII 28 to avoid /r
        {
            CPU_OPERATING_MODE new_mode = toggle_cpu_operating_mode();
            if (new_mode == CPU_STOPPED)
            {
                extern uint16_t bus_switches;
                extern intel8080_t cpu;
                bus_switches = cpu.address_bus;
                publish_message("\r\nCPU MONITOR> ", 15);
                return true;
            }
            c = 0x0d;
        }

        enqueue_terminal_input_character(c);
        return true;
    }
    return false;
}

/// <summary>
/// Handle single character input
/// </summary>
/// <param name="data">Input character</param>
/// <param name="application_message_size">Size of the message</param>
/// <param name="cpu_mode">Current CPU operating mode</param>
/// <returns>true if handled and should cleanup, false to continue processing</returns>
static bool handle_single_character(char *data, size_t application_message_size, CPU_OPERATING_MODE cpu_mode)
{
    if (application_message_size == 1)
    {
        if (cpu_mode == CPU_RUNNING)
        {
            terminal_enqueue_input_command(data, 1);
        }
        else
        {
            data[0] = (char)toupper(data[0]);
            data[1] = 0x00;
            process_virtual_input(data);
        }
        return true;
    }
    return false;
}

// =============================================================================
// WebSocket Event Handlers
// =============================================================================

/// <summary>
/// WebSocket connection opened event handler
/// </summary>
/// <param name="client">WebSocket client connection</param>
void onopen(ws_cli_conn_t client)
{
    // Validate client connection
    if (client == 0)
    {
        printf("onopen: Invalid client connection\n");
        return;
    }

    printf("New session\n");
    atomic_store(&current_client, (uintptr_t)client);

    // Clear the output buffer for new client
    reset_output_buffer();

    if (cleanup_required)
    {
        cleanup_session();
    }

#ifdef ALTAIR_CLOUD
    cleanup_required = true;
    dx_asyncSend(&async_expire_session, NULL);
#endif

    // Log new session instead of device twin reporting
    printf("New WebSocket session established\n");

    // Call client connected callback if it's valid
    if (_client_connected_cb != NULL)
    {
        _client_connected_cb();
    }
}

/// <summary>
/// WebSocket connection closed event handler
/// </summary>
/// <param name="client">WebSocket client connection</param>
void onclose(ws_cli_conn_t client)
{
    // Validate that we're closing the current client
    ws_cli_conn_t current = (ws_cli_conn_t)atomic_load(&current_client);
    if (client != current && current != 0)
    {
        printf("onclose: Closing client does not match current client\n");
    }

    // Flush any remaining data before closing
    flush_output_buffer();

    printf("Session closed\n");
    atomic_store(&current_client, 0);

    // Clear the output buffer
    reset_output_buffer();

    if (cleanup_required)
    {
        cleanup_session();
    }
}

/// <summary>
/// WebSocket message received event handler
/// </summary>
/// <param name="client">WebSocket client connection</param>
/// <param name="msg">Message data received</param>
/// <param name="size">Size of the message</param>
/// <param name="type">Message type</param>
/// <summary>
/// Terminal input handler (consolidated from main.c)
/// </summary>
/// <param name="data">Input data buffer</param>
/// <param name="application_message_size">Size of the message</param>
void terminal_handler(char *data, size_t application_message_size)
{
#define TERMINAL_COMMAND_BUFFER_SIZE 30
    char command[TERMINAL_COMMAND_BUFFER_SIZE];
    memset(command, 0x00, sizeof(command));

    if (data == NULL || application_message_size == 0 || application_message_size >= 1024)
    {
        return;
    }

    // Handle control characters
    if (handle_ctrl_character(data, application_message_size))
    {
        return;
    }

    CPU_OPERATING_MODE cpu_mode = get_cpu_operating_mode_fast();

    // Was just enter pressed
    if (data[0] == '\r')
    {
        if (handle_enter_key(data, cpu_mode))
        {
            return;
        }
    }

    // Handle single character input
    if (handle_single_character(data, application_message_size, cpu_mode))
    {
        return;
    }

    // Prepare upper-case copy for virtual command processing when CPU is stopped
    size_t copy_len = (sizeof(command) - 1 < application_message_size) ? sizeof(command) - 1 : application_message_size;
    for (size_t i = 0; i < copy_len; i++)
    {
        command[i] = (char)toupper((unsigned char)data[i]);
    }
    command[copy_len] = '\0';

    switch (cpu_mode)
    {
        case CPU_RUNNING:
            if (application_message_size > 0)
            {
                terminal_enqueue_input_command(data, application_message_size);
            }
            break;
        case CPU_STOPPED:
            process_virtual_input(command);
            break;
        default:
            break;
    }
}

void onmessage(ws_cli_conn_t client, const unsigned char *msg, uint64_t size, int type)
{
    (void)client;
    (void)type;

    if (msg == NULL || size == 0)
    {
        return;
    }

    terminal_handler((char *)msg, (size_t)size);
}

// =============================================================================
// WebSocket Server Functions
// =============================================================================

/// <summary>
/// Initialize the WebSocket server
/// </summary>
/// <param name="client_connected_cb">Callback function called when client connects</param>
void init_web_socket_server(void (*client_connected_cb)(void))
{
    // Validate input parameter
    if (client_connected_cb == NULL)
    {
        printf("init_web_socket_server: client_connected_cb callback is NULL\n");
        return;
    }

    _client_connected_cb = client_connected_cb;

    // Start timers
    dx_timerStart(&tmr_expire_session);
    dx_timerStart(&tmr_flush_output);

    struct ws_server ws_srv = {.host = NULL, // NULL means bind to all interfaces
        .port                        = 8082,
        .thread_loop                 = 1,
        .timeout_ms                  = 250,
        .evs                         = {.onopen = &onopen, .onclose = &onclose, .onmessage = &onmessage},
        .context                     = NULL};

    ws_socket(&ws_srv);
}
