#include <stdio.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

typedef struct {
    size_t count;
    expr_t *const *wrts;
} mat_integrate_sequence_context_t;

static expr_t *mat_integrate_spectral_expression(const expr_t *spectral_expression, void *opaque_context)
{
    const mat_integrate_sequence_context_t *context = opaque_context;
    expr_t *current = NULL;

    if (!spectral_expression || !context || context->count == 0u || !context->wrts)
        return NULL;
    for (size_t index = 0u; index < context->count; ++index) {
        expr_t *next;

        if (!context->wrts[index]) {
            expr_free(current);
            return NULL;
        }
        next = expr_integrate(current ? current : spectral_expression, context->wrts[index]);
        next = mat_simplify_post_calculus_expression(next);
        expr_free(current);
        current = next;
        if (!current)
            return NULL;
    }
    return current;
}

/* Integrate a matrix entrywise using a binding returned by the matrix parser. */
matrix_t *mat_integrate_by_name(const matrix_t *A, mat_bindings_t *bindings, const char *name)
{
    expr_t *binding;

    if (!A || !bindings || !name)
        return NULL;

    binding = mat_bindings_get(bindings, name);
    if (!binding)
        return NULL;

    return mat_integrate(A, binding);
}

/* Integrate every matrix entry with the general symbolic integration rules. */
matrix_t *mat_integrate(const matrix_t *A, expr_t *wrt)
{
    matrix_t *integrated;

    if (!A || !wrt || !A->elem)
        return NULL;

    integrated = mat_create_zero_with_elem(A->rows, A->cols, &expr_elem);
    if (!integrated)
        return NULL;

    for (size_t row = 0u; row < A->rows; ++row) {
        for (size_t col = 0u; col < A->cols; ++col) {
            expr_t *owned_entry = NULL;
            expr_t *entry = NULL;
            expr_t *antiderivative;

            if (matrix_is_symbolic(A)) {
                mat_get(A, row, col, &entry);
                if (!entry)
                    entry = (expr_t *)EXPR_ZERO;
            } else {
                number_t value = mat_get_num(A, row, col);

                owned_entry = expr_new_const(value);
                num_destroy(&value);
                entry = owned_entry;
            }

            antiderivative = entry ? expr_integrate(entry, wrt) : NULL;
            expr_free(owned_entry);
            if (!antiderivative) {
                mat_free(integrated);
                return NULL;
            }
            antiderivative = mat_simplify_post_calculus_expression(antiderivative);
            mat_set(integrated, row, col, &antiderivative);
            expr_free(antiderivative);
        }
    }
    return integrated;
}

/* Integrate a matrix successively through an ordered variable sequence. */
matrix_t *mat_integrate_sequence(const matrix_t *A, size_t count, expr_t *const *wrts)
{
    matrix_t *current = NULL;

    if (!A || count == 0u || !wrts)
        return NULL;
    for (size_t index = 0u; index < count; ++index) {
        matrix_t *next;

        if (!wrts[index]) {
            mat_free(current);
            return NULL;
        }
        next = mat_integrate(current ? current : A, wrts[index]);
        mat_free(current);
        current = next;
        if (!current)
            return NULL;
    }
    return current;
}

/* Integrate a constant exact matrix power through its spectral powers. */
matrix_t *mat_integrate_pow_expr(const matrix_t *A, const expr_t *exponent, expr_t *wrt)
{
    expr_t *wrts[1] = {wrt};

    return mat_integrate_pow_expr_sequence(A, exponent, 1u, wrts);
}

/* Integrate a constant square-matrix power through an ordered variable sequence. */
matrix_t *mat_integrate_pow_expr_sequence(const matrix_t *A, const expr_t *exponent, size_t count,
                                          expr_t *const *wrts)
{
    mat_integrate_sequence_context_t context = {.count = count, .wrts = wrts};

    return mat_pow_expr_spectral_map(A, exponent, mat_integrate_spectral_expression, &context);
}

/* Append one independent arbitrary constant to every antiderivative entry. */
matrix_t *mat_integrate_append_constants(const matrix_t *antiderivative)
{
    matrix_t *family;

    if (!antiderivative || !antiderivative->elem)
        return NULL;

    family = mat_create_zero_with_elem(antiderivative->rows, antiderivative->cols, &expr_elem);
    if (!family)
        return NULL;

    for (size_t row = 0u; row < antiderivative->rows; ++row) {
        for (size_t col = 0u; col < antiderivative->cols; ++col) {
            expr_t *entry = NULL;
            expr_t *constant;
            expr_t *entry_family;
            char constant_name[64];

            mat_get(antiderivative, row, col, &entry);
            snprintf(constant_name, sizeof(constant_name), "C_%zu%zu", row + 1u, col + 1u);
            constant = expr_new_named_const(NUM_NAN, constant_name);
            entry_family = constant ? expr_add(entry ? entry : EXPR_ZERO, constant) : NULL;
            expr_free(constant);
            if (!entry_family)
                goto fail;

            mat_set(family, row, col, &entry_family);
            expr_free(entry_family);
        }
    }

    return family;

fail:
    mat_free(family);
    return NULL;
}

/* Integrate every entry and append one independent arbitrary constant per entry. */
matrix_t *mat_integrate_family(const matrix_t *A, expr_t *wrt)
{
    matrix_t *antiderivative = mat_integrate(A, wrt);
    matrix_t *family = antiderivative ? mat_integrate_append_constants(antiderivative) : NULL;

    mat_free(antiderivative);
    return family;
}
