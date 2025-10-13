#include "storage.h"

#include <unistd.h>
#include <stdio.h>

static unsigned update_period = 5;

void storage_set_update_period(unsigned seconds) {
    if (seconds > 0) {
        update_period = seconds;
    }
}

int storage_start(const char *db_path) {    
    int status = db_init(db_path);

    return status;
}

void *storage_loop(void *arg) {
    while (1) {
        sleep(update_period);
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