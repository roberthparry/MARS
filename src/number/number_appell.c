/**
 * @file number_appell.c
 * @brief Guarded linear-work Appell F1 series for real and complex multiprecision values.
 *
 * Groups the defining double series by total degree. Coefficients of
 * (1-x*t)^(-b1)*(1-y*t)^(-b2) obey a three-term recurrence, avoiding both nested summation and a coefficient
 * array. Input conversion, domain checks and cancellation guards remain native number-module responsibilities.
 * The principal convergent series is supported for |x|, |y| < 0.95, matching the established numerical domain.
 */
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { APPELL_MAX_TERMS = 100000, APPELL_MAX_WORK_BITS = 65536 };

static bool appell_finite(mpc_srcptr value)
{
    return mpfr_number_p(mpc_realref(value)) && mpfr_number_p(mpc_imagref(value));
}

/* Evaluate the total-degree recurrence with explicit roundoff and convergence checks. */
int number_mpc_appell_f1(mpc_ptr out, mpc_srcptr a, mpc_srcptr b1, mpc_srcptr b2, mpc_srcptr c,
                         mpc_srcptr x, mpc_srcptr y, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 96;
    if (work > APPELL_MAX_WORK_BITS) { mpc_set_nan(out); return -1; }
    const mpc_srcptr inputs[] = {a, b1, b2, c, x, y};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i)
        if (!appell_finite(inputs[i])) { mpc_set_nan(out); return -1; }
    mpc_t previous, current, next, ratio, sum, term, scratch, denominator, upper, xy, xy_sum, weighted, b_sum;
    mpc_init2(previous, work); mpc_init2(current, work); mpc_init2(next, work); mpc_init2(ratio, work);
    mpc_init2(sum, work); mpc_init2(term, work); mpc_init2(scratch, work); mpc_init2(denominator, work);
    mpc_init2(upper, work); mpc_init2(xy, work); mpc_init2(xy_sum, work); mpc_init2(weighted, work);
    mpc_init2(b_sum, work);
    mpfr_t magnitude, absolute_sum, threshold, tolerance, radius;
    mpfr_inits2(work, magnitude, absolute_sum, threshold, tolerance, radius, (mpfr_ptr)0);
    bool converged = false;
    mpc_abs(radius, x, MPFR_RNDU);
    if (mpfr_cmp_d(radius, 0.95) >= 0) goto done;
    mpc_abs(radius, y, MPFR_RNDU);
    if (mpfr_cmp_d(radius, 0.95) >= 0) goto done;
    mpc_mul(xy, x, y, MPC_RNDNN);
    mpc_add(xy_sum, x, y, MPC_RNDNN);
    mpc_mul(weighted, b1, x, MPC_RNDNN);
    mpc_mul(scratch, b2, y, MPC_RNDNN);
    mpc_add(weighted, weighted, scratch, MPC_RNDNN);
    mpc_add(b_sum, b1, b2, MPC_RNDNN);
    mpc_set_ui(previous, 0u, MPC_RNDNN);
    mpc_set_ui(current, 1u, MPC_RNDNN);
    mpc_set_ui(ratio, 1u, MPC_RNDNN);
    mpc_set_ui(sum, 1u, MPC_RNDNN);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    mpfr_set_ui_2exp(tolerance, 1u, -target - 32, MPFR_RNDN);
    unsigned small = 0;
    for (unsigned long n = 0; n < APPELL_MAX_TERMS; ++n) {
        mpc_add_ui(upper, a, n, MPC_RNDNN);
        if (mpfr_zero_p(mpc_realref(upper)) && mpfr_zero_p(mpc_imagref(upper))) {
            converged = true;
            break;
        }
        mpc_add_ui(denominator, c, n, MPC_RNDNN);
        if (mpfr_zero_p(mpc_realref(denominator)) && mpfr_zero_p(mpc_imagref(denominator))) goto done;
        mpc_div(scratch, upper, denominator, MPC_RNDNN);
        mpc_mul(ratio, ratio, scratch, MPC_RNDNN);
        mpc_mul_ui(scratch, xy_sum, n, MPC_RNDNN);
        mpc_add(scratch, scratch, weighted, MPC_RNDNN);
        mpc_mul(next, scratch, current, MPC_RNDNN);
        mpc_add_si(scratch, b_sum, (long)n - 1, MPC_RNDNN);
        mpc_mul(scratch, scratch, xy, MPC_RNDNN);
        mpc_mul(scratch, scratch, previous, MPC_RNDNN);
        mpc_sub(next, next, scratch, MPC_RNDNN);
        mpc_div_ui(next, next, n + 1, MPC_RNDNN);
        mpc_mul(term, ratio, next, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (!appell_finite(sum) || !appell_finite(term)) goto done;
        mpc_abs(magnitude, term, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        mpc_abs(threshold, sum, MPFR_RNDD);
        mpfr_mul(threshold, threshold, tolerance, MPFR_RNDD);
        small = mpfr_lessequal_p(magnitude, threshold) ? small + 1 : 0;
        if (small >= 8) { converged = true; break; }
        mpc_swap(previous, current);
        mpc_swap(current, next);
    }
    if (converged) {
        mpfr_mul_2si(magnitude, absolute_sum, -work + 16, MPFR_RNDU);
        mpc_abs(threshold, sum, MPFR_RNDD);
        mpfr_mul_2si(threshold, threshold, -target - 16, MPFR_RNDD);
        converged = mpfr_lessequal_p(magnitude, threshold);
    }
done:
    if (converged) mpc_set(out, sum, rounding);
    else mpc_set_nan(out);
    mpfr_clears(magnitude, absolute_sum, threshold, tolerance, radius, (mpfr_ptr)0);
    mpc_clear(previous); mpc_clear(current); mpc_clear(next); mpc_clear(ratio); mpc_clear(sum); mpc_clear(term);
    mpc_clear(scratch); mpc_clear(denominator); mpc_clear(upper); mpc_clear(xy); mpc_clear(xy_sum);
    mpc_clear(weighted); mpc_clear(b_sum);
    return converged ? 0 : -1;
}

/* Use the same recurrence for real operands without reducing their precision. */
int number_mpfr_appell_f1(mpfr_ptr out, mpfr_srcptr a, mpfr_srcptr b1, mpfr_srcptr b2, mpfr_srcptr c,
                          mpfr_srcptr x, mpfr_srcptr y)
{
    mpfr_prec_t precision = mpfr_get_prec(out);
    const mpfr_srcptr inputs[] = {a, b1, b2, c, x, y};
    mpc_t values[6], result;
    mpc_init2(result, precision);
    for (size_t i = 0; i < 6; ++i) {
        mpc_init2(values[i], precision);
        mpc_set_fr(values[i], inputs[i], MPC_RNDNN);
    }
    int status = number_mpc_appell_f1(result, values[0], values[1], values[2], values[3], values[4], values[5], MPC_RNDNN);
    if (!status && mpfr_zero_p(mpc_imagref(result))) mpfr_set(out, mpc_realref(result), MPFR_RNDN);
    else { mpfr_set_nan(out); status = -1; }
    for (size_t i = 0; i < 6; ++i) mpc_clear(values[i]);
    mpc_clear(result);
    return status;
}
