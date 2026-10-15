/**
 * @file plot_shade.c
 * @brief Signed interval shading clipped in native precision before SVG conversion.
 *
 * Line segments contribute separate polygons between the sampled curve and zero.
 * The interval, viewport and sign planes clip each polygon without joining gaps.
 * This is a visual annotation only, not numerical integration or convergence analysis.
 */
#include <errno.h>

#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

/* Store an oriented, finite interval independently of the displayed viewport. */
bool plot_shade_interval(plot_t *plot, number_t lower, number_t upper)
{
    if (!plot || plot->surface_z || !num_is_real(lower) || !num_is_real(upper) ||
        !num_is_finite(lower) || !num_is_finite(upper)) {
        errno = EINVAL;
        return false;
    }
    bool reversed = num_gt(lower, upper);
    number_t copies[] = {num_clone(reversed ? upper : lower), num_clone(reversed ? lower : upper)};
    if (!num_is_finite(copies[0]) || !num_is_finite(copies[1])) {
        num_destroy(&copies[0]);
        num_destroy(&copies[1]);
        return false;
    }
    for (unsigned i = 0; i < 2; ++i) {
        num_destroy(&plot->shade_bounds[i]);
        plot->shade_bounds[i] = copies[i];
    }
    plot->shaded = true;
    plot->shade_reversed = reversed;
    return true;
}

static size_t clip(const plot_point_t *input, size_t count, plot_point_t *output,
                    number_t boundary, unsigned axis, bool upper)
{
    if (!count)
        return 0;
    size_t used = 0;
    plot_point_t a = input[count - 1];
    number_t av = axis ? a.y : a.x;
    bool inside = upper ? num_le(av, boundary) : num_ge(av, boundary);
    for (size_t i = 0; i < count; ++i) {
        plot_point_t b = input[i];
        number_t bv = axis ? b.y : b.x;
        bool keep = upper ? num_le(bv, boundary) : num_ge(bv, boundary);
        if (keep != inside) {
            number_t t = num_div(num_sub(boundary, av), num_sub(bv, av));
            output[used++] = (plot_point_t){axis ? num_add(a.x, num_mul(t, num_sub(b.x, a.x))) : boundary,
                                            axis ? boundary : num_add(a.y, num_mul(t, num_sub(b.y, a.y))), false};
        }
        if (keep)
            output[used++] = b;
        a = b;
        av = bv;
        inside = keep;
    }
    return used;
}

/* Clip each trapezoid and its two signs independently, using only bounded drawing coordinates. */
bool plot_shade_svg(const plot_t *plot, const plot_series_t *series, string_t *out,
                     const number_t bounds[4], const double frame[4])
{
    if (!plot->shaded || series->style != PLOT_LINES)
        return true;
    if (string_append_cstr(out, "<g data-integral-shading=\"true\" stroke=\"none\" fill-opacity=\"0.23\" "
                                "pointer-events=\"none\">\n"))
        return false;
    for (size_t i = 1; i < series->count; ++i) {
        const plot_point_t a = series->points[i - 1], b = series->points[i];
        if (b.break_before || !num_is_real(a.x) || !num_is_real(a.y) || !num_is_real(b.x) || !num_is_real(b.y) ||
            !num_is_finite(a.x) || !num_is_finite(a.y) || !num_is_finite(b.x) || !num_is_finite(b.y))
            continue;
        for (unsigned sign = 0; sign < 2; ++sign) {
            NUM_SCOPE(numbers);
            plot_point_t polygon[12] = {{a.x, NUM_ZERO, false}, a, b, {b.x, NUM_ZERO, false}}, next[12];
            number_t lower = num_gt(bounds[0], plot->shade_bounds[0]) ? bounds[0] : plot->shade_bounds[0];
            number_t upper = num_lt(bounds[1], plot->shade_bounds[1]) ? bounds[1] : plot->shade_bounds[1];
            if (!num_lt(lower, upper))
                continue;
            number_t planes[] = {lower, upper, bounds[2], bounds[3], NUM_ZERO};
            size_t count = 4;
            for (unsigned plane = 0; count && plane < 5; ++plane) {
                count = clip(polygon, count, next, planes[plane], plane > 1,
                             plane == 4 ? sign == 1 : (plane % 2) != 0);
                for (size_t j = 0; j < count; ++j)
                    polygon[j] = next[j];
            }
            if (count < 3)
                continue;
            if (string_append_format(out, "<path data-sign=\"%s\" fill=\"#%06x\" d=\"",
                (sign != plot->shade_reversed) ? "negative" : "positive",
                (sign != plot->shade_reversed) ? 0xe8bd65 : 0x78d8cb) < 0)
                return false;
            for (size_t j = 0; j < count; ++j) {
                double x = num_to_double(num_div(num_sub(polygon[j].x, bounds[0]), num_sub(bounds[1], bounds[0])));
                double y = num_to_double(num_div(num_sub(polygon[j].y, bounds[2]), num_sub(bounds[3], bounds[2])));
                if (string_append_format(out, "%c %.3f %.3f ", j ? 'L' : 'M', frame[0] + x * frame[2],
                                          frame[1] + (1 - y) * frame[3]) < 0)
                    return false;
            }
            if (string_append_cstr(out, "Z\"/>\n"))
                return false;
        }
    }
    if (string_append_cstr(out, "</g><g data-integral-limits=\"true\" stroke=\"#e8bd65\" "
                                "stroke-width=\"1\" stroke-dasharray=\"4 4\" pointer-events=\"none\">\n"))
        return false;
    for (unsigned i = 0; i < 2; ++i) {
        NUM_SCOPE(numbers);
        number_t boundary = plot->shade_bounds[i];
        if (num_lt(boundary, bounds[0]) || num_gt(boundary, bounds[1]))
            continue;
        double x = num_to_double(num_div(num_sub(boundary, bounds[0]), num_sub(bounds[1], bounds[0])));
        x = frame[0] + x * frame[2];
        if (string_append_format(out, "<path d=\"M %.3f %.3f L %.3f %.3f\"/>\n",
                                  x, frame[1], x, frame[1] + frame[3]) < 0)
            return false;
    }
    return string_append_cstr(out, "</g>\n") == 0;
}
