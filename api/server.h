#pragma once

#include <microhttpd.h>
#include <unistd.h>

int server_start(int port);

int server_stop();

enum MHD_Result handle_requests(void *cls, struct MHD_Connection *connection,
                    const char *url, const char *method,
                    const char *version, const char *upload_data,
                    size_t *upload_data_size, void **con_cls);