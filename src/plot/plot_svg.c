/**
 * @file plot_svg.c
 * @brief In-memory SVG axes, clipped curves, markers and escaped annotations.
 *
 * Values are normalised with number_t before drawing conversion. Each coordinate
 * is visited once per rendering stage. SVG syntax uses a thread-local C numeric
 * locale; the caller's locale is restored on every exit. No external renderer,
 * temporary file, script, hyperlink or embedded resource is used.
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
    bool clipped_y;
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
    bool ok = num_is_real(ratio) && num_is_finite(ratio) && isfinite(*out) && fabs(*out) <= 1e100;
    num_destroy(&ratio);
    num_destroy(&delta);
    if (!ok)
        errno = ERANGE;
    return ok;
}

static bool position(svg_view_t *view, const plot_point_t *point, double *x, double *y)
{
    if (num_lt(point->y, view->bounds[2]) || num_gt(point->y, view->bounds[3]))
        view->clipped_y = true;
    return normalise(point->x, view->bounds[0], view->spans[0], x) &&
           normalise(point->y, view->bounds[2], view->spans[1], y);
}

static bool line(svg_view_t *view, double x1, double y1, double x2, double y2)
{
    return string_append_format(view->out, "<path d=\"M %.3f %.3f L %.3f %.3f\"/>\n", x1, y1, x2, y2) >= 0;
}

/* Liang-Barsky clipping visits the four viewport boundaries, without sampling the segment. */
static bool clip(double *x1, double *y1, double *x2, double *y2)
{
    double dx = *x2 - *x1, dy = *y2 - *y1, start = 0, end = 1;
    double p[] = {-dx, dx, -dy, dy}, q[] = {*x1, 1 - *x1, *y1, 1 - *y1};
    for (unsigned i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0)
                return false;
        } else {
            double ratio = q[i] / p[i];
            if (p[i] < 0)
                start = fmax(start, ratio);
            else
                end = fmin(end, ratio);
            if (start > end)
                return false;
        }
    }
    double x = *x1, y = *y1;
    *x1 = fmin(1, fmax(0, x + start * dx));
    *y1 = fmin(1, fmax(0, y + start * dy));
    *x2 = fmin(1, fmax(0, x + end * dx));
    *y2 = fmin(1, fmax(0, y + end * dy));
    return true;
}

static bool valid(const plot_point_t *point)
{
    return num_is_real(point->x) && num_is_finite(point->x) && num_is_real(point->y) && num_is_finite(point->y);
}

static string_t *tick_label(number_t value)
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
    bool previous = false;
    double px = 0, py = 0;
    for (size_t i = 0; i < series->count; ++i) {
        if (!computation_step_current("plot.lines"))
            return false;
        const plot_point_t *point = &series->points[i];
        if (!valid(point)) {
            previous = false;
            continue;
        }
        double x, y;
        if (!position(view, point, &x, &y))
            return false;
        if (previous && !point->break_before) {
            double x1 = px, y1 = py, x2 = x, y2 = y;
            if (clip(&x1, &y1, &x2, &y2) &&
                !line(view, view->left + x1 * view->width, view->top + (1 - y1) * view->height,
                      view->left + x2 * view->width, view->top + (1 - y2) * view->height))
                return false;
        }
        px = x;
        py = y;
        previous = true;
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
    if (string_append_cstr(view->out, "<g fill=\"currentColor\" stroke=\"none\" font-family=\"sans-serif\" font-size=\"12\">\n"))
        return false;
    bool ok = text(view, plot->options.width / 2.0, 24, "middle", plot->title) &&
              text(view, view->left + view->width / 2, view->top + view->height + 46, "middle", plot->x_label) &&
              text(view, 12, 40, "start", plot->y_label);
    for (unsigned axis = 0; ok && axis < 2; ++axis) {
        number_t ticks[12];
        size_t count = plot_ticks(view->bounds[2 * axis], view->bounds[2 * axis + 1], ticks);
        ok = count > 0;
        for (size_t i = 0; i < count; ++i) {
            string_t *label = tick_label(ticks[i]);
            double position = 0;
            ok = ok && label && normalise(ticks[i], view->bounds[2 * axis], view->spans[axis], &position) &&
                 text(view, axis ? view->left - 8 : view->left + position * view->width,
                      axis ? view->top + (1 - position) * view->height + 4 : view->top + view->height + 20,
                      axis ? "end" : "middle", label);
            if (ok && plot->options.grid) {
                double x = view->left + position * view->width, y = view->top + (1 - position) * view->height;
                ok = string_append_cstr(view->out, "<g stroke=\"currentColor\" stroke-opacity=\"0.2\">") == 0 &&
                     (axis ? line(view, view->left, y, view->left + view->width, y) :
                             line(view, x, view->top, x, view->top + view->height)) &&
                     string_append_cstr(view->out, "</g>\n") == 0;
            }
            string_free(label);
            num_destroy(&ticks[i]);
        }
    }
    if (!ok || string_append_cstr(view->out, "</g>\n<g fill=\"none\" stroke=\"currentColor\">\n"))
        return false;
    ok = string_append_format(view->out, "<rect x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\"/>\n",
                               view->left, view->top, view->width, view->height) >= 0;
    for (unsigned axis = 0; ok && axis < 2; ++axis) {
        if (num_le(view->bounds[2 * axis], NUM_ZERO) && num_ge(view->bounds[2 * axis + 1], NUM_ZERO)) {
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
    if (string_append_format(view->out,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" viewBox=\"0 0 %u %u\" role=\"img\">\n<title>",
        plot->options.width, plot->options.height, plot->options.width, plot->options.height) < 0 ||
        !plot_escape(view->out, plot->title) || string_append_cstr(view->out, "</title>\n") || !axes(view, plot))
        return false;
    for (size_t i = 0; i < plot->count; ++i) {
        const plot_series_t *series = &plot->series[i];
        if (string_append_format(view->out, "<g data-series=\"%zu\" stroke=\"#%06x\" color=\"#%06x\" "
                                           "fill=\"%s\" stroke-width=\"1.5\">\n",
                                  i, (unsigned)series->colour, (unsigned)series->colour,
                                  series->style == PLOT_POINTS ? "currentColor" : "none") < 0 ||
            !drawers[series->style](view, series) || string_append_cstr(view->out, "</g>\n"))
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
    if (view->clipped_y && string_append_format(view->out,
        "<text x=\"%.3f\" y=\"40\" text-anchor=\"end\" fill=\"currentColor\" font-family=\"sans-serif\" "
        "font-size=\"11\">Vertical range clipped</text>\n", view->left + view->width) < 0)
        return false;
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
    bool ok = plot_ranges(plot, bounds);
    if (ok) {
        view.spans[0] = num_sub(bounds[1], bounds[0]);
        view.spans[1] = num_sub(bounds[3], bounds[2]);
        ok = num_is_finite(view.spans[0]) && num_is_finite(view.spans[1]);
        if (!ok)
            errno = ERANGE;
    }
    locale_t locale = ok ? newlocale(LC_NUMERIC_MASK, "C", (locale_t)0) : (locale_t)0;
    locale_t previous = locale ? uselocale(locale) : (locale_t)0;
    if (ok)
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
