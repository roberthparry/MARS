/**
 * @file sampler_adaptive.c
 * @brief Bounded chord and tangent subdivision with safeguarded stationary-point probes.
 *
 * Endpoint samples are reused through depth-first subdivision. Testing both
 * coordinates supports parametric curves without assuming monotonic x. Limits
 * bound stack depth and callback work; no finite probe scheme proves continuity.
 */
#include "number-t.h"
#define MARS_SAMPLER_INTERNAL_ACCESS
#include "sampler_internal.h"

static number_t midpoint(number_t a, number_t b)
{
    number_t half_a = num_mul(a, NUM_HALF), half_b = num_mul(b, NUM_HALF);
    number_t value = num_add(half_a, half_b);
    num_destroy(&half_b);
    num_destroy(&half_a);
    return value;
}

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static number_t coordinate_tolerance(const sampler_t *sampler, number_t a, number_t m, number_t b)
{
    NUM_SCOPE(numbers);
    number_t aa = num_abs(a), ab = num_abs(b), am = num_abs(m);
    number_t scale = num_gt(aa, ab) ? aa : ab;
    if (num_gt(am, scale))
        scale = am;
    return num_scope_detach(num_add(sampler->absolute, num_mul(sampler->relative, scale)));
}

static bool coordinate_flat(const sampler_t *sampler, number_t a, number_t m, number_t b,
                            number_t da, number_t dm, number_t db, number_t dda, number_t ddm, number_t ddb,
                            number_t width, number_t fraction)
{
    NUM_SCOPE(numbers);
    number_t change = num_sub(b, a), centre = num_add(a, num_mul(fraction, change));
    number_t tolerance = coordinate_tolerance(sampler, a, m, b);
    if (!num_is_finite(tolerance) || !num_le(num_abs(num_sub(m, centre)), tolerance)) {
        num_destroy(&tolerance);
        return false;
    }
    const number_t slopes[] = {da, dm, db};
    number_t four_tolerances = num_mul(num_create_from_long(4), tolerance);
    /* A tangent/secant mismatch exposes oscillations whose point values alias.
     * Missing individual tangents do not invalidate finite coordinate samples. */
    for (size_t i = 0; i < 3; ++i)
        if (finite_real(slopes[i]) &&
            !num_le(num_abs(num_sub(num_mul(slopes[i], width), change)), four_tolerances)) {
            num_destroy(&tolerance);
            return false;
        }
    const number_t curvatures[] = {dda, ddm, ddb};
    number_t squared_width = num_mul(width, width);
    number_t eight_tolerances = num_mul(num_create_from_long(8), tolerance);
    /* Curvature catches flat first-derivative probes. This sampled Taylor
     * estimate is a refinement hint, not a bound on unobserved derivatives. */
    for (size_t i = 0; i < 3; ++i)
        if (finite_real(curvatures[i]) &&
            !num_le(num_abs(num_mul(curvatures[i], squared_width)), eight_tolerances)) {
            num_destroy(&tolerance);
            return false;
        }
    num_destroy(&tolerance);
    return true;
}

static bool brackets_stationary(const sampler_point_t *left, const sampler_point_t *right)
{
    return left->valid && right->valid && finite_real(left->dy) && finite_real(right->dy) &&
        ((num_lt(left->dy, NUM_ZERO) && num_gt(right->dy, NUM_ZERO)) ||
         (num_gt(left->dy, NUM_ZERO) && num_lt(right->dy, NUM_ZERO)));
}

static bool near_stationary(const sampler_t *sampler, const sampler_point_t *left,
                            const sampler_point_t *middle, const sampler_point_t *right, number_t width)
{
    NUM_SCOPE(numbers);
    if (!brackets_stationary(left, right) || !middle->valid || !finite_real(middle->dy))
        return false;
    number_t tolerance = coordinate_tolerance(sampler, left->y, middle->y, right->y);
    bool near = num_le(num_abs(num_mul(middle->dy, width)), tolerance);
    num_destroy(&tolerance);
    return near;
}

bool sampler_refine(sampler_t *sampler, const sampler_point_t *left, const sampler_point_t *right, unsigned depth)
{
    number_t width = num_sub(right->parameter, left->parameter);
    number_t parameter = midpoint(left->parameter, right->parameter);
    bool bracket = brackets_stationary(left, right);
    if (!num_gt(parameter, left->parameter) || !num_lt(parameter, right->parameter)) {
        num_destroy(&parameter);
        num_destroy(&width);
        return sampler_append(sampler, right, true);
    }
    sampler_point_t middle;
    bool ok = sampler_evaluate(sampler, parameter, &middle);
    number_t offset = num_sub(parameter, left->parameter), fraction = num_div(offset, width);
    num_destroy(&offset);
    num_destroy(&parameter);
    if (ok) {
        middle.stationary = near_stationary(sampler, left, &middle, right, width);
        bool flat = left->valid && middle.valid && right->valid &&
                    (!bracket || middle.stationary) &&
                    coordinate_flat(sampler, left->x, middle.x, right->x,
                                    left->dx, middle.dx, right->dx, left->ddx, middle.ddx, right->ddx, width, fraction) &&
                    coordinate_flat(sampler, left->y, middle.y, right->y,
                                    left->dy, middle.dy, right->dy, left->ddy, middle.ddy, right->ddy, width, fraction);
        bool empty = !left->valid && !middle.valid && !right->valid;
        if (flat || empty || depth == sampler->options.max_depth)
            ok = sampler_append(sampler, &middle, !flat) && sampler_append(sampler, right, !flat);
        else
            ok = sampler_refine(sampler, left, &middle, depth + 1) &&
                 sampler_refine(sampler, &middle, right, depth + 1);
    }
    sampler_point_clear(&middle);
    num_destroy(&fraction);
    num_destroy(&width);
    return ok;
}
