#include "miner_info.h"

#include <string.h>
#include <stdlib.h>
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

char *get_miner_name() {
    char *copy = (char *) malloc(strlen(miner_name) + 1);
    if (copy != NULL) {
        strcpy(copy, miner_name);
    }

    return copy;
}

char *get_miner_model() {
    char *copy = (char *) malloc(strlen(miner_model) + 1);
    if (copy != NULL) {
        strcpy(copy, miner_model);
    }

    return copy;
}