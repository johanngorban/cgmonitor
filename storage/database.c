#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <stdint.h>
#include <sys/types.h>

static sqlite3 *DATABASE = NULL;

const char SQL_CURRENT[] = 
    "CREATE TABLE IF NOT EXISTS asics_current ("
    "id INTEGER PRIMARY KEY,"
    "name TEXT,"
    "mhs_av REAL,"
    "temperature REAL,"
    "utility REAL,"
    "accepted INTEGER,"
    "rejected INTEGER,"
    "hw_errors INTEGER);";

const char SQL_HISTORY[] =
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


int database_init(const char *db_path) {
    if (sqlite3_open(db_path, &DATABASE) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(DATABASE));
        return -1;
    }

    char *err = NULL;
    if (sqlite3_exec(DATABASE, SQL_CURRENT, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    if (sqlite3_exec(DATABASE, SQL_HISTORY, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    return 0;
}

void database_free() {
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

int storage_get_history(asic_record **records, int *count) {
    if (DATABASE == NULL || records == NULL || count == NULL) {
        return -1;
    }

    const char *sql =
        "SELECT id, name, mhs_av, temperature, utility, accepted, rejected, hw_errors, ts "
        "FROM asics_history "
        "ORDER BY ts ASC;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(DATABASE, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "SQL prepare failed: %s\n", sqlite3_errmsg(DATABASE));
        return -1;
    }

    int capacity = 16;
    *records = malloc(sizeof(asic_record) * capacity);
    if (*records == NULL) {
        sqlite3_finalize(stmt);
        return -1;
    }
    *count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            *records = realloc(*records, sizeof(asic_record) * capacity);
            if (*records == NULL) {
                sqlite3_finalize(stmt);
                return -1;
            }
        }

        asic_record *rec = &(*records)[*count];

        rec->asic.id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (name != NULL) {
            strncpy(rec->asic.name, name, sizeof(rec->asic.name) - 1);
            rec->asic.name[sizeof(rec->asic.name) - 1] = '\0';
        } else {
            rec->asic.name[0] = '\0';
        }
        rec->asic.mhs_av = sqlite3_column_double(stmt, 2);
        rec->asic.temperature = sqlite3_column_double(stmt, 3);
        rec->asic.utility = sqlite3_column_double(stmt, 4);
        rec->asic.accepted = sqlite3_column_int(stmt, 5);
        rec->asic.rejected = sqlite3_column_int(stmt, 6);
        rec->asic.hw_errors = sqlite3_column_int(stmt, 7);
        rec->timestamp = (time_t) sqlite3_column_int64(stmt, 8);

        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}
