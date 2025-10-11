#pragma once

#include <microhttpd.h>

#define MAX_ENDPOINT_LENGTH 256
#define MAX_HTTP_METHOD_LENGTH 8

typedef int (*handler_t)(struct MHD_Connection *, void *); 

typedef struct {
    char            method[MAX_HTTP_METHOD_LENGTH];
    char            url[MAX_ENDPOINT_LENGTH];
    const handler_t handle;
} http_handler_t;

handler_t find_handler(const char *url, const char *method);

int handle_metrics_general(struct MHD_Connection *, void *data);

int handle_device_info(struct MHD_Connection *, void *data);

int handle_unknown(struct MHD_Connection *, void *data);