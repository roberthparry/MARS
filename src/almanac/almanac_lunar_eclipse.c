/**
 * @file almanac_lunar_eclipse.c
 * @brief Lunar-eclipse geometry and event circumstances.
 *
 * Calculates the Moon's relation to the Earth's shadow, refines contacts and assembles lunar-eclipse results.
 * Searches depend on the configured ephemeris and requested time window.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Lunar eclipse geometry, circumstances and event searches. */
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"

typedef struct almanac_lunar_eclipse_geometry_t {
    double opposition_error;
    double sun_sd;
    double moon_sd;
    double delta_moon;
    double umbra_radius;
    double penumbra_radius;
    double moon_altitude_degrees;
} almanac_lunar_eclipse_geometry_t;

struct _almanac_lunar_eclipse_t {
    almanac_lunar_eclipse_kind_t kind;
    almanac_event_time_t p1_contact;
    almanac_event_time_t u1_contact;
    almanac_event_time_t u2_contact;
    almanac_event_time_t greatest_eclipse;
    almanac_event_time_t u3_contact;
    almanac_event_time_t u4_contact;
    almanac_event_time_t p4_contact;
    double opposition_error_degrees;
    double umbral_magnitude;
    double penumbral_magnitude;
    double totality_percent;
    double umbral_radius_degrees;
    double penumbral_radius_degrees;
    double moon_semi_diameter_degrees;
};

static bool almanac_lunar_eclipse_geometry(almanac_t *almanac, double jd, const almanac_observer_t *observer,
    almanac_lunar_eclipse_geometry_t *out)
{
    static const double earth_radius_au = 6378.137 / 149597870.7;
    static const double reduced_parallax_factor = 0.9983407;
    static const double shadow_enlargement = 1.02;
    almanac_state_t sun_state;
    almanac_state_t moon_state;
    cartesian3_t moon_topocentric;
    cartesian3_t antisolar_geocentric;
    double sun_distance;
    double moon_parallax;
    double sun_parallax;

    if (!almanac || !observer || !out)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state) ||
        !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_MOON, jd, &moon_state)) {
        return false;
    }
    if (!almanac_topocentric_vector(almanac, &moon_state.geocentric_equatorial_au, observer, jd, &moon_topocentric)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    antisolar_geocentric = cartesian_negate(&sun_state.geocentric_equatorial_au);
    out->opposition_error = almanac_angular_separation_degrees(&antisolar_geocentric, &moon_state.geocentric_equatorial_au);
    sun_distance = cartesian_length(&sun_state.geocentric_equatorial_au);
    out->delta_moon = cartesian_length(&moon_state.geocentric_equatorial_au);
    out->sun_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_SUN, sun_distance);
    out->moon_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_MOON, out->delta_moon);
    out->moon_altitude_degrees = almanac_topocentric_altitude_degrees(almanac, &moon_topocentric, observer, jd);
    if (!(out->opposition_error == out->opposition_error) || !(out->sun_sd > 0.0) ||
        !(out->moon_altitude_degrees == out->moon_altitude_degrees) || !(out->moon_sd > 0.0) ||
        !(out->delta_moon > 0.0)) {
        return false;
    }

    moon_parallax =
        almanac_radians_to_degrees(asin(almanac_clamp_unit(reduced_parallax_factor * earth_radius_au / out->delta_moon)));
    sun_parallax = almanac_radians_to_degrees(asin(almanac_clamp_unit(earth_radius_au / sun_distance)));
    out->umbra_radius = shadow_enlargement * (moon_parallax + sun_parallax - out->sun_sd);
    out->penumbra_radius = shadow_enlargement * (moon_parallax + sun_parallax + out->sun_sd);
    if (!(out->umbra_radius > 0.0) || !(out->penumbra_radius > 0.0))
        return false;
    return true;
}

static double almanac_lunar_eclipse_contact_residual(almanac_t *almanac, double jd, void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_lunar_eclipse_geometry_t geometry;

    if (!contact || !almanac_lunar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_OUTER)
        return geometry.opposition_error - (geometry.penumbra_radius + geometry.moon_sd);
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_INNER)
        return geometry.opposition_error - (geometry.umbra_radius + geometry.moon_sd);
    if (!(geometry.umbra_radius > geometry.moon_sd))
        return NAN;
    return geometry.opposition_error - (geometry.umbra_radius - geometry.moon_sd);
}

static double almanac_lunar_eclipse_metric(almanac_t *almanac, double jd, void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_lunar_eclipse_geometry_t geometry;

    if (!contact || !almanac_lunar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    return geometry.opposition_error;
}

static bool almanac_fill_lunar_eclipse(almanac_t *almanac, double jd, const almanac_observer_t *observer,
    almanac_lunar_eclipse_t *out)
{
    almanac_lunar_eclipse_geometry_t geometry;
    almanac_eclipse_contact_context_t contact = {ALMANAC_BODY_ID_MOON, observer, 0};
    static const double contact_step_days = 1.0 / 24.0;
    double umbral_magnitude;
    double penumbral_magnitude;
    double totality_percent;

    if (!almanac || !out)
        return false;
    if (!almanac_lunar_eclipse_geometry(almanac, jd, observer, &geometry))
        return false;
    if (geometry.moon_altitude_degrees + geometry.moon_sd <= 0.0)
        return false;

    penumbral_magnitude =
        (geometry.penumbra_radius + geometry.moon_sd - geometry.opposition_error) / (2.0 * geometry.moon_sd);
    umbral_magnitude =
        (geometry.umbra_radius + geometry.moon_sd - geometry.opposition_error) / (2.0 * geometry.moon_sd);
    if (penumbral_magnitude <= 0.0)
        return false;
    totality_percent = umbral_magnitude > 0.0 ? almanac_disc_coverage_percent(geometry.moon_sd, geometry.umbra_radius,
                                                                              geometry.opposition_error)
                                              : 0.0;
    if (!(totality_percent == totality_percent))
        return false;
    totality_percent = fmax(0.0, fmin(100.0, totality_percent));

    memset(out, 0, sizeof(*out));
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_lunar_eclipse_contact_residual, &contact, jd,
                                                       -1.0, 1.0, contact_step_days),
                               &out->p1_contact);
    almanac_event_time_from_jd(NAN, &out->u1_contact);
    almanac_event_time_from_jd(NAN, &out->u2_contact);
    almanac_event_time_from_jd(jd, &out->greatest_eclipse);
    almanac_event_time_from_jd(NAN, &out->u3_contact);
    almanac_event_time_from_jd(NAN, &out->u4_contact);
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_lunar_eclipse_contact_residual, &contact, jd,
                                                       1.0, 1.0, contact_step_days),
                               &out->p4_contact);
    out->opposition_error_degrees = geometry.opposition_error;
    out->umbral_magnitude = umbral_magnitude;
    out->penumbral_magnitude = penumbral_magnitude;
    out->totality_percent = totality_percent;
    out->umbral_radius_degrees = geometry.umbra_radius;
    out->penumbral_radius_degrees = geometry.penumbra_radius;
    out->moon_semi_diameter_degrees = geometry.moon_sd;
    if (umbral_magnitude >= 1.0)
        out->kind = ALMANAC_LUNAR_ECLIPSE_TOTAL;
    else if (umbral_magnitude > 0.0)
        out->kind = ALMANAC_LUNAR_ECLIPSE_PARTIAL;
    else
        out->kind = ALMANAC_LUNAR_ECLIPSE_PENUMBRAL;
    if (out->kind == ALMANAC_LUNAR_ECLIPSE_PARTIAL || out->kind == ALMANAC_LUNAR_ECLIPSE_TOTAL) {
        contact.contact_level = ALMANAC_CONTACT_LEVEL_INNER;
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_lunar_eclipse_contact_residual, &contact,
                                                           jd, -1.0, 1.0, contact_step_days),
                                   &out->u1_contact);
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_lunar_eclipse_contact_residual, &contact,
                                                           jd, 1.0, 1.0, contact_step_days),
                                   &out->u4_contact);
    }
    if (out->kind == ALMANAC_LUNAR_ECLIPSE_TOTAL) {
        contact.contact_level = ALMANAC_CONTACT_LEVEL_TOTAL;
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_lunar_eclipse_contact_residual, &contact,
                                                           jd, -1.0, 1.0, contact_step_days),
                                   &out->u2_contact);
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_lunar_eclipse_contact_residual, &contact,
                                                           jd, 1.0, 1.0, contact_step_days),
                                   &out->u3_contact);
    }
    return true;
}

/* Return the classification for a lunar eclipse event. */
almanac_lunar_eclipse_kind_t almanac_lunar_eclipse_kind(const almanac_lunar_eclipse_t *event)
{
    return event ? event->kind : ALMANAC_LUNAR_ECLIPSE_PENUMBRAL;
}

/* Copy a named time from a lunar eclipse event. */
bool almanac_lunar_eclipse_time(const almanac_lunar_eclipse_t *event, almanac_event_time_kind_t time_kind,
    almanac_event_time_t *out)
{
    const almanac_event_time_t *time = NULL;

    if (!out)
        return false;
    almanac_event_time_from_jd(NAN, out);
    if (!event)
        return false;

    switch (time_kind) {
        case ALMANAC_EVENT_TIME_P1_CONTACT:
            time = &event->p1_contact;
            break;
        case ALMANAC_EVENT_TIME_U1_CONTACT:
            time = &event->u1_contact;
            break;
        case ALMANAC_EVENT_TIME_U2_CONTACT:
            time = &event->u2_contact;
            break;
        case ALMANAC_EVENT_TIME_GREATEST:
            time = &event->greatest_eclipse;
            break;
        case ALMANAC_EVENT_TIME_U3_CONTACT:
            time = &event->u3_contact;
            break;
        case ALMANAC_EVENT_TIME_U4_CONTACT:
            time = &event->u4_contact;
            break;
        case ALMANAC_EVENT_TIME_P4_CONTACT:
            time = &event->p4_contact;
            break;
        default:
            return false;
    }
    *out = *time;
    return time->valid;
}

/* Return the Moon's angular distance from the antisolar direction in degrees. */
double almanac_lunar_eclipse_opposition_error_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->opposition_error_degrees : NAN;
}

/* Return the lunar eclipse umbral magnitude. */
double almanac_lunar_eclipse_umbral_magnitude(const almanac_lunar_eclipse_t *event)
{
    return event ? event->umbral_magnitude : NAN;
}

/* Return the lunar eclipse penumbral magnitude. */
double almanac_lunar_eclipse_penumbral_magnitude(const almanac_lunar_eclipse_t *event)
{
    return event ? event->penumbral_magnitude : NAN;
}

/* Return the percentage of the lunar disc covered by the umbra at greatest eclipse. */
double almanac_lunar_eclipse_totality_percent(const almanac_lunar_eclipse_t *event)
{
    return event ? event->totality_percent : NAN;
}

/* Return the apparent umbral radius in degrees at greatest eclipse. */
double almanac_lunar_eclipse_umbral_radius_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->umbral_radius_degrees : NAN;
}

/* Return the apparent penumbral radius in degrees at greatest eclipse. */
double almanac_lunar_eclipse_penumbral_radius_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->penumbral_radius_degrees : NAN;
}

/* Return the Moon's apparent semi-diameter in degrees at greatest eclipse. */
double almanac_lunar_eclipse_moon_semi_diameter_degrees(const almanac_lunar_eclipse_t *event)
{
    return event ? event->moon_semi_diameter_degrees : NAN;
}

/* Find lunar eclipses within a civil time window. */
array_t *almanac_find_lunar_eclipses(almanac_t *almanac, const almanac_observer_t *observer, const datetime_t *start,
    const datetime_t *end)
{
    static const double synodic_month_days = 29.530588861;
    static const double base_new_moon_jd = 2451550.09766;
    double start_jd;
    double end_jd;
    long first_k;
    long last_k;
    long k;
    array_t *events;
    almanac_eclipse_contact_context_t contact = {ALMANAC_BODY_ID_MOON, observer, 0};

    if (!almanac_event_window_is_valid(almanac, start, end, &start_jd, &end_jd))
        return NULL;
    if (!almanac_observer_is_valid(almanac, observer))
        return NULL;
    events = array_create(sizeof(almanac_lunar_eclipse_t), NULL, NULL);
    if (!events) {
        almanac_set_error(almanac, "failed to allocate lunar eclipse array");
        return NULL;
    }
    first_k = (long)floor((start_jd - base_new_moon_jd) / synodic_month_days - 0.5) - 2L;
    last_k = (long)ceil((end_jd - base_new_moon_jd) / synodic_month_days - 0.5) + 2L;
    for (k = first_k; k <= last_k; ++k) {
        almanac_lunar_eclipse_t eclipse;
        double phase_k = (double)k + 0.5;
        double phase_jd;
        double local_jd;

        if (!almanac_eclipse_candidate_near_node(phase_k))
            continue;
        if (!almanac_refine_body_sun_longitude_near(almanac, ALMANAC_BODY_ID_MOON, 180.0,
                                                    almanac_mean_moon_phase_jd(phase_k), 2.0, &phase_jd)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to refine lunar eclipse opposition");
            return NULL;
        }
        if (phase_jd < start_jd - 1.0 || phase_jd > end_jd + 1.0)
            continue;
        local_jd = almanac_find_local_minimum_jd(almanac, almanac_lunar_eclipse_metric, &contact, phase_jd, 0.75,
                                                 1.0 / 8.0, 9);
        if (local_jd >= start_jd - 1e-9 && local_jd <= end_jd + 1e-9 &&
            almanac_fill_lunar_eclipse(almanac, local_jd, observer, &eclipse) && !array_add(events, &eclipse)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to append lunar eclipse event");
            return NULL;
        }
    }

    return events;
}
