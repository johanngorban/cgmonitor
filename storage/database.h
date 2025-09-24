#pragma once

#include <time.h>
#include "asic_info.h"

typedef struct {
    asic_info asic;
    time_t timestamp;
} asic_record;

int database_init(const char *db_path);

void database_free();

int storage_add_record(const asic_info *asic, time_t record_time);

int storage_get_history(asic_record **records, int *count);