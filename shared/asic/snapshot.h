/**
 * snapshot.h — unified miner snapshot.
 *
 * This is the canonical data model that flows through the system:
 *   firmware-specific protocol → snapshot_t → cache → JSON/whatever
 *
 * Protocol implementations fill in what they can. Fields they don't have
 * MUST be left at the zero-initialized default. Frontend gracefully handles
 * missing data (NaN/zero/empty arrays).
 *
 * All counts and array sizes are int (not size_t) to keep the JSON
 * serializer simple — we never expect 2B+ entities anyway.
 */
#pragma once

#include <stdbool.h>
#include <time.h>

#define POOL_URL_MAX 256
#define MINER_FW_VER_MAX 64

typedef struct {
    int index;
    int rpm;
} fan_t;

typedef enum {
    HB_STATUS_UNKNOWN = 0,
    HB_STATUS_ALIVE   = 1,
    HB_STATUS_FAULT   = 2,
} hashboard_status_t;

typedef struct {
    int   index;
    hashboard_status_t status;
    double chip_temp_c;       /* hottest chip on the board */
    double pcb_temp_c;
    int    chip_frequency_mhz;
    int    chips_active;
    int    chips_total;
} hashboard_t;

typedef enum {
    POOL_STATUS_UNKNOWN = 0,
    POOL_STATUS_ALIVE   = 1,
    POOL_STATUS_DEAD    = 2,
} pool_status_t;

typedef struct {
    int   index;
    char  url[POOL_URL_MAX];
    pool_status_t status;
    bool  active;
    int   latency_ms;         /* -1 if unknown */
    long long accepted;
    long long rejected;
    long long stale;
} pool_t;

typedef struct {
    /* Wall-clock time the snapshot was produced by the collector. */
    time_t timestamp;

    /* Marker: true once at least one successful collection has happened. */
    bool   has_data;

    /* Miner-wide */
    long   uptime_sec;
    double hashrate_5s_ghs;
    double hashrate_5m_ghs;
    double hashrate_1h_ghs;
    long long shares_accepted;
    long long shares_rejected;
    long long hw_errors;
    double power_w;
    double efficiency_j_per_ghs;   /* derived if not provided */

    /* -1 if no active pool / unknown */
    int    active_pool_index;

    /* Optional miner identification */
    char   fw_version[MINER_FW_VER_MAX];

    /* Variable-length arrays — owned by this struct.
     * Use snapshot_alloc_* to (re)allocate, snapshot_free to release. */
    int          n_hashboards;
    hashboard_t *hashboards;

    int          n_fans;
    fan_t       *fans;

    int          n_pools;
    pool_t      *pools;
} snapshot_t;

/* Zero-init. Safe to call on an uninitialized snapshot_t. */
void snapshot_init(snapshot_t *s);

/* Free dynamic arrays and re-zero. Safe to call multiple times. */
void snapshot_free(snapshot_t *s);

/* (Re)allocate the corresponding array. Sets count on success.
 * Old data is discarded. Returns 0 on success, -1 on OOM. */
int snapshot_alloc_hashboards(snapshot_t *s, int n);
int snapshot_alloc_fans(snapshot_t *s, int n);
int snapshot_alloc_pools(snapshot_t *s, int n);

/* Deep copy. dest is freed first if needed. Returns 0 on success. */
int snapshot_copy(snapshot_t *dest, const snapshot_t *src);

/* Derive efficiency from power_w / total_hashrate_ghs if not already set.
 * Called by collectors after a fetch. */
void snapshot_derive(snapshot_t *s);
