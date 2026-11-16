/**
 * @file number_complex_bounds.c
 * @brief Directed magnitude and exponent bounds for native series convergence checks.
 *
 * Supplies max(|real|, |imag|) as a lower bound and |real| + |imag| as an upper bound on the Euclidean modulus.
 * These avoid repeated square roots where a conservative convergence or roundoff bound is sufficient. Mathematical
 * values and actual polar conversions still use their ordinary modulus. Only number-module implementations use
 * these helpers; callers retain their initialised MPFR destinations and all input ownership.
 * Guarded complex multiplication supplies an explicit absolute error allowance for finite series polynomials,
 * allowing three rounded real products without MPC's correctly rounded component contract.
 */
#include <limits.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Use three rounded real products where the owning series provides an absolute error budget. */
void number_mpc_mul_guarded(mpc_ptr out, mpc_srcptr left, mpc_srcptr right, mpfr_t *scratch)
{
    if (mpfr_zero_p(mpc_imagref(left)) && mpfr_zero_p(mpc_imagref(right))) {
        mpfr_mul(mpc_realref(out), mpc_realref(left), mpc_realref(right), MPFR_RNDN);
        mpfr_set_zero(mpc_imagref(out), 1);
        return;
    }
    mpfr_mul(scratch[0], mpc_realref(left), mpc_realref(right), MPFR_RNDN);
    mpfr_mul(scratch[1], mpc_imagref(left), mpc_imagref(right), MPFR_RNDN);
    mpfr_add(scratch[2], mpc_realref(left), mpc_imagref(left), MPFR_RNDN);
    mpfr_add(scratch[3], mpc_realref(right), mpc_imagref(right), MPFR_RNDN);
    mpfr_mul(scratch[2], scratch[2], scratch[3], MPFR_RNDN);
    mpfr_sub(scratch[2], scratch[2], scratch[0], MPFR_RNDN);
    mpfr_sub(mpc_imagref(out), scratch[2], scratch[1], MPFR_RNDN);
    mpfr_sub(mpc_realref(out), scratch[0], scratch[1], MPFR_RNDN);
}

/* Bound the modulus from above without a square root. */
void number_mpc_abs_upper(mpfr_ptr out, mpc_srcptr value)
{
    mpfr_abs(out, mpc_realref(value), MPFR_RNDU);
    if (mpfr_zero_p(mpc_imagref(value))) return;
    if (mpfr_signbit(mpc_imagref(value))) mpfr_sub(out, out, mpc_imagref(value), MPFR_RNDU);
    else mpfr_add(out, out, mpc_imagref(value), MPFR_RNDU);
}

/* Bound the modulus from below, preserving NaN propagation. */
void number_mpc_abs_lower(mpfr_ptr out, mpc_srcptr value)
{
    if (mpfr_zero_p(mpc_imagref(value))) { mpfr_abs(out, mpc_realref(value), MPFR_RNDD); return; }
    if (mpfr_nan_p(mpc_realref(value)) || mpfr_nan_p(mpc_imagref(value))) { mpfr_set_nan(out); return; }
    mpfr_abs(out, mpfr_cmpabs(mpc_realref(value), mpc_imagref(value)) >= 0 ?
                    mpc_realref(value) : mpc_imagref(value), MPFR_RNDD);
}

/* Prove a scaled upper bound small using exponents, without multiplying full-width significands. */
bool number_series_small(mpfr_srcptr term_upper, mpfr_srcptr sum_lower, mpfr_prec_t bits, unsigned factor_bits)
{
    if (bits <= 0 || !mpfr_number_p(term_upper) || !mpfr_number_p(sum_lower) ||
        mpfr_sgn(term_upper) < 0 || mpfr_sgn(sum_lower) < 0) return false;
    if (mpfr_zero_p(term_upper)) return true;
    if (mpfr_zero_p(sum_lower)) return false;
    mpfr_exp_t upper = mpfr_get_exp(term_upper), lower = mpfr_get_exp(sum_lower);
    if (upper >= lower || (unsigned long)bits >= ULONG_MAX - (unsigned long)factor_bits) return false;
    /* Unsigned subtraction also covers the full distance between opposite signed exponents. */
    unsigned long gap = (unsigned long)lower - (unsigned long)upper;
    return gap >= (unsigned long)bits + (unsigned long)factor_bits + 1ul;
}
