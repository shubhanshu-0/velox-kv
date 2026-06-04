# VeloxKV — High-Performance In-Memory Cache

A **production-ready, thread-safe key-value cache** built from scratch in C++ with multiple eviction policies, TTL support, and a TCP server interface.

```
SET key value  →  [Concurrent Cache Manager]  →  OK
GET key        →  [DLL + HashMap Lookup]     →  VALUE: value
```

---

## ⚡ Features

✅ **Three Cache Architectures**
- `SERIAL` — Single-threaded (benchmarking baseline)
- `CONCURRENT` — Shared reader-writer lock (thread-safe)
- `SHARDED` — 16-way sharding (high-concurrency)

✅ **Eviction Policies**
- **LRU** (Least Recently Used) — O(1) via DLL + HashMap
- Pluggable strategy pattern (easy to add LFU, FIFO, etc.)

✅ **Advanced Features**
- TTL/Expiration with background eviction threads
- Comprehensive STATS/INFO commands
- Text-based protocol 

✅ **Modern C++17**
- Smart pointers (`shared_ptr`, `unique_ptr`)
- Templates & generic programming
- Thread-safe primitives (`std::shared_mutex`, `std::atomic`)

---

## 🚀 Quick Start

### Build
```bash
make
```

### Run Server
```bash
./velox-kv-server
```

### Run Client (Interactive)
```bash
./velox-kv-client
```

### Example Session
```
> INIT serial 10000
OK: SERIAL cache ready (capacity: 10000)

> SET username alice
OK

> GET username
VALUE: alice

> DEL username
1

> QUIT
```

### Run Unit Tests
```bash
./velox-kv-tests
```

---

## 🏗️ Architecture

### System Design
```
┌─────────────────────────────────────────────────────┐
│           TCP Server (Port 6380)                    │
│      Thread-per-connection + Text Protocol         │
└──────────────────┬──────────────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
   ┌────▼────┐         ┌─────▼──────┐
   │  SERIAL │         │ CONCURRENT │
   │ (no locks)        │(shared_mutex)
   └────┬────┘         └─────┬──────┘
        │                    │
        └────────┬───────────┘
                 │
        ┌────────▼────────┐
        │ Cache Manager   │
        │  Interface      │
        └────────┬────────┘
                 │
        ┌────────▼────────┐
        │  LRU Policy     │
        │ DLL + HashMap   │ ← O(1) GET/SET/DEL
        │   (64KB cache)  │
        └─────────────────┘
```

### Data Structure: LRU Cache
```
Memory Layout:
┌─────────────┐
│   HashMap   │  ← O(1) key lookup
│  [key→node] │
└─────────────┘
       │
       ├──────┐
       │      │
   ┌───▼──┐ ┌─▼───┐ ┌───┐ ┌────┐
   │HEAD  │→│Node1│→│...│→│TAIL│  ← MRU...LRU (DLL)
   └──────│←┴──┬──┴←┴───┴←┴─┬──┘
          └────┴──────────┘
```

### Cache Types Comparison

| Type | Locking | Best For | Throughput |
|------|---------|----------|-----------|
| **SERIAL** | None | Single-threaded, baseline | Baseline |
| **CONCURRENT** | Global `shared_mutex` | Moderate concurrency | ~2-3x |
| **SHARDED** | 16 per-shard mutexes | High concurrency | ~8-12x |

---

## 📡 Protocol (Text-based)

### Commands

| Command | Format | Response | Example |
|---------|--------|----------|---------|
| **INIT** | `INIT <type> <capacity>` | `OK: TYPE ready (capacity: N)` | `INIT concurrent 10000` |
| **SET** | `SET <key> <value> [ttl]` | `OK` | `SET name alice` |
| **GET** | `GET <key>` | `VALUE: value` or `nil` | `GET name` |
| **DEL** | `DEL <key>` | `1` (deleted) or `0` (not found) | `DEL name` |
| **FLUSH** | `FLUSH` | `OK` | `FLUSH` |
| **STATS** | `STATS` | Stats output | `STATS` |
| **QUIT** | `QUIT` | `OK` | `QUIT` |

### Example Interaction
```bash
$ telnet localhost 6380

INIT serial 1000
OK: SERIAL cache ready (capacity: 1000)

SET mykey myvalue
OK

GET mykey
VALUE: myvalue

DEL mykey
1

GET mykey
nil

QUIT
OK
```

---

## 📊 Project Structure

```
velox-kv/
├── include/
│   ├── core/
│   │   ├── cache_base.hpp          # Abstract Cache<K,V> interface
│   │   ├── cache_manager.hpp       # Serial/Concurrent/Sharded implementations
│   │   └── cache_expiration.hpp    # TTL + background eviction
│   ├── policies/
│   │   └── lru.hpp                 # LRU eviction policy
│   └── server/
│       └── tcp_server.hpp          # TCP server interface
├── src/
│   ├── server/
│   │   ├── main.cpp                # Server entry point
│   │   └── tcp_server.cpp          # Socket + connection handling
│   └── (header-only templates, no other .cpp)
├── tests/
│   ├── tests.cpp                   # Unit tests (all 3 managers)
│   └── test_client.cpp             # Interactive test client
├── Makefile                        # g++ build (no external deps except pthread)
└── docs/               
    └── design.md                      # Deep-dive architecture
```

---

## 🔧 Build Details

### Requirements
- **C++17** (or later)
- **pthread** (standard on macOS/Linux)
- `g++` or `clang++`

### Build Options
```bash
make                # Build all (server, tests, client)
make server         # Build only server
make test           # Build only unit tests
make client         # Build only interactive client
make clean          # Remove binaries
```

### Compilation
```bash
g++ -std=c++17 -Wall -Wextra -O3 -I. -lpthread src/server/main.cpp src/server/tcp_server.cpp -o velox-kv-server
```

---

## 🧪 Testing

### Unit Tests (Cache Correctness)
Tests all 3 cache manager types with concurrent operations:
```bash
./velox-kv-tests
```

Expected output:
```
✓ Running SERIAL cache test...
✓ Running CONCURRENT cache test...
✓ Running SHARDED cache test...
✓ All tests passed!
```

### Integration Test (Server + Client)
**Terminal 1:**
```bash
./velox-kv-server
```

**Terminal 2:**
```bash
./velox-kv-client
```

Test commands:
```
> INIT concurrent 5000
> SET key1 value1
> GET key1
> SET key2 value2
> DEL key1
> GET key1
> QUIT
```

### Manual Testing with Telnet
```bash
# Start server in one terminal
./velox-kv-server

# In another terminal
telnet localhost 6380
> INIT serial 1000
> SET test data
> GET test
> QUIT
```
---

## 📈 Performance Characteristics

| Operation | Time | Notes |
|-----------|------|-------|
| GET/SET/DEL | O(1) | Average case, hash map lookup |
| Eviction | O(1) | Just move node in DLL |
| Cache Miss | O(hash)| Depends on hash collision rate |
| TTL Check | O(1) lazy | Checked on get(), not background |
