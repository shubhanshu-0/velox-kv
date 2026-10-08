# VeloxKV

VeloxKV is a high-performance, single-threaded, non-blocking in-memory key-value cache server written in C++17.

## Overview

- **Asynchronous Non-Blocking I/O:** Uses POSIX `poll()` event multiplexing to handle thousands of concurrent client connections on a single OS thread.
- **Lock-Free Cache Engine:** Integrated LRU eviction policy with zero mutex overhead and zero virtual call indirection.
- **Threadless TTL Expiration:** Passive expiration on read accesses combined with active non-blocking sampling.
- **Protocol:** Text-based protocol supporting `SET`, `GET`, `DEL`, `STATS`, `INFO`, `HELP`, and `QUIT`.

## Getting Started

```bash
make

# Start the server (default port: 6380, capacity: 10000)
./velox-kv-server
```

In another terminal, connect using `nc`, `telnet`, or Python:

```bash
nc localhost 6380
```

Example session:

```text
> SET name alice
OK

> GET name
VALUE alice

> SET session_token 12345 10
OK

> STATS
STATS:
  hits: 1
  misses: 0
  hit_rate: 100.000000%
  evictions: 0
  active_connections: 1

> QUIT
OK
```

## Running Tests

```bash
make test
```

Exercises basic key-value operations and high-volume throughput tests against the `LRUCache` engine.

## Architecture Overview

```text
  [ Client Connections ]
     │  nc / python / sockets
     ▼
┌─────────────────────────────────────────────────────────────┐
│  Single-Threaded Non-Blocking Network Server                │
│  - POSIX poll() Event Multiplexer                           │
│  - Non-Blocking Sockets (O_NONBLOCK)                        │
│  - Line-Buffered Stream Parser                              │
└──────────────────────────────┬──────────────────────────────┘
                               │ Direct Execution (Zero Locks)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│  Lock-Free LRU Cache Engine                                 │
│  ├── std::unordered_map<K, Node*>  ───> O(1) Key Lookups   │
│  ├── Doubly-Linked List (Sentinel) ───> MRU / LRU Recency   │
│  └── Threadless Expiration Manager ───> Passive/Active TTL  │
└─────────────────────────────────────────────────────────────┘
```

## Protocol Overview

| Command | Example | Meaning |
|---------|---------|---------|
| `SET` | `SET name alice [ttl]` | Store a key/value pair with optional TTL in seconds |
| `GET` | `GET name` | Retrieve a value |
| `DEL` | `DEL name` | Delete a key |
| `STATS` | `STATS` | View server cache metrics & active connection count |
| `INFO` | `INFO` | View server version & capacity details |
| `HELP` | `HELP` | List supported commands |
| `QUIT` | `QUIT` | Close the client connection |

## Repository Structure

```text
include/
├── core/                # Cache interfaces and threadless TTL manager
├── policies/            # High-performance LRU cache engine
└── server/              # Non-blocking poll() TCP server interface
src/
└── server/              # Server daemon entry point and socket handling
tests/
└── tests.cpp            # Unit & throughput test suite
Makefile                 # Build configuration
```
