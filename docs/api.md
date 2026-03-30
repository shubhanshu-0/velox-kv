# API Reference — KV Store Server

The server listens on port **6380** (default) and accepts newline-delimited text commands over TCP.

---

## Quick Start Testing (No Client Needed)

```bash
# Connect with netcat
nc localhost 6380

# Or with telnet
telnet localhost 6380
```

---

## Command Reference

### SET

Store a key-value pair. Overwrites if key exists.

```
SET <key> <value>
SET <key> <value> TTL <milliseconds>
```

**Examples:**
```
SET name Alice
OK

SET session_token abc123 TTL 60000
OK
```

**Notes:**
- Keys and values are strings
- TTL is in **milliseconds**
- Without TTL, key persists until evicted or explicitly deleted

---

### GET

Retrieve the value for a key.

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
