#include "storage.h"
#include "cache.h"
#include "database.h"
#include "logging.h"

#include <errno.h>
#include <stdatomic.h>
#include <time.h>

static int  enabled        = 0;
static int  write_ms       = 10000;
static long retention_sec  = 0;

static atomic_int stop_flag = 0;

static void sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}

int storage_init(const config_t *cfg) {
    if (cfg == NULL) return -1;
    enabled = cfg->db_enabled ? 1 : 0;
    write_ms = cfg->db_write_interval_ms > 0 ? cfg->db_write_interval_ms : 10000;
    retention_sec = (long) cfg->db_retention_hours * 3600L;
    if (!enabled) {
        log_info("Storage disabled (config)");
        return 0;
    }
    if (database_init(cfg->db_path) < 0) {
        log_error("Database init failed; disabling storage");
        enabled = 0;
        return -1;
    }
    if (retention_sec > 0) database_prune(retention_sec);
    return 0;
}

void storage_shutdown(void) {
    if (enabled) database_close();
}

void storage_stop(void) { atomic_store(&stop_flag, 1); }

void *storage_loop(void *arg) {
    (void) arg;
    if (!enabled) return NULL;
    log_info("Storage loop started (write every %dms, retention %lds)",
             write_ms, retention_sec);

    time_t last_prune = time(NULL);

    while (!atomic_load(&stop_flag)) {
        sleep_ms(write_ms);
        if (atomic_load(&stop_flag)) break;
        if (!cache_has_data()) continue;

        snapshot_t snap;
        snapshot_init(&snap);
        if (cache_get(&snap) == 0 && snap.has_data) {
            if (database_insert_snapshot(&snap) < 0) {
                log_warning("Failed to write snapshot to DB");
            }
        }
        snapshot_free(&snap);

        /* prune once an hour */
        if (retention_sec > 0 && time(NULL) - last_prune > 3600) {
            database_prune(retention_sec);
            last_prune = time(NULL);
        }
    }
    log_info("Storage loop exiting");
    return NULL;
}
