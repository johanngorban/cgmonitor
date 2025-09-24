#pragma once

#include "miner_info.h"

int parse_json_summary(miner_info *miner, const char *summary_json);

int parse_json_stats(miner_info *miner, const char *stats_json);