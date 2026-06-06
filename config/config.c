#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void str_copy(char *dst, size_t cap, const char *src) {
    if (dst == NULL || cap == 0) return;
    if (src == NULL) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void config_set_defaults(config_t *cfg) {
    if (cfg == NULL) return;
    memset(cfg, 0, sizeof(*cfg));

    cfg->server_port           = 9097;
    str_copy(cfg->server_bind, sizeof(cfg->server_bind), "0.0.0.0");

    str_copy(cfg->fw_protocol, sizeof(cfg->fw_protocol), "cgminer");
    str_copy(cfg->fw_host,     sizeof(cfg->fw_host),     "127.0.0.1");
    cfg->fw_port               = 4028;

    cfg->fw_poll_interval_ms   = 2000;
    cfg->fw_connect_timeout_ms = 2000;
    cfg->fw_read_timeout_ms    = 3000;
    cfg->fw_max_failures       = 5;
    cfg->fw_backoff_ms         = 20000;

    cfg->db_enabled            = true;
    str_copy(cfg->db_path, sizeof(cfg->db_path), "data/cgmonitor.db");
    cfg->db_write_interval_ms  = 10000;
    cfg->db_retention_hours    = 168;

    str_copy(cfg->log_path,  sizeof(cfg->log_path),  "cgmonitor.log");
    str_copy(cfg->log_level, sizeof(cfg->log_level), "info");

    cfg->temp_warn_c           = 70.0;
    cfg->temp_crit_c           = 85.0;
}

/* --- INI parser ---------------------------------------------------------- */

static char *trim(char *s) {
    if (s == NULL) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static bool parse_bool(const char *v) {
    if (v == NULL) return false;
    if (strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0
        || strcasecmp(v, "on") == 0 || strcmp(v, "1") == 0) return true;
    return false;
}

static void apply_kv(config_t *cfg, const char *section,
                     const char *key, const char *val)
{
    /* We treat section + key together for uniqueness, but section is optional
     * for forward compat. Below, both "[server] port" and bare "server_port"
     * are accepted. */
    #define EQ(a,b) (strcasecmp((a),(b)) == 0)
    #define IN(sec) (section[0] == '\0' || EQ(section, sec))

    if (IN("server") && (EQ(key, "port") || EQ(key, "server_port"))) {
        cfg->server_port = atoi(val);
    } else if (IN("server") && (EQ(key, "bind") || EQ(key, "server_bind"))) {
        str_copy(cfg->server_bind, sizeof(cfg->server_bind), val);

    } else if (IN("firmware") && (EQ(key, "protocol") || EQ(key, "fw_protocol"))) {
        str_copy(cfg->fw_protocol, sizeof(cfg->fw_protocol), val);
    } else if (IN("firmware") && (EQ(key, "host") || EQ(key, "fw_host"))) {
        str_copy(cfg->fw_host, sizeof(cfg->fw_host), val);
    } else if (IN("firmware") && (EQ(key, "port") || EQ(key, "fw_port"))) {
        cfg->fw_port = atoi(val);
    } else if (IN("firmware") && (EQ(key, "poll_interval_ms")
                                || EQ(key, "fw_poll_interval_ms"))) {
        cfg->fw_poll_interval_ms = atoi(val);
    } else if (IN("firmware") && (EQ(key, "connect_timeout_ms")
                                || EQ(key, "fw_connect_timeout_ms"))) {
        cfg->fw_connect_timeout_ms = atoi(val);
    } else if (IN("firmware") && (EQ(key, "read_timeout_ms")
                                || EQ(key, "fw_read_timeout_ms"))) {
        cfg->fw_read_timeout_ms = atoi(val);
    } else if (IN("firmware") && (EQ(key, "max_failures")
                                || EQ(key, "fw_max_failures"))) {
        cfg->fw_max_failures = atoi(val);
    } else if (IN("firmware") && (EQ(key, "backoff_ms")
                                || EQ(key, "fw_backoff_ms"))) {
        cfg->fw_backoff_ms = atoi(val);

    } else if (IN("storage") && (EQ(key, "enabled") || EQ(key, "db_enabled"))) {
        cfg->db_enabled = parse_bool(val);
    } else if (IN("storage") && (EQ(key, "path") || EQ(key, "db_path"))) {
        str_copy(cfg->db_path, sizeof(cfg->db_path), val);
    } else if (IN("storage") && (EQ(key, "write_interval_ms")
                                || EQ(key, "db_write_interval_ms"))) {
        cfg->db_write_interval_ms = atoi(val);
    } else if (IN("storage") && (EQ(key, "retention_hours")
                                || EQ(key, "db_retention_hours"))) {
        cfg->db_retention_hours = atoi(val);

    } else if (IN("logging") && (EQ(key, "path") || EQ(key, "log_path"))) {
        str_copy(cfg->log_path, sizeof(cfg->log_path), val);
    } else if (IN("logging") && (EQ(key, "level") || EQ(key, "log_level"))) {
        str_copy(cfg->log_level, sizeof(cfg->log_level), val);

    } else if (IN("thresholds") && (EQ(key, "temp_warn_c"))) {
        cfg->temp_warn_c = atof(val);
    } else if (IN("thresholds") && (EQ(key, "temp_crit_c"))) {
        cfg->temp_crit_c = atof(val);
    }
    /* unknown keys silently ignored — forward compat */
}

int config_load_file(config_t *cfg, const char *path) {
    if (cfg == NULL || path == NULL) return -1;
    FILE *f = fopen(path, "r");
    if (f == NULL) return -1;

    char  line[512];
    char  section[64] = {0};
    while (fgets(line, sizeof(line), f) != NULL) {
        /* strip comments */
        for (char *p = line; *p; ++p) {
            if (*p == '#' || *p == ';') { *p = '\0'; break; }
        }
        char *t = trim(line);
        if (*t == '\0') continue;

        if (*t == '[') {
            char *end = strchr(t, ']');
            if (end == NULL) { fclose(f); return -2; }
            *end = '\0';
            str_copy(section, sizeof(section), trim(t + 1));
            continue;
        }

        char *eq = strchr(t, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        char *key = trim(t);
        char *val = trim(eq + 1);
        /* strip surrounding quotes if present */
        size_t vl = strlen(val);
        if (vl >= 2 && ((val[0] == '"' && val[vl - 1] == '"')
                    ||  (val[0] == '\'' && val[vl - 1] == '\''))) {
            val[vl - 1] = '\0';
            val++;
        }
        apply_kv(cfg, section, key, val);
    }

    fclose(f);
    return 0;
}

/* --- CLI parsing --------------------------------------------------------- */

static void print_usage(const char *prog) {
    printf("cgmonitor — cgminer monitoring service\n\n"
           "Usage: %s [OPTIONS]\n\n"
           "Options:\n"
           "  -c, --config <path>    Path to INI config file\n"
           "  -p, --port <n>         HTTP server port (default 9097)\n"
           "      --fw-host <host>   Firmware API host (default 127.0.0.1)\n"
           "      --fw-port <n>      Firmware API port (default 4028)\n"
           "      --fw-protocol <n>  Firmware protocol name (default 'cgminer')\n"
           "      --fw-poll-ms <n>   Firmware poll period in ms (default 2000)\n"
           "      --log-level <lvl>  debug|info|warn|error (default info)\n"
           "      --no-db            Disable SQLite history\n"
           "  -h, --help             Show this help\n\n"
           "Environment:\n"
           "  CG_MON_CONFIG          Default config path if -c is not given\n\n",
           prog);
}

static int needs_value(const char *flag, int i, int argc, char **argv) {
    if (i + 1 >= argc) {
        fprintf(stderr, "cgmonitor: option '%s' requires a value\n", flag);
        return -1;
    }
    return 0;
}

int config_parse_args(config_t *cfg, int argc, char **argv,
                      char *config_file_out, int config_file_out_size)
{
    if (config_file_out != NULL && config_file_out_size > 0) {
        config_file_out[0] = '\0';
    }
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            return 1;
        } else if (strcmp(a, "-c") == 0 || strcmp(a, "--config") == 0) {
            if (needs_value(a, i, argc, argv) < 0) return -1;
            if (config_file_out != NULL) {
                str_copy(config_file_out, (size_t) config_file_out_size, argv[++i]);
            }
        } else if (strcmp(a, "-p") == 0 || strcmp(a, "--port") == 0) {
            if (needs_value(a, i, argc, argv) < 0) return -1;
            cfg->server_port = atoi(argv[++i]);
        } else if (strcmp(a, "--fw-host") == 0) {
            if (needs_value(a, i, argc, argv) < 0) return -1;
            str_copy(cfg->fw_host, sizeof(cfg->fw_host), argv[++i]);
        } else if (strcmp(a, "--fw-port") == 0) {
            if (needs_value(a, i, argc, argv) < 0) return -1;
            cfg->fw_port = atoi(argv[++i]);
        } else if (strcmp(a, "--fw-protocol") == 0) {
            if (needs_value(a, i, argc, argv) < 0) return -1;
            str_copy(cfg->fw_protocol, sizeof(cfg->fw_protocol), argv[++i]);
        } else if (strcmp(a, "--fw-poll-ms") == 0) {
            if (needs_value(a, i, argc, argv) < 0) return -1;
            cfg->fw_poll_interval_ms = atoi(argv[++i]);
        } else if (strcmp(a, "--log-level") == 0) {
            if (needs_value(a, i, argc, argv) < 0) return -1;
            str_copy(cfg->log_level, sizeof(cfg->log_level), argv[++i]);
        } else if (strcmp(a, "--no-db") == 0) {
            cfg->db_enabled = false;
        } else {
            fprintf(stderr, "cgmonitor: unrecognized argument '%s'\n"
                            "Try '%s --help' for more information.\n",
                    a, argv[0]);
            return -1;
        }
    }
    return 0;
}

void config_dump(const config_t *cfg) {
    if (cfg == NULL) return;
    fprintf(stderr,
        "cgmonitor configuration:\n"
        "  server:    %s:%d\n"
        "  firmware:  %s://%s:%d (poll %d ms)\n"
        "  storage:   %s (path=%s, write %d ms, retention %d h)\n"
        "  logging:   %s @ %s\n"
        "  temp warn/crit: %.1f / %.1f C\n",
        cfg->server_bind, cfg->server_port,
        cfg->fw_protocol, cfg->fw_host, cfg->fw_port, cfg->fw_poll_interval_ms,
        cfg->db_enabled ? "ON" : "OFF",
        cfg->db_path, cfg->db_write_interval_ms, cfg->db_retention_hours,
        cfg->log_level, cfg->log_path,
        cfg->temp_warn_c, cfg->temp_crit_c);
}
