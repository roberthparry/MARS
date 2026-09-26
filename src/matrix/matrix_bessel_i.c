#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

/* Evaluate modified Bessel I using analytic numeric or symbolic matrix functional calculus. */
matrix_t *mat_bessel_i(const matrix_t *A, const number_t *order)
{
    return mat_cylindrical_function(A, order, false, false, num_bessel_i, expr_bessel_i);
}
