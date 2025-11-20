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

#define TERMINAL_INPUT_BUFFER_SIZE 128
#define COMMAND_BUFFER_SIZE        30

// =============================================================================
// Type Definitions
// =============================================================================

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

// Terminal input queue (for CPU_RUNNING mode)
static terminal_input_queue_t terminal_input_queue = {
    .buffer = {0},
    .head   = 0,
    .tail   = 0,
    .count  = 0,
    .mutex  = PTHREAD_MUTEX_INITIALIZER,
};

// Command buffer for CPU_STOPPED mode
static char command_buffer[COMMAND_BUFFER_SIZE] = {0};
static size_t command_buffer_length             = 0;

// WebSocket client management
static atomic_uintptr_t current_client = 0;
static void (*_client_connected_cb)(void);
static void (*_client_disconnected_cb)(void);

// Session management
static const int session_minutes = 1 * 60 * 30; // 30 minutes

// =============================================================================
// Forward Declarations
// =============================================================================

static DX_DECLARE_TIMER_HANDLER(expire_session_handler);
static inline size_t terminal_queue_capacity(void);
static void handle_websocket_error(ws_cli_conn_t client, const char *error_msg);

// =============================================================================
// Timer Bindings
// =============================================================================

static DX_TIMER_BINDING tmr_expire_session = {
    .name    = "tmr_expire_session",
    .handler = expire_session_handler,
};

// =============================================================================
// Session Management Functions
// =============================================================================

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
        return;
    }

    ws_cli_conn_t client = (ws_cli_conn_t)atomic_load(&current_client);
    if (client == 0)
    {
        return;
    }

    // Send message directly to WebSocket
    if (ws_sendframe(client, message, message_length, WS_FR_OP_TXT) == -1)
    {
        handle_websocket_error(client, "ws_sendframe failed - connection may be broken");
    }
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
        c = terminal_input_queue.buffer[terminal_input_queue.head];

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
/// Handle control character input (CTRL-M for mode toggle)
/// </summary>
/// <param name="data">Input character</param>
/// <param name="application_message_size">Size of the message</param>
/// <returns>true if handled, false to continue processing</returns>
static bool handle_ctrl_character(char *data, size_t application_message_size)
{
    char c = data[0];

    if (application_message_size > 0 && c == 28) // CTRL_M_MAPPED_VALUE - ctrl-m mapped to ASCII 28 to avoid /r
    {
        CPU_OPERATING_MODE new_mode = toggle_cpu_operating_mode();
        if (new_mode == CPU_STOPPED)
        {
            extern uint16_t bus_switches;
            extern intel8080_t cpu;
            bus_switches = cpu.address_bus;

            // Clear command buffer when switching to stopped mode
            command_buffer_length = 0;
            command_buffer[0]     = '\0';
            publish_message("\r\nCPU MONITOR> ", 15);
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

    printf("Session closed\n");
    atomic_store(&current_client, 0);
    _client_disconnected_cb();
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
    if (data == NULL || application_message_size == 0 || application_message_size >= 1024)
    {
        return;
    }

    // Handle control characters (including CTRL-M for mode toggle)
    if (handle_ctrl_character(data, application_message_size))
    {
        return;
    }

    CPU_OPERATING_MODE cpu_mode = get_cpu_operating_mode_fast();

    // Process based on CPU mode
    switch (cpu_mode)
    {
        case CPU_RUNNING:
            // When CPU is running, queue all input directly
            if (data[0] == '\r')
            {
                enqueue_terminal_input_character(0x0d);
            }
            else if (application_message_size > 0)
            {
                terminal_enqueue_input_command(data, application_message_size);
            }
            break;

        case CPU_STOPPED:
            // When CPU is stopped, accumulate characters until return key is pressed
            if (data[0] == '\r')
            {
                // Return key pressed - process the accumulated command
                command_buffer[command_buffer_length] = '\0';

                // Process the command if not empty
                if (command_buffer_length > 0)
                {
                    process_virtual_input(command_buffer);
                }
                else
                {
                    // Empty command - just call with empty string
                    process_virtual_input("");
                }

                // Reset the command buffer
                command_buffer_length = 0;
                command_buffer[0]     = '\0';
            }
            else
            {
                // Accumulate characters into the command buffer
                for (size_t i = 0; i < application_message_size && command_buffer_length < COMMAND_BUFFER_SIZE - 1; i++)
                {
                    // Convert to uppercase and add to buffer
                    command_buffer[command_buffer_length++] = (char)toupper((unsigned char)data[i]);
                    publish_message(data, application_message_size);
                }
            }
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
void init_web_socket_server(void (*client_connected_cb)(void), void (*client_disconnected_cb)(void))
{
    // Validate input parameter
    if (client_connected_cb == NULL)
    {
        printf("init_web_socket_server: client_connected_cb callback is NULL\n");
        return;
    }

    _client_connected_cb = client_connected_cb;
    _client_disconnected_cb = client_disconnected_cb;

    // Start timers
    dx_timerStart(&tmr_expire_session);

    struct ws_server ws_srv = {.host = NULL, // NULL means bind to all interfaces
        .port                        = 8082,
        .thread_loop                 = 1,
        .timeout_ms                  = 250,
        .evs                         = {.onopen = &onopen, .onclose = &onclose, .onmessage = &onmessage},
        .context                     = NULL};

    ws_socket(&ws_srv);
}
