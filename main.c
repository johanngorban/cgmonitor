#include <pthread.h>
#include <stdio.h>

#include "server.h"
#include "collector.h"
#include "storage.h"
#include "logging.h"

pthread_t collector_tid;
pthread_t storage_tid;

void startup() {
    log_init("cgmonitor.log");

    int status = server_start(9097);
    if (status != 0) {
        printf("An error occurred!\n");
    }
}

void finish() {
    server_stop();
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
    
    return 0;
}