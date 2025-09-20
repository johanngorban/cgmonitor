#include "collector.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

static int cgminer_socket = -1;

void collect_loop() {
    int status = cgminer_connect(&cgminer_socket);
    if (status < 0) {
        return;
    }

    status = send_request(cgminer_socket, GET_MINERS_INFO);
    if (status < 0) {
        return;
    }

    char *response;
    ssize_t bytes_received = get_response(cgminer_socket, &response);
    if (bytes_received < 1) {
        return;
    }
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