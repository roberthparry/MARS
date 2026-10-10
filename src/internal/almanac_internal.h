/**
 * @file almanac_internal.h
 * @brief Controlled cross-module access to native solar-totality searches.
 *
 * Allows the Lab's built-in almanac calculation module to use the private
 * geographical totality search results without including an owning module's
 * implementation header directly. Consumers must explicitly define
 * MARS_ALMANAC_INTERNAL_ACCESS. Ordinary applications should use almanac.h;
 * this interface is not installed and is not a public compatibility contract.
 */
#ifndef MARS_SHARED_ALMANAC_INTERNAL_H
#define MARS_SHARED_ALMANAC_INTERNAL_H

#include "almanac/almanac_internal.h"

#endif
