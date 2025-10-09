#include "collector.h"
#include "storage.h"
#include "asic_info.h"
#include "parsers.h"
#include "logging.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>

const char  CGMINER_ADDRESS[] = "127.0.0.1";
const char  CGMINER_PORT[]    = "4028";

// Requests
const char  CGMINER_GET_DEVS[]      = "{\"command\": \"devs\"}";
const char  CGMINER_GET_SUMMARY[]   = "{\"command\": \"summary\"}";
const char  CGMINER_GET_STATS[]     = "{\"command\": \"stats\"}";

const int   CHUNK_SIZE = 4096;
const int   MAX_CONNECTION_ATTEMPTS = 5;
const int   COLLECTING_PERIOD_SEC = 5;
const int   CONNECTION_FAILURE_SLEEP_SEC = 20;

static int failed_connection_attempts = 0;

void *collect_loop(void *arg) {
    (void) arg;

    static int cgminer_socket = -1;

    while (1) {
        if (failed_connection_attempts >= MAX_CONNECTION_ATTEMPTS) {
            log_error("Max connection attempts reached");
            failed_connection_attempts = 0;
            sleep(CONNECTION_FAILURE_SLEEP_SEC);
            continue;
        }
        else {
            log_debug("Sleeping before next collection cycle");
            sleep(COLLECTING_PERIOD_SEC);
        }

        log_debug("Attempting to connect to cgminer");
        int status = cgminer_connect(&cgminer_socket);
        if (status < 0) {
            failed_connection_attempts++;
            log_warning("Failed to connect to cgminer");
            continue;
        }
        log_info("Successfully connected to cgminer");

        status = send_request(cgminer_socket, CGMINER_GET_STATS);
        if (status < 0) {
            log_error("Failed to send request to cgminer");
            close(cgminer_socket);
            continue;
        }
        log_debug("Request sent to cgminer");

        char *response;
        ssize_t bytes_received = get_response(cgminer_socket, &response);
        if (bytes_received < 1) {
            log_warning("No response received from cgminer");
            close(cgminer_socket);
            continue;
        }
        log_debug("Received response from cgminer");

        miner_info miner;
        status = parse_json_stats(&miner, response);
        log_info("Summary response processing");
        free(response);

        if (status < 0) {
            log_error("Failed to parse summary JSON");
            close(cgminer_socket);
            continue;
        }

        status = storage_save_miner_info(&miner, time(NULL));
        if (status < 0)  {
            log_error("Failed to save miner info");
        }

        close(cgminer_socket);
        log_debug("Closed cgminer socket");
    }

    return NULL;
}

int cgminer_connect(int *sockfd) {
    if (sockfd == NULL) {
        perror("socket ptr is NULL");
        return -1;
    }

    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(CGMINER_PORT));
    inet_pton(AF_INET, CGMINER_ADDRESS, &server.sin_addr);

    int status = connect(*sockfd, (struct sockaddr *) &server, sizeof(server));
    if (status < 0) {
        perror("connect");
        return -1;
    }

    return status;
}

int send_request(int sockfd, const char *request) {
    if (request == NULL) {
        perror("request ptr is NULL");
        return -1;
    }

    size_t request_size = strlen(request);

    ssize_t bytes_sent = send(sockfd, request, request_size, 0);
    if (bytes_sent != (ssize_t) request_size) {
        perror("sent incorrect number of bytes");
        return -1;
    }

    return 0;
}

ssize_t get_response(int sockfd, char **response) {
    if (response == NULL) {
        perror("response ptr is NULL");
        return -1;
    }

    size_t buf_size = CHUNK_SIZE;

    char *buf = (char *) malloc(buf_size);
    if (buf == NULL) {
        perror("buffer allocation");
        return -1;
    }

    ssize_t total_bytes = 0;

    while (1) {
        ssize_t bytes_received = recv(sockfd, buf + total_bytes, buf_size - total_bytes, 0);
        if (bytes_received < 0) {
            perror("recv");
            free(buf);
            return -1;
        }

        if (bytes_received == 0) {
            break;
        }

        total_bytes += bytes_received;

        if (total_bytes == (ssize_t) buf_size) {
            buf_size += CHUNK_SIZE;
            char *tmp = (char *) realloc(buf, buf_size);
            if (tmp == NULL) {
                perror("realloc");
                free(buf);
                return -1;
            }
            buf = tmp;
        }
    } 

    if (total_bytes == (ssize_t) buf_size) {
        char *tmp = (char *) realloc(buf, buf_size + 1);
        if (tmp == NULL) {
            perror("realloc while adding null terminator");
            free(buf);
            return -1;
        }
        buf = tmp;
    }
    buf[total_bytes] = '\0';

    *response = buf;
    return total_bytes;
}