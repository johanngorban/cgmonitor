#include "cache.h"

#include <pthread.h>
#include <string.h>

static pthread_rwlock_t lock;
static snapshot_t       current;
static int              initialized = 0;

int cache_init(void) {
    if (initialized) return 0;
    if (pthread_rwlock_init(&lock, NULL) != 0) return -1;
    snapshot_init(&current);
    initialized = 1;
    return 0;
}

void cache_destroy(void) {
    if (!initialized) return;
    snapshot_free(&current);
    pthread_rwlock_destroy(&lock);
    initialized = 0;
}

int cache_set(const snapshot_t *s) {
    if (!initialized || s == NULL) return -1;
    pthread_rwlock_wrlock(&lock);
    int rc = snapshot_copy(&current, s);
    pthread_rwlock_unlock(&lock);
    return rc;
}

int cache_get(snapshot_t *out) {
    if (!initialized || out == NULL) return -1;
    pthread_rwlock_rdlock(&lock);
    int rc = snapshot_copy(out, &current);
    pthread_rwlock_unlock(&lock);
    return rc;
}

bool cache_has_data(void) {
    if (!initialized) return false;
    pthread_rwlock_rdlock(&lock);
    bool has = current.has_data;
    pthread_rwlock_unlock(&lock);
    return has;
}
