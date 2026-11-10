/**
 * @file number_error.c
 * @brief Guarded complex error functions, their inverses and related Gaussian distribution functions.
 *
 * Uses the entire Kummer series for complex erf and erfc, with additional precision for exponential cancellation.
 * Real forward values use MPFR directly. Inverses use a seed followed by checked Halley iteration at the requested
 * precision; approximate starting values never determine the returned accuracy. No fixed-precision Faddeeva
 * approximation or double-precision inverse result is used as the final answer.
 */
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

enum { ERROR_MAX_WORK_BITS = 65536, ERROR_MAX_SERIES_TERMS = 100000 };

static bool error_finite(mpc_srcptr z)
{
    return mpfr_number_p(mpc_realref(z)) && mpfr_number_p(mpc_imagref(z));
}

static bool error_zero(mpc_srcptr z)
{
    return mpfr_zero_p(mpc_realref(z)) && mpfr_zero_p(mpc_imagref(z));
}

static int error_forward(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding, bool complement)
{
    if (mpfr_zero_p(mpc_imagref(input))) {
        if (complement) mpfr_erfc(mpc_realref(out), mpc_realref(input), MPC_RND_RE(rounding));
        else mpfr_erf(mpc_realref(out), mpc_realref(input), MPC_RND_RE(rounding));
        mpfr_set_zero(mpc_imagref(out), mpfr_signbit(mpc_imagref(input)) ? -1 : 1);
        return 0;
    }
    if (!error_finite(input)) { mpc_set_nan(out); return -1; }
    mpfr_prec_t target = mpc_get_prec(out);
    double radius = hypot(mpfr_get_d(mpc_realref(input), MPFR_RNDU), mpfr_get_d(mpc_imagref(input), MPFR_RNDU));
    if (!isfinite(radius) || radius > 140) { mpc_set_nan(out); return -1; }
    mpfr_prec_t work = target + 96 + (mpfr_prec_t)ceil(3 * radius * radius);
    if (work > ERROR_MAX_WORK_BITS) { mpc_set_nan(out); return -1; }
    mpc_t z, square, term, sum, result;
    mpc_init2(z, work); mpc_init2(square, work); mpc_init2(term, work); mpc_init2(sum, work);
    mpc_init2(result, work);
    mpfr_t magnitude, absolute_sum, threshold, tolerance, square_radius, scale;
    mpfr_inits2(work, magnitude, absolute_sum, threshold, tolerance, square_radius, scale, (mpfr_ptr)0);
    bool reflect = mpfr_sgn(mpc_realref(input)) < 0;
    if (reflect) mpc_neg(z, input, MPC_RNDNN);
    else mpc_set(z, input, MPC_RNDNN);
    mpc_sqr(square, z, MPC_RNDNN);
    mpc_abs(square_radius, square, MPFR_RNDU);
    mpc_set_ui(term, 1u, MPC_RNDNN);
    mpc_set_ui(sum, 1u, MPC_RNDNN);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    mpfr_set_ui_2exp(tolerance, 1u, -work + 24, MPFR_RNDN);
    bool converged = false;
    for (unsigned long n = 1; n <= ERROR_MAX_SERIES_TERMS; ++n) {
        mpc_mul(term, term, square, MPC_RNDNN);
        mpc_mul_2ui(term, term, 1u, MPC_RNDNN);
        mpc_div_ui(term, term, 2 * n + 1, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (!error_finite(sum) || !error_finite(term)) break;
        mpc_abs(magnitude, term, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        mpc_abs(threshold, sum, MPFR_RNDD);
        mpfr_mul(threshold, threshold, tolerance, MPFR_RNDD);
        if (mpfr_cmp_ui(square_radius, n / 2) < 0 && mpfr_lessequal_p(magnitude, threshold)) {
            converged = true;
            break;
        }
    }
    if (converged) {
        mpc_neg(result, square, MPC_RNDNN);
        mpc_exp(result, result, MPC_RNDNN);
        mpc_mul(result, result, z, MPC_RNDNN);
        mpc_mul(result, result, sum, MPC_RNDNN);
        mpfr_const_pi(scale, MPFR_RNDN);
        mpfr_sqrt(scale, scale, MPFR_RNDN);
        mpfr_ui_div(scale, 2u, scale, MPFR_RNDN);
        mpc_mul_fr(result, result, scale, MPC_RNDNN);
        if (complement) mpc_ui_sub(result, 1u, result, MPC_RNDNN);
        if (reflect) {
            if (complement) mpc_ui_sub(result, 2u, result, MPC_RNDNN);
            else mpc_neg(result, result, MPC_RNDNN);
        }
        /* The guard includes the cancellation between 1 and erf(z) for the complementary value. */
        mpc_abs(threshold, result, MPFR_RNDD);
        mpfr_mul_2si(threshold, threshold, -target - 16, MPFR_RNDD);
        mpfr_mul_2si(magnitude, absolute_sum, -work + 16, MPFR_RNDU);
        mpc_abs(scale, z, MPFR_RNDU);
        mpfr_mul(magnitude, magnitude, scale, MPFR_RNDU);
        mpfr_neg(scale, mpc_realref(square), MPFR_RNDN);
        mpfr_exp(scale, scale, MPFR_RNDU);
        mpfr_mul(magnitude, magnitude, scale, MPFR_RNDU);
        converged = error_finite(result) && mpfr_lessequal_p(magnitude, threshold);
    }
    if (converged) mpc_set(out, result, rounding);
    else mpc_set_nan(out);
    mpfr_clears(magnitude, absolute_sum, threshold, tolerance, square_radius, scale, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(square); mpc_clear(term); mpc_clear(sum); mpc_clear(result);
    return converged ? 0 : -1;
}

/* Evaluate the entire complex error function with cancellation guards. */
int number_mpc_erf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    return error_forward(out, input, rounding, false);
}

/* Preserve small complementary values by using MPFR on the real axis and extra complex guard bits. */
int number_mpc_erfc(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    return error_forward(out, input, rounding, true);
}

/* Evaluate the Gaussian cumulative distribution without subtracting a near-unit erf value. */
int number_mpc_normal_cdf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpfr_prec_t precision = mpc_get_prec(out) + 32;
    mpc_t value;
    mpfr_t scale;
    mpc_init2(value, precision);
    mpfr_init2(scale, precision);
    mpfr_sqrt_ui(scale, 2u, MPFR_RNDN);
    mpc_div_fr(value, input, scale, MPC_RNDNN);
    mpc_neg(value, value, MPC_RNDNN);
    int status = error_forward(value, value, MPC_RNDNN, true);
    mpc_div_2ui(out, value, 1u, rounding);
    mpfr_clear(scale);
    mpc_clear(value);
    return status;
}

/* Retain complex precision in the analytic Gaussian log density. */
int number_mpc_normal_logpdf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpfr_prec_t precision = mpc_get_prec(out) + 32;
    mpc_t value;
    mpfr_t scale;
    mpc_init2(value, precision);
    mpfr_init2(scale, precision);
    mpfr_const_pi(scale, MPFR_RNDN);
    mpfr_mul_2ui(scale, scale, 1u, MPFR_RNDN);
    mpfr_log(scale, scale, MPFR_RNDN);
    mpc_sqr(value, input, MPC_RNDNN);
    mpfr_add(mpc_realref(value), mpc_realref(value), scale, MPFR_RNDN);
    mpc_neg(value, value, MPC_RNDNN);
    mpc_div_2ui(out, value, 1u, rounding);
    mpfr_clear(scale);
    mpc_clear(value);
    return 0;
}

/* Exponentiate the analytic log density with guard bits before final rounding. */
int number_mpc_normal_pdf(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpc_t value;
    mpc_init2(value, mpc_get_prec(out) + 32);
    int status = number_mpc_normal_logpdf(value, input, MPC_RNDNN);
    mpc_exp(out, value, rounding);
    mpc_clear(value);
    return status;
}

static bool error_inverse_seed(mpc_ptr out, mpc_srcptr input, bool complement)
{
    if (mpfr_zero_p(mpc_imagref(input))) {
        double x = mpfr_get_d(mpc_realref(input), MPFR_RNDN);
        qfloat_t initial = complement ? qf_erfcinv(qf_from_double(x)) : qf_erfinv(qf_from_double(x));
        double value = qf_to_double(initial);
        if (isfinite(value)) { mpc_set_d(out, value, MPC_RNDNN); return true; }
    }
    mpfr_prec_t work = mpc_get_prec(out);
    mpc_t target, logarithm, centre, square;
    mpc_init2(target, work); mpc_init2(logarithm, work); mpc_init2(centre, work); mpc_init2(square, work);
    mpfr_t coefficient, pi, size;
    mpfr_inits2(work, coefficient, pi, size, (mpfr_ptr)0);
    if (complement) mpc_ui_sub(target, 1u, input, MPC_RNDNN);
    else mpc_set(target, input, MPC_RNDNN);
    mpc_abs(size, input, MPFR_RNDN);
    if (complement && mpfr_cmp_d(size, 1e-8) < 0) {
        mpc_log(out, input, MPC_RNDNN);
        mpc_neg(out, out, MPC_RNDNN);
        mpc_sqrt(out, out, MPC_RNDNN);
    } else {
        /* Winitzki's approximation is only a seed; checked Halley steps supply the final precision. */
        mpfr_set_str(coefficient, "0.147", 10, MPFR_RNDN);
        mpfr_const_pi(pi, MPFR_RNDN);
        mpc_sqr(logarithm, target, MPC_RNDNN);
        mpc_ui_sub(logarithm, 1u, logarithm, MPC_RNDNN);
        mpc_log(logarithm, logarithm, MPC_RNDNN);
        mpfr_mul(pi, pi, coefficient, MPFR_RNDN);
        mpfr_ui_div(pi, 2u, pi, MPFR_RNDN);
        mpc_div_2ui(centre, logarithm, 1u, MPC_RNDNN);
        mpfr_add(mpc_realref(centre), mpc_realref(centre), pi, MPFR_RNDN);
        mpc_sqr(square, centre, MPC_RNDNN);
        mpc_div_fr(logarithm, logarithm, coefficient, MPC_RNDNN);
        mpc_sub(square, square, logarithm, MPC_RNDNN);
        mpc_sqrt(square, square, MPC_RNDNN);
        mpc_sub(square, square, centre, MPC_RNDNN);
        mpc_sqrt(out, square, MPC_RNDNN);
        if (mpfr_sgn(mpc_realref(target)) < 0 ||
            (mpfr_zero_p(mpc_realref(target)) && mpfr_sgn(mpc_imagref(target)) < 0))
            mpc_neg(out, out, MPC_RNDNN);
    }
    bool ok = error_finite(out);
    mpfr_clears(coefficient, pi, size, (mpfr_ptr)0);
    mpc_clear(target); mpc_clear(logarithm); mpc_clear(centre); mpc_clear(square);
    return ok;
}

static int error_inverse(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding, bool complement)
{
    if (!error_finite(input)) { mpc_set_nan(out); return -1; }
    if (mpfr_zero_p(mpc_imagref(input))) {
        int endpoint = complement ? mpfr_cmp_ui(mpc_realref(input), 0u) == 0 ? 1 :
                       mpfr_cmp_ui(mpc_realref(input), 2u) == 0 ? -1 : 0 :
                       mpfr_cmp_si(mpc_realref(input), 1) == 0 ? 1 :
                       mpfr_cmp_si(mpc_realref(input), -1) == 0 ? -1 : 0;
        if (endpoint) { mpfr_set_inf(mpc_realref(out), endpoint); mpfr_set_zero(mpc_imagref(out), 1); return 0; }
        if ((!complement && mpfr_zero_p(mpc_realref(input))) ||
            (complement && mpfr_cmp_ui(mpc_realref(input), 1u) == 0)) {
            mpc_set_ui(out, 0u, rounding);
            return 0;
        }
    }
    mpfr_prec_t target = mpc_get_prec(out), work = target + 96;
    if (work > ERROR_MAX_WORK_BITS) { mpc_set_nan(out); return -1; }
    mpc_t value, residual, derivative, denominator, delta, scratch;
    mpc_init2(value, work); mpc_init2(residual, work); mpc_init2(derivative, work);
    mpc_init2(denominator, work); mpc_init2(delta, work); mpc_init2(scratch, work);
    mpfr_t scale, tolerance, magnitude, coefficient;
    mpfr_inits2(work, scale, tolerance, magnitude, coefficient, (mpfr_ptr)0);
    mpfr_const_pi(coefficient, MPFR_RNDN);
    mpfr_sqrt(coefficient, coefficient, MPFR_RNDN);
    mpfr_ui_div(coefficient, 2u, coefficient, MPFR_RNDN);
    if (complement) mpfr_neg(coefficient, coefficient, MPFR_RNDN);
    mpfr_set_ui_2exp(tolerance, 1u, -target - 24, MPFR_RNDN);
    bool ok = error_inverse_seed(value, input, complement), converged = false;
    unsigned steps = 24;
    for (mpfr_prec_t p = target; p > 1; p /= 2) ++steps;
    for (unsigned i = 0; ok && i < steps; ++i) {
        ok = !error_forward(residual, value, MPC_RNDNN, complement);
        if (!ok) break;
        mpc_sub(residual, residual, input, MPC_RNDNN);
        mpc_sqr(scratch, value, MPC_RNDNN);
        mpc_neg(scratch, scratch, MPC_RNDNN);
        mpc_exp(derivative, scratch, MPC_RNDNN);
        mpc_mul_fr(derivative, derivative, coefficient, MPC_RNDNN);
        mpc_mul(denominator, value, residual, MPC_RNDNN);
        mpc_add(denominator, denominator, derivative, MPC_RNDNN);
        if (error_zero(denominator)) break;
        mpc_div(delta, residual, denominator, MPC_RNDNN);
        mpc_sub(value, value, delta, MPC_RNDNN);
        if (!error_finite(value) || !error_finite(delta)) break;
        mpc_abs(scale, value, MPFR_RNDD);
        if (mpfr_cmp_ui(scale, 1u) < 0) mpfr_set_ui(scale, 1u, MPFR_RNDN);
        mpfr_mul(scale, scale, tolerance, MPFR_RNDD);
        mpc_abs(magnitude, delta, MPFR_RNDU);
        if (mpfr_lessequal_p(magnitude, scale)) { converged = true; break; }
    }
    if (converged && !error_forward(residual, value, MPC_RNDNN, complement)) {
        mpc_sub(residual, residual, input, MPC_RNDNN);
        mpc_abs(magnitude, residual, MPFR_RNDU);
        mpc_abs(scale, input, MPFR_RNDD);
        mpfr_mul_2si(scale, scale, -target - 16, MPFR_RNDD);
        converged = mpfr_lessequal_p(magnitude, scale);
    } else converged = false;
    if (converged) mpc_set(out, value, rounding);
    else mpc_set_nan(out);
    mpfr_clears(scale, tolerance, magnitude, coefficient, (mpfr_ptr)0);
    mpc_clear(value); mpc_clear(residual); mpc_clear(derivative); mpc_clear(denominator); mpc_clear(delta); mpc_clear(scratch);
    return converged ? 0 : -1;
}

/* Refine an inverse erf seed to the requested precision and verify its defining equation. */
int number_mpc_erfinv(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    return error_inverse(out, input, rounding, false);
}

/* Refine inverse erfc directly, preserving very small real complementary probabilities. */
int number_mpc_erfcinv(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    return error_inverse(out, input, rounding, true);
}

static int error_inverse_real(mpfr_ptr value, bool complement)
{
    if (!mpfr_number_p(value) || (complement ? mpfr_sgn(value) < 0 || mpfr_cmp_ui(value, 2u) > 0 :
                                             mpfr_cmp_si(value, -1) < 0 || mpfr_cmp_ui(value, 1u) > 0)) {
        mpfr_set_nan(value);
        return -1;
    }
    mpc_t input, result;
    mpc_init2(input, mpfr_get_prec(value)); mpc_init2(result, mpfr_get_prec(value));
    mpc_set_fr(input, value, MPC_RNDNN);
    int status = error_inverse(result, input, MPC_RNDNN, complement);
    if (!status && mpfr_zero_p(mpc_imagref(result))) mpfr_set(value, mpc_realref(result), MPFR_RNDN);
    else { mpfr_set_nan(value); status = -1; }
    mpc_clear(input); mpc_clear(result);
    return status;
}

/* Evaluate real inverse erf without narrowing the operand or result to double. */
int number_mpfr_erfinv_mut(mpfr_t value) { return error_inverse_real(value, false); }

/* Evaluate real inverse erfc with the same checked multiprecision iteration. */
int number_mpfr_erfcinv_mut(mpfr_t value) { return error_inverse_real(value, true); }
