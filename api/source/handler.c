#include "handler.h"
#include "utils.h"
#include "miner_info.h"
#include "storage.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// typedef struct {
//     char            method[MAX_HTTP_METHOD_LENGTH];
//     char            url[MAX_ENDPOINT_LENGTH];
//     const handler_t handle;
// } http_handler_t;

const http_handler_t handler_table[] = {
    {"GET", "/api/metrics/general", handle_metrics_general},
    {"GET", "/api/device-info", handle_device_info}
};

handler_t find_handler(const char *url, const char *method) {
    size_t handlers_count = sizeof(handler_table) / sizeof(handler_table[0]);

    handler_t handler = handle_unknown;
    for (size_t i = 0; i < handlers_count; i++) {
        if ((strcmp(url, handler_table[i].url) == 0) && (strcmp(method, handler_table[i].method) == 0)) {
            handler = handler_table[i].handle;
        }
    }

    return handler;
}

int handle_metrics_general(struct MHD_Connection *connection, void *data) {
    miner_record info;
    storage_get_miner_info(&info);

    cJSON *root = cJSON_CreateObject();
        
    cJSON_AddNumberToObject(root, "hashrate", info.data.hashrate);
    cJSON_AddNumberToObject(root, "voltage", info.data.voltage);
    cJSON_AddNumberToObject(root, "power", info.data.power);

    char *json_str = cJSON_PrintUnformatted(root);

    int status = send_json(connection, json_str);
    free(json_str);
    cJSON_Delete(root);

    return status;
}

int handle_device_info(struct MHD_Connection *connection, void *data) {
    char *device_model = get_miner_model();

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "model", device_model);

    char *json_str = cJSON_PrintUnformatted(root);

    int status = send_json(connection, json_str);
    free(json_str);
    cJSON_Delete(root);

    return status;
}

int handle_unknown(struct MHD_Connection *connection, void *data) {
    cJSON *err = cJSON_CreateObject();
    cJSON_AddStringToObject(err, "error", "Unknown endpoint");
    char *err_str = cJSON_PrintUnformatted(err);

    int status = send_json(connection, err_str);
    free(err_str);
    cJSON_Delete(err);

    return status;
}