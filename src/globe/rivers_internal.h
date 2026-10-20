/**
 * @file rivers_internal.h
 * @brief Private bounded river coordinate codec shared by the reader and importer.
 *
 * Only river implementation units use this header. Applications use globe-t.h and never depend on the on-disk
 * endian representation or chunk size. Coordinates are integer microdegrees, decoded without alignment assumptions.
 */
#ifndef MARS_RIVERS_INTERNAL_H
#define MARS_RIVERS_INTERNAL_H

#include <stdint.h>

#if !defined(MARS_RIVERS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_RIVERS_INTERNAL_ACCESS."
#endif

/** @brief Decode one signed big-endian microdegree coordinate. */
int32_t rivers_coordinate(const unsigned char *bytes);

#endif
