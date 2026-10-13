/**
 * @file plot_clip.c
 * @brief Native-precision segment clipping before SVG drawing conversion.
 *
 * Cohen-Sutherland region codes discard invisible edges without converting huge
 * off-screen values to doubles. Intersections are measured from the nearer endpoint
 * to avoid subtracting a large displacement from a remote coordinate. Only final
 * viewport-relative positions become doubles. Sampler gaps are honoured by callers.
 */
#include <errno.h>
#include <math.h>

#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

static unsigned region(const number_t point[2], const number_t bounds[4])
{
    return (num_lt(point[0], bounds[0]) ? 1u : num_gt(point[0], bounds[1]) ? 2u : 0u) |
           (num_lt(point[1], bounds[2]) ? 4u : num_gt(point[1], bounds[3]) ? 8u : 0u);
}

static bool finite_point(const number_t point[2])
{
    return num_is_real(point[0]) && num_is_finite(point[0]) && num_is_real(point[1]) && num_is_finite(point[1]);
}

int plot_clip_segment(const plot_point_t *first, const plot_point_t *last, const number_t bounds[4], double output[4])
{
    NUM_SCOPE(scope);
    number_t points[2][2] = {{first->x, first->y}, {last->x, last->y}};
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        if (!finite_point(points[0]) || !finite_point(points[1]))
            break;
        unsigned a = region(points[0], bounds), b = region(points[1], bounds);
        if (a & b)
            return 0;
        if (!(a | b)) {
            for (unsigned axis = 0; axis < 2; ++axis) {
                number_t span = num_sub(bounds[2 * axis + 1], bounds[2 * axis]);
                for (unsigned end = 0; end < 2; ++end) {
                    number_t ratio = num_div(num_sub(points[end][axis], bounds[2 * axis]), span);
                    double value = num_to_double(ratio);
                    if (!isfinite(value)) {
                        errno = ERANGE;
                        return -1;
                    }
                    output[2 * end + axis] = fmin(1, fmax(0, value));
                }
            }
            return 1;
        }
        unsigned target = a ? 0 : 1, code = a ? a : b;
        unsigned axis = (code & 3) ? 0 : 1;
        unsigned edge = 2 * axis + ((code & (1u << (2 * axis))) ? 0 : 1);
        number_t distances[] = {num_sub(bounds[edge], points[0][axis]), num_sub(bounds[edge], points[1][axis])};
        unsigned near = num_le(num_abs(distances[0]), num_abs(distances[1])) ? 0 : 1;
        unsigned far = 1 - near, other = 1 - axis;
        number_t ratio = num_div(distances[near], num_sub(points[far][axis], points[near][axis]));
        number_t displacement = num_mul(ratio, num_sub(points[far][other], points[near][other]));
        points[target][other] = num_add(points[near][other], displacement);
        points[target][axis] = bounds[edge];
    }
    errno = ERANGE;
    return -1;
}
