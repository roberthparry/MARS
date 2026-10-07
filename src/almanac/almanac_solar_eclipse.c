/**
 * @file almanac_solar_eclipse.c
 * @brief Solar-eclipse geometry and local contact searches.
 *
 * Evaluates apparent Sun-Moon overlap, classifies local eclipse circumstances and refines event contacts. This
 * unit supplies both interval searches and checks for an eclipse already in progress.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Solar eclipse geometry, circumstances and event searches. */
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"

bool almanac_solar_eclipse_geometry(almanac_t *almanac, double jd, const almanac_observer_t *observer,
    almanac_solar_eclipse_geometry_t *out)
{
    almanac_state_t sun_state;
    almanac_state_t moon_state;
    cartesian3_t sun_topocentric;
    cartesian3_t moon_topocentric;
    double sun_distance;
    double moon_distance;

    if (!almanac || !observer || !out)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state) ||
        !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_MOON, jd, &moon_state)) {
        return false;
    }
    if (!almanac_topocentric_vector(almanac, &sun_state.geocentric_equatorial_au, observer, jd, &sun_topocentric) ||
        !almanac_topocentric_vector(almanac, &moon_state.geocentric_equatorial_au, observer, jd, &moon_topocentric)) {
        return false;
    }
    sun_distance = cartesian_length(&sun_topocentric);
    moon_distance = cartesian_length(&moon_topocentric);

    memset(out, 0, sizeof(*out));
    out->separation = almanac_angular_separation_degrees(&sun_topocentric, &moon_topocentric);
    out->sun_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_SUN, sun_distance);
    out->moon_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_MOON, moon_distance);
    out->sun_altitude_degrees = almanac_topocentric_altitude_degrees(almanac, &sun_topocentric, observer, jd);
    if (!(out->separation == out->separation) || !(out->sun_sd > 0.0) || !(out->moon_sd > 0.0) ||
        !(out->sun_altitude_degrees == out->sun_altitude_degrees)) {
        return false;
    }
    out->apparent_separation = out->separation;
    return true;
}

bool almanac_solar_eclipse_circumstance_from_geometry(const almanac_solar_eclipse_geometry_t *geometry,
    almanac_solar_eclipse_circumstance_t *out)
{
    double magnitude;
    double totality_percent;
    bool central;

    if (!geometry || !out)
        return false;
    if (geometry->sun_altitude_degrees + geometry->sun_sd <= 0.0 ||
        geometry->separation >= geometry->sun_sd + geometry->moon_sd) {
        return false;
    }

    magnitude = (geometry->sun_sd + geometry->moon_sd - geometry->separation) / (2.0 * geometry->sun_sd);
    totality_percent =
        almanac_disc_coverage_percent(geometry->sun_sd, geometry->moon_sd, geometry->apparent_separation);
    if (!(magnitude == magnitude) || !(totality_percent == totality_percent))
        return false;

    memset(out, 0, sizeof(*out));
    central = geometry->separation <= fabs(geometry->sun_sd - geometry->moon_sd);
    out->magnitude = magnitude;
    out->totality_percent = fmax(0.0, fmin(100.0, totality_percent));
    out->central = central;
    if (central && geometry->moon_sd >= geometry->sun_sd)
        out->kind = ALMANAC_SOLAR_ECLIPSE_TOTAL;
    else if (central)
        out->kind = ALMANAC_SOLAR_ECLIPSE_ANNULAR;
    else
        out->kind = ALMANAC_SOLAR_ECLIPSE_PARTIAL;
    return true;
}

static double almanac_solar_eclipse_contact_residual(almanac_t *almanac, double jd, void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_eclipse_geometry_t geometry;

    if (!contact || !almanac_solar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_INNER)
        return geometry.apparent_separation - fabs(geometry.sun_sd - geometry.moon_sd);
    return geometry.separation - (geometry.sun_sd + geometry.moon_sd);
}

double almanac_solar_eclipse_metric(almanac_t *almanac, double jd, void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_eclipse_geometry_t geometry;

    if (!contact || !almanac_solar_eclipse_geometry(almanac, jd, contact->observer, &geometry))
        return NAN;
    if (geometry.sun_altitude_degrees + geometry.sun_sd <= 0.0)
        return NAN;
    return geometry.separation;
}

static bool almanac_fill_solar_eclipse(almanac_t *almanac, double jd, const almanac_observer_t *observer,
    almanac_solar_eclipse_t *out)
{
    almanac_solar_eclipse_geometry_t geometry;
    almanac_solar_eclipse_circumstance_t circumstance;
    almanac_eclipse_contact_context_t contact = {ALMANAC_BODY_ID_MOON, observer, 0};
    static const double contact_step_days = 1.0 / 24.0;

    if (!almanac || !out)
        return false;
    if (!almanac_solar_eclipse_geometry(almanac, jd, observer, &geometry))
        return false;
    if (!almanac_solar_eclipse_circumstance_from_geometry(&geometry, &circumstance))
        return false;

    memset(out, 0, sizeof(*out));
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_eclipse_contact_residual, &contact, jd,
                                                       -1.0, 1.0, contact_step_days),
                               &out->first_contact);
    almanac_event_time_from_jd(NAN, &out->second_contact);
    almanac_event_time_from_jd(jd, &out->greatest_eclipse);
    almanac_event_time_from_jd(NAN, &out->third_contact);
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_eclipse_contact_residual, &contact, jd,
                                                       1.0, 1.0, contact_step_days),
                               &out->fourth_contact);
    out->separation_degrees = geometry.separation;
    out->magnitude = circumstance.magnitude;
    out->totality_percent = circumstance.totality_percent;
    out->sun_semi_diameter_degrees = geometry.sun_sd;
    out->moon_semi_diameter_degrees = geometry.moon_sd;
    out->central = circumstance.central;
    out->kind = circumstance.kind;
    if (out->kind == ALMANAC_SOLAR_ECLIPSE_TOTAL || out->kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR) {
        contact.contact_level = ALMANAC_CONTACT_LEVEL_INNER;
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_eclipse_contact_residual, &contact,
                                                           jd, -1.0, 1.0, contact_step_days),
                                   &out->second_contact);
        almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_eclipse_contact_residual, &contact,
                                                           jd, 1.0, 1.0, contact_step_days),
                                   &out->third_contact);
    }
    return true;
}

/* Return the classification for a solar eclipse event. */
almanac_solar_eclipse_kind_t almanac_solar_eclipse_kind(const almanac_solar_eclipse_t *event)
{
    return event ? event->kind : ALMANAC_SOLAR_ECLIPSE_PARTIAL;
}

/* Copy a named time from a solar eclipse event. */
bool almanac_solar_eclipse_time(const almanac_solar_eclipse_t *event, almanac_event_time_kind_t time_kind,
    almanac_event_time_t *out)
{
    const almanac_event_time_t *time = NULL;

    if (!out)
        return false;
    almanac_event_time_from_jd(NAN, out);
    if (!event)
        return false;

    switch (time_kind) {
        case ALMANAC_EVENT_TIME_FIRST_CONTACT:
            time = &event->first_contact;
            break;
        case ALMANAC_EVENT_TIME_SECOND_CONTACT:
            time = &event->second_contact;
            break;
        case ALMANAC_EVENT_TIME_GREATEST:
            time = &event->greatest_eclipse;
            break;
        case ALMANAC_EVENT_TIME_THIRD_CONTACT:
            time = &event->third_contact;
            break;
        case ALMANAC_EVENT_TIME_FOURTH_CONTACT:
            time = &event->fourth_contact;
            break;
        default:
            return false;
    }
    *out = *time;
    return time->valid;
}

/* Return the Sun-Moon angular separation in degrees at greatest eclipse. */
double almanac_solar_eclipse_separation_degrees(const almanac_solar_eclipse_t *event)
{
    return event ? event->separation_degrees : NAN;
}

/* Return the solar eclipse magnitude. */
double almanac_solar_eclipse_magnitude(const almanac_solar_eclipse_t *event)
{
    return event ? event->magnitude : NAN;
}

/* Return the percentage of the solar disc obscured at greatest eclipse. */
double almanac_solar_eclipse_totality_percent(const almanac_solar_eclipse_t *event)
{
    return event ? event->totality_percent : NAN;
}

/* Return the Sun's apparent semi-diameter in degrees at greatest eclipse. */
double almanac_solar_eclipse_sun_semi_diameter_degrees(const almanac_solar_eclipse_t *event)
{
    return event ? event->sun_semi_diameter_degrees : NAN;
}

/* Return the Moon's apparent semi-diameter in degrees at greatest eclipse. */
double almanac_solar_eclipse_moon_semi_diameter_degrees(const almanac_solar_eclipse_t *event)
{
    return event ? event->moon_semi_diameter_degrees : NAN;
}

/* Report whether the solar eclipse is central at the observer's location. */
bool almanac_solar_eclipse_is_central(const almanac_solar_eclipse_t *event)
{
    return event ? event->central : false;
}

/* Find solar eclipses within a civil time window. */
array_t *almanac_find_solar_eclipses(almanac_t *almanac, const almanac_observer_t *observer, const datetime_t *start,
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
    events = array_create(sizeof(almanac_solar_eclipse_t), NULL, NULL);
    if (!events) {
        almanac_set_error(almanac, "failed to allocate solar eclipse array");
        return NULL;
    }
    first_k = (long)floor((start_jd - base_new_moon_jd) / synodic_month_days) - 2L;
    last_k = (long)ceil((end_jd - base_new_moon_jd) / synodic_month_days) + 2L;
    for (k = first_k; k <= last_k; ++k) {
        almanac_solar_eclipse_t eclipse;
        double phase_jd;
        double local_jd;

        if (!almanac_eclipse_candidate_near_node((double)k))
            continue;
        if (!almanac_refine_body_sun_longitude_near(almanac, ALMANAC_BODY_ID_MOON, 0.0,
                                                    almanac_mean_moon_phase_jd((double)k), 2.0, &phase_jd)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to refine solar eclipse conjunction");
            return NULL;
        }
        if (phase_jd < start_jd - 1.0 || phase_jd > end_jd + 1.0)
            continue;
        local_jd = almanac_find_local_minimum_jd(almanac, almanac_solar_eclipse_metric, &contact, phase_jd, 0.75,
                                                 1.0 / 8.0, 9);
        if (local_jd >= start_jd - 1e-9 && local_jd <= end_jd + 1e-9 &&
            almanac_fill_solar_eclipse(almanac, local_jd, observer, &eclipse) && !array_add(events, &eclipse)) {
            array_destroy(events);
            almanac_set_error(almanac, "failed to append solar eclipse event");
            return NULL;
        }
    }

    return events;
}

/* Test whether a solar eclipse is locally in progress. */
bool almanac_solar_eclipse_in_progress(almanac_t *almanac, const almanac_observer_t *observer, const datetime_t *moment)
{
    almanac_solar_eclipse_geometry_t geometry;
    double jd;

    if (!almanac || !observer || !moment)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    jd = datetime_jd(moment);
    if (!isfinite(jd))
        return false;
    if (!almanac_solar_eclipse_geometry(almanac, jd, observer, &geometry))
        return false;
    return geometry.sun_altitude_degrees + geometry.sun_sd > 0.0 &&
           geometry.separation < geometry.sun_sd + geometry.moon_sd;
}
