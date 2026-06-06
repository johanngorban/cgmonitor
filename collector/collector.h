/**
 * collector.h — generic poller that drives any registered protocol.
 *
 * Lifecycle:
 *   collector_init(cfg)   — pick protocol, set up handle
 *   pthread_create(collect_loop, NULL)
 *   collector_stop()      — signal the loop to exit
 *   pthread_join
 *   collector_shutdown()  — destroy the protocol handle
 */
#pragma once

#include "config.h"

int  collector_init(const config_t *cfg);
void collector_shutdown(void);

/* Used as pthread start routine. arg is ignored. */
void *collector_loop(void *arg);

/* Asynchronously request the loop to exit at the next iteration. */
void collector_stop(void);
