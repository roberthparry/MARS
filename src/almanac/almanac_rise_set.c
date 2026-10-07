/**
 * @file almanac_rise_set.c
 * @brief Rise and set searches within a local civil day.
 *
 * Resolves local offsets and UTC search windows, accounts for horizon geometry, and locates body crossings. Sun
 * and Moon wrappers use the same observer-dependent machinery.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Local civil-day conversion and rise/set event searches. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"
#include "jurisdiction.h"

typedef struct almanac_rise_set_day_t {
    double start_jd;
    double end_jd;
    double offset_guess_hours;
    long local_jdn;
} almanac_rise_set_day_t;

typedef struct almanac_horizon_context_t {
    almanac_body_id_t body_id;
    const almanac_observer_t *observer;
} almanac_horizon_context_t;

static bool almanac_local_offset_for_jd(almanac_t *almanac, jurisdiction_t *jurisdiction, double local_jd,
    double *out_offset_hours)
{
    datetime_t *local_date;
    bool ok;

    if (!almanac || !jurisdiction || !out_offset_hours || !(local_jd == local_jd)) {
        almanac_set_error(almanac, "invalid local almanac time conversion");
        return false;
    }

    local_date = datetime_alloc();
    if (!local_date) {
        almanac_set_error(almanac, "failed to allocate local almanac datetime");
        return false;
    }
    if (!datetime_init_jd(local_date, local_jd)) {
        datetime_dealloc(local_date);
        almanac_set_error(almanac, "failed to initialise local almanac datetime");
        return false;
    }
    ok = jurisdict_default_gmt_offset(jurisdiction, local_date, out_offset_hours);
    datetime_dealloc(local_date);
    if (!ok) {
        const char *jurisdiction_error = jurisdict_last_error(jurisdiction);

        almanac_set_error(almanac,
                          jurisdiction_error ? jurisdiction_error : "failed to resolve jurisdiction GMT offset");
        return false;
    }
    return true;
}

static bool almanac_local_event_time_from_utc_jd(almanac_t *almanac, jurisdiction_t *jurisdiction, double utc_jd,
    double offset_guess_hours, almanac_event_time_t *out)
{
    double offset_hours = offset_guess_hours;
    double local_jd;
    int iteration;

    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    out->jd = NAN;
    if (!(utc_jd == utc_jd))
        return true;

    for (iteration = 0; iteration < 4; ++iteration) {
        double resolved_offset_hours;

        local_jd = utc_jd + offset_hours / 24.0;
        if (!almanac_local_offset_for_jd(almanac, jurisdiction, local_jd, &resolved_offset_hours)) {
            return false;
        }
        if (fabs(resolved_offset_hours - offset_hours) < 1e-9) {
            offset_hours = resolved_offset_hours;
            break;
        }
        offset_hours = resolved_offset_hours;
    }

    local_jd = utc_jd + offset_hours / 24.0;
    almanac_event_time_from_jds(utc_jd, local_jd, out);
    return true;
}

static bool almanac_local_day_utc_window_for_jdn(almanac_t *almanac, jurisdiction_t *jurisdiction, long jdn,
    double *out_start_jd, double *out_end_jd, double *out_offset_guess_hours)
{
    datetime_t *local_start;
    datetime_t *local_end;
    double offset_start_hours;
    double offset_end_hours;
    double local_start_jd;
    bool ok;

    if (!almanac || !jurisdiction || jdn == LONG_MAX || !out_start_jd || !out_end_jd || !out_offset_guess_hours) {
        almanac_set_error(almanac, "invalid almanac local day request");
        return false;
    }

    local_start = datetime_alloc();
    local_end = datetime_alloc();
    if (!local_start || !local_end) {
        datetime_dealloc(local_end);
        datetime_dealloc(local_start);
        almanac_set_error(almanac, "failed to allocate local day datetimes");
        return false;
    }

    ok = datetime_init_jdn(local_start, jdn) && datetime_init_jdn(local_end, jdn + 1L) &&
         jurisdict_default_gmt_offset(jurisdiction, local_start, &offset_start_hours) &&
         jurisdict_default_gmt_offset(jurisdiction, local_end, &offset_end_hours);
    if (!ok) {
        const char *jurisdiction_error = jurisdict_last_error(jurisdiction);

        datetime_dealloc(local_end);
        datetime_dealloc(local_start);
        almanac_set_error(almanac,
                          jurisdiction_error ? jurisdiction_error : "failed to resolve jurisdiction GMT offset");
        return false;
    }

    local_start_jd = (double)jdn - 0.5;
    *out_start_jd = local_start_jd - offset_start_hours / 24.0;
    *out_end_jd = local_start_jd + 1.0 - offset_end_hours / 24.0;
    *out_offset_guess_hours = offset_start_hours;

    datetime_dealloc(local_end);
    datetime_dealloc(local_start);
    return true;
}

static double almanac_horizon_dip_degrees(double elevation_metres)
{
    static const double earth_mean_radius_metres = 6371008.8;

    if (!(elevation_metres > 0.0))
        return 0.0;
    return almanac_radians_to_degrees(acos(earth_mean_radius_metres / (earth_mean_radius_metres + elevation_metres)));
}

static double almanac_body_horizon_residual(almanac_t *almanac, double jd, void *context)
{
    static const double standard_refraction_degrees = 34.0 / 60.0;
    almanac_horizon_context_t *horizon_context = context;
    almanac_horizon_geometry_t geometry;
    double horizon_dip;

    if (!horizon_context || !horizon_context->observer)
        return NAN;
    if (!almanac_body_horizon_geometry(almanac, horizon_context->body_id, horizon_context->observer, jd, &geometry)) {
        return NAN;
    }

    horizon_dip = almanac_horizon_dip_degrees(horizon_context->observer->elevation_metres);
    return geometry.altitude_degrees + geometry.semi_diameter_degrees + standard_refraction_degrees + horizon_dip;
}

static bool almanac_find_body_horizon_crossing(almanac_t *almanac, almanac_body_id_t body_id,
    const almanac_observer_t *observer, double start_jd, double end_jd, bool rising, double *out_jd)
{
    static const double step_days = 1.0 / 288.0;
    almanac_horizon_context_t context = {body_id, observer};
    double left_jd = start_jd;
    double left_value;
    double probe_jd;

    if (!almanac || !observer || !out_jd || !(end_jd > start_jd))
        return false;

    left_value = almanac_body_horizon_residual(almanac, left_jd, &context);
    if (!(left_value == left_value))
        return false;

    for (probe_jd = start_jd + step_days; probe_jd <= end_jd + 1e-12; probe_jd += step_days) {
        double right_jd = probe_jd > end_jd ? end_jd : probe_jd;
        double right_value = almanac_body_horizon_residual(almanac, right_jd, &context);

        if (!(right_value == right_value))
            return false;
        if ((rising && left_value <= 0.0 && right_value > 0.0) || (!rising && left_value >= 0.0 && right_value < 0.0)) {
            return almanac_bisect_event_residual(almanac, almanac_body_horizon_residual, &context, left_jd, right_jd,
                                                 out_jd);
        }
        left_jd = right_jd;
        left_value = right_value;
    }
    return false;
}

static bool almanac_find_solar_horizon_crossing(almanac_t *almanac, const almanac_rise_set_day_t *day,
    const almanac_observer_t *observer, bool rising, double *out_jd)
{
    static const double initial_radius_days = 10.0 / 1440.0;
    static const double max_radius_days = 8.0 / 24.0;
    almanac_horizon_context_t context = {ALMANAC_BODY_ID_SUN, observer};
    datetime_sun_status_t status = DATETIME_SUN_UNAVAILABLE;
    datetime_t *estimate;
    double estimate_utc_jd;
    double radius_days;

    if (!almanac || !day || !observer || !out_jd || !(day->end_jd > day->start_jd))
        return false;

    estimate = rising ? datetime_init_sunrise_checked(datetime_alloc(), day->local_jdn, observer->latitude_degrees,
                                                      observer->longitude_degrees, day->offset_guess_hours, &status)
                      : datetime_init_sunset_checked(datetime_alloc(), day->local_jdn, observer->latitude_degrees,
                                                     observer->longitude_degrees, day->offset_guess_hours, &status);
    if (!estimate)
        return false;
    estimate_utc_jd = datetime_jd(estimate) - day->offset_guess_hours / 24.0;
    datetime_dealloc(estimate);
    if (status != DATETIME_SUN_OK || !(estimate_utc_jd == estimate_utc_jd))
        return false;

    for (radius_days = initial_radius_days; radius_days <= max_radius_days + 1e-12; radius_days *= 2.0) {
        double left_jd = estimate_utc_jd - radius_days;
        double right_jd = estimate_utc_jd + radius_days;
        double left_value;
        double right_value;

        if (left_jd < day->start_jd)
            left_jd = day->start_jd;
        if (right_jd > day->end_jd)
            right_jd = day->end_jd;
        if (!(right_jd > left_jd))
            continue;

        left_value = almanac_body_horizon_residual(almanac, left_jd, &context);
        right_value = almanac_body_horizon_residual(almanac, right_jd, &context);
        if (!(left_value == left_value) || !(right_value == right_value))
            return false;
        if ((rising && left_value <= 0.0 && right_value > 0.0) || (!rising && left_value >= 0.0 && right_value < 0.0)) {
            return almanac_bisect_event_residual(almanac, almanac_body_horizon_residual, &context, left_jd, right_jd,
                                                 out_jd);
        }
    }

    return almanac_find_body_horizon_crossing(almanac, ALMANAC_BODY_ID_SUN, observer, day->start_jd, day->end_jd,
                                              rising, out_jd);
}

static almanac_rise_set_status_t almanac_body_rise_set_status_for_day(almanac_t *almanac, almanac_body_id_t body_id,
    const almanac_observer_t *observer, double start_jd, double end_jd)
{
    almanac_horizon_context_t context = {body_id, observer};
    double step_days = 1.0 / 48.0;
    double probe_jd;
    double max_value = -DBL_MAX;

    for (probe_jd = start_jd; probe_jd <= end_jd + 1e-12; probe_jd += step_days) {
        double value = almanac_body_horizon_residual(almanac, probe_jd, &context);

        if (!(value == value))
            return ALMANAC_RISE_SET_UNAVAILABLE;
        if (value > max_value)
            max_value = value;
    }
    return max_value > 0.0 ? ALMANAC_RISE_SET_NEVER_SETS : ALMANAC_RISE_SET_NEVER_RISES;
}

static void almanac_init_rise_set_event(almanac_rise_set_status_t missing_status, almanac_rise_set_event_t *event)
{
    if (!event)
        return;
    memset(event, 0, sizeof(*event));
    event->status = missing_status;
    event->time.jd = NAN;
    event->time.local_jd = NAN;
    event->azimuth_degrees = NAN;
}

static bool almanac_fill_rise_set_event(almanac_t *almanac, jurisdiction_t *jurisdiction, almanac_body_id_t body_id,
    const almanac_observer_t *observer, double utc_jd, double offset_guess_hours,
    almanac_rise_set_status_t missing_status, almanac_rise_set_event_t *event)
{
    almanac_horizon_geometry_t geometry;

    if (!event)
        return false;
    almanac_init_rise_set_event(missing_status, event);
    if (!(utc_jd == utc_jd))
        return true;

    if (!almanac_body_horizon_geometry(almanac, body_id, observer, utc_jd, &geometry))
        return false;
    event->status = ALMANAC_RISE_SET_OK;
    event->azimuth_degrees = geometry.azimuth_degrees;
    return almanac_local_event_time_from_utc_jd(almanac, jurisdiction, utc_jd, offset_guess_hours, &event->time);
}

static bool almanac_prepare_rise_set_window(almanac_t *almanac, jurisdiction_t *jurisdiction, const datetime_t *date,
    const almanac_observer_t *observer, const char *error_message, almanac_rise_set_day_t *day)
{
    long local_jdn;

    if (!almanac || !jurisdiction || !date || !observer || !day) {
        almanac_set_error(almanac, error_message);
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    local_jdn = datetime_jdn(date);
    if (local_jdn == LONG_MAX)
        return false;
    memset(day, 0, sizeof(*day));
    day->local_jdn = local_jdn;
    return almanac_local_day_utc_window_for_jdn(almanac, jurisdiction, day->local_jdn, &day->start_jd, &day->end_jd,
                                                &day->offset_guess_hours);
}

/* Find accurate local rise and set for one supported body and day. */
bool almanac_body_rise_set(almanac_t *almanac, jurisdiction_t *jurisdiction, almanac_body_id_t body_id,
    const datetime_t *date, const almanac_observer_t *observer, almanac_rise_set_t *out)
{
    almanac_rise_set_day_t day;
    double rise_jd = NAN;
    double set_jd = NAN;
    bool has_rise;
    bool has_set;
    almanac_rise_set_status_t missing_status = ALMANAC_RISE_SET_UNAVAILABLE;
    const char *error_message;

    if (!almanac || !jurisdiction || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !date ||
        !observer || !out) {
        almanac_set_error(almanac, "invalid almanac rise/set request");
        return false;
    }
    memset(out, 0, sizeof(*out));
    almanac_init_rise_set_event(ALMANAC_RISE_SET_UNAVAILABLE, &out->rise);
    almanac_init_rise_set_event(ALMANAC_RISE_SET_UNAVAILABLE, &out->set);

    error_message = body_id == ALMANAC_BODY_ID_SUN    ? "invalid almanac sunrise/sunset request"
                    : body_id == ALMANAC_BODY_ID_MOON ? "invalid almanac moonrise/moonset request"
                                                      : "invalid almanac rise/set request";
    if (!almanac_prepare_rise_set_window(almanac, jurisdiction, date, observer, error_message, &day)) {
        return false;
    }

    if (body_id == ALMANAC_BODY_ID_SUN) {
        has_rise = almanac_find_solar_horizon_crossing(almanac, &day, observer, true, &rise_jd);
        has_set = almanac_find_solar_horizon_crossing(almanac, &day, observer, false, &set_jd);
    } else {
        has_rise =
            almanac_find_body_horizon_crossing(almanac, body_id, observer, day.start_jd, day.end_jd, true, &rise_jd);
        has_set =
            almanac_find_body_horizon_crossing(almanac, body_id, observer, day.start_jd, day.end_jd, false, &set_jd);
    }

    if (!has_rise || !has_set) {
        if (body_id != ALMANAC_BODY_ID_SUN && (has_rise || has_set)) {
            missing_status = ALMANAC_RISE_SET_NOT_ON_DATE;
        } else {
            missing_status = almanac_body_rise_set_status_for_day(almanac, body_id, observer, day.start_jd, day.end_jd);
        }
    }

    return almanac_fill_rise_set_event(almanac, jurisdiction, body_id, observer, has_rise ? rise_jd : NAN,
                                       day.offset_guess_hours, missing_status, &out->rise) &&
           almanac_fill_rise_set_event(almanac, jurisdiction, body_id, observer, has_set ? set_jd : NAN,
                                       day.offset_guess_hours, missing_status, &out->set);
}

/* Find accurate local sunrise and sunset for one observer and day. */
bool almanac_sunrise_sunset(almanac_t *almanac, jurisdiction_t *jurisdiction, const datetime_t *date,
    const almanac_observer_t *observer, almanac_sun_times_t *out)
{
    return almanac_body_rise_set(almanac, jurisdiction, ALMANAC_BODY_ID_SUN, date, observer, out);
}

/* Find accurate local moonrise and moonset for one observer and day. */
bool almanac_moonrise_moonset(almanac_t *almanac, jurisdiction_t *jurisdiction, const datetime_t *date,
    const almanac_observer_t *observer, almanac_moon_times_t *out)
{
    return almanac_body_rise_set(almanac, jurisdiction, ALMANAC_BODY_ID_MOON, date, observer, out);
}
