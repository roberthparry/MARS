/**
 * @file matrix_lambert.c
 * @brief Matrix functional calculus for MARS's defined Lambert sum.
 *
 * Scalar parameters remain separate from the matrix initial coordinate. The existing spectral engine and
 * analytic Taylor engine evaluate genuine matrix functions rather than applying the scalar sum entrywise.
 * A restored thread-local context carries borrowed parameters through synchronous matrix callbacks.
 */
#include "expression-t.h"
#include "matrix-t.h"
#include "number-t.h"
#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

typedef struct _matrix_lambert_context_t matrix_lambert_context_t;
struct _matrix_lambert_context_t {
    number_t a, q, weight, order, count;
};

static _Thread_local const matrix_lambert_context_t *lambert_context;

static expr_t *lambert_expression(const expr_t *z)
{
    const matrix_lambert_context_t *context = lambert_context;
    expr_t *a = expr_new_const(context->a), *q = expr_new_const(context->q);
    expr_t *r = expr_new_const(context->weight), *s = expr_new_const(context->order), *n = expr_new_const(context->count);
    expr_t *out = expr_lambert_S(a, q, z, r, s, n);
    expr_free(n); expr_free(s); expr_free(r); expr_free(q); expr_free(a);
    return out;
}

static void lambert_number_callback(void *out, const void *in)
{
    const matrix_lambert_context_t *context = lambert_context;
    number_t value = num_lambert_S(context->a, context->q, *(const number_t *)in,
                                   context->weight, context->order, context->count);
    num_destroy((number_t *)out);
    *(number_t *)out = value;
}

static void lambert_expression_callback(void *out, const void *in)
{
    expr_t *value = lambert_expression(*(expr_t *const *)in);
    expr_free(*(expr_t **)out);
    *(expr_t **)out = value;
}

/* Apply the defined scalar function to the matrix coordinate through existing functional-calculus engines. */
matrix_t *mat_lambert_S(const matrix_t *Z, number_t a, number_t q, number_t weight, number_t order, number_t count)
{
    if (!Z || Z->rows != Z->cols) return NULL;
    const matrix_lambert_context_t context = {a, q, weight, order, count};
    const matrix_lambert_context_t *previous = lambert_context;
    lambert_context = &context;
    matrix_t *out = Z->elem == &number_elem ? mat_number_unary_taylor_from_expr(Z, lambert_expression) : NULL;
    if (!out) out = mat_apply_scalar_callbacks(Z, lambert_number_callback, lambert_expression_callback);
    lambert_context = previous;
    return out;
}
