/**
 * @file sampler_adaptive.c
 * @brief Bounded midpoint subdivision with numerical chord tests and explicit unresolved edges.
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

static bool coordinate_flat(const sampler_t *sampler, number_t a, number_t m, number_t b)
{
    number_t centre = midpoint(a, b), delta = num_sub(m, centre), deviation = num_abs(delta);
    number_t aa = num_abs(a), ab = num_abs(b), am = num_abs(m);
    number_t scale = num_gt(aa, ab) ? aa : ab;
    if (num_gt(am, scale))
        scale = am;
    number_t relative = num_mul(sampler->relative, scale), tolerance = num_add(sampler->absolute, relative);
    bool flat = num_is_finite(deviation) && num_is_finite(tolerance) && num_le(deviation, tolerance);
    num_destroy(&tolerance);
    num_destroy(&relative);
    num_destroy(&am);
    num_destroy(&ab);
    num_destroy(&aa);
    num_destroy(&deviation);
    num_destroy(&delta);
    num_destroy(&centre);
    return flat;
}

bool sampler_refine(sampler_t *sampler, const sampler_point_t *left, const sampler_point_t *right, unsigned depth)
{
    number_t parameter = midpoint(left->parameter, right->parameter);
    if (!num_gt(parameter, left->parameter) || !num_lt(parameter, right->parameter)) {
        num_destroy(&parameter);
        return sampler_append(sampler, right, true);
    }
    sampler_point_t middle;
    bool ok = sampler_evaluate(sampler, parameter, &middle);
    num_destroy(&parameter);
    if (ok) {
        bool flat = left->valid && middle.valid && right->valid &&
                    coordinate_flat(sampler, left->x, middle.x, right->x) &&
                    coordinate_flat(sampler, left->y, middle.y, right->y);
        bool empty = !left->valid && !middle.valid && !right->valid;
        if (flat || empty || depth == sampler->options.max_depth)
            ok = sampler_append(sampler, &middle, !flat) && sampler_append(sampler, right, !flat);
        else
            ok = sampler_refine(sampler, left, &middle, depth + 1) &&
                 sampler_refine(sampler, &middle, right, depth + 1);
    }
    sampler_point_clear(&middle);
    return ok;
}
