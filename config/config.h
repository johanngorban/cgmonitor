/**
 * config.h — runtime configuration.
 *
 * Precedence (lowest to highest):
 *   1. Compiled-in defaults
 *   2. INI file (path passed via -c or CG_MON_CONFIG env var)
 *   3. Command-line flags
 *
 * The INI format is intentionally minimalist:
 *   [section]
 *   key = value
 *   # comment
 */
#pragma once

#include <stdbool.h>

#define CFG_STR_MAX 128

typedef struct {
    /* HTTP server (frontend ↔ backend) */
    int  server_port;            /* default 9097 */
    char server_bind[CFG_STR_MAX];  /* "0.0.0.0" */

    /* Firmware connection (backend ↔ miner API) */
    char fw_protocol[CFG_STR_MAX];   /* "cgminer" */
    char fw_host[CFG_STR_MAX];       /* "127.0.0.1" */
    int  fw_port;                /* 4028 */

    /* Polling cadence (milliseconds!) */
    int  fw_poll_interval_ms;    /* how often the collector hits the firmware. default 2000 */
    int  fw_connect_timeout_ms;  /* default 2000 */
    int  fw_read_timeout_ms;     /* default 3000 */
    int  fw_max_failures;        /* before backoff. default 5 */
    int  fw_backoff_ms;          /* sleep after max failures. default 20000 */

    /* Storage */
    bool db_enabled;             /* default true */
    char db_path[CFG_STR_MAX];   /* "data/cgmonitor.db" */
    int  db_write_interval_ms;   /* how often to flush a snapshot to disk. default 10000 */
    int  db_retention_hours;     /* prune older rows on startup. 0 = keep forever. default 168 (7d) */

    /* Logging */
    char log_path[CFG_STR_MAX];  /* "cgmonitor.log" */
    char log_level[CFG_STR_MAX]; /* "info" — debug/info/warn/error */

    /* Temperature thresholds — used by frontend, exposed via /api/config. */
    double temp_warn_c;          /* default 70 */
    double temp_crit_c;          /* default 85 */
} config_t;

/* Initialize with compiled-in defaults. */
void config_set_defaults(config_t *cfg);

/* Load values from an INI file, overriding fields present in the file.
 * Returns 0 on success, -1 on open error, -2 on parse error.
 * Lines that don't match a known key are silently ignored (forward-compat). */
int config_load_file(config_t *cfg, const char *path);

/* Parse argc/argv. Recognized flags:
 *   -c, --config <path>
 *   -p, --port <n>             (server port)
 *       --fw-host <host>
 *       --fw-port <n>
 *       --fw-protocol <name>
 *       --fw-poll-ms <n>
 *       --log-level <lvl>
 *       --no-db
 *   -h, --help
 *
 * Returns 0 on success, 1 if help was requested (program should exit 0),
 * -1 on argument error.
 * On help, prints usage to stdout.
 */
int config_parse_args(config_t *cfg, int argc, char **argv,
                      char *config_file_out, int config_file_out_size);

/* Pretty-print the active config to a FILE* for diagnostics. */
struct __sFILE; /* opaque forward decl to avoid pulling in <stdio.h> here */
void config_dump(const config_t *cfg);
