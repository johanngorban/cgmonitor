#include "cache.h"
#include <string.h>

static miner_record cache = {0};
static int cached = 0;

int cache_get_miner_info(miner_record *miner) {
    if (cached == 0 || miner == NULL) {
        return -1;
    }

    *miner = cache;
    return 0;
}

int cache_put_miner_info(const miner_info *miner, time_t t) {
    if (miner == NULL) {
        return -1;
    }

    cache.data = *miner;
    cache.time = t;

    cached = 1;
    return 0;
}

void cache_clear() {
    memset(&cache, 0, sizeof(cache));
    cached = 0;
}