/**
 * @file number_lommel.c
 * @brief Principal complex Lommel s and its argument derivative with guarded convergence.
 *
 * Implements the defining power series at the participating MPC precision. Terms and their differentiated
 * counterparts share the same recurrence, preserving branch conventions and avoiding finite differences.
 * Real-only legacy paths remain in number_maths.c; complex double-double adapters round this guarded kernel.
 */
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static bool lommel_finite(mpc_srcptr z)
{
    return mpfr_number_p(mpc_realref(z)) && mpfr_number_p(mpc_imagref(z));
}

static bool lommel_zero(mpc_srcptr z)
{
    return mpfr_zero_p(mpc_realref(z)) && mpfr_zero_p(mpc_imagref(z));
}

static int lommel_series(mpc_ptr out, mpc_srcptr mu, mpc_srcptr nu, mpc_srcptr argument,
                         mpc_rnd_t rounding, bool derivative)
{
    mpfr_prec_t target = mpc_get_prec(out);
    if (!lommel_finite(mu) || !lommel_finite(nu) || !lommel_finite(argument)) { mpc_set_nan(out); return -1; }
    double magnitude = hypot(mpfr_get_d(mpc_realref(argument), MPFR_RNDN),
                             mpfr_get_d(mpc_imagref(argument), MPFR_RNDN));
    if (!isfinite(magnitude) || magnitude > 1000) { mpc_set_nan(out); return -1; }
    mpfr_prec_t work = target + 96 + (mpfr_prec_t)ceil(3 * magnitude);
    if (work > 65536) { mpc_set_nan(out); return -1; }
    mpc_t z, square, exponent, nu_square, denominator, term, contribution, sum;
    mpc_init2(z, work); mpc_init2(square, work); mpc_init2(exponent, work); mpc_init2(nu_square, work);
    mpc_init2(denominator, work); mpc_init2(term, work); mpc_init2(contribution, work); mpc_init2(sum, work);
    mpfr_t size, absolute_sum, threshold, tolerance, tail_start;
    mpfr_inits2(work, size, absolute_sum, threshold, tolerance, tail_start, (mpfr_ptr)0);
    mpc_abs(tail_start, nu, MPFR_RNDU);
    mpc_abs(size, argument, MPFR_RNDU);
    mpfr_add(tail_start, tail_start, size, MPFR_RNDU);
    mpfr_add_ui(tail_start, tail_start, 4u, MPFR_RNDU);
    bool ok = false;
    mpc_set(z, argument, MPC_RNDNN);
    if (mpfr_zero_p(mpc_imagref(z))) mpfr_set_zero(mpc_imagref(z), 1);
    mpc_sqr(square, z, MPC_RNDNN);
    mpc_sqr(nu_square, nu, MPC_RNDNN);
    mpc_add_ui(exponent, mu, 1u, MPC_RNDNN);
    mpc_sqr(denominator, exponent, MPC_RNDNN);
    mpc_sub(denominator, denominator, nu_square, MPC_RNDNN);
    if (lommel_zero(denominator)) goto done;
    if (lommel_zero(z)) {
        if (derivative && mpc_cmp_si_si(mu, -1, 0) == 0) {
            mpc_set_ui(sum, 0u, MPC_RNDNN);
            ok = true;
            goto done;
        }
        mpfr_srcptr power = derivative ? mpc_realref(mu) : mpc_realref(exponent);
        if (mpfr_sgn(power) > 0) { mpc_set_ui(sum, 0u, MPC_RNDNN); ok = true; goto done; }
        if (!mpfr_zero_p(power) || !mpfr_zero_p(mpc_imagref(mu))) goto done;
        mpc_ui_div(sum, 1u, denominator, MPC_RNDNN);
        ok = true;
        goto done;
    }
    mpc_pow(term, z, exponent, MPC_RNDNN);
    mpc_div(term, term, denominator, MPC_RNDNN);
    if (derivative) {
        mpc_mul(contribution, term, exponent, MPC_RNDNN);
        mpc_div(contribution, contribution, z, MPC_RNDNN);
    } else mpc_set(contribution, term, MPC_RNDNN);
    mpc_set(sum, contribution, MPC_RNDNN);
    mpc_abs(absolute_sum, contribution, MPFR_RNDU);
    mpfr_set_ui_2exp(tolerance, 1u, -target - 32, MPFR_RNDN);
    for (unsigned long k = 1; k <= 100000; ++k) {
        mpc_add_ui(exponent, mu, 2 * k + 1, MPC_RNDNN);
        mpc_sqr(denominator, exponent, MPC_RNDNN);
        mpc_sub(denominator, denominator, nu_square, MPC_RNDNN);
        if (lommel_zero(denominator)) goto done;
        mpc_mul(term, term, square, MPC_RNDNN);
        mpc_div(term, term, denominator, MPC_RNDNN);
        mpc_neg(term, term, MPC_RNDNN);
        if (derivative) {
            mpc_mul(contribution, term, exponent, MPC_RNDNN);
            mpc_div(contribution, contribution, z, MPC_RNDNN);
        } else mpc_set(contribution, term, MPC_RNDNN);
        mpc_add(sum, sum, contribution, MPC_RNDNN);
        if (!lommel_finite(sum) || !lommel_finite(contribution)) goto done;
        mpc_abs(size, contribution, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, size, MPFR_RNDU);
        mpc_abs(threshold, sum, MPFR_RNDD);
        mpfr_mul(threshold, threshold, tolerance, MPFR_RNDD);
        /* A differentiated term may vanish before later terms do. Only accept a small tail after
         * the powers have passed both parameter and argument magnitudes and the ratios decrease. */
        if (mpfr_greater_p(mpc_realref(exponent), tail_start) && mpfr_lessequal_p(size, threshold)) {
            ok = true;
            break;
        }
    }
    if (ok) {
        mpfr_mul_2si(size, absolute_sum, -work + 16, MPFR_RNDU);
        mpc_abs(threshold, sum, MPFR_RNDD);
        mpfr_mul_2si(threshold, threshold, -target - 16, MPFR_RNDD);
        ok = mpfr_lessequal_p(size, threshold);
    }
done:
    if (ok) mpc_set(out, sum, rounding);
    else mpc_set_nan(out);
    mpfr_clears(size, absolute_sum, threshold, tolerance, tail_start, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(square); mpc_clear(exponent); mpc_clear(nu_square); mpc_clear(denominator);
    mpc_clear(term); mpc_clear(contribution); mpc_clear(sum);
    return ok ? 0 : -1;
}

/* Evaluate the principal complex Lommel function at the caller's requested precision. */
int number_mpc_lommel_s(mpc_ptr out, mpc_srcptr mu, mpc_srcptr nu, mpc_srcptr argument, mpc_rnd_t rounding)
{
    return lommel_series(out, mu, nu, argument, rounding, false);
}

/* Differentiate each series term without changing the principal power's branch. */
int number_mpc_lommel_s_derivative(mpc_ptr out, mpc_srcptr mu, mpc_srcptr nu, mpc_srcptr argument,
                                   mpc_rnd_t rounding)
{
    return lommel_series(out, mu, nu, argument, rounding, true);
}
