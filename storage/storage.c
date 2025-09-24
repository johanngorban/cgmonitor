#include "storage.h"

#include <unistd.h>
#include <stdio.h>

#define DB_PATH "data/miner_data.db"
#define STORAGE_PERIOD_SEC 5

void *storage_loop(void *arg) {
    int status;
    
    status = database_init(DB_PATH);
    if (status < 0) {
        return NULL;
    }
    
    while (1) {
        sleep(STORAGE_PERIOD_SEC);
    }

    return NULL;
}