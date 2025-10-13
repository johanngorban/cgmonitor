#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cjson/cJSON.h>

#include "server.h"
#include "collector.h"
#include "storage.h"
#include "logging.h"

pthread_t collector_tid;
pthread_t storage_tid;

// TODO: create entry point with config data
char log_path[64];
char db_path[64];
int debug = 0;

char server_bind[16];
int server_port;

char miner_bind[16];
int miner_port;
unsigned polling_interval;
unsigned connection_retry_limit;

// TODO: move it in separated file in entry point
int extract_settings() {
    FILE *f = fopen("settings.json", "rb");
    if (f == NULL) {
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = malloc(length + 1);
    if (!data) {
        fclose(f);
        return -1;
    }

    fread(data, 1, length, f);
    data[length] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    free(data);

    if (root == NULL) {
        fprintf(stderr, "Error parsing settings.json: %s\n", cJSON_GetErrorPtr());
        return -1;
    }

    cJSON *debug_item = cJSON_GetObjectItemCaseSensitive(root, "debug");
    if (cJSON_IsBool(debug_item)) {
        debug = cJSON_IsTrue(debug_item);
    }
    else {
        return -1;
    }

    cJSON *log_item = cJSON_GetObjectItemCaseSensitive(root, "log_file");
    if (cJSON_IsString(log_item) && log_item->valuestring) {
        strncpy(log_path, log_item->valuestring, sizeof(log_path) - 1);
    }
    else {
        return -1;
    }

    cJSON *db_item = cJSON_GetObjectItemCaseSensitive(root, "db_path");
    if (cJSON_IsString(db_item) && db_item->valuestring) {
        strncpy(db_path, db_item->valuestring, sizeof(db_path) - 1);
    }
    else {
        return -1;
    }

    cJSON *srv_port_item = cJSON_GetObjectItemCaseSensitive(root, "server_port");
    if (cJSON_IsNumber(srv_port_item)) {
        server_port = srv_port_item->valueint;
    }
    else {
        return -1;
    }

    cJSON *srv_bind_item = cJSON_GetObjectItemCaseSensitive(root, "server_bind");
    if (cJSON_IsString(srv_bind_item) && srv_bind_item->valuestring) {
        strncpy(server_bind, srv_bind_item->valuestring, sizeof(server_bind) - 1);
    }
    else {
        return -1;
    }

    cJSON *miner_bind_item = cJSON_GetObjectItemCaseSensitive(root, "miner_bind");
    if (cJSON_IsString(miner_bind_item) && miner_bind_item->valuestring) {
        strncpy(miner_bind, miner_bind_item->valuestring, sizeof(miner_bind) - 1);
    }
    else {
        return -1;
    }

    cJSON *miner_port_item = cJSON_GetObjectItemCaseSensitive(root, "miner_port");
    if (cJSON_IsNumber(miner_port_item)) {
        miner_port = miner_port_item->valueint;
    }
    else {
        return -1;
    }

    cJSON *poll_item = cJSON_GetObjectItemCaseSensitive(root, "polling_interval");
    if (cJSON_IsNumber(poll_item)) {
        polling_interval = (unsigned)poll_item->valueint;
    }
    else {
        return -1;
    }

    cJSON *retry_item = cJSON_GetObjectItemCaseSensitive(root, "connection_retry_limit");
    if (cJSON_IsNumber(retry_item)) {
        connection_retry_limit = (unsigned)retry_item->valueint;
    }
    else {
        return -1;
    }

    cJSON_Delete(root);

    return 0;
}

void startup() {
    int status = 0;

    status = extract_settings();
    if (status != 0) {
        printf("Error occurred: cannot extract settings.json!\n");
        return;
    }

    if (debug) {
        set_log_debug();
    }
    
    set_log_flags(LOG_SHORT);
    
    log_file_append(log_path);

    status = server_start(server_port);
    if (status != 0) {
        printf("An error occurred!\n");
    }

    status = storage_start(db_path);
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