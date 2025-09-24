#include "miner_info.h"

#include <string.h>
#include <stdio.h>

static char miner_name[MINER_NAME_LENGTH];
static char miner_model[MINER_MODEL_LENGTH];

static int set_str(char *restrict dest, const char *restrict src, int length) {
    if (dest == NULL || src == NULL) {
        return -1;
    }

    if (strlen(src) >= length) {
        return -1;
    }

    snprintf(dest, length, "%s", src);

    return 0;
}

int set_miner_name(const char *name) {
    return set_str(miner_name, name, MINER_NAME_LENGTH);
}

int set_miner_model(const char *model) {
    return set_str(miner_model, model, MINER_MODEL_LENGTH);
}

int get_miner_name(char *buf, size_t buf_size) {
    if (!buf) return -1;

    strncpy(buf, miner_name, buf_size - 1);
    buf[buf_size - 1] = '\0';

    return 0;
}

int get_miner_model(char *buf, size_t buf_size) {
    if (!buf) return -1;

    strncpy(buf, miner_model, buf_size - 1);
    buf[buf_size - 1] = '\0';

    return 0;
}