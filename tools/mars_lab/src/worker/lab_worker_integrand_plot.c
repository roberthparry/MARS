/**
 * @file lab_worker_integrand_plot.c
 * @brief Independent integrand sampling and signed definite-interval shading for MARS Lab.
 *
 * Uses authored integration rows, not an antiderivative or its numerical value.
 * Drawing precision and viewport edits are independent of integration precision
 * and limits. Unset parameters and higher-dimensional integrals are reported
 * locally without changing the integration result cards.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static number_t limit(const char *text)
{
    expr_t *expression = *text ? expr_from_string(text, NULL) : NULL;
    number_t result = expression ? expr_eval(expression) : NUM_NAN;
    expr_free(expression);
    return result;
}

/* Build a separate drawing; no numerical quadrature is performed by this worker. */
int lab_worker_integrand_plot(int argc, char **argv)
{
    if (argc != 22) {
        printf("error Integrand plotting currently requires one integration variable; "
               "multidimensional plots are not yet available.\n");
        return 1;
    }
    const char *axis = argv[19];
    if (!*axis || strpbrk(axis, "\r\n\t")) {
        printf("error Supply a valid integration variable name.\n");
        return 1;
    }
    expr_bindings_t *bindings = NULL;
    expr_t *expression = expr_from_string(argv[1], &bindings);
    expr_t *variable = bindings ? expr_bindings_get(bindings, axis) : NULL;
    bool parameters = expression != NULL;
    for (size_t i = 0; parameters && i < expr_bindings_count(bindings); ++i) {
        expr_t *node = expr_bindings_get(bindings, expr_bindings_name_at(bindings, i));
        if (node == variable)
            continue;
        number_t value = expr_eval(node);
        parameters = num_is_real(value) && num_is_finite(value);
        num_destroy(&value);
    }
    if (!parameters) {
        printf("error Supply finite real values for the remaining integrand parameters.\n");
        expr_free(expression);
        expr_bindings_free(bindings);
        return 1;
    }
    size_t saved_precision = num_get_default_prec_bits(), precision = 53;
    num_set_default_prec_bits(53);
    number_t lower = limit(argv[20]), upper = limit(argv[21]);
    bool definite = *argv[20] && *argv[21];
    bool finite_limits = definite && num_is_real(lower) && num_is_finite(lower) &&
                         num_is_real(upper) && num_is_finite(upper);
    number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    char *source = expr_to_string(expression, style_EXPRESSION);
    const char *specified[] = {argv[5], argv[6], "", ""};
    bool ranged = false;
    if (finite_limits && !*argv[5] && !*argv[6]) {
        double a = num_to_double(lower), b = num_to_double(upper);
        double padding = fmax(fabs(b - a) * 0.1, 0.1);
        bounds[0] = num_create_from_double(fmin(a, b) - padding);
        bounds[1] = num_create_from_double(fmax(a, b) + padding);
        ranged = true;
    } else if (source) {
        ranged = lab_worker_plot_ranges(source, axis, NULL, specified, bounds);
    }
    if (!ranged && !*argv[5] && !*argv[6]) {
        num_destroy(&bounds[0]);
        num_destroy(&bounds[1]);
        bounds[0] = num_create_from_long(-10);
        bounds[1] = num_create_from_long(10);
        ranged = true;
    }
    number_t ymin = NUM_NAN, ymax = NUM_NAN;
    bool manual = !strcmp(argv[7], "manual");
    bool ok = source && ranged && (!manual || lab_worker_plot_bounds(argv[8], argv[9], &ymin, &ymax));
    sampler_t *samples = ok ? lab_worker_plot_samples_quality(source, axis, bounds[0], bounds[1], ymin, ymax,
                                                               &precision, !strcmp(argv[17], "preview")) : NULL;
    plot_options_t options = plot_options_default();
    options.x_label = axis;
    options.y_label = "";
    options.legend = false;
    options.smart_y = !strcmp(argv[7], "auto");
    options.guard_gaps = true;
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    plot_t *plot = samples ? plot_new(&options) : NULL;
    ok = plot && plot_add_sampler(plot, samples, "Integrand", PLOT_LINES, 0x78d8cb);
    if (ok && manual)
        ok = plot_set_bounds(plot, bounds[0], bounds[1], ymin, ymax);
    if (ok && finite_limits)
        ok = plot_shade_interval(plot, lower, upper);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    char *title = expression ? expr_to_string(expression, style_LATEX) : NULL;
    if (svg)
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\n"
               "binding variable\t%s\tNAN\ndifferentiable no\n", argv[1], argv[1], source,
               title ? title : "", string_c_str(svg), axis);
    else
        printf("error No finite real integrand curve found; check its parameters, domain and plotting bounds.\n");
    int rc = svg ? 0 : 1;
    free(title);
    free(source);
    string_free(svg);
    plot_free(plot);
    sampler_free(samples);
    num_destroy(&lower);
    num_destroy(&upper);
    num_destroy(&ymin);
    num_destroy(&ymax);
    for (unsigned i = 0; i < 4; ++i)
        num_destroy(&bounds[i]);
    expr_free(expression);
    expr_bindings_free(bindings);
    num_set_default_prec_bits(saved_precision);
    return rc;
}
