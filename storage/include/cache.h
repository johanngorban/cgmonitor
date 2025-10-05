#pragma once

#include "miner_info.h"

int cache_get_miner_info(miner_record *miner);

int cache_put_miner_info(const miner_info *miner, time_t t);

void cache_clear();