/**
 * @file lab_globe_shore_internal.h
 * @brief Controlled native admission boundary between shoreline transport and projection.
 *
 * Only the shoreline loader and its HTTP adapter borrow this interface. Unlike the public host-handle ABI,
 * it consumes bytes already resident in WASM memory and never copies them through JavaScript.
 */
#if !defined(MARS_LAB_GLOBE_SHORE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_GLOBE_SHORE_INTERNAL_ACCESS."
#endif
#ifndef LAB_GLOBE_SHORE_INTERNAL_H
#define LAB_GLOBE_SHORE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

/** @brief Validate and atomically admit borrowed MSH1 bytes with native bounds; all inputs expire on return. */
bool lab_globe_shore_admit(const unsigned char *bytes, size_t length, const double bounds[4]);

#endif
