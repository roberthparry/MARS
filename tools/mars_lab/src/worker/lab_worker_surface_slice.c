/**
 * @file lab_worker_surface_slice.c
 * @brief Plot-local parameter slices, native range selection and exact slider interpolation.
 *
 * Reparse the worksheet into a private binding graph. Unplotted unset symbols
 * receive finite slice values without changing authored bindings. Principal
 * inverse-trigonometric affine inputs use a conservative domain envelope;
 * other expressions use bounded real-value probes. These probes are not a
 * completeness proof. Protocol rows preserve exact text and use integer slider
 * positions, never browser floating-point coordinates.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static bool real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

/* A deliberately small structural interval evaluator, independent of symbol names. */
static bool real_input(const expr_t *expr, unsigned depth)
{
    if (!expr || depth > 16)
        return false;
    number_t value = expr_eval(expr);
    bool finite = real(value), variable = expr_is_variable(expr) && num_is_nan(value);
    num_destroy(&value);
    if (finite || variable)
        return true;
    const expr_t *left = NULL, *right = NULL;
    bool subtract = false;
    if (expr_match_add_sub_expr(expr, &left, &right, &subtract) || expr_match_mul_expr(expr, &left, &right) ||
        expr_match_div_expr(expr, &left, &right))
        return real_input(left, depth + 1) && real_input(right, depth + 1);
    if (expr_match_neg_expr(expr, &left) || expr_match_sin_expr(expr, &left) ||
        expr_match_cos_expr(expr, &left) || expr_match_exp_expr(expr, &left))
        return real_input(left, depth + 1);
    number_t exponent = NUM_NAN;
    bool power = expr_match_pow_const(expr, &left, &exponent);
    bool ok = power && num_is_integer(exponent) && real_input(left, depth + 1);
    num_destroy(&exponent);
    return ok;
}

static bool enclosure(const expr_t *expr, number_t bounds[2], unsigned depth)
{
    if (!expr || depth > 16)
        return false;
    const expr_t *left = NULL, *right = NULL;
    number_t value = expr_eval(expr);
    if (real(value)) {
        bounds[0] = value;
        bounds[1] = num_clone(value);
        return true;
    }
    num_destroy(&value);
    if ((expr_match_sin_expr(expr, &left) || expr_match_cos_expr(expr, &left)) && real_input(left, 0)) {
        bounds[0] = num_neg(NUM_ONE);
        bounds[1] = num_clone(NUM_ONE);
        return true;
    }
    bool subtract = false;
    if (!expr_match_add_sub_expr(expr, &left, &right, &subtract))
        return false;
    number_t a[] = {NUM_NAN, NUM_NAN}, b[] = {NUM_NAN, NUM_NAN};
    bool ok = enclosure(left, a, depth + 1) && enclosure(right, b, depth + 1);
    if (ok) {
        bounds[0] = subtract ? num_sub(a[0], b[1]) : num_add(a[0], b[0]);
        bounds[1] = subtract ? num_sub(a[1], b[0]) : num_add(a[1], b[1]);
    }
    for (unsigned i = 0; i < 2; ++i) {
        num_destroy(&a[i]);
        num_destroy(&b[i]);
    }
    return ok;
}

static bool domain_range(expr_t *expr, expr_t *parameter, number_t bounds[2])
{
    NUM_SCOPE(numbers);
    const expr_t *argument = NULL;
    expr_t *prototype = expr_acos(parameter);
    bool match = prototype && expr_match_same_reapplicable_unary_function(expr, prototype, &argument);
    expr_free(prototype);
    if (!match) {
        prototype = expr_asin(parameter);
        match = prototype && expr_match_same_reapplicable_unary_function(expr, prototype, &argument);
        expr_free(prototype);
    }
    if (!match)
        return false;
    number_t saved = expr_get_val(parameter);
    expr_set_val(parameter, NUM_NAN);
    const expr_t *derivative = expr_get_deriv(argument, parameter);
    number_t coefficient = derivative ? expr_eval(derivative) : NUM_NAN;
    expr_t *zero = expr_new_const(NUM_ZERO);
    expr_t *remainder = zero ? expr_substitute(argument, parameter, zero) : NULL;
    number_t range[] = {NUM_NAN, NUM_NAN};
    bool ok = real(coefficient) && !num_is_zero(coefficient) && enclosure(remainder, range, 0);
    if (ok) {
        number_t lo = num_div(num_sub(num_neg(NUM_ONE), range[1]), coefficient);
        number_t hi = num_div(num_sub(NUM_ONE, range[0]), coefficient);
        bounds[0] = num_scope_detach(num_clone(num_lt(lo, hi) ? lo : hi));
        bounds[1] = num_scope_detach(num_clone(num_lt(lo, hi) ? hi : lo));
    }
    expr_set_val(parameter, saved);
    expr_free(remainder);
    expr_free(zero);
    return ok;
}

static bool probe(expr_t *expr, expr_t *parameter, number_t value, expr_t *x, expr_t *y)
{
    NUM_SCOPE(numbers);
    number_t saved_x = expr_get_val(x), saved_y = expr_get_val(y), saved = expr_get_val(parameter);
    expr_set_val(parameter, value);
    bool found = false;
    /* Twenty-five fixed real-domain probes; no unbounded search is attempted. */
    for (int r = -2; !found && r <= 2; ++r) {
        expr_set_val(y, num_create_from_frac(r * 3, 2));
        for (int c = -2; !found && c <= 2; ++c) {
            expr_set_val(x, num_create_from_frac(c * 3, 2));
            number_t height = expr_eval(expr);
            found = real(height);
            num_destroy(&height);
        }
    }
    expr_set_val(x, saved_x);
    expr_set_val(y, saved_y);
    expr_set_val(parameter, saved);
    return found;
}

static number_t exact(const char *text)
{
    expr_t *expr = expr_from_string(text, NULL);
    number_t value = expr ? expr_eval(expr) : NUM_NAN;
    expr_free(expr);
    return value;
}

/* The returned source owns its text; all mutations belong to this private graph. */
char *lab_worker_surface_slice(const char *source, const char *first, const char *second, const char *request)
{
    NUM_SCOPE(numbers);
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(source, &bindings);
    expr_t *x = bindings ? expr_bindings_get(bindings, first) : NULL;
    expr_t *y = bindings ? expr_bindings_get(bindings, second) : NULL;
    expr_t *parameters[8];
    size_t count = 0;
    bool ok = expr && x && y;
    for (size_t i = 0; ok && i < expr_bindings_count(bindings); ++i) {
        expr_t *binding = expr_bindings_expr_at(bindings, i);
        number_t value = expr_get_val(binding);
        if (binding == x || binding == y || !num_is_nan(value))
            continue;
        if (count == sizeof parameters / sizeof *parameters) {
            ok = false;
            break;
        }
        parameters[count++] = binding;
        expr_set_val(binding, NUM_ZERO);
    }
    string_t *input = string_new_with(request ? request : "");
    size_t rows_count = 0;
    string_t **rows = input && string_length(input) ? string_split(input, "\n", &rows_count) : NULL;
    ok = ok && rows_count <= 8;
    string_t *metadata = string_new(), *title = string_new(), *title_TeX = string_new();
    ok = ok && metadata && title && title_TeX;
    for (size_t i = 0; ok && i < count; ++i) {
        const char *name = expr_symbol_name(parameters[i]);
        size_t fields_count = 0;
        string_t **fields = NULL;
        /* At most eight parameter rows; retain authored order and reject duplicates. */
        for (size_t r = 0; ok && r < rows_count; ++r) {
            size_t n = 0;
            string_t **parts = string_split(rows[r], "\t", &n);
            if (n && !strcmp(string_c_str(parts[0]), name)) {
                if (fields || n != 5) {
                    ok = false;
                    string_split_free(parts, n);
                    break;
                }
                fields = parts;
                fields_count = n;
            } else {
                string_split_free(parts, n);
            }
        }
        number_t bounds[] = {NUM_NAN, NUM_NAN}, value = NUM_ZERO;
        long position = -1;
        bool automatic = fields && !strcmp(string_c_str(fields[4]), "-2");
        if (ok && fields && !automatic) {
            ok = lab_worker_plot_bounds(string_c_str(fields[1]), string_c_str(fields[2]), &bounds[0], &bounds[1]);
            size_t bits = ok ? lab_worker_plot_range_precision(bounds[0], bounds[1]) : 0;
            ok = ok && bits && bits <= 32768;
            if (ok && bits > num_get_default_prec_bits())
                num_set_default_prec_bits(bits);
            char *end = NULL;
            position = strtol(string_c_str(fields[4]), &end, 10);
            ok = ok && end && end != string_c_str(fields[4]) && !*end && position >= -1 && position <= 1000;
            if (ok && position >= 0)
                value = num_add(bounds[0], num_mul(num_sub(bounds[1], bounds[0]), num_create_from_frac(position, 1000)));
            else if (ok)
                value = exact(string_c_str(fields[3]));
        } else if (ok) {
            number_t saved_x = expr_get_val(x), saved_y = expr_get_val(y);
            expr_set_val(x, NUM_NAN);
            expr_set_val(y, NUM_NAN);
            bool bounded = domain_range(expr, parameters[i], bounds);
            expr_set_val(x, saved_x);
            expr_set_val(y, saved_y);
            if (!bounded) {
                long radius = 1;
                bool found = probe(expr, parameters[i], value, x, y);
                for (long step = 1; !found && step <= 64; step *= 2) {
                    for (int sign = -1; !found && sign <= 1; sign += 2) {
                        number_t candidate = num_create_from_long(sign * step);
                        if (probe(expr, parameters[i], candidate, x, y)) {
                            value = candidate;
                            radius = step;
                            found = true;
                        }
                    }
                }
                if (num_is_zero(value) && (probe(expr, parameters[i], NUM_ONE, x, y) ||
                                          probe(expr, parameters[i], num_neg(NUM_ONE), x, y)))
                    radius = 2;
                bounds[0] = num_sub(value, num_create_from_long(radius));
                bounds[1] = num_add(value, num_create_from_long(radius));
            }
            if (num_lt(value, bounds[0]) || num_gt(value, bounds[1]))
                value = num_mul(num_add(bounds[0], bounds[1]), NUM_HALF);
            if (automatic) {
                number_t previous = exact(string_c_str(fields[3]));
                if (real(previous) && !num_lt(previous, bounds[0]) && !num_gt(previous, bounds[1]))
                    value = previous;
            }
        }
        ok = ok && real(value) && real(bounds[0]) && real(bounds[1]) && num_lt(bounds[0], bounds[1]) &&
             !num_lt(value, bounds[0]) && !num_gt(value, bounds[1]);
        if (ok) {
            number_t fraction = num_div(num_sub(value, bounds[0]), num_sub(bounds[1], bounds[0]));
            position = lround(1000 * num_to_double(fraction));
            expr_set_val(parameters[i], value);
            int digits = (int)(num_get_default_prec_bits() * 30103 / 100000 + 3);
            string_t *lo = num_sprintf_text("%.*n", digits, bounds[0]);
            string_t *hi = num_sprintf_text("%.*n", digits, bounds[1]);
            string_t *text = num_sprintf_text("%.*n", digits, value);
            expr_t *constant = expr_new_const(value);
            char *label = expr_to_TeX_body(parameters[i]), *value_TeX = constant ? expr_to_TeX_body(constant) : NULL;
            ok = lo && hi && text && label && value_TeX &&
                 string_append_format(metadata, "%s%s\t%s\t%s\t%s\t%ld", i ? "\n" : "", name,
                                      string_c_str(lo), string_c_str(hi), string_c_str(text), position) >= 0 &&
                 string_append_format(title, "%s%s = %s", i ? ", " : "", name, string_c_str(text)) >= 0 &&
                 string_append_format(title_TeX, "%s%s = %s", i ? ",\\;" : "", label, value_TeX) >= 0;
            free(label);
            free(value_TeX);
            expr_free(constant);
            string_free(lo);
            string_free(hi);
            string_free(text);
        }
        string_split_free(fields, fields_count);
        num_destroy(&bounds[0]);
        num_destroy(&bounds[1]);
    }
    char *result = ok ? expr_to_string(expr, style_EXPRESSION) : NULL;
    if (result && count)
        printf("plot_slices %s\nplot_slice_title %s\nplot_slice_TeX %s\n", string_c_str(metadata),
               string_c_str(title), string_c_str(title_TeX));
    else if (!result)
        printf("error Invalid slice value or limits; use finite ordered bounds and a value within them (at most eight slices).\n");
    string_free(metadata);
    string_free(title);
    string_free(title_TeX);
    string_split_free(rows, rows_count);
    string_free(input);
    expr_free(expr);
    expr_bindings_free(bindings);
    return result;
}
