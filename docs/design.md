# Design Notes — VeloxKV

This document describes the current implementation of VeloxKV rather than an idealized future system.

## 1. System shape

VeloxKV is arranged in two layers:

- a TCP server layer that accepts client connections and parses commands
- a cache layer that stores key/value entries and applies the chosen concurrency strategy

The server currently uses one thread per accepted client connection. The cache layer then decides how to protect shared state internally.

## 2. Cache architecture

The cache interface is defined by the abstract `Cache<K, V>` base and implemented through a small set of wrappers:

- `SerialCache` for baseline single-threaded behavior
- `ConcurrentCache` for a shared-lock design using `std::shared_mutex`
- `ShardedConcurrentCache` for reduced contention across 16 shards

The LRU policy is implemented with a doubly linked list plus a hash map. This gives average O(1) behavior for lookup and update operations.

## 3. Why the three cache modes exist

Each mode is a different tradeoff:

- `serial`: simplest and lowest overhead
- `concurrent`: introduces locking to make the cache safe under concurrent access
- `sharded`: reduces lock contention by splitting keys across multiple shards

This makes the project useful as a concurrency study because the server-level threading and the cache-level synchronization are separate concerns.

## 4. Expiration and TTL handling

The project supports expiration through `CacheExpiration`.

The current design uses a background thread that waits until the next expiration time and then removes expired entries. This keeps the system responsive while avoiding a fully polling-based cleanup loop.

## 5. Server-side concurrency

The server layer is intentionally simple:

- accept a client socket
- spawn a worker thread
- run `handle_client(...)` in that thread

This makes it easy to reason about and test. It is not yet a high-scale event-driven server, but it is good enough to demonstrate how a server can service multiple clients concurrently.

## 6. Why the sharded design helps

A single global lock becomes a bottleneck when many threads compete for the same resources. Sharding reduces that contention because each shard has its own mutex and its own small cache instance. Keys are routed to a shard based on a hash, so concurrent access to different keys tends to stay isolated.

## 7. Notes on correctness

The design is careful about thread safety in the places where data structures are mutated. The cache wrappers protect the underlying policy with locks, and the expiration logic is coordinated so that cleanup does not race with regular access.

## 8. Current limitations

The project is intentionally scoped for learning and demonstration. It does not yet include:

- persistence
- replication or clustering
- TLS or authentication
- a high-scale event-driven networking model such as `epoll`

## 9. Why this design is still useful

This project is useful because it shows a compact but real backend stack:

- network I/O
- concurrency
- synchronization tradeoffs
- cache policy design
- testable behavior

That makes it a strong interview and portfolio project for someone targeting backend or systems work.

