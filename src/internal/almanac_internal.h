/**
 * @file almanac_internal.h
 * @brief Controlled cross-module access to native solar-totality searches.
 *
 * Allows the Lab's built-in almanac calculation module to use the private
 * geographical totality search results without including an owning module's
 * implementation header directly. Consumers must explicitly define
 * MARS_SHARED_ALMANAC_INTERNAL_ACCESS. Ordinary applications should use almanac-t.h;
 * this interface is not installed and is not a public compatibility contract.
 */
#ifndef MARS_SHARED_ALMANAC_INTERNAL_H
#define MARS_SHARED_ALMANAC_INTERNAL_H

#if !defined(MARS_SHARED_ALMANAC_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_ALMANAC_INTERNAL_ACCESS."
#endif

#define MARS_ALMANAC_INTERNAL_ACCESS
#include "almanac/almanac_internal.h"

#endif
