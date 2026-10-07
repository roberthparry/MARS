/**
 * @file number_special.c
 * @brief Orthogonal polynomials and scalar signal functions over generic numbers.
 *
 * Implements Chebyshev and Hermite recurrences together with sign, step, rectangular, triangular, circular and
 * sinc functions using the public numeric operations. These small algebraic families share exact comparisons,
 * precision-preserving arithmetic and normal numeric-scope ownership rather than separate numerical backends.
 *
 * Cylindrical series and larger special-function algorithms remain in dedicated implementation units. Public
 * declarations and caller-visible behaviour are unchanged in number.h.
 */

#include "number.h"
#include <limits.h>

static number_t polynomial(number_t degree, number_t x, unsigned family)
{
    double value = num_to_double(degree);
    if (!num_is_real(degree) || !num_is_integer(degree) || !num_is_finite(degree) ||
        value < 0 || value > INT_MAX)
        return num_clone(NUM_NAN);
    int n = (int)value;
    number_t previous = num_clone(NUM_ONE);
    number_t current = family == 0u ? num_clone(x) : num_mul_long(x, 2);
    if (n == 0) {
        num_destroy(&current);
        return previous;
    }
    for (int k = 1; k < n; ++k) {
        number_t product = num_mul(x, current);
        number_t twice = num_mul_long(product, 2);
        number_t back = family == 2u ? num_mul_long(previous, 2L * k) : num_clone(previous);
        number_t next = num_sub(twice, back);
        num_destroy(&product);
        num_destroy(&twice);
        num_destroy(&back);
        num_destroy(&previous);
        previous = current;
        current = next;
    }
    num_destroy(&previous);
    return current;
}

/* Evaluate the first-kind Chebyshev polynomial with precision-preserving number arithmetic. */
number_t num_chebyshev_t(number_t n, number_t x) { return polynomial(n, x, 0u); }

/* Evaluate the second-kind Chebyshev polynomial with precision-preserving number arithmetic. */
number_t num_chebyshev_u(number_t n, number_t x) { return polynomial(n, x, 1u); }

/* Evaluate the physicists' Hermite polynomial with precision-preserving number arithmetic. */
number_t num_hermite_h(number_t n, number_t x) { return polynomial(n, x, 2u); }

/* Return the exact real sign without reducing the input's numeric precision. */
number_t num_sgn(number_t x)
{
    return num_clone(!num_is_real(x) || num_is_nan(x) ? NUM_NAN
                     : num_is_zero(x) ? NUM_ZERO : num_gt(x, NUM_ZERO) ? NUM_ONE : NUM_NEG_ONE);
}

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
