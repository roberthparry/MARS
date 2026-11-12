/**
 * @file number_chi.c
 * @brief Blocked odd-power Legendre chi series with explicit tail and rounding bounds.
 *
 * Rectangular splitting reuses powers within blocks and combines blocks by Horner evaluation, reducing expensive
 * multiplications from one per term to roughly twice the square root of the term count. A geometric tail bound
 * includes the integer-order denominator, so large orders need only a few terms. Outside |z|<0.95 the dispatcher
 * retains polylogarithm continuation. All scratch storage is local and released before return.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static bool chi_tail_small(mpfr_srcptr logarithm, mpfr_srcptr log_gap, unsigned long order,
                            unsigned long count, mpfr_prec_t target)
{
    mpfr_t bound, denominator;
    mpfr_inits2(64, bound, denominator, (mpfr_ptr)0);
    mpfr_set_ui(denominator, 2 * count + 1, MPFR_RNDN);
    mpfr_log2(denominator, denominator, MPFR_RNDD);
    mpfr_mul_ui(denominator, denominator, order, MPFR_RNDD);
    mpfr_mul_ui(bound, logarithm, count, MPFR_RNDU);
    mpfr_sub(bound, bound, denominator, MPFR_RNDU); mpfr_sub(bound, bound, log_gap, MPFR_RNDU);
    bool small = mpfr_cmp_si(bound, -target - 80) <= 0;
    mpfr_clears(bound, denominator, (mpfr_ptr)0);
    return small;
}

/* Use binary64 only to propose a count, then prove the omitted-tail bound with directed MPFR arithmetic. */
static unsigned long chi_term_count(mpfr_srcptr radius, unsigned long order, mpfr_prec_t target)
{
    if (mpfr_zero_p(radius)) return 1;
    double approximate_radius = mpfr_get_d(radius, MPFR_RNDU);
    double approximate_logarithm = log2(approximate_radius);
    double approximate_gap = log2(1.0 - approximate_radius);
    unsigned long lower = 0, upper = 100000;
    while (lower + 1 < upper) {
        unsigned long middle = lower + (upper - lower) / 2;
        double tail = middle * approximate_logarithm - (double)order * log2(2.0 * middle + 1.0) - approximate_gap;
        if (tail <= -(double)target - 80.0) upper = middle;
        else lower = middle;
    }
    mpfr_t logarithm, gap;
    mpfr_inits2(64, logarithm, gap, (mpfr_ptr)0);
    mpfr_log2(logarithm, radius, MPFR_RNDU);
    mpfr_ui_sub(gap, 1u, radius, MPFR_RNDD); mpfr_log2(gap, gap, MPFR_RNDD);
    if (!chi_tail_small(logarithm, gap, order, upper, target)) {
        /* Usually at most one extra term is needed. Underflow in the estimate uses bounded geometric growth. */
        if (upper < 100000) ++upper;
        while (upper < 100000 && !chi_tail_small(logarithm, gap, order, upper, target)) {
            upper = upper > 50000 ? 100000 : 2 * upper;
        }
        if (!chi_tail_small(logarithm, gap, order, upper, target)) upper = 0;
    }
    mpfr_clears(logarithm, gap, (mpfr_ptr)0);
    return upper;
}

static bool chi_roundoff_small(mpfr_srcptr radius, mpfr_srcptr z_size, mpfr_srcptr result_size,
                                mpfr_prec_t target, mpfr_prec_t work)
{
    mpfr_t error, tolerance;
    mpfr_inits2(work, error, tolerance, (mpfr_ptr)0);
    mpfr_ui_sub(error, 1u, radius, MPFR_RNDD);
    mpfr_div(error, z_size, error, MPFR_RNDU); mpfr_mul_2si(error, error, -work + 32, MPFR_RNDU);
    mpfr_mul_2si(tolerance, z_size, -target - 80, MPFR_RNDU); mpfr_add(error, error, tolerance, MPFR_RNDU);
    mpfr_mul_2si(tolerance, result_size, -target - 16, MPFR_RNDD);
    bool ok = mpfr_lessequal_p(error, tolerance);
    mpfr_clears(error, tolerance, (mpfr_ptr)0);
    return ok;
}

/* Sum real blocks with exact machine-word denominators when they fit. */
int number_mpfr_chi_series(mpfr_ptr out, unsigned long order, mpfr_srcptr input)
{
    mpfr_prec_t target = mpfr_get_prec(out), work = target + 128;
    mpfr_t radius, square, step, sum, block, term, denominator, z_size, result_size;
    mpfr_inits2(work, radius, square, step, sum, block, term, denominator, z_size, result_size, (mpfr_ptr)0);
    mpfr_sqr(radius, input, MPFR_RNDU);
    int status = 1;
    if (!mpfr_number_p(radius) || mpfr_cmp_d(radius, 0.9025) >= 0) goto done;
    unsigned long count = chi_term_count(radius, order, target);
    status = -1;
    if (!count) goto done;
    unsigned long width = (unsigned long)sqrt((double)count) + 1;
    mpfr_t *powers = calloc(width, sizeof(*powers));
    if (!powers) goto done;
    mpfr_sqr(square, input, MPFR_RNDN);
    for (unsigned long j = 0; j < width; ++j) {
        mpfr_init2(powers[j], work);
        if (j) mpfr_mul(powers[j], powers[j - 1], square, MPFR_RNDN);
        else mpfr_set_ui(powers[j], 1u, MPFR_RNDN);
    }
    mpfr_mul(step, powers[width - 1], square, MPFR_RNDN); mpfr_set_zero(sum, 1);
    for (unsigned long start = ((count - 1) / width) * width;; start -= width) {
        mpfr_set_zero(block, 1);
        unsigned long end = count - start < width ? count - start : width;
        for (unsigned long j = 0; j < end; ++j) {
            unsigned long base = 2 * (start + j) + 1, small = number_series_integer_power(base, order);
            if (small) mpfr_div_ui(term, powers[j], small, MPFR_RNDN);
            else {
                mpfr_ui_pow_ui(denominator, base, order, MPFR_RNDN);
                mpfr_div(term, powers[j], denominator, MPFR_RNDN);
            }
            mpfr_add(block, block, term, MPFR_RNDN);
        }
        mpfr_mul(sum, sum, step, MPFR_RNDN); mpfr_add(sum, sum, block, MPFR_RNDN);
        if (!start) break;
    }
    mpfr_mul(sum, sum, input, MPFR_RNDN);
    mpfr_abs(z_size, input, MPFR_RNDU); mpfr_abs(result_size, sum, MPFR_RNDD);
    if (chi_roundoff_small(radius, z_size, result_size, target, work)) {
        mpfr_set(out, sum, MPFR_RNDN);
        status = 0;
    }
    for (unsigned long j = 0; j < width; ++j) mpfr_clear(powers[j]);
    free(powers);
done:
    if (status < 0) mpfr_set_nan(out);
    mpfr_clears(radius, square, step, sum, block, term, denominator, z_size, result_size, (mpfr_ptr)0);
    return status;
}

/* Sum complex blocks with the same term plan and absolute error checks as the real series. */
int number_mpc_chi_series(mpc_ptr out, unsigned long order, mpc_srcptr input)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 128;
    mpc_t square, step, sum, block, term;
    mpc_init2(square, work); mpc_init2(step, work); mpc_init2(sum, work);
    mpc_init2(block, work); mpc_init2(term, work);
    mpfr_t radius, denominator, z_size, result_size;
    mpfr_inits2(work, radius, denominator, z_size, result_size, (mpfr_ptr)0);
    mpc_norm(radius, input, MPFR_RNDU);
    int status = 1;
    if (!mpfr_number_p(radius) || mpfr_cmp_d(radius, 0.9025) >= 0) goto done;
    unsigned long count = chi_term_count(radius, order, target);
    status = -1;
    if (!count) goto done;
    unsigned long width = (unsigned long)sqrt((double)count) + 1;
    mpc_t *powers = calloc(width, sizeof(*powers));
    if (!powers) goto done;
    mpc_sqr(square, input, MPC_RNDNN);
    for (unsigned long j = 0; j < width; ++j) {
        mpc_init2(powers[j], work);
        if (j) mpc_mul(powers[j], powers[j - 1], square, MPC_RNDNN);
        else mpc_set_ui(powers[j], 1u, MPC_RNDNN);
    }
    mpc_mul(step, powers[width - 1], square, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
    for (unsigned long start = ((count - 1) / width) * width;; start -= width) {
        mpc_set_ui(block, 0u, MPC_RNDNN);
        unsigned long end = count - start < width ? count - start : width;
        for (unsigned long j = 0; j < end; ++j) {
            unsigned long base = 2 * (start + j) + 1, small = number_series_integer_power(base, order);
            if (small) mpc_div_ui(term, powers[j], small, MPC_RNDNN);
            else {
                mpfr_ui_pow_ui(denominator, base, order, MPFR_RNDN);
                mpc_div_fr(term, powers[j], denominator, MPC_RNDNN);
            }
            mpc_add(block, block, term, MPC_RNDNN);
        }
        mpc_mul(sum, sum, step, MPC_RNDNN); mpc_add(sum, sum, block, MPC_RNDNN);
        if (!start) break;
    }
    mpc_mul(sum, sum, input, MPC_RNDNN);
    number_mpc_abs_upper(z_size, input); number_mpc_abs_lower(result_size, sum);
    if (chi_roundoff_small(radius, z_size, result_size, target, work)) {
        mpc_set(out, sum, MPC_RNDNN);
        status = 0;
    }
    for (unsigned long j = 0; j < width; ++j) mpc_clear(powers[j]);
    free(powers);
done:
    if (status < 0) mpc_set_nan(out);
    mpfr_clears(radius, denominator, z_size, result_size, (mpfr_ptr)0);
    mpc_clear(square); mpc_clear(step); mpc_clear(sum); mpc_clear(block); mpc_clear(term);
    return status;
}
