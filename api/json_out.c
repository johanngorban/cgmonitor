#include "json_out.h"
#include "config.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *hb_status_str(hashboard_status_t s) {
    switch (s) {
        case HB_STATUS_ALIVE: return "alive";
        case HB_STATUS_FAULT: return "fault";
        default:              return "unknown";
    }
}

static const char *pool_status_str(pool_status_t s) {
    switch (s) {
        case POOL_STATUS_ALIVE: return "alive";
        case POOL_STATUS_DEAD:  return "dead";
        default:                return "unknown";
    }
}

char *snapshot_to_json(const snapshot_t *s) {
    if (s == NULL) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "timestamp",  (double) s->timestamp);
    cJSON_AddBoolToObject  (root, "has_data",   s->has_data);
    cJSON_AddStringToObject(root, "fw_version", s->fw_version);

    /* miner-wide block */
    cJSON *m = cJSON_AddObjectToObject(root, "miner");
    cJSON_AddNumberToObject(m, "uptime_sec",          (double) s->uptime_sec);
    cJSON_AddNumberToObject(m, "hashrate_5s_ghs",     s->hashrate_5s_ghs);
    cJSON_AddNumberToObject(m, "hashrate_5m_ghs",     s->hashrate_5m_ghs);
    cJSON_AddNumberToObject(m, "hashrate_1h_ghs",     s->hashrate_1h_ghs);
    cJSON_AddNumberToObject(m, "shares_accepted",     (double) s->shares_accepted);
    cJSON_AddNumberToObject(m, "shares_rejected",     (double) s->shares_rejected);
    cJSON_AddNumberToObject(m, "hw_errors",           (double) s->hw_errors);
    cJSON_AddNumberToObject(m, "power_w",             s->power_w);
    cJSON_AddNumberToObject(m, "efficiency_j_per_ghs", s->efficiency_j_per_ghs);
    cJSON_AddNumberToObject(m, "active_pool_index",   s->active_pool_index);

    /* derived percentages — frontend could compute, but it's cleaner here */
    long long total = s->shares_accepted + s->shares_rejected;
    double rej_pct = total > 0 ? (double) s->shares_rejected * 100.0 / (double) total : 0.0;
    double hwe_pct = total > 0 ? (double) s->hw_errors       * 100.0 / (double) total : 0.0;
    cJSON_AddNumberToObject(m, "shares_rejected_pct", rej_pct);
    cJSON_AddNumberToObject(m, "hw_errors_pct",       hwe_pct);

    /* hashboards */
    cJSON *hbs = cJSON_AddArrayToObject(root, "hashboards");
    for (int i = 0; i < s->n_hashboards; ++i) {
        const hashboard_t *b = &s->hashboards[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "index",              b->index);
        cJSON_AddStringToObject(o, "status",             hb_status_str(b->status));
        cJSON_AddNumberToObject(o, "chip_temp_c",        b->chip_temp_c);
        cJSON_AddNumberToObject(o, "pcb_temp_c",         b->pcb_temp_c);
        cJSON_AddNumberToObject(o, "chip_frequency_mhz", b->chip_frequency_mhz);
        cJSON_AddNumberToObject(o, "chips_active",       b->chips_active);
        cJSON_AddNumberToObject(o, "chips_total",        b->chips_total);
        cJSON_AddItemToArray(hbs, o);
    }

    /* fans */
    cJSON *fs = cJSON_AddArrayToObject(root, "fans");
    for (int i = 0; i < s->n_fans; ++i) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "index", s->fans[i].index);
        cJSON_AddNumberToObject(o, "rpm",   s->fans[i].rpm);
        cJSON_AddItemToArray(fs, o);
    }

    /* pools */
    cJSON *ps = cJSON_AddArrayToObject(root, "pools");
    for (int i = 0; i < s->n_pools; ++i) {
        const pool_t *p = &s->pools[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "index",      p->index);
        cJSON_AddStringToObject(o, "url",        p->url);
        cJSON_AddStringToObject(o, "status",     pool_status_str(p->status));
        cJSON_AddBoolToObject  (o, "active",     p->active);
        cJSON_AddNumberToObject(o, "latency_ms", p->latency_ms);
        cJSON_AddNumberToObject(o, "accepted",   (double) p->accepted);
        cJSON_AddNumberToObject(o, "rejected",   (double) p->rejected);
        cJSON_AddNumberToObject(o, "stale",      (double) p->stale);
        cJSON_AddItemToArray(ps, o);
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

char *config_to_json_subset(const struct config_t *cfg_in) {
    const config_t *cfg = (const config_t *) cfg_in;
    if (cfg == NULL) return NULL;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "fw_poll_interval_ms", cfg->fw_poll_interval_ms);
    cJSON_AddStringToObject(o, "fw_protocol",         cfg->fw_protocol);
    cJSON_AddNumberToObject(o, "temp_warn_c",         cfg->temp_warn_c);
    cJSON_AddNumberToObject(o, "temp_crit_c",         cfg->temp_crit_c);
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}
