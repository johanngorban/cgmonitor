#include "config.h"
#include "logging.h"
#include "protocol.h"
#include "cache.h"
#include "collector.h"
#include "storage.h"
#include "server.h"

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static atomic_int shutting_down = 0;
static pthread_t  collector_tid;
static pthread_t  storage_tid;
static int        collector_started = 0;
static int        storage_started   = 0;

static void on_signal(int sig) {
    (void) sig;
    if (atomic_exchange(&shutting_down, 1)) return;
    /* Async-signal-safe: just flip flags. */
    collector_stop();
    storage_stop();
}

static void install_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    /* ignore SIGPIPE: HTTP clients dropping connections shouldn't kill us */
    signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char **argv) {
    config_t cfg;
    config_set_defaults(&cfg);

    /* phase 1: CLI to find --config */
    char cfgfile[256] = {0};
    int rc = config_parse_args(&cfg, argc, argv, cfgfile, sizeof(cfgfile));
    if (rc == 1) return 0;
    if (rc < 0) return 2;

    /* phase 2: load file if either CLI or env points to one */
    const char *env_cfg = getenv("CG_MON_CONFIG");
    const char *cfg_path = cfgfile[0] ? cfgfile : env_cfg;
    if (cfg_path != NULL && cfg_path[0] != '\0') {
        if (config_load_file(&cfg, cfg_path) == 0) {
            /* re-apply CLI args so they win over the file */
            char dummy[256];
            config_parse_args(&cfg, argc, argv, dummy, sizeof(dummy));
        } else {
            fprintf(stderr, "cgmonitor: could not load config '%s' (continuing with defaults)\n",
                    cfg_path);
        }
    }

    log_init(cfg.log_path);
    log_set_level_str(cfg.log_level);
    config_dump(&cfg);

    /* register built-in protocols */
    protocol_register_builtins();

    /* infrastructure */
    if (cache_init() < 0)            { fprintf(stderr, "cache_init failed\n");     return 3; }
    if (storage_init(&cfg) < 0) {
        /* non-fatal: storage_init() already logged the issue and disabled itself */
    }
    if (collector_init(&cfg) < 0)    { fprintf(stderr, "collector_init failed\n"); return 3; }
    if (server_start(&cfg) < 0)      { fprintf(stderr, "server_start failed\n");   return 3; }

    install_signals();

    if (pthread_create(&collector_tid, NULL, collector_loop, NULL) != 0) {
        perror("pthread_create collector");
        return 4;
    }
    collector_started = 1;

    if (pthread_create(&storage_tid, NULL, storage_loop, NULL) != 0) {
        perror("pthread_create storage");
        return 4;
    }
    storage_started = 1;

    log_info("cgmonitor up. Visit http://%s:%d/", cfg.server_bind, cfg.server_port);

    /* Wait for shutdown signal — block until threads exit. */
    if (collector_started) pthread_join(collector_tid, NULL);
    if (storage_started)   pthread_join(storage_tid, NULL);

    log_info("Stopping HTTP server");
    server_stop();
    collector_shutdown();
    storage_shutdown();
    cache_destroy();
    log_close();
    return 0;
}
