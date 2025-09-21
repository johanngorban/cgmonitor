#include "collector.h"
#include "asic_info.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>

const char CGMINER_ADDRESS[] = "127.0.0.1";
const char CGMINER_PORT[]    = "4028";

const char GET_MINERS_INFO[] = "{\"command\": \"devs\"}";

const int CHUNK_SIZE = 4096;


void *collect_loop(void *arg) {
    (void) arg;

    static int cgminer_socket = -1;

    while (1) {
        sleep(5);

        printf("Collecting loop is running\n");

        int status = cgminer_connect(&cgminer_socket);
        if (status < 0) {
            return NULL;
        }

        status = send_request(cgminer_socket, GET_MINERS_INFO);
        if (status < 0) {
            return NULL;
        }

        char *response;
        ssize_t bytes_received = get_response(cgminer_socket, &response);
        if (bytes_received < 1) {
            return NULL;
        }

        asic_info *asics = NULL;
        int asic_counter = extract_asics_from_devs(&asics, response);
        printf("Got %d asics\n", asic_counter);
        for (int i = 0; i < asic_counter; i++) {
            printf("Asic %s: %.2fMHs\n", asics[i].name, asics[i].mhs_av);
        }
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

size_t extract_asics_from_devs(asic_info **asics, const char *json) {
    if (asics == NULL || json == NULL) {
        return -1;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        perror("cJSON_Parse");
        return -1;
    }

    cJSON *devs = cJSON_GetObjectItemCaseSensitive(root, "DEVS");
    if (devs == NULL || !cJSON_IsArray(devs)) {
        perror("DEVS is not an array");
        cJSON_Delete(root);
        return -1;
    }

    size_t count = cJSON_GetArraySize(devs);
    if (count == 0) {
        *asics = NULL;
        cJSON_Delete(root);
        return 0;
    }

    asic_info *array = (asic_info *) malloc(sizeof(asic_info) * count);
    if (array == NULL) {
        perror("malloc asic array");
        cJSON_Delete(root);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(devs, i);
        if (item == NULL || !cJSON_IsObject(item)) {
            perror("array item is not an object");
            continue;
        }

        if (asic_info_from_json(&array[i], item) < 0) {
            perror("failed to parse asic_info");
        }
    }

    *asics = array;
    cJSON_Delete(root);

    return count;
}