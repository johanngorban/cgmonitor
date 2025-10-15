#pragma once

#include <time.h>
#include "miner_info.h"

/* Basic */

// Init db
int db_init(const char *db_path);

// Close db
void db_close();


/* CRUD */

// Insert  miner info into database
// Return 0 on sucess, -1 otherwise
int db_insert_miner_info(const miner_info *m, time_t t);

// Get all miner info in database
// Return the count of miner info extracted (<= max_count), -1 on errors
int db_get_all_miner_info(miner_record **m, int max_count);

// Get only the last miner info in database, save it into m
// Return 0 on success, -1 on errors
int db_get_last_miner_info(miner_record *m);

//
//
int db_get_new_miner_info(miner_record **m, time_t newer_than);