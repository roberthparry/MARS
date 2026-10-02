#include <limits.h>
#include <stdlib.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

typedef number_t (*cylindrical_number_fn)(const number_t order, const number_t argument);
typedef expr_t *(*cylindrical_expression_fn)(const expr_t *order, const expr_t *argument);

typedef struct {
    const number_t *order;
    cylindrical_number_fn number_function;
    cylindrical_expression_fn expression_function;
} cylindrical_context_t;

static _Thread_local const cylindrical_context_t *cylindrical_context;

static expr_t *cylindrical_spectral(const expr_t *value, void *context)
{
    const cylindrical_context_t *family = context;
    number_t argument = expr_get_val(value);
    number_t result = family->number_function(*family->order, argument);
    expr_t *expression = num_is_finite(result) ? expr_new_const(result) : NULL;

    num_destroy(&result);
    num_destroy(&argument);
    return expression;
}

/* Matrix arithmetic copies numbers into slots; detach rolling entries before their temporary scope ends. */
static matrix_t *cylindrical_detach_entries(matrix_t *A)
{
    if (!A)
        return NULL;
    for (size_t row = 0u; row < A->rows; ++row) {
        for (size_t col = 0u; col < A->cols; ++col) {
            number_t entry = mat_get_num(A, row, col);

            mat_set_num_owned(A, row, col, &entry);
        }
    }
    return A;
}

/* The gamma shift is 3/2 for Struve H/L and 1 for Bessel I; H alternates signs. */
static matrix_t *cylindrical_series(const matrix_t *A, const number_t *order, size_t precision, bool struve,
                                   bool alternating)
{
    NUM_SCOPE(scope);
    const unsigned int maximum_terms = 4096u;
    number_t shift = struve ? num_add(NUM_ONE, NUM_HALF) : NUM_ONE;
    number_t gamma_argument = num_add(*order, shift);
    unsigned int first = 0u;
    matrix_t *power = NULL, *square = NULL, *term = NULL, *sum = NULL;
    number_t square_norm = number_invalid();
    number_t exponent = number_invalid();
    number_t tolerance = num_ldexp(NUM_ONE, 8 - (int)precision);

    /* Reciprocal gamma vanishes at non-positive integers; start with the first non-zero coefficient. */
    if (num_is_integer(gamma_argument) && num_le(gamma_argument, NUM_ZERO)) {
        double skipped = 1.0 - num_to_double(gamma_argument);

        if (skipped >= maximum_terms)
            goto cleanup;
        first = (unsigned int)skipped;
    }

    exponent = num_add_long(*order, 2L * first + (struve ? 1L : 0L));
    number_t left_argument = num_add_long(shift, first);
    number_t right_argument = num_add_long(gamma_argument, first);
    number_t gamma_left = num_gamma(left_argument);
    number_t gamma_right = num_gamma(right_argument);
    number_t coefficient = num_div(num_pow(NUM_HALF, exponent), num_mul(gamma_left, gamma_right));

    num_destroy(&right_argument);
    num_destroy(&left_argument);
    if (alternating && (first & 1u))
        coefficient = num_neg(coefficient);

    if (!num_is_finite(coefficient) || num_eq(coefficient, NUM_ZERO))
        goto cleanup;
    if (num_is_integer(exponent) && num_to_double(exponent) > INT_MIN && num_to_double(exponent) <= INT_MAX)
        power = mat_pow_int(A, (int)num_to_double(exponent));
    else
        power = mat_pow(A, &exponent);
    num_destroy(&exponent);
    if (!power)
        goto cleanup;
    if (matrix_is_symbolic(power)) {
        matrix_t *numeric = mat_convert_preserving_store(power, &number_elem);

        mat_free(power);
        power = numeric;
    }
    square = mat_mul(A, A);
    term = power ? mat_scalar_mul(power, &coefficient) : NULL;
    sum = term ? mat_copy_preserving_store(term) : NULL;
    if (!square || !term || !sum || mat_norm(square, MAT_NORM_INF, &square_norm) != 0 ||
        !num_is_finite(square_norm))
        goto cleanup;

    for (unsigned int k = first; k < maximum_terms; ++k) {
        NUM_SCOPE(iteration);
        number_t term_norm = number_invalid(), sum_norm = number_invalid();
        number_t first_factor = num_create_from_long(2L * k + (struve ? 3L : 2L));
        number_t second = num_add(num_mul(NUM_TWO, *order), first_factor);
        number_t denominator = num_mul(first_factor, second);
        number_t ratio = num_div(square_norm, num_abs(denominator));
        bool converged;

        if (alternating)
            denominator = num_neg(denominator);

        if (mat_norm(term, MAT_NORM_INF, &term_norm) != 0 || mat_norm(sum, MAT_NORM_INF, &sum_norm) != 0 ||
            !num_is_finite(term_norm) || !num_is_finite(sum_norm)) {
            num_destroy(&sum_norm);
            num_destroy(&term_norm);
            goto cleanup;
        }
        /* Once the real part of the second factor is positive, denominator magnitudes increase. */
        converged = num_eq(term_norm, NUM_ZERO) ||
                    (num_gt(num_real_part(second), NUM_ZERO) && num_le(ratio, NUM_HALF) &&
                     num_le(num_mul(NUM_TWO, term_norm), num_mul(tolerance, num_add(NUM_ONE, sum_norm))));
        num_destroy(&sum_norm);
        num_destroy(&term_norm);
        if (converged) {
            num_destroy(&square_norm);
            mat_free(term);
            mat_free(square);
            mat_free(power);
            return cylindrical_detach_entries(sum);
        }

        matrix_t *product = mat_mul(term, square);
        matrix_t *next_term = product ? mat_scalar_div(product, &denominator) : NULL;
        matrix_t *next_sum = next_term ? mat_add(sum, next_term) : NULL;

        mat_free(product);
        mat_free(term);
        term = cylindrical_detach_entries(next_term);
        mat_free(sum);
        sum = cylindrical_detach_entries(next_sum);
        if (!term || !sum)
            goto cleanup;
    }

cleanup:
    num_destroy(&exponent);
    num_destroy(&square_norm);
    mat_free(sum);
    mat_free(term);
    mat_free(square);
    mat_free(power);
    return NULL;
}

static void cylindrical_number_scalar(void *out, const void *input)
{
    *(number_t *)out = cylindrical_context->number_function(*cylindrical_context->order, *(const number_t *)input);
}

static void cylindrical_expression_scalar(void *out, const void *input)
{
    expr_t *order = expr_new_const(*cylindrical_context->order);

    *(expr_t **)out = order ? cylindrical_context->expression_function(order, *(expr_t *const *)input) : NULL;
    expr_free(order);
}

/* Retain bindings and analytic derivatives through the common symbolic matrix machinery. */
static matrix_t *cylindrical_symbolic(const matrix_t *A, const number_t *order,
                                     cylindrical_number_fn number_function,
                                     cylindrical_expression_fn expression_function)
{
    cylindrical_context_t context = {order, number_function, expression_function};
    const cylindrical_context_t *previous = cylindrical_context;

    cylindrical_context = &context;
    matrix_t *result = mat_apply_scalar_callbacks(A, cylindrical_number_scalar, cylindrical_expression_scalar);

    cylindrical_context = previous;
    return result;
}

/* Share analytic series and symbolic functional calculus between cylindrical functions. */
static matrix_t *cylindrical_function(const matrix_t *A, const number_t *order, bool struve, bool alternating,
                                     cylindrical_number_fn number_function,
                                     cylindrical_expression_fn expression_function)
{
    matrix_t *result = NULL;
    size_t precision;
    bool entire;

    if (!A || !order || !num_is_finite(*order) || A->rows != A->cols || A->rows == 0u)
        return NULL;
    if (!struve && num_is_integer(*order) && num_lt(*order, NUM_ZERO)) {
        number_t positive_order = num_neg(*order);

        result = cylindrical_function(A, &positive_order, false, alternating, number_function, expression_function);
        num_destroy(&positive_order);
        return result;
    }
    cylindrical_context_t context = {order, number_function, expression_function};

    if (matrix_is_symbolic(A))
        return cylindrical_symbolic(A, order, number_function, expression_function);
    precision = num_get_effective_prec_bits(*order);
    entire = num_is_integer(*order) && num_ge(*order, struve ? NUM_NEG_ONE : NUM_ZERO);
    for (size_t row = 0u; row < A->rows; ++row) {
        for (size_t col = 0u; col < A->cols; ++col) {
            NUM_SCOPE(entry_scope);
            number_t entry = mat_get_num(A, row, col);
            size_t bits = num_get_effective_prec_bits(entry);

            if (!num_is_finite(entry))
                return NULL;
            if (bits < precision)
                precision = bits;
        }
    }
    if (precision < 16u)
        precision = 16u;
    if (precision > INT_MAX)
        return NULL;

    /* A finite scalar value at zero does not make a fractional power analytic there. */
    if (!entire) {
        number_t determinant = number_invalid();
        bool singular = mat_det(A, &determinant) != 0 || !num_is_finite(determinant) ||
                        num_eq(determinant, NUM_ZERO);

        num_destroy(&determinant);
        if (singular)
            return NULL;
    }
    if (mat_is_diagonal(A)) {
        result = mat_create_diagonal_with_elem(A->rows, &number_elem);
        if (!result)
            return NULL;
        for (size_t i = 0u; i < A->rows; ++i) {
            NUM_SCOPE(entry_scope);
            number_t value = number_function(*order, mat_get_num(A, i, i));

            if (!num_is_finite(value)) {
                num_destroy(&value);
                mat_free(result);
                return NULL;
            }
            mat_set_num_owned(result, i, i, &value);
        }
        return result;
    }
    result = cylindrical_series(A, order, precision, struve, alternating);
    if (!result) {
        expr_t *one = expr_new_const(NUM_ONE);

        result = one ? mat_pow_expr_spectral_map(A, one, cylindrical_spectral, &context) : NULL;
        expr_free(one);
    }
    return result;
}

/* Evaluate modified Bessel I using analytic numeric or symbolic matrix functional calculus. */
matrix_t *mat_bessel_i(const matrix_t *A, const number_t *order)
{
    return cylindrical_function(A, order, false, false, num_bessel_i, expr_bessel_i);
}

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

        num_destroy(&shifted);
        if (!num_is_finite(value)) {
            num_destroy(&value);
            return num_clone(NUM_NAN);
        }
        sum = num_add(sum, num_mul(weight, value));
        num_destroy(&value);
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
            if (!num_is_finite(value)) {
                num_destroy(&value);
                goto cleanup;
            }
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
        return cylindrical_symbolic(A, order, num_bessel_y, expr_bessel_y);
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
                num_destroy(&value);
                mat_free(result);
                return NULL;
            }
            mat_set_num_owned(result, i, i, &value);
        }
        return result;
    }
    return bessel_y_interpolate(A, order);
}

/* Evaluate ordinary Struve H using its alternating matrix series and symbolic functional calculus. */
matrix_t *mat_struve_h(const matrix_t *A, const number_t *order)
{
    return cylindrical_function(A, order, true, true, num_struve_h, expr_struve_h);
}

/* Evaluate modified Struve L using analytic numeric or symbolic matrix functional calculus. */
matrix_t *mat_struve_l(const matrix_t *A, const number_t *order)
{
    return cylindrical_function(A, order, true, false, num_struve_l, expr_struve_l);
}
