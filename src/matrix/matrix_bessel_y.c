#include <limits.h>
#include <stdlib.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

static bool bessel_y_matrix_finite(const matrix_t *A)
{
    for (size_t row = 0u; row < A->rows; ++row) {
        for (size_t col = 0u; col < A->cols; ++col) {
            NUM_SCOPE(scope);
            number_t entry = mat_get_num(A, row, col);

            if (!num_is_finite(entry))
                return false;
        }
    }
    return true;
}

/* Group equal complex nodes contiguously so confluent divided differences use analytic derivatives. */
static int bessel_y_compare_nodes(const void *left, const void *right)
{
    NUM_SCOPE(scope);
    number_t a = *(const number_t *)left, b = *(const number_t *)right;
    int real_order = num_cmp(num_real_part(a), num_real_part(b));

    return real_order ? real_order : num_cmp(num_imag_part(a), num_imag_part(b));
}

/* Y_nu^(k)(z)/k! = sum_j (-1)^j binom(k,j) Y_(nu-k+2j)(z) / (2^k k!). */
static number_t bessel_y_taylor(const number_t *order, number_t argument, size_t degree)
{
    NUM_SCOPE(scope);
    number_t factorial = NUM_ONE;
    number_t sum = NUM_ZERO;

    for (size_t k = 2u; k <= degree; ++k)
        factorial = num_mul(factorial, num_create_from_long((long)k));
    number_t weight = num_div(num_pow_int(NUM_HALF, (int)degree), factorial);

    for (size_t j = 0u; j <= degree; ++j) {
        number_t shifted = num_add_long(*order, 2L * (long)j - (long)degree);
        number_t value = num_bessel_y(shifted, argument);

        if (!num_is_finite(value))
            return num_clone(NUM_NAN);
        sum = num_add(sum, num_mul(weight, value));
        if (j < degree) {
            number_t ratio = num_create_from_frac((long)(degree - j), (long)(j + 1u));

            weight = num_neg(num_mul(weight, ratio));
        }
    }
    return num_scope_detach(sum);
}

static void bessel_y_add_diagonal(matrix_t *A, number_t value)
{
    for (size_t i = 0u; i < A->rows; ++i) {
        NUM_SCOPE(scope);
        number_t entry = num_add(mat_get_num(A, i, i), value);

        mat_set_num_owned(A, i, i, &entry);
    }
}

static matrix_t *bessel_y_interpolate(const matrix_t *A, const number_t *order)
{
    const size_t n = A->rows;
    number_t *nodes = calloc(n, sizeof(*nodes));
    number_t *coefficients = calloc(n, sizeof(*coefficients));
    mat_schur_factor_t schur = {0};
    matrix_t *result = NULL;

    if (!nodes || !coefficients) {
        free(nodes);
        free(coefficients);
        return NULL;
    }
    for (size_t i = 0u; i < n; ++i) {
        nodes[i] = number_invalid();
        coefficients[i] = number_invalid();
    }

    const matrix_t *triangular = A;

    if (!mat_is_upper_triangular(A) && !mat_is_lower_triangular(A)) {
        if (mat_schur_factor(A, &schur) != 0 || !schur.T)
            goto cleanup;
        triangular = schur.T;
    }
    for (size_t i = 0u; i < n; ++i) {
        nodes[i] = mat_get_num(triangular, i, i);
        if (!num_is_finite(nodes[i]) || num_eq(nodes[i], NUM_ZERO))
            goto cleanup;
    }
    qsort(nodes, n, sizeof(*nodes), bessel_y_compare_nodes);
    for (size_t i = 0u; i < n; ++i) {
        coefficients[i] = num_bessel_y(*order, nodes[i]);
        if (!num_is_finite(coefficients[i]))
            goto cleanup;
    }

    /* The n-by-n Hermite table includes all multiplicities, including separated repeated eigenvalues. */
    for (size_t degree = 1u; degree < n; ++degree) {
        for (size_t i = n; i-- > degree;) {
            NUM_SCOPE(scope);
            number_t value;

            if (num_eq(nodes[i], nodes[i - degree]))
                value = bessel_y_taylor(order, nodes[i], degree);
            else
                value = num_div(num_sub(coefficients[i], coefficients[i - 1u]),
                                num_sub(nodes[i], nodes[i - degree]));
            if (!num_is_finite(value))
                goto cleanup;
            num_destroy(&coefficients[i]);
            coefficients[i] = num_scope_detach(value);
        }
    }

    result = mat_new(n, n);
    if (!result)
        goto cleanup;
    bessel_y_add_diagonal(result, coefficients[n - 1u]);
    for (size_t k = n - 1u; k > 0u; --k) {
        matrix_t *factor = mat_copy_preserving_store(A);
        number_t negative_node = num_neg(nodes[k - 1u]);

        if (factor)
            bessel_y_add_diagonal(factor, negative_node);
        num_destroy(&negative_node);
        matrix_t *next = factor ? mat_mul(factor, result) : NULL;

        mat_free(factor);
        mat_free(result);
        result = next;
        if (!result)
            goto cleanup;
        bessel_y_add_diagonal(result, coefficients[k - 1u]);
    }
    if (!bessel_y_matrix_finite(result)) {
        mat_free(result);
        result = NULL;
    }

cleanup:
    for (size_t i = 0u; i < n; ++i) {
        num_destroy(&coefficients[i]);
        num_destroy(&nodes[i]);
    }
    free(coefficients);
    free(nodes);
    mat_schur_factor_free(&schur);
    return result;
}

/* Evaluate ordinary Bessel Y by analytic Hermite interpolation or symbolic matrix functional calculus. */
matrix_t *mat_bessel_y(const matrix_t *A, const number_t *order)
{
    if (!A || !order || !num_is_finite(*order) || A->rows != A->cols || A->rows == 0u || A->rows > INT_MAX)
        return NULL;
    if (matrix_is_symbolic(A))
        return mat_cylindrical_symbolic(A, order, num_bessel_y, expr_bessel_y);
    if (!bessel_y_matrix_finite(A))
        return NULL;

    number_t determinant = number_invalid();
    bool singular = mat_det(A, &determinant) != 0 || !num_is_finite(determinant) || num_eq(determinant, NUM_ZERO);

    num_destroy(&determinant);
    if (singular)
        return NULL;
    if (mat_is_diagonal(A)) {
        matrix_t *result = mat_create_diagonal_with_elem(A->rows, &number_elem);

        if (!result)
            return NULL;
        for (size_t i = 0u; i < A->rows; ++i) {
            NUM_SCOPE(scope);
            number_t value = num_bessel_y(*order, mat_get_num(A, i, i));

            if (!num_is_finite(value)) {
                mat_free(result);
                return NULL;
            }
            mat_set_num_owned(result, i, i, &value);
        }
        return result;
    }
    return bessel_y_interpolate(A, order);
}
