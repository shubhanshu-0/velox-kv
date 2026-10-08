# API Reference — VeloxKV Commands

VeloxKV uses a simple text-based TCP protocol over non-blocking sockets. The server listens on port 6380 by default.

## Connection Methods

- `nc localhost 6380`
- `telnet localhost 6380`
- Python / Go / Node.js socket connections

## Command Reference

### SET — Store a key/value pair

Format:
```text
SET <key> <value> [expiration_seconds]
```

Example:
```text
SET name alice
OK

SET token abc123 60
OK
```

### GET — Retrieve a value

Format:
```text
GET <key>
```

Example:
```text
GET name
VALUE alice

GET nonexistent
nil
```

### DEL — Delete a key

Format:
```text
DEL <key>
```

Example:
```text
DEL name
1
```

### STATS — View cache metrics

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

### INFO — Server information

Format:
```text
INFO
```

Example output:
```text
# VeloxKV Server
version: 0.2.0 (Non-blocking Event Loop)
port: 6380
capacity: 10000
active_connections: 1
```

### HELP — Usage help

Format:
```text
HELP
```

### QUIT / EXIT — Disconnect

Format:
```text
QUIT
```
