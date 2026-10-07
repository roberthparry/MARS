/**
 * @file almanac_totality.c
 * @brief Nearby solar-totality location and land searches.
 *
 * Scores candidate observer positions, refines local eclipse maxima and searches for nearby totality on land. This
 * geographical search builds on solar-eclipse geometry rather than defining a separate ephemeris.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Nearby solar-totality location searches and land refinement. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "almanac_engine_internal.h"

#define MARS_ALMANAC_INTERNAL_ACCESS
#include "almanac_internal.h"

typedef struct almanac_land_box_t {
    double min_lat;
    double max_lat;
    double min_lon;
    double max_lon;
} almanac_land_box_t;

static const almanac_land_box_t ALMANAC_PROBABLE_LAND_BOXES[] = {
    {35.5, 44.5, -10.0, 4.5},     /* Iberia */
    {41.0, 51.5, -5.5, 10.5},     /* France */
    {49.0, 59.0, -11.0, 2.5},     /* Britain and Ireland */
    {63.0, 67.5, -25.0, -13.0},   /* Iceland */
    {59.0, 84.0, -75.0, -10.0},   /* Greenland */
    {-35.5, 37.5, -18.0, 52.0},   /* Africa */
    {5.0, 80.0, 25.0, 180.0},     /* Asia */
    {-47.0, 10.0, 95.0, 155.0},   /* Australia and nearby islands */
    {-56.0, 15.0, -82.0, -34.0},  /* South America */
    {15.0, 84.0, -170.0, -52.0},  /* North America */
    {-48.5, -33.0, 165.0, 180.0}, /* New Zealand */
    {-48.5, -33.0, -180.0, -175.0}};

typedef struct almanac_land_box_distance_t {
    size_t index;
    double distance_km;
} almanac_land_box_distance_t;

static double almanac_solar_totality_score_degrees(const almanac_solar_eclipse_geometry_t *geometry)
{
    if (!geometry)
        return NAN;
    if (geometry->sun_altitude_degrees + geometry->sun_sd <= 0.0 || geometry->moon_sd < geometry->sun_sd) {
        return NAN;
    }
    return geometry->separation - (geometry->moon_sd - geometry->sun_sd);
}

static bool almanac_solar_totality_score_at_local_minimum(almanac_t *almanac, const almanac_observer_t *observer,
    double seed_jd, double *out_jd, double *out_score_degrees)
{
    almanac_eclipse_contact_context_t contact = {ALMANAC_BODY_ID_MOON, observer, ALMANAC_CONTACT_LEVEL_OUTER};
    almanac_solar_eclipse_geometry_t geometry;
    double local_jd;
    double score_degrees;

    if (out_jd)
        *out_jd = NAN;
    if (out_score_degrees)
        *out_score_degrees = NAN;
    if (!almanac || !observer || !out_score_degrees)
        return false;
    local_jd = almanac_find_local_minimum_jd(almanac, almanac_solar_eclipse_metric, &contact, seed_jd, 0.25,
                                             1.0 / 12.0, 3);
    if (!almanac_solar_eclipse_geometry(almanac, local_jd, observer, &geometry))
        return false;
    score_degrees = almanac_solar_totality_score_degrees(&geometry);
    if (!(score_degrees == score_degrees))
        return false;
    if (out_jd)
        *out_jd = local_jd;
    *out_score_degrees = score_degrees;
    return true;
}

static double almanac_surface_distance_km(double lat1_degrees, double lon1_degrees, double lat2_degrees,
    double lon2_degrees)
{
    static const double earth_radius_km = 6371.0088;
    double lat1 = almanac_degrees_to_radians(lat1_degrees);
    double lat2 = almanac_degrees_to_radians(lat2_degrees);
    double dlat = almanac_degrees_to_radians(lat2_degrees - lat1_degrees);
    double dlon = almanac_degrees_to_radians(lon2_degrees - lon1_degrees);
    double a = sin(dlat * 0.5) * sin(dlat * 0.5) + cos(lat1) * cos(lat2) * sin(dlon * 0.5) * sin(dlon * 0.5);

    return earth_radius_km * 2.0 * atan2(sqrt(a), sqrt(fmax(0.0, 1.0 - a)));
}

static void almanac_destination_point(double lat_degrees, double lon_degrees, double distance_km, double bearing_degrees,
    double *out_lat_degrees, double *out_lon_degrees)
{
    static const double earth_radius_km = 6371.0088;
    double angular_distance = distance_km / earth_radius_km;
    double bearing = almanac_degrees_to_radians(bearing_degrees);
    double lat1 = almanac_degrees_to_radians(lat_degrees);
    double lon1 = almanac_degrees_to_radians(lon_degrees);
    double sin_lat1 = sin(lat1);
    double cos_lat1 = cos(lat1);
    double sin_distance = sin(angular_distance);
    double cos_distance = cos(angular_distance);
    double lat2 = asin(almanac_clamp_unit(sin_lat1 * cos_distance + cos_lat1 * sin_distance * cos(bearing)));
    double lon2 = lon1 + atan2(sin(bearing) * sin_distance * cos_lat1, cos_distance - sin_lat1 * sin(lat2));

    if (out_lat_degrees)
        *out_lat_degrees = almanac_radians_to_degrees(lat2);
    if (out_lon_degrees)
        *out_lon_degrees = almanac_normalize_degrees(almanac_radians_to_degrees(lon2) + 180.0) - 180.0;
}

static bool almanac_observer_at_location(almanac_t *almanac, const almanac_observer_t *origin, double latitude_degrees,
    double longitude_degrees, almanac_observer_t *out)
{
    if (!almanac || !origin || !out)
        return false;
    out->latitude_degrees = latitude_degrees;
    out->longitude_degrees = longitude_degrees;
    out->elevation_metres = origin->elevation_metres;
    return almanac_observer_is_valid(almanac, out);
}

static void almanac_fill_solar_totality_location(const almanac_observer_t *origin, double latitude_degrees,
    double longitude_degrees, double jd, double magnitude, double totality_percent,
    almanac_solar_totality_location_t *out)
{
    memset(out, 0, sizeof(*out));
    out->found = true;
    almanac_event_time_from_jd(jd, &out->greatest_eclipse);
    out->latitude_degrees = latitude_degrees;
    out->longitude_degrees = longitude_degrees;
    out->distance_km = almanac_surface_distance_km(origin->latitude_degrees, origin->longitude_degrees,
                                                   latitude_degrees, longitude_degrees);
    out->magnitude = magnitude;
    out->totality_percent = totality_percent;
}

static bool almanac_probe_solar_totality(almanac_t *almanac, const almanac_observer_t *origin, double seed_jd,
    double latitude_degrees, double longitude_degrees, almanac_solar_totality_location_t *out)
{
    almanac_observer_t observer;
    almanac_solar_eclipse_geometry_t geometry;
    almanac_solar_eclipse_circumstance_t circumstance;

    if (!almanac || !origin || !out)
        return false;
    if (!almanac_observer_at_location(almanac, origin, latitude_degrees, longitude_degrees, &observer))
        return false;

    if (!almanac_solar_eclipse_geometry(almanac, seed_jd, &observer, &geometry))
        return false;
    if (!almanac_solar_eclipse_circumstance_from_geometry(&geometry, &circumstance) ||
        circumstance.kind != ALMANAC_SOLAR_ECLIPSE_TOTAL) {
        return false;
    }

    almanac_fill_solar_totality_location(origin, latitude_degrees, longitude_degrees, seed_jd, circumstance.magnitude,
                                         circumstance.totality_percent, out);
    return true;
}

static bool almanac_probe_solar_totality_candidate(almanac_t *almanac, const almanac_observer_t *origin, double seed_jd,
    double latitude_degrees, double longitude_degrees, double tolerance_degrees, almanac_solar_totality_location_t *out)
{
    almanac_observer_t observer;
    almanac_solar_eclipse_geometry_t geometry;
    double score_degrees;

    if (!almanac || !origin || !out)
        return false;
    if (!almanac_observer_at_location(almanac, origin, latitude_degrees, longitude_degrees, &observer))
        return false;
    if (!almanac_solar_eclipse_geometry(almanac, seed_jd, &observer, &geometry))
        return false;
    score_degrees = almanac_solar_totality_score_degrees(&geometry);
    if (!(score_degrees == score_degrees) || score_degrees > tolerance_degrees) {
        return false;
    }

    almanac_fill_solar_totality_location(origin, latitude_degrees, longitude_degrees, seed_jd, NAN, NAN, out);
    return true;
}

static bool almanac_refine_solar_totality(almanac_t *almanac, const almanac_observer_t *origin, double seed_jd,
    const almanac_solar_totality_location_t *coarse, almanac_solar_totality_location_t *out)
{
    almanac_observer_t observer;
    almanac_eclipse_contact_context_t contact;
    almanac_solar_eclipse_geometry_t geometry;
    almanac_solar_eclipse_circumstance_t circumstance;
    double local_jd;

    if (!almanac || !origin || !coarse || !coarse->found || !out)
        return false;
    if (!almanac_observer_at_location(almanac, origin, coarse->latitude_degrees, coarse->longitude_degrees,
                                      &observer)) {
        return false;
    }

    contact.body_id = ALMANAC_BODY_ID_MOON;
    contact.observer = &observer;
    contact.contact_level = ALMANAC_CONTACT_LEVEL_OUTER;
    local_jd = almanac_find_local_minimum_jd(almanac, almanac_solar_eclipse_metric, &contact, seed_jd, 0.125,
                                             1.0 / 96.0, 9);
    if (!almanac_solar_eclipse_geometry(almanac, local_jd, &observer, &geometry))
        return false;
    if (!almanac_solar_eclipse_circumstance_from_geometry(&geometry, &circumstance) ||
        circumstance.kind != ALMANAC_SOLAR_ECLIPSE_TOTAL) {
        return false;
    }

    *out = *coarse;
    almanac_event_time_from_jd(local_jd, &out->greatest_eclipse);
    out->magnitude = circumstance.magnitude;
    out->totality_percent = circumstance.totality_percent;
    return true;
}

static void almanac_remember_nearest_totality(const almanac_solar_totality_location_t *candidate,
    almanac_solar_totality_location_t *best)
{
    if (!candidate || !candidate->found || !best)
        return;
    if (!best->found || candidate->distance_km < best->distance_km)
        *best = *candidate;
}

static int almanac_land_box_distance_compare(const void *lhs, const void *rhs)
{
    const almanac_land_box_distance_t *left = lhs;
    const almanac_land_box_distance_t *right = rhs;

    if (left->distance_km < right->distance_km)
        return -1;
    if (left->distance_km > right->distance_km)
        return 1;
    return 0;
}

static double almanac_land_box_min_distance_km(const almanac_land_box_t *box, const almanac_observer_t *observer)
{
    double lat;
    double lon;

    if (!box || !observer)
        return DBL_MAX;
    lat = fmax(box->min_lat, fmin(box->max_lat, observer->latitude_degrees));
    lon = fmax(box->min_lon, fmin(box->max_lon, observer->longitude_degrees));
    return almanac_surface_distance_km(observer->latitude_degrees, observer->longitude_degrees, lat, lon);
}

static double almanac_land_box_min_distance_to_point_km(const almanac_land_box_t *box, double latitude_degrees,
    double longitude_degrees)
{
    double lat;
    double lon;

    if (!box)
        return DBL_MAX;
    lat = fmax(box->min_lat, fmin(box->max_lat, latitude_degrees));
    lon = fmax(box->min_lon, fmin(box->max_lon, longitude_degrees));
    return almanac_surface_distance_km(latitude_degrees, longitude_degrees, lat, lon);
}

static bool almanac_probable_land_point(double latitude_degrees, double longitude_degrees)
{
    size_t i;

    for (i = 0u; i < sizeof(ALMANAC_PROBABLE_LAND_BOXES) / sizeof(ALMANAC_PROBABLE_LAND_BOXES[0]); ++i) {
        if (latitude_degrees >= ALMANAC_PROBABLE_LAND_BOXES[i].min_lat &&
            latitude_degrees <= ALMANAC_PROBABLE_LAND_BOXES[i].max_lat &&
            longitude_degrees >= ALMANAC_PROBABLE_LAND_BOXES[i].min_lon &&
            longitude_degrees <= ALMANAC_PROBABLE_LAND_BOXES[i].max_lon) {
            return true;
        }
    }
    return false;
}

/* Search for the nearest location with solar totality. */
bool almanac_nearest_solar_totality(almanac_t *almanac, const almanac_observer_t *observer,
    const almanac_solar_eclipse_t *eclipse, almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t best;
    almanac_solar_totality_location_t candidate;
    double radius_km;
    bool coarse_found = false;

    if (!almanac || !observer || !eclipse || !out) {
        almanac_set_error(almanac, "invalid nearest solar totality request");
        return false;
    }
    memset(out, 0, sizeof(*out));
    memset(&best, 0, sizeof(best));
    if (!almanac_observer_is_valid(almanac, observer) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }

    if (almanac_probe_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, observer->latitude_degrees,
                                     observer->longitude_degrees, &candidate)) {
        *out = candidate;
        return true;
    }

    for (radius_km = 75.0; radius_km <= 12000.0 && !coarse_found; radius_km += 75.0) {
        double bearing_step = radius_km < 750.0 ? 15.0 : 7.5;
        double bearing;

        for (bearing = 0.0; bearing < 360.0; bearing += bearing_step) {
            double lat;
            double lon;

            almanac_destination_point(observer->latitude_degrees, observer->longitude_degrees, radius_km, bearing, &lat,
                                      &lon);
            if (almanac_probe_solar_totality_candidate(almanac, observer, eclipse->greatest_eclipse.jd, lat, lon, 1.0,
                                                       &candidate) &&
                almanac_refine_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, &candidate,
                                              &candidate)) {
                almanac_remember_nearest_totality(&candidate, &best);
                coarse_found = true;
            }
        }
    }

    if (best.found) {
        double north_km;
        almanac_solar_totality_location_t refined = best;

        for (north_km = -300.0; north_km <= 300.0; north_km += 25.0) {
            double east_km;

            for (east_km = -300.0; east_km <= 300.0; east_km += 25.0) {
                double lat;
                double lon;

                almanac_destination_point(best.latitude_degrees, best.longitude_degrees, hypot(north_km, east_km),
                                          almanac_radians_to_degrees(atan2(east_km, north_km)), &lat, &lon);
                if (almanac_probe_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, lat, lon,
                                                 &candidate)) {
                    almanac_remember_nearest_totality(&candidate, &refined);
                }
            }
        }
        best = refined;
        if (almanac_refine_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, &best, &candidate)) {
            best = candidate;
        }
    }

    *out = best;
    return true;
}

/* Check totality at the observer's location. */
bool almanac_solar_eclipse_totality_at(almanac_t *almanac, const almanac_observer_t *observer,
    const almanac_solar_eclipse_t *eclipse, almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t candidate;
    almanac_solar_totality_location_t refined;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!almanac || !observer || !eclipse) {
        almanac_set_error(almanac, "invalid solar totality test request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }
    if (!almanac_probe_solar_totality_candidate(almanac, observer, eclipse->greatest_eclipse.jd,
                                                observer->latitude_degrees, observer->longitude_degrees, 1.0,
                                                &candidate)) {
        return false;
    }
    if (!almanac_refine_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, &candidate, &refined)) {
        return false;
    }
    if (out)
        *out = refined;
    return true;
}

/* Refine a nearby candidate location into a totality result. */
bool almanac_solar_eclipse_totality_from_seed(almanac_t *almanac, const almanac_observer_t *origin,
    const almanac_observer_t *seed, const almanac_solar_eclipse_t *eclipse, double tolerance_degrees,
    almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t candidate;
    double local_jd;
    double score_degrees;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!almanac || !origin || !seed || !eclipse || !out) {
        almanac_set_error(almanac, "invalid nearby solar totality request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, origin) || !almanac_observer_is_valid(almanac, seed) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd) || !(tolerance_degrees >= 0.0)) {
        return false;
    }
    if (!almanac_solar_totality_score_at_local_minimum(almanac, seed, eclipse->greatest_eclipse.jd, &local_jd,
                                                       &score_degrees) ||
        score_degrees > tolerance_degrees) {
        return false;
    }
    almanac_fill_solar_totality_location(origin, seed->latitude_degrees, seed->longitude_degrees, local_jd, NAN, NAN,
                                         &candidate);
    return almanac_refine_solar_totality(almanac, origin, local_jd, &candidate, out);
}

/* Score a candidate location near greatest eclipse. */
bool almanac_solar_eclipse_totality_seed_score(almanac_t *almanac, const almanac_observer_t *seed,
    const almanac_solar_eclipse_t *eclipse, double *out_score_degrees)
{
    if (out_score_degrees)
        *out_score_degrees = NAN;
    if (!almanac || !seed || !eclipse || !out_score_degrees) {
        almanac_set_error(almanac, "invalid solar totality seed score request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, seed) || !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }
    return almanac_solar_totality_score_at_local_minimum(almanac, seed, eclipse->greatest_eclipse.jd, NULL,
                                                         out_score_degrees);
}

/* Search probable land areas for nearby solar totality. */
bool almanac_nearest_solar_totality_land(almanac_t *almanac, const almanac_observer_t *observer,
    const almanac_solar_eclipse_t *eclipse, almanac_solar_totality_location_t *out)
{
    almanac_solar_totality_location_t best;
    almanac_solar_totality_location_t candidate;
    almanac_solar_totality_location_t path_seed;
    almanac_land_box_distance_t
        box_distances[sizeof(ALMANAC_PROBABLE_LAND_BOXES) / sizeof(ALMANAC_PROBABLE_LAND_BOXES[0])];
    size_t box_count = sizeof(ALMANAC_PROBABLE_LAND_BOXES) / sizeof(ALMANAC_PROBABLE_LAND_BOXES[0]);
    size_t box_index;
    bool have_path_seed = false;

    if (!almanac || !observer || !eclipse || !out) {
        almanac_set_error(almanac, "invalid nearest land solar totality request");
        return false;
    }
    memset(out, 0, sizeof(*out));
    memset(&best, 0, sizeof(best));
    memset(&path_seed, 0, sizeof(path_seed));
    if (!almanac_observer_is_valid(almanac, observer) ||
        !(eclipse->greatest_eclipse.jd == eclipse->greatest_eclipse.jd)) {
        return false;
    }

    if (almanac_probable_land_point(observer->latitude_degrees, observer->longitude_degrees) &&
        almanac_probe_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, observer->latitude_degrees,
                                     observer->longitude_degrees, &candidate)) {
        *out = candidate;
        return true;
    }

    have_path_seed = almanac_nearest_solar_totality(almanac, observer, eclipse, &path_seed) && path_seed.found;
    if (have_path_seed && almanac_probable_land_point(path_seed.latitude_degrees, path_seed.longitude_degrees)) {
        *out = path_seed;
        return true;
    }

    if (!best.found) {
        for (box_index = 0u; box_index < box_count; ++box_index) {
            box_distances[box_index].index = box_index;
            box_distances[box_index].distance_km =
                have_path_seed
                    ? almanac_land_box_min_distance_to_point_km(&ALMANAC_PROBABLE_LAND_BOXES[box_index],
                                                                path_seed.latitude_degrees, path_seed.longitude_degrees)
                    : almanac_land_box_min_distance_km(&ALMANAC_PROBABLE_LAND_BOXES[box_index], observer);
        }
        qsort(box_distances, box_count, sizeof(box_distances[0]), almanac_land_box_distance_compare);

        for (box_index = 0u; box_index < box_count; ++box_index) {
            const almanac_land_box_t *box = &ALMANAC_PROBABLE_LAND_BOXES[box_distances[box_index].index];
            double lat_step = (box->max_lat - box->min_lat) > 20.0 ? 1.0 : 0.5;
            double lon_step = (box->max_lon - box->min_lon) > 20.0 ? 1.0 : 0.5;
            double lat;

            if (best.found && have_path_seed && box_distances[box_index].distance_km > 1500.0)
                break;
            if (best.found && !have_path_seed && box_distances[box_index].distance_km > best.distance_km + 25.0)
                break;

            for (lat = box->min_lat; lat <= box->max_lat + 1e-9; lat += lat_step) {
                double lon;

                for (lon = box->min_lon; lon <= box->max_lon + 1e-9; lon += lon_step) {
                    if (best.found &&
                        almanac_surface_distance_km(observer->latitude_degrees, observer->longitude_degrees, lat, lon) >
                            best.distance_km + 25.0) {
                        continue;
                    }
                    if (have_path_seed && almanac_surface_distance_km(path_seed.latitude_degrees,
                                                                      path_seed.longitude_degrees, lat, lon) > 1500.0) {
                        continue;
                    }
                    if (almanac_probe_solar_totality_candidate(almanac, observer, eclipse->greatest_eclipse.jd, lat,
                                                               lon, 0.35, &candidate)) {
                        almanac_remember_nearest_totality(&candidate, &best);
                    }
                }
            }
        }
    }

    if (best.found) {
        static const double refine_radii_km[] = {120.0, 45.0};
        static const double refine_steps_km[] = {30.0, 15.0};
        size_t pass;

        for (pass = 0u; pass < sizeof(refine_radii_km) / sizeof(refine_radii_km[0]); ++pass) {
            almanac_solar_totality_location_t refined = best;
            double north_km;

            for (north_km = -refine_radii_km[pass]; north_km <= refine_radii_km[pass];
                 north_km += refine_steps_km[pass]) {
                double east_km;

                for (east_km = -refine_radii_km[pass]; east_km <= refine_radii_km[pass];
                     east_km += refine_steps_km[pass]) {
                    double lat;
                    double lon;

                    almanac_destination_point(best.latitude_degrees, best.longitude_degrees, hypot(north_km, east_km),
                                              almanac_radians_to_degrees(atan2(east_km, north_km)), &lat, &lon);
                    if (!almanac_probable_land_point(lat, lon))
                        continue;
                    if (almanac_probe_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, lat, lon,
                                                     &candidate)) {
                        almanac_remember_nearest_totality(&candidate, &refined);
                    }
                }
            }
            best = refined;
        }
        if (almanac_refine_solar_totality(almanac, observer, eclipse->greatest_eclipse.jd, &best, &candidate)) {
            best = candidate;
        }
    }

    *out = best;
    return true;
}
