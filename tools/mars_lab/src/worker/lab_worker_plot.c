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
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

static number_t plot_bound(const char *source)
{
    expr_t *expression = expr_from_string(source, NULL);
    number_t value = expression ? expr_eval(expression) : num_clone(NUM_NAN);
    expr_free(expression);
    return value;
}

/* Produce one real-valued curve while preserving the worksheet's original expression. */
int lab_worker_plot(const expr_t *expression, const expr_t *variable, const char *lower, const char *upper,
                    const char *scale, const char *y_lower, const char *y_upper)
{
    if (!variable || !expr_is_variable(variable)) {
        printf("error       Select a variable to plot.\n");
        return 1;
    }
    number_t lo = plot_bound(lower), hi = plot_bound(upper);
    bool manual = !strcmp(scale, "manual");
    number_t ymin = manual ? plot_bound(y_lower) : NUM_NAN, ymax = manual ? plot_bound(y_upper) : NUM_NAN;
    if ((strcmp(scale, "auto") && strcmp(scale, "all") && !manual) ||
        (manual && (!num_is_real(ymin) || !num_is_finite(ymin) || !num_is_real(ymax) ||
                    !num_is_finite(ymax) || !num_lt(ymin, ymax)))) {
        num_destroy(&ymax);
        num_destroy(&ymin);
        num_destroy(&hi);
        num_destroy(&lo);
        printf("error       Select a valid Y scale and finite manual bounds with minimum < maximum.\n");
        return 1;
    }
    sampler_options_t sampling = sampler_options_default();
    sampling.intervals = 64;
    sampling.max_evaluations = 8193;
    sampling.max_depth = 8;
    sampling.absolute_tolerance = 1e-5;
    sampling.relative_tolerance = 0.002;
    sampler_t *samples = sampler_expression(expression, variable, NULL, lo, hi, &sampling);
    plot_options_t options = plot_options_default();
    options.x_label = expr_symbol_name(variable);
    options.y_label = "f";
    options.title = "Function plot";
    options.legend = false;
    options.smart_y = !strcmp(scale, "auto");
    plot_t *plot = samples ? plot_new(&options) : NULL;
    bool ok = plot && plot_add_sampler(plot, samples, "", PLOT_LINES, 0xd9b45f);
    if (ok && manual)
        ok = plot_set_bounds(plot, lo, hi, ymin, ymax);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    int saved = errno;
    char *text = svg ? expr_to_string(expression, style_EXPRESSION) : NULL;
    if (svg && text) {
        printf("expression  %s\n", text);
        printf("plot_svg    %s\n", string_c_str(svg));
        printf("differentiable  yes\n");
    } else {
        const char *message = saved == EINVAL ? "Plot bounds must be finite real values with lower < upper." :
                              saved == ENODATA ? "No finite real points: bind the other parameters or change the range." :
                              saved == E2BIG ? "Plot sampling budget exhausted; narrow the range." :
                              "Unable to produce the plot within the available calculation limits.";
        printf("error       %s\n", message);
    }
    ok = svg && text;
    free(text);
    string_free(svg);
    plot_free(plot);
    sampler_free(samples);
    num_destroy(&ymax);
    num_destroy(&ymin);
    num_destroy(&hi);
    num_destroy(&lo);
    errno = saved;
    return ok ? 0 : 1;
}
