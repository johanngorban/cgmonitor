#pragma once

#include <unistd.h>
#include "asic_info.h"

/// @brief infinity collecting loop
void *collect_loop(void *arg);

/// @brief connect to cgminer
/// @return status code, 0 on success
int cgminer_connect(int *sockfd);

/// @brief send request to cgminer
/// @return status code, 0 on success
int send_request(int sockfd, const char *request);

/// @brief get response from cgminer
/// @return bytes accepted from cgminer, -1 on error
ssize_t get_response(int sockfd, char **response);

/// @brief extract asic array from json
/// @return count of asics in the array, -1 on error
size_t extract_asics_from_devs(asic_info **asics, const char *json);