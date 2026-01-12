# Remote File System Server

This Python server provides remote disk access for the Altair 8800 emulator running on Raspberry Pi Pico W.

## Overview

The server handles disk sector read/write requests over TCP, allowing the Pico to use disk images stored on a remote machine. Each client (identified by IP address) gets their own copy of the disk images, enabling multiple Altair emulators to operate independently.

## Requirements

- Python 3.7+
- No external dependencies (uses standard library only)

## Quick Start

1. Clone this repository to your server machine
2. Navigate to the `RemoteFS` directory
3. Run the server:

```bash
python3 remote_fs_server.py
```

The server will:
- Listen on port 8085 by default
- Use the `disks/` directory (parent of RemoteFS) as the template disk source
- Store per-client disk copies in `RemoteFS/clients/`

## Command Line Options

```
usage: remote_fs_server.py [-h] [--host HOST] [--port PORT]
                           [--template-dir TEMPLATE_DIR]
                           [--clients-dir CLIENTS_DIR] [--debug]

Remote File System Server for Altair 8800 Emulator

optional arguments:
  -h, --help            show this help message and exit
  --host HOST           Host address to bind to (default: 0.0.0.0)
  --port PORT           Port to listen on (default: 8085)
  --template-dir PATH   Directory containing template disk images
                        (default: ../disks)
  --clients-dir PATH    Directory for per-client disk storage
                        (default: ./clients)
  --debug               Enable debug logging
```

## Directory Structure

```
pico-altair-8800/
├── disks/                    # Template disk images
│   ├── cpm63k.dsk
│   ├── bdsc-v1.60.dsk
│   ├── escape-posix.dsk
│   └── blank.dsk
└── RemoteFS/
    ├── remote_fs_server.py   # Server script
    ├── requirements.txt
    ├── README.md
    └── clients/              # Per-client disk storage (auto-created)
        ├── 192_168_1_100/    # Folder for client 192.168.1.100
        │   ├── cpm63k.dsk
        │   ├── bdsc-v1.60.dsk
        │   └── ...
        └── 192_168_1_101/    # Folder for client 192.168.1.101
            └── ...
```

## Protocol

The server uses a simple binary protocol over TCP:

### Commands (Client → Server)

| Command | Value | Request Payload | Response |
|---------|-------|-----------------|----------|
| INIT | 0x03 | (none) | status (1 byte) |
| READ_SECTOR | 0x01 | drive + track + sector (3 bytes) | status (1) + data (137 bytes) |
| WRITE_SECTOR | 0x02 | drive + track + sector + data (3 + 137 bytes) | status (1 byte) |

### Response Status

- `0x00`: OK
- `0xFF`: Error

## Building the Pico Firmware with Remote FS

To build the Altair 8800 emulator with Remote FS support:

```bash
mkdir build && cd build
cmake -DREMOTE_FS=ON -DREMOTE_FS_SERVER_IP="192.168.1.151" -DPICO_BOARD=pico2_w ..
make
```

## Troubleshooting

### Connection Refused
- Ensure the server is running and listening on the correct port
- Check firewall settings allow incoming connections on port 8085
- Verify the Pico is on the same network and can reach the server IP

### No Disk Images Found
- Ensure disk images exist in the `disks/` directory
- Expected filenames: `cpm63k.dsk`, `bdsc-v1.60.dsk`, `escape-posix.dsk`, `blank.dsk`

### Permission Denied
- Ensure the server process has read/write access to the disk images
- Check that the `clients/` directory can be created and written to
