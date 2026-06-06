#pragma once

#include "config.h"

int  storage_init(const config_t *cfg);
void storage_shutdown(void);

void *storage_loop(void *arg);
void storage_stop(void);
