/**
 * @file lab_worker_plot_precision.c
 * @brief Range-aware working precision for isolated native Lab plotting requests.
 *
 * Ordinary curves use a 53-bit mantissa. Close bounds retain guard bits, and a
 * small independent set of higher-precision probes detects numerical instability.
 * Every pass reparses the authored formula: rounding an existing graph cannot
 * recover constants already evaluated at the wrong precision. This policy is
 * local to the Lab worker, not a change to sampler_t's precision-preserving API.
 */
#include <ctype.h>
#include <errno.h>
#include <string.h>

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static const size_t plot_precision_limit = 65536;

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static number_t lesser(number_t a, number_t b)
{
    return num_clone(num_lt(a, b) ? a : b);
}

static number_t greater(number_t a, number_t b)
{
    return num_clone(num_gt(a, b) ? a : b);
}

size_t lab_worker_plot_range_precision(number_t lower, number_t upper)
{
    NUM_SCOPE(numbers);
    number_t span = num_sub(upper, lower);
    number_t magnitude = greater(num_abs(lower), num_abs(upper));
    if (!finite_real(span) || !num_gt(span, NUM_ZERO))
        return 0;
    /* Fourteen subdivision bits plus guard bits keep neighbouring probes distinct. */
    long offset = num_get_exponent2(magnitude), width = num_get_exponent2(span);
    if (offset > width && (long double)offset - (long double)width > plot_precision_limit - 48)
        return plot_precision_limit + 1;
    size_t needed = offset > width ? (size_t)(offset - width) + 48 : 53;
    return needed > 53 ? needed : 53;
}

size_t lab_worker_plot_check_precision(number_t lower, number_t upper)
{
    NUM_SCOPE(numbers);
    /* Resolve loss against a unit-sized intermediate even when the whole range is tiny. */
    number_t magnitude = greater(num_abs(lower), num_abs(upper));
    long exponent = num_get_exponent2(magnitude);
    if (exponent >= 0)
        return 128;
    if (exponent < -(long)(plot_precision_limit - 64))
        return plot_precision_limit;
    size_t bits = (size_t)-exponent + 64;
    return bits > 128 ? bits : 128;
}

static number_t bound_value(const char *source)
{
    expr_t *expression = expr_from_string(source, NULL);
    number_t value = expression ? expr_eval(expression) : num_clone(NUM_NAN);
    expr_free(expression);
    return value;
}

/* Parse before rounding to sampling precision, including ranges below worksheet resolution. */
bool lab_worker_plot_bounds(const char *lower, const char *upper, number_t *lo, number_t *hi)
{
    while (isspace((unsigned char)*lower))
        ++lower;
    while (isspace((unsigned char)*upper))
        ++upper;
    size_t saved_precision = num_get_default_prec_bits();
    size_t length = strlen(lower) + strlen(upper);
    size_t bits = length < (plot_precision_limit - 128) / 4 ? 128 + length * 4 : plot_precision_limit;
    bool ok = false;
    *lo = *hi = NUM_NAN;
    for (;;) {
        num_set_default_prec_bits(bits);
        num_destroy(lo);
        num_destroy(hi);
        *lo = bound_value(lower);
        *hi = bound_value(upper);
        if (!finite_real(*lo) || !finite_real(*hi) || num_gt(*lo, *hi)) {
            errno = EINVAL;
            break;
        }
        size_t needed = lab_worker_plot_range_precision(*lo, *hi);
        if (needed && needed <= bits) {
            ok = true;
            break;
        }
        if (bits == plot_precision_limit) {
            errno = num_eq(*lo, *hi) ? EINVAL : ERANGE;
            break;
        }
        bits = bits > plot_precision_limit / 2 ? plot_precision_limit : bits * 2;
    }
    int saved_error = errno;
    num_set_default_prec_bits(saved_precision);
    errno = saved_error;
    return ok;
}

typedef struct {
    expr_t *expression, *axis;
    expr_t *second_derivative;
} plot_evaluation_t;

static sampler_status_t evaluate_plot(number_t parameter, number_t *x, number_t *y, void *argument)
{
    plot_evaluation_t *plot = argument;
    /* This freshly parsed graph is private: update its free binding, including uses inside binders.
     * Display-context snapshots deliberately do not enter summations or other scoped operations. */
    expr_set_val(plot->axis, parameter);
    *x = num_clone(parameter);
    *y = expr_eval(plot->expression);
    return SAMPLER_VALUE;
}

static sampler_status_t evaluate_plot_derivatives(number_t parameter, number_t *x, number_t *y,
                                                  number_t *dx, number_t *dy, number_t *ddx, number_t *ddy,
                                                  void *argument)
{
    plot_evaluation_t *plot = argument;
    expr_set_val(plot->axis, parameter);
    *x = num_clone(parameter);
    *dx = num_clone(NUM_ONE);
    *ddx = num_clone(NUM_ZERO);
    const expr_t *variables[] = {plot->axis};
    /* Reverse mode uses this private graph's actual binding node. Unsupported
     * pullbacks leave tangents unavailable; the ordinary value remains useful. */
    if (expr_eval_derivatives(plot->expression, 1, variables, y, dy) != 0)
        *y = expr_eval(plot->expression);
    if (plot->second_derivative)
        *ddy = expr_eval(plot->second_derivative);
    return SAMPLER_VALUE;
}

static sampler_t *sample_at(const char *source, const char *variable, number_t lower, number_t upper,
                             size_t bits, bool probes)
{
    num_set_default_prec_bits(bits);
    expr_bindings_t *bindings = NULL;
    expr_t *expression = expr_from_string(source, &bindings);
    expr_t *axis = bindings ? expr_bindings_get(bindings, variable) : NULL;
    number_t lo = num_as_inexact_real_prec(lower, bits), hi = num_as_inexact_real_prec(upper, bits);
    sampler_options_t options = sampler_options_default();
    options.intervals = probes ? 16 : 64;
    options.max_evaluations = probes ? 17 : 8193;
    options.method = probes ? SAMPLER_UNIFORM : SAMPLER_ADAPTIVE;
    options.max_depth = 8;
    options.absolute_tolerance = 1e-5;
    options.relative_tolerance = 0.002;
    plot_evaluation_t evaluation = {.expression = expression, .axis = axis};
    if (!probes && expression && axis)
        evaluation.second_derivative = expr_create_2nd_deriv(expression, axis, axis);
    sampler_t *samples = NULL;
    if (expression && axis)
        samples = probes ? sampler_new(evaluate_plot, &evaluation, lo, hi, &options) :
            sampler_new_with_derivatives(evaluate_plot_derivatives, &evaluation, lo, hi, &options);
    int saved_error = samples ? 0 : (errno ? errno : EINVAL);
    expr_free(evaluation.second_derivative);
    num_destroy(&hi);
    num_destroy(&lo);
    expr_free(expression);
    expr_bindings_free(bindings);
    errno = saved_error;
    return samples;
}

static number_t visible_value(number_t value, number_t ymin, number_t ymax)
{
    NUM_SCOPE(numbers);
    if (finite_real(ymin) && finite_real(ymax))
        return num_scope_detach(lesser(greater(value, ymin), ymax));
    return num_scope_detach(num_clone(value));
}

static bool probes_agree(const sampler_t *low, const sampler_t *high, number_t ymin, number_t ymax)
{
    NUM_SCOPE(numbers);
    number_t minimum = NUM_NAN, maximum = NUM_NAN;
    /* Seventeen ordered probes: bounded traversal, not a key lookup. */
    for (size_t i = 0; i < sampler_count(high); ++i) {
        const sampler_point_t *point = sampler_point(high, i);
        if (!point->valid)
            continue;
        number_t value = visible_value(point->y, ymin, ymax);
        minimum = num_is_nan(minimum) ? value : lesser(minimum, value);
        maximum = num_is_nan(maximum) ? value : greater(maximum, value);
    }
    number_t scale = num_sub(maximum, minimum);
    if (num_is_zero(scale))
        scale = num_abs(maximum);
    number_t tolerance = num_mul(scale, num_create_from_frac(1, 10000000));
    for (size_t i = 0; i < sampler_count(high); ++i) {
        const sampler_point_t *a = sampler_point(low, i), *b = sampler_point(high, i);
        if (a->valid != b->valid)
            return false;
        if (!a->valid)
            continue;
        number_t delta = num_abs(num_sub(visible_value(a->y, ymin, ymax), visible_value(b->y, ymin, ymax)));
        if (!num_le(delta, tolerance))
            return false;
    }
    return true;
}

/* Only this isolated worker temporarily changes the numerical default; every exit restores it. */
sampler_t *lab_worker_plot_samples(const char *source, const char *variable, number_t lower, number_t upper,
                                  number_t ymin, number_t ymax, size_t *precision_bits)
{
    size_t saved_precision = num_get_default_prec_bits();
    size_t bits = lab_worker_plot_range_precision(lower, upper);
    size_t vertical = finite_real(ymin) && finite_real(ymax) ? lab_worker_plot_range_precision(ymin, ymax) : 53;
    size_t range_check = lab_worker_plot_check_precision(lower, upper);
    if (vertical > bits)
        bits = vertical;
    sampler_t *result = NULL;
    if (!bits || bits > plot_precision_limit / 2) {
        errno = ERANGE;
        return NULL;
    }
    /* Offset validation points from the dyadic sampling grid, avoiding its common pole locations. */
    size_t bound_bits = num_get_effective_prec_bits(lower);
    if (bound_bits < num_get_effective_prec_bits(upper))
        bound_bits = num_get_effective_prec_bits(upper);
    if (bound_bits < bits + 64)
        bound_bits = bits + 64;
    num_set_default_prec_bits(bound_bits);
    number_t span = num_sub(upper, lower), divisor = num_create_from_long(34);
    number_t inset = num_div(span, divisor);
    number_t probe_lower = num_add(lower, inset), probe_upper = num_sub(upper, inset);
    num_destroy(&divisor);
    num_destroy(&span);
    num_destroy(&inset);
    for (;;) {
        size_t reference_bits = bits * 2;
        if (reference_bits < 128)
            reference_bits = 128;
        if (reference_bits < range_check)
            reference_bits = range_check;
        /* Use a few worksheet-precision checks, never that precision for every ordinary sample. */
        size_t check_bits = saved_precision < 4096 ? saved_precision : 4096;
        if (reference_bits < check_bits)
            reference_bits = check_bits;
        sampler_t *low = sample_at(source, variable, probe_lower, probe_upper, bits, true);
        sampler_t *high = low ? sample_at(source, variable, probe_lower, probe_upper, reference_bits, true) : NULL;
        bool agree = low && high && probes_agree(low, high, ymin, ymax);
        bool failed = !low || !high;
        sampler_free(high);
        sampler_free(low);
        if (failed)
            break;
        if (agree) {
            result = sample_at(source, variable, lower, upper, bits, false);
            if (result)
                *precision_bits = bits;
            break;
        }
        if (bits >= plot_precision_limit / 2) {
            errno = ERANGE;
            break;
        }
        bits = bits * 2 > plot_precision_limit / 2 ? plot_precision_limit / 2 : bits * 2;
    }
    int saved_error = errno;
    num_destroy(&probe_upper);
    num_destroy(&probe_lower);
    num_set_default_prec_bits(saved_precision);
    errno = saved_error;
    return result;
}
