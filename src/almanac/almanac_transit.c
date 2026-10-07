/**
 * @file almanac_transit.c
 * @brief Mercury and Venus transit searches across the Sun.
 *
 * Calculates projected transit geometry, locates contacts and constructs per-event circumstances. The search is
 * specialised to the supported interior planets and configured ephemeris coverage.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Mercury and Venus solar-transit geometry and event searches. */
#include <float.h>
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"

typedef struct almanac_solar_transit_geometry_t {
    double separation;
    double sun_sd;
    double body_sd;
    almanac_body_id_t body_id;
    double sun_altitude_degrees;
} almanac_solar_transit_geometry_t;

struct _almanac_solar_transit_t {
    almanac_body_id_t body_id;
    almanac_event_time_t first_contact;
    almanac_event_time_t second_contact;
    almanac_event_time_t greatest_transit;
    almanac_event_time_t third_contact;
    almanac_event_time_t fourth_contact;
    double separation_degrees;
    double solar_semi_diameter_degrees;
    double planet_semi_diameter_degrees;
    double chord_distance_fraction;
    bool interior;
};

static bool almanac_solar_transit_geometry(almanac_t *almanac, almanac_body_id_t body_id, double jd,
    const almanac_observer_t *observer, almanac_solar_transit_geometry_t *out)
{
    almanac_state_t sun_state;
    almanac_state_t body_state;
    cartesian3_t sun_topocentric;
    cartesian3_t body_topocentric;
    double sun_distance;
    double body_distance;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !observer || !out) {
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state) ||
        !almanac_state_for_body_at_jd(almanac, body_id, jd, &body_state)) {
        return false;
    }
    if (!almanac_topocentric_vector(almanac, &sun_state.geocentric_equatorial_au, observer, jd, &sun_topocentric) ||
        !almanac_topocentric_vector(almanac, &body_state.geocentric_equatorial_au, observer, jd, &body_topocentric)) {
        return false;
    }
    sun_distance = cartesian_length(&sun_topocentric);
    body_distance = cartesian_length(&body_topocentric);

    memset(out, 0, sizeof(*out));
    out->body_id = body_id;
    out->sun_sd = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_SUN, sun_distance);
    out->body_sd = almanac_body_semi_diameter_from_distance_degrees(body_id, body_distance);
    out->separation = almanac_angular_separation_degrees(&sun_topocentric, &body_topocentric);
    out->sun_altitude_degrees = almanac_topocentric_altitude_degrees(almanac, &sun_topocentric, observer, jd);
    return out->sun_sd > 0.0 && out->body_sd > 0.0 && out->separation == out->separation &&
           out->sun_altitude_degrees == out->sun_altitude_degrees;
}

static double almanac_solar_transit_contact_residual(almanac_t *almanac, double jd, void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_transit_geometry_t geometry;

    if (!contact || !almanac_solar_transit_geometry(almanac, contact->body_id, jd, contact->observer, &geometry))
        return NAN;
    if (contact->contact_level == ALMANAC_CONTACT_LEVEL_INNER)
        return geometry.separation - (geometry.sun_sd - geometry.body_sd);
    return geometry.separation - (geometry.sun_sd + geometry.body_sd);
}

static double almanac_solar_transit_metric(almanac_t *almanac, double jd, void *context)
{
    almanac_eclipse_contact_context_t *contact = context;
    almanac_solar_transit_geometry_t geometry;

    if (!contact || !almanac_solar_transit_geometry(almanac, contact->body_id, jd, contact->observer, &geometry))
        return NAN;
    if (geometry.sun_altitude_degrees + geometry.sun_sd <= 0.0)
        return NAN;
    return geometry.separation;
}

static bool almanac_fill_solar_transit(almanac_t *almanac, almanac_body_id_t body_id, double jd,
    const almanac_observer_t *observer, almanac_solar_transit_t *out)
{
    almanac_solar_transit_geometry_t geometry;
    almanac_eclipse_contact_context_t contact = {body_id, observer, 0};
    static const double contact_step_days = 1.0 / 24.0;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !out)
        return false;
    if (!almanac_solar_transit_geometry(almanac, body_id, jd, observer, &geometry))
        return false;
    if (geometry.separation >= geometry.sun_sd + geometry.body_sd)
        return false;
    if (geometry.sun_altitude_degrees + geometry.sun_sd <= 0.0)
        return false;

    memset(out, 0, sizeof(*out));
    out->body_id = geometry.body_id;
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_transit_contact_residual, &contact, jd,
                                                       -1.0, 0.75, contact_step_days),
                               &out->first_contact);
    contact.contact_level = ALMANAC_CONTACT_LEVEL_INNER;
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_transit_contact_residual, &contact, jd,
                                                       -1.0, 0.75, contact_step_days),
                               &out->second_contact);
    almanac_event_time_from_jd(jd, &out->greatest_transit);
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_transit_contact_residual, &contact, jd,
                                                       1.0, 0.75, contact_step_days),
                               &out->third_contact);
    contact.contact_level = ALMANAC_CONTACT_LEVEL_OUTER;
    almanac_event_time_from_jd(almanac_find_contact_jd(almanac, almanac_solar_transit_contact_residual, &contact, jd,
                                                       1.0, 0.75, contact_step_days),
                               &out->fourth_contact);
    out->separation_degrees = geometry.separation;
    out->solar_semi_diameter_degrees = geometry.sun_sd;
    out->planet_semi_diameter_degrees = geometry.body_sd;
    out->chord_distance_fraction = geometry.separation / geometry.sun_sd;
    out->interior = geometry.separation <= geometry.sun_sd - geometry.body_sd;
    return true;
}

/* Return the transiting body for a solar transit event. */
almanac_body_id_t almanac_solar_transit_body_id(const almanac_solar_transit_t *event)
{
    return event ? event->body_id : ALMANAC_BODY_ID_UNKNOWN;
}

/* Copy a named time from a solar transit event. */
bool almanac_solar_transit_time(const almanac_solar_transit_t *event, almanac_event_time_kind_t time_kind,
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
            time = &event->greatest_transit;
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

/* Return the Sun-planet angular separation in degrees at greatest transit. */
double almanac_solar_transit_separation_degrees(const almanac_solar_transit_t *event)
{
    return event ? event->separation_degrees : NAN;
}

/* Return the Sun's apparent semi-diameter in degrees at greatest transit. */
double almanac_solar_transit_solar_semi_diameter_degrees(const almanac_solar_transit_t *event)
{
    return event ? event->solar_semi_diameter_degrees : NAN;
}

/* Return the planet's apparent semi-diameter in degrees at greatest transit. */
double almanac_solar_transit_planet_semi_diameter_degrees(const almanac_solar_transit_t *event)
{
    return event ? event->planet_semi_diameter_degrees : NAN;
}

/* Return the centre separation as a fraction of the solar semi-diameter. */
double almanac_solar_transit_chord_distance_fraction(const almanac_solar_transit_t *event)
{
    return event ? event->chord_distance_fraction : NAN;
}

/* Report whether the planet lies wholly within the solar disc at greatest transit. */
bool almanac_solar_transit_is_interior(const almanac_solar_transit_t *event)
{
    return event ? event->interior : false;
}

/* Find Mercury or Venus transits of the Sun by enum id. */
array_t *almanac_find_solar_transits_for_body(almanac_t *almanac, almanac_body_id_t body_id,
    const almanac_observer_t *observer, const datetime_t *start, const datetime_t *end)
{
    double reference_jd;
    double synodic_period_days;
    double start_jd;
    double end_jd;
    double previous_root = DBL_MAX;
    long first_cycle;
    long last_cycle;
    long cycle;
    array_t *events;

    if (!almanac) {
        almanac_set_error(almanac, "invalid solar transit request");
        return NULL;
    }
    if (body_id != ALMANAC_BODY_ID_MERCURY && body_id != ALMANAC_BODY_ID_VENUS) {
        almanac_set_error(almanac, "solar transits currently support MERCURY or VENUS only");
        return NULL;
    }
    if (!almanac_event_window_is_valid(almanac, start, end, &start_jd, &end_jd))
        return NULL;
    if (!almanac_observer_is_valid(almanac, observer))
        return NULL;
    if (body_id == ALMANAC_BODY_ID_MERCURY) {
        /*
         * ESAA 9.222 gives a 116-day mean synodic period.  This more precise
         * value and the 2019 transit epoch keep each inferior-conjunction
         * estimate close enough for rapid ephemeris refinement.
         */
        reference_jd = 2458799.139;
        synodic_period_days = 115.8774771;
    } else {
        reference_jd = 2456084.562;
        synodic_period_days = 583.921361;
    }
    events = array_create(sizeof(almanac_solar_transit_t), NULL, NULL);
    if (!events) {
        almanac_set_error(almanac, "failed to allocate solar transit array");
        return NULL;
    }

    first_cycle = (long)floor((start_jd - reference_jd) / synodic_period_days) - 1L;
    last_cycle = (long)ceil((end_jd - reference_jd) / synodic_period_days) + 1L;
    for (cycle = first_cycle; cycle <= last_cycle; ++cycle) {
        double estimate_jd = reference_jd + (double)cycle * synodic_period_days;
        double conjunction_jd;

        if (almanac_refine_body_sun_longitude_near(almanac, body_id, 0.0, estimate_jd, 8.0, &conjunction_jd)) {
            almanac_solar_transit_t transit;
            almanac_eclipse_contact_context_t contact = {body_id, observer, 0};
            double local_jd;

            if (conjunction_jd < start_jd - 1.0 || conjunction_jd > end_jd + 1.0)
                continue;
            if (previous_root != DBL_MAX && fabs(conjunction_jd - previous_root) < 1.0)
                continue;
            previous_root = conjunction_jd;
            local_jd = almanac_find_local_minimum_jd(almanac, almanac_solar_transit_metric, &contact, conjunction_jd,
                                                     0.75, 1.0 / 8.0, 9);
            if (local_jd < start_jd - 1e-9 || local_jd > end_jd + 1e-9)
                continue;
            if (almanac_fill_solar_transit(almanac, body_id, local_jd, observer, &transit) &&
                !array_add(events, &transit)) {
                array_destroy(events);
                almanac_set_error(almanac, "failed to append solar transit event");
                return NULL;
            }
        }
    }

    return events;
}

/* Find Mercury or Venus transits of the Sun by legacy body code. */
array_t *almanac_find_solar_transits(almanac_t *almanac, const char *body_code, const almanac_observer_t *observer,
    const datetime_t *start, const datetime_t *end)
{
    almanac_body_id_t body_id;

    if (!almanac || !body_code) {
        almanac_set_error(almanac, "invalid solar transit request");
        return NULL;
    }
    body_id = almanac_body_id_from_code(body_code);
    return almanac_find_solar_transits_for_body(almanac, body_id, observer, start, end);
}
