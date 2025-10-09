#pragma once

#include <cjson/cJSON.h>
#include <time.h>

#define MINER_NAME_LENGTH   256
#define MINER_MODEL_LENGTH  256

typedef struct {
    double hashrate;    // Hashrate in MHs
    double temp;        // Temperature in grad Celsium
    double power;       // Power in watts
    double voltage;     // Voltage in volts
    int    uptime;      // Uptime in secods
} miner_info;

typedef struct {
    miner_info data;
    time_t time;
} miner_record;

int set_miner_name(const char *name);

int set_miner_model(const char *model);

int get_miner_name(char *buf, size_t buf_size);

int get_miner_model(char *buf, size_t buf_size);