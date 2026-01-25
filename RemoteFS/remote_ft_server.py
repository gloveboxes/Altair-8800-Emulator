#!/usr/bin/env python3
"""
Remote File Transfer Server for Altair 8800 Emulator

This server handles file GET requests from clients over TCP.
Stateless protocol - server can restart without breaking clients.

Protocol (synchronous request-response):
- CMD_SET_FILENAME (0x01): Receive null-terminated filename (deprecated, optional)
- CMD_GET_CHUNK (0x02): [offset:4][filename:null-terminated] -> data chunk
- CMD_CLOSE (0x03): Close cached file handle (optional optimization)

GET_CHUNK request format:
- offset: 4 bytes, little-endian, byte offset in file
- filename: null-terminated UTF-8 string

GET_CHUNK response format:
- status byte:
  * 0x00 (OK): 1-256 valid bytes, count=0 encodes 256
  * 0x01 (EOF): 1-256 valid bytes, count=0 encodes 256 (final chunk)
  * 0xFF (ERROR): status+count only, no data payload
- count byte: Number of valid bytes (0 means 256)
- data: exactly 'count' bytes (no padding)

File handle caching:
- Server optionally caches open file handles for performance
- Cache is transparent to protocol - purely an optimization
- Clients don't depend on caching and work across server restarts
"""

import os
import sys
import socket
import asyncio
import argparse
import logging
import time
from pathlib import Path
from collections import OrderedDict

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


class FileHandleCache:
    """LRU cache for open file handles with TTL"""
    
    def __init__(self, max_size=100, ttl_seconds=300):
        self.cache = OrderedDict()  # key -> (file_handle, last_access_time)
        self.max_size = max_size
        self.ttl_seconds = ttl_seconds
        self.lock = asyncio.Lock()
    
    async def get(self, key):
        """Get cached file handle, return None if not found or expired"""
        async with self.lock:
            if key not in self.cache:
                return None
            
            file_handle, last_access = self.cache[key]
            
            # Check if expired
            if time.time() - last_access > self.ttl_seconds:
                file_handle.close()
                del self.cache[key]
                return None
            
            # Move to end (most recently used)
            self.cache.move_to_end(key)
            self.cache[key] = (file_handle, time.time())
            return file_handle
    
    async def put(self, key, file_handle):
        """Add file handle to cache"""
        async with self.lock:
            # Close existing if present
            if key in self.cache:
                old_handle, _ = self.cache[key]
                old_handle.close()
                del self.cache[key]
            
            # Evict oldest if at capacity
            if len(self.cache) >= self.max_size:
                oldest_key, (oldest_handle, _) = self.cache.popitem(last=False)
                oldest_handle.close()
                logger.debug(f"Evicted cached file: {oldest_key}")
            
            self.cache[key] = (file_handle, time.time())
    
    async def remove(self, key):
        """Remove and close cached file handle"""
        async with self.lock:
            if key in self.cache:
                file_handle, _ = self.cache[key]
                file_handle.close()
                del self.cache[key]
    
    async def clear(self):
        """Close all cached file handles"""
        async with self.lock:
            for file_handle, _ in self.cache.values():
                file_handle.close()
            self.cache.clear()


class ClientSession:
    """Handles a single client connection"""
    
    def __init__(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter, files_dir: Path, file_cache: FileHandleCache):
        self.reader = reader
        self.writer = writer
        self.addr = writer.get_extra_info('peername')
        self.client_ip = self.addr[0] if self.addr else 'unknown'
        self.files_dir = files_dir
        self.file_cache = file_cache
        self.running = True
        
        # Legacy state for SET_FILENAME command (deprecated)
        self.current_file = None
        self.filename = ""
        self.file_position = 0
        
    async def handle(self):
        """Main handler loop for client connection"""
        logger.info(f"Client connected: {self.client_ip}")
        
        try:
            while self.running:
                # Read command byte
                cmd_data = await self._recv_exact(1)
                if not cmd_data:
                    break
                    
                cmd = cmd_data[0]
                logger.info(f"[{self.client_ip}] Received command: 0x{cmd:02X}")
                
                if cmd == CMD_SET_FILENAME:
                    await self._handle_set_filename()
                elif cmd == CMD_GET_CHUNK:
                    await self._handle_get_chunk()
                elif cmd == CMD_CLOSE:
                    await self._handle_close()
                else:
                    logger.warning(f"Unknown command: 0x{cmd:02X}")
                    self.writer.write(bytes([RESP_ERROR]))
                    await self.writer.drain()
                    
        except ConnectionResetError:
            logger.info(f"Client disconnected: {self.client_ip}")
        except Exception as e:
            logger.error(f"Error handling client {self.client_ip}: {e}")
        finally:
            await self._close_file()
            try:
                self.writer.close()
                await self.writer.wait_closed()
            except:
                pass
            logger.info(f"Connection closed: {self.client_ip}")
    
    async def _recv_exact(self, size: int) -> bytes:
        """Receive exactly 'size' bytes from the connection"""
        try:
            data = await self.reader.readexactly(size)
            return data
        except asyncio.IncompleteReadError:
            return None
        except Exception:
            return None
    
    async def _recv_until_null(self) -> str:
        """Receive bytes until null terminator"""
        data = b''
        try:
            while True:
                byte = await self.reader.readexactly(1)
                if byte[0] == 0:
                    break
                data += byte
                if len(data) > 256:  # Safety limit
                    break
            return data.decode('utf-8', errors='replace')
        except:
            return None
    
    async def _handle_set_filename(self):
        """Handle SET_FILENAME command"""
        # Close any existing file
        await self._close_file()
        
        # Receive null-terminated filename
        filename = await self._recv_until_null()
        if filename is None:
            return
            
        # Parse URI (Case-insensitive scheme)
        filename_lower = filename.lower()
        if filename_lower.startswith('file://'):
            filename = filename[7:] # Keep original case for path (mostly) or strip safely
        elif filename_lower.startswith('https://') or filename_lower.startswith('http://'):
            # Future support for internet proxy
            logger.warning(f"[{self.client_ip}] HTTP/HTTPS not yet supported: {filename}")
            self.writer.write(bytes([RESP_ERROR]))
            await self.writer.drain()
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
                self.writer.write(bytes([RESP_ERROR]))
                await self.writer.drain()
                return
                
            self.current_file = open(filepath, 'rb')
            self.file_position = 0
            self.writer.write(bytes([RESP_OK]))
            await self.writer.drain()
            logger.info(f"[{self.client_ip}] File opened: {filename}")
            
        except Exception as e:
            logger.error(f"[{self.client_ip}] Error opening file: {e}")
            self.writer.write(bytes([RESP_ERROR]))
            await self.writer.drain()
    
    async def _handle_get_chunk(self):
        """Handle GET_CHUNK command (stateless)

        Request format:
        [offset:4 bytes little-endian][filename:null-terminated]
        
        Response format:
        [status:1][count:1][data:count]
        count = valid bytes in this chunk (0-255, 256 encoded as 0)
        """
        try:
            # Read offset (4 bytes, little-endian)
            offset_data = await self._recv_exact(4)
            if not offset_data:
                return
            offset = int.from_bytes(offset_data, byteorder='little')
            
            # Read filename (null-terminated)
            filename = await self._recv_until_null()
            if filename is None:
                return
            
            # Parse URI and sanitize filename
            filename_lower = filename.lower()
            if filename_lower.startswith('file://'):
                filename = filename[7:]
            elif filename_lower.startswith('https://') or filename_lower.startswith('http://'):
                logger.warning(f"[{self.client_ip}] HTTP/HTTPS not yet supported: {filename}")
                self.writer.write(bytes([RESP_ERROR, 0]))
                await self.writer.drain()
                return
            
            # Sanitize filename
            filename = filename.replace('..', '').lstrip('/')
            filepath = self.files_dir / filename
            
            if not filepath.exists():
                logger.warning(f"[{self.client_ip}] File not found: {filepath}")
                self.writer.write(bytes([RESP_ERROR, 0]))
                await self.writer.drain()
                return
            
            # Try to get cached file handle
            cache_key = str(filepath)
            file_handle = await self.file_cache.get(cache_key)
            
            if file_handle is None:
                # Open new file and cache it
                file_handle = open(filepath, 'rb')
                await self.file_cache.put(cache_key, file_handle)
                logger.debug(f"[{self.client_ip}] Opened and cached: {filename}")
            else:
                logger.debug(f"[{self.client_ip}] Using cached file: {filename}")
            
            # Seek to requested offset
            file_handle.seek(offset)
            
            # Read up to CHUNK_SIZE bytes + 1 to check for EOF in one operation
            data = file_handle.read(CHUNK_SIZE + 1)
            bytes_read = len(data)
            
            if bytes_read == 0:
                # At or past EOF - treat as error
                response = bytes([RESP_ERROR, 0])
                self.writer.write(response)
                await self.writer.drain()
                logger.debug(f"[{self.client_ip}] Sent ERROR for offset {offset} (EOF)")
                return
            
            # Determine status based on bytes read
            if bytes_read > CHUNK_SIZE:
                # More data available (read the extra byte)
                status = RESP_OK
                data = data[:CHUNK_SIZE]  # Trim to CHUNK_SIZE
                bytes_read = CHUNK_SIZE
            else:
                # This is the last chunk
                status = RESP_EOF
            
            # Encode count: 256 -> 0, otherwise actual count
            count_byte = bytes_read if bytes_read < 256 else 0
            
            # Response format: status + count + data (no padding)
            response = bytes([status, count_byte]) + data
            self.writer.write(response)
            await self.writer.drain()
            
            logger.info(f"[{self.client_ip}] Sent chunk: offset={offset}, bytes={bytes_read}, status={'EOF' if status == RESP_EOF else 'OK'}")
            
        except Exception as e:
            logger.error(f"[{self.client_ip}] Error reading chunk: {e}")
            response = bytes([RESP_ERROR, 0])
            self.writer.write(response)
            await self.writer.drain()
    
    async def _handle_close(self):
        """Handle CLOSE command (optional cache eviction)"""
        # Read filename (null-terminated)
        filename = await self._recv_until_null()
        if filename is None:
            return
        
        # Sanitize and build path
        filename_lower = filename.lower()
        if filename_lower.startswith('file://'):
            filename = filename[7:]
        filename = filename.replace('..', '').lstrip('/')
        filepath = self.files_dir / filename
        
        # Remove from cache if present
        cache_key = str(filepath)
        await self.file_cache.remove(cache_key)
        
        logger.info(f"[{self.client_ip}] Closed cached file: {filename}")
        
        # Also close legacy state if any
        await self._close_file()
        
        self.writer.write(bytes([RESP_OK]))
        await self.writer.drain()
    
    async def _close_file(self):
        """Close current file if open"""
        if self.current_file:
            self.current_file.close()
            self.current_file = None
            self.filename = ""
            self.file_position = 0


class RemoteFTServer:
    """Remote File Transfer Server"""
    
    def __init__(self, host: str, port: int, files_dir: Path, cache_size: int = 100, cache_ttl: int = 300):
        self.host = host
        self.port = port
        self.files_dir = files_dir
        self.running = False
        self.server = None
        self.file_cache = FileHandleCache(max_size=cache_size, ttl_seconds=cache_ttl)
        
    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        """Handle a new client connection"""
        # Set TCP_NODELAY for lower latency
        sock = writer.get_extra_info('socket')
        if sock:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            # Enable keepalive for better connection management
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            # Optimize send buffer (64KB)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
        
        session = ClientSession(reader, writer, self.files_dir, self.file_cache)
        await session.handle()
        
    async def start(self):
        """Start the server"""
        # Ensure directory exists
        if not self.files_dir.exists():
            logger.error(f"Files directory not found: {self.files_dir}")
            logger.error("Please create the directory or specify a valid --files-dir")
            sys.exit(1)
            
        # Create server with increased backlog
        self.server = await asyncio.start_server(
            self.handle_client,
            self.host,
            self.port,
            backlog=128  # Increased from 5 to 128 for better scalability
        )
        
        self.running = True
        logger.info(f"Remote FT Server listening on {self.host}:{self.port}")
        logger.info(f"Files directory: {self.files_dir}")
        logger.info(f"Async mode: backlog=128, optimized for low-latency")
        
        async with self.server:
            await self.server.serve_forever()
    
    async def stop(self):
        """Stop the server"""
        self.running = False
        if self.server:
            self.server.close()
            await self.server.wait_closed()
        await self.file_cache.clear()
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
    parser.add_argument(
        '--cache-size', type=int, default=100,
        help='Maximum number of cached file handles (default: 100)'
    )
    parser.add_argument(
        '--cache-ttl', type=int, default=300,
        help='Cache TTL in seconds (default: 300)'
    )
    
    args = parser.parse_args()
    
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    print("FT server protocol v3 (stateless with caching) - Async mode")

    server = RemoteFTServer(
        host=args.host,
        port=args.port,
        files_dir=args.files_dir,
        cache_size=args.cache_size,
        cache_ttl=args.cache_ttl
    )
    
    try:
        asyncio.run(server.start())
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    except Exception as e:
        logger.error(f"Server error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
