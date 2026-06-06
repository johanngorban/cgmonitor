/**
 * cache.h — single-snapshot in-memory cache.
 *
 * Producer: collector thread, writes a fresh snapshot every poll cycle.
 * Consumers: HTTP handlers, read the latest snapshot under a read-lock.
 *
 * Reads are deep-copied out to the caller and the lock is released
 * immediately — serialization (JSON build) happens outside the lock so
 * a slow client cannot block the producer.
 */
#pragma once

#include "snapshot.h"
#include <stdbool.h>

int  cache_init(void);
void cache_destroy(void);

/* Producer: replace the cached snapshot. Deep-copies `s` internally. */
int  cache_set(const snapshot_t *s);

/* Consumer: deep-copy current cache into `out`. `out` must be initialized
 * (via snapshot_init); on success it is replaced with a fresh copy.
 * Returns 0 even if the cache is empty (out->has_data will be false). */
int  cache_get(snapshot_t *out);

/* Liveness check for /api/health. */
bool cache_has_data(void);
