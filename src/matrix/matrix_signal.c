#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

#define SIGNAL_CALLBACKS(name)                                                                                          \
    static void number_##name(void *out, const void *in)                                                                \
    {                                                                                                                  \
        number_t value = num_##name(*(const number_t *)in);                                                             \
        num_destroy((number_t *)out);                                                                                  \
        *(number_t *)out = value;                                                                                      \
    }                                                                                                                  \
    static void expression_##name(void *out, const void *in)                                                            \
    {                                                                                                                  \
        expr_t *value = expr_##name(*(expr_t *const *)in);                                                              \
        expr_free(*(expr_t **)out);                                                                                    \
        *(expr_t **)out = value;                                                                                       \
    }

SIGNAL_CALLBACKS(step)
SIGNAL_CALLBACKS(rect)
SIGNAL_CALLBACKS(tri)
SIGNAL_CALLBACKS(circ)
SIGNAL_CALLBACKS(sinc)
#undef SIGNAL_CALLBACKS

/* Apply the unit step to the spectrum rather than individual matrix entries. */
matrix_t *mat_step(const matrix_t *a) { return mat_apply_scalar_callbacks(a, number_step, expression_step); }
/* Apply the rectangular pulse to the spectrum. */
matrix_t *mat_rect(const matrix_t *a) { return mat_apply_scalar_callbacks(a, number_rect, expression_rect); }
/* Apply the triangular pulse to the spectrum. */
matrix_t *mat_tri(const matrix_t *a) { return mat_apply_scalar_callbacks(a, number_tri, expression_tri); }
/* Apply the even aperture profile to the spectrum. */
matrix_t *mat_circ(const matrix_t *a) { return mat_apply_scalar_callbacks(a, number_circ, expression_circ); }
/* Apply the entire normalised sinc function to the spectrum. */
matrix_t *mat_sinc(const matrix_t *a)
{
    matrix_t *structured = a && !mat_is_diagonal(a) ? mat_number_unary_taylor_from_expr(a, expr_sinc) : NULL;
    return structured ? structured : mat_apply_scalar_callbacks(a, number_sinc, expression_sinc);
}
