#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <clog/logging.h>

#include "server.h"
#include "collector.h"
#include "storage.h"
#include "settings.h"

pthread_t collector_tid;
pthread_t storage_tid;

void startup() {
    int status = 0;

    status = extract_settings("settings.json");
    if (status != 0) {
        printf("Error occurred: cannot extract settings.json!\n");
        return;
    }

    if (get_debug_flag()) {
        set_log_debug();
    }
    
    set_log_flags(LOG_SHORT);
    
    log_file_append(get_log_path());

    status = server_start(get_server_port());
    if (status != 0) {
        printf("An error occurred!\n");
    }

    status = storage_start(get_db_path());
    if (status != 0) {
        printf("An error occurred during the storage starting\n");
    }
    storage_set_update_period(5);
}

void finish() {
    server_stop();

    log_exit();
}

void create_threads() {
    int status = pthread_create(&collector_tid, NULL, collect_loop, NULL);
    if (status != 0) {
        perror("collector loop creation");
        return;
    }

    status = pthread_create(&storage_tid, NULL, storage_loop, NULL);
    if (status != 0) {
        perror("storage loop creation");
        return;
    }
}


int main() {
    startup();
    create_threads();

    pthread_join(collector_tid, NULL);
    pthread_join(storage_tid, NULL);

    finish();
    
    return 0;
}