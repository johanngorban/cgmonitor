#pragma once

#include "snapshot.h"

/* Render a full snapshot to a freshly-allocated JSON string.
 * Caller free()s. Returns NULL on OOM. */
char *snapshot_to_json(const snapshot_t *s);

/* Render the current config (whatever the frontend needs to know — thresholds,
 * fw protocol name, server period suggestion, etc.). */
struct config_t;
char *config_to_json_subset(const struct config_t *cfg);
