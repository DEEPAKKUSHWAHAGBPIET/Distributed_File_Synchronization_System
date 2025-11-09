# Problem Statement 

Traditional file transfer tools resend the entire file after every change, even for small edits.

### 🧠 Rsync-Lite: Efficient File Synchronization System 

A lightweight efficient file synchronization system, that is used to synchronize files between clients and a server by transferring only the changed portions.

### System Overview

```
                      ┌────────────────────────────┐
                      │         CLIENT(S)          │
                      │────────────────────────────│
                      │ - Reads file (sample.txt)  │
                      │ - Splits into 1KB blocks   │
                      │ - Computes checksums       |
                      │ - Compress the blocks      |
                      │ - Sends to server via TCP  │
                      └──────────────┬─────────────┘
                                     │
                          ┌──────────▼──────────┐
                          │       SERVER        │
                          │─────────────────────│
                          │ - Receives blocks   │
                          │ - Compares hashes   │
                          │ - Requests deltas   │
                          | - Decompress                    |
                          │ - Updates file      │
                          │ - Saves index.db    │
                          └──────────┬──────────┘
                                     │
                     ┌───────────────▼───────────────┐
                     │         syncedData/           │
                     │ Stores latest file versions   │
                     │ and persistent index(index.db)│
                     └───────────────────────────────┘

```

### Core Features

| Feature                         | Description                                                                                  |
| ------------------------------- | -------------------------------------------------------------------------------------------- |
| 🧮 **Delta Detection**          | Detects only modified parts using checksum comparison per block.                             |
| 💾 **Persistent Index Storage** | Stores file signatures (checksums) across sessions for incremental syncs.                    |
| 🗜️ **Compression (zlib)**      | Compresses blocks before sending, reducing bandwidth use.                                    |
| 🌐 **Client–Server Protocol**   | Custom TCP-based protocol using messages (`FILE_HDR`, `BLOCK_DATA`, `BLOCK_END`, `FILE_OK`). |
| 🤝 **Multi-Client Support**     | Handles concurrent syncs with per-client threads and shared index database.                  |


### Technical Highlights

Language: C
---

**Libraries Used:** zlib, OpenSSL (MD5), pthread

**Protocols:** Custom TCP command protocol

**Sync Type:** Fixed-size block-based delta sync

**Compression:** zlib (deflate/inflate)

**Persistence:** On-disk index file storing checksum metadata

---

# Results

✅ First-time sync transfers full file.  
✅ On subsequent syncs, only changed blocks are transferred.  
✅ Demonstrated bandwidth savings and reduced sync latency.  
✅ Supports multiple clients updating the same server file. 

### Compile Server

```
gcc -o server/server \
    server/server.c \
    server/index_store.c \
    common_utils/file_hasher.c \
    common_utils/compressor.c \
    -Icommon_utils -lpthread -lssl -lcrypto -lz

```

### Start server

```
./server/server

```
## Your synced file will appear under:
```
server/syncedData/sample.txt

```

### Compile client 

```
gcc -o client/client \
    client/client.c \
    common_utils/file_hasher.c \
    common_utils/compressor.c \
    -Icommon_utils -lssl -lcrypto -lz

```
### Run client

```
./client/client sample.txt

```

### Download latest copy from server
```
./client/client sample.txt --get

```
