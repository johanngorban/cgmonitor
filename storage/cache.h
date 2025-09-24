#pragma once

#include "miner_info.h"

int storage_save_miner_info(const miner_info *miner);

int storage_get_miner_info(miner_info *miner);