#include "server.h"
#include "miner_info.h"
#include "storage.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <cjson/cJSON.h>

static struct MHD_Daemon *serverd = NULL;

int server_start(int port) {
    if (serverd != NULL) {
        return -1;
    }

    serverd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, &handle_requests, NULL, MHD_OPTION_END);

    if (serverd != NULL) {
        return 0;
    }

    return -1;
}

int server_stop() {
    if (serverd != NULL) {
        MHD_stop_daemon(serverd);
        serverd = NULL;
    }

    return 0;
}

static int send_json(struct MHD_Connection *connection, const char *json) {
    struct MHD_Response *response;
    response = MHD_create_response_from_buffer(strlen(json), (void *) json, MHD_RESPMEM_MUST_COPY);

    MHD_add_response_header(response, "Content-Type", "application/json");
    int status = MHD_queue_response(connection, MHD_HTTP_OK, response);

    MHD_destroy_response(response);

    return status;
}

enum MHD_Result handle_requests(void *cls, struct MHD_Connection *connection,
                    const char *url, const char *method,
                    const char *version, const char *upload_data,
                    size_t *upload_data_size, void **con_cls) 
{
    if (strcmp(url, "/api/metrics/general") == 0 && strcmp(method, "GET") == 0) {
        miner_record info;
        storage_get_miner_info(&info);

        char buf[512];
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "hashrate", info.data.hashrate);
        cJSON_AddNumberToObject(root, "utility", info.data.utility);
        cJSON_AddNumberToObject(root, "power", info.data.power);

        char *json_str = cJSON_PrintUnformatted(root);

        int status = send_json(connection, json_str);
        free(json_str);
        cJSON_Delete(root);

        return status;
    }

    cJSON *err = cJSON_CreateObject();
    cJSON_AddStringToObject(err, "error", "Unknown endpoint");
    char *err_str = cJSON_PrintUnformatted(err);

    int status = send_json(connection, err_str);
    free(err_str);
    cJSON_Delete(err);

    return status;
}
