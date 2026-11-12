/**
 * @file number_appell.c
 * @brief Guarded linear-work Appell F1 series for real and complex multiprecision values.
 *
 * Groups the defining double series by total degree. Coefficients of
 * (1-x*t)^(-b1)*(1-y*t)^(-b2) obey a three-term recurrence, avoiding both nested summation and a coefficient
 * array. Linear recurrence factors advance by addition between periodic refreshes. Shifted parameters retain
 * their exact binary data without unnecessary padding; compact complex parameters admit an exact quadratic
 * ratio recurrence with a real denominator. Real inputs use MPFR directly, without complex temporaries.
 * Domain checks and cancellation guards remain native number-module responsibilities.
 * The principal convergent series is supported for |x|, |y| < 0.95, matching the established numerical domain.
 */
#include <limits.h>
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { APPELL_MAX_TERMS = 100000, APPELL_MAX_WORK_BITS = 65536 };

/* Retain every input bit and every integer shift, without padding simple dyadic parameters to full precision. */
static mpfr_prec_t appell_shift_precision(mpfr_srcptr input, mpfr_prec_t work)
{
    const mpfr_exp_t shift_bits = sizeof(unsigned) * CHAR_BIT - __builtin_clz((unsigned)APPELL_MAX_TERMS);
    if (mpfr_zero_p(input)) return shift_bits + 1;
    mpfr_prec_t significant = mpfr_min_prec(input);
    mpfr_exp_t exponent = mpfr_get_exp(input);
    if (significant > work || exponent > work || exponent < -work) return work;
    mpfr_exp_t lower = exponent - significant;
    mpfr_exp_t upper = exponent > shift_bits ? exponent : shift_bits;
    mpfr_prec_t needed = upper + 1 - (lower < 0 ? lower : 0);
    return needed < work ? needed : work;
}

/* Bound a common binary scale for both components and every permitted integer shift. */
static mpfr_prec_t appell_complex_shift_precision(mpc_srcptr input, mpfr_prec_t work)
{
    const mpfr_srcptr components[] = {mpc_realref(input), mpc_imagref(input)};
    mpfr_exp_t upper = sizeof(unsigned) * CHAR_BIT - __builtin_clz((unsigned)APPELL_MAX_TERMS), lower = 0;
    for (size_t i = 0; i < 2; ++i) {
        if (mpfr_zero_p(components[i])) continue;
        mpfr_prec_t significant = mpfr_min_prec(components[i]);
        mpfr_exp_t exponent = mpfr_get_exp(components[i]);
        if (significant > work || exponent > work || exponent < -work) return work;
        if (exponent > upper) upper = exponent;
        if (exponent - significant < lower) lower = exponent - significant;
    }
    mpfr_prec_t needed = upper + 1 - lower;
    return needed < work ? needed : work;
}

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
    mpc_t forward, backward, numerator, numerator_step;
    mpc_init2(previous, work); mpc_init2(current, work); mpc_init2(next, work); mpc_init2(ratio, work);
    mpc_init2(sum, work); mpc_init2(term, work); mpc_init2(scratch, work); mpc_init2(denominator, work);
    mpc_init2(upper, work); mpc_init2(xy, work); mpc_init2(xy_sum, work); mpc_init2(weighted, work);
    mpc_init2(b_sum, work);
    mpc_init2(forward, work); mpc_init2(backward, work);
    mpfr_prec_t ar = appell_shift_precision(mpc_realref(a), work);
    mpfr_prec_t ai = appell_shift_precision(mpc_imagref(a), work);
    mpfr_prec_t cr = appell_shift_precision(mpc_realref(c), work);
    mpfr_prec_t ci = appell_shift_precision(mpc_imagref(c), work);
    mpc_set_prec(upper, ar > ai ? ar : ai);
    mpc_set_prec(denominator, cr > ci ? cr : ci);
    mpfr_prec_t ap = appell_complex_shift_precision(a, work), cp = appell_complex_shift_precision(c, work);
    bool exact_ratio = ap + cp + 1 <= work && 2 * cp + 1 <= work;
    mpc_init2(numerator, exact_ratio ? ap + cp + 1 : MPFR_PREC_MIN);
    mpc_init2(numerator_step, exact_ratio ? ap + cp + 1 : MPFR_PREC_MIN);
    mpfr_t magnitude, absolute_sum, threshold, radius, norm, norm_step;
    mpfr_inits2(work, magnitude, absolute_sum, threshold, radius, (mpfr_ptr)0);
    mpfr_inits2(exact_ratio ? 2 * cp + 1 : MPFR_PREC_MIN, norm, norm_step, (mpfr_ptr)0);
    bool converged = false;
    mpc_abs(radius, x, MPFR_RNDU);
    if (mpfr_cmp_d(radius, 0.95) >= 0) goto done;
    if (exact_ratio) {
        /* P_n = (a+n)*conj(c+n), R_n = |c+n|^2; both quadratic recurrences remain exact. */
        mpc_conj(scratch, c, MPC_RNDNN);
        mpc_mul(numerator, a, scratch, MPC_RNDNN);
        mpc_add(numerator_step, a, scratch, MPC_RNDNN);
        mpc_add_ui(numerator_step, numerator_step, 1u, MPC_RNDNN);
        mpc_norm(norm, c, MPFR_RNDN);
        mpfr_mul_2ui(norm_step, mpc_realref(c), 1u, MPFR_RNDN);
        mpfr_add_ui(norm_step, norm_step, 1u, MPFR_RNDN);
    }
    mpc_abs(radius, y, MPFR_RNDU);
    if (mpfr_cmp_d(radius, 0.95) >= 0) goto done;
    if (!mpc_cmp(a, c) && !(mpfr_zero_p(mpc_imagref(c)) &&
        mpfr_integer_p(mpc_realref(c)) && mpfr_sgn(mpc_realref(c)) <= 0) &&
        !mpc_ui_sub(scratch, 1u, x, MPC_RNDNN) && !mpc_ui_sub(xy, 1u, y, MPC_RNDNN)) {
        /* F1(a,b1,b2;a;x,y) = (1-x)^(-b1) (1-y)^(-b2). */
        mpc_neg(next, b1, MPC_RNDNN); mpc_pow(sum, scratch, next, MPC_RNDNN);
        mpc_neg(next, b2, MPC_RNDNN); mpc_pow(scratch, xy, next, MPC_RNDNN);
        mpc_mul(sum, sum, scratch, MPC_RNDNN);
        converged = appell_finite(sum);
        goto done;
    }
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
    unsigned small = 0;
    for (unsigned long n = 0; n < APPELL_MAX_TERMS; ++n) {
        mpc_add_ui(upper, a, n, MPC_RNDNN);
        if (mpfr_zero_p(mpc_realref(upper)) && mpfr_zero_p(mpc_imagref(upper))) {
            converged = true;
            break;
        }
        if (exact_ratio) {
            if (mpfr_zero_p(norm)) goto done;
            mpc_mul(ratio, ratio, numerator, MPC_RNDNN);
            mpc_div_fr(ratio, ratio, norm, MPC_RNDNN);
        } else {
            mpc_add_ui(denominator, c, n, MPC_RNDNN);
            if (mpfr_zero_p(mpc_realref(denominator)) && mpfr_zero_p(mpc_imagref(denominator))) goto done;
            mpc_div(scratch, upper, denominator, MPC_RNDNN);
            mpc_mul(ratio, ratio, scratch, MPC_RNDNN);
        }
        if (!(n & 63ul)) {
            mpc_mul_ui(forward, xy_sum, n, MPC_RNDNN);
            mpc_add(forward, forward, weighted, MPC_RNDNN);
            mpc_add_si(backward, b_sum, (long)n - 1, MPC_RNDNN);
            mpc_mul(backward, backward, xy, MPC_RNDNN);
        }
        mpc_mul(next, forward, current, MPC_RNDNN);
        mpc_mul(scratch, backward, previous, MPC_RNDNN);
        mpc_sub(next, next, scratch, MPC_RNDNN);
        mpc_div_ui(next, next, n + 1, MPC_RNDNN);
        mpc_mul(term, ratio, next, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (!appell_finite(sum) || !appell_finite(term)) goto done;
        number_mpc_abs_upper(magnitude, term);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        number_mpc_abs_lower(threshold, sum);
        small = number_series_small(magnitude, threshold, target + 32, 0u) ? small + 1 : 0;
        if (small >= 8) { converged = true; break; }
        mpc_swap(previous, current);
        mpc_swap(current, next);
        mpc_add(forward, forward, xy_sum, MPC_RNDNN);
        mpc_add(backward, backward, xy, MPC_RNDNN);
        if (exact_ratio) {
            mpc_add(numerator, numerator, numerator_step, MPC_RNDNN);
            mpc_add_ui(numerator_step, numerator_step, 2u, MPC_RNDNN);
            mpfr_add(norm, norm, norm_step, MPFR_RNDN);
            mpfr_add_ui(norm_step, norm_step, 2u, MPFR_RNDN);
        }
    }
    if (converged) {
        mpfr_mul_2si(magnitude, absolute_sum, -work + 24, MPFR_RNDU);
        number_mpc_abs_lower(threshold, sum);
        mpfr_mul_2si(threshold, threshold, -target - 16, MPFR_RNDD);
        converged = mpfr_lessequal_p(magnitude, threshold);
    }
done:
    if (converged) mpc_set(out, sum, rounding);
    else mpc_set_nan(out);
    mpfr_clears(magnitude, absolute_sum, threshold, radius, norm, norm_step, (mpfr_ptr)0);
    mpc_clear(previous); mpc_clear(current); mpc_clear(next); mpc_clear(ratio); mpc_clear(sum); mpc_clear(term);
    mpc_clear(scratch); mpc_clear(denominator); mpc_clear(upper); mpc_clear(xy); mpc_clear(xy_sum);
    mpc_clear(weighted); mpc_clear(b_sum);
    mpc_clear(forward); mpc_clear(backward);
    mpc_clear(numerator); mpc_clear(numerator_step);
    return converged ? 0 : -1;
}

/* Evaluate real operands directly, retaining the complex kernel's convergence and cancellation guards. */
int number_mpfr_appell_f1(mpfr_ptr out, mpfr_srcptr a, mpfr_srcptr b1, mpfr_srcptr b2, mpfr_srcptr c,
                          mpfr_srcptr x, mpfr_srcptr y)
{
    mpfr_prec_t target = mpfr_get_prec(out), work = target + 96;
    if (work > APPELL_MAX_WORK_BITS) { mpfr_set_nan(out); return -1; }
    const mpfr_srcptr inputs[] = {a, b1, b2, c, x, y};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i)
        if (!mpfr_number_p(inputs[i])) { mpfr_set_nan(out); return -1; }
    mpfr_t previous, current, next, ratio, sum, term, scratch, denominator, upper;
    mpfr_t xy, xy_sum, weighted, b_sum, forward, backward;
    mpfr_t magnitude, absolute_sum, threshold, cutoff;
    mpfr_inits2(work, previous, current, next, ratio, sum, term, scratch, denominator, upper,
                xy, xy_sum, weighted, b_sum, forward, backward, magnitude, absolute_sum, threshold,
                cutoff, (mpfr_ptr)0);
    mpfr_set_prec(upper, appell_shift_precision(a, work));
    mpfr_set_prec(denominator, appell_shift_precision(c, work));
    bool compact_ratio = mpfr_get_prec(upper) + mpfr_get_prec(denominator) + 1 <= work;
    bool converged = false;
    mpfr_set_d(cutoff, 0.95, MPFR_RNDN);
    if (mpfr_cmpabs(x, cutoff) >= 0 || mpfr_cmpabs(y, cutoff) >= 0) goto done;
    if (mpfr_equal_p(a, c) && !(mpfr_integer_p(c) && mpfr_sgn(c) <= 0) &&
        !mpfr_ui_sub(scratch, 1u, x, MPFR_RNDN) && !mpfr_ui_sub(xy, 1u, y, MPFR_RNDN)) {
        mpfr_neg(next, b1, MPFR_RNDN); mpfr_pow(sum, scratch, next, MPFR_RNDN);
        mpfr_neg(next, b2, MPFR_RNDN); mpfr_pow(scratch, xy, next, MPFR_RNDN);
        mpfr_mul(sum, sum, scratch, MPFR_RNDN);
        converged = mpfr_number_p(sum);
        goto done;
    }
    mpfr_mul(xy, x, y, MPFR_RNDN);
    mpfr_add(xy_sum, x, y, MPFR_RNDN);
    mpfr_mul(weighted, b1, x, MPFR_RNDN);
    mpfr_mul(scratch, b2, y, MPFR_RNDN);
    mpfr_add(weighted, weighted, scratch, MPFR_RNDN);
    mpfr_add(b_sum, b1, b2, MPFR_RNDN);
    mpfr_set_zero(previous, 1);
    mpfr_set_ui(current, 1u, MPFR_RNDN);
    mpfr_set_ui(ratio, 1u, MPFR_RNDN);
    mpfr_set_ui(sum, 1u, MPFR_RNDN);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    unsigned small = 0;
    for (unsigned long n = 0; n < APPELL_MAX_TERMS; ++n) {
        mpfr_add_ui(upper, a, n, MPFR_RNDN);
        if (mpfr_zero_p(upper)) { converged = true; break; }
        mpfr_add_ui(denominator, c, n, MPFR_RNDN);
        if (mpfr_zero_p(denominator)) goto done;
        if (compact_ratio) {
            mpfr_mul(ratio, ratio, upper, MPFR_RNDN);
            mpfr_div(ratio, ratio, denominator, MPFR_RNDN);
        } else {
            mpfr_div(scratch, upper, denominator, MPFR_RNDN);
            mpfr_mul(ratio, ratio, scratch, MPFR_RNDN);
        }
        if (!(n & 63ul)) {
            mpfr_mul_ui(forward, xy_sum, n, MPFR_RNDN);
            mpfr_add(forward, forward, weighted, MPFR_RNDN);
            mpfr_add_si(backward, b_sum, (long)n - 1, MPFR_RNDN);
            mpfr_mul(backward, backward, xy, MPFR_RNDN);
        }
        mpfr_mul(next, forward, current, MPFR_RNDN);
        mpfr_mul(scratch, backward, previous, MPFR_RNDN);
        mpfr_sub(next, next, scratch, MPFR_RNDN);
        mpfr_div_ui(next, next, n + 1, MPFR_RNDN);
        mpfr_mul(term, ratio, next, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        if (!mpfr_number_p(sum) || !mpfr_number_p(term)) goto done;
        mpfr_abs(magnitude, term, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        mpfr_abs(threshold, sum, MPFR_RNDD);
        small = number_series_small(magnitude, threshold, target + 32, 0u) ? small + 1 : 0;
        if (small >= 8) { converged = true; break; }
        mpfr_swap(previous, current);
        mpfr_swap(current, next);
        mpfr_add(forward, forward, xy_sum, MPFR_RNDN);
        mpfr_add(backward, backward, xy, MPFR_RNDN);
    }
    if (converged) {
        mpfr_mul_2si(magnitude, absolute_sum, -work + 24, MPFR_RNDU);
        mpfr_abs(threshold, sum, MPFR_RNDD);
        mpfr_mul_2si(threshold, threshold, -target - 16, MPFR_RNDD);
        converged = mpfr_lessequal_p(magnitude, threshold);
    }
done:
    if (converged) mpfr_set(out, sum, MPFR_RNDN);
    else mpfr_set_nan(out);
    mpfr_clears(previous, current, next, ratio, sum, term, scratch, denominator, upper,
                xy, xy_sum, weighted, b_sum, forward, backward, magnitude, absolute_sum, threshold,
                cutoff, (mpfr_ptr)0);
    return converged ? 0 : -1;
}
