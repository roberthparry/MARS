/**
 * @file number_zeta_blocks.c
 * @brief Bounded binomial blocks for finite shifted Dirichlet sums and order derivatives.
 *
 * Groups thirty-two consecutive bases about one centre and shares binomial-weighted moments between centres.
 * Symmetric centres make every odd moment zero; positive even moments need only sixteen scalar recurrences.
 * Analytic coefficient derivatives avoid dividing by a possibly zero rising factorial. Directed majorants cover
 * both polynomial tails, and an absolute arithmetic allowance checks the completed finite sum before rounding.
 * Unsupported domains and exhausted bounds select the owning zeta kernel's ordinary finite summation.
 * One small, mutex-protected finite-sum entry preserves warmed calls without affecting cold or changing inputs.
 */
#include <pthread.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { ZETA_BLOCK = 32, ZETA_HALF = 16, ZETA_HALF_BITS = 4 };

static mpc_t shifted_argument, shifted_offset, shifted_value, shifted_slope;
static mpfr_prec_t shifted_precision;
static unsigned long shifted_count;
static bool shifted_derivative;
static pthread_mutex_t shifted_lock = PTHREAD_MUTEX_INITIALIZER;

static bool shifted_same(mpc_srcptr a, mpc_srcptr b)
{
    return !mpc_cmp(a, b) &&
        (!mpfr_zero_p(mpc_realref(a)) || mpfr_signbit(mpc_realref(a)) == mpfr_signbit(mpc_realref(b))) &&
        (!mpfr_zero_p(mpc_imagref(a)) || mpfr_signbit(mpc_imagref(a)) == mpfr_signbit(mpc_imagref(b)));
}

static bool shifted_copy(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr s, mpc_srcptr a, unsigned long count)
{
    if (pthread_mutex_lock(&shifted_lock)) return false;
    bool found = shifted_precision >= mpc_get_prec(sum) && shifted_count == count &&
        (!derivative || shifted_derivative) && shifted_same(s, shifted_argument) && shifted_same(a, shifted_offset);
    if (found) {
        mpc_set(sum, shifted_value, MPC_RNDNN);
        if (derivative) mpc_set(derivative, shifted_slope, MPC_RNDNN);
    }
    pthread_mutex_unlock(&shifted_lock);
    return found;
}

static void shifted_save(mpc_srcptr sum, mpc_srcptr derivative, mpc_srcptr s, mpc_srcptr a, unsigned long count)
{
    if (mpc_get_prec(s) > 16384 || mpc_get_prec(a) > 16384 || pthread_mutex_lock(&shifted_lock)) return;
    if (!shifted_precision) {
        mpc_init2(shifted_argument, mpc_get_prec(s)); mpc_init2(shifted_offset, mpc_get_prec(a));
        mpc_init2(shifted_value, mpc_get_prec(sum)); mpc_init2(shifted_slope, mpc_get_prec(sum));
    } else {
        mpc_set_prec(shifted_argument, mpc_get_prec(s)); mpc_set_prec(shifted_offset, mpc_get_prec(a));
        mpc_set_prec(shifted_value, mpc_get_prec(sum)); mpc_set_prec(shifted_slope, mpc_get_prec(sum));
    }
    mpc_set(shifted_argument, s, MPC_RNDNN); mpc_set(shifted_offset, a, MPC_RNDNN);
    mpc_set(shifted_value, sum, MPC_RNDNN);
    if (derivative) mpc_set(shifted_slope, derivative, MPC_RNDNN);
    shifted_precision = mpc_get_prec(sum); shifted_count = count; shifted_derivative = derivative != NULL;
    pthread_mutex_unlock(&shifted_lock);
}

typedef struct _shifted_coefficients_t shifted_coefficients_t;
struct _shifted_coefficients_t {
    mpc_t *values, *derivatives;
    unsigned long count, capacity;
    mpc_t binomial, slope, factor, temporary;
    mpfr_t weights[ZETA_HALF], moment, scratch[4];
    mpfr_prec_t precision;
};

static bool shifted_coefficients_init(shifted_coefficients_t *c, mpfr_prec_t precision, bool derivative)
{
    c->count = 0; c->capacity = ((unsigned long)precision + 1024) / 2; c->precision = precision;
    c->values = calloc(c->capacity, sizeof *c->values);
    c->derivatives = derivative ? calloc(c->capacity, sizeof *c->derivatives) : NULL;
    if (!c->values || (derivative && !c->derivatives)) { free(c->values); free(c->derivatives); return false; }
    mpc_init2(c->binomial, precision); mpc_init2(c->slope, precision);
    mpc_init2(c->factor, precision); mpc_init2(c->temporary, precision);
    mpc_set_ui(c->binomial, 1u, MPC_RNDNN); mpc_set_ui(c->slope, 0u, MPC_RNDNN);
    for (unsigned k = 0; k < ZETA_HALF; ++k) {
        mpfr_init2(c->weights[k], precision); mpfr_set_ui(c->weights[k], 1u, MPFR_RNDN);
    }
    mpfr_init2(c->moment, precision);
    for (unsigned k = 0; k < 4; ++k) mpfr_init2(c->scratch[k], precision);
    return true;
}

static void shifted_coefficients_clear(shifted_coefficients_t *c)
{
    for (unsigned long j = 0; j < c->count; ++j) {
        mpc_clear(c->values[j]);
        if (c->derivatives) mpc_clear(c->derivatives[j]);
    }
    free(c->derivatives); free(c->values);
    for (unsigned k = 0; k < ZETA_HALF; ++k) mpfr_clear(c->weights[k]);
    for (unsigned k = 0; k < 4; ++k) mpfr_clear(c->scratch[k]);
    mpfr_clear(c->moment);
    mpc_clear(c->temporary); mpc_clear(c->factor); mpc_clear(c->slope); mpc_clear(c->binomial);
}

/* Every block starts beyond 32, so |16/centre| <= 16/47.75 < 3/8.
 * (3/8)^5 < 2^-7 gives an exact rational exponent bound without logarithms.
 * The 320-bit allowance covers polynomial growth and the coefficient recurrence's rounding counts. */
static mpfr_prec_t shifted_coefficient_precision(mpfr_prec_t work, unsigned long index)
{
    long adjustment = 320 - (long)(7ul * index / 5ul);
    return adjustment < 0 ? (adjustment < 128 - work ? 128 : work + adjustment) : work;
}

/* M_0=B, M_(2j+1)=0, M_(2j)=2*sum_(k=0)^(B/2-1)((2k+1)/B)^(2j).
 * q_(j+1)=q_j*(s+j)/(j+1), q'_(j+1)=(q'_j*(s+j)+q_j)/(j+1), including integral s. */
static bool shifted_coefficients(shifted_coefficients_t *c, mpc_srcptr s, unsigned long index)
{
    if (index >= c->capacity) return false;
    while (c->count <= index) {
        unsigned long slot = c->count;
        for (unsigned long j = slot ? 2 * slot - 1 : 1; j <= 2 * slot; ++j) {
            mpfr_prec_t precision = shifted_coefficient_precision(c->precision, j);
            mpfr_prec_round(mpc_realref(c->binomial), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(c->binomial), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_realref(c->slope), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(c->slope), precision, MPFR_RNDN);
            if (mpc_get_prec(c->factor) != precision) {
                mpc_set_prec(c->factor, precision); mpc_set_prec(c->temporary, precision);
                for (unsigned k = 0; k < 4; ++k) mpfr_set_prec(c->scratch[k], precision);
            }
            mpc_add_ui(c->factor, s, j - 1, MPC_RNDNN);
            if (c->derivatives) {
                number_mpc_mul_guarded(c->temporary, c->slope, c->factor, c->scratch);
                mpc_add(c->slope, c->temporary, c->binomial, MPC_RNDNN);
                mpc_div_ui(c->slope, c->slope, j, MPC_RNDNN);
            }
            number_mpc_mul_guarded(c->binomial, c->binomial, c->factor, c->scratch);
            mpc_div_ui(c->binomial, c->binomial, j, MPC_RNDNN);
        }
        if (!slot) mpfr_set_ui(c->moment, ZETA_BLOCK, MPFR_RNDN);
        else {
            mpfr_prec_t precision = mpc_get_prec(c->binomial);
            if (mpfr_get_prec(c->moment) != precision) mpfr_set_prec(c->moment, precision);
            mpfr_set_zero(c->moment, 1);
            for (unsigned k = 0; k < ZETA_HALF; ++k) {
                mpfr_prec_round(c->weights[k], precision, MPFR_RNDN);
                mpfr_mul_ui(c->weights[k], c->weights[k], (2 * k + 1) * (2 * k + 1), MPFR_RNDN);
                mpfr_div_2ui(c->weights[k], c->weights[k], 2 * (ZETA_HALF_BITS + 1), MPFR_RNDN);
                mpfr_add(c->moment, c->moment, c->weights[k], MPFR_RNDN);
            }
            mpfr_mul_2ui(c->moment, c->moment, 1u, MPFR_RNDN);
        }
        mpc_init2(c->values[slot], mpc_get_prec(c->binomial));
        mpc_mul_fr(c->values[slot], c->binomial, c->moment, MPC_RNDNN);
        if (c->derivatives) {
            mpc_init2(c->derivatives[slot], mpc_get_prec(c->binomial));
            mpc_mul_fr(c->derivatives[slot], c->slope, c->moment, MPC_RNDNN);
        }
        ++c->count;
    }
    return true;
}

/* q_j(S)+q'_j(S) <= q_j(S+1) for S>=0, by the positive-coefficient Taylor expansion of q_j.
 * This single majorant bounds both order polynomials without dividing by a rising factorial. */
static unsigned long shifted_degree(mpfr_ptr tail, mpfr_srcptr radius, mpfr_srcptr order, mpfr_prec_t work)
{
    mpfr_t coefficient, ratio, denominator, tolerance;
    mpfr_inits2(64, coefficient, ratio, denominator, tolerance, (mpfr_ptr)0);
    mpfr_set_ui(coefficient, ZETA_BLOCK, MPFR_RNDN);
    mpfr_set_ui_2exp(tolerance, 1u, -work + 96, MPFR_RNDD);
    unsigned long degree = 0;
    for (unsigned long j = 1; j < (unsigned long)work + 1024; ++j) {
        mpfr_add_ui(ratio, order, j, MPFR_RNDU);
        mpfr_div_ui(ratio, ratio, j, MPFR_RNDU); mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        mpfr_mul(coefficient, coefficient, ratio, MPFR_RNDU);
        if (j < 16 || j % 8) continue;
        mpfr_add_ui(ratio, order, 1u, MPFR_RNDU); mpfr_div_ui(ratio, ratio, j + 1, MPFR_RNDU);
        mpfr_add_ui(ratio, ratio, 1u, MPFR_RNDU); mpfr_mul(ratio, ratio, radius, MPFR_RNDU);
        mpfr_ui_sub(denominator, 1u, ratio, MPFR_RNDD);
        if (mpfr_sgn(denominator) <= 0) continue;
        mpfr_div(tail, ratio, denominator, MPFR_RNDU); mpfr_mul(tail, tail, coefficient, MPFR_RNDU);
        if (mpfr_lessequal_p(tail, tolerance)) { degree = j; break; }
    }
    mpfr_clears(coefficient, ratio, denominator, tolerance, (mpfr_ptr)0);
    return degree;
}

static void shifted_polynomial(mpc_ptr value, mpc_ptr derivative, mpc_srcptr x,
                                const shifted_coefficients_t *c, unsigned long degree, mpfr_srcptr radius)
{
    mpfr_prec_t work = c->precision;
    mpfr_t logarithm, exponent, scratch[4];
    mpfr_inits2(64, logarithm, exponent, (mpfr_ptr)0);
    for (unsigned i = 0; i < 4; ++i) mpfr_init2(scratch[i], work);
    mpfr_log2(logarithm, radius, MPFR_RNDU);
    mpc_t argument, term;
    mpc_init2(argument, work); mpc_init2(term, work);
    mpc_set_ui(value, 0u, MPC_RNDNN);
    if (derivative) mpc_set_ui(derivative, 0u, MPC_RNDNN);
    for (unsigned long j = degree + 1; j-- > 0;) {
        /* 2*j<9728, |s|<=16: |q_(2j)|+|q'_(2j)| <= (2*j+17)^17 < 2^240. Moments add at most five bits.
         * The 320-bit allowance keeps shortened weighted absolute errors below 2^(-work-64). */
        mpfr_mul_ui(exponent, logarithm, j, MPFR_RNDU);
        long adjustment = mpfr_get_si(exponent, MPFR_RNDU) + 320;
        mpfr_prec_t precision = adjustment < 0 ? (adjustment < 128 - work ? 128 : work + adjustment) : work;
        mpfr_prec_round(mpc_realref(value), precision, MPFR_RNDN);
        mpfr_prec_round(mpc_imagref(value), precision, MPFR_RNDN);
        if (derivative) {
            mpfr_prec_round(mpc_realref(derivative), precision, MPFR_RNDN);
            mpfr_prec_round(mpc_imagref(derivative), precision, MPFR_RNDN);
        }
        if (mpc_get_prec(term) != precision) { mpc_set_prec(term, precision); mpc_set_prec(argument, precision); }
        for (unsigned i = 0; i < 4; ++i)
            if (mpfr_get_prec(scratch[i]) != precision) mpfr_set_prec(scratch[i], precision);
        mpc_set(argument, x, MPC_RNDNN);
        number_mpc_mul_guarded(value, value, argument, scratch);
        mpc_set(term, c->values[j], MPC_RNDNN); mpc_add(value, value, term, MPC_RNDNN);
        if (derivative) {
            number_mpc_mul_guarded(derivative, derivative, argument, scratch);
            mpc_set(term, c->derivatives[j], MPC_RNDNN); mpc_add(derivative, derivative, term, MPC_RNDNN);
        }
    }
    mpc_clear(term); mpc_clear(argument);
    for (unsigned i = 0; i < 4; ++i) mpfr_clear(scratch[i]);
    mpfr_clears(logarithm, exponent, (mpfr_ptr)0);
}

/* Attempt the finite sum without changing the surrounding Euler–Maclaurin continuation. */
bool number_zeta_shifted_sum(mpc_ptr out, mpc_ptr out_derivative, mpc_srcptr s, mpc_srcptr a, unsigned long count)
{
    mpfr_prec_t precision = mpc_get_prec(out), work = precision + 384;
    if (precision < 1024 || precision > 8320 || count < 64 || count > 4096 ||
        mpfr_cmp_d(mpc_realref(a), 0.25) < 0) return false;
    mpfr_t radius, order, size, logarithm_size, norm, absolute_sum, error, tail, bound, temporary;
    mpfr_inits2(64, radius, order, size, logarithm_size, norm, absolute_sum, error, tail, bound, temporary,
                 (mpfr_ptr)0);
    mpc_abs(order, s, MPFR_RNDU); mpc_abs(size, a, MPFR_RNDU);
    bool ok = mpfr_number_p(order) && mpfr_number_p(size) && mpfr_cmp_ui(order, 16u) <= 0 &&
              mpfr_cmp_ui(size, 16u) <= 0;
    if (!ok) goto bounds_done;
    if (shifted_copy(out, out_derivative, s, a, count)) goto bounds_done;
    shifted_coefficients_t coefficients;
    if (!shifted_coefficients_init(&coefficients, work, out_derivative != NULL)) { ok = false; goto bounds_done; }
    mpc_t sum, derivative, centre, anchor, anchor_offset, logarithm, polynomial, polynomial_slope, x, term;
    mpc_init2(sum, work); mpc_init2(derivative, work); mpc_init2(centre, work); mpc_init2(anchor, work);
    mpc_init2(logarithm, work); mpc_init2(polynomial, work); mpc_init2(polynomial_slope, work);
    mpc_init2(x, work); mpc_init2(term, work);
    mpc_init2(anchor_offset, work);
    mpc_set(anchor_offset, a, MPC_RNDNN);
    mpfr_add_d(mpc_realref(anchor_offset), mpc_realref(anchor_offset), 0.5, MPFR_RNDN);
    mpc_set_ui(sum, 0u, MPC_RNDNN); mpc_set_ui(derivative, 0u, MPC_RNDNN);
    mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(error, 1);
    unsigned long maximum_degree = 0;
    for (unsigned long k = 0; ok && k < count;) {
        if (k < ZETA_BLOCK || count - k < ZETA_BLOCK) {
            number_dirichlet_power(term, a, s, k);
            mpc_add(sum, sum, term, MPC_RNDNN); number_mpc_abs_upper(norm, term);
            mpfr_add(absolute_sum, absolute_sum, norm, MPFR_RNDU);
            if (out_derivative) {
                number_zeta_logarithm(logarithm, a, k);
                mpc_mul(term, term, logarithm, MPC_RNDNN); mpc_sub(derivative, derivative, term, MPC_RNDNN);
                number_mpc_abs_upper(norm, term); mpfr_add(absolute_sum, absolute_sum, norm, MPFR_RNDU);
            }
            ++k;
            continue;
        }
        mpc_add_ui(centre, a, k + ZETA_HALF, MPC_RNDNN);
        mpfr_sub_d(mpc_realref(centre), mpc_realref(centre), 0.5, MPFR_RNDN);
        mpc_abs(size, centre, MPFR_RNDD); mpfr_ui_div(radius, ZETA_HALF, size, MPFR_RNDU);
        if (mpfr_cmp_d(radius, 0.375) > 0) { ok = false; break; }
        unsigned long degree = shifted_degree(tail, radius, order, work);
        if (!degree || !shifted_coefficients(&coefficients, s, degree / 2)) { ok = false; break; }
        if (degree > maximum_degree) maximum_degree = degree;
        mpc_ui_div(x, ZETA_HALF, centre, MPC_RNDNN); mpc_neg(x, x, MPC_RNDNN);
        mpc_sqr(x, x, MPC_RNDNN);
        mpfr_sqr(temporary, radius, MPFR_RNDU);
        shifted_polynomial(polynomial, out_derivative ? polynomial_slope : NULL, x, &coefficients, degree / 2, temporary);
        number_dirichlet_power(anchor, anchor_offset, s, k + ZETA_HALF - 1);
        number_zeta_logarithm(logarithm, anchor_offset, k + ZETA_HALF - 1);
        number_mpc_abs_upper(norm, anchor); number_mpc_abs_upper(logarithm_size, logarithm);
        mpfr_add_ui(logarithm_size, logarithm_size, 1u, MPFR_RNDU);
        mpfr_mul(norm, norm, logarithm_size, MPFR_RNDU);
        mpfr_mul(tail, tail, norm, MPFR_RNDU); mpfr_add(error, error, tail, MPFR_RNDU);
        mpfr_ui_sub(temporary, 1u, radius, MPFR_RNDD);
        mpfr_add_ui(size, order, 1u, MPFR_RNDU); mpfr_neg(size, size, MPFR_RNDN);
        mpfr_pow(temporary, temporary, size, MPFR_RNDU);
        mpfr_mul(norm, norm, temporary, MPFR_RNDU); mpfr_mul_ui(norm, norm, ZETA_BLOCK, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, norm, MPFR_RNDU);
        mpc_mul(term, anchor, polynomial, MPC_RNDNN); mpc_add(sum, sum, term, MPC_RNDNN);
        if (out_derivative) {
            mpc_mul(term, logarithm, polynomial, MPC_RNDNN);
            mpc_sub(term, polynomial_slope, term, MPC_RNDNN);
            mpc_mul(term, anchor, term, MPC_RNDNN); mpc_add(derivative, derivative, term, MPC_RNDNN);
        }
        k += ZETA_BLOCK;
    }
    mpfr_mul_ui(bound, absolute_sum, maximum_degree + 1, MPFR_RNDU);
    mpfr_mul_ui(bound, bound, maximum_degree + 1, MPFR_RNDU);
    mpfr_mul_ui(bound, bound, count + 1, MPFR_RNDU); mpfr_mul_ui(bound, bound, count + 1, MPFR_RNDU);
    /* Quadratic counts cover coefficient/moment recurrences, Horner errors, input copies and accumulation.
     * All full-width state has 384 extra bits; the shortened products have a separate 320-bit growth allowance. */
    mpfr_mul_2si(bound, bound, -work + 96, MPFR_RNDU); mpfr_add(error, error, bound, MPFR_RNDU);
    mpfr_set_ui_2exp(bound, 1u, -precision - 32, MPFR_RNDD);
    ok = ok && mpfr_number_p(error) && mpfr_lessequal_p(error, bound) &&
        mpfr_number_p(mpc_realref(sum)) && mpfr_number_p(mpc_imagref(sum)) &&
        (!out_derivative || (mpfr_number_p(mpc_realref(derivative)) && mpfr_number_p(mpc_imagref(derivative))));
    if (ok) {
        mpc_set(out, sum, MPC_RNDNN);
        if (out_derivative) mpc_set(out_derivative, derivative, MPC_RNDNN);
        shifted_save(out, out_derivative, s, a, count);
    }
    mpc_clear(term); mpc_clear(x); mpc_clear(polynomial_slope); mpc_clear(polynomial);
    mpc_clear(logarithm); mpc_clear(anchor_offset); mpc_clear(anchor); mpc_clear(centre); mpc_clear(derivative); mpc_clear(sum);
    shifted_coefficients_clear(&coefficients);
bounds_done:
    mpfr_clears(radius, order, size, logarithm_size, norm, absolute_sum, error, tail, bound, temporary, (mpfr_ptr)0);
    return ok;
}

static void __attribute__((destructor)) shifted_shutdown(void)
{
    if (!shifted_precision) return;
    mpc_clear(shifted_argument); mpc_clear(shifted_offset); mpc_clear(shifted_value); mpc_clear(shifted_slope);
}
