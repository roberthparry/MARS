#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

/* Evaluate ordinary Struve H using its alternating matrix series and symbolic functional calculus. */
matrix_t *mat_struve_h(const matrix_t *A, const number_t *order)
{
    return mat_cylindrical_function(A, order, true, true, num_struve_h, expr_struve_h);
}
