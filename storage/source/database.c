#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <stdint.h>
#include <sys/types.h>

static sqlite3 *sql_db = NULL;

int db_init(const char *db_path) {
    int status = sqlite3_open(db_path, &sql_db);
    if (status != SQLITE_OK) {
        fprintf(stderr, "Cannot open sql_db: %s\n", sqlite3_errmsg(sql_db));
        return -1;
    }

    char *err = NULL;
    status = sqlite3_exec(sql_db, SQL_CURRENT, 0, 0, &err);
    if (status != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    status = sqlite3_exec(sql_db, SQL_HISTORY, 0, 0, &err);
    if (status != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    return 0;
}

void db_free() {
    if (sql_db != NULL) {
        sqlite3_close(sql_db);
        sql_db = NULL;
    }
}

int db_insert_miner_info(const miner_info *m, time_t t) {
    if (m == NULL) {
        return -1;
    }

    return 0;
}

int db_get_all_miner_info(miner_record *m, int max_count) {
    if (m == NULL || max_count < 1) {
        return -1;
    }

    return 0;
}

int db_get_last_miner_info(miner_record *m) {
    if (m == NULL) {
        return -1;
    }

    return 0;
}