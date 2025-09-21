#include <pthread.h>
#include <stdio.h>

#include "collector.h"
#include "crud.h"

pthread_t collector_tid;

void startup() {
    storage_init("data/asic_data.db");
}

void create_threads() {
    int status = pthread_create(&collector_tid, NULL, collect_loop, NULL);
    if (status != 0) {
        perror("collector loop creation");
        return;
    }
}


int main() {
    startup();
    create_threads();

    pthread_join(collector_tid, NULL);
    
    return 0;
}