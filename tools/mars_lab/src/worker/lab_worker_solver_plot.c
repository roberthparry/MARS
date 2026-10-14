/**
 * @file lab_worker_solver_plot.c
 * @brief Native curves and solution surfaces for algebraic and differential equations.
 *
 * Parses equation graphs without browser-side rewriting. Both algebraic sides
 * share one viewport; explicit ODE solution branches are sampled independently.
 * Two-variable PDE solutions use the shared surface renderer. Implicit solutions,
 * unset parameters and unsupported dimensions are reported,
 * never replaced by guessed functions. Existing guarded sampling and clipping
 * policies apply to every curve in the isolated calculation process.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"
#include "equation-t.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static int plot_curves(const char *input, const char *title, const char *axis,
                       const expr_t *const *curves, size_t count, bool equation, char **argv)
{
    char *sources[16] = {0};
    number_t bounds[4] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    number_t ymin = NUM_NAN, ymax = NUM_NAN;
    const char *limits[] = {argv[5], argv[6], "", ""};
    bool manual = !strcmp(argv[7], "manual"), ranged = false, constants = true;
    bool ok = count && count <= 16 &&
              (!strcmp(argv[7], "auto") || !strcmp(argv[7], "all") || manual);
    if (ok && manual)
        ok = lab_worker_plot_bounds(argv[8], argv[9], &ymin, &ymax);
    size_t saved_precision = num_get_default_prec_bits(), drawing_precision = 53;
    for (size_t i = 0; ok && i < count; ++i) {
        sources[i] = expr_to_string(curves[i], style_EXPRESSION);
        number_t value = expr_eval(curves[i]);
        constants &= num_is_real(value) && num_is_finite(value);
        num_destroy(&value);
        number_t candidate[4] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
        bool found = sources[i] && lab_worker_plot_ranges(sources[i], axis, NULL, limits, candidate);
        if (!sources[i] || (!found && errno == EINVAL))
            ok = false;
        if (found) {
            if (!ranged || num_lt(candidate[0], bounds[0])) {
                num_destroy(&bounds[0]);
                bounds[0] = num_clone(candidate[0]);
            }
            if (!ranged || num_gt(candidate[1], bounds[1])) {
                num_destroy(&bounds[1]);
                bounds[1] = num_clone(candidate[1]);
            }
            ranged = true;
        }
        for (size_t j = 0; j < 4; ++j)
            num_destroy(&candidate[j]);
    }
    /* A pair of constants has no variation from which to infer a horizontal span. */
    if (ok && !ranged && constants && !*argv[5] && !*argv[6]) {
        bounds[0] = num_create_from_long(-1);
        bounds[1] = num_clone(NUM_ONE);
        ranged = true;
    }
    plot_options_t options = plot_options_default();
    options.x_label = axis;
    options.y_label = "";
    options.title = "";
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.legend = true;
    options.smart_y = !strcmp(argv[7], "auto");
    plot_t *plot = ok && ranged ? plot_new(&options) : NULL;
    ok = ok && plot;
    static const unsigned colours[] = {0x78d8cb, 0xe8bd65, 0xd68cc4, 0x8caff0};
    for (size_t i = 0; ok && i < count; ++i) {
        size_t precision = 53;
        sampler_t *samples = lab_worker_plot_samples(sources[i], axis, bounds[0], bounds[1], ymin, ymax, &precision);
        bool finite = false;
        /* At most 8193 points per curve; reject an entirely unavailable branch rather than hide it. */
        for (size_t j = 0; !finite && j < sampler_count(samples); ++j)
            finite = sampler_point(samples, j)->valid;
        if (precision > drawing_precision)
            drawing_precision = precision;
        char label[40];
        snprintf(label, sizeof label, "Solution %zu", i + 1);
        ok = finite && plot_add_sampler(plot, samples, equation ? (i ? "Right side" : "Left side") : label,
                                          PLOT_LINES, colours[i % 4]);
        sampler_free(samples);
    }
    num_set_default_prec_bits(drawing_precision);
    if (ok && manual)
        ok = plot_set_bounds(plot, bounds[0], bounds[1], ymin, ymax);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    if (svg) {
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\ndifferentiable no\n",
               input, input, input, title ? title : "", string_c_str(svg));
    } else {
        printf("error Unable to plot finite real curves. Supply valid bounds and values for remaining parameters; "
               "for differential equations supply sufficient initial conditions.\n");
    }
    ok = svg != NULL;
    string_free(svg);
    plot_free(plot);
    for (size_t i = 0; i < count && i < 16; ++i)
        free(sources[i]);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&bounds[i]);
    num_destroy(&ymin);
    num_destroy(&ymax);
    num_set_default_prec_bits(saved_precision);
    return ok ? 0 : 1;
}

static void print_axis(const char *name)
{
    expr_t *axis = expr_new_named_var(NUM_NAN, name);
    char *text = axis ? expr_to_string(axis, style_UNBOUND) : NULL;
    if (text)
        printf("binding variable\t%s\tNAN\n", text);
    free(text);
    expr_free(axis);
}

/* Select native equation sides or explicit differential-equation graphs, preserving the worksheet source. */
int lab_worker_solver_plot(int argc, char **argv)
{
    if (argc < 19 || (strcmp(argv[18], "equation") && strcmp(argv[18], "diffequation"))) {
        printf("error Unknown plotting source mode.\n");
        return 1;
    }
    bool equation = !strcmp(argv[18], "equation");
    if ((equation && (!strcmp(argv[10], "surface") || *argv[11])) ||
        (strcmp(argv[10], "auto") && strcmp(argv[10], "curve") && strcmp(argv[10], "surface")) ||
        (strcmp(argv[17], "full") && strcmp(argv[17], "preview"))) {
        printf("error Select a valid plot type and quality; algebraic equations support 2-D curves only.\n");
        return 1;
    }
    const char *axis = argv[2];
    if (equation) {
        equation_t *equ = equ_from_string(argv[1]);
        expr_bindings_t *bindings = equ ? equ_bindings(equ) : NULL;
        expr_t *variable = bindings ? expr_bindings_get(bindings, axis) : NULL;
        if (!equ || !variable || !expr_is_variable(variable)) {
            equ_free(equ);
            printf("error Select an equation variable to plot.\n");
            return 1;
        }
        const expr_t *curves[] = {equ_lhs(equ), equ_rhs(equ)};
        char *title = equ_to_TeX_body_wrapped(equ, 110);
        for (size_t i = 0; i < expr_bindings_count(bindings); ++i)
            if (!expr_bindings_is_constant_at(bindings, i))
                print_axis(expr_bindings_name_at(bindings, i));
        int rc = plot_curves(argv[1], title, axis, curves, 2, true, argv);
        free(title);
        equ_free(equ);
        return rc;
    }
    diffequ_t *de = de_from_string(argv[1]);
    size_t dimensions = de ? de_independent_count(de) : 0;
    const expr_t *independent = NULL, *second = NULL;
    /* At most two declared coordinates; no free-parameter search is involved. */
    for (size_t i = 0; i < dimensions && dimensions <= 2; ++i) {
        const expr_t *coordinate = de_independent_at(de, i);
        const char *name = expr_symbol_name(coordinate);
        if (name && !strcmp(name, axis))
            independent = coordinate;
        else
            second = coordinate;
    }
    if (!independent || dimensions > 2 ||
        (dimensions == 1 && (!strcmp(argv[10], "surface") || *argv[11])) ||
        (dimensions == 2 && (!second || !expr_symbol_name(second) || !strcmp(argv[10], "curve") ||
                            (*argv[11] && strcmp(argv[11], expr_symbol_name(second)))))) {
        de_free(de);
        printf("error Use one independent variable for an ODE curve or two distinct independent variables "
               "for a PDE surface.\n");
        return 1;
    }
    diffequ_solve_result_t *solutions = de_solve_with_context(de, 0, NULL);
    size_t count = solutions ? de_solve_result_count(solutions) : 0;
    bool explicit = count > 0 && count <= 16 && de_solve_result_status(solutions) == DE_SOLVE_STATUS_SOLVED;
    const expr_t *curves[16] = {0};
    for (size_t i = 0; explicit && i < count; ++i) {
        const equation_t *solution = de_solve_result_at(solutions, i);
        const expr_t *lhs = equ_lhs(solution);
        explicit = expr_is_variable(lhs) && expr_symbol_name(lhs) && strcmp(expr_symbol_name(lhs), axis) &&
                   (!second || strcmp(expr_symbol_name(lhs), expr_symbol_name(second)));
        curves[i] = equ_rhs(solution);
    }
    int rc = 1;
    if (explicit && dimensions == 2 && count != 1) {
        printf("error Multiple PDE solution branches require selecting an explicit branch in Expression mode.\n");
    } else if (explicit && dimensions == 2) {
        char *source = expr_to_string(curves[0], style_EXPRESSION);
        printf("input %s\n", argv[1]);
        print_axis(axis);
        print_axis(expr_symbol_name(second));
        if (source)
            rc = lab_worker_plot_surface(source, curves[0], independent, second,
                                          equ_lhs(de_solve_result_at(solutions, 0)), argc, argv);
        free(source);
    } else if (explicit) {
        char *title = de_to_string(de, style_LATEX);
        print_axis(axis);
        rc = plot_curves(argv[1], title, axis, curves, count, false, argv);
        free(title);
    } else {
        printf("error Plotting requires explicit real solution functions; implicit, series-only or unsupported "
               "solutions cannot yet be plotted.\n");
    }
    de_solve_result_free(solutions);
    de_free(de);
    return rc;
}
