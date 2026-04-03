# Design Document — Concurrent In-Memory K-V Store

This document explains the data structures, memory model, concurrency strategy, and design decisions behind each component. Read this before working on a new phase.

---

## Table of Contents

1. [LRU Cache](#1-lru-cache)
2. [LFU Cache](#2-lfu-cache)
3. [Memory Model](#3-memory-model)
4. [Concurrency Design](#4-concurrency-design)
5. [TTL / Expiration](#5-ttl--expiration)
6. [Abstract Base Class Design](#6-abstract-base-class-design)
7. [Server Design](#7-server-design)
8. [Why These Choices](#8-why-these-choices)

---

## 1. LRU Cache

### What is LRU?
**Least Recently Used** — when the cache is full, the item that was accessed the longest time ago gets evicted.

### Why is it useful?
It exploits **temporal locality** — recently accessed items are more likely to be accessed again soon.

### Data Structures Used

**Doubly Linked List (DLL)** + **`std::unordered_map`**

```
head <-> [Node3] <-> [Node1] <-> [Node2] <-> tail
         (MRU)                             (LRU)
```

- `head->next` = Most Recently Used (MRU)
- `tail->prev` = Least Recently Used (LRU) — candidate for eviction
- `head` and `tail` are **sentinel nodes** (dummy nodes with no real data, just markers)

### Why Sentinel Nodes?
Without sentinels, every insert/delete needs null pointer checks for edge cases (empty list, single element). Sentinels simplify all operations to the same code path.

### How get() Works — O(1)

```
1. Look up key in unordered_map → get pointer to Node
2. Remove Node from its current position in DLL
3. Insert Node right after head (MRU position)
4. Return Node->value
```

### How put() Works — O(1)

```
Case 1: Key already exists
  → Update value
  → Move node to MRU position (same as get)

Case 2: Key doesn't exist AND cache not full
  → Create new Node
  → Insert after head (MRU position)
  → Add to unordered_map

Case 3: Key doesn't exist AND cache full
  → Evict: remove node before tail (LRU position)
  → Delete it from unordered_map
  → Proceed same as Case 2
```

### Why unordered_map and not map?
- `std::map` → O(log n) lookup (red-black tree)
- `std::unordered_map` → O(1) average lookup (hash table)
- For a cache, lookup speed is critical — always use `unordered_map`

---

## 2. LFU Cache

### What is LFU?
**Least Frequently Used** — when the cache is full, the item with the fewest accesses gets evicted. Ties broken by recency (LRU among same-frequency items).

### Why useful?
Better than LRU for workloads with hot items (some keys accessed very often, most accessed rarely). Avoids evicting popular items just because they haven't been used in the last moment.

### Data Structures Used

**Two hash maps + per-frequency doubly linked lists**

```
min_freq = 1

freq_map:
  1 → DLL: [KeyC] <-> [KeyD]   (accessed 1 time)
  2 → DLL: [KeyA] <-> [KeyB]   (accessed 2 times)
  5 → DLL: [KeyE]              (accessed 5 times)

key_map:
  KeyA → {value, freq=2, iterator_into_freq_map_dll}
  KeyB → {value, freq=2, ...}
  ...
```

### How get() Works — O(1)

```
1. Look up key in key_map
2. Get current frequency f
3. Remove node from freq_map[f] DLL
4. If freq_map[f] is now empty AND f == min_freq → min_freq++
5. Insert node into freq_map[f+1] DLL
6. Update key_map[key].freq = f+1
7. Return value
```

### How put() Works — O(1)

```
Case 1: Key exists → same as get() but also update value

Case 2: New key, cache not full
  → Insert into freq_map[1] at front
  → Add to key_map with freq=1
  → min_freq = 1

Case 3: New key, cache full
  → Evict: remove last node from freq_map[min_freq] DLL
  → Delete from key_map
  → Proceed same as Case 2, set min_freq = 1
```

### Why min_freq?
Tracks the current minimum frequency so eviction is O(1) — no need to scan all frequencies.

---

## 3. Memory Model

### Node Ownership

```
cache_map (unordered_map) → shared_ptr<Node>  (OWNER)
DLL (prev/next pointers)  → raw Node*          (OBSERVER)
```

**Rule:** The map owns the nodes. The DLL just observes them via raw pointers.

This avoids **circular shared_ptr references** which would cause memory leaks (ref count never hits 0).

### Why not unique_ptr?
`unique_ptr` can't be shared — we need both the map and the DLL to reference the same node.

### head and tail (Sentinel Nodes)
```cpp
std::shared_ptr<Node<K, V>> head;  // Owned by Cache
std::shared_ptr<Node<K, V>> tail;  // Owned by Cache
```
When `Cache` is destroyed, `head` and `tail` are auto-cleaned (no manual delete needed).

### Memory layout diagram

```
Cache object
├── unordered_map
│     ├── key1 → shared_ptr → Node {key1, val1, prev*, next*}
│     ├── key2 → shared_ptr → Node {key2, val2, prev*, next*}
│     └── key3 → shared_ptr → Node {key3, val3, prev*, next*}
├── head (shared_ptr) → dummy Node
└── tail (shared_ptr) → dummy Node

DLL traversal uses raw Node* pointers (no ownership)
```

---

## 4. Concurrency Design

### Problem
Without synchronization, two threads doing `get()` and `put()` simultaneously will corrupt the DLL (node pointers get mangled) or the hash map.

### Solution: `std::shared_mutex` (Read-Write Lock)

```
Multiple threads can READ simultaneously (shared_lock)
Only ONE thread can WRITE at a time (unique_lock)
```

```cpp
mutable std::shared_mutex rw_mutex;

// get() — read operation
V get(K key) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    // multiple threads can call this concurrently
}

// put(), remove() — write operations
bool put(K key, V value) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    // exclusive access — no other thread can read or write
}
```

### Why shared_mutex over regular mutex?
Caches are typically **read-heavy** (80-90% reads). With a regular `mutex`, even reads block each other. With `shared_mutex`, reads run in parallel — much higher throughput.

### Advanced: Shard-Based Locking (Optional)

Even with `shared_mutex`, all writes serialize. For very high write throughput, split into N shards:

```cpp
static const int NUM_SHARDS = 16;

struct Shard {
    std::unordered_map<K, SharedPtr<Node>> map;
    std::shared_mutex mutex;
};

Shard shards[NUM_SHARDS];

int getShard(K key) {
    return std::hash<K>{}(key) % NUM_SHARDS;
}
```

Now different keys can be written in parallel (if they hash to different shards). This is how production caches scale concurrency.

### Atomic Statistics (No Lock Needed)

```cpp
std::atomic<uint64_t> hit_count{0};   // Increment in get() on hit
std::atomic<uint64_t> miss_count{0};  // Increment in get() on miss
std::atomic<uint64_t> eviction_count{0}; // Increment in put() on eviction
```

`atomic` uses CPU-level compare-and-swap — no mutex needed, extremely fast.

---

## 5. TTL / Expiration

### Approach: Background Sweeper Thread

Each Node stores a `std::chrono::steady_clock::time_point` for when it expires:

```cpp
template <typename K, typename V>
struct Node {
    K key;
    V value;
    Node *prev, *next;
    std::chrono::steady_clock::time_point expires_at;
    bool has_ttl = false;
};
```

A background thread wakes every N milliseconds, scans the map for expired keys, and removes them:

```cpp
void sweeper_loop() {
    while (running) {
        std::unique_lock<std::mutex> lock(sweeper_mutex);
        // Wait N ms OR wake early if notified (on shutdown)
        sweeper_cv.wait_for(lock, std::chrono::milliseconds(sweep_interval_ms));

        if (!running) break;

        auto now = std::chrono::steady_clock::now();
        // Acquire write lock and evict expired keys
        std::unique_lock<std::shared_mutex> cache_lock(rw_mutex);
        for (auto it = cache_map.begin(); it != cache_map.end(); ) {
            if (it->second->has_ttl && it->second->expires_at <= now) {
                remove_from_dll(it->second.get());
                it = cache_map.erase(it);
                eviction_count++;
            } else {
                ++it;
            }
        }
    }
}
```

### Clean Shutdown
On `Cache` destruction, signal the sweeper thread to stop:
```cpp
~Cache() {
    running = false;
    sweeper_cv.notify_all();  // Wake it up so it sees running=false
    sweeper_thread.join();    // Wait for it to finish
}
```

---

## 6. Abstract Base Class Design

### Goal
Both `LRU` and `LFU` implement the same interface so they can be swapped at runtime or compile time.

```cpp
// cache_base.hpp
template <typename K, typename V>
class Cache {
public:
    virtual V get(K key) = 0;
    virtual bool put(K key, V value) = 0;
    virtual bool remove(K key) = 0;
    virtual ~Cache() = default;  // MUST be virtual for polymorphic delete
};
```

### Why virtual destructor?
If you delete an `LRU` via a `Cache*` pointer, without a virtual destructor, only the base destructor runs — the derived class destructor is skipped → memory leak.

### Usage
```cpp
// Swap policies at runtime
std::unique_ptr<Cache<int, int>> cache;

cache = std::make_unique<LRUCache<int, int>>(100);
// or
cache = std::make_unique<LFUCache<int, int>>(100);

cache->put(1, 100);
cache->get(1);
```

---

## 7. Server Design

### Protocol: Newline-delimited text commands

Simple text over TCP — easy to test with `telnet` or `nc`:

```bash
$ nc localhost 6380
SET name Alice
OK
GET name
Alice
```

### Connection Handling: Thread-per-connection (Phase 3 start)

```
accept() → spawn std::thread → handle commands → thread exits
```

Simple to implement, easy to reason about. Works fine up to ~100-200 concurrent connections.

### Upgrade Path: Event-driven I/O (Phase 3 optional)

For high connection counts, use `epoll` (Linux) or `kqueue` (macOS):
- Single thread handles thousands of connections
- Non-blocking I/O — thread never sleeps waiting for data
- Much more complex to implement

### Command Parser

```
Input:  "SET foo bar\r\n"
Split:  ["SET", "foo", "bar"]
Route:  switch on command[0]
```

---

## 8. Why These Choices

| Decision | Choice | Alternative Considered | Reason |
|----------|--------|----------------------|--------|
| DLL + hash map | O(1) LRU | Ordered map (O log n) | Speed |
| `unordered_map` | O(1) avg | `std::map` O(log n) | Speed |
| `shared_ptr` for nodes | No manual delete | Raw pointers | Safety |
| Raw `Node*` in DLL | No circular refs | `shared_ptr` everywhere | Would cause ref cycle leak |
| `shared_mutex` | Concurrent reads | `mutex` (all serial) | Read-heavy cache workloads |
| Header-only templates | Single file = readable | .cpp separate file | Templates require header visibility |
| Sentinel head/tail | Simpler DLL logic | Null checks everywhere | Cleaner code |
| Meson build | Fast, clean syntax | CMake | Less boilerplate, modern |
| Text protocol | Easy to debug | Binary protocol | Simplicity; testable with netcat |
