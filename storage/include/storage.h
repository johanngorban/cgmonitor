#pragma once

#include "miner_info.h"
#include "database.h"
#include "cache.h"

void storage_set_update_period(unsigned seconds);

int storage_start(const char *db_path);

// Storage main loop
void *storage_loop(void *arg);

int storage_save_miner_info(const miner_info *info, time_t timestamp);

int storage_get_miner_info(miner_record *out);

int storage_get_all_miner_info(miner_record **out, int max_count);