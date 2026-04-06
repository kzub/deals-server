# deals-server

High-performance in-memory flight deals server. C++, POSIX shared memory, single-threaded poll-based TCP.

## Core Concept

The server runs as **multiple independent processes** (typically by CPU cores number) on a single machine, all sharing the same data through POSIX shared memory. nginx sits in front and round-robins incoming requests to alive backends.

This architecture enables **zero-downtime rolling restarts**: replace the binary, then restart each process one by one. While a process restarts, nginx routes to the remaining ones. Since all data lives in shared memory (not in-process heap), restarted processes immediately see all existing data. After a full rolling restart, every process runs the new code with all data preserved.

**Trade-off**: this is a single-machine design. You cannot cluster it across multiple hosts because the data sharing mechanism is POSIX shared memory, which is local to one kernel. Horizontal scaling means a bigger machine, not more machines.

## Build

```
make          # builds bin/deals-server
make clean    # removes build artifacts
```

Requires `clang++`. On Linux also links `-lrt -pthread`.

## Run

```
bin/deals-server <host> <port>
```

Use the helper script to start multiple instances:

```bash
# Start instances 0 through 7 (ports 5000-5007)
script/deals.sh start 0 7

# Stop all instances
script/deals.sh stop
```

## Rolling Restart

To deploy new code without losing data:

```bash
# Build new binary
make

# Restart each instance one at a time
# (your deployment tool should do this, or manually kill/restart each PID)
# nginx automatically routes around the stopped instance via proxy_next_upstream
```

All shared memory segments persist across process restarts. Only a machine reboot or explicit `shm_unlink` destroys the data.

## Shared Memory Setup (Linux)

The server uses `/dev/shm` for POSIX shared memory. By default, Linux limits `/dev/shm` to 50% of RAM. For production, allocate the full RAM:

`/etc/fstab`:
```
tmpfs /dev/shm tmpfs rw,nosuid,nodev,size=100% 0 2
```

Then remount or reboot:
```
mount -o remount /dev/shm
```

## nginx Configuration

```nginx
upstream deals_server {
    least_conn;
    server 127.0.0.1:5000;
    server 127.0.0.1:5001;
    server 127.0.0.1:5002;
    server 127.0.0.1:5003;
    server 127.0.0.1:5004;
    server 127.0.0.1:5005;
    server 127.0.0.1:5006;
    server 127.0.0.1:5007;
}

location / {
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header host $host;
    proxy_pass http://deals_server;
    proxy_ignore_client_abort on;
    proxy_next_upstream error http_502 http_503 non_idempotent;
}
```

`proxy_next_upstream` is what makes rolling restarts transparent — if a backend is down, nginx retries the request on the next one.

## API

| Endpoint | Method | Description |
|---|---|---|
| `/deals/add` | POST | Ingest a deal (metadata in query params, payload in body) |
| `/deals/top` | GET | Search deals (cheapest by destination, date, or country) |
| `/deals/uniqueRoutes` | GET | List unique origin-destination pairs (CSV) |
| `/deals/stats` | GET | Database statistics (JSON) |
| `/destinations/top` | GET | Top destinations by locale |
| `/deals/clear` | GET | Truncate deals database |
| `/destinations/clear` | GET | Truncate destinations database |
| `/clear` | GET | Truncate all databases |
| `/ping` | GET | Health check (returns "pong") |

See [DOCUMENTATION.md](DOCUMENTATION.md) for full API reference, query parameters, response format, and architecture details.

## Host Setup

```bash
apt-get install build-essential clang nginx runit
```
