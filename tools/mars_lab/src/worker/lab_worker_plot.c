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

/* Leave two MiB of the wire envelope for the mathematical title and other fields. */
bool lab_worker_plot_fits(const string_t *svg)
{
    return svg && string_byte_length(svg) <= 6u * 1024u * 1024u;
}

/* Expose actual sampling quality, not merely the requested result-card mode. */
bool lab_worker_plot_quality(string_t *svg, bool preview, size_t side)
{
    if (!svg || strncmp(string_c_str(svg), "<svg ", 5))
        return false;
    string_t *attributes = string_sprintf("data-plot-quality=\"%s\" data-sample-side=\"%zu\" ",
                                         preview ? "preview" : "full", side);
    bool ok = attributes && string_insert(svg, 5, string_c_str(attributes)) == 0;
    string_free(attributes);
    if (ok && preview) {
        size_t length = string_byte_length(svg);
        while (length && strchr(" \t\r\n", string_c_str(svg)[length - 1]))
            --length;
        ok = length >= 6 && !memcmp(string_c_str(svg) + length - 6, "</svg>", 6) &&
            string_insert(svg, string_length(svg) - (string_byte_length(svg) - length) - 6,
            "<text data-preview-notice=\"true\" x=\"50%\" y=\"95%\" text-anchor=\"middle\" "
            "fill=\"#b8ccc5\" font-family=\"sans-serif\" font-size=\"11\">"
            "Coarse preview — detail returns when the gesture ends</text>") == 0;
    }
    return ok;
}

/* Make reduced sampling visible without changing the requested coordinate volume. */
bool lab_worker_plot_reduced(string_t *svg)
{
    size_t length = svg ? string_byte_length(svg) : 0;
    while (length && strchr(" \t\r\n", string_c_str(svg)[length - 1]))
        --length;
    return length >= 6 && !memcmp(string_c_str(svg) + length - 6, "</svg>", 6) &&
           string_insert(svg, string_length(svg) - (string_byte_length(svg) - length) - 6,
                         "<text data-detail-reduced=\"true\" x=\"50%\" y=\"98%\" text-anchor=\"middle\" "
                         "fill=\"#b8ccc5\" font-family=\"sans-serif\" font-size=\"11\">"
                         "Detail reduced for this view; zoom in for finer sampling</text>") == 0;
}

/* Produce one real-valued curve while preserving the worksheet's original expression. */
int lab_worker_plot(const char *source, const expr_t *expression, const expr_t *variable, const char *lower, const char *upper,
                    const char *scale, const char *y_lower, const char *y_upper, bool preview)
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
        lab_worker_plot_samples_quality(source, expr_symbol_name(variable), lo, hi, ymin, ymax,
                                         &plotting_precision, preview) : NULL;
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
    options.guard_gaps = true;
    plot_t *plot = samples ? plot_new(&options) : NULL;
    bool ok = plot && plot_add_sampler(plot, samples, "", PLOT_LINES, 0x78d8cb);
    if (ok && manual)
        ok = plot_set_bounds(plot, lo, hi, ymin, ymax);
    string_t *svg = ok ? plot_to_svg(plot) : NULL;
    int saved = errno;
    num_set_default_prec_bits(worksheet_precision);
    int result = lab_worker_plot_output(source, expression, svg, saved, NULL, NULL, NULL);
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

static string_t *surface_title(const string_t *body, const expr_t *function, const expr_t *first,
                               const expr_t *second, bool TeX)
{
    /* Title arguments are names, not the worksheet values carried by bound axis nodes. */
    expr_t *x_symbol = expr_new_named_var(NUM_NAN, expr_symbol_name(first));
    expr_t *y_symbol = expr_new_named_var(NUM_NAN, expr_symbol_name(second));
    char *name = TeX ? expr_to_TeX_body(function) : expr_to_string(function, style_UNBOUND);
    char *x = x_symbol ? (TeX ? expr_to_TeX_body(x_symbol) : expr_to_string(x_symbol, style_UNBOUND)) : NULL;
    char *y = y_symbol ? (TeX ? expr_to_TeX_body(y_symbol) : expr_to_string(y_symbol, style_UNBOUND)) : NULL;
    string_t *title = name && x && y ? string_sprintf("%s(%s,%s) = %s", name, x, y, string_c_str(body)) : NULL;
    free(y);
    free(x);
    free(name);
    expr_free(y_symbol);
    expr_free(x_symbol);
    return title;
}

/* Surface titles name the function and its selected axes without altering the worksheet formula. */
int lab_worker_plot_output(const char *source, const expr_t *expression, const string_t *svg, int saved,
                           const expr_t *function, const expr_t *first, const expr_t *second)
{
    char *text = svg ? expr_to_string(expression, style_EXPRESSION) : NULL;
    string_t *title = svg ? expr_to_text(expression, style_UNBOUND) : NULL;
    string_t *title_TeX = svg ? expr_source_TeX_internal(source) : NULL;
    if (function && title && title_TeX) {
        string_t *named = surface_title(title, function, first, second, false);
        string_t *named_TeX = surface_title(title_TeX, function, first, second, true);
        string_free(title);
        string_free(title_TeX);
        title = named;
        title_TeX = named_TeX;
    }
    if (svg && text && title && title_TeX) {
        printf("expression  %s\n", text);
        printf("plot_title  %s\n", string_c_str(title));
        printf("plot_title_TeX %s\n", string_c_str(title_TeX));
        printf("plot_svg    %s\n", string_c_str(svg));
        printf("differentiable  yes\n");
    } else {
        const char *message = saved == ENXIO ? "Automatic range selection failed: the trial windows did not contain "
                                             "enough finite real variation to choose a view. Set explicit axis bounds; "
                                             "this does not establish that the expression cannot be plotted." :
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
    string_free(title);
    free(text);
    return ok ? 0 : 1;
}
