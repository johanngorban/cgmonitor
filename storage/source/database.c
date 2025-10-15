#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <stdint.h>
#include <sys/types.h>
#include <clog/logging.h>

static sqlite3 *sql_db = NULL;

const char sql_create_device_table[] =      \
    "CREATE TABLE device("                  \
        "id             INTEGER PRIMARY KEY AUTOINCREMENT," \
        "hashrate       REAL NOT NULL,"         \
        "temperature    REAL NOT NULL,"         \
        "power          REAL NOT NULL,"         \
        "voltage        REAL NOT NULL,"         \
        "time           INTEGER NOT NULL"       \
    ");";

int db_init(const char *db_path) {
    int status = sqlite3_open(db_path, &sql_db);
    if (status != SQLITE_OK) {
        log_debug("Cannot open sql_db: %s\n", sqlite3_errmsg(sql_db));
        return -1;
    }

    char *err = NULL;
    status = sqlite3_exec(sql_db, sql_create_device_table, NULL, NULL, &err);
    if (status != SQLITE_OK) {
        log_debug("Cannot create table for database");
        sqlite3_free(err);
    }

    return 0;
}

void db_free() {
    if (sql_db != NULL) {
        log_debug("Cannot close database");
        sqlite3_close(sql_db);
        sql_db = NULL;
    }
}

int db_insert_miner_info(const miner_info *m, time_t t) {
    if (m == NULL) {
        return -1;
    }

    const char sql_insert_miner_info[] = \
        "INSERT INTO device(hashrate, temperature, power, voltage, time) " \
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int status = sqlite3_prepare_v2(sql_db, sql_insert_miner_info, -1, &stmt, NULL);
    if (status != SQLITE_OK) {
        log_debug("Internal error while preparing insert SQL-query");
        return -1;
    }

    sqlite3_bind_double(stmt, 1, m->hashrate);
    sqlite3_bind_double(stmt, 2, m->temp);
    sqlite3_bind_double(stmt, 3, m->power);
    sqlite3_bind_double(stmt, 4, m->voltage);
    sqlite3_bind_int(stmt, 5, (int) t);

    status = sqlite3_step(stmt);
    if (status != SQLITE_DONE) {
        log_debug("Error occurred while binding data in inserting");
        return -1;
    }

    sqlite3_finalize(stmt);

    return 0;
}

static int allocate_miner_records(miner_record **m, size_t count) {
    if (*m != NULL) {
        free(*m);
    }
    *m = (miner_record *) malloc(sizeof(miner_record) * count);
    if (*m == NULL) {
        log_debug("Miner record allocation error");
        return -1;
    }

    return 0;
}

int db_get_all_miner_info(miner_record **m, int max_count) {
    if (m == NULL || max_count < 1) {
        return -1;
    }

    const char sql_select_records[] = \
    "SELECT * FROM device ORDER BY time DESC LIMIT ?;";

    sqlite3_stmt *stmt;
    int status = sqlite3_prepare_v2(sql_db, sql_select_records, -1, &stmt, NULL);
    if (status != SQLITE_OK) {
        log_debug("Internal error while preparing select SQL-query");
        return -1;
    }
    
    status = allocate_miner_records(m, max_count);
    if (status != 0) {
        return -1;
    }

    sqlite3_bind_int(stmt, 1, max_count);
    int extracted = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && extracted < max_count) {
        (*m)[extracted].data.hashrate   = sqlite3_column_double(stmt, 1);
        (*m)[extracted].data.temp       = sqlite3_column_double(stmt, 2);
        (*m)[extracted].data.power      = sqlite3_column_double(stmt, 3);
        (*m)[extracted].data.voltage    = sqlite3_column_double(stmt, 4);
        (*m)[extracted].time            = sqlite3_column_int(stmt, 5);
        extracted++;

        log_debug("Extracted miner record of time: %lu", (*m)[extracted].time);
    }
    sqlite3_finalize(stmt);

    log_debug("Extracted %d records", extracted);

    return extracted;
}

int db_get_last_miner_info(miner_record *m) {
    if (m == NULL) {
        return -1;
    }

    const char sql_select_last[] = \
        "SELECT * FROM device ORDER BY time DESC LIMIT 1;";

    sqlite3_stmt *stmt;
    int status = sqlite3_prepare_v2(sql_db, sql_select_last, -1, &stmt, NULL);
    if (status != SQLITE_OK) {
        log_debug("Internal error while preparing select SQL-query");
        return -1;
    }

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        m->data.hashrate = sqlite3_column_double(stmt, 1);
        m->data.temp     = sqlite3_column_double(stmt, 2);
        m->data.power    = sqlite3_column_double(stmt, 3);
        m->data.voltage  = sqlite3_column_double(stmt, 4);
        m->time          = sqlite3_column_int(stmt, 5);
        found = 1;
    }

    sqlite3_finalize(stmt);
    return found;
}

int db_get_new_miner_info(miner_record **m, time_t newer_than) {
    if (m == NULL) {
        return -1;
    }

    const char sql_select_records[] = \
    "SELECT * FROM device WHERE time >= ? ORDER BY time";

    sqlite3_stmt *stmt;
    int status = sqlite3_prepare_v2(sql_db, sql_select_records, -1, &stmt, NULL);
    if (status != SQLITE_OK) {
        log_debug("Internal error while preparing select SQL-query");
        return -1;
    }
    
    size_t capacity = 128;
    status = allocate_miner_records(m, capacity); // default value
    if (status != 0) {
        free(*m);
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64) newer_than);

    size_t extracted = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (extracted >= capacity) {
            capacity *= (capacity >= 4096) ? 1.2 : 2;
            miner_record *tmp = realloc(*m, sizeof(miner_record) * capacity);
            if (tmp == NULL) {
                free(*m);
                sqlite3_finalize(stmt);
                return -1;
            }
            *m = tmp;
        }

        (*m)[extracted].data.hashrate   = sqlite3_column_double(stmt, 1);
        (*m)[extracted].data.temp       = sqlite3_column_double(stmt, 2);
        (*m)[extracted].data.power      = sqlite3_column_double(stmt, 3);
        (*m)[extracted].data.voltage    = sqlite3_column_double(stmt, 4);
        (*m)[extracted].time            = sqlite3_column_int64(stmt, 5);

        log_debug("Extracted miner record of time: %lu", (*m)[extracted].time);

        extracted++;
    }
    sqlite3_finalize(stmt);

    log_debug("Extracted %d records", extracted);

    return (int) extracted;
}