# Design Notes — VeloxKV

This document describes the high-performance architecture of VeloxKV.

## 1. System Shape

VeloxKV is built with a single-threaded non-blocking architecture:

- **Server Network Layer:** POSIX `poll()` event multiplexing for non-blocking TCP socket I/O.
- **Cache Engine Layer:** Single-layer lock-free `LRUCache` with integrated TTL expiration and metrics.

```text
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

## 2. Event-Driven Network Model

The server avoids the overhead of spawning an OS thread per client connection:

1. Listening socket and client sockets are set to non-blocking mode (`O_NONBLOCK` via `fcntl`).
2. Sockets are registered with POSIX `poll()`.
3. Incoming TCP streams are accumulated in per-client `ClientSession` read buffers, parsing complete line commands (`\n` or `\r\n`).
4. Thousands of client connections can be multiplexed efficiently on **1 single thread** with kilobytes of RAM overhead.

## 3. Lock-Free LRU Cache Engine

The cache layer combines an `unordered_map` for $O(1)$ lookups and a doubly-linked list with sentinel nodes for recency tracking.

* **Zero Mutex Overhead:** Since execution is single-threaded inside the event loop, all mutexes (`std::shared_mutex`, `std::mutex`) and atomic variables have been eliminated.
* **Direct Execution:** Key lookups and updates execute directly without virtual dispatch or wrapper indirection.

## 4. Expiration and TTL Handling

TTL handling is **threadless**:

* **Passive Expiration:** Key read calls (`GET`) check timestamp and TTL; expired keys are purged immediately on access.
* **Active Purging:** Non-blocking `purge_expired()` calls can be triggered directly during event loop idle cycles.
