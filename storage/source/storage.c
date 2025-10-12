#include "storage.h"

#include <unistd.h>
#include <stdio.h>

#define DB_PATH "data/miner_data.db"
#define STORAGE_PERIOD_SEC 5

void *storage_loop(void *arg) {
    int status;
    
    status = db_init(DB_PATH);
    if (status < 0) {
        return NULL;
    }
    
    while (1) {
        sleep(STORAGE_PERIOD_SEC);
    }

    return NULL;
}

int storage_save_miner_info(const miner_info *info, time_t timestamp) {
    if (info == NULL) {
        return -1;
    }

    int status = cache_put_miner_info(info, timestamp);
    if (status != 0) {
        return -1;
    }

    return db_insert_miner_info(info, timestamp);
}

int storage_get_miner_info(miner_record *out) {
    if (out == NULL) {
        return -1;
    }

    int status = cache_get_miner_info(out);
    if (status == 0) {
        return 0;
    }

    return db_get_last_miner_info(out);
}

int storage_get_all_miner_info(miner_record **out, int max_count) {
    return db_get_all_miner_info(out, max_count);
}