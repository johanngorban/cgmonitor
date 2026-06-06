/**
 * logging.h — minimal logger.
 *
 * If you have the clog submodule, you can drop this header and point
 * the CMake target to clog instead. The API is intentionally compatible.
 */
#pragma once

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
} log_level_t;

int  log_init(const char *path);
void log_close(void);
void log_set_level(log_level_t level);
void log_set_level_str(const char *s);

void log_log(log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define log_debug(...)   log_log(LOG_DEBUG,   __VA_ARGS__)
#define log_info(...)    log_log(LOG_INFO,    __VA_ARGS__)
#define log_warning(...) log_log(LOG_WARN,    __VA_ARGS__)
#define log_error(...)   log_log(LOG_ERROR,   __VA_ARGS__)
