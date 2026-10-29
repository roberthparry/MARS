/**
 * @file almanac_internal.h
 * @brief Controlled cross-module solar-totality search interface.
 *
 * Declares totality-location results and geographical search operations shared by almanac and Lab.
 * Define MARS_SHARED_ALMANAC_INTERNAL_ACCESS immediately before inclusion. Inputs are borrowed unless
 * explicitly stated otherwise. Screening grids are caller-owned and released with almanac_totality_screen_free;
 * result structures and output parameters remain caller-owned. Applications use almanac-t.h; this guarded
 * interface is not installed and does not expose the almanac engine's private representation.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef MARS_SHARED_ALMANAC_INTERNAL_H
#define MARS_SHARED_ALMANAC_INTERNAL_H

#if !defined(MARS_SHARED_ALMANAC_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_ALMANAC_INTERNAL_ACCESS."
#endif

#include "almanac-t.h"

/** @brief Opaque, event-local sampled Sun/Moon positions used only to shortlist catalogue locations. */
typedef struct _almanac_totality_screen_t almanac_totality_screen_t;

/**
 * @brief Build an owned two-minute screening grid over the local refinement's twelve-hour window.
 * @param almanac Borrowed configured catalogue used to calculate Sun and Moon positions.
 * @param eclipse Borrowed event defining the sampled time window.
 * @return Owned grid released with almanac_totality_screen_free, or NULL on failure or cancellation.
 * @note The grid copies samples and does not retain either input; it only shortlists possible candidates.
 */
almanac_totality_screen_t *almanac_totality_screen_new(almanac_t *almanac, const almanac_solar_eclipse_t *eclipse);

/**
 * @brief Release an owned screening grid; NULL is accepted.
 * @param screen Owned grid to release; NULL is accepted.
 */
void almanac_totality_screen_free(almanac_totality_screen_t *screen);

/**
 * @brief Shortlist an observer location using a generous angular margin and interpolated separation.
 * @param almanac Borrowed catalogue used to validate and locate the observer.
 * @param screen Borrowed grid for the event being searched.
 * @param observer Borrowed candidate location.
 * @param possible Required caller-owned output; true requests exact totality refinement and land checks.
 * @return True when screening completes, including a rejected candidate; false on failure or cancellation.
 * @note A possible candidate is not a verified totality location.
 */
bool almanac_totality_screen_candidate(almanac_t *almanac, const almanac_totality_screen_t *screen,
                                       const almanac_observer_t *observer, bool *possible);

/** @brief Geographical search result for a solar eclipse's totality. */
typedef struct _almanac_solar_totality_location_t {
    bool found;                           /**< Whether a location was found. */
    almanac_event_time_t greatest_eclipse; /**< Time of greatest eclipse at the location. */
    double latitude_degrees;              /**< Latitude in degrees. */
    double longitude_degrees;             /**< Longitude in degrees. */
    double distance_km;                   /**< Distance from the search origin in kilometres. */
    double magnitude;                     /**< Eclipse magnitude at the location. */
    double totality_percent;              /**< Solar disc obscuration in percent. */
} almanac_solar_totality_location_t;

/**
 * @brief Refine a totality search from a supplied geographical seed.
 * @param almanac Borrowed configured catalogue used for the calculation; no ownership transfers.
 * @param origin Borrowed observer location from which distances are measured.
 * @param seed Borrowed observer location used to initialise local totality refinement.
 * @param eclipse Borrowed eclipse event defining the time window to inspect.
 * @param tolerance_degrees Angular refinement tolerance in degrees.
 * @param out Required caller-owned result structure; no dynamic output ownership transfers.
 * @return True when a totality candidate is successfully refined; false when unavailable, invalid, cancelled
 * or unsuccessful.
 */
bool almanac_solar_eclipse_totality_from_seed(almanac_t *almanac, const almanac_observer_t *origin,
                                              const almanac_observer_t *seed, const almanac_solar_eclipse_t *eclipse,
                                              double tolerance_degrees, almanac_solar_totality_location_t *out);

#endif /* MARS_SHARED_ALMANAC_INTERNAL_H */
