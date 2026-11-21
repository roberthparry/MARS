/**
 * @file matrix_appell.c
 * @brief Joint Appell functional calculus for commuting matrix arguments.
 *
 * Diagonal numerical and symbolic pairs use the corresponding scalar or expression APIs. General numerical
 * pairs must commute and satisfy an independently bounded norm-domain series, or terminate as a polynomial.
 * The scalar module supplies the same Pochhammer majorant and guarded truncation plan used by its evaluators.
 * Matrix products preserve the two distinct variables; unsupported shapes, non-commutation and exhausted
 * storage/iteration budgets fail explicitly. All rolling matrices and numerical temporaries are locally owned.
 */
#include <limits.h>
#include <stdlib.h>

#include "expression-t.h"
#include "matrix-t.h"
#include "number-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

static number_t matrix_appell_scalar(unsigned family, const number_t *a, number_t x, number_t y)
{
    if (family == 1) return num_appell_F1(a[0], a[1], a[2], a[3], x, y);
    if (family == 2) return num_appell_F2(a[0], a[1], a[2], a[3], a[4], x, y);
    if (family == 3) return num_appell_F3(a[0], a[1], a[2], a[3], a[4], x, y);
    return num_appell_F4(a[0], a[1], a[2], a[3], x, y);
}

static expr_t *matrix_appell_expression(unsigned family, const expr_t *const *a, const expr_t *x, const expr_t *y)
{
    if (family == 1) return expr_appell_F1(a[0], a[1], a[2], a[3], x, y);
    if (family == 2) return expr_appell_F2(a[0], a[1], a[2], a[3], a[4], x, y);
    if (family == 3) return expr_appell_F3(a[0], a[1], a[2], a[3], a[4], x, y);
    return expr_appell_F4(a[0], a[1], a[2], a[3], x, y);
}

static matrix_t *matrix_appell_detach(matrix_t *matrix, size_t precision)
{
    if (!matrix || matrix->elem != &number_elem) return matrix;
    for (size_t i = 0; i < matrix->rows; ++i) for (size_t j = 0; j < matrix->cols; ++j) {
        number_t value = mat_get_num(matrix, i, j);
        if (!num_is_finite(value) || num_set_prec_bits(&value, precision) != 0) {
            num_destroy(&value); mat_free(matrix); return NULL;
        }
        mat_set_num_owned(matrix, i, j, &value);
    }
    return matrix;
}

static matrix_t *matrix_appell_diagonal(unsigned family, const number_t *a, const matrix_t *x, const matrix_t *y)
{
    NUM_SCOPE(scope);
    bool symbolic = x->elem == &expr_elem || y->elem == &expr_elem;
    matrix_t *result = symbolic ? mat_new_expr(x->rows, x->cols) : mat_new(x->rows, x->cols);
    if (!result) return NULL;
    size_t precision = num_get_default_prec_bits();
    expr_t *parameters[5] = {NULL};
    unsigned count = family == 1 || family == 4 ? 4u : 5u;
    if (symbolic) for (unsigned j = 0; j < count; ++j) parameters[j] = expr_new_const(a[j]);
    bool ok = true;
    for (size_t i = 0; ok && i < x->rows; ++i) {
        if (symbolic) {
            expr_t *left = NULL, *right = NULL;
            if (x->elem == &expr_elem) mat_get(x, i, i, &left);
            else left = expr_new_const(mat_get_num(x, i, i));
            if (y->elem == &expr_elem) mat_get(y, i, i, &right);
            else right = expr_new_const(mat_get_num(y, i, i));
            expr_t *value = matrix_appell_expression(family, (const expr_t *const *)parameters, left, right);
            ok = value != NULL;
            if (ok) mat_set(result, i, i, &value);
            expr_free(value);
            if (x->elem != &expr_elem) expr_free(left);
            if (y->elem != &expr_elem) expr_free(right);
        } else {
            number_t left = mat_get_num(x, i, i), right = mat_get_num(y, i, i);
            number_t value = matrix_appell_scalar(family, a, left, right);
            size_t bits = num_get_prec_bits(value);
            if (bits > precision) precision = bits;
            ok = num_is_finite(value);
            if (ok) mat_set_num_owned(result, i, i, &value);
            else num_destroy(&value);
            num_destroy(&right); num_destroy(&left);
        }
    }
    for (unsigned j = 0; j < count; ++j) expr_free(parameters[j]);
    if (!ok) { mat_free(result); return NULL; }
    return symbolic ? result : matrix_appell_detach(result, precision);
}

static number_t matrix_appell_ratio(unsigned family, const number_t *a, unsigned m, unsigned n, bool first,
                                    number_t one)
{
    NUM_SCOPE(scope);
    unsigned index = first ? m : n, total = m + n;
    number_t numerator = one, denominator = NUM_NAN;
    if (family == 1 || family == 2) {
        numerator = num_mul(numerator, num_mul(num_add_long(a[0], total), num_add_long(a[first ? 1 : 2], index)));
        denominator = num_add_long(a[family == 1 ? 3 : first ? 3 : 4], family == 1 ? total : index);
    } else if (family == 3) {
        numerator = num_mul(numerator, num_mul(num_add_long(a[first ? 0 : 1], index),
                                               num_add_long(a[first ? 2 : 3], index)));
        denominator = num_add_long(a[4], total);
    } else {
        numerator = num_mul(numerator, num_mul(num_add_long(a[0], total), num_add_long(a[1], total)));
        denominator = num_add_long(a[first ? 2 : 3], index);
    }
    return num_scope_detach(num_div(numerator, num_mul(denominator, num_create_from_long(index + 1u))));
}

static matrix_t *matrix_appell_series(unsigned family, const number_t *a, const matrix_t *x, const matrix_t *y)
{
    NUM_SCOPE(scope);
    if (x->elem != &number_elem || y->elem != &number_elem || x->rows > 64u) return NULL;
    matrix_t *xy = mat_mul(x, y), *yx = mat_mul(y, x);
    matrix_t *difference = xy && yx ? mat_sub(xy, yx) : NULL;
    number_t commute_norm = NUM_NAN, x_norm = NUM_NAN, y_norm = NUM_NAN;
    bool commute = difference && mat_norm(difference, MAT_NORM_INF, &commute_norm) == 0 && num_is_zero(commute_norm);
    mat_free(difference); mat_free(yx); mat_free(xy);
    num_destroy(&commute_norm);
    if (!commute || mat_norm(x, MAT_NORM_INF, &x_norm) || mat_norm(y, MAT_NORM_INF, &y_norm)) {
        num_destroy(&y_norm);
        num_destroy(&x_norm);
        return NULL;
    }
    number_t inflation = num_create_from_frac(65537, 65536);
    number_t inflated_x_norm = num_mul(x_norm, inflation), inflated_y_norm = num_mul(y_norm, inflation);
    num_destroy(&y_norm);
    num_destroy(&x_norm);
    size_t precision = num_get_default_prec_bits();
    unsigned parameter_count = family == 1 || family == 4 ? 4u : 5u;
    for (unsigned j = 0; j < parameter_count; ++j) {
        size_t bits = num_get_prec_bits(a[j]);
        if (bits > precision) precision = bits;
    }
    for (size_t i = 0; i < x->rows; ++i) for (size_t j = 0; j < x->cols; ++j) {
        size_t left = num_get_prec_bits(mat_get_num(x, i, j)), right = num_get_prec_bits(mat_get_num(y, i, j));
        if (left > precision) precision = left;
        if (right > precision) precision = right;
    }
    number_appell_plan_t plan;
    if (!number_appell_series_plan(family, a, inflated_x_norm, inflated_y_norm, precision, &plan) ||
        plan.degree > 2048)
        return NULL;
    number_t one = num_create_from_string("1.0");
    if (num_set_prec_bits(&one, plan.working_bits)) return NULL;
    size_t width = plan.m_last < (long)plan.degree ? (size_t)plan.m_last + 1u : (size_t)plan.degree + 1u;
    matrix_t **row = calloc(width, sizeof *row), **next = calloc(width, sizeof *next);
    if (!row || !next) { free(row); free(next); return NULL; }
    row[0] = mat_pow_int(x, 0);
    matrix_t *sum = row[0] ? mat_copy_preserving_store(row[0]) : NULL;
    size_t cells = 0;
    bool ok = sum != NULL;
    for (unsigned k = 1; ok && k <= plan.degree; ++k) {
        unsigned low = k > (unsigned long)plan.n_last ? k - (unsigned)plan.n_last : 0u;
        unsigned high = k < width ? k : (unsigned)width - 1u;
        if (low > high || (cells += high - low + 1u) > 100000u) { ok = false; break; }
        bool row_nonzero = false;
        for (unsigned m = low; ok && m <= high; ++m) {
            number_t ratio = m ? matrix_appell_ratio(family, a, m - 1u, k - m, true, one) :
                                  matrix_appell_ratio(family, a, 0, k - 1u, false, one);
            matrix_t *product = mat_mul(row[m ? m - 1u : 0], m ? x : y);
            next[m] = product && num_is_finite(ratio) ? mat_scalar_mul(product, &ratio) : NULL;
            row_nonzero = row_nonzero || (next[m] && mat_nonzero_count(next[m]) != 0);
            mat_free(product); num_destroy(&ratio);
            matrix_t *updated = next[m] ? mat_add(sum, next[m]) : NULL;
            mat_free(sum); sum = updated; ok = sum != NULL;
        }
        for (size_t j = 0; j < width; ++j) { mat_free(row[j]); row[j] = NULL; }
        matrix_t **swap = row; row = next; next = swap;
        /* A completely zero coefficient row stays zero under both matrix recurrences. */
        if (!row_nonzero) break;
    }
    for (size_t j = 0; j < width; ++j) { mat_free(next[j]); mat_free(row[j]); }
    free(next); free(row);
    if (!ok) { mat_free(sum); return NULL; }
    return matrix_appell_detach(sum, precision);
}

static matrix_t *matrix_appell(unsigned family, const number_t *a, const matrix_t *x, const matrix_t *y)
{
    if (!x || !y || x->rows != x->cols || y->rows != x->rows || y->cols != x->cols) return NULL;
    return mat_is_diagonal(x) && mat_is_diagonal(y) ? matrix_appell_diagonal(family, a, x, y) :
                                                    matrix_appell_series(family, a, x, y);
}

/* Evaluate the first Appell function of a commuting matrix pair. */
matrix_t *mat_appell_F1(number_t a, number_t b1, number_t b2, number_t c, const matrix_t *x, const matrix_t *y)
{
    const number_t parameters[] = {a, b1, b2, c};
    return matrix_appell(1, parameters, x, y);
}

/* Evaluate the second Appell function through joint matrix functional calculus. */
matrix_t *mat_appell_F2(number_t a, number_t b1, number_t b2, number_t c1, number_t c2,
                        const matrix_t *x, const matrix_t *y)
{
    const number_t parameters[] = {a, b1, b2, c1, c2};
    return matrix_appell(2, parameters, x, y);
}

/* Evaluate the third Appell function, retaining the separate upper-parameter pairs. */
matrix_t *mat_appell_F3(number_t a1, number_t a2, number_t b1, number_t b2, number_t c,
                        const matrix_t *x, const matrix_t *y)
{
    const number_t parameters[] = {a1, a2, b1, b2, c};
    return matrix_appell(3, parameters, x, y);
}

/* Evaluate the fourth Appell function for supported commuting arguments. */
matrix_t *mat_appell_F4(number_t a, number_t b, number_t c1, number_t c2, const matrix_t *x, const matrix_t *y)
{
    const number_t parameters[] = {a, b, c1, c2};
    return matrix_appell(4, parameters, x, y);
}
