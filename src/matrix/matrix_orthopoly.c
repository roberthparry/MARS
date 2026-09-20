#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

static matrix_t *polynomial(const matrix_t *a, unsigned int degree, unsigned family)
{
    if (!a || a->rows != a->cols)
        return NULL;
    matrix_t *previous = mat_pow_int(a, 0);
    if (degree == 0u)
        return previous;
    number_t two = num_create_from_long(2);
    matrix_t *current = mat_scalar_mul((matrix_t *)a, family == 0u ? &NUM_ONE : &two);
    for (unsigned int k = 1u; previous && current && k < degree; ++k) {
        matrix_t *product = mat_mul(a, current);
        matrix_t *twice = product ? mat_scalar_mul(product, &two) : NULL;
        number_t factor = family == 2u ? num_create_from_long(2L * k) : num_clone(NUM_ONE);
        matrix_t *back = mat_scalar_mul(previous, &factor);
        matrix_t *next = twice && back ? mat_sub(twice, back) : NULL;
        num_destroy(&factor);
        mat_free(product);
        mat_free(twice);
        mat_free(back);
        mat_free(previous);
        previous = current;
        current = next;
    }
    num_destroy(&two);
    mat_free(previous);
    return current;
}

/* Evaluate a first-kind Chebyshev matrix polynomial, not an entrywise function. */
matrix_t *mat_chebyshev_t(const matrix_t *a, unsigned int n) { return polynomial(a, n, 0u); }
/* Evaluate a second-kind Chebyshev matrix polynomial. */
matrix_t *mat_chebyshev_u(const matrix_t *a, unsigned int n) { return polynomial(a, n, 1u); }
/* Evaluate a physicists' Hermite matrix polynomial. */
matrix_t *mat_hermite_h(const matrix_t *a, unsigned int n) { return polynomial(a, n, 2u); }
