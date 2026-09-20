#include "number.h"

/* Preserve exact half-height endpoints without reducing numeric precision. */
number_t num_step(number_t x)
{
    return num_clone(!num_is_real(x) || num_is_nan(x) ? NUM_NAN
                     : num_is_zero(x) ? NUM_HALF : num_gt(x, NUM_ZERO) ? NUM_ONE : NUM_ZERO);
}

/* Evaluate the unit-width rectangular pulse using exact comparisons. */
number_t num_rect(number_t x)
{
    NUM_SCOPE(scope);
    if (!num_is_real(x))
        return num_scope_detach(num_clone(NUM_NAN));
    return num_scope_detach(num_step(num_sub(NUM_HALF, num_abs(x))));
}

/* Evaluate the triangular pulse without multiplying zero by infinity. */
number_t num_tri(number_t x)
{
    NUM_SCOPE(scope);
    if (!num_is_real(x) || num_is_nan(x))
        return num_scope_detach(num_clone(NUM_NAN));
    number_t a = num_abs(x);
    return num_scope_detach(num_ge(a, NUM_ONE) ? num_clone(NUM_ZERO) : num_sub(NUM_ONE, a));
}

/* Evaluate the even unit-radius aperture profile. */
number_t num_circ(number_t x)
{
    NUM_SCOPE(scope);
    if (!num_is_real(x))
        return num_scope_detach(num_clone(NUM_NAN));
    return num_scope_detach(num_step(num_sub(NUM_ONE, num_abs(x))));
}

/* Evaluate normalised sinc at the active real or complex precision. */
number_t num_sinc(number_t x)
{
    NUM_SCOPE(scope);
    if (num_is_zero(x))
        return num_scope_detach(num_clone(NUM_ONE));
    number_t angle = num_mul(num_const(NUM_PI), x);
    return num_scope_detach(num_div(num_sin(angle), angle));
}
