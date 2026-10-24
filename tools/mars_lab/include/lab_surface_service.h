/**
 * @file lab_surface_service.h
 * @brief Single-owner native shoreline service for the prefork HTTP workers.
 *
 * Anonymous socket pairs carry bounded Protobuf messages to one lazy geometry
 * owner. No database is opened during startup and no geometry is copied into
 * the HTTP processes. The launcher supervises the returned child process.
 */
#ifndef MARS_LAB_SURFACE_SERVICE_H
#define MARS_LAB_SURFACE_SERVICE_H

#include <sys/types.h>

#include "json-t.h"

/** @brief Fork one geometry owner for one to eight HTTP workers; return -1 on failure. */
pid_t lab_surface_service_start(unsigned workers);

/** @brief Retain only this HTTP worker's channel; an out-of-range index closes all channels. */
void lab_surface_service_select(unsigned worker);

/** @brief Return an owned shoreline response, using direct execution outside the supervised server. */
json_t *lab_surface_service_request(const json_t *payload, unsigned *status);

#endif
