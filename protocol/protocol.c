#include "protocol.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

#define MAX_PROTOCOLS 8

static const firmware_protocol_t *registry[MAX_PROTOCOLS];
static int registry_count = 0;

/* Forward declarations of built-in protocols. Each lives in its own dir. */
extern const firmware_protocol_t cgminer_protocol;

int protocol_register(const firmware_protocol_t *p) {
    if (p == NULL || p->name == NULL || p->fetch == NULL) return -1;
    for (int i = 0; i < registry_count; ++i) {
        if (strcasecmp(registry[i]->name, p->name) == 0) return -1;
    }
    if (registry_count >= MAX_PROTOCOLS) return -1;
    registry[registry_count++] = p;
    return 0;
}

const firmware_protocol_t *protocol_find(const char *name) {
    if (name == NULL) return NULL;
    for (int i = 0; i < registry_count; ++i) {
        if (strcasecmp(registry[i]->name, name) == 0) return registry[i];
    }
    return NULL;
}

int protocol_count(void) { return registry_count; }

const firmware_protocol_t *protocol_at(int i) {
    if (i < 0 || i >= registry_count) return NULL;
    return registry[i];
}

void protocol_register_builtins(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    protocol_register(&cgminer_protocol);
    /* Add new built-ins here as one-liners. */
}
