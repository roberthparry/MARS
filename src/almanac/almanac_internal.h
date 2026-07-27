#ifndef MARS_ALMANAC_INTERNAL_H
#define MARS_ALMANAC_INTERNAL_H

#if !defined(MARS_ALMANAC_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || \
     (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "almanac_internal.h is private to the almanac module; include almanac.h instead."
#endif

#include "almanac.h"

typedef struct _almanac_solar_totality_location_t {
    bool found;
    almanac_event_time_t greatest_eclipse;
    double latitude_degrees;
    double longitude_degrees;
    double distance_km;
    double magnitude;
    double totality_percent;
} almanac_solar_totality_location_t;

bool almanac_nearest_solar_totality(almanac_t *almanac,
                                    const almanac_observer_t *observer,
                                    const almanac_solar_eclipse_t *eclipse,
                                    almanac_solar_totality_location_t *out);

bool almanac_solar_eclipse_totality_at(almanac_t *almanac,
                                       const almanac_observer_t *observer,
                                       const almanac_solar_eclipse_t *eclipse,
                                       almanac_solar_totality_location_t *out);

bool almanac_solar_eclipse_totality_from_seed(almanac_t *almanac,
                                              const almanac_observer_t *origin,
                                              const almanac_observer_t *seed,
                                              const almanac_solar_eclipse_t *eclipse,
                                              double tolerance_degrees,
                                              almanac_solar_totality_location_t *out);

bool almanac_solar_eclipse_totality_seed_score(almanac_t *almanac,
                                               const almanac_observer_t *seed,
                                               const almanac_solar_eclipse_t *eclipse,
                                               double *out_score_degrees);

bool almanac_nearest_solar_totality_land(almanac_t *almanac,
                                         const almanac_observer_t *observer,
                                         const almanac_solar_eclipse_t *eclipse,
                                         almanac_solar_totality_location_t *out);

#endif /* MARS_ALMANAC_INTERNAL_H */
