/**
 * @file lab_worker_plot.c
 * @brief Native expression-to-SVG plotting in the isolated Lab calculation process.
 *
 * Bounds and bindings are interpreted by MARS, never JavaScript. Sampler work and
 * output size are bounded; images are produced in memory without temporary files.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plot-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

/* Produce one real-valued curve while preserving the worksheet's original expression. */
int lab_worker_plot(const char *source, const expr_t *expression, const expr_t *variable, const char *lower, const char *upper,
                    const char *scale, const char *y_lower, const char *y_upper)
{
    if (!variable || !expr_is_variable(variable)) {
        printf("error       Select a variable to plot.\n");
        return 1;
    }
    size_t worksheet_precision = num_get_default_prec_bits(), plotting_precision = 53;
    number_t lo = NUM_NAN, hi = NUM_NAN, ymin = NUM_NAN, ymax = NUM_NAN;
    bool manual = !strcmp(scale, "manual");
    if ((strcmp(scale, "auto") && strcmp(scale, "all") && !manual) ||
        (manual && !lab_worker_plot_bounds(y_lower, y_upper, &ymin, &ymax))) {
        num_destroy(&ymax);
        num_destroy(&ymin);
        num_destroy(&hi);
        num_destroy(&lo);
        printf("error       Select a valid Y scale and finite manual bounds with minimum < maximum.\n");
        return 1;
    }
    const char *limits[] = {lower, upper, "", ""};
    number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    bool ranged = lab_worker_plot_ranges(source, expr_symbol_name(variable), NULL, limits, bounds);
    lo = bounds[0];
    hi = bounds[1];
    sampler_t *samples = ranged ?
        lab_worker_plot_samples(source, expr_symbol_name(variable), lo, hi, ymin, ymax, &plotting_precision) : NULL;
    /* Drawing also needs the range's precision, but never the worksheet's unnecessarily large default. */
    num_set_default_prec_bits(plotting_precision);
    plot_options_t options = plot_options_default();
    options.x_label = "";
    options.y_label = "";
    options.title = "";
    options.axis_colour = 0x94b8b1;
    options.grid_colour = 0x82aaa3;
    options.label_colour = 0xb8ccc5;
    options.legend = false;
    options.smart_y = !strcmp(scale, "auto");
    plot_t *plot = samples ? plot_new(&options) : NULL;
    bool ok = plot && plot_add_sampler(plot, samples, "", PLOT_LINES, 0x78d8cb);
    if (ok && manual)
        ok = plot_set_bounds(plot, lo, hi, ymin, ymax);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    int saved = errno;
    num_set_default_prec_bits(worksheet_precision);
    int result = lab_worker_plot_output(source, expression, svg, saved);
    string_free(svg);
    plot_free(plot);
    sampler_free(samples);
    num_destroy(&ymax);
    num_destroy(&ymin);
    num_destroy(&hi);
    num_destroy(&lo);
    errno = saved;
    return result;
}

/* Curves and surfaces use the same native formula title and worksheet representation. */
int lab_worker_plot_output(const char *source, const expr_t *expression, const string_t *svg, int saved)
{
    char *text = svg ? expr_to_string(expression, style_EXPRESSION) : NULL;
    char *title = svg ? expr_to_string(expression, style_UNBOUND) : NULL;
    string_t *title_TeX = svg ? expr_source_TeX_internal(source) : NULL;
    if (svg && text && title && title_TeX) {
        printf("expression  %s\n", text);
        printf("plot_title  %s\n", title);
        printf("plot_title_TeX %s\n", string_c_str(title_TeX));
        printf("plot_svg    %s\n", string_c_str(svg));
        printf("differentiable  yes\n");
    } else {
        const char *message = saved == ENXIO ? "Unable to infer a useful real plotting range for this expression. "
                                             "Supply bounds and bind any remaining parameters." :
                              saved == EINVAL ? "Plot bounds must be finite real values with lower < upper." :
                              saved == ENODATA ? "No finite real points found in the sampled range. Check the formula's "
                                                "domain, convergence and any unset parameters, or change the range." :
                              saved == E2BIG ? "Plot sampling budget exhausted; narrow the range." :
                              saved == ERANGE ? "Plot range or evaluation exceeds the available plotting precision." :
                              "Unable to produce the plot within the available calculation limits.";
        printf("error       %s\n", message);
    }
    bool ok = svg && text && title && title_TeX;
    string_free(title_TeX);
    free(title);
    free(text);
    return ok ? 0 : 1;
}
