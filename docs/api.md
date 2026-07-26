# API Reference — VeloxKV Commands

VeloxKV uses a simple text-based TCP protocol. The server listens on port 6380 by default and expects one command per line.

## Connection methods

- `telnet localhost 6380`
- `nc localhost 6380`
- `./velox-kv-client`

## Command reference

### INIT — initialize the cache

Format:
```text
INIT <type> <capacity>
```

Accepted values for `<type>`:
- `serial`
- `concurrent`
- `sharded`

Example:
```text
INIT concurrent 1000
OK: CONCURRENT cache initialized (capacity: 1000)
```

Notes:
- The cache must be initialized before most other commands.
- Repeated initialization returns an error.

### SET — store a key/value pair

Format:
```text
SET <key> <value> [expiration_seconds]
```

Example:
```text
SET name alice
OK
```

If an expiration is provided:
```text
SET token abc123 60
OK
```

### GET — retrieve a value

Format:
```text
GET <key>
```

Example:
```text
GET name
VALUE alice
```

If the key is missing or expired:
```text
nil
```

### DEL — delete a key

Format:
```text
DEL <key>
```

Example:
```text
DEL name
1
```

### FLUSH — clear the cache

Format:
```text
FLUSH
```

Response:
```text
OK
```

### STATS — show cache statistics

Format:
```text
STATS
```

Example output:
```text
STATS:
  hits: 10
  misses: 3
  hit_rate: 76.923077%
  evictions: 0
  active_connections: 1
```

### INFO — show server information

Format:
```text
INFO
```

Example output:
```text
# VeloxKV Server
version: 0.1.0
port: 6380
cache_type: concurrent
capacity: 1000
active_connections: 1
```

### HELP — show available commands

Format:
```text
HELP
```

### QUIT / EXIT — disconnect

Format:
```text
QUIT
```

Response:
```text
OK
```

## Notes on the protocol

- The parser splits commands on whitespace.
- Values with spaces are not supported in the current simple text protocol.
- The server replies with a line-based response and then typically prints a prompt-style suffix.
- The current implementation is intentionally small and easy to inspect.

## Example session

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

DEL temp_data
1

GET temp_data
nil

STATS
Total Hits: 5
Total Misses: 1
Hit Rate: 83.3%
Evictions: 0
Keys in Cache: 2

FLUSH
OK

GET name
nil

QUIT
OK
```

---

## Error Handling

### Common Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `ERROR: Cache not initialized. Send INIT command first.` | Called command before INIT | Run `INIT type capacity` first |
| `ERROR: Invalid cache type. Must be 0/serial, 1/concurrent, or 2/sharded` | Wrong type in INIT | Use 0, 1, 2 or serial, concurrent, sharded |
| `ERROR: Capacity must be > 0` | Capacity was 0 or negative | Use positive integer |
| `ERROR: Cache already initialized. Cannot reinitialize.` | Called INIT twice | Start new connection for new cache |
| `ERROR: GET requires key` | Missing key argument | Format: `GET <key>` |
| `ERROR: SET requires 'SET key value [ttl]'` | Invalid SET format | Format: `SET <key> <value> [ttl]` |

---

## Performance Notes

| Operation | Time Complexity | Notes |
|-----------|-----------------|-------|
| `SET` | O(1) | Hash map insertion + LRU update |
| `GET` | O(1) | Hash map lookup + LRU move-to-front |
| `DEL` | O(1) | Hash map removal + DLL node removal |
| `FLUSH` | O(n) | n = number of keys in cache |
| `STATS` | O(1) | Atomic counter reads |

---

## Protocol Notes

- **Line Ending:** Commands terminated with `\n` (newline)
- **Case Sensitivity:** Commands are case-insensitive (`SET`, `set`, `Set` all work)
- **Whitespace:** Multiple spaces between arguments treated as single separator
- **Response Encoding:** UTF-8 text, line-terminated with `\r\n> `
- **Max Command Size:** 1024 bytes per command

---

## Testing Commands

Test all commands:
```bash
INIT serial 100
SET test1 value1
SET test2 value2 10
GET test1
GET test2
GET nonexistent
DEL test1
GET test1
STATS
INFO
FLUSH
QUIT
```
GET <key>
```

**Examples:**
```
GET name
Alice

GET nonexistent
(nil)
```

**Notes:**
- Returns `(nil)` if key does not exist or has expired
- Accessing a key via GET counts as a cache hit and updates recency (LRU) or frequency (LFU)

---

### DEL

Delete a key.

```
DEL <key>
```

**Examples:**
```
DEL name
OK

DEL nonexistent
(nil)
```

---

### EXISTS

Check if a key exists (without updating recency/frequency).

```
EXISTS <key>
```

**Examples:**
```
EXISTS name
1

EXISTS ghost
0
```

---

### TTL

Get the remaining time-to-live for a key in milliseconds.

```
TTL <key>
```

**Examples:**
```
TTL session_token
45231

TTL name
-1

TTL nonexistent
-2
```

**Return values:**
- `>= 0` — milliseconds remaining
- `-1` — key exists but has no TTL (persistent)
- `-2` — key does not exist

---

### MGET

Get multiple keys in one command.

```
MGET <key1> <key2> <key3> ...
```

**Examples:**
```
MGET name age city
Alice
25
(nil)
```

**Notes:**
- Returns one value per line in the same order as the keys
- Returns `(nil)` for keys that don't exist

---

### MSET

Set multiple key-value pairs atomically in one command.

```
MSET <key1> <value1> <key2> <value2> ...
```

**Examples:**
```
MSET name Alice age 25 city London
OK
```

**Notes:**
- Must have an even number of arguments (key-value pairs)
- All or nothing — either all keys are set or none (atomic)

---

### STATS

Get cache statistics.

```
STATS
```

**Example response:**
```
hits: 142
misses: 23
evictions: 8
hit_rate: 86.06%
capacity: 100
size: 67
policy: LRU
uptime_ms: 35201
```

**Fields:**
| Field | Description |
|-------|-------------|
| `hits` | Total successful GET lookups |
| `misses` | Total failed GET lookups |
| `evictions` | Total keys evicted due to capacity |
| `hit_rate` | `hits / (hits + misses) * 100` |
| `capacity` | Maximum number of keys |
| `size` | Current number of keys |
| `policy` | Eviction policy in use (LRU / LFU) |
| `uptime_ms` | Server uptime in milliseconds |

---

### FLUSH

Remove all keys from the cache.

```
FLUSH
```

**Example:**
```
FLUSH
OK
```

**Notes:**
- Does not reset statistics (`hits`, `misses`, `evictions` are preserved)
- Use `STATS RESET` (future) to also reset counters

---

### PING

Health check — verify server is running.

```
PING
```

**Example:**
```
PING
PONG
```

---

## Error Responses

All errors start with `ERR`:

| Error | Cause |
|-------|-------|
| `ERR unknown command` | Command not recognized |
| `ERR wrong number of arguments for 'MSET'` | Odd number of args to MSET |
| `ERR value is not an integer` | TTL is not a valid number |
| `ERR TTL must be positive` | TTL <= 0 |

**Example:**
```
SET
ERR wrong number of arguments for 'SET'

SET key value TTL abc
ERR value is not an integer
```

---

## Protocol Details

### Request Format
```
<COMMAND> [arg1] [arg2] ...\r\n
```
Commands are case-insensitive. Both `GET key` and `get key` are valid.

### Response Format
- **Single value:** plain string followed by `\r\n`
- **Nil value:** `(nil)\r\n`
- **Success:** `OK\r\n`
- **Integer:** plain number followed by `\r\n`
- **Error:** `ERR <message>\r\n`
- **Multi-value (MGET):** one value per line

### Example Full Session
```
$ nc localhost 6380

PING
PONG

SET user:1:name Alice
OK

SET user:1:session token123 TTL 30000
OK

GET user:1:name
Alice

TTL user:1:session
28941

MGET user:1:name user:1:age
Alice
(nil)

MSET user:1:age 25 user:1:city London
OK

STATS
hits: 3
misses: 1
evictions: 0
hit_rate: 75.00%
capacity: 1000
size: 3
policy: LRU
uptime_ms: 4521

DEL user:1:name
OK

FLUSH
OK
```

---

## Connecting Programmatically

Since the protocol is plain TCP text, any language works:

**Python:**
```python
import socket

s = socket.socket()
s.connect(('localhost', 6380))

s.sendall(b'SET name Alice\r\n')
print(s.recv(1024))  # b'OK\r\n'

s.sendall(b'GET name\r\n')
print(s.recv(1024))  # b'Alice\r\n'
```

**Bash:**
```bash
echo -e "SET name Alice\r" | nc -q1 localhost 6380
echo -e "GET name\r" | nc -q1 localhost 6380
```
