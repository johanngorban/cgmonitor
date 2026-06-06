/**
 * database.h — SQLite history.
 *
 * Schema:
 *   snapshots         one row per write, holds miner-wide metrics + ts
 *   hashboards_hist   per-board metrics, FK to snapshots.id
 *   fans_hist         per-fan rpm
 *   pools_hist        per-pool counters
 *
 * Query: /api/history?param=hashrate&range=24
 *   returns [{time, value}, ...] from `snapshots` filtered by param.
 *
 * Retention: rows older than retention_hours are pruned at startup
 * and periodically by the storage thread.
 */
#pragma once

#include "snapshot.h"
#include <stdbool.h>

int  database_init(const char *db_path);
void database_close(void);

/* Append a snapshot record. Atomic across all child tables. */
int  database_insert_snapshot(const snapshot_t *s);

/* Prune snapshots older than `older_than_sec` epoch seconds (and their children). */
int  database_prune(long older_than_sec);

/* History query for a single miner-wide scalar.
 * `param` is one of: hashrate, hashrate_5m, hashrate_1h,
 *                    power, efficiency, shares_accepted,
 *                    shares_rejected, hw_errors, uptime
 * `range_sec` = how far back to look.
 *
 * On success, sets *out_json to a newly-allocated JSON array string
 * ('[{"time":..,"value":..}, ...]'), caller frees().
 * Returns 0 on success, -1 on error.
 */
int  database_query_history(const char *param, long range_sec, char **out_json);
