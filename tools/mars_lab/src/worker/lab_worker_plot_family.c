/**
 * @file lab_worker_plot_family.c
 * @brief Representative curves from native ODE solution families.
 *
 * Inspects native binding metadata and specialises generated integration
 * constants only. Authored parameters are never guessed. Private variable copies
 * support derivative-guided parameter sweeps for five or nine representatives.
 * General solution renderings remain unchanged in the calling plot worker.
 * The isolated calculation process bounds the total family to sixteen curves.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static void parameter_values(const expr_t *formula, expr_bindings_t *bindings, const char *axis,
                              const char *const *names, size_t count, double selected[2][5])
{
    size_t saved_precision = num_get_default_prec_bits();
    /* Selection is a visual heuristic; retain guard precision without inheriting thousands of worksheet digits. */
    if (saved_precision > 128)
        num_set_default_prec_bits(128);
    expr_t *copy = NULL, *parameters[2] = {NULL, NULL};
    size_t side = count == 1 ? 5 : 3;
    for (size_t p = 0; p < count; ++p) {
        for (size_t k = 0; k < side; ++k)
            selected[p][k] = (double)k - (double)(side / 2);
        parameters[p] = expr_new_named_var(NUM_ZERO, names[p]);
    }
    expr_t *x = expr_new_named_var(NUM_ZERO, axis);
    size_t symbols_count = expr_bindings_count(bindings);
    const char **symbol_names = calloc(symbols_count, sizeof *symbol_names);
    expr_t **symbols = calloc(symbols_count, sizeof *symbols);
    char *body = expr_to_function_body(formula);
    bool ready = x && body && symbol_names && symbols;
    for (size_t i = 0; ready && i < symbols_count; ++i) {
        symbol_names[i] = expr_bindings_name_at(bindings, i);
        symbols[i] = expr_bindings_expr_at(bindings, i);
        if (!strcmp(symbol_names[i], axis))
            symbols[i] = x;
        for (size_t p = 0; p < count; ++p)
            if (!strcmp(symbol_names[i], names[p]))
                symbols[i] = parameters[p];
        ready = symbols[i] != NULL;
    }
    if (ready)
        copy = expr_from_function_body_with_symbols(body, symbol_names, symbols, symbols_count);
    free(body);
    free(symbol_names);
    free(symbols);
    /* Bounded exploratory slices, not a claim about the entire parameter plane.
     * Each bin receives equal spatial weight despite adaptive sample clustering. */
    for (size_t p = 0; copy && x && p < count; ++p) {
        double density[64] = {0};
        for (unsigned slice = 0; slice < 6; ++slice) {
            number_t xv = num_create_from_long(2 * (long)(slice % 3) - 2);
            expr_set_val(x, xv);
            num_destroy(&xv);
            if (count == 2) {
                number_t other = num_create_from_long(slice < 3 ? -1 : 1);
                expr_set_val(parameters[1 - p], other);
                num_destroy(&other);
            }
            sampler_options_t options = sampler_options_default();
            options.intervals = 32;
            options.max_evaluations = 257;
            options.max_depth = 6;
            number_t lower = num_create_from_long(-4), upper = num_create_from_long(4);
            sampler_t *samples = sampler_expression(copy, parameters[p], NULL, lower, upper, &options);
            num_destroy(&lower);
            num_destroy(&upper);
            double weights[64] = {0};
            for (size_t i = 0; samples && i < sampler_count(samples); ++i) {
                const sampler_point_t *point = sampler_point(samples, i);
                double t = num_to_double(point->parameter), y = num_to_double(point->y);
                double dy = num_to_double(point->dy);
                if (!point->valid || !isfinite(t) || !isfinite(y) || !isfinite(dy) || t < -4 || t > 4)
                    continue;
                unsigned bin = (unsigned)fmin(63, floor((t + 4) * 8));
                /* Angular ordinate sensitivity limits the influence of poles.
                 * The sampler also uses second derivatives to refine its probes. */
                double sensitivity = fabs(dy) / hypot(1, y) / hypot(1, y);
                weights[bin] = fmax(weights[bin], fmin(8, sensitivity));
            }
            sampler_free(samples);
            for (unsigned bin = 0; bin < 64; ++bin)
                density[bin] += weights[bin];
        }
        double total = 0;
        for (unsigned bin = 0; bin < 64; ++bin)
            total += density[bin];
        if (!(total > 0) || !isfinite(total))
            continue; /* Keep bounded seeds when no usable sensitivity exists. */
        /* A uniform floor keeps quiet portions represented as well. */
        double floor_weight = total / 256;
        total += 64 * floor_weight;
        for (size_t k = 0; k < side; ++k) {
            double target = total * (k + 0.5) / side, cumulative = 0;
            for (unsigned bin = 0; bin < 64; ++bin) {
                double weight = density[bin] + floor_weight;
                if (cumulative + weight >= target) {
                    selected[p][k] = -4 + (bin + (target - cumulative) / weight) / 8;
                    break;
                }
                cumulative += weight;
            }
        }
    }
    expr_free(copy);
    expr_free(x);
    for (size_t p = 0; p < count; ++p)
        expr_free(parameters[p]);
    num_set_default_prec_bits(saved_precision);
}

static bool free_constants(expr_bindings_t *bindings, expr_bindings_t *authored, const char *axis,
                            const char **names, size_t *count)
{
    *count = 0;
    /* Enumerate the native catalogue once; membership uses indexed binding lookups. */
    for (size_t i = 0; i < expr_bindings_count(bindings); ++i) {
        const char *name = expr_bindings_name_at(bindings, i);
        if (!strcmp(name, axis))
            continue;
        expr_t *symbol = expr_bindings_expr_at(bindings, i);
        number_t value = expr_eval(symbol);
        bool unset = num_is_nan(value);
        num_destroy(&value);
        if (!unset)
            continue;
        if (!expr_bindings_is_constant_at(bindings, i) || (authored && expr_bindings_get(authored, name))) {
            printf("error Supply a value for parameter %s; family plots vary integration constants only.\n", name);
            return false;
        }
        if (*count == 2) {
            printf("error Family plotting supports up to two free integration constants. Supply more initial conditions.\n");
            return false;
        }
        names[(*count)++] = name;
    }
    return true;
}

static bool append_family(const expr_t *formula, expr_bindings_t *bindings, const char *axis, const char *const *names,
                           size_t parameter_count, size_t branch, size_t branches,
                           expr_t **curves, string_t **labels, size_t *count)
{
    size_t side = parameter_count == 1 ? 5 : 3;
    size_t members = parameter_count == 2 ? side * side : parameter_count ? side : 1;
    if (*count + members > 16) {
        printf("error The representative solution family exceeds sixteen curves. Supply more initial conditions.\n");
        return false;
    }
    double selected[2][5] = {{0}};
    if (parameter_count)
        parameter_values(formula, bindings, axis, names, parameter_count, selected);
    for (size_t member = 0; member < members; ++member) {
        expr_t *curve = expr_clone(formula);
        expr_bindings_t *values = bindings;
        string_t *label = string_new();
        bool ok = curve && label;
        if (ok && branches > 1)
            ok = string_append_format(label, "Solution %zu%s", branch + 1, parameter_count ? ": " : "") >= 0;
        for (size_t p = 0; ok && p < parameter_count; ++p) {
            double value = selected[p][p ? member / side : member % side];
            char value_text[32];
            snprintf(value_text, sizeof value_text, "%.12g", value);
            expr_bindings_t *updated = NULL;
            expr_t *specialised = expr_edit_binding(curve, values, names[p], value_text, &updated);
            expr_free(curve);
            if (values != bindings)
                expr_bindings_free(values);
            values = updated;
            curve = specialised;
            ok = curve && string_append_format(label, "%s%s = %s", p ? ", " : "", names[p], value_text) >= 0;
        }
        if (values != bindings)
            expr_bindings_free(values);
        if (!ok) {
            expr_free(curve);
            string_free(label);
            return false;
        }
        curves[*count] = curve;
        labels[(*count)++] = label;
    }
    return true;
}

/* Native substitutions build illustrative members without changing the problem or its general solution. */
bool lab_worker_plot_family(expr_bindings_t *authored, const char *axis, const expr_t *const *branches,
                             size_t branch_count, expr_t **curves, string_t **labels, size_t *count, bool *family)
{
    *count = 0;
    *family = false;
    for (size_t i = 0; i < branch_count; ++i) {
        char *source = expr_to_string(branches[i], style_EXPRESSION);
        expr_bindings_t *bindings = NULL;
        expr_t *formula = source ? expr_from_string(source, &bindings) : NULL;
        const char *names[2] = {NULL, NULL};
        size_t parameter_count = 0;
        bool ok = formula && free_constants(bindings, authored, axis, names, &parameter_count);
        *family |= parameter_count > 0;
        if (ok)
            ok = append_family(formula, bindings, axis, names, parameter_count, i, branch_count, curves, labels, count);
        expr_free(formula);
        expr_bindings_free(bindings);
        free(source);
        if (!ok)
            return false;
    }
    return *count > 0;
}
