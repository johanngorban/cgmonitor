# cgmonitor

Lightweight monitoring service for cgminer-class ASIC firmwares. Designed to
run on the miner itself (embedded Linux ARM) with a single self-contained
binary — frontend assets are embedded into the executable at build time.

## Features

- **Pluggable protocol layer.** cgminer JSON RPC is the only built-in for now;
  adding a vendor XML / binary protocol is a 3-step recipe (see below).
- **Cache-first architecture.** The collector polls the firmware on its own
  cadence; the HTTP API serves a hot in-memory snapshot under a RW-lock, so
  the frontend can hit `/api/snapshot` at any rate it likes without backpressuring
  the firmware.
- **SQLite history.** A separate writer thread persists snapshots periodically
  with configurable retention. Querying via `/api/history?param=X&range=Y`.
- **Embedded frontend.** Single HTML + ES-module JS + CSS, no build step, no
  framework, no external fonts. Gruvbox-dark monitoring-console aesthetic.
- **Fully configurable.** INI file + CLI overrides + env var. Sensible defaults.

## Architecture

```
                     ┌──────── cgmonitor binary ────────────────────────────┐
                     │                                                       │
                     │   ┌────────────┐  poll  ┌────────────┐                │
firmware  ──TCP─────►│   │ cgminer    │ ◄──┐   │ snapshot_t │                │
(cgminer-API)        │   │ protocol   │    │   │   cache    │ ◄── RW-lock ──┐│
                     │   └────────────┘    │   │ (current)  │               ││
                     │       ▲             │   └─────┬──────┘               ││
                     │       │ vtable      │         │                       ││
                     │   ┌────────────┐    │         │                       ││
                     │   │  protocol  │    │         ▼                       ││
                     │   │  registry  │    │   ┌────────────┐  HTTP    ┌─────┴┴───┐
                     │   └────────────┘    │   │ collector  │   ┌────► │  HTTP    │
                     │       ▲             │   │  thread    │   │      │  server  │ ◄── browser
                     │       │             │   └────────────┘   │      └──────────┘
                     │   ┌────────────┐    │         │           │
                     │   │ vendor X   │    │         ▼           │
                     │   │ protocol   │    │   ┌────────────┐    │
                     │   │ (future)   │    │   │ storage    │    │ embedded
                     │   └────────────┘    │   │ thread     │    │ static files
                     │                     │   └─────┬──────┘    │ (index.html,
                     │                     │         │           │ app.js, style.css)
                     │                     │         ▼           │
                     │                     │   ┌────────────┐    │
                     │                     │   │  SQLite    │    │
                     │                     │   │ history    │ ───┘
                     │                     │   └────────────┘
                     └──────────────────────────────────────────────────────┘
```

## Quick start

```bash
# 1. Build + install (auto-detects Arch/Debian/Ubuntu/Fedora/Alpine)
sudo ./install.sh

# 2. Edit the config
sudo $EDITOR /etc/cgmonitor/cgmonitor.conf

# 3. Run
sudo systemctl enable --now cgmonitor

# 4. Open the dashboard
xdg-open http://<miner-ip>:9097
```

Or, without systemd:

```bash
sudo ./install.sh --no-systemd
cgmonitor -c /etc/cgmonitor/cgmonitor.conf
```

## Building from source

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cgmonitor --help
```

Dependencies:

- C11 compiler
- CMake ≥ 3.16
- Python 3 (build-time only, for embedding static files)
- pkg-config
- libcjson, libmicrohttpd, libsqlite3, pthreads

## Cross-compiling for ARM

Copy `cmake/toolchains/arm-linux-gnueabihf.cmake.example` to
`~/cgmonitor-arm.cmake`, point `CMAKE_SYSROOT` at your target sysroot, then:

```bash
cmake -S . -B build-arm \
      -DCMAKE_TOOLCHAIN_FILE=~/cgmonitor-arm.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm -j
```

## Testing without a miner

The repository ships a small Python mock:

```bash
python3 tests/cgminer_mock.py --port 4028 --boards 3 --fans 4 --pools 2
# in another terminal
./build/cgmonitor --fw-host 127.0.0.1 --fw-port 4028 --fw-poll-ms 1000
```

## HTTP API

| Path                                        | Description                          |
|---------------------------------------------|--------------------------------------|
| `GET /api/snapshot`                         | Full latest snapshot                 |
| `GET /api/history?param=hashrate&range=3600`| Historical points for one scalar     |
| `GET /api/config`                           | Frontend-relevant config (thresholds)|
| `GET /api/health`                           | Liveness check                       |
| `GET /`                                     | Dashboard (embedded HTML)            |

History `param` is one of: `hashrate`, `hashrate_5m`, `hashrate_1h`, `power`,
`efficiency`, `shares_accepted`, `shares_rejected`, `hw_errors`, `uptime`.
`range` is in seconds.

### `/api/snapshot` shape

```json
{
  "timestamp": 1730000000,
  "has_data": true,
  "fw_version": "...",
  "miner": {
    "uptime_sec": 12345,
    "hashrate_5s_ghs": 105500.2,
    "hashrate_5m_ghs": 105200.0,
    "hashrate_1h_ghs": 104900.0,
    "shares_accepted": 12345,
    "shares_rejected": 23,
    "shares_rejected_pct": 0.19,
    "hw_errors": 5,
    "hw_errors_pct": 0.04,
    "power_w": 3240,
    "efficiency_j_per_ghs": 30.7,
    "active_pool_index": 0
  },
  "hashboards": [
    { "index": 0, "status": "alive", "chip_temp_c": 72.5, "pcb_temp_c": 56.2,
      "chip_frequency_mhz": 700, "chips_active": 76, "chips_total": 76 }
  ],
  "fans": [
    { "index": 0, "rpm": 4500 }
  ],
  "pools": [
    { "index": 0, "url": "stratum+tcp://…", "status": "alive", "active": true,
      "latency_ms": 23, "accepted": 12345, "rejected": 23, "stale": 5 }
  ]
}
```

## Adding a new firmware protocol

The whole point of the abstraction is that the collector loop never changes
when you add a new firmware. Three steps:

**1.** Create `protocol/<name>/<name>.c` that defines the vtable:

```c
#include "protocol.h"
#include "snapshot.h"

typedef struct { /* ... your handle state ... */ } ctx_t;

static protocol_handle_t myproto_create(const protocol_config_t *cfg) { /* ... */ }
static int               myproto_fetch (protocol_handle_t h, snapshot_t *out) { /* ... */ }
static void              myproto_destroy(protocol_handle_t h) { /* ... */ }

const firmware_protocol_t myproto_protocol = {
    .name    = "myproto",
    .create  = myproto_create,
    .fetch   = myproto_fetch,
    .destroy = myproto_destroy,
};
```

**2.** Add the source file to `CMakeLists.txt`:

```cmake
set(SRC
    ...
    protocol/myproto/myproto.c
)
```

**3.** Register it in `protocol/protocol.c::protocol_register_builtins()`:

```c
extern const firmware_protocol_t myproto_protocol;
/* ... */
protocol_register(&myproto_protocol);
```

Done. The user can now select it in `cgmonitor.conf`:

```ini
[firmware]
protocol = myproto
```

`fetch()` returns `0` on success and `-1` on transient failure. On `-1` the
collector backs off and retries. Partially-filled snapshots on success are
fine — fields the protocol can't fill simply stay at zero.

## Configuration

All keys, with defaults:

| Section       | Key                  | Default               |
|---------------|----------------------|-----------------------|
| `server`      | `port`               | `9097`                |
| `server`      | `bind`               | `0.0.0.0`             |
| `firmware`    | `protocol`           | `cgminer`             |
| `firmware`    | `host`               | `127.0.0.1`           |
| `firmware`    | `port`               | `4028`                |
| `firmware`    | `poll_interval_ms`   | `2000`                |
| `firmware`    | `connect_timeout_ms` | `2000`                |
| `firmware`    | `read_timeout_ms`    | `3000`                |
| `firmware`    | `max_failures`       | `5`                   |
| `firmware`    | `backoff_ms`         | `20000`               |
| `storage`     | `enabled`            | `true`                |
| `storage`     | `path`               | `data/cgmonitor.db`   |
| `storage`     | `write_interval_ms`  | `10000`               |
| `storage`     | `retention_hours`    | `168` (7 days)        |
| `logging`     | `path`               | `cgmonitor.log`       |
| `logging`     | `level`              | `info`                |
| `thresholds`  | `temp_warn_c`        | `70`                  |
| `thresholds`  | `temp_crit_c`        | `85`                  |

CLI flags override the file. Run `cgmonitor --help` for the full list.

## License

MIT.
