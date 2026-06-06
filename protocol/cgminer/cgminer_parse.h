#pragma once

#include "snapshot.h"

int cgminer_parse_summary(snapshot_t *s, const char *json);
int cgminer_parse_stats  (snapshot_t *s, const char *json);
int cgminer_parse_pools  (snapshot_t *s, const char *json);
int cgminer_parse_devs   (snapshot_t *s, const char *json);
