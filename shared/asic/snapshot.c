#include "snapshot.h"

#include <stdlib.h>
#include <string.h>

void snapshot_init(snapshot_t *s) {
    if (s == NULL) return;
    memset(s, 0, sizeof(*s));
    s->active_pool_index = -1;
}

void snapshot_free(snapshot_t *s) {
    if (s == NULL) return;
    free(s->hashboards);
    free(s->fans);
    free(s->pools);
    /* re-zero so a freed snapshot is also a valid empty one */
    memset(s, 0, sizeof(*s));
    s->active_pool_index = -1;
}

int snapshot_alloc_hashboards(snapshot_t *s, int n) {
    if (s == NULL || n < 0) return -1;
    free(s->hashboards);
    s->hashboards = NULL;
    s->n_hashboards = 0;
    if (n == 0) return 0;
    s->hashboards = (hashboard_t *) calloc((size_t) n, sizeof(hashboard_t));
    if (s->hashboards == NULL) return -1;
    s->n_hashboards = n;
    return 0;
}

int snapshot_alloc_fans(snapshot_t *s, int n) {
    if (s == NULL || n < 0) return -1;
    free(s->fans);
    s->fans = NULL;
    s->n_fans = 0;
    if (n == 0) return 0;
    s->fans = (fan_t *) calloc((size_t) n, sizeof(fan_t));
    if (s->fans == NULL) return -1;
    s->n_fans = n;
    return 0;
}

int snapshot_alloc_pools(snapshot_t *s, int n) {
    if (s == NULL || n < 0) return -1;
    free(s->pools);
    s->pools = NULL;
    s->n_pools = 0;
    if (n == 0) return 0;
    s->pools = (pool_t *) calloc((size_t) n, sizeof(pool_t));
    if (s->pools == NULL) return -1;
    s->n_pools = n;
    return 0;
}

int snapshot_copy(snapshot_t *dest, const snapshot_t *src) {
    if (dest == NULL || src == NULL) return -1;
    snapshot_free(dest);

    /* Copy POD parts first */
    dest->timestamp            = src->timestamp;
    dest->has_data             = src->has_data;
    dest->uptime_sec           = src->uptime_sec;
    dest->hashrate_5s_ghs      = src->hashrate_5s_ghs;
    dest->hashrate_5m_ghs      = src->hashrate_5m_ghs;
    dest->hashrate_1h_ghs      = src->hashrate_1h_ghs;
    dest->shares_accepted      = src->shares_accepted;
    dest->shares_rejected      = src->shares_rejected;
    dest->hw_errors            = src->hw_errors;
    dest->power_w              = src->power_w;
    dest->efficiency_j_per_ghs = src->efficiency_j_per_ghs;
    dest->active_pool_index    = src->active_pool_index;
    memcpy(dest->fw_version, src->fw_version, sizeof(dest->fw_version));

    if (snapshot_alloc_hashboards(dest, src->n_hashboards) < 0) return -1;
    if (src->n_hashboards > 0) {
        memcpy(dest->hashboards, src->hashboards,
               (size_t) src->n_hashboards * sizeof(hashboard_t));
    }

    if (snapshot_alloc_fans(dest, src->n_fans) < 0) return -1;
    if (src->n_fans > 0) {
        memcpy(dest->fans, src->fans,
               (size_t) src->n_fans * sizeof(fan_t));
    }

    if (snapshot_alloc_pools(dest, src->n_pools) < 0) return -1;
    if (src->n_pools > 0) {
        memcpy(dest->pools, src->pools,
               (size_t) src->n_pools * sizeof(pool_t));
    }

    return 0;
}

void snapshot_derive(snapshot_t *s) {
    if (s == NULL) return;
    /* Efficiency: prefer instantaneous, fall back to 5m if 5s is zero. */
    double hr_ghs = s->hashrate_5s_ghs;
    if (hr_ghs <= 0.0) hr_ghs = s->hashrate_5m_ghs;
    if (hr_ghs <= 0.0) hr_ghs = s->hashrate_1h_ghs;

    if (s->efficiency_j_per_ghs == 0.0 && hr_ghs > 0.0 && s->power_w > 0.0) {
        s->efficiency_j_per_ghs = s->power_w / hr_ghs;
    }
}
