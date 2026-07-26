# VeloxKV

VeloxKV is a small C++17 in-memory cache server with a simple TCP text protocol. It is designed to be easy to build, understand, and extend.

## What this project includes

- a TCP server that accepts client connections
- a basic command protocol with `INIT`, `SET`, `GET`, `DEL`, `STATS`, `INFO`, and `QUIT`
- three cache implementations:
  - `serial` for a baseline single-threaded path
  - `concurrent` for a shared-lock approach
  - `sharded` for reduced contention across shards
- an LRU-based cache policy
- TTL/expiration support in the cache layer

## Build and run

```bash
make
./velox-kv-server
```

In another terminal:

```bash
./velox-kv-client
```

Example session:

```text
> INIT concurrent 1000
OK: CONCURRENT cache initialized (capacity: 1000)

> SET name alice
OK

> GET name
VALUE alice

> QUIT
OK
```

## Tests

```bash
make test
```

The test suite exercises the serial, concurrent, and sharded cache implementations.

## Architecture overview

```text
+---------------------+       +---------------------------+
| TCP Server          | ----> | Cache Engine              |
| - accept clients   |       | - serial                  |
| - per-client thread|       | - concurrent              |
| - text protocol    |       | - sharded                 |
+---------------------+       +---------------------------+
```

### Cache type comparison

| Type | Concurrency model | Best for | Notes |
|------|-------------------|----------|-------|
| `serial` | none | baseline and simple debugging | lowest overhead |
| `concurrent` | shared lock | moderate contention | simple thread-safe option |
| `sharded` | multiple shard locks | higher concurrency | reduces lock contention |

## Protocol overview

| Command | Example | Meaning |
|---------|---------|---------|
| `INIT` | `INIT concurrent 1000` | initialize the cache engine |
| `SET` | `SET name alice` | store a key/value pair |
| `GET` | `GET name` | retrieve a value |
| `DEL` | `DEL name` | delete a key |
| `STATS` | `STATS` | show cache stats |
| `INFO` | `INFO` | show server info |
| `QUIT` | `QUIT` | close the client session |

### Example interaction

```text
> INIT serial 1000
OK: SERIAL cache initialized (capacity: 1000)

> SET user alice
OK

> GET user
VALUE alice

> DEL user
1

> QUIT
OK
```

## Project structure

```text
include/
├── core/                # cache abstractions and managers
├── policies/            # eviction policies such as LRU
└── server/              # server interface
src/
├── server/              # server entry point and socket handling
tests/
├── tests.cpp            # cache engine tests
└── test_client.cpp      # interactive client smoke test
Makefile                 # build targets
```
