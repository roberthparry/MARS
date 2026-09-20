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
