/* Phase classification, conjunction refinement and exact Moon phases. */
#include <float.h>
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"

/* Derive illuminated fraction and phase classification for a body entry. */
bool almanac_phase_details(const almanac_entry_t *body, almanac_phase_details_t *out)
{
    double phase_angle_radians;
    double illuminated_fraction;

    if (!body || !out)
        return false;

    memset(out, 0, sizeof(*out));
    out->phase_angle_degrees = body->phase_angle_degrees;
    out->illuminated_fraction = NAN;
    out->phase_class = ALMANAC_PHASE_UNKNOWN;

    if (!(body->phase_angle_degrees == body->phase_angle_degrees))
        return true;

    phase_angle_radians = almanac_degrees_to_radians(body->phase_angle_degrees);
    illuminated_fraction = 0.5 * (1.0 + cos(phase_angle_radians));
    if (illuminated_fraction < 0.0)
        illuminated_fraction = 0.0;
    if (illuminated_fraction > 1.0)
        illuminated_fraction = 1.0;
    out->illuminated_fraction = illuminated_fraction;

    if (body->phase_angle_degrees <= 5.0)
        out->phase_class = ALMANAC_PHASE_FULL;
    else if (body->phase_angle_degrees >= 175.0)
        out->phase_class = ALMANAC_PHASE_NEW;
    else if (fabs(body->phase_angle_degrees - 90.0) <= 10.0)
        out->phase_class = ALMANAC_PHASE_QUARTER;
    else if (illuminated_fraction < 0.5)
        out->phase_class = ALMANAC_PHASE_CRESCENT;
    else
        out->phase_class = ALMANAC_PHASE_GIBBOUS;
    return true;
}

static double almanac_body_sun_longitude_residual_degrees(almanac_t *almanac, almanac_body_id_t body_id, double jd,
    double target_degrees)
{
    almanac_state_t body_state;
    almanac_state_t sun_state;
    double body_longitude;
    double sun_longitude;

    if (!almanac_state_for_body_at_jd(almanac, body_id, jd, &body_state) ||
        !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun_state)) {
        return NAN;
    }
    if (!body_state.has_ecliptic_vector || !sun_state.has_ecliptic_vector)
        return NAN;
    body_longitude = almanac_ecliptic_longitude_degrees(&body_state.geocentric_ecliptic_au);
    sun_longitude = almanac_ecliptic_longitude_degrees(&sun_state.geocentric_ecliptic_au);
    return almanac_normalize_degrees_signed((body_longitude - sun_longitude) - target_degrees);
}

static double almanac_moon_phase_target_degrees(almanac_moon_phase_kind_t kind)
{
    static const double target_degrees_by_kind[ALMANAC_MOON_PHASE_LAST_QUARTER + 1] = {
        [ALMANAC_MOON_PHASE_NEW]           = 0.0,
        [ALMANAC_MOON_PHASE_FIRST_QUARTER] = 90.0,
        [ALMANAC_MOON_PHASE_FULL]          = 180.0,
        [ALMANAC_MOON_PHASE_LAST_QUARTER]  = 270.0
    };

    if (kind < ALMANAC_MOON_PHASE_NEW || kind > ALMANAC_MOON_PHASE_LAST_QUARTER)
        return NAN;
    return target_degrees_by_kind[kind];
}

double almanac_mean_moon_phase_jd(double k)
{
    double T = k / 1236.85;

    return 2451550.09766 + 29.530588861 * k + 0.00015437 * T * T - 0.000000150 * T * T * T +
           0.00000000073 * T * T * T * T;
}

static double almanac_moon_argument_latitude_radians(double k)
{
    double T = k / 1236.85;
    double degrees =
        160.7108 + 390.67050284 * k - 0.0016118 * T * T - 0.00000227 * T * T * T + 0.000000011 * T * T * T * T;

    return almanac_degrees_to_radians(almanac_normalize_degrees(degrees));
}

bool almanac_eclipse_candidate_near_node(double k)
{
    /*
     * ESAA 8.22 requires conjunction or opposition near a lunar node.
     * The 0.36 limit is deliberately conservative (about 21 degrees in
     * argument of latitude); exact geometry remains the final test.
     */
    return fabs(sin(almanac_moon_argument_latitude_radians(k))) <= 0.36;
}

bool almanac_refine_body_sun_longitude_near(almanac_t *almanac, almanac_body_id_t body_id, double target_degrees,
    double estimate_jd, double max_step_days, double *out_jd)
{
    static const double derivative_step_days = 1.0 / 48.0;
    double jd = estimate_jd;
    int iteration;

    if (!almanac || !out_jd || !(max_step_days > 0.0))
        return false;
    for (iteration = 0; iteration < 12; ++iteration) {
        double value = almanac_body_sun_longitude_residual_degrees(almanac, body_id, jd, target_degrees);
        double before;
        double after;
        double slope;
        double correction;

        if (!(value == value))
            return false;
        if (fabs(value) < 1e-9) {
            *out_jd = jd;
            return true;
        }
        before =
            almanac_body_sun_longitude_residual_degrees(almanac, body_id, jd - derivative_step_days, target_degrees);
        after =
            almanac_body_sun_longitude_residual_degrees(almanac, body_id, jd + derivative_step_days, target_degrees);
        if (!(before == before) || !(after == after))
            return false;
        slope = almanac_normalize_degrees_signed(after - before) / (2.0 * derivative_step_days);
        if (fabs(slope) < 1e-9)
            return false;
        correction = value / slope;
        if (correction > max_step_days)
            correction = max_step_days;
        else if (correction < -max_step_days)
            correction = -max_step_days;
        jd -= correction;
        if (fabs(correction) < 1e-9) {
            *out_jd = jd;
            return true;
        }
    }
    if (fabs(almanac_body_sun_longitude_residual_degrees(almanac, body_id, jd, target_degrees)) >= 1e-7) {
        return false;
    }
    *out_jd = jd;
    return true;
}

/* Find the next exact Moon phase after a civil moment. */
bool almanac_next_moon_phase_exact(almanac_t *almanac, const datetime_t *after, almanac_moon_phase_kind_t kind,
    almanac_moon_phase_event_t *out)
{
    static const double synodic_month_days = 29.530588861;
    static const double base_new_moon_jd = 2451550.09766;
    double target_degrees;
    double phase_fraction;
    double after_jd;
    double k;
    double guess_jd;
    double root_jd;
    int attempt;
    almanac_entry_t moon_entry;
    almanac_phase_details_t phase_details;

    if (!almanac || !after || !out) {
        almanac_set_error(almanac, "invalid exact moon phase request");
        return false;
    }
    target_degrees = almanac_moon_phase_target_degrees(kind);
    if (!(target_degrees == target_degrees)) {
        almanac_set_error(almanac, "unsupported Moon phase kind");
        return false;
    }
    after_jd = datetime_jd(after);
    if (after_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian date for Moon phase search");
        return false;
    }

    phase_fraction = target_degrees / 360.0;
    k = floor((after_jd - base_new_moon_jd) / synodic_month_days - phase_fraction) + 1.0 + phase_fraction;

    root_jd = NAN;
    for (attempt = 0; attempt < 2; ++attempt) {
        guess_jd = almanac_mean_moon_phase_jd(k);
        if (!almanac_refine_body_sun_longitude_near(almanac, ALMANAC_BODY_ID_MOON, target_degrees, guess_jd, 2.0,
                                                    &root_jd)) {
            almanac_set_error(almanac, "failed to bracket exact Moon phase");
            return false;
        }
        if (root_jd > after_jd + 1e-9)
            break;
        k += 1.0;
    }
    if (!(root_jd > after_jd + 1e-9)) {
        almanac_set_error(almanac, "failed to bracket exact Moon phase");
        return false;
    }
    if (!almanac_entry_fill_at_jd(almanac, ALMANAC_BODY_ID_MOON, root_jd, &moon_entry) ||
        !almanac_phase_details(&moon_entry, &phase_details)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->kind = kind;
    out->time.valid = true;
    out->time.jd = root_jd;
    out->time.local_jd = root_jd;
    out->phase_angle_degrees = moon_entry.phase_angle_degrees;
    out->illuminated_fraction = phase_details.illuminated_fraction;
    return true;
}
