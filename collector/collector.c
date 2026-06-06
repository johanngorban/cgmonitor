#include "collector.h"
#include "protocol.h"
#include "cache.h"
#include "snapshot.h"
#include "logging.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const firmware_protocol_t *proto      = NULL;
static protocol_handle_t          proto_h    = NULL;
static int                        poll_ms    = 2000;
static int                        max_fails  = 5;
static int                        backoff_ms = 20000;

static atomic_int stop_flag = 0;

static void sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { /* retry */ }
}

int collector_init(const config_t *cfg) {
    if (cfg == NULL) return -1;

    proto = protocol_find(cfg->fw_protocol);
    if (proto == NULL) {
        log_error("Unknown firmware protocol: %s", cfg->fw_protocol);
        return -1;
    }

    protocol_config_t pcfg = {
        .host               = cfg->fw_host,
        .port               = cfg->fw_port,
        .connect_timeout_ms = cfg->fw_connect_timeout_ms,
        .read_timeout_ms    = cfg->fw_read_timeout_ms,
    };
    proto_h = proto->create(&pcfg);
    if (proto_h == NULL) {
        log_error("Protocol '%s' failed to initialize", proto->name);
        return -1;
    }

    poll_ms    = cfg->fw_poll_interval_ms > 0 ? cfg->fw_poll_interval_ms : 2000;
    max_fails  = cfg->fw_max_failures > 0    ? cfg->fw_max_failures    : 5;
    backoff_ms = cfg->fw_backoff_ms > 0      ? cfg->fw_backoff_ms      : 20000;

    log_info("Collector initialized: protocol=%s target=%s:%d period=%dms",
             proto->name, cfg->fw_host, cfg->fw_port, poll_ms);
    return 0;
}

void collector_shutdown(void) {
    if (proto != NULL && proto->destroy != NULL && proto_h != NULL) {
        proto->destroy(proto_h);
    }
    proto = NULL;
    proto_h = NULL;
}

void collector_stop(void) {
    atomic_store(&stop_flag, 1);
}

void *collector_loop(void *arg) {
    (void) arg;
    if (proto == NULL || proto_h == NULL) {
        log_error("Collector loop started without init");
        return NULL;
    }

    int failures = 0;

    while (!atomic_load(&stop_flag)) {
        snapshot_t snap;
        snapshot_init(&snap);

        int rc = proto->fetch(proto_h, &snap);
        if (rc < 0) {
            failures++;
            log_warning("Firmware fetch failed (%d/%d)", failures, max_fails);
            snapshot_free(&snap);
            if (failures >= max_fails) {
                log_error("Max firmware failures reached, backing off %dms", backoff_ms);
                failures = 0;
                sleep_ms(backoff_ms);
                continue;
            }
            sleep_ms(poll_ms);
            continue;
        }

        failures = 0;
        snap.timestamp = time(NULL);
        snapshot_derive(&snap);
        if (cache_set(&snap) < 0) {
            log_error("cache_set failed");
        }
        snapshot_free(&snap);

        sleep_ms(poll_ms);
    }

    log_info("Collector loop exiting");
    return NULL;
}
