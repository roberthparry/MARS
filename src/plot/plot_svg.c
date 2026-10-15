/**
 * @file plot_svg.c
 * @brief In-memory SVG axes, clipped curves, markers and escaped annotations.
 *
 * Values are normalised with number_t before drawing conversion. Each coordinate
 * is visited once per rendering stage. SVG syntax uses a thread-local C numeric
 * locale; the caller's locale is restored on every exit. No external renderer,
 * temporary file, script, hyperlink or embedded resource is used.
 * Series names remain available as native hover tooltips when legends are hidden.
 */
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

typedef struct {
    string_t *out;
    number_t *bounds;
    number_t spans[2];
    double left, top, width, height;
} svg_view_t;

bool plot_escape(string_t *out, const string_t *text)
{
    const unsigned char *bytes = (const unsigned char *)string_c_str(text);
    size_t count = string_byte_length(text);
    for (size_t i = 0; i < count; ++i) {
        unsigned char c = bytes[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            errno = EINVAL;
            return false;
        }
        const char *escaped = c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : NULL;
        if (escaped ? string_append_cstr(out, escaped) != 0 : string_append_bytes(out, (const char *)&bytes[i], 1) != 0)
            return false;
    }
    return true;
}

static bool text(svg_view_t *view, double x, double y, const char *anchor, const string_t *label)
{
    return string_append_format(view->out, "<text x=\"%.3f\" y=\"%.3f\" text-anchor=\"%s\">", x, y, anchor) >= 0 &&
           plot_escape(view->out, label) && string_append_cstr(view->out, "</text>\n") == 0;
}

static bool normalise(number_t value, number_t low, number_t span, double *out)
{
    number_t delta = num_sub(value, low), ratio = num_div(delta, span);
    *out = num_to_double(ratio);
    bool ok = num_is_real(ratio) && num_is_finite(ratio) && isfinite(*out);
    num_destroy(&ratio);
    num_destroy(&delta);
    if (!ok)
        errno = ERANGE;
    return ok;
}

static bool position(svg_view_t *view, const plot_point_t *point, double *x, double *y)
{
    return normalise(point->x, view->bounds[0], view->spans[0], x) &&
           normalise(point->y, view->bounds[2], view->spans[1], y);
}

static bool line(svg_view_t *view, double x1, double y1, double x2, double y2)
{
    return string_append_format(view->out, "<path d=\"M %.3f %.3f L %.3f %.3f\"/>\n", x1, y1, x2, y2) >= 0;
}


static bool valid(const plot_point_t *point)
{
    return num_is_real(point->x) && num_is_finite(point->x) && num_is_real(point->y) && num_is_finite(point->y);
}

string_t *plot_tick_label(number_t value)
{
    /* Axis labels are decimal approximations, even when the tick is an exact fraction. */
    number_t decimal = num_as_inexact_real_prec(value, num_get_effective_prec_bits(value));
    string_t *label = num_is_finite(decimal) ? num_sprintf_text("%.6n", decimal) : NULL;
    num_destroy(&decimal);
    if (!label)
        return NULL;
    const char *source = string_c_str(label), *exponent = strpbrk(source, "eE");
    if (!exponent)
        return label;
    const char *digits = exponent + 1;
    bool negative = *digits == '-';
    if (*digits == '+' || *digits == '-')
        ++digits;
    if (!*digits || strspn(digits, "0123456789") != strlen(digits))
        return label;
    string_t *result = string_new();
    bool ok = result && string_append_bytes(result, source, (size_t)(exponent - source)) == 0 &&
              string_append_cstr(result, "×10") == 0;
    while (*digits == '0' && digits[1])
        ++digits;
    if (negative)
        ok = ok && string_append_cstr(result, "⁻") == 0;
    static const char *const superscripts[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    while (ok && *digits)
        ok = string_append_cstr(result, superscripts[(unsigned)(*digits++ - '0')]) == 0;
    string_free(label);
    if (!ok) {
        string_free(result);
        return NULL;
    }
    return result;
}

static bool draw_lines(svg_view_t *view, const plot_series_t *series)
{
    const plot_point_t *previous = NULL;
    for (size_t i = 0; i < series->count; ++i) {
        if (!computation_step_current("plot.lines"))
            return false;
        const plot_point_t *point = &series->points[i];
        if (!valid(point)) {
            previous = NULL;
            continue;
        }
        if (previous && !point->break_before) {
            double xy[4];
            int clipped = plot_clip_segment(previous, point, view->bounds, xy);
            if (clipped < 0 || (clipped &&
                !line(view, view->left + xy[0] * view->width, view->top + (1 - xy[1]) * view->height,
                      view->left + xy[2] * view->width, view->top + (1 - xy[3]) * view->height)))
                return false;
        }
        previous = point;
    }
    return true;
}

static bool draw_points(svg_view_t *view, const plot_series_t *series)
{
    for (size_t i = 0; i < series->count; ++i) {
        if (!computation_step_current("plot.points"))
            return false;
        const plot_point_t *point = &series->points[i];
        if (!valid(point))
            continue;
        if (num_lt(point->y, view->bounds[2]) || num_gt(point->y, view->bounds[3])) {
            continue;
        }
        if (num_lt(point->x, view->bounds[0]) || num_gt(point->x, view->bounds[1]))
            continue;
        double x, y;
        if (!position(view, point, &x, &y))
            return false;
        if (x >= 0 && x <= 1 && y >= 0 && y <= 1 &&
            string_append_format(view->out, "<circle cx=\"%.3f\" cy=\"%.3f\" r=\"2.5\"/>\n",
                                  view->left + x * view->width, view->top + (1 - y) * view->height) < 0)
            return false;
    }
    return true;
}

static bool axes(svg_view_t *view, const plot_t *plot)
{
    plot_tick_layout_t labels = {.width = plot->options.width, .height = plot->options.height, .font_size = 12};
    if (string_append_format(view->out, "<g fill=\"#%06x\" stroke=\"none\" font-family=\"sans-serif\" font-size=\"12\">\n",
                             (unsigned)plot->options.label_colour) < 0)
        return false;
    bool ok = text(view, plot->options.width / 2.0, 24, "middle", plot->title) &&
              text(view, view->left + view->width / 2, view->top + view->height + 46, "middle", plot->x_label) &&
              text(view, 12, 40, "start", plot->y_label);
    for (unsigned axis = 0; ok && axis < 2; ++axis) {
        number_t ticks[12];
        size_t count = plot_ticks(view->bounds[2 * axis], view->bounds[2 * axis + 1], ticks);
        ok = count > 0;
        for (size_t i = 0; i < count; ++i) {
            string_t *label = plot_tick_label(ticks[i]);
            double position = 0;
            ok = ok && label && normalise(ticks[i], view->bounds[2 * axis], view->spans[axis], &position) &&
                 plot_tick_text(view->out, &labels, label, axis,
                      axis ? view->left - 12 : view->left + position * view->width,
                      axis ? view->top + (1 - position) * view->height + 4 : view->top + view->height + 20,
                      axis ? -1 : 0, 0, 0);
            if (ok) {
                double x = axis ? view->left : view->left + position * view->width;
                double y = axis ? view->top + (1 - position) * view->height : view->top + view->height;
                ok = string_append_format(view->out, "<g data-plot-tick=\"%u\" stroke=\"#%06x\" "
                                          "stroke-width=\"1.1\">", axis, (unsigned)plot->options.axis_colour) >= 0 &&
                     line(view, x, y, axis ? x - 5 : x, axis ? y : y + 5) &&
                     string_append_cstr(view->out, "</g>\n") == 0;
            }
            if (ok && plot->options.grid && position > 0 && position < 1 && !num_is_zero(ticks[i])) {
                double x = view->left + position * view->width, y = view->top + (1 - position) * view->height;
                ok = string_append_format(view->out, "<g data-plot-grid=\"true\" stroke=\"#%06x\" "
                                          "stroke-opacity=\"0.2\" stroke-dasharray=\"3 6\">",
                                          (unsigned)plot->options.grid_colour) >= 0 &&
                     (axis ? line(view, view->left, y, view->left + view->width, y) :
                             line(view, x, view->top, x, view->top + view->height)) &&
                     string_append_cstr(view->out, "</g>\n") == 0;
            }
            string_free(label);
            num_destroy(&ticks[i]);
        }
    }
    if (!ok || string_append_format(view->out, "</g>\n<g data-plot-axes=\"true\" fill=\"none\" stroke=\"#%06x\" "
                                    "stroke-opacity=\"0.75\" stroke-width=\"1.1\">\n",
                                    (unsigned)plot->options.axis_colour) < 0)
        return false;
    ok = line(view, view->left, view->top + view->height, view->left + view->width, view->top + view->height) &&
         line(view, view->left, view->top, view->left, view->top + view->height);
    for (unsigned axis = 0; ok && axis < 2; ++axis) {
        /* Boundary axes already exist; keep zero guides inside the open top/right edges. */
        if (num_lt(view->bounds[2 * axis], NUM_ZERO) && num_gt(view->bounds[2 * axis + 1], NUM_ZERO)) {
            double zero;
            ok = normalise(NUM_ZERO, view->bounds[2 * axis], view->spans[axis], &zero);
            if (ok)
                ok = axis ? line(view, view->left, view->top + (1 - zero) * view->height,
                                  view->left + view->width, view->top + (1 - zero) * view->height) :
                            line(view, view->left + zero * view->width, view->top,
                                  view->left + zero * view->width, view->top + view->height);
        }
    }
    return ok && string_append_cstr(view->out, "</g>\n") == 0;
}

static bool render(svg_view_t *view, const plot_t *plot)
{
    static bool (*const drawers[])(svg_view_t *, const plot_series_t *) = {draw_lines, draw_points};
    double label_width = plot_tick_width(view->bounds[2], view->bounds[3], 12);
    double x_width = plot_tick_width(view->bounds[0], view->bounds[1], 12);
    if (label_width < 0 || x_width < 0)
        return false;
    view->left = fmax(view->left, fmin(label_width + 16, plot->options.width - 112));
    view->left = fmax(view->left, fmin(x_width / 2 + 4, plot->options.width / 3.0));
    double right = fmax(32, fmin(x_width / 2 + 4, plot->options.width / 3.0));
    view->width = plot->options.width - view->left - right;
    if (string_append_format(view->out,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" viewBox=\"0 0 %u %u\" role=\"img\" "
        "data-plot-left=\"%.3f\" data-plot-top=\"%.3f\" data-plot-width=\"%.3f\" data-plot-height=\"%.3f\" "
        "data-x-min=\"%.17g\" data-x-max=\"%.17g\" data-y-min=\"%.17g\" data-y-max=\"%.17g\">\n<title>",
        plot->options.width, plot->options.height, plot->options.width, plot->options.height,
        view->left, view->top, view->width, view->height,
        num_to_double(view->bounds[0]), num_to_double(view->bounds[1]),
        num_to_double(view->bounds[2]), num_to_double(view->bounds[3])) < 0 ||
        !plot_escape(view->out, plot->title) || string_append_cstr(view->out, "</title>\n") || !axes(view, plot))
        return false;
    for (size_t i = 0; i < plot->count; ++i) {
        const plot_series_t *series = &plot->series[i];
        /* A local SVG viewport clips transient browser translations without moving axes or labels. */
        if (string_append_format(view->out,
            "<svg data-plot-window=\"true\" x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" "
            "viewBox=\"%.3f %.3f %.3f %.3f\" style=\"width:%.3fpx;height:%.3fpx;"
            "transform:none;margin:0;filter:none;overflow:hidden\">\n",
            view->left, view->top, view->width, view->height, view->left, view->top, view->width, view->height,
            view->width, view->height) < 0)
            return false;
        double frame[] = {view->left, view->top, view->width, view->height};
        if (string_append_format(view->out, "<g data-series=\"%zu\" stroke=\"#%06x\" color=\"#%06x\" "
                                           "fill=\"%s\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n",
                                  i, (unsigned)series->colour, (unsigned)series->colour,
                                  series->style == PLOT_POINTS ? "currentColor" : "none") < 0 ||
            string_append_cstr(view->out, "<title>") || !plot_escape(view->out, series->name) ||
            string_append_cstr(view->out, "</title>\n") ||
            !plot_shade_svg(plot, series, view->out, view->bounds, frame) ||
            !drawers[series->style](view, series) || string_append_cstr(view->out, "</g></svg>\n"))
            return false;
        if (plot->options.legend && string_byte_length(series->name)) {
            double y = view->top + 16 + 18 * i;
            if (y > view->top + view->height - 8)
                continue;
            if (string_append_format(view->out, "<g fill=\"#%06x\" font-family=\"sans-serif\" font-size=\"12\">",
                                      (unsigned)series->colour) < 0 ||
                !text(view, view->left + view->width - 8, y, "end", series->name) ||
                string_append_cstr(view->out, "</g>\n"))
                return false;
        }
    }
    return string_append_cstr(view->out, "</svg>\n") == 0;
}

/* Render into one owned memory buffer and restore the caller's numeric locale. */
string_t *plot_to_svg(const plot_t *plot)
{
    if (!plot) {
        errno = EINVAL;
        return NULL;
    }
    number_t bounds[] = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    svg_view_t view = {.bounds = bounds, .spans = {NUM_NAN, NUM_NAN}, .left = 96, .top = 48,
                       .width = plot->options.width - 128, .height = plot->options.height - 112};
    bool surface = plot->surface_z != NULL;
    bool ok = surface || plot_ranges(plot, bounds);
    if (ok && !surface) {
        view.spans[0] = num_sub(bounds[1], bounds[0]);
        view.spans[1] = num_sub(bounds[3], bounds[2]);
        ok = num_is_finite(view.spans[0]) && num_is_finite(view.spans[1]);
        if (!ok)
            errno = ERANGE;
    }
    locale_t locale = ok ? newlocale(LC_NUMERIC_MASK, "C", (locale_t)0) : (locale_t)0;
    locale_t previous = locale ? uselocale(locale) : (locale_t)0;
    if (ok && surface)
        ok = locale && previous && (view.out = plot_surface_svg(plot)) && computation_poll_current();
    else if (ok)
        ok = locale && previous && (view.out = string_new()) && render(&view, plot) && computation_poll_current();
    int saved = errno;
    if (previous)
        uselocale(previous);
    if (locale)
        freelocale(locale);
    for (size_t i = 0; i < 4; ++i)
        num_destroy(&bounds[i]);
    num_destroy(&view.spans[0]);
    num_destroy(&view.spans[1]);
    if (!ok) {
        string_free(view.out);
        view.out = NULL;
    }
    errno = saved;
    return view.out;
}
