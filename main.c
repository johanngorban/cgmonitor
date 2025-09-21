#include <pthread.h>
#include <stdio.h>

#define DEBUG

#include "collector.h"
#include "asic_info.h"


int main() {
    int status = 0;

    pthread_t tid;

    status = pthread_create(&tid, NULL, collect_loop, NULL);
    if (status != 0) {
        perror("collector loop creation");
        return -1;
    }

    while (1) {
        printf("Main thread alive\n");
        sleep(5);
    }

    return 0;
}