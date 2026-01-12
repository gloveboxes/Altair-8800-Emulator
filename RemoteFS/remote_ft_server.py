#!/usr/bin/env python3
"""
Remote File Transfer Server for Altair 8800 Emulator

This server handles file GET requests from Pico clients over TCP.
Each client (identified by IP address) can download files in 256-byte chunks.

Protocol (synchronous request-response):
- CMD_SET_FILENAME (0x01): Receive null-terminated filename, open file
- CMD_GET_CHUNK (0x02): Return [status:1][count:1][data:count] (variable length)
- CMD_CLOSE (0x03): Close current file session

Response format:
- status byte:
  * 0x00 (OK): 1-256 valid bytes, count=0 encodes 256
  * 0x01 (EOF): 1-256 valid bytes, count=0 encodes 256 (final chunk)
  * 0xFF (ERROR): status+count only, no data payload
- count byte: Number of valid bytes (0 means 256)
- data: exactly 'count' bytes (no padding)

Client protocol flow:
1. Client sends GET_CHUNK request
2. Client blocks polling status port
3. Server sends status+count+data response
4. Client consumes all 256 data bytes
5. Client sends next GET_CHUNK (synchronous, no pipelining)
"""

import os
import sys
import socket
import threading
import argparse
import logging
from pathlib import Path

# Protocol constants
CMD_SET_FILENAME = 0x01
CMD_GET_CHUNK = 0x02
CMD_CLOSE = 0x03

RESP_OK = 0x00
RESP_EOF = 0x01
RESP_ERROR = 0xFF

CHUNK_SIZE = 256

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


class ClientSession:
    """Handles a single client connection"""
    
    def __init__(self, conn: socket.socket, addr: tuple, files_dir: Path):
        self.conn = conn
        self.addr = addr
        self.client_ip = addr[0]
        self.files_dir = files_dir
        self.running = True
        
        # Current file state
        self.current_file = None
        self.filename = ""
        self.file_position = 0
        
    def handle(self):
        """Main handler loop for client connection"""
        logger.info(f"Client connected: {self.client_ip}")
        
        try:
            while self.running:
                # Read command byte
                cmd_data = self._recv_exact(1)
                if not cmd_data:
                    break
                    
                cmd = cmd_data[0]
                logger.info(f"[{self.client_ip}] Received command: 0x{cmd:02X}")
                
                if cmd == CMD_SET_FILENAME:
                    self._handle_set_filename()
                elif cmd == CMD_GET_CHUNK:
                    self._handle_get_chunk()
                elif cmd == CMD_CLOSE:
                    self._handle_close()
                else:
                    logger.warning(f"Unknown command: 0x{cmd:02X}")
                    self.conn.sendall(bytes([RESP_ERROR]))
                    
        except ConnectionResetError:
            logger.info(f"Client disconnected: {self.client_ip}")
        except Exception as e:
            logger.error(f"Error handling client {self.client_ip}: {e}")
        finally:
            self._close_file()
            self.conn.close()
            logger.info(f"Connection closed: {self.client_ip}")
    
    def _recv_exact(self, size: int) -> bytes:
        """Receive exactly 'size' bytes from the connection"""
        data = b''
        while len(data) < size:
            chunk = self.conn.recv(size - len(data))
            if not chunk:
                return None
            data += chunk
        return data
    
    def _recv_until_null(self) -> str:
        """Receive bytes until null terminator"""
        data = b''
        while True:
            byte = self.conn.recv(1)
            if not byte:
                return None
            if byte[0] == 0:
                break
            data += byte
            if len(data) > 256:  # Safety limit
                break
        return data.decode('utf-8', errors='replace')
    
    def _handle_set_filename(self):
        """Handle SET_FILENAME command"""
        # Close any existing file
        self._close_file()
        
        # Receive null-terminated filename
        filename = self._recv_until_null()
        if filename is None:
            return
            
        # Parse URI (Case-insensitive scheme)
        filename_lower = filename.lower()
        if filename_lower.startswith('file://'):
            filename = filename[7:] # Keep original case for path (mostly) or strip safely
        elif filename_lower.startswith('https://') or filename_lower.startswith('http://'):
            # Future support for internet proxy
            logger.warning(f"[{self.client_ip}] HTTP/HTTPS not yet supported: {filename}")
            self.conn.sendall(bytes([RESP_ERROR]))
            return
            
        # Sanitize filename (remove path traversal attempts)
        filename = filename.replace('..', '').lstrip('/')
        self.filename = filename
        
        # Open the file
        filepath = self.files_dir / filename
        logger.info(f"[{self.client_ip}] Opening file: {filepath}")
        
        try:
            if not filepath.exists():
                logger.warning(f"[{self.client_ip}] File not found: {filepath}")
                self.conn.sendall(bytes([RESP_ERROR]))
                return
                
            self.current_file = open(filepath, 'rb')
            self.file_position = 0
            self.conn.sendall(bytes([RESP_OK]))
            logger.info(f"[{self.client_ip}] File opened: {filename}")
            
        except Exception as e:
            logger.error(f"[{self.client_ip}] Error opening file: {e}")
            self.conn.sendall(bytes([RESP_ERROR]))
    
    def _handle_get_chunk(self):
        """Handle GET_CHUNK command

        Response format:
        [status:1][count:1][data:count]
        count = valid bytes in this chunk (0-255, 256 encoded as 0)
        """
        if self.current_file is None:
            logger.warning(f"[{self.client_ip}] GET_CHUNK with no file open")
            # Send error status + count=0 (no data payload)
            response = bytes([RESP_ERROR, 0])
            self.conn.sendall(response)
            return
            
        try:
            # Read up to CHUNK_SIZE bytes
            data = self.current_file.read(CHUNK_SIZE)
            bytes_read = len(data)
            
            if bytes_read == 0:
                # Already at EOF (empty file or read past end) - treat as error
                response = bytes([RESP_ERROR, 0])
                self.conn.sendall(response)
                self.file_position += bytes_read
                logger.debug(f"[{self.client_ip}] Sent ERROR for empty chunk")
                return
            
            # Check if there's more data
            next_byte = self.current_file.read(1)
            if next_byte:
                # More data available - put byte back and send OK
                self.current_file.seek(-1, 1)
                status = RESP_OK
            else:
                # This is the last chunk
                status = RESP_EOF
            
            # Encode count: 256 -> 0, otherwise actual count
            count_byte = bytes_read if bytes_read < 256 else 0
            
            # Response format: status + count + data (no padding)
            response = bytes([status, count_byte]) + data
            
            self.file_position += bytes_read
            self.conn.sendall(response)
            
            logger.info(f"[{self.client_ip}] Sent chunk: {bytes_read} bytes, status={'EOF' if status == RESP_EOF else 'OK'}")
            
        except Exception as e:
            logger.error(f"[{self.client_ip}] Error reading chunk: {e}")
            response = bytes([RESP_ERROR, 0])
            self.conn.sendall(response)
    
    def _handle_close(self):
        """Handle CLOSE command"""
        logger.info(f"[{self.client_ip}] Closing file: {self.filename}")
        self._close_file()
        self.conn.sendall(bytes([RESP_OK]))
    
    def _close_file(self):
        """Close current file if open"""
        if self.current_file:
            self.current_file.close()
            self.current_file = None
            self.filename = ""
            self.file_position = 0


class RemoteFTServer:
    """Remote File Transfer Server"""
    
    def __init__(self, host: str, port: int, files_dir: Path):
        self.host = host
        self.port = port
        self.files_dir = files_dir
        self.running = False
        self.server_socket = None
        
    def start(self):
        """Start the server"""
        # Ensure directory exists
        if not self.files_dir.exists():
            logger.error(f"Files directory not found: {self.files_dir}")
            logger.error("Please create the directory or specify a valid --files-dir")
            sys.exit(1)
            
        # Create server socket
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        
        self.running = True
        logger.info(f"Remote FT Server listening on {self.host}:{self.port}")
        logger.info(f"Files directory: {self.files_dir}")
        
        try:
            while self.running:
                try:
                    conn, addr = self.server_socket.accept()
                    
                    # Disable Nagle's algorithm for lower latency
                    conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    
                    # Handle client in a new thread
                    session = ClientSession(conn, addr, self.files_dir)
                    thread = threading.Thread(target=session.handle, daemon=True)
                    thread.start()
                    
                except KeyboardInterrupt:
                    break
                    
        finally:
            self.stop()
    
    def stop(self):
        """Stop the server"""
        self.running = False
        if self.server_socket:
            self.server_socket.close()
        logger.info("Server stopped")


def main():
    parser = argparse.ArgumentParser(
        description="Remote File Transfer Server for Altair 8800 Emulator"
    )
    parser.add_argument(
        '--host', default='0.0.0.0',
        help='Host address to bind to (default: 0.0.0.0)'
    )
    parser.add_argument(
        '--port', type=int, default=8090,
        help='Port to listen on (default: 8090)'
    )
    parser.add_argument(
        '--files-dir', type=Path,
        default=Path(__file__).parent / 'files',
        help='Directory containing files to serve (default: ./files)'
    )
    parser.add_argument(
        '--debug', action='store_true',
        help='Enable debug logging'
    )
    
    args = parser.parse_args()
    
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    print("FT server protocol v2")

    server = RemoteFTServer(
        host=args.host,
        port=args.port,
        files_dir=args.files_dir
    )
    
    try:
        server.start()
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    except Exception as e:
        logger.error(f"Server error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
