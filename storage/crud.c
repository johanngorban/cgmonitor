#include "crud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <stdint.h>
#include <sys/types.h>

static sqlite3 *DATABASE = NULL;

const char SQL_CURRENT_TABLE[] = 
    "CREATE TABLE IF NOT EXISTS asics_current ("
    "id INTEGER PRIMARY KEY,"
    "name TEXT,"
    "mhs_av REAL,"
    "temperature REAL,"
    "utility REAL,"
    "accepted INTEGER,"
    "rejected INTEGER,"
    "hw_errors INTEGER);";

const char SQL_HISTORY_TABLE[] =
    "CREATE TABLE IF NOT EXISTS asics_history ("
    "record_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "id INTEGER,"
    "name TEXT,"
    "mhs_av REAL,"
    "temperature REAL,"
    "utility REAL,"
    "accepted INTEGER,"
    "rejected INTEGER,"
    "hw_errors INTEGER,"
    "ts INTEGER);";


int storage_init(const char *db_path) {
    if (sqlite3_open(db_path, &DATABASE) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(DATABASE));
        return -1;
    }

    char *err = NULL;
    if (sqlite3_exec(DATABASE, SQL_CURRENT_TABLE, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    if (sqlite3_exec(DATABASE, SQL_HISTORY_TABLE, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    return 0;
}

void storage_free() {
    if (DATABASE != NULL) {
        sqlite3_close(DATABASE);
        DATABASE = NULL;
    }
}

int storage_add_record(const asic_info *asic, time_t record_time) {
    if (DATABASE == NULL || asic == NULL) {
        return -1;
    }

    const char *sql =
        "INSERT INTO asics_history (id, name, mhs_av, temperature, utility, accepted, rejected, hw_errors, ts) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(DATABASE, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "SQL prepare failed: %s\n", sqlite3_errmsg(DATABASE));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, asic->id);
    sqlite3_bind_text(stmt, 2, asic->name, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, asic->mhs_av);
    sqlite3_bind_double(stmt, 4, asic->temperature);
    sqlite3_bind_double(stmt, 5, asic->utility);
    sqlite3_bind_int(stmt, 6, asic->accepted);
    sqlite3_bind_int(stmt, 7, asic->rejected);
    sqlite3_bind_int(stmt, 8, asic->hw_errors);
    sqlite3_bind_int64(stmt, 9, (sqlite3_int64)record_time);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "SQL step failed: %s\n", sqlite3_errmsg(DATABASE));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int storage_get_current(int asic_id, asic_info *asic) {
    
}