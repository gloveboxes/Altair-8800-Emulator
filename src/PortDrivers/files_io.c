/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "files_io.h"
#include "dx_utilities.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

// Protocol constants
#define FT_PROTO_GET_CHUNK 0x02
#define FT_PROTO_CLOSE 0x03

#define FT_PROTO_RESP_OK 0x00
#define FT_PROTO_RESP_EOF 0x01
#define FT_PROTO_RESP_ERROR 0xFF

#define FT_CHUNK_SIZE 256
#define FT_SERVER_PORT 8090
#define FT_SOCKET_TIMEOUT_SEC 10

// Status values returned on port 60
enum FT_STATUS
{
    FT_STATUS_IDLE = 0,
    FT_STATUS_DATA_READY = 1,
    FT_STATUS_EOF = 2,
    FT_STATUS_BUSY = 3,
    FT_STATUS_ERROR = 255
};

// Commands received on port 60
enum FT_COMMAND
{
    FT_CMD_NOP = 0,
    FT_CMD_REQUEST_CHUNK = 3,
    FT_CMD_CLOSE = 4
};

// State structure
typedef struct
{
    // Port state
    char filename[128];
    size_t filename_idx;
    
    // Current chunk being read by Altair (count byte + data)
    uint8_t chunk_buffer[FT_CHUNK_SIZE + 1];
    size_t chunk_len;      // bytes available (including count byte)
    size_t chunk_position; // current read position
    
    // File position tracking for stateless protocol
    uint32_t file_offset;
    
    enum FT_STATUS status;
    
    // TCP connection
    int sockfd;
    bool connected;
    
    // Configuration
    char server_ip[64];
    
} files_io_state_t;

static files_io_state_t ft_state;

// Forward declarations
static int connect_to_server(void);
static int send_get_chunk(void);
static int send_close(void);
static void disconnect_from_server(void);

void files_io_init(void)
{
    memset(&ft_state, 0, sizeof(files_io_state_t));
    ft_state.status = FT_STATUS_IDLE;
    ft_state.sockfd = -1;
    
    printf("[FT] File transfer I/O initialized (stateless protocol v3)\n");
}

void files_io_set_server_ip(const char *ip)
{
    if (ip != NULL)
    {
        strncpy(ft_state.server_ip, ip, sizeof(ft_state.server_ip) - 1);
        ft_state.server_ip[sizeof(ft_state.server_ip) - 1] = '\0';
        printf("[FT] Remote FT server IP set to: %s\n", ip);
    }
}

size_t files_io_output(int port, uint8_t data, char *buffer, size_t buffer_length)
{
    (void)buffer;
    (void)buffer_length;
    
    if (port == 61)
    {
        // Data port - receive filename characters
        if (data == 0)
        {
            // Null terminator - filename complete
            ft_state.filename[ft_state.filename_idx] = '\0';
            
            // Reset state for new file
            ft_state.chunk_len = 0;
            ft_state.chunk_position = 0;
            ft_state.file_offset = 0;
            ft_state.filename_idx = 0;  // Reset for next filename
            ft_state.status = FT_STATUS_IDLE;
            
            // Connect if needed (but don't send filename yet - GET_CHUNK will do it)
            if (!ft_state.connected)
            {
                if (connect_to_server() != 0)
                {
                    ft_state.status = FT_STATUS_ERROR;
                    return 0;
                }
            }
        }
        else if (ft_state.filename_idx < sizeof(ft_state.filename) - 1)
        {
            ft_state.filename[ft_state.filename_idx++] = (char)data;
        }
        return 0;
    }
    
    if (port != 60)
        return 0;
    
    // Command port
    switch ((enum FT_COMMAND)data)
    {
        case FT_CMD_NOP:
            break;
            
        case FT_CMD_REQUEST_CHUNK:
            // Connect if needed (connection may have been lost)
            if (!ft_state.connected)
            {
                if (connect_to_server() != 0)
                {
                    ft_state.status = FT_STATUS_ERROR;
                    return 0;
                }
            }
            
            // If we still have data in buffer, don't request more
            if (ft_state.chunk_len > 0 && ft_state.chunk_position < ft_state.chunk_len)
            {
                break;
            }
            
            // Reset chunk buffer
            ft_state.chunk_len = 0;
            ft_state.chunk_position = 0;
            
            // Request chunk (includes filename and offset - fully stateless)
            if (send_get_chunk() != 0)
            {
                ft_state.status = FT_STATUS_ERROR;
                disconnect_from_server();
            }
            break;
            
        case FT_CMD_CLOSE:
            send_close();
            ft_state.status = FT_STATUS_IDLE;
            break;
            
        default:
            break;
    }
    
    return 0;
}

uint8_t files_io_input(uint8_t port)
{
    if (port == 60)
    {
        // Status port
        // If we have data in buffer, always return DATA_READY
        if (ft_state.chunk_len > 0 && ft_state.chunk_position < ft_state.chunk_len)
        {
            return FT_STATUS_DATA_READY;
        }
        
        return (uint8_t)ft_state.status;
    }
    else if (port == 61)
    {
        // Data port
        if (ft_state.chunk_len > 0 && ft_state.chunk_position < ft_state.chunk_len)
        {
            return ft_state.chunk_buffer[ft_state.chunk_position++];
        }
        return 0x00;
    }
    
    return 0x00;
}

static int connect_to_server(void)
{
    struct sockaddr_in server_addr;
    struct timeval timeout;
    
    if (ft_state.server_ip[0] == '\0')
    {
        printf("[FT] ERROR: Server IP not configured\n");
        return -1;
    }
    
    printf("[FT] Connecting to %s:%d\n", ft_state.server_ip, FT_SERVER_PORT);
    
    ft_state.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ft_state.sockfd < 0)
    {
        printf("[FT] ERROR: Failed to create socket: %s\n", strerror(errno));
        return -1;
    }
    
    // Set socket timeout
    timeout.tv_sec = FT_SOCKET_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(ft_state.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(ft_state.sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Disable Nagle's algorithm
    int flag = 1;
    setsockopt(ft_state.sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FT_SERVER_PORT);
    
    if (inet_pton(AF_INET, ft_state.server_ip, &server_addr.sin_addr) <= 0)
    {
        printf("[FT] ERROR: Invalid server IP: %s\n", ft_state.server_ip);
        close(ft_state.sockfd);
        ft_state.sockfd = -1;
        return -1;
    }
    
    if (connect(ft_state.sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("[FT] ERROR: Failed to connect: %s\n", strerror(errno));
        close(ft_state.sockfd);
        ft_state.sockfd = -1;
        return -1;
    }
    
    ft_state.connected = true;
    printf("[FT] Connected\n");
    return 0;
}

static int send_get_chunk(void)
{
    uint8_t cmd = FT_PROTO_GET_CHUNK;
    uint8_t status;
    uint8_t count;
    size_t chunk_size;
    
    // Send command
    if (send(ft_state.sockfd, &cmd, 1, 0) != 1)
    {
        printf("[FT] ERROR: Failed to send GET_CHUNK command\n");
        return -1;
    }
    
    // Send offset (4 bytes, little-endian)
    uint8_t offset_bytes[4];
    offset_bytes[0] = ft_state.file_offset & 0xFF;
    offset_bytes[1] = (ft_state.file_offset >> 8) & 0xFF;
    offset_bytes[2] = (ft_state.file_offset >> 16) & 0xFF;
    offset_bytes[3] = (ft_state.file_offset >> 24) & 0xFF;
    
    if (send(ft_state.sockfd, offset_bytes, 4, 0) != 4)
    {
        printf("[FT] ERROR: Failed to send offset\n");
        return -1;
    }
    
    // Send filename with null terminator
    size_t filename_len = strlen(ft_state.filename) + 1;
    if (send(ft_state.sockfd, ft_state.filename, filename_len, 0) != (ssize_t)filename_len)
    {
        printf("[FT] ERROR: Failed to send filename\n");
        return -1;
    }
    
    // Receive status
    if (recv(ft_state.sockfd, &status, 1, 0) != 1)
    {
        printf("[FT] ERROR: Failed to receive status\n");
        return -1;
    }
    
    if (status == FT_PROTO_RESP_ERROR)
    {
        // Read count byte even for errors
        recv(ft_state.sockfd, &count, 1, 0);
        printf("[FT] Server returned error\n");
        ft_state.status = FT_STATUS_ERROR;
        return -1;
    }
    
    if (status != FT_PROTO_RESP_OK && status != FT_PROTO_RESP_EOF)
    {
        printf("[FT] ERROR: Unknown status: 0x%02X\n", status);
        ft_state.status = FT_STATUS_ERROR;
        return -1;
    }
    
    // Receive count byte
    if (recv(ft_state.sockfd, &count, 1, 0) != 1)
    {
        printf("[FT] ERROR: Failed to receive count\n");
        return -1;
    }
    
    // Decode count (0 means 256)
    chunk_size = (count == 0) ? 256 : count;
    
    // Store count byte in buffer
    ft_state.chunk_buffer[0] = count;
    
    // Receive data bytes
    size_t total_received = 0;
    while (total_received < chunk_size)
    {
        ssize_t n = recv(ft_state.sockfd, &ft_state.chunk_buffer[1 + total_received],
                        chunk_size - total_received, 0);
        if (n <= 0)
        {
            printf("[FT] ERROR: Failed to receive data\n");
            return -1;
        }
        total_received += n;
    }
    
    ft_state.chunk_len = chunk_size + 1; // +1 for count byte
    ft_state.chunk_position = 0;
    ft_state.file_offset += chunk_size;  // Update offset for next request
    ft_state.status = (status == FT_PROTO_RESP_EOF) ? FT_STATUS_EOF : FT_STATUS_DATA_READY;
    
    return 0;
}

static int send_close(void)
{
    if (!ft_state.connected)
        return 0;
    
    uint8_t cmd = FT_PROTO_CLOSE;
    send(ft_state.sockfd, &cmd, 1, 0);
    
    // Send filename for cache eviction
    size_t filename_len = strlen(ft_state.filename) + 1;
    send(ft_state.sockfd, ft_state.filename, filename_len, 0);
    
    // Try to receive response but don't fail if we can't
    uint8_t status;
    recv(ft_state.sockfd, &status, 1, 0);
    
    return 0;
}

static void disconnect_from_server(void)
{
    if (ft_state.sockfd >= 0)
    {
        close(ft_state.sockfd);
        ft_state.sockfd = -1;
    }
    ft_state.connected = false;
    printf("[FT] Disconnected\n");
}
