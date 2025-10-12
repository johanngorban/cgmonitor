#include "utils.h"

#include <string.h>

int send_json(struct MHD_Connection *connection, const char *json) {
    struct MHD_Response *response;
    response = MHD_create_response_from_buffer(strlen(json), (void *) json, MHD_RESPMEM_MUST_COPY);

    MHD_add_response_header(response, "Content-Type", "application/json");
    int status = MHD_queue_response(connection, MHD_HTTP_OK, response);

    MHD_destroy_response(response);

    return status;
}