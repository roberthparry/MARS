/**
 * @file diffequ_sample.c
 * @brief Bounded real trajectories of scalar first- and second-order differential equations.
 *
 * Isolates the highest derivative through native algebra, substitutes private coordinate/state leaves, and
 * integrates outwards from a supplied initial point with adaptive RK4 step doubling. Uniform output retains
 * gaps after a singularity. This visualisation path does not depend on a closed-form symbolic solution.
 */
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "sampler-t.h"
#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

typedef struct {
    expr_t *rate, *coordinate, *position, *velocity;
    unsigned order;
    size_t evaluations, limit;
    double absolute, relative;
    bool failed;
} trajectory_t;

typedef struct {
    double *values;
    size_t next;
} trajectory_output_t;

static bool real_value(number_t number, double *out)
{
    if (!num_is_real(number) || !num_is_finite(number)) return false;
    *out = num_to_double(number);
    return isfinite(*out);
}

static bool evaluate(trajectory_t *trajectory, double t, const double state[2], double out[2])
{
    if (trajectory->evaluations >= trajectory->limit) {
        errno = E2BIG; trajectory->failed = true; return false;
    }
    if (!computation_step_current("diffequation.trajectory")) {
        trajectory->failed = true; return false;
    }
    ++trajectory->evaluations;
    expr_t *symbols[] = {trajectory->coordinate, trajectory->position, trajectory->velocity};
    double values[] = {t, state[0], state[1]};
    for (size_t i = 0; i < 3u; ++i) {
        number_t value = num_create_from_double(values[i]);
        expr_set_val(symbols[i], value);
        num_destroy(&value);
    }
    number_t result = expr_eval(trajectory->rate);
    double rate = NAN;
    bool ok = real_value(result, &rate);
    num_destroy(&result);
    out[0] = trajectory->order == 2u ? state[1] : rate;
    out[1] = trajectory->order == 2u ? rate : 0;
    return ok;
}

static bool rk4(trajectory_t *trajectory, double t, const double state[2], double h, double out[2])
{
    double slopes[4][2], temporary[2];
    if (!evaluate(trajectory, t, state, slopes[0])) return false;
    for (size_t stage = 1; stage < 4u; ++stage) {
        double fraction = stage == 3u ? 1 : 0.5;
        for (size_t i = 0; i < 2u; ++i)
            temporary[i] = state[i] + h * fraction * slopes[stage - 1u][i];
        if (!evaluate(trajectory, t + h * fraction, temporary, slopes[stage])) return false;
    }
    for (size_t i = 0; i < 2u; ++i) {
        out[i] = state[i] + h * (slopes[0][i] + 2 * slopes[1][i] + 2 * slopes[2][i] + slopes[3][i]) / 6;
        if (!isfinite(out[i])) return false;
    }
    return true;
}

static bool advance(trajectory_t *trajectory, double *t, double state[2], double target)
{
    double h = target - *t;
    while (target != *t) {
        if (fabs(h) > fabs(target - *t)) h = target - *t;
        if (*t + h == *t || fabs(h) <= DBL_EPSILON * fmax(1, fabs(*t))) return false;
        double whole[2], half[2], fine[2], error = 0;
        bool ok = rk4(trajectory, *t, state, h, whole) &&
                  rk4(trajectory, *t, state, h / 2, half) &&
                  rk4(trajectory, *t + h / 2, half, h / 2, fine);
        if (trajectory->failed) return false;
        if (ok) {
            for (size_t i = 0; i < trajectory->order; ++i) {
                double scale = trajectory->absolute + trajectory->relative * fmax(fabs(state[i]), fabs(fine[i]));
                error = fmax(error, fabs(fine[i] - whole[i]) / (15 * fmax(scale, DBL_MIN)));
            }
        }
        if (ok && error <= 1) {
            *t = h == target - *t ? target : *t + h;
            for (size_t i = 0; i < 2u; ++i) state[i] = fine[i] + (fine[i] - whole[i]) / 15;
            h *= error > 0 ? fmin(2, fmax(0.2, 0.9 * pow(error, -0.2))) : 2;
        } else {
            h *= ok ? fmax(0.1, 0.9 * pow(error, -0.2)) : 0.25;
        }
    }
    return true;
}

static sampler_status_t emit(number_t parameter, number_t *x, number_t *y, void *argument)
{
    trajectory_output_t *output = argument;
    double value = output->values[output->next++];
    *x = num_clone(parameter);
    *y = isfinite(value) ? num_create_from_double(value) : NUM_NAN;
    return isfinite(value) ? SAMPLER_VALUE : SAMPLER_GAP;
}

static bool prepare(trajectory_t *trajectory, const diffequ_t *de)
{
    if (de_independent_count(de) != 1u) { errno = ENOTSUP; return false; }
    const expr_t *coordinate = de_independent_at(de, 0u), *dependent = NULL, *first = NULL, *second = NULL;
    const equation_t *equation = de_equation(de);
    expr_t *residual = expr_sub(equ_lhs(equation), equ_rhs(equation));
    size_t order = 0;
    expr_t *coefficient = NULL, *remainder = NULL, *negative = NULL, *rate = NULL, *velocity_rate = NULL;
    expr_t *position_rate = NULL;
    bool ok = residual && de_find_derivatives(residual, coordinate, &dependent, &first, &second, &order) &&
        order >= 1u && order <= 2u && dependent && expr_is_variable(dependent) &&
        de_linear_decompose(residual, order == 2u ? second : first, &coefficient, &remainder) &&
        !expr_is_exact_zero(coefficient);
    if (!ok) {
        expr_free(remainder); expr_free(coefficient); expr_free(residual);
        errno = ENOTSUP;
        return false;
    }
    trajectory->order = (unsigned)order;
    trajectory->coordinate = expr_new_var(NUM_NAN);
    trajectory->position = expr_new_var(NUM_NAN);
    trajectory->velocity = expr_new_var(NUM_NAN);
    bool allocated = trajectory->coordinate && trajectory->position && trajectory->velocity;
    negative = allocated ? expr_neg(remainder) : NULL;
    rate = negative ? expr_div(negative, coefficient) : NULL;
    velocity_rate = rate ? (first && order == 2u ? expr_substitute(rate, first, trajectory->velocity)
                                                               : expr_clone(rate)) : NULL;
    position_rate = velocity_rate ? expr_substitute(velocity_rate, dependent, trajectory->position) : NULL;
    expr_t *coordinate_rate = position_rate ? expr_substitute(position_rate, coordinate, trajectory->coordinate) : NULL;
    expr_t *states = allocated ? expr_add(trajectory->position, trajectory->velocity) : NULL;
    expr_t *symbols = states ? expr_add(trajectory->coordinate, states) : NULL;
    trajectory->rate = coordinate_rate && symbols ? expr_clone_linked_symbols(coordinate_rate, symbols) : NULL;
    expr_free(symbols); expr_free(states); expr_free(coordinate_rate);
    ok = ok && trajectory->rate && trajectory->position && trajectory->velocity && trajectory->coordinate;
    if (!ok) errno = ENOMEM;
    expr_free(position_rate); expr_free(velocity_rate); expr_free(rate); expr_free(negative);
    expr_free(remainder); expr_free(coefficient); expr_free(residual);
    return ok;
}

/* Sample an isolated native ODE with bounded numerical work; the caller's problem remains unchanged. */
sampler_t *de_sample_trajectory(const diffequ_t *de, number_t initial_coordinate, number_t initial_value,
    number_t initial_slope, number_t lower, number_t upper, const sampler_options_t *options)
{
    sampler_options_t settings = options ? *options : sampler_options_default();
    double origin, value, slope = 0, lo, hi;
    if (!de || !real_value(initial_coordinate, &origin) || !real_value(initial_value, &value) ||
        !real_value(lower, &lo) || !real_value(upper, &hi) || !(lo < hi) || origin < lo || origin > hi ||
        !settings.intervals || settings.intervals > 65536u || settings.max_evaluations <= settings.intervals ||
        !isfinite(settings.absolute_tolerance) || !isfinite(settings.relative_tolerance) ||
        settings.absolute_tolerance < 0 || settings.relative_tolerance < 0 ||
        settings.absolute_tolerance + settings.relative_tolerance == 0 || !isfinite(hi - lo)) {
        errno = EINVAL; return NULL;
    }
    trajectory_t trajectory = {.limit = settings.max_evaluations, .absolute = settings.absolute_tolerance,
                               .relative = settings.relative_tolerance};
    sampler_t *samples = NULL;
    double *values = NULL;
    size_t precision = num_get_default_prec_bits();
    num_set_default_prec_bits(53u);
    if (!prepare(&trajectory, de)) goto cleanup;
    if (trajectory.order == 2u && !real_value(initial_slope, &slope)) { errno = EINVAL; goto cleanup; }
    double state[2] = {value, slope}, rate[2];
    if (!evaluate(&trajectory, origin, state, rate)) {
        if (!trajectory.failed) errno = ENODATA;
        goto cleanup;
    }
    values = malloc((settings.intervals + 1u) * sizeof *values);
    if (!values) goto cleanup;
    for (size_t i = 0; i <= settings.intervals; ++i) values[i] = NAN;
    size_t pivot = (size_t)floor((origin - lo) / (hi - lo) * settings.intervals);
    if (pivot > settings.intervals) pivot = settings.intervals;
    for (unsigned direction = 0; direction < 2u && !trajectory.failed; ++direction) {
        double t = origin;
        state[0] = value; state[1] = slope;
        size_t count = direction ? settings.intervals - pivot : pivot + 1u;
        for (size_t j = 0; j < count; ++j) {
            size_t i = direction ? pivot + 1u + j : pivot - j;
            double target = i == settings.intervals ? hi : lo + (hi - lo) * ((double)i / settings.intervals);
            if (target != t && !advance(&trajectory, &t, state, target)) break;
            values[i] = state[0];
        }
    }
    if (trajectory.failed) goto cleanup;
    trajectory_output_t output = {.values = values};
    settings.method = SAMPLER_UNIFORM;
    samples = sampler_new(emit, &output, lower, upper, &settings);
cleanup:
    free(values); expr_free(trajectory.rate); expr_free(trajectory.velocity);
    expr_free(trajectory.position); expr_free(trajectory.coordinate);
    num_set_default_prec_bits(precision);
    return samples;
}
