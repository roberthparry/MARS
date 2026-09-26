#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

/* Evaluate modified Struve L using analytic numeric or symbolic matrix functional calculus. */
matrix_t *mat_struve_l(const matrix_t *A, const number_t *order)
{
    return mat_cylindrical_function(A, order, true, false, num_struve_l, expr_struve_l);
}
