/**
 * @file almanac_events.c
 * @brief Shared astronomical event search and time refinement.
 *
 * Validates event windows and converts numerical event times into result records. Bisection, contact searches and
 * local minimisation support the specialised eclipse, transit and phase searches.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Event times, search windows and shared numerical refinement. */
#include <float.h>
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"

bool almanac_event_window_is_valid(almanac_t *almanac, const datetime_t *start, const datetime_t *end,
    double *out_start_jd, double *out_end_jd)
{
    double start_jd;
    double end_jd;

    if (!almanac || !start || !end || !out_start_jd || !out_end_jd) {
        almanac_set_error(almanac, "invalid almanac event window");
        return false;
    }
    start_jd = datetime_jd(start);
    end_jd = datetime_jd(end);
    if (start_jd == DBL_MAX || end_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian date for almanac event window");
        return false;
    }
    if (end_jd < start_jd) {
        almanac_set_error(almanac, "almanac event window end precedes start");
        return false;
    }
    *out_start_jd = start_jd;
    *out_end_jd = end_jd;
    return true;
}

void almanac_event_time_from_jds(double jd, double local_jd, almanac_event_time_t *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->jd = NAN;
    out->local_jd = NAN;
    if (!(jd == jd) || !(local_jd == local_jd))
        return;

    out->valid = true;
    out->jd = jd;
    out->local_jd = local_jd;
}

void almanac_event_time_from_jd(double jd, almanac_event_time_t *out)
{
    almanac_event_time_from_jds(jd, jd, out);
}

/* One observer location for horizon-style almanac calculations. */
bool almanac_event_time_datetime(const almanac_event_time_t *event_time, datetime_t *out)
{
    if (!event_time || !event_time->valid || !out || !(event_time->local_jd == event_time->local_jd)) {
        return false;
    }
    return datetime_init_jd(out, event_time->local_jd) != NULL;
}

bool almanac_bisect_event_residual(almanac_t *almanac, almanac_event_residual_fn residual, void *context, double left_jd,
    double right_jd, double *out_jd)
{
    double left_value;
    double right_value;
    bool last_replaced_left = false;
    bool last_replaced_right = false;
    int iteration;

    if (!residual || !out_jd)
        return false;
    left_value = residual(almanac, left_jd, context);
    right_value = residual(almanac, right_jd, context);
    if (!(left_value == left_value) || !(right_value == right_value))
        return false;
    if (left_value == 0.0) {
        *out_jd = left_jd;
        return true;
    }
    if (right_value == 0.0) {
        *out_jd = right_jd;
        return true;
    }
    if (left_value * right_value > 0.0)
        return false;

    /*
     * ESAA 8.422 recommends inverse interpolation of a contact
     * discriminant.  Illinois regula falsi keeps the root bracketed like
     * bisection, but normally converges in a handful of ephemeris samples.
     */
    for (iteration = 0; iteration < 24; ++iteration) {
        double candidate_jd = (left_jd * right_value - right_jd * left_value) / (right_value - left_value);
        double candidate_value;

        if (!(candidate_jd > left_jd && candidate_jd < right_jd))
            candidate_jd = 0.5 * (left_jd + right_jd);
        candidate_value = residual(almanac, candidate_jd, context);
        if (!(candidate_value == candidate_value))
            return false;
        if (fabs(candidate_value) < 1e-9 || fabs(right_jd - left_jd) < 1e-8) {
            *out_jd = candidate_jd;
            return true;
        }
        if (left_value * candidate_value <= 0.0) {
            right_jd = candidate_jd;
            right_value = candidate_value;
            if (last_replaced_right)
                left_value *= 0.5;
            last_replaced_right = true;
            last_replaced_left = false;
        } else {
            left_jd = candidate_jd;
            left_value = candidate_value;
            if (last_replaced_left)
                right_value *= 0.5;
            last_replaced_left = true;
            last_replaced_right = false;
        }
    }

    *out_jd = 0.5 * (left_jd + right_jd);
    return true;
}

double almanac_find_contact_jd(almanac_t *almanac, almanac_event_residual_fn residual, void *context, double greatest_jd,
    double direction, double max_span_days, double step_days)
{
    double previous_jd = greatest_jd;
    double previous_value;
    double offset;

    if (!residual || !(direction == -1.0 || direction == 1.0) || !(max_span_days > 0.0) || !(step_days > 0.0)) {
        return NAN;
    }
    previous_value = residual(almanac, previous_jd, context);
    if (!(previous_value == previous_value) || previous_value > 0.0)
        return NAN;

    for (offset = step_days; offset <= max_span_days + 1e-9; offset += step_days) {
        double probe_jd = greatest_jd + direction * offset;
        double probe_value = residual(almanac, probe_jd, context);

        if (!(probe_value == probe_value))
            return NAN;
        if (probe_value > 0.0) {
            double contact_jd;
            double left_jd = direction < 0.0 ? probe_jd : previous_jd;
            double right_jd = direction < 0.0 ? previous_jd : probe_jd;

            if (almanac_bisect_event_residual(almanac, residual, context, left_jd, right_jd, &contact_jd)) {
                return contact_jd;
            }
            return NAN;
        }
        previous_jd = probe_jd;
        previous_value = probe_value;
    }

    (void)previous_value;
    return NAN;
}

double almanac_find_local_minimum_jd(almanac_t *almanac, almanac_event_metric_fn metric, void *context, double centre_jd,
    double half_span_days, double sample_step_days, int refinement_iterations)
{
    double best_jd = centre_jd;
    double best_value;
    double probe;
    double step;
    int iteration;

    if (!metric || !(half_span_days > 0.0) || !(sample_step_days > 0.0))
        return centre_jd;
    best_value = metric(almanac, centre_jd, context);
    if (!(best_value == best_value))
        best_value = DBL_MAX;

    for (probe = centre_jd - half_span_days; probe <= centre_jd + half_span_days + 1e-9; probe += sample_step_days) {
        double value = metric(almanac, probe, context);

        if (value == value && value < best_value) {
            best_value = value;
            best_jd = probe;
        }
    }
    if (best_value == DBL_MAX)
        return centre_jd;

    /*
     * ESAA 8.42 obtains greatest eclipse by rapidly interpolating the
     * minimum of the squared separation.  Three-point parabolic
     * interpolation is the scalar equivalent here.  The phase/conjunction
     * estimate has already put us close to the event, so this converges in
     * a handful of ephemeris evaluations instead of a long golden-section
     * search.
     */
    step = fmin(sample_step_days, fmin(best_jd - (centre_jd - half_span_days), centre_jd + half_span_days - best_jd));
    if (!(step > 0.0))
        return best_jd;

    for (iteration = 0; iteration < refinement_iterations && step > 1e-10; ++iteration) {
        double left_value = metric(almanac, best_jd - step, context);
        double centre_value = metric(almanac, best_jd, context);
        double right_value = metric(almanac, best_jd + step, context);
        double denominator;
        double offset;
        double candidate_jd;
        double candidate_value;

        if (!(left_value == left_value) || !(centre_value == centre_value) || !(right_value == right_value)) {
            step *= 0.25;
            continue;
        }
        left_value *= left_value;
        centre_value *= centre_value;
        right_value *= right_value;
        denominator = left_value - 2.0 * centre_value + right_value;
        if (!(denominator > DBL_EPSILON)) {
            step *= 0.25;
            continue;
        }
        offset = 0.5 * step * (left_value - right_value) / denominator;
        if (offset > step)
            offset = step;
        else if (offset < -step)
            offset = -step;
        candidate_jd = best_jd + offset;
        candidate_value = metric(almanac, candidate_jd, context);
        if (candidate_value == candidate_value) {
            best_jd = candidate_jd;
            best_value = candidate_value;
        }
        step *= 0.25;
    }

    return best_jd;
}
