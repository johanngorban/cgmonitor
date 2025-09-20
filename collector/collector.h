#pragma once

#include <unistd.h>

const char CGMINER_ADDRESS[] = "127.0.0.1";
const char CGMINER_PORT[]    = "4228";

const char GET_MINERS_INFO[] = "{\"command\": \"devs\"}";

const int CHUNK_SIZE = 4096;

/// @brief infinity collecting loop
void collect_loop();

/// @brief connect to cgminer
/// @return status code, 0 on success
int cgminer_connect(int *sockfd);

/// @brief send request to cgminer
/// @return status code, 0 on success
int send_request(int sockfd, const char *request);

/// @brief get response from cgminer
/// @return bytes accepted from cgminer, -1 on error
ssize_t get_response(int sockfd, char **response);