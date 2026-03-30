# Concurrent In-Memory Key-Value Store (C++)

A high-performance, thread-safe in-memory caching system built in C++ with pluggable eviction policies, TTL support, and a TCP server interface.

---

## Current Status

- [x] LRU Cache core (doubly linked list + hash map)
- [x] Template-based generic key-value types
- [x] Smart pointer memory management
- [x] Fix remaining bugs (head/tail, put() value update)
- [x] Abstract base class (`Cache_Operations`)
- [ ] Eviction strategy pattern (abstract base class)
- [ ] LFU eviction policy
- [ ] Thread safety (`std::shared_mutex`)
- [ ] TTL / expiration support
- [ ] Google Test suite
- [ ] Meson build system
- [ ] TCP server
- [ ] Benchmarks

---

## Architecture

```
include/
  core/
    cache_base.hpp          # Abstract base: Cache_Operations<K,V>
    eviction_policy.hpp     # Abstract eviction strategy
  policies/
    lru.hpp                 # LRU: O(1) via DLL + unordered_map
    lfu.hpp                 # LFU: O(1) via frequency buckets
  server/
    tcp_server.hpp
    connection.hpp
src/
  main.cpp
tests/
  test_lru.cpp
  test_lfu.cpp
  test_concurrent.cpp
  bench_cache.cpp
docs/
  design.md
  api.md
meson.build
```

---

## Building

### Prerequisites

```bash
# macOS
brew install meson ninja

# Ubuntu/Debian
sudo apt install meson ninja-build
```

### Build & Run

```bash
meson setup build
cd build
ninja
./kv_store
```

### Run Tests

```bash
cd build
ninja test
# or
meson test --verbose
```

### `meson.build`

```meson
project('kv-store', 'cpp',
  version : '0.1.0',
  default_options : ['cpp_std=c++17', 'warning_level=3']
)

inc = include_directories('include')

executable('kv_store',
  'src/main.cpp',
  include_directories : inc
)

# Tests (add after Google Test is set up)
gtest = dependency('gtest', main : true, required : false)

if gtest.found()
  test_lru = executable('test_lru',
    'tests/test_lru.cpp',
    include_directories : inc,
    dependencies : gtest
  )
  test('LRU Tests', test_lru)

  test_lfu = executable('test_lfu',
    'tests/test_lfu.cpp',
    include_directories : inc,
    dependencies : gtest
  )
  test('LFU Tests', test_lfu)

  test_concurrent = executable('test_concurrent',
    'tests/test_concurrent.cpp',
    include_directories : inc,
    dependencies : [gtest, dependency('threads')]
  )
  test('Concurrency Tests', test_concurrent)
endif
```

---

## Prerequisites (Learn Before Each Phase)

### Phase 1 — Core Engine
| Topic | Why Needed |
|-------|-----------|
| C++ Templates | Generic `Cache<K,V>` |
| `shared_ptr`, `unique_ptr` | Memory safety |
| Rule of 5 | Proper resource management |
| `unordered_map` vs `map` | O(1) vs O(log n) lookups |

### Phase 2 — Concurrency
| Topic | Why Needed |
|-------|-----------|
| `std::mutex`, `lock_guard` | Basic locking |
| `std::shared_mutex` | Read-write lock for cache |
| `std::atomic` | Lock-free hit/miss counters |
| `std::thread`, `condition_variable` | Background TTL expiry thread |
| **Book: "C++ Concurrency in Action" — Anthony Williams** | Best resource |

### Phase 3 — Server
| Topic | Why Needed |
|-------|-----------|
| BSD Sockets | TCP server foundation |
| `epoll` / `select` | Non-blocking I/O (Linux) |
| `kqueue` | Non-blocking I/O (macOS) |
| **Beej's Guide to Network Programming** — beej.us/guide/bgnet | Best free resource |

---

## Roadmap

### Phase 1 — Core Engine (Now)

**Step 1: Fix current bugs**
- `head`/`tail`: `shared_ptr<Node>*` → `shared_ptr<Node>` (pointer to shared_ptr is wrong)
- `put()`: doesn't update value when key already exists with different value
- Move `main()` out of header → `src/main.cpp`
- Switch `std::map` → `std::unordered_map` (O(1) average)

**Step 2: Proper abstract base class**
- Add `virtual` to methods in `cache_base.hpp`
- `Cache<K,V>` inherits from `Cache_Operations<K,V>`

**Step 3: Meson build**
- `meson.build` in project root (see above)
- Verify `meson setup build && ninja` compiles cleanly

**Step 4: LFU eviction policy**
- `include/policies/lfu.hpp`
- O(1) via min-frequency tracking + per-frequency doubly linked list

**Step 5: Implement `remove()` method**
- Declared in `Cache_Operations` base but not yet implemented
- Remove from both `cache_map` and DLL in O(1)
- Handle missing key gracefully (return false)

**Step 6: Google Tests**
- Capacity eviction, recency ordering, duplicate keys, edge cases
- Test `remove()` and verify map + DLL are both cleaned up
- Test `put()` value update (key exists, different value)

---

### Phase 2 — Thread Safety

**Step 7: `std::shared_mutex`**
- `get()` → `shared_lock` (multiple concurrent readers OK)
- `put()`, `remove()` → `unique_lock` (exclusive writer)

**Step 7b (Optional — Advanced): Shard-based locking**
- Split cache into N shards (e.g. 16), each with its own `shared_mutex`
- Key → shard index via `hash(key) % N`
- Dramatically higher throughput under heavy concurrency
- Worth mentioning in interviews as a known optimization

**Step 7: TTL support**
- `std::chrono::steady_clock::time_point` expiry per node
- Background `std::thread` sweeps expired keys every N ms
- `std::condition_variable` for clean shutdown

**Step 8: Atomic statistics**
```cpp
std::atomic<uint64_t> hit_count{0};
std::atomic<uint64_t> miss_count{0};
std::atomic<uint64_t> eviction_count{0};

double hit_rate() const {
    auto total = hit_count + miss_count;
    return total ? (double)hit_count / total * 100.0 : 0.0;
}
```

---

### Phase 3 — TCP Server

**Step 9: Command protocol (Redis-like text)**
```
SET <key> <value>             → OK
SET <key> <value> TTL <ms>    → OK
GET <key>                     → <value> or (nil)
DEL <key>                     → OK or (nil)
EXISTS <key>                  → 1 or 0
MGET <key1> <key2> ...        → <val1> <val2> ... (bulk get)
MSET <k1> <v1> <k2> <v2> ... → OK (bulk set)
TTL <key>                     → remaining ms or -1
STATS                         → hit_rate, misses, evictions
FLUSH                         → OK
```

**Step 10: TCP server** (default port: 6380, thread-per-connection)

**Step 11: Client CLI**
```bash
$ ./kv-client
> SET name Alice
OK
> GET name
Alice
> STATS
hits: 5, misses: 1, hit_rate: 83.3%
```

---

### Phase 4 — Polish

**Step 12: Benchmarks**
- Throughput: ops/second for GET-heavy, PUT-heavy, mixed workloads
- LRU vs LFU on sequential, random, Zipfian access patterns
- Compare single-threaded vs multi-threaded throughput

**Step 13: Documentation**
- Architecture diagram
- API reference

**Step 14: Optional persistence**
- `SAVE` / `LOAD` commands — dump cache to binary file on shutdown

---

## Key Design Decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Hash map | `unordered_map` | O(1) average vs O(log n) |
| Concurrency | `shared_mutex` | Read-heavy workloads benefit from shared reads |
| Memory | `shared_ptr` for cache nodes | No manual delete, exception-safe |
| DLL node pointers | Raw pointers | Avoids circular `shared_ptr` references |
| Templates | Header-only | Must be visible at compile-time instantiation |
| Build system | Meson + Ninja | Faster than CMake, clean syntax |

---

## Known Limitations (Intentional Scope)

- Single-node only (no distributed clustering)
- In-memory only (no WAL or durability by default)
- No authentication or access control
- Text protocol only

---

## Resources

- **"C++ Concurrency in Action"** — Anthony Williams (essential for Phase 2)
- **Beej's Guide to Network Programming** — beej.us/guide/bgnet (Phase 3)
- **cppreference.com** — STL reference
- **Google Test** — google.github.io/googletest
- **Meson Build System** — mesonbuild.com/documentation.html

---

## Docs

- [Design doc](docs/design.md) — Data structures, memory model, concurrency strategy
- [API reference](docs/api.md) — Server command reference with examples