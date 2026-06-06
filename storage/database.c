#include "database.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

static sqlite3 *DB = NULL;

static const char *SCHEMA[] = {
    "PRAGMA journal_mode = WAL;",
    "PRAGMA synchronous = NORMAL;",
    "PRAGMA foreign_keys = ON;",

    "CREATE TABLE IF NOT EXISTS snapshots ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  ts INTEGER NOT NULL,"
    "  uptime INTEGER,"
    "  hashrate_5s REAL,"
    "  hashrate_5m REAL,"
    "  hashrate_1h REAL,"
    "  shares_accepted INTEGER,"
    "  shares_rejected INTEGER,"
    "  hw_errors INTEGER,"
    "  power REAL,"
    "  efficiency REAL,"
    "  active_pool INTEGER"
    ");",
    "CREATE INDEX IF NOT EXISTS idx_snapshots_ts ON snapshots(ts);",

    "CREATE TABLE IF NOT EXISTS hashboards_hist ("
    "  snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,"
    "  idx INTEGER NOT NULL,"
    "  status INTEGER,"
    "  chip_temp REAL,"
    "  pcb_temp REAL,"
    "  freq_mhz INTEGER,"
    "  chips_active INTEGER,"
    "  chips_total INTEGER"
    ");",

    "CREATE TABLE IF NOT EXISTS fans_hist ("
    "  snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,"
    "  idx INTEGER NOT NULL,"
    "  rpm INTEGER"
    ");",

    "CREATE TABLE IF NOT EXISTS pools_hist ("
    "  snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,"
    "  idx INTEGER NOT NULL,"
    "  url TEXT,"
    "  status INTEGER,"
    "  active INTEGER,"
    "  latency_ms INTEGER,"
    "  accepted INTEGER,"
    "  rejected INTEGER,"
    "  stale INTEGER"
    ");",

    NULL
};

/* ensure parent directory of db_path exists */
static int ensure_dir(const char *path) {
    char *dup = strdup(path);
    if (dup == NULL) return -1;
    char *dir = dirname(dup);
    if (dir == NULL || strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0) {
        free(dup);
        return 0;
    }
    int rc = mkdir(dir, 0755);
    if (rc < 0 && errno != EEXIST) {
        free(dup);
        return -1;
    }
    free(dup);
    return 0;
}

int database_init(const char *db_path) {
    if (db_path == NULL) return -1;
    if (ensure_dir(db_path) < 0) {
        log_warning("Could not create dir for db path %s: %s", db_path, strerror(errno));
    }
    if (sqlite3_open(db_path, &DB) != SQLITE_OK) {
        log_error("sqlite3_open(%s): %s", db_path, sqlite3_errmsg(DB));
        return -1;
    }
    char *err = NULL;
    for (int i = 0; SCHEMA[i] != NULL; ++i) {
        if (sqlite3_exec(DB, SCHEMA[i], 0, 0, &err) != SQLITE_OK) {
            log_error("schema exec failed: %s [%s]", err, SCHEMA[i]);
            sqlite3_free(err);
            return -1;
        }
    }
    log_info("Database initialized at %s", db_path);
    return 0;
}

void database_close(void) {
    if (DB != NULL) {
        sqlite3_close(DB);
        DB = NULL;
    }
}

int database_insert_snapshot(const snapshot_t *s) {
    if (DB == NULL || s == NULL || !s->has_data) return -1;

    char *err = NULL;
    if (sqlite3_exec(DB, "BEGIN;", 0, 0, &err) != SQLITE_OK) {
        log_error("BEGIN: %s", err);
        sqlite3_free(err);
        return -1;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql_snap =
        "INSERT INTO snapshots(ts, uptime, hashrate_5s, hashrate_5m, hashrate_1h,"
        " shares_accepted, shares_rejected, hw_errors, power, efficiency, active_pool)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?);";

    if (sqlite3_prepare_v2(DB, sql_snap, -1, &stmt, NULL) != SQLITE_OK) {
        log_error("prepare snap: %s", sqlite3_errmsg(DB));
        goto rollback;
    }
    sqlite3_bind_int64 (stmt,  1, (sqlite3_int64) s->timestamp);
    sqlite3_bind_int64 (stmt,  2, (sqlite3_int64) s->uptime_sec);
    sqlite3_bind_double(stmt,  3, s->hashrate_5s_ghs);
    sqlite3_bind_double(stmt,  4, s->hashrate_5m_ghs);
    sqlite3_bind_double(stmt,  5, s->hashrate_1h_ghs);
    sqlite3_bind_int64 (stmt,  6, (sqlite3_int64) s->shares_accepted);
    sqlite3_bind_int64 (stmt,  7, (sqlite3_int64) s->shares_rejected);
    sqlite3_bind_int64 (stmt,  8, (sqlite3_int64) s->hw_errors);
    sqlite3_bind_double(stmt,  9, s->power_w);
    sqlite3_bind_double(stmt, 10, s->efficiency_j_per_ghs);
    sqlite3_bind_int   (stmt, 11, s->active_pool_index);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        log_error("step snap: %s", sqlite3_errmsg(DB));
        sqlite3_finalize(stmt);
        goto rollback;
    }
    sqlite3_finalize(stmt);
    sqlite3_int64 snap_id = sqlite3_last_insert_rowid(DB);

    /* hashboards */
    if (s->n_hashboards > 0) {
        const char *q = "INSERT INTO hashboards_hist VALUES(?,?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(DB, q, -1, &stmt, NULL) != SQLITE_OK) goto rollback;
        for (int i = 0; i < s->n_hashboards; ++i) {
            const hashboard_t *b = &s->hashboards[i];
            sqlite3_bind_int64 (stmt, 1, snap_id);
            sqlite3_bind_int   (stmt, 2, b->index);
            sqlite3_bind_int   (stmt, 3, (int) b->status);
            sqlite3_bind_double(stmt, 4, b->chip_temp_c);
            sqlite3_bind_double(stmt, 5, b->pcb_temp_c);
            sqlite3_bind_int   (stmt, 6, b->chip_frequency_mhz);
            sqlite3_bind_int   (stmt, 7, b->chips_active);
            sqlite3_bind_int   (stmt, 8, b->chips_total);
            if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); goto rollback; }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    /* fans */
    if (s->n_fans > 0) {
        const char *q = "INSERT INTO fans_hist VALUES(?,?,?);";
        if (sqlite3_prepare_v2(DB, q, -1, &stmt, NULL) != SQLITE_OK) goto rollback;
        for (int i = 0; i < s->n_fans; ++i) {
            sqlite3_bind_int64(stmt, 1, snap_id);
            sqlite3_bind_int  (stmt, 2, s->fans[i].index);
            sqlite3_bind_int  (stmt, 3, s->fans[i].rpm);
            if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); goto rollback; }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    /* pools */
    if (s->n_pools > 0) {
        const char *q = "INSERT INTO pools_hist VALUES(?,?,?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(DB, q, -1, &stmt, NULL) != SQLITE_OK) goto rollback;
        for (int i = 0; i < s->n_pools; ++i) {
            const pool_t *p = &s->pools[i];
            sqlite3_bind_int64(stmt, 1, snap_id);
            sqlite3_bind_int  (stmt, 2, p->index);
            sqlite3_bind_text (stmt, 3, p->url, -1, SQLITE_STATIC);
            sqlite3_bind_int  (stmt, 4, (int) p->status);
            sqlite3_bind_int  (stmt, 5, p->active ? 1 : 0);
            sqlite3_bind_int  (stmt, 6, p->latency_ms);
            sqlite3_bind_int64(stmt, 7, (sqlite3_int64) p->accepted);
            sqlite3_bind_int64(stmt, 8, (sqlite3_int64) p->rejected);
            sqlite3_bind_int64(stmt, 9, (sqlite3_int64) p->stale);
            if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); goto rollback; }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_exec(DB, "COMMIT;", 0, 0, &err) != SQLITE_OK) {
        log_error("COMMIT: %s", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;

rollback:
    sqlite3_exec(DB, "ROLLBACK;", 0, 0, NULL);
    return -1;
}

int database_prune(long older_than_sec) {
    if (DB == NULL || older_than_sec <= 0) return -1;
    const char *sql =
        "DELETE FROM snapshots WHERE ts < (strftime('%s','now') - ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64) older_than_sec);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Whitelist param → column. Returns NULL if param is unknown. */
static const char *map_param(const char *param) {
    if (param == NULL) return NULL;
    if (strcasecmp(param, "hashrate")        == 0) return "hashrate_5s";
    if (strcasecmp(param, "hashrate_5s")     == 0) return "hashrate_5s";
    if (strcasecmp(param, "hashrate_5m")     == 0) return "hashrate_5m";
    if (strcasecmp(param, "hashrate_1h")     == 0) return "hashrate_1h";
    if (strcasecmp(param, "power")           == 0) return "power";
    if (strcasecmp(param, "efficiency")      == 0) return "efficiency";
    if (strcasecmp(param, "shares_accepted") == 0) return "shares_accepted";
    if (strcasecmp(param, "shares_rejected") == 0) return "shares_rejected";
    if (strcasecmp(param, "hw_errors")       == 0) return "hw_errors";
    if (strcasecmp(param, "uptime")          == 0) return "uptime";
    return NULL;
}

/* Tiny dynamic string buffer (avoids dragging in another dep). */
typedef struct { char *p; size_t len; size_t cap; } strbuf;

static int sb_reserve(strbuf *b, size_t need) {
    if (b->cap >= need) return 0;
    size_t nc = b->cap > 0 ? b->cap : 256;
    while (nc < need) nc *= 2;
    char *np = (char *) realloc(b->p, nc);
    if (np == NULL) return -1;
    b->p = np; b->cap = nc;
    return 0;
}

static int sb_append(strbuf *b, const char *s, size_t len) {
    if (sb_reserve(b, b->len + len + 1) < 0) return -1;
    memcpy(b->p + b->len, s, len);
    b->len += len;
    b->p[b->len] = '\0';
    return 0;
}

static int sb_printf(strbuf *b, const char *fmt, ...) {
    char  tmp[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    if ((size_t) n < sizeof(tmp)) return sb_append(b, tmp, (size_t) n);
    /* unlikely path: re-do with heap */
    char *big = (char *) malloc((size_t) n + 1);
    if (big == NULL) return -1;
    va_start(ap, fmt);
    vsnprintf(big, (size_t) n + 1, fmt, ap);
    va_end(ap);
    int rc = sb_append(b, big, (size_t) n);
    free(big);
    return rc;
}

int database_query_history(const char *param, long range_sec, char **out_json) {
    if (DB == NULL || out_json == NULL) return -1;
    const char *col = map_param(param);
    if (col == NULL) return -1;
    if (range_sec <= 0) range_sec = 3600;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT ts, %s FROM snapshots WHERE ts > (strftime('%%s','now') - ?) "
             "ORDER BY ts ASC;", col);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64) range_sec);

    strbuf b = {0};
    if (sb_append(&b, "[", 1) < 0) { sqlite3_finalize(stmt); return -1; }

    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        long long ts = sqlite3_column_int64(stmt, 0);
        double    v  = sqlite3_column_double(stmt, 1);
        if (!first) sb_append(&b, ",", 1);
        first = 0;
        sb_printf(&b, "{\"time\":%lld,\"value\":%g}", ts, v);
    }
    sqlite3_finalize(stmt);
    sb_append(&b, "]", 1);

    *out_json = b.p;
    return 0;
}
