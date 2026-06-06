/**
 * cgminer.c — cgminer JSON-API implementation of firmware_protocol_t.
 *
 * Each fetch() opens a fresh TCP connection per command (cgminer historically
 * closes the connection after each response — this is well-known behavior).
 * We issue: summary, stats, pools, devs — and aggregate results into one
 * snapshot.
 *
 * Failure mode: if `summary` fails we return error. If subsidiary commands
 * fail we keep going — partial data is better than none.
 */
#include "cgminer.h"
#include "cgminer_parse.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
    char host[64];
    int  port;
    int  connect_timeout_ms;
    int  read_timeout_ms;
} cgminer_ctx_t;

/* --- low-level helpers --------------------------------------------------- */

static int set_socket_timeouts(int fd, int rd_ms) {
    struct timeval tv;
    tv.tv_sec  = rd_ms / 1000;
    tv.tv_usec = (rd_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) return -1;
    return 0;
}

/* Connect with a non-blocking socket so we can enforce connect_timeout_ms. */
static int connect_with_timeout(const char *host, int port,
                                int connect_ms, int rw_ms)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    /* non-blocking for connect */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) { close(fd); return -1; }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { close(fd); return -1; }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t) port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) { close(fd); return -1; }

    int rc = connect(fd, (struct sockaddr *) &sa, sizeof(sa));
    if (rc < 0 && errno != EINPROGRESS) { close(fd); return -1; }

    if (rc != 0) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv;
        tv.tv_sec  = connect_ms / 1000;
        tv.tv_usec = (connect_ms % 1000) * 1000;
        rc = select(fd + 1, NULL, &wfds, NULL, &tv);
        if (rc <= 0) { close(fd); return -1; }

        int err = 0;
        socklen_t elen = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen) < 0 || err != 0) {
            close(fd); return -1;
        }
    }

    /* back to blocking + per-op timeouts */
    if (fcntl(fd, F_SETFL, flags) < 0) { close(fd); return -1; }
    if (set_socket_timeouts(fd, rw_ms) < 0) { close(fd); return -1; }

    /* small write buffer is fine; disable Nagle since we send tiny commands */
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    return fd;
}

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) return -1;
        sent += (size_t) n;
    }
    return 0;
}

/* Read until peer closes. Caller frees *out. */
static int read_all(int fd, char **out, size_t *out_len) {
    size_t cap = 4096;
    size_t len = 0;
    char  *buf = (char *) malloc(cap);
    if (buf == NULL) return -1;
    for (;;) {
        if (len + 1 >= cap) {
            cap *= 2;
            if (cap > 2 * 1024 * 1024) { free(buf); return -1; } /* sanity cap */
            char *nb = (char *) realloc(buf, cap);
            if (nb == NULL) { free(buf); return -1; }
            buf = nb;
        }
        ssize_t n = recv(fd, buf + len, cap - len - 1, 0);
        if (n < 0) { if (errno == EINTR) continue; free(buf); return -1; }
        if (n == 0) break;
        len += (size_t) n;
    }
    buf[len] = '\0';
    *out = buf;
    *out_len = len;
    return 0;
}

/* One-shot: connect, send command, read whole reply, close. */
static int rpc_call(const cgminer_ctx_t *c, const char *cmd, char **out)
{
    int fd = connect_with_timeout(c->host, c->port,
                                  c->connect_timeout_ms, c->read_timeout_ms);
    if (fd < 0) return -1;

    if (send_all(fd, cmd, strlen(cmd)) < 0) { close(fd); return -1; }
    /* Some cgminer variants need shutdown(write) to flush. Harmless if not. */
    shutdown(fd, SHUT_WR);

    size_t n = 0;
    int rc = read_all(fd, out, &n);
    close(fd);
    return rc;
}

/* --- vtable implementation ----------------------------------------------- */

static protocol_handle_t cgminer_create(const protocol_config_t *cfg) {
    if (cfg == NULL) return NULL;
    cgminer_ctx_t *c = (cgminer_ctx_t *) calloc(1, sizeof(*c));
    if (c == NULL) return NULL;
    strncpy(c->host, cfg->host ? cfg->host : "127.0.0.1", sizeof(c->host) - 1);
    c->port = cfg->port > 0 ? cfg->port : 4028;
    c->connect_timeout_ms = cfg->connect_timeout_ms > 0
                          ? cfg->connect_timeout_ms : 2000;
    c->read_timeout_ms = cfg->read_timeout_ms > 0
                       ? cfg->read_timeout_ms : 3000;
    return c;
}

static void cgminer_destroy(protocol_handle_t h) {
    free(h);
}

static int cgminer_fetch(protocol_handle_t h, snapshot_t *out) {
    if (h == NULL || out == NULL) return -1;
    cgminer_ctx_t *c = (cgminer_ctx_t *) h;

    static const char *CMD_SUMMARY = "{\"command\":\"summary\"}";
    static const char *CMD_STATS   = "{\"command\":\"stats\"}";
    static const char *CMD_POOLS   = "{\"command\":\"pools\"}";
    static const char *CMD_DEVS    = "{\"command\":\"devs\"}";

    char *resp = NULL;

    /* summary is mandatory — without it we don't have meaningful data */
    if (rpc_call(c, CMD_SUMMARY, &resp) < 0) return -1;
    int rc = cgminer_parse_summary(out, resp);
    free(resp); resp = NULL;
    if (rc < 0) return -1;

    /* the rest are best-effort */
    if (rpc_call(c, CMD_STATS, &resp) == 0) {
        cgminer_parse_stats(out, resp);
        free(resp); resp = NULL;
    }
    if (rpc_call(c, CMD_POOLS, &resp) == 0) {
        cgminer_parse_pools(out, resp);
        free(resp); resp = NULL;
    }
    if (rpc_call(c, CMD_DEVS, &resp) == 0) {
        cgminer_parse_devs(out, resp);
        free(resp); resp = NULL;
    }

    out->has_data = true;
    return 0;
}

const firmware_protocol_t cgminer_protocol = {
    .name    = "cgminer",
    .create  = cgminer_create,
    .fetch   = cgminer_fetch,
    .destroy = cgminer_destroy,
};
