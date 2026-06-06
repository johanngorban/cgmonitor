/**
 * cgminer_parse.c — parsing of cgminer JSON responses into snapshot_t.
 *
 * Each parser is forgiving: missing fields are skipped, not errors.
 * cgminer's field names are notoriously inconsistent across versions
 * (some have spaces, capitalization varies); the cJSON case-insensitive
 * getters handle that for us.
 */
#include "cgminer_parse.h"

#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

/* Helpers ----------------------------------------------------------------- */

static double get_num(const cJSON *obj, const char *key, double dflt) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (cJSON_IsNumber(v)) ? v->valuedouble : dflt;
}

static long long get_int64(const cJSON *obj, const char *key, long long dflt) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(v)) return (long long) v->valuedouble;
    return dflt;
}

static const char *get_str(const cJSON *obj, const char *key, const char *dflt) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (cJSON_IsString(v) && v->valuestring) ? v->valuestring : dflt;
}

/* MHS → GHS conversion. cgminer historically reports MH/s. */
static double mhs_to_ghs(double v) { return v / 1000.0; }

/* --- summary ------------------------------------------------------------- */

int cgminer_parse_summary(snapshot_t *s, const char *json) {
    if (s == NULL || json == NULL) return -1;
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) return -1;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "SUMMARY");
    cJSON *it  = cJSON_IsArray(arr) ? cJSON_GetArrayItem(arr, 0) : NULL;
    if (it == NULL) { cJSON_Delete(root); return -1; }

    /* Hashrate: prefer the most-granular average available */
    double mhs_5s = get_num(it, "MHS 5s",  0.0);
    double mhs_av = get_num(it, "MHS av",  0.0);
    double mhs_5m = get_num(it, "MHS 5m",  0.0);
    double mhs_1h = get_num(it, "MHS 1h",  0.0);
    if (mhs_5s == 0.0) mhs_5s = mhs_av;
    if (mhs_5m == 0.0) mhs_5m = mhs_av;
    if (mhs_1h == 0.0) mhs_1h = mhs_av;
    s->hashrate_5s_ghs = mhs_to_ghs(mhs_5s);
    s->hashrate_5m_ghs = mhs_to_ghs(mhs_5m);
    s->hashrate_1h_ghs = mhs_to_ghs(mhs_1h);

    s->shares_accepted = get_int64(it, "Accepted", 0);
    s->shares_rejected = get_int64(it, "Rejected", 0);
    s->hw_errors       = get_int64(it, "Hardware Errors", 0);
    s->uptime_sec      = (long) get_int64(it, "Elapsed", 0);

    cJSON_Delete(root);
    return 0;
}

/* --- stats: power, board temps, fan rpms, board status ------------------- */

int cgminer_parse_stats(snapshot_t *s, const char *json) {
    if (s == NULL || json == NULL) return -1;
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) return -1;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "STATS");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return -1; }

    /* STATS array typically contains one or two entries; the relevant one
     * has Power/Voltage/temp/fan fields. We aggregate from the first match. */
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n; ++i) {
        cJSON *it = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(it)) continue;

        double p = get_num(it, "Power", 0.0);
        if (p > 0.0 && s->power_w == 0.0) s->power_w = p;

        /* Firmware version, if reported. */
        const char *fw = get_str(it, "Firmware", NULL);
        if (fw != NULL && s->fw_version[0] == '\0') {
            strncpy(s->fw_version, fw, sizeof(s->fw_version) - 1);
            s->fw_version[sizeof(s->fw_version) - 1] = '\0';
        }

        /* Vendor extension: walk all keys, picking up indexed temp/fan/freq.
         * Field-name conventions vary; we accept several common ones:
         *   temp1, temp2_1, temp_pcb1, fan1, fan_num, chain_acn1, freq_avg1 */
        cJSON *child = it->child;
        int max_fan = 0, max_board = 0;

        /* First pass: figure out how many boards/fans we have. */
        for (cJSON *c = child; c != NULL; c = c->next) {
            if (c->string == NULL) continue;
            const char *k = c->string;
            int idx = 0;

            if (strncasecmp(k, "fan", 3) == 0
                && k[3] != '_' && k[3] != '\0'
                && (idx = atoi(k + 3)) > 0
                && idx > max_fan) max_fan = idx;

            /* "chain_acn<i>" → chip count for board i (1-based) */
            if (strncasecmp(k, "chain_acn", 9) == 0
                && (idx = atoi(k + 9)) > 0
                && idx > max_board) max_board = idx;

            /* "temp<i>" is hashboard temp on Antminer-like firmwares */
            if (strncasecmp(k, "temp", 4) == 0
                && k[4] != '_' && k[4] >= '0' && k[4] <= '9'
                && (idx = atoi(k + 4)) > 0
                && idx > max_board) max_board = idx;
        }

        /* Allocate arrays only if we found anything. Preserve already-set
         * counts (a later STATS entry shouldn't shrink). */
        if (max_fan > s->n_fans) snapshot_alloc_fans(s, max_fan);
        if (max_board > s->n_hashboards) snapshot_alloc_hashboards(s, max_board);

        /* Second pass: populate. */
        for (cJSON *c = child; c != NULL; c = c->next) {
            if (c->string == NULL || !cJSON_IsNumber(c)) continue;
            const char *k = c->string;
            int idx = 0;

            if (strncasecmp(k, "fan", 3) == 0
                && k[3] != '_' && k[3] != '\0'
                && (idx = atoi(k + 3)) > 0
                && idx <= s->n_fans) {
                s->fans[idx - 1].index = idx - 1;
                s->fans[idx - 1].rpm   = (int) c->valuedouble;
            }

            if (strncasecmp(k, "chain_acn", 9) == 0
                && (idx = atoi(k + 9)) > 0
                && idx <= s->n_hashboards) {
                s->hashboards[idx - 1].index = idx - 1;
                s->hashboards[idx - 1].chips_active = (int) c->valuedouble;
                if (s->hashboards[idx - 1].chips_total == 0) {
                    s->hashboards[idx - 1].chips_total = (int) c->valuedouble;
                }
            }

            if (strncasecmp(k, "chain_acs", 9) == 0
                && (idx = atoi(k + 9)) > 0
                && idx <= s->n_hashboards) {
                /* chain_acs is the ASCII chip map ("oooo xx oo"); the count of
                 * 'o' characters = total chip slots. */
                const char *map = c->valuestring;
                if (cJSON_IsString(c) && map) {
                    int total = 0;
                    for (const char *ch = map; *ch; ++ch) {
                        if (*ch == 'o' || *ch == 'x') total++;
                    }
                    s->hashboards[idx - 1].chips_total = total;
                }
            }

            if (strncasecmp(k, "temp", 4) == 0
                && k[4] != '_' && k[4] >= '0' && k[4] <= '9'
                && (idx = atoi(k + 4)) > 0
                && idx <= s->n_hashboards) {
                s->hashboards[idx - 1].index = idx - 1;
                s->hashboards[idx - 1].chip_temp_c = c->valuedouble;
            }

            if (strncasecmp(k, "temp2_", 6) == 0
                && (idx = atoi(k + 6)) > 0
                && idx <= s->n_hashboards) {
                /* secondary temp = PCB */
                s->hashboards[idx - 1].pcb_temp_c = c->valuedouble;
            }

            if (strncasecmp(k, "freq_avg", 8) == 0
                && (idx = atoi(k + 8)) > 0
                && idx <= s->n_hashboards) {
                s->hashboards[idx - 1].chip_frequency_mhz = (int) c->valuedouble;
            }
        }

        /* Derive board status: alive if it has any chips_active > 0; fault otherwise.
         * Refined by parse_devs which can flag chip-level faults. */
        for (int b = 0; b < s->n_hashboards; ++b) {
            if (s->hashboards[b].status == HB_STATUS_UNKNOWN) {
                s->hashboards[b].status = (s->hashboards[b].chips_active > 0)
                    ? HB_STATUS_ALIVE : HB_STATUS_FAULT;
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}

/* --- pools --------------------------------------------------------------- */

static pool_status_t parse_pool_status(const char *s) {
    if (s == NULL) return POOL_STATUS_UNKNOWN;
    if (strcasecmp(s, "Alive") == 0) return POOL_STATUS_ALIVE;
    if (strcasecmp(s, "Dead")  == 0) return POOL_STATUS_DEAD;
    return POOL_STATUS_UNKNOWN;
}

int cgminer_parse_pools(snapshot_t *s, const char *json) {
    if (s == NULL || json == NULL) return -1;
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) return -1;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "POOLS");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return -1; }

    int n = cJSON_GetArraySize(arr);
    if (snapshot_alloc_pools(s, n) < 0) { cJSON_Delete(root); return -1; }

    for (int i = 0; i < n; ++i) {
        cJSON *it = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(it)) continue;

        pool_t *p = &s->pools[i];
        p->index = (int) get_int64(it, "POOL", i);
        const char *url = get_str(it, "URL", "");
        strncpy(p->url, url, sizeof(p->url) - 1);
        p->url[sizeof(p->url) - 1] = '\0';

        p->status   = parse_pool_status(get_str(it, "Status", NULL));
        const char *st = get_str(it, "Stratum Active", NULL);
        p->active   = (st != NULL && (strcasecmp(st, "true") == 0
                                      || strcmp(st, "1") == 0));

        p->latency_ms = (int) get_int64(it, "Stratum Latency", -1);
        p->accepted = get_int64(it, "Accepted", 0);
        p->rejected = get_int64(it, "Rejected", 0);
        p->stale    = get_int64(it, "Stale", 0);

        if (p->active && s->active_pool_index < 0) {
            s->active_pool_index = p->index;
        }
    }

    cJSON_Delete(root);
    return 0;
}

/* --- devs: refine chip status / fault detection -------------------------- */

int cgminer_parse_devs(snapshot_t *s, const char *json) {
    if (s == NULL || json == NULL) return -1;
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) return -1;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "DEVS");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return -1; }

    int n = cJSON_GetArraySize(arr);
    /* If stats didn't allocate hashboards but devs has entries, use that. */
    if (s->n_hashboards == 0 && n > 0) {
        snapshot_alloc_hashboards(s, n);
    }

    for (int i = 0; i < n; ++i) {
        cJSON *it = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(it)) continue;
        if (i >= s->n_hashboards) break;

        hashboard_t *hb = &s->hashboards[i];
        hb->index = (int) get_int64(it, "ID", i);

        const char *status = get_str(it, "Status", NULL);
        if (status != NULL) {
            hb->status = (strcasecmp(status, "Alive") == 0)
                ? HB_STATUS_ALIVE : HB_STATUS_FAULT;
        }

        if (hb->chip_temp_c == 0.0) {
            hb->chip_temp_c = get_num(it, "Temperature", 0.0);
        }
    }

    cJSON_Delete(root);
    return 0;
}
