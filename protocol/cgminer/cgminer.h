/**
 * cgminer.h — cgminer JSON-API protocol implementation.
 *
 * Speaks the standard cgminer RPC over TCP, issuing 'summary', 'devs', 'stats',
 * and 'pools' commands and aggregating their results into a snapshot_t.
 *
 * Exposed publicly only as the `cgminer_protocol` symbol; everything else is
 * internal and lives in cgminer.c / cgminer_parse.c.
 */
#pragma once

#include "protocol.h"

extern const firmware_protocol_t cgminer_protocol;
