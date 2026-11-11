/**
 * @file lab_worker_solver_plot.c
 * @brief Native curves and solution surfaces for algebraic and differential equations.
 *
 * Parses equation graphs without browser-side rewriting. Both algebraic sides
 * share one viewport; explicit ODE solution branches are sampled independently.
 * Two-variable algebraic equations use real zero contours; PDE solutions use the
 * shared surface renderer. Unset authored parameters use labelled, plot-local slider choices before solving;
 * supplied values remain fixed. Implicit ODE solutions and unsupported dimensions are reported.
 * Existing guarded sampling and clipping
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
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_plot_prepared.h"

static int plot_curves(const char *input, const char *title_text, const char *title, const char *axis,
                       const expr_t *const *curves, string_t *const *labels, size_t count,
                       bool equation, bool family, char **argv, const diffequ_t *de)
{
    char *sources[16] = {0};
    number_t bounds[4] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    number_t ymin = NUM_NAN, ymax = NUM_NAN;
    const char *limits[] = {argv[5], argv[6], "", ""};
    string_t *interval_limits[2] = {NULL, NULL};
    if (de && de_independent_count(de) == 1u) {
        const expr_t *endpoints[] = {de_independent_lower_bound(de, 0u), de_independent_upper_bound(de, 0u)};
        for (size_t i = 0; i < 2u; ++i)
            if (!*limits[i] && endpoints[i]) {
                interval_limits[i] = expr_to_text(endpoints[i], style_UNBOUND);
                if (interval_limits[i])
                    limits[i] = string_c_str(interval_limits[i]);
            }
        if (!*limits[0]) {
            interval_limits[0] = lab_worker_de_time_lower(de, axis);
            if (interval_limits[0])
                limits[0] = string_c_str(interval_limits[0]);
        }
    }
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
    if (ok && !ranged && constants && !*limits[0] && !*limits[1]) {
        bounds[0] = num_create_from_long(-1);
        bounds[1] = num_clone(NUM_ONE);
        ranged = true;
    }
    if (ok && !ranged && constants && interval_limits[0] && !*limits[1]) {
        expr_t *lower = expr_from_string(limits[0], NULL);
        bounds[0] = lower ? expr_eval(lower) : num_clone(NUM_NAN);
        bounds[1] = num_add(bounds[0], NUM_TEN);
        ranged = num_is_real(bounds[0]) && num_is_finite(bounds[0]);
        expr_free(lower);
    }
    if (ok && ranged && family && !*limits[0] && !*limits[1])
        lab_worker_plot_frame(sources, count, axis, bounds);
    bool feature_frame = ok && ranged && equation && !strcmp(argv[7], "auto") &&
                         lab_worker_plot_equation_frame(curves, sources, axis, !*argv[5] && !*argv[6], bounds);
    if (feature_frame) {
        ymin = num_clone(bounds[2]);
        ymax = num_clone(bounds[3]);
    }
    plot_options_t options = plot_options_default();
    options.x_label = axis;
    options.y_label = "";
    options.title = "";
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.legend = !equation && !family && count > 1;
    options.smart_y = !strcmp(argv[7], "auto");
    options.guard_gaps = true;
    plot_t *plot = ok && ranged ? plot_new(&options) : NULL;
    ok = ok && plot;
    static const unsigned colours[] = {
        0x78d8cb, 0xe8bd65, 0xd68cc4, 0x8caff0, 0xa9d879, 0xf0a06a, 0xb2a1ec, 0xe0dbb5,
        0xec8294, 0x70bad8, 0xd0ce70, 0xa6d8ba, 0xd6a9d1, 0xb5c9e8, 0xd8b393, 0xc1d3c9
    };
    size_t drawn = 0;
    for (size_t i = 0; ok && i < count; ++i) {
        size_t precision = 53;
        sampler_t *samples = lab_worker_plot_samples_quality(sources[i], axis, bounds[0], bounds[1], ymin, ymax,
                                                              &precision, !strcmp(argv[17], "preview"));
        bool finite = false;
        /* At most 8193 points per curve; reject an entirely unavailable branch rather than hide it. */
        for (size_t j = 0; samples && !finite && j < sampler_count(samples); ++j)
            finite = sampler_point(samples, j)->valid;
        /* A representative parameter choice may have no real points in this viewport. */
        if (!finite && family && (samples || errno == ENODATA || errno == EDOM || errno == ENXIO)) {
            sampler_free(samples);
            continue;
        }
        if (precision > drawing_precision)
            drawing_precision = precision;
        char label[40];
        if (count > 1)
            snprintf(label, sizeof label, "Solution %zu", i + 1);
        else
            label[0] = '\0';
        const char *name = labels ? string_c_str(labels[i]) : label;
        ok = finite && plot_add_sampler(plot, samples, equation ? sources[i] : name,
                                          PLOT_LINES, colours[i]);
        drawn += ok;
        sampler_free(samples);
    }
    num_set_default_prec_bits(drawing_precision);
    ok = ok && drawn > 0;
    if (ok && (manual || feature_frame))
        ok = plot_set_bounds(plot, bounds[0], bounds[1], ymin, ymax);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    if (svg) {
        printf("input %s\nexpression %s\nplot_title %s\nplot_title_TeX %s\nplot_svg %s\ndifferentiable no\n",
               input, input, title_text ? title_text : "", title ? title : "", string_c_str(svg));
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
    string_free(interval_limits[1]);
    string_free(interval_limits[0]);
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
    if ((strcmp(argv[10], "auto") && strcmp(argv[10], "curve") && strcmp(argv[10], "surface")) ||
        (strcmp(argv[17], "full") && strcmp(argv[17], "preview"))) {
        printf("error Select a valid plot type and quality.\n");
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
        for (size_t i = 0; i < expr_bindings_count(bindings); ++i)
            if (!expr_bindings_is_constant_at(bindings, i))
                print_axis(expr_bindings_name_at(bindings, i));
        expr_t *second = NULL, *third = NULL;
        bool unresolved = false;
        /* The parsed binding catalogue is bounded by the authored expression; inspect each parameter once. */
        for (size_t i = 0; i < expr_bindings_count(bindings); ++i) {
            expr_t *node = expr_bindings_get(bindings, expr_bindings_name_at(bindings, i));
            if (node == variable)
                continue;
            number_t value = expr_eval(node);
            bool fixed = num_is_real(value) && num_is_finite(value);
            num_destroy(&value);
            if (fixed)
                continue;
            if (expr_bindings_is_constant_at(bindings, i) || third)
                unresolved = true;
            else if (second)
                third = node;
            else
                second = node;
        }
        if (unresolved) {
            equ_free(equ);
            printf("error Bind remaining parameters; an equation plot supports at most three free coordinates.\n");
            return 1;
        }
        if (third) {
            if (*argv[11] && !strcmp(argv[11], expr_symbol_name(third))) {
                expr_t *swap = second;
                second = third;
                third = swap;
            }
            int rc = lab_worker_equation_mesh(equ, variable, second, third, argv);
            equ_free(equ);
            return rc;
        }
        if (second) {
            int rc = lab_worker_equation_contour(equ, variable, second, argv);
            equ_free(equ);
            return rc;
        }
        lab_plot_prepared_t *prepared = lab_worker_plot_prepared_read(argc, argv);
        if (prepared && prepared->family) {
            int rc = lab_worker_plot_integer_families((const expr_t *const *)prepared->curves, prepared->labels,
                                                      prepared->count, axis, string_c_str(prepared->title),
                                                      string_c_str(prepared->TeX), argv);
            if (!rc)
                fputs(string_c_str(prepared->numeric), stdout);
            lab_worker_plot_prepared_free(prepared);
            equ_free(equ);
            return rc;
        }
        if (prepared && prepared->count == 2 && !prepared->family) {
            int rc = prepared->root_count
                ? lab_worker_plot_roots(prepared->roots, prepared->root_count, equ, axis,
                                         string_c_str(prepared->title), string_c_str(prepared->TeX), argv)
                : plot_curves(argv[1], string_c_str(prepared->title), string_c_str(prepared->TeX), axis,
                                (const expr_t *const *)prepared->curves, NULL, 2, true, false, argv, NULL);
            if (!rc)
                fputs(string_c_str(prepared->numeric), stdout);
            lab_worker_plot_prepared_free(prepared);
            equ_free(equ);
            return rc;
        }
        lab_worker_plot_prepared_free(prepared);
        char *title = equ_to_TeX_body_wrapped(equ, 110);
        size_t root_count = 0, family_count = 0;
        number_t *roots = NULL;
        expr_t *families[16] = {0};
        string_t *parameters[16] = {0};
        string_t *numeric = lab_worker_equation_plot_solutions(equ, atoi(argv[3]), &roots, &root_count,
                                                               families, parameters, &family_count);
        int rc = family_count
                     ? lab_worker_plot_integer_families((const expr_t *const *)families, parameters, family_count,
                                                         axis, argv[1], title, argv)
                     : root_count ? lab_worker_plot_roots(roots, root_count, equ, axis, argv[1], title, argv)
                           : plot_curves(argv[1], argv[1], title, axis, curves, NULL, 2, true, false, argv, NULL);
        if (!rc) {
            if (numeric) {
                fputs(string_c_str(numeric), stdout);
                if (family_count)
                    lab_worker_plot_prepared_write(argc, argv, argv[1], title, (const expr_t *const *)families,
                                                   parameters, family_count, true, numeric, NULL, 0);
                else
                    lab_worker_plot_prepared_write(argc, argv, argv[1], title, curves, NULL, 2, false,
                                                   numeric, roots, root_count);
            }
        }
        string_free(numeric);
        for (size_t i = 0; i < root_count; ++i)
            num_destroy(&roots[i]);
        free(roots);
        for (size_t i = 0; i < family_count; ++i) {
            expr_free(families[i]);
            string_free(parameters[i]);
        }
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
    if (!lab_worker_de_plot_parameters(de, argc > 19 && strcmp(argv[19], "--plot-cache") ? argv[19] : NULL)) {
        de_free(de);
        return 1;
    }
    lab_plot_prepared_t *prepared = dimensions == 1 ? lab_worker_plot_prepared_read(argc, argv) : NULL;
    if (prepared) {
        print_axis(axis);
        int rc = plot_curves(argv[1], string_c_str(prepared->title), string_c_str(prepared->TeX), axis,
                             (const expr_t *const *)prepared->curves, prepared->labels, prepared->count,
                             false, prepared->family, argv, de);
        lab_worker_plot_prepared_free(prepared);
        de_free(de);
        return rc;
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
        string_t *title_text = string_new(), *title_TeX = string_new();
        bool titled = title_text && title_TeX;
        diffequ_t *authored = de_from_string(argv[1]);
        expr_bindings_t *coefficients = authored ? de_constants(authored) : NULL;
        bool parameter_title = authored && (de_condition_count(authored) || expr_bindings_count(coefficients));
        if (parameter_title) {
            string_free(title_text);
            string_free(title_TeX);
            title_text = de_to_text(authored, style_EXPRESSION);
            title_TeX = de_problem_to_TeX(authored);
            titled = title_text && title_TeX;
        }
        de_free(authored);
        if (titled && !parameter_title && count > 1)
            titled = string_append_cstr(title_TeX, "\\begin{aligned}") >= 0;
        for (size_t i = 0; titled && !parameter_title && i < count; ++i) {
            const equation_t *solution = de_solve_result_at(solutions, i);
            string_t *text = equ_to_text(solution, style_UNBOUND);
            char *TeX = equ_to_TeX_body_wrapped(solution, 110);
            titled = text && TeX &&
                     string_append_format(title_text, "%s%s", i ? "; " : "", string_c_str(text)) >= 0 &&
                     string_append_format(title_TeX, "%s%s", i ? "\\\\ " : "", TeX) >= 0;
            free(TeX);
            string_free(text);
        }
        if (titled && !parameter_title && count > 1)
            titled = string_append_cstr(title_TeX, "\\end{aligned}") >= 0;
        expr_t *members[16] = {NULL};
        string_t *labels[16] = {NULL};
        size_t member_count = 0;
        bool family = false;
        bool expanded = titled && lab_worker_plot_family(de_constants(de), axis, curves, count,
                                                          members, labels, &member_count, &family);
        if (expanded && family)
            titled = string_append_cstr(title_text, " — representative real members") >= 0 &&
                     string_append_cstr(title_TeX, "\\quad\\text{representative real members}") >= 0;
        print_axis(axis);
        if (titled && expanded)
            rc = plot_curves(argv[1], string_c_str(title_text), string_c_str(title_TeX), axis,
                              (const expr_t *const *)members, labels, member_count, false, family, argv, de);
        else if (!titled)
            printf("error Unable to format the solution plot title.\n");
        if (!rc)
            lab_worker_plot_prepared_write(argc, argv, string_c_str(title_text), string_c_str(title_TeX),
                                           (const expr_t *const *)members, labels, member_count, family, NULL, NULL, 0);
        for (size_t i = 0; i < member_count; ++i) {
            expr_free(members[i]);
            string_free(labels[i]);
        }
        string_free(title_TeX);
        string_free(title_text);
    } else if (solutions && de_solve_result_status(solutions) == DE_SOLVE_STATUS_QUADRATURE) {
        print_axis(axis);
        rc = lab_worker_de_trajectory_plot(de, argv);
    } else {
        printf("error Plotting requires explicit real solution functions; implicit, series-only or unsupported "
               "solutions cannot yet be plotted.\n");
    }
    de_solve_result_free(solutions);
    de_free(de);
    return rc;
}
