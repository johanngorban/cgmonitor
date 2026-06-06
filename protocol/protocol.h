/**
 * protocol.h — firmware-API abstraction.
 *
 * The collector loop is generic: it knows only this vtable.
 * Each concrete protocol (cgminer JSON, vendor XML, binary UART, ...)
 * ships as a static const firmware_protocol_t with three functions.
 *
 * To add a new protocol:
 *   1. Create protocol/<name>/<name>.c that defines and exposes a
 *      `const firmware_protocol_t <name>_protocol`.
 *   2. Add it to PROTOCOL_SOURCES in the top-level CMakeLists.txt.
 *   3. Reference it once in protocol_register_builtins() below.
 *
 * That's it. No dlopen, no plugin discovery — just three steps.
 */
#pragma once

#include "snapshot.h"

typedef struct {
    const char *host;
    int         port;
    int         connect_timeout_ms;
    int         read_timeout_ms;
    /* free-form, protocol-specific extras can be wedged here later */
} protocol_config_t;

/* Opaque handle, owned by the implementation. */
typedef void *protocol_handle_t;

typedef struct firmware_protocol {
    /* Unique name. Matched against config.fw_protocol. */
    const char *name;

    /* Create an instance. Returns NULL on failure.
     * The handle owns any sockets/buffers and lives as long as the collector. */
    protocol_handle_t (*create)(const protocol_config_t *cfg);

    /* Fetch a complete snapshot in one call. The implementation is responsible
     * for connecting (if applicable), issuing whatever requests it needs,
     * and parsing into `out`. `out` is pre-initialized (zeroed) by the caller.
     *
     * Returns 0 on success, -1 on transient failure (collector will retry/backoff).
     * On failure, `out` may be partially populated — collector treats it as invalid. */
    int (*fetch)(protocol_handle_t handle, snapshot_t *out);

    /* Release all resources. handle is invalid after this returns. */
    void (*destroy)(protocol_handle_t handle);
} firmware_protocol_t;

/* Look up a registered protocol by name. Returns NULL if unknown.
 * Names are case-insensitive. */
const firmware_protocol_t *protocol_find(const char *name);

/* Iterate registered protocols (for diagnostics / `--list-protocols`). */
int protocol_count(void);
const firmware_protocol_t *protocol_at(int i);

/* Register a protocol. Called by built-ins at startup.
 * Returns 0 on success, -1 if the registry is full (compile-time limit) or
 * a duplicate name. */
int protocol_register(const firmware_protocol_t *p);

/* Register all protocols that were compiled in. Idempotent. */
void protocol_register_builtins(void);
