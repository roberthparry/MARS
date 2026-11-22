/**
 * @file number_qdigamma_deriv.c
 * @brief Analytic numerical q-digamma partials backed by a private Lambert kernel.
 *
 * Euler derivatives q*d/dq shift the kernel's weight and polylogarithm order. Signed Stirling numbers
 * convert them to ordinary q derivatives, including mixed argument derivatives. Real bases greater than
 * one use reciprocal-base continuation. Exact Bernoulli-polynomial jets handle the real extension at one.
 * The private kernel and these derivative evaluators are not installed public special functions.
 */
#include <stdint.h>

#include "number-t.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

/* Bernoulli numbers use exact rational arithmetic; only the supplied argument may be inexact. */
static number_t qdigamma_bernoulli(unsigned degree, number_t z)
{
    NUM_SCOPE(scope);
    number_t b[17];
    b[0] = num_clone(NUM_ONE);
    for (unsigned m = 1; m <= degree; ++m) {
        number_t sum = num_clone(NUM_ZERO);
        long binomial = 1;
        for (unsigned k = 0; k < m; ++k) {
            number_t term = num_mul_long(b[k], binomial), next = num_add(sum, term);
            num_destroy(&term); num_destroy(&sum); sum = next;
            binomial = binomial * (long)(m + 1 - k) / (long)(k + 1);
        }
        number_t negative = num_neg(sum);
        number_t divisor = num_create_from_long((long)m + 1);
        b[m] = num_div(negative, divisor);
    }
    number_t polynomial = num_clone(b[0]);
    long binomial = 1;
    for (unsigned k = 1; k <= degree; ++k) {
        binomial = binomial * (long)(degree + 1 - k) / (long)k;
        number_t coefficient = num_mul_long(b[k], binomial), product = num_mul(polynomial, z);
        number_t next = num_add(product, coefficient);
        num_destroy(&polynomial); polynomial = next;
    }
    return num_scope_detach(polynomial);
}

static number_t qdigamma_unit_jet(number_t z, unsigned euler_order, unsigned z_order)
{
    NUM_SCOPE(scope);
    if (!euler_order) return num_scope_detach(z_order ? num_polygamma(z_order, z) : num_digamma(z));
    if (z_order > euler_order) return num_clone(NUM_ZERO);
    number_t polynomial = qdigamma_bernoulli(euler_order - z_order, z);
    long falling = 1;
    for (unsigned j = 0; j < z_order; ++j) falling *= (long)(euler_order - j);
    number_t scaled = num_mul_long(polynomial, falling);
    number_t factor = num_sub(z_order ? NUM_ZERO : NUM_ONE, scaled);
    number_t index = num_create_from_long(1L - (long)euler_order), zeta = num_zeta(index);
    return num_scope_detach(num_mul(zeta, factor));
}

/* Differentiate log(q)^p times a kernel by the exact binomial product rule. */
static number_t qdigamma_euler(number_t q, number_t z, unsigned euler_order, unsigned z_order)
{
    NUM_SCOPE(scope);
    number_t logarithm = num_log(q);
    if (!z_order) {
        number_t first = number_lambert_series(NUM_ONE, q, z, euler_order, -(long)euler_order, SIZE_MAX);
        number_t product = num_mul(logarithm, first);
        if (!euler_order) {
            number_t one_minus = num_sub(NUM_ONE, q), log_one_minus = num_log(one_minus);
            return num_scope_detach(num_sub(product, log_one_minus));
        }
        number_t second = number_lambert_series(NUM_ONE, q, z, euler_order - 1,
                                                 1L - (long)euler_order, SIZE_MAX);
        number_t weighted = num_mul_long(second, (long)euler_order);
        /* D^k[-log(1-q)] = Li_(1-k)(q): a one-term kernel supplies its rational continuation. */
        number_t elementary = number_lambert_series(q, NUM_ONE, NUM_ZERO, 0,
                                                      1L - (long)euler_order, 1);
        number_t sum = num_add(product, weighted);
        return num_scope_detach(num_add(sum, elementary));
    }
    unsigned power = z_order + 1;
    unsigned last = euler_order < power ? euler_order : power;
    number_t sum = num_clone(NUM_ZERO), coefficient = num_clone(NUM_ONE);
    for (unsigned j = 0; j <= last; ++j) {
        NUM_SCOPE(term_scope);
        number_t series = number_lambert_series(NUM_ONE, q, z, euler_order - j,
                                                 -(long)z_order - (long)euler_order + (long)j, SIZE_MAX);
        number_t log_power = num_pow_int(logarithm, (int)(power - j));
        number_t weighted = num_mul(coefficient, series), term = num_mul(weighted, log_power);
        number_t next = num_add(sum, term);
        num_destroy(&sum); sum = num_scope_detach(next);
        if (j < last) {
            number_t a = num_mul_long(coefficient, (long)(euler_order - j));
            number_t b = num_mul_long(a, (long)(power - j)), divisor = num_create_from_long((long)j + 1);
            number_t c = num_div(b, divisor);
            num_destroy(&coefficient); coefficient = num_scope_detach(c);
        }
    }
    num_destroy(&coefficient);
    return num_scope_detach(sum);
}

/* Evaluate an analytic partial; the new series remains an implementation detail of existing q-functions. */
number_t number_qdigamma_partial(number_t q, number_t z, unsigned q_order, unsigned z_order)
{
    NUM_SCOPE(scope);
    if (q_order > 16 || z_order > 16 || !num_is_finite(q) || !num_is_finite(z) || num_is_zero(q))
        return num_clone(NUM_NAN);
    if (num_is_real(z) && num_is_integer(z) && num_le(z, NUM_ZERO)) return num_clone(NUM_NAN);
    number_t radius = num_abs(q);
    bool unit = num_is_one(q), reciprocal = num_gt(radius, NUM_ONE);
    if (!unit && num_eq(radius, NUM_ONE)) return num_clone(NUM_NAN);
    number_t base = reciprocal ? num_div(NUM_ONE, q) : num_clone(q);
    long stirling[17] = {1};
    for (unsigned n = 1; n <= q_order; ++n) {
        for (unsigned k = n; k > 0; --k)
            stirling[k] = stirling[k - 1] - (long)(n - 1) * stirling[k];
        stirling[0] = 0;
    }
    number_t sum = num_clone(NUM_ZERO);
    for (unsigned k = 0; k <= q_order; ++k) {
        if (!stirling[k]) continue;
        NUM_SCOPE(term_scope);
        number_t derivative = unit ? qdigamma_unit_jet(z, k, z_order) : qdigamma_euler(base, z, k, z_order);
        if (reciprocal) {
            number_t continued = (k & 1) ? num_neg(derivative) : num_clone(derivative);
            number_t offset = z_order ? num_clone(NUM_ONE) : num_sub(z, NUM_ONE_AND_HALF);
            number_t correction = z_order > 1 || k > 1 ? num_clone(NUM_ZERO) :
                                  k ? num_clone(offset) : num_mul(offset, num_log(q));
            num_destroy(&derivative); derivative = num_add(continued, correction);
        }
        number_t term = num_mul_long(derivative, stirling[k]), next = num_add(sum, term);
        num_destroy(&sum); sum = num_scope_detach(next);
    }
    number_t denominator = num_pow_int(q, (int)q_order), out = num_div(sum, denominator);
    num_destroy(&sum);
    return num_scope_detach(out);
}
