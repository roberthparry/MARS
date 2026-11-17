/**
 * @file number_polylog_blocks.c
 * @brief Guarded rectangular splitting of integer polylogarithm series.
 *
 * Reuses short powers of the argument within each finite block, replacing most full-width complex products
 * with divisions by integer denominators. Late blocks use fewer relative bits while retaining an explicit
 * absolute roundoff allowance. Directed geometric tails and cancellation checks validate the result before
 * the ordinary polylogarithm implementation accepts it. Storage is local and no result cache is introduced.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { POLYLOG_BLOCK_WIDTH = 32, POLYLOG_BLOCK_MAX_TERMS = 200000 };

/* Attempt the convergent integer-order series without modifying the output on failure. */
bool number_mpc_polylog_blocks(mpc_ptr out, unsigned order, mpc_srcptr input)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 96;
    if (target < 384 || target > 16384 || order < 2 || order > 32 ||
        !mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input))) return false;
    mpfr_t radius, giant_bound, factor_bound, denominator, weight, absolute_sum, rounding, tail, scale, bound;
    mpfr_inits2(64, radius, giant_bound, factor_bound, denominator, weight, absolute_sum, rounding, tail, scale,
                bound, (mpfr_ptr)0);
    mpc_abs(radius, input, MPFR_RNDU);
    if (mpfr_zero_p(radius) || mpfr_cmp_d(radius, 0.9) > 0) {
        mpfr_clears(radius, giant_bound, factor_bound, denominator, weight, absolute_sum, rounding, tail, scale,
                    bound, (mpfr_ptr)0);
        return false;
    }
    mpc_t baby[POLYLOG_BLOCK_WIDTH], giant, factor, local, add, sum, contribution;
    for (unsigned j = 0; j < POLYLOG_BLOCK_WIDTH; ++j) mpc_init2(baby[j], work);
    mpc_init2(giant, work); mpc_init2(factor, work); mpc_init2(local, work);
    mpc_init2(add, work); mpc_init2(sum, work); mpc_init2(contribution, work);
    mpfr_t divisor;
    mpfr_init2(divisor, work);
    mpc_set(baby[0], input, MPC_RNDNN);
    for (unsigned j = 1; j < POLYLOG_BLOCK_WIDTH; ++j)
        mpc_mul(baby[j], baby[j - 1], input, MPC_RNDNN);
    mpc_set(giant, baby[POLYLOG_BLOCK_WIDTH - 1], MPC_RNDNN);
    mpc_set_ui(factor, 1u, MPC_RNDNN); mpc_set_ui(sum, 0u, MPC_RNDNN);
    mpfr_pow_ui(giant_bound, radius, POLYLOG_BLOCK_WIDTH, MPFR_RNDU);
    mpfr_set_ui(factor_bound, 1u, MPFR_RNDN);
    mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(rounding, 1);
    bool ok = false;
    for (unsigned long first = 0; first < POLYLOG_BLOCK_MAX_TERMS; first += POLYLOG_BLOCK_WIDTH) {
        mpfr_prec_t precision = work;
        number_mpc_abs_lower(scale, sum);
        if (!mpfr_zero_p(scale)) {
            mpfr_exp_t difference = mpfr_get_exp(factor_bound) - mpfr_get_exp(scale);
            if (difference < -32) precision = difference < 96 - work ? 128 : work + difference + 32;
        }
        if (mpc_get_prec(local) != precision) {
            mpc_set_prec(local, precision); mpc_set_prec(add, precision); mpfr_set_prec(divisor, precision);
        }
        mpc_set_ui(local, 0u, MPC_RNDNN);
        for (unsigned j = 0; j < POLYLOG_BLOCK_WIDTH; ++j) {
            unsigned long k = first + j + 1u, small = number_series_integer_power(k, order);
            if (small) mpc_div_ui(add, baby[j], small, MPC_RNDNN);
            else {
                mpfr_ui_pow_ui(divisor, k, order, MPFR_RNDN);
                mpc_div_fr(add, baby[j], divisor, MPC_RNDNN);
            }
            mpc_add(local, local, add, MPC_RNDNN);
        }
        mpc_mul(contribution, factor, local, MPC_RNDNN);
        mpc_add(sum, sum, contribution, MPC_RNDNN);
        if (!mpfr_number_p(mpc_realref(sum)) || !mpfr_number_p(mpc_imagref(sum))) break;
        /* The block's absolute sum is at most B*rho/(first+1)^s before its giant power. */
        mpfr_ui_pow_ui(denominator, first + 1u, order, MPFR_RNDD);
        mpfr_mul_ui(weight, radius, POLYLOG_BLOCK_WIDTH, MPFR_RNDU);
        mpfr_div(weight, weight, denominator, MPFR_RNDU);
        mpfr_mul(weight, weight, factor_bound, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, weight, MPFR_RNDU);
        mpfr_mul_2si(bound, weight, -precision + 10, MPFR_RNDU);
        mpfr_add(rounding, rounding, bound, MPFR_RNDU);
        mpc_mul(factor, factor, giant, MPC_RNDNN);
        mpfr_mul(factor_bound, factor_bound, giant_bound, MPFR_RNDU);
        /* The first omitted term bounds the complete remaining tail after division by 1-rho. */
        mpfr_ui_pow_ui(denominator, first + POLYLOG_BLOCK_WIDTH + 1u, order, MPFR_RNDD);
        mpfr_mul(tail, factor_bound, radius, MPFR_RNDU);
        mpfr_div(tail, tail, denominator, MPFR_RNDU);
        mpfr_ui_sub(denominator, 1u, radius, MPFR_RNDD);
        mpfr_div(tail, tail, denominator, MPFR_RNDU);
        number_mpc_abs_lower(scale, sum);
        mpfr_mul_2si(scale, scale, -target - 16, MPFR_RNDD);
        if (mpfr_greater_p(tail, scale)) continue;
        /* At most 200000 terms, including rounded baby powers and giant-power accumulation. */
        mpfr_mul_2si(bound, absolute_sum, -work + 40, MPFR_RNDU);
        mpfr_add(bound, bound, rounding, MPFR_RNDU);
        mpfr_add(bound, bound, tail, MPFR_RNDU);
        if (mpfr_lessequal_p(bound, scale)) { ok = true; mpc_set(out, sum, MPC_RNDNN); }
        break;
    }
    mpfr_clear(divisor);
    mpc_clear(giant); mpc_clear(factor); mpc_clear(local); mpc_clear(add); mpc_clear(sum); mpc_clear(contribution);
    for (unsigned j = 0; j < POLYLOG_BLOCK_WIDTH; ++j) mpc_clear(baby[j]);
    mpfr_clears(radius, giant_bound, factor_bound, denominator, weight, absolute_sum, rounding, tail, scale,
                bound, (mpfr_ptr)0);
    return ok;
}
