/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "88dcdd.h"
#include "dx_async.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <ws.h>

// =============================================================================
// External Declarations
// =============================================================================

extern DX_ASYNC_BINDING async_expire_session;

// =============================================================================
// Event Handlers
// =============================================================================

DX_DECLARE_ASYNC_HANDLER(async_expire_session_handler);
DX_DECLARE_TIMER_HANDLER(ws_ping_pong_handler);

// =============================================================================
// WebSocket Server Functions
// =============================================================================

void init_web_socket_server(void (*client_connected_cb)(void));

// =============================================================================
// Output Functions
// =============================================================================

void publish_message(const void *application_message, size_t application_message_length);

// =============================================================================
// Terminal Input Queue Functions
// =============================================================================

void enqueue_terminal_input_character(char character);
char dequeue_terminal_input_character(void);
void clear_terminal_input_queue(void);
bool terminal_enqueue_input_command(const char *characters, size_t length);

// =============================================================================
// Utility Functions
// =============================================================================

void print_console_banner(void);
void terminal_handler(char *data, size_t length);
