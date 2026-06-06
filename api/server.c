#include "server.h"
#include "json_out.h"
#include "static_files.h"
#include "cache.h"
#include "database.h"
#include "snapshot.h"
#include "logging.h"

#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct MHD_Daemon *daemon_ = NULL;
static const config_t    *active_cfg = NULL;  /* snapshot of config for /api/config */

/* MHD compatibility — older headers return int, newer return MHD_Result. */
#if MHD_VERSION >= 0x00097002
  #define MHD_RESULT enum MHD_Result
#else
  #define MHD_RESULT int
#endif

static MHD_RESULT respond_buf(struct MHD_Connection *c, unsigned int status,
                               const char *content_type,
                               const void *data, size_t len, int must_copy)
{
    struct MHD_Response *r = MHD_create_response_from_buffer(
        len, (void *) data,
        must_copy ? MHD_RESPMEM_MUST_COPY : MHD_RESPMEM_PERSISTENT);
    if (r == NULL) return MHD_NO;
    if (content_type != NULL)
        MHD_add_response_header(r, "Content-Type", content_type);
    /* CORS for browser clients on another host */
    MHD_add_response_header(r, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(r, "Cache-Control", "no-store");
    MHD_RESULT rc = MHD_queue_response(c, status, r);
    MHD_destroy_response(r);
    return rc;
}

static MHD_RESULT respond_json(struct MHD_Connection *c, unsigned int status,
                                const char *json)
{
    /* json is heap-allocated by caller; we copy then they free. */
    return respond_buf(c, status, "application/json; charset=utf-8",
                       json, strlen(json), 1);
}

static MHD_RESULT respond_error(struct MHD_Connection *c, unsigned int status,
                                 const char *msg)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    return respond_buf(c, status, "application/json; charset=utf-8",
                       buf, (size_t) n, 1);
}

/* --- handlers ------------------------------------------------------------ */

static MHD_RESULT handle_snapshot(struct MHD_Connection *c) {
    snapshot_t snap;
    snapshot_init(&snap);
    if (cache_get(&snap) < 0) {
        snapshot_free(&snap);
        return respond_error(c, MHD_HTTP_SERVICE_UNAVAILABLE, "cache unavailable");
    }
    char *json = snapshot_to_json(&snap);
    snapshot_free(&snap);
    if (json == NULL) return respond_error(c, MHD_HTTP_INTERNAL_SERVER_ERROR, "oom");
    MHD_RESULT rc = respond_json(c, MHD_HTTP_OK, json);
    free(json);
    return rc;
}

static MHD_RESULT handle_history(struct MHD_Connection *c) {
    const char *param = MHD_lookup_connection_value(c, MHD_GET_ARGUMENT_KIND, "param");
    const char *range = MHD_lookup_connection_value(c, MHD_GET_ARGUMENT_KIND, "range");
    if (param == NULL) {
        return respond_error(c, MHD_HTTP_BAD_REQUEST, "missing 'param'");
    }
    long range_sec = 3600;
    if (range != NULL) range_sec = atol(range);
    if (range_sec < 60) range_sec = 60;

    char *json = NULL;
    if (database_query_history(param, range_sec, &json) < 0) {
        if (json) free(json);
        return respond_error(c, MHD_HTTP_BAD_REQUEST, "unknown param or storage disabled");
    }
    MHD_RESULT rc = respond_json(c, MHD_HTTP_OK, json);
    free(json);
    return rc;
}

static MHD_RESULT handle_config(struct MHD_Connection *c) {
    char *json = config_to_json_subset((const struct config_t *) active_cfg);
    if (json == NULL) return respond_error(c, MHD_HTTP_INTERNAL_SERVER_ERROR, "oom");
    MHD_RESULT rc = respond_json(c, MHD_HTTP_OK, json);
    free(json);
    return rc;
}

static MHD_RESULT handle_health(struct MHD_Connection *c) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "{\"ok\":true,\"has_data\":%s}",
                     cache_has_data() ? "true" : "false");
    return respond_buf(c, MHD_HTTP_OK, "application/json; charset=utf-8",
                       buf, (size_t) n, 1);
}

static MHD_RESULT handle_static(struct MHD_Connection *c, const char *url) {
    const static_file_t *f = static_files_get(url);
    if (f == NULL) {
        return respond_buf(c, MHD_HTTP_NOT_FOUND,
                           "text/plain", "Not Found", 9, 0);
    }
    /* persistent: data lives in .rodata, no copy needed */
    return respond_buf(c, MHD_HTTP_OK, f->content_type, f->data, f->size, 0);
}

/* --- dispatcher ---------------------------------------------------------- */

static MHD_RESULT handle_request(void *cls, struct MHD_Connection *connection,
                                  const char *url, const char *method,
                                  const char *version, const char *upload_data,
                                  size_t *upload_data_size, void **con_cls)
{
    (void) cls; (void) version; (void) upload_data; (void) upload_data_size;
    (void) con_cls;

    if (strcmp(method, "OPTIONS") == 0) {
        return respond_buf(connection, MHD_HTTP_NO_CONTENT, NULL, "", 0, 0);
    }
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        return respond_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "method not allowed");
    }
    /* libmicrohttpd strips the body for HEAD itself if we send the headers,
     * so we can treat HEAD identically to GET below. */

    if      (strcmp(url, "/api/snapshot") == 0) return handle_snapshot(connection);
    else if (strcmp(url, "/api/history")  == 0) return handle_history(connection);
    else if (strcmp(url, "/api/config")   == 0) return handle_config(connection);
    else if (strcmp(url, "/api/health")   == 0) return handle_health(connection);

    /* legacy compat with the original spec — best-effort. */
    if (strcmp(url, "/api/metrics/general") == 0
        || strcmp(url, "/api/metrics/fans") == 0
        || strcmp(url, "/api/metrics/chips") == 0) {
        return handle_snapshot(connection);  /* frontend should migrate to /api/snapshot */
    }

    /* fall through: static asset */
    return handle_static(connection, url);
}

int server_start(const config_t *cfg) {
    if (cfg == NULL) return -1;
    if (daemon_ != NULL) return -1;
    active_cfg = cfg;

    unsigned int flags = MHD_USE_INTERNAL_POLLING_THREAD;
    daemon_ = MHD_start_daemon(flags, (uint16_t) cfg->server_port,
                               NULL, NULL,
                               &handle_request, NULL,
                               MHD_OPTION_CONNECTION_TIMEOUT, (unsigned) 30,
                               MHD_OPTION_END);
    if (daemon_ == NULL) {
        log_error("HTTP daemon failed to start on port %d", cfg->server_port);
        return -1;
    }
    log_info("HTTP server listening on %s:%d", cfg->server_bind, cfg->server_port);
    return 0;
}

void server_stop(void) {
    if (daemon_ != NULL) {
        MHD_stop_daemon(daemon_);
        daemon_ = NULL;
    }
}
