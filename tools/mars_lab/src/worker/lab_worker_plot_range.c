/**
 * @file lab_worker_plot_range.c
 * @brief Expression-dependent automatic curve and surface ranges for the native Lab.
 *
 * Bounded probes search several coordinate scales for finite real, visibly varying
 * values. Invalid outer margins are trimmed only at unspecified endpoints. This is
 * a viewing heuristic, not a proof of the complete domain or a global feature search;
 * unresolved parameters, isolated values and missed narrow features require bounds.
 * All work uses a private expression graph and preserves worksheet precision/bindings.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static bool blank(const char *text)
{
    while (isspace((unsigned char)*text))
        ++text;
    return !*text;
}

static number_t endpoint(const char *text)
{
    expr_t *value = expr_from_string(text, NULL);
    number_t result = value ? expr_eval(value) : num_clone(NUM_NAN);
    expr_free(value);
    return result;
}

static number_t coordinate(number_t lo, number_t hi, unsigned index, unsigned count)
{
    NUM_SCOPE(numbers);
    number_t value = num_add(lo, num_mul(num_sub(hi, lo), num_create_from_frac(index, count - 1)));
    return num_scope_detach(value);
}

static bool candidate(expr_t *formula, expr_t *const axes[2], unsigned dimensions,
                      number_t bounds[4], const bool automatic[4], bool constant)
{
    unsigned side = dimensions == 2 ? 7 : 33, rows = dimensions == 2 ? side : 1;
    unsigned minimum[] = {side, side}, maximum[] = {0, 0}, finite = 0;
    double values[49], peak = 0;
    for (unsigned row = 0; row < rows; ++row) {
        NUM_SCOPE(row_numbers);
        if (dimensions == 2)
            expr_set_val(axes[1], coordinate(bounds[2], bounds[3], row, side));
        for (unsigned col = 0; col < side; ++col) {
            NUM_SCOPE(point_numbers);
            expr_set_val(axes[0], coordinate(bounds[0], bounds[1], col, side));
            number_t result = expr_eval(formula);
            double value = num_to_double(result);
            if (!num_is_real(result) || !num_is_finite(result) || !isfinite(value))
                continue;
            values[finite++] = value;
            peak = fmax(peak, fabs(value));
            if (col < minimum[0])
                minimum[0] = col;
            if (col > maximum[0])
                maximum[0] = col;
            if (row < minimum[1])
                minimum[1] = row;
            if (row > maximum[1])
                maximum[1] = row;
        }
    }
    if (finite < (side * rows + 3) / 4 || (!peak && !constant))
        return false;
    if (!peak)
        peak = 1;
    double low = 1, high = -1;
    for (unsigned i = 0; i < finite; ++i) {
        values[i] /= peak;
        low = fmin(low, values[i]);
        high = fmax(high, values[i]);
    }
    if (high - low < 1e-10 && !constant)
        return false;
    unsigned interior = 0, lower_level = 0, upper_level = 0;
    for (unsigned i = 0; i < finite; ++i) {
        double fraction = constant ? 0.5 : (values[i] - low) / (high - low);
        interior += fraction > 0.001 && fraction < 0.999;
        lower_level += fraction <= 0.001;
        upper_level += fraction >= 0.999;
    }
    /* A solitary spike or an almost entirely underflowed tail is not a useful automatic view. */
    /* A well-populated two-level step is informative despite having no interior heights.
     * Requiring both levels to cover a quarter of the probes still rejects solitary spikes. */
    bool stepped = lower_level >= (finite + 3) / 4 && upper_level >= (finite + 3) / 4;
    if (interior < (finite + 7) / 8 && !stepped)
        return false;
    for (unsigned axis = 0; axis < dimensions; ++axis) {
        if (minimum[axis] >= maximum[axis])
            return false;
        unsigned offset = 2 * axis;
        number_t lo = coordinate(bounds[offset], bounds[offset + 1], minimum[axis], side);
        number_t hi = coordinate(bounds[offset], bounds[offset + 1], maximum[axis], side);
        if (automatic[offset]) {
            num_destroy(&bounds[offset]);
            bounds[offset] = num_clone(lo);
        }
        if (automatic[offset + 1]) {
            num_destroy(&bounds[offset + 1]);
            bounds[offset + 1] = num_clone(hi);
        }
        num_destroy(&lo);
        num_destroy(&hi);
    }
    return true;
}

/* Infer only absent endpoints; two explicit endpoints retain the existing guarded parsing policy. */
bool lab_worker_plot_ranges(const char *source, const char *first, const char *second,
                            const char *const limits[4], number_t bounds[4])
{
    unsigned dimensions = second ? 2 : 1;
    bool automatic[4] = {false}, needed = false;
    for (unsigned i = 0; i < 2 * dimensions; ++i) {
        automatic[i] = blank(limits[i]);
        needed |= automatic[i];
        bounds[i] = NUM_NAN;
    }
    if (!needed)
        return lab_worker_plot_bounds(limits[0], limits[1], &bounds[0], &bounds[1]) &&
               (!second || lab_worker_plot_bounds(limits[2], limits[3], &bounds[2], &bounds[3]));
    size_t saved = num_get_default_prec_bits(), length = 0;
    for (unsigned i = 0; i < 2 * dimensions; ++i)
        length += strlen(limits[i]);
    num_set_default_prec_bits(length < 2000 ? 128 + 4 * length : 8192);
    bool ok = true, found = false;
    int error = ENXIO;
    for (unsigned axis = 0; ok && axis < dimensions; ++axis) {
        unsigned offset = 2 * axis;
        if (!automatic[offset] && !automatic[offset + 1]) {
            ok = lab_worker_plot_bounds(limits[offset], limits[offset + 1], &bounds[offset], &bounds[offset + 1]);
            if (!ok)
                error = errno;
            for (unsigned i = offset; ok && i < offset + 2; ++i) {
                size_t precision = num_get_effective_prec_bits(bounds[i]);
                if (precision > num_get_default_prec_bits())
                    num_set_default_prec_bits(precision);
            }
            continue;
        }
        for (unsigned i = offset; ok && i < offset + 2; ++i) {
            if (automatic[i])
                continue;
            bounds[i] = endpoint(limits[i]);
            if (!num_is_real(bounds[i]) || !num_is_finite(bounds[i])) {
                ok = false;
                error = EINVAL;
            }
        }
    }
    expr_bindings_t *bindings = NULL;
    expr_t *formula = ok ? expr_from_string(source, &bindings) : NULL;
    expr_t *axes[] = {bindings ? expr_bindings_get(bindings, first) : NULL,
                     second && bindings ? expr_bindings_get(bindings, second) : NULL};
    bool owned[] = {!axes[0], second && !axes[1]};
    for (unsigned i = 0; i < dimensions; ++i)
        if (owned[i])
            axes[i] = expr_new_named_var(NUM_NAN, i ? second : first);
    ok = ok && formula && axes[0] && (!second || axes[1]);
    bool constant = false;
    if (ok && dimensions == 2) {
        expr_set_val(axes[0], NUM_NAN);
        expr_set_val(axes[1], NUM_NAN);
        number_t value = expr_eval(formula);
        constant = num_is_real(value) && num_is_finite(value);
        num_destroy(&value);
    }
    /* A finite work budget, not a key search: stop at the first informative real window. */
    static const char *const scales[] = {
        "10", "1", "0.1", "0.01", "0.001", "100", "1000", "10000",
        "5", "2", "0.5", "0.2", "0.05", "0.02", "0.005", "0.002", "20", "50", "200", "500", "2000", "5000"
    };
    for (unsigned attempt = 0; ok && !found && attempt < sizeof scales / sizeof *scales; ++attempt) {
        NUM_SCOPE(candidate_numbers);
        number_t radius = endpoint(scales[attempt]);
        for (unsigned axis = 0; axis < dimensions; ++axis) {
            unsigned i = axis * 2;
            if (!automatic[i] && !automatic[i + 1])
                continue;
            number_t lo, hi;
            if (automatic[i] && automatic[i + 1]) {
                lo = num_neg(radius);
                hi = num_clone(radius);
            } else {
                number_t anchor = bounds[automatic[i] ? i + 1 : i];
                number_t size = num_abs(anchor);
                if (num_lt(size, NUM_ONE))
                    size = num_clone(NUM_ONE);
                number_t span = num_mul(radius, size);
                lo = automatic[i] ? num_sub(anchor, span) : num_clone(anchor);
                hi = automatic[i + 1] ? num_add(anchor, span) : num_clone(anchor);
            }
            if (automatic[i]) {
                num_destroy(&bounds[i]);
                bounds[i] = num_scope_detach(num_clone(lo));
            }
            if (automatic[i + 1]) {
                num_destroy(&bounds[i + 1]);
                bounds[i + 1] = num_scope_detach(num_clone(hi));
            }
        }
        found = candidate(formula, axes, dimensions, bounds, automatic, constant);
        /* Candidate may replace endpoints while the local number scope is active. */
        for (unsigned i = 0; i < 2 * dimensions; ++i)
            bounds[i] = num_scope_detach(bounds[i]);
    }
    expr_free(formula);
    for (unsigned i = 0; i < dimensions; ++i)
        if (owned[i])
            expr_free(axes[i]);
    expr_bindings_free(bindings);
    num_set_default_prec_bits(saved);
    if (!found)
        errno = error;
    return found;
}
