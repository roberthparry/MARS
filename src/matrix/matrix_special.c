/**
 * @file matrix_special.c
 * @brief Matrix polynomial and signal-function families.
 *
 * Implements supported orthogonal polynomials, finite logarithmic sums and signal functions over matrices. The
 * functions use matrix algebra where meaningful rather than assuming scalar elementwise semantics.
 *
 * This is part of matrix.h. Preserve numeric and symbolic element semantics and supported storage forms; callers
 * must not rely on private matrix representation.
 */

#include <math.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"

/* Polynomial recurrences and finite logarithmic sums. */

static matrix_t *polynomial(const matrix_t *a, unsigned int degree, unsigned family)
{
    if (!a || a->rows != a->cols)
        return NULL;
    matrix_t *previous = mat_pow_int(a, 0);
    if (degree == 0u)
        return previous;
    number_t two = num_create_from_long(2);
    matrix_t *current = mat_scalar_mul((matrix_t *)a, family == 0u ? &NUM_ONE : &two);
    for (unsigned int k = 1u; previous && current && k < degree; ++k) {
        matrix_t *product = mat_mul(a, current);
        matrix_t *twice = product ? mat_scalar_mul(product, &two) : NULL;
        number_t factor = family == 2u ? num_create_from_long(2L * k) : num_clone(NUM_ONE);
        matrix_t *back = mat_scalar_mul(previous, &factor);
        matrix_t *next = twice && back ? mat_sub(twice, back) : NULL;
        num_destroy(&factor);
        mat_free(product);
        mat_free(twice);
        mat_free(back);
        mat_free(previous);
        previous = current;
        current = next;
    }
    num_destroy(&two);
    mat_free(previous);
    return current;
}

/* Evaluate a first-kind Chebyshev matrix polynomial, not an entrywise function. */
matrix_t *mat_chebyshev_t(const matrix_t *a, unsigned int n) { return polynomial(a, n, 0u); }
/* Evaluate a second-kind Chebyshev matrix polynomial. */
matrix_t *mat_chebyshev_u(const matrix_t *a, unsigned int n) { return polynomial(a, n, 1u); }
/* Evaluate a physicists' Hermite matrix polynomial. */
matrix_t *mat_hermite_h(const matrix_t *a, unsigned int n) { return polynomial(a, n, 2u); }

/* Evaluate the finite logarithmic matrix polynomial by repeated multiplication. */
matrix_t *mat_harmonic_poly(const matrix_t *A, unsigned int degree)
{
    matrix_t *power = NULL;
    matrix_t *sum = NULL;

    if (!A || A->rows != A->cols)
        return NULL;
    power = mat_copy_preserving_store(A);
    sum = power ? mat_scalar_mul(power, &NUM_ZERO) : NULL;
    if (!power || !sum)
        goto cleanup;
    if (degree == 0u) {
        mat_free(power);
        return sum;
    }

    for (unsigned int k = 1u;; ++k) {
        number_t divisor = num_create_from_long((long)k);
        matrix_t *term = mat_scalar_div(power, &divisor);
        matrix_t *next_sum = term ? mat_add(sum, term) : NULL;

        num_destroy(&divisor);
        mat_free(term);
        if (!next_sum)
            goto cleanup;
        mat_free(sum);
        sum = next_sum;
        if (k == degree)
            break;

        matrix_t *next_power = mat_mul(power, A);

        if (!next_power)
            goto cleanup;
        mat_free(power);
        power = next_power;
    }

    mat_free(power);
    return sum;

cleanup:
    mat_free(sum);
    mat_free(power);
    return NULL;
}

/* Special-function series. */

/* Evaluate the Lerch matrix function through its defining power series. */
matrix_t *mat_lerch_phi(const matrix_t *Z, const number_t *s, const number_t *a)
{
    const unsigned int maximum_terms = 4096u;
    matrix_t *power = NULL;
    matrix_t *sum = NULL;

    if (!Z || !s || !a || Z->rows != Z->cols || Z->elem != &number_elem)
        return NULL;
    power = mat_create_identity(Z->rows);
    sum = power ? mat_scalar_mul(power, &NUM_ZERO) : NULL;
    if (!power || !sum)
        goto cleanup;
    for (unsigned int k = 0u; k < maximum_terms; ++k) {
        number_t shift = num_add_long(*a, (long)k);
        number_t denominator = num_pow(shift, *s);
        number_t coefficient = num_div(NUM_ONE, denominator);
        matrix_t *term = mat_scalar_mul(power, &coefficient);
        matrix_t *next_sum = term ? mat_add(sum, term) : NULL;
        number_t term_norm = NUM_NAN;
        number_t sum_norm = NUM_NAN;
        bool converged = term && next_sum && mat_norm(term, MAT_NORM_INF, &term_norm) == 0 &&
                         mat_norm(next_sum, MAT_NORM_INF, &sum_norm) == 0 && k > 8u &&
                         num_to_double(term_norm) <= 1e-30 * (1.0 + num_to_double(sum_norm));
        matrix_t *next_power = !converged && k + 1u < maximum_terms ? mat_mul(power, Z) : NULL;

        num_destroy(&sum_norm);
        num_destroy(&term_norm);
        num_destroy(&coefficient);
        num_destroy(&denominator);
        num_destroy(&shift);
        mat_free(term);
        if (!next_sum || (!converged && k + 1u < maximum_terms && !next_power)) {
            mat_free(next_sum);
            mat_free(next_power);
            goto cleanup;
        }
        mat_free(sum);
        sum = next_sum;
        if (converged) {
            mat_free(power);
            return sum;
        }
        if (k + 1u < maximum_terms) {
            mat_free(power);
            power = next_power;
        }
    }
    mat_free(power);
    return sum;

cleanup:
    mat_free(sum);
    mat_free(power);
    return NULL;
}

/* Evaluate the q-digamma matrix function through its Lambert series. */
matrix_t *mat_qdigamma(const matrix_t *Z, const number_t *q)
{
    const unsigned int maximum_terms = 4096u;
    matrix_t *identity = NULL;
    matrix_t *scaled = NULL;
    matrix_t *base = NULL;
    matrix_t *power = NULL;
    matrix_t *sum = NULL;
    number_t magnitude = NUM_NAN;
    number_t log_q = NUM_NAN;
    number_t q_power = NUM_NAN;
    bool series_converged = false;

    if (!Z || !q || Z->rows != Z->cols || Z->elem != &number_elem || num_is_nan(*q) || num_is_zero(*q))
        return NULL;
    if (num_is_one(*q))
        return mat_digamma(Z);
    magnitude = num_abs(*q);
    if (num_gt(magnitude, NUM_ONE)) {
        number_t reciprocal = num_div(NUM_ONE, *q);
        number_t three_halves = num_create_from_string("1.5");
        matrix_t *continued = mat_qdigamma(Z, &reciprocal);
        matrix_t *offset;
        matrix_t *shifted;
        matrix_t *correction;
        matrix_t *out;

        identity = mat_create_identity(Z->rows);
        offset = identity ? mat_scalar_mul(identity, &three_halves) : NULL;
        shifted = offset ? mat_sub(Z, offset) : NULL;
        log_q = num_log(*q);
        correction = shifted ? mat_scalar_mul(shifted, &log_q) : NULL;
        out = continued && correction ? mat_add(continued, correction) : NULL;
        mat_free(correction);
        mat_free(shifted);
        mat_free(offset);
        mat_free(identity);
        mat_free(continued);
        num_destroy(&three_halves);
        num_destroy(&reciprocal);
        num_destroy(&log_q);
        num_destroy(&magnitude);
        return out;
    }
    if (fabs(num_to_double(magnitude) - 1.0) <= 1e-14 || num_ge(magnitude, NUM_ONE)) {
        num_destroy(&magnitude);
        return NULL;
    }

    log_q = num_log(*q);
    identity = mat_create_identity(Z->rows);
    scaled = identity ? mat_scalar_mul((matrix_t *)Z, &log_q) : NULL;
    base = scaled ? mat_exp(scaled) : NULL;
    power = base ? mat_copy_preserving_store(base) : NULL;
    if (identity) {
        number_t one_minus_q = num_sub(NUM_ONE, *q);
        number_t logarithm = num_log(one_minus_q);
        number_t constant = num_neg(logarithm);

        sum = mat_scalar_mul(identity, &constant);
        num_destroy(&constant);
        num_destroy(&logarithm);
        num_destroy(&one_minus_q);
    }
    q_power = num_clone(*q);
    if (!identity || !scaled || !base || !power || !sum)
        goto cleanup;

    for (unsigned int k = 1u; k <= maximum_terms; ++k) {
        number_t denominator = num_sub(NUM_ONE, q_power);
        number_t coefficient;
        matrix_t *term;
        matrix_t *next_sum;
        number_t term_norm = NUM_NAN;
        number_t sum_norm = NUM_NAN;
        bool converged;
        matrix_t *next_power = NULL;

        if (num_is_zero(denominator)) {
            num_destroy(&denominator);
            goto cleanup;
        }
        coefficient = num_div(log_q, denominator);
        term = mat_scalar_mul(power, &coefficient);
        next_sum = term ? mat_add(sum, term) : NULL;
        converged = term && next_sum && mat_norm(term, MAT_NORM_INF, &term_norm) == 0 &&
                    mat_norm(next_sum, MAT_NORM_INF, &sum_norm) == 0 && k > 8u &&
                    num_to_double(term_norm) <= 1e-30 * (1.0 + num_to_double(sum_norm));
        if (!converged && k < maximum_terms)
            next_power = mat_mul(power, base);
        num_destroy(&sum_norm);
        num_destroy(&term_norm);
        num_destroy(&coefficient);
        num_destroy(&denominator);
        mat_free(term);
        if (!next_sum || (!converged && k < maximum_terms && !next_power)) {
            mat_free(next_sum);
            mat_free(next_power);
            goto cleanup;
        }
        mat_free(sum);
        sum = next_sum;
        if (converged) {
            series_converged = true;
            break;
        }
        if (k < maximum_terms) {
            number_t next_q_power = num_mul(q_power, *q);

            mat_free(power);
            power = next_power;
            num_destroy(&q_power);
            q_power = next_q_power;
        }
    }
    if (!series_converged)
        goto cleanup;

    mat_free(power);
    mat_free(base);
    mat_free(scaled);
    mat_free(identity);
    num_destroy(&q_power);
    num_destroy(&log_q);
    num_destroy(&magnitude);
    return sum;

cleanup:
    mat_free(sum);
    mat_free(power);
    mat_free(base);
    mat_free(scaled);
    mat_free(identity);
    num_destroy(&q_power);
    num_destroy(&log_q);
    num_destroy(&magnitude);
    return NULL;
}

/* Spectral signal functions. */

static void number_sgn(void *out, const void *in)
{
    number_t value = num_sgn(*(const number_t *)in);

    num_destroy((number_t *)out);
    *(number_t *)out = value;
}

static void expression_sgn(void *out, const void *in)
{
    expr_t *value = expr_sgn(*(expr_t *const *)in);

    expr_free(*(expr_t **)out);
    *(expr_t **)out = value;
}

static void number_step(void *out, const void *in)
{
    number_t value = num_step(*(const number_t *)in);

    num_destroy((number_t *)out);
    *(number_t *)out = value;
}

static void expression_step(void *out, const void *in)
{
    expr_t *value = expr_step(*(expr_t *const *)in);

    expr_free(*(expr_t **)out);
    *(expr_t **)out = value;
}

static void number_rect(void *out, const void *in)
{
    number_t value = num_rect(*(const number_t *)in);

    num_destroy((number_t *)out);
    *(number_t *)out = value;
}

static void expression_rect(void *out, const void *in)
{
    expr_t *value = expr_rect(*(expr_t *const *)in);

    expr_free(*(expr_t **)out);
    *(expr_t **)out = value;
}

static void number_tri(void *out, const void *in)
{
    number_t value = num_tri(*(const number_t *)in);

    num_destroy((number_t *)out);
    *(number_t *)out = value;
}

static void expression_tri(void *out, const void *in)
{
    expr_t *value = expr_tri(*(expr_t *const *)in);

    expr_free(*(expr_t **)out);
    *(expr_t **)out = value;
}

static void number_circ(void *out, const void *in)
{
    number_t value = num_circ(*(const number_t *)in);

    num_destroy((number_t *)out);
    *(number_t *)out = value;
}

static void expression_circ(void *out, const void *in)
{
    expr_t *value = expr_circ(*(expr_t *const *)in);

    expr_free(*(expr_t **)out);
    *(expr_t **)out = value;
}

static void number_sinc(void *out, const void *in)
{
    number_t value = num_sinc(*(const number_t *)in);

    num_destroy((number_t *)out);
    *(number_t *)out = value;
}

static void expression_sinc(void *out, const void *in)
{
    expr_t *value = expr_sinc(*(expr_t *const *)in);

    expr_free(*(expr_t **)out);
    *(expr_t **)out = value;
}

/* Apply real signum to the spectrum. */
matrix_t *mat_sgn(const matrix_t *a) { return mat_apply_scalar_callbacks(a, number_sgn, expression_sgn); }
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
