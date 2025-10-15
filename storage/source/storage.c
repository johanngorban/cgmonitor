#include "storage.h"

#include <unistd.h>
#include <stdio.h>
#include <clog/logging.h>
#include <time.h>

static unsigned update_period = 5;

void storage_set_update_period(unsigned seconds) {
    log_info("Setting storage update period to %us", seconds);
    update_period = seconds;
}

int storage_start(const char *db_path) {    
    log_info("Starting storage...");

    int status = db_init(db_path);
    if (status < 0) {
        log_error("Failed to init database");
    }

    return status;
}

void *storage_loop(void *arg) {
    log_info("Starting storage loop");

    while (1) {
        sleep(update_period);
    }

    log_info("Storage terminated");
    return NULL;
}

int storage_save_miner_info(const miner_info *info, time_t timestamp) {
    if (info == NULL) {
        return -1;
    }

    int status = cache_put_miner_info(info, timestamp);
    if (status < 0) {
        log_error("Cannot save miner info to cache");
        return -1;
    }

    status = db_insert_miner_info(info, timestamp);
    if (status < 0) {
        log_error("Cannot save miner info to database");
    }

    return status;
}

int storage_get_miner_info(miner_record *out) {
    if (out == NULL) {
        return -1;
    }

    int status = cache_get_miner_info(out);
    if (status == 0) {
        log_error("Cannot extract miner info from cache");
        return 0;
    }

    status = db_get_last_miner_info(out);
    if (status < 0) {
        log_error("Cannot extract miner info from database");
    }

    return status;
}

int storage_get_all_miner_info(miner_record **out, int max_count) {
    int extracted = db_get_all_miner_info(out, max_count);
    if (extracted < 0) {
        log_error("Cannot extract %d records of miner info from database", max_count);
    }
    else {
        log_info("Extracted %d records of miner info from database", extracted);
    }

    return extracted;
}

int storage_get_new_miner_info(miner_record **out, time_t newer_than) {
    return db_get_new_miner_info(out, newer_than);
}