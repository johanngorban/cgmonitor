#pragma once

#include "database.h"
#include "cache.h"

// Store miner info and time
typedef struct {
    miner_info data;
    time_t timestamp;
} miner_record;

// Storage main loop
void *storage_loop(void *arg);