#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

static expr_t *bessel_k_spectral(const expr_t *value, void *context)
{
    return expr_bessel_k(context, value);
}

/* Reconstruct the matrix function from its scalar spectral values, never elementwise. */
matrix_t *mat_bessel_k(const matrix_t *A, const number_t *order)
{
    if (!order || !num_is_finite(*order))
        return NULL;
    expr_t *n = expr_new_const(*order), *one = expr_new_const(NUM_ONE);
    matrix_t *result = mat_pow_expr_spectral_map(A, one, bessel_k_spectral, n);
    expr_free(one);
    expr_free(n);
    return result;
}
