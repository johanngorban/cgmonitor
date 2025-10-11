#pragma once

#include <microhttpd.h>

int send_json(struct MHD_Connection *connection, const char *json);