/**
 * @file number_chi.c
 * @brief Direct odd-power Legendre chi series for real and complex multiprecision arguments.
 *
 * Within |z| < 0.95, sums z^(2k+1)/(2k+1)^s directly instead of subtracting two complete polylogarithms. A geometric
 * tail bound uses the squared argument modulus and decreasing non-negative integer-order coefficients. Directed
 * magnitude bounds also check cancellation against the working precision. Outside this convergence region the
 * owning dispatcher retains the existing polylogarithm continuation. No fixed-precision path is narrowed.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Sum real odd powers with a geometric bound on the omitted tail. */
int number_mpfr_chi_series(mpfr_ptr out, unsigned long order, mpfr_srcptr input)
{
    mpfr_prec_t target = mpfr_get_prec(out), work = target + 64;
    mpfr_t z, square, power, sum, add, denominator, tail_factor, tail, threshold, absolute_sum;
    mpfr_inits2(work, z, square, power, sum, add, denominator, tail_factor, tail, threshold, absolute_sum,
                (mpfr_ptr)0);
    mpfr_set(z, input, MPFR_RNDN);
    mpfr_sqr(square, z, MPFR_RNDU);
    int status = 1;
    if (!mpfr_number_p(square) || mpfr_cmp_d(square, 0.9025) >= 0) goto done;
    mpfr_ui_sub(denominator, 1u, square, MPFR_RNDD);
    mpfr_div(tail_factor, square, denominator, MPFR_RNDU);
    unsigned tail_bits = mpfr_cmp_ui(tail_factor, 1u) <= 0 ? 0u : (unsigned)mpfr_get_exp(tail_factor);
    mpfr_set(power, z, MPFR_RNDN);
    mpfr_set_zero(sum, 1); mpfr_set_zero(absolute_sum, 1);
    status = -1;
    for (unsigned long k = 0; k < 100000; ++k) {
        unsigned long small = number_series_integer_power(2u * k + 1u, order);
        if (small) mpfr_div_ui(add, power, small, MPFR_RNDN);
        else {
            mpfr_ui_pow_ui(denominator, 2u * k + 1u, order, MPFR_RNDN);
            mpfr_div(add, power, denominator, MPFR_RNDN);
        }
        mpfr_add(sum, sum, add, MPFR_RNDN);
        mpfr_abs(tail, add, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, tail, MPFR_RNDU);
        mpfr_abs(threshold, sum, MPFR_RNDD);
        if (number_series_small(tail, threshold, target + 24, tail_bits)) { status = 0; break; }
        mpfr_mul(power, power, square, MPFR_RNDN);
    }
    mpfr_mul_2si(tail, absolute_sum, -(long)work + 16, MPFR_RNDU);
    mpfr_abs(threshold, sum, MPFR_RNDD);
    mpfr_mul_2si(threshold, threshold, -(long)target - 16, MPFR_RNDD);
    if (status == 0 && mpfr_lessequal_p(tail, threshold)) mpfr_set(out, sum, MPFR_RNDN);
    else { mpfr_set_nan(out); status = -1; }
done:
    mpfr_clears(z, square, power, sum, add, denominator, tail_factor, tail, threshold, absolute_sum, (mpfr_ptr)0);
    return status;
}

/* Sum complex odd powers without duplicate polylogarithm evaluations or per-term square roots. */
int number_mpc_chi_series(mpc_ptr out, unsigned long order, mpc_srcptr input)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 64;
    mpc_t z, square, power, sum, add;
    mpc_init2(z, work); mpc_init2(square, work); mpc_init2(power, work);
    mpc_init2(sum, work); mpc_init2(add, work);
    mpfr_t radius, denominator, tail_factor, tail, threshold, absolute_sum;
    mpfr_inits2(work, radius, denominator, tail_factor, tail, threshold, absolute_sum, (mpfr_ptr)0);
    mpc_set(z, input, MPC_RNDNN);
    mpc_norm(radius, z, MPFR_RNDU);
    int status = 1;
    if (!mpfr_number_p(radius) || mpfr_cmp_d(radius, 0.9025) >= 0) goto done;
    mpfr_ui_sub(denominator, 1u, radius, MPFR_RNDD);
    mpfr_div(tail_factor, radius, denominator, MPFR_RNDU);
    unsigned tail_bits = mpfr_cmp_ui(tail_factor, 1u) <= 0 ? 0u : (unsigned)mpfr_get_exp(tail_factor);
    mpc_sqr(square, z, MPC_RNDNN); mpc_set(power, z, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
    mpfr_set_zero(absolute_sum, 1);
    status = -1;
    for (unsigned long k = 0; k < 100000; ++k) {
        unsigned long small = number_series_integer_power(2u * k + 1u, order);
        if (small) mpc_div_ui(add, power, small, MPC_RNDNN);
        else {
            mpfr_ui_pow_ui(denominator, 2u * k + 1u, order, MPFR_RNDN);
            mpc_div_fr(add, power, denominator, MPC_RNDNN);
        }
        mpc_add(sum, sum, add, MPC_RNDNN);
        number_mpc_abs_upper(tail, add);
        mpfr_add(absolute_sum, absolute_sum, tail, MPFR_RNDU);
        number_mpc_abs_lower(threshold, sum);
        if (number_series_small(tail, threshold, target + 24, tail_bits)) { status = 0; break; }
        mpc_mul(power, power, square, MPC_RNDNN);
    }
    mpfr_mul_2si(tail, absolute_sum, -(long)work + 16, MPFR_RNDU);
    number_mpc_abs_lower(threshold, sum);
    mpfr_mul_2si(threshold, threshold, -(long)target - 16, MPFR_RNDD);
    if (status == 0 && mpfr_lessequal_p(tail, threshold)) mpc_set(out, sum, MPC_RNDNN);
    else { mpc_set_nan(out); status = -1; }
done:
    mpfr_clears(radius, denominator, tail_factor, tail, threshold, absolute_sum, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(square); mpc_clear(power); mpc_clear(sum); mpc_clear(add);
    return status;
}
