#include "cache.h"

miner_info cache = {0};

static int copy_miner_info(miner_info *restrict dest, const miner_info *restrict src) {
    if (dest == NULL || src == NULL) {
        return -1;
    }

    dest->hashrate = src->hashrate;
    dest->utility  = src->utility;
    dest->power    = src->power;
    dest->voltage  = src->voltage;
    dest->uptime   = src->uptime;

    return 0;
}

int storage_save_miner_info(const miner_info *miner) {
    return copy_miner_info(&cache, miner);
}

int storage_get_miner_info(miner_info *miner)  {
    return copy_miner_info(miner, &cache);
}