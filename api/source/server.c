#include "server.h"
#include "miner_info.h"
#include "handler.h"

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

enum MHD_Result handle_requests(void *cls, struct MHD_Connection *connection,
                    const char *url, const char *method,
                    const char *version, const char *upload_data,
                    size_t *upload_data_size, void **con_cls) 
{
    handler_t handler = find_handler(url, method);
    if (handler == NULL) {
        return 0;
    }

    return handler(connection, NULL);
}
