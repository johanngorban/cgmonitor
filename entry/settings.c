#include "settings.h"
#include <cjson/cJSON.h>
#include <clog/logging.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char log_path[64];
static char db_path[64];

static int debug = 0;

static int server_port;

static int miner_port;
static unsigned polling_interval;
static unsigned connection_retry_limit;

int get_debug_flag() {
    return debug;
}

const char *get_log_path() {
    return log_path;
}

const char *get_db_path() {
    return db_path;
}

int get_server_port() {
    return server_port;
}

int get_miner_port() {
    return miner_port;
}

int get_polling_interval() {
    return polling_interval;
}

int get_connection_retry_limit() {
    return connection_retry_limit;
}

int extract_settings(const char *path) {
    FILE *f = fopen(path, "rb");
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
        log_fatal("Error parsing settings.json: %s\n", cJSON_GetErrorPtr());
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