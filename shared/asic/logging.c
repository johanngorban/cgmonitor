#include "logging.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static FILE *fp = NULL;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static log_level_t threshold = LOG_INFO;

static const char *level_str(log_level_t l) {
    switch (l) {
        case LOG_DEBUG: return "DBG";
        case LOG_INFO:  return "INF";
        case LOG_WARN:  return "WRN";
        case LOG_ERROR: return "ERR";
    }
    return "???";
}

int log_init(const char *path) {
    if (path == NULL || path[0] == '\0') {
        fp = NULL;
        return 0;
    }
    fp = fopen(path, "a");
    return (fp != NULL) ? 0 : -1;
}

void log_close(void) {
    pthread_mutex_lock(&mu);
    if (fp != NULL) { fclose(fp); fp = NULL; }
    pthread_mutex_unlock(&mu);
}

void log_set_level(log_level_t level) { threshold = level; }

void log_set_level_str(const char *s) {
    if (s == NULL) return;
    if      (strcasecmp(s, "debug") == 0) threshold = LOG_DEBUG;
    else if (strcasecmp(s, "info")  == 0) threshold = LOG_INFO;
    else if (strcasecmp(s, "warn")  == 0) threshold = LOG_WARN;
    else if (strcasecmp(s, "warning") == 0) threshold = LOG_WARN;
    else if (strcasecmp(s, "error") == 0) threshold = LOG_ERROR;
}

void log_log(log_level_t level, const char *fmt, ...) {
    if (level < threshold) return;

    char    tbuf[32];
    time_t  t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);

    pthread_mutex_lock(&mu);

    /* stderr */
    fprintf(stderr, "[%s] %s ", tbuf, level_str(level));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);

    /* file (if open) */
    if (fp != NULL) {
        fprintf(fp, "[%s] %s ", tbuf, level_str(level));
        va_start(ap, fmt);
        vfprintf(fp, fmt, ap);
        va_end(ap);
        fputc('\n', fp);
        fflush(fp);
    }

    pthread_mutex_unlock(&mu);
}
