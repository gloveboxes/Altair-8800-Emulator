/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <stddef.h>
#include <stdint.h>

/// <summary>
/// Initialize the file transfer I/O system
/// </summary>
void files_io_init(void);

/// <summary>
/// Set the remote FT server IP address
/// </summary>
/// <param name="ip">IP address of the remote FT server (e.g., "192.168.1.100")</param>
void files_io_set_server_ip(const char *ip);

/// <summary>
/// Handle output to file transfer ports (60, 61)
/// Port 60: Command port (NOP, SET_FILENAME, GET_CHUNK, CLOSE)
/// Port 61: Data port (filename characters)
/// </summary>
/// <param name="port">Port number (60 or 61)</param>
/// <param name="data">Data byte to write</param>
/// <param name="buffer">Buffer for response data (unused)</param>
/// <param name="buffer_length">Length of buffer (unused)</param>
/// <returns>Length of data written to buffer (always 0)</returns>
size_t files_io_output(int port, uint8_t data, char *buffer, size_t buffer_length);

/// <summary>
/// Handle input from file transfer ports (60, 61)
/// Port 60: Status port (IDLE=0, DATA_READY=1, EOF=2, BUSY=3, ERROR=255)
/// Port 61: Data port (count byte or file data)
/// </summary>
/// <param name="port">Port number (60 or 61)</param>
/// <returns>Status byte (port 60) or data byte (port 61)</returns>
uint8_t files_io_input(uint8_t port);
