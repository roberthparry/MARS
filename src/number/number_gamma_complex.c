/**
 * @file number_gamma_complex.c
 * @brief Guarded complex gamma and logarithmic gamma at the requested precision.
 *
 * Shifts to the right half-plane and uses the Stirling expansion with cached Bernoulli coefficients. A product
 * accumulates positive-real-part recurrence factors, recording complete turns so the analytic logarithm is not
 * replaced by the principal logarithm of that product. Negative-real-part factors are handled individually.
 * Local scratch storage and bounded iteration leave coefficient retention to the number module's shared cache.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Evaluate log Gamma continuously on the plane cut along the non-positive real axis. */
int number_mpc_lgamma(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 128;
    if (work > 65536 || !mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input))) {
        mpc_set_nan(out);
        return -1;
    }
    mpc_t z, product, correction, sum, inverse, square, power, term;
    mpc_init2(z, work); mpc_init2(product, work); mpc_init2(correction, work); mpc_init2(sum, work);
    mpc_init2(inverse, work); mpc_init2(square, work); mpc_init2(power, work); mpc_init2(term, work);
    mpfr_t coefficient, pi, magnitude;
    mpfr_inits2(work, coefficient, pi, magnitude, (mpfr_ptr)0);
    mpc_set(z, input, MPC_RNDNN); mpc_set_ui(product, 1u, MPC_RNDNN); mpc_set_ui(correction, 0u, MPC_RNDNN);
    bool ok = false;
    unsigned shifts = 0;
    while (mpfr_sgn(mpc_realref(z)) <= 0) {
        if (++shifts > 100000 || (mpfr_zero_p(mpc_realref(z)) && mpfr_zero_p(mpc_imagref(z)))) goto done;
        mpc_log(term, z, MPC_RNDNN);
        mpc_add(correction, correction, term, MPC_RNDNN);
        mpc_add_ui(z, z, 1u, MPC_RNDNN);
    }
    long turns = 0;
    int direction = mpfr_sgn(mpc_imagref(z));
    unsigned long limit = (unsigned long)work / 3 + 16;
    while (mpfr_cmp_ui(mpc_realref(z), limit) < 0) {
        if (++shifts > 100000) goto done;
        int previous = mpfr_sgn(mpc_imagref(product));
        mpc_mul(term, product, z, MPC_RNDNN);
        int next = mpfr_sgn(mpc_imagref(term));
        if (mpfr_sgn(mpc_realref(product)) < 0 || mpfr_sgn(mpc_realref(term)) < 0) {
            if (direction > 0 && previous >= 0 && next < 0) ++turns;
            if (direction < 0 && previous <= 0 && next > 0) --turns;
        }
        mpc_set(product, term, MPC_RNDNN);
        mpc_add_ui(z, z, 1u, MPC_RNDNN);
    }
    mpc_log(term, product, MPC_RNDNN);
    mpfr_const_pi(pi, MPFR_RNDN);
    mpfr_mul_si(coefficient, pi, 2 * turns, MPFR_RNDN);
    mpfr_add(mpc_imagref(term), mpc_imagref(term), coefficient, MPFR_RNDN);
    mpc_add(correction, correction, term, MPC_RNDNN);
    mpc_log(term, z, MPC_RNDNN);
    mpc_set(sum, z, MPC_RNDNN);
    mpfr_sub_d(mpc_realref(sum), mpc_realref(sum), 0.5, MPFR_RNDN);
    mpc_mul(sum, sum, term, MPC_RNDNN);
    mpc_sub(sum, sum, z, MPC_RNDNN);
    mpfr_mul_2ui(coefficient, pi, 1u, MPFR_RNDN);
    mpfr_log(coefficient, coefficient, MPFR_RNDN);
    mpfr_div_2ui(coefficient, coefficient, 1u, MPFR_RNDN);
    mpfr_add(mpc_realref(sum), mpc_realref(sum), coefficient, MPFR_RNDN);
    mpc_ui_div(inverse, 1u, z, MPC_RNDNN);
    mpc_sqr(square, inverse, MPC_RNDNN); mpc_set(power, inverse, MPC_RNDNN);
    for (unsigned long n = 1; n < (unsigned long)work; ++n) {
        if (number_coefficient_bernoulli(coefficient, n)) goto done;
        mpfr_div_ui(coefficient, coefficient, 2 * n * (2 * n - 1), MPFR_RNDN);
        mpc_mul_fr(term, power, coefficient, MPC_RNDNN);
        number_mpc_abs_upper(magnitude, term);
        /* In Re(z)>0, sec(arg(z)/2)^(2n) <= 2^n bounds the first omitted Stirling term. */
        if (mpfr_cmp_ui_2exp(magnitude, 1u, -target - 48 - (long)n) < 0) { ok = true; break; }
        mpc_add(sum, sum, term, MPC_RNDNN);
        mpc_mul(power, power, square, MPC_RNDNN);
    }
    if (ok) {
        mpc_sub(sum, sum, correction, MPC_RNDNN);
        mpc_set(out, sum, rounding);
    }
done:
    if (!ok) mpc_set_nan(out);
    mpfr_clears(coefficient, pi, magnitude, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(product); mpc_clear(correction); mpc_clear(sum);
    mpc_clear(inverse); mpc_clear(square); mpc_clear(power); mpc_clear(term);
    return ok ? 0 : -1;
}

/* Exponentiate guarded log Gamma rather than using a fixed-precision Lanczos approximation. */
int number_mpc_gamma(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    if (!mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input))) {
        mpc_set_nan(out);
        return -1;
    }
    mpfr_exp_t extra = 32;
    const mpfr_srcptr parts[] = {mpc_realref(input), mpc_imagref(input)};
    for (size_t i = 0; i < 2; ++i)
        if (!mpfr_zero_p(parts[i]) && mpfr_get_exp(parts[i]) > extra) extra = mpfr_get_exp(parts[i]);
    if (extra > 65000 || mpc_get_prec(out) > 65536 - extra - 128) { mpc_set_nan(out); return -1; }
    mpc_t logarithm;
    mpc_init2(logarithm, mpc_get_prec(out) + extra);
    int status = number_mpc_lgamma(logarithm, input, MPC_RNDNN);
    if (!status) mpc_exp(out, logarithm, rounding);
    else mpc_set_nan(out);
    mpc_clear(logarithm);
    return status;
}

/* Bound the lower series and retain extra bits before subtracting it from complete gamma. */
int number_mpc_gammainc(mpc_ptr out, mpc_srcptr order, mpc_srcptr argument,
                        number_gamma_mode_t mode, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out);
    mpfr_t radius;
    mpfr_init2(radius, 64);
    mpc_abs(radius, argument, MPFR_RNDU);
    if (!mpfr_number_p(radius) || mpfr_cmp_ui(radius, 8000u) > 0 ||
        !mpfr_number_p(mpc_realref(order)) || !mpfr_number_p(mpc_imagref(order))) {
        mpfr_clear(radius); mpc_set_nan(out); return -1;
    }
    mpfr_prec_t work = target + 160 + 4 * mpfr_get_ui(radius, MPFR_RNDU);
    if (work > 65536) { mpfr_clear(radius); mpc_set_nan(out); return -1; }
    mpc_t a, z, term, sum, denominator, factor, complete;
    mpc_init2(a, work); mpc_init2(z, work); mpc_init2(term, work); mpc_init2(sum, work);
    mpc_init2(denominator, work); mpc_init2(factor, work); mpc_init2(complete, work);
    mpfr_t order_size, ratio, tail, scale, absolute_sum, error;
    mpfr_inits2(work, order_size, ratio, tail, scale, absolute_sum, error, (mpfr_ptr)0);
    mpc_set(a, order, MPC_RNDNN); mpc_set(z, argument, MPC_RNDNN);
    mpc_abs(order_size, a, MPFR_RNDU);
    bool ok = false;
    if (mpfr_zero_p(mpc_imagref(a)) && mpfr_integer_p(mpc_realref(a)) && mpfr_sgn(mpc_realref(a)) <= 0)
        goto done;
    if (mpfr_zero_p(radius) && mpfr_sgn(mpc_realref(a)) <= 0) goto done;
    mpc_ui_div(term, 1u, a, MPC_RNDNN); mpc_set(sum, term, MPC_RNDNN);
    number_mpc_abs_upper(absolute_sum, term);
    for (unsigned long n = 1; n < 100000; ++n) {
        mpc_add_ui(denominator, a, n, MPC_RNDNN);
        mpc_mul(term, term, z, MPC_RNDNN); mpc_div(term, term, denominator, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        number_mpc_abs_upper(tail, term); mpfr_add(absolute_sum, absolute_sum, tail, MPFR_RNDU);
        mpfr_ui_sub(ratio, n + 1, order_size, MPFR_RNDD);
        if (mpfr_sgn(ratio) <= 0) continue;
        mpfr_div(ratio, radius, ratio, MPFR_RNDU);
        if (mpfr_cmp_d(ratio, 0.5) > 0) continue;
        number_mpc_abs_lower(scale, sum);
        if (number_series_small(tail, scale, work - 40, 1u)) { ok = true; break; }
    }
    if (!ok) goto done;
    if (mpfr_zero_p(radius)) mpc_set_ui(factor, 0u, MPC_RNDNN);
    else {
        mpc_log(factor, z, MPC_RNDNN); mpc_mul(factor, factor, a, MPC_RNDNN);
        mpc_sub(factor, factor, z, MPC_RNDNN); mpc_exp(factor, factor, MPC_RNDNN);
    }
    number_mpc_abs_upper(error, factor);
    mpfr_mul(error, error, absolute_sum, MPFR_RNDU);
    mpfr_mul_2si(error, error, -work + 32, MPFR_RNDU);
    mpc_mul(sum, sum, factor, MPC_RNDNN);
    if (mode != NUMBER_GAMMA_LOWER) {
        if (number_mpc_gamma(complete, a, MPC_RNDNN)) { ok = false; goto done; }
        number_mpc_abs_upper(scale, complete);
        mpfr_mul_2si(scale, scale, -work + 32, MPFR_RNDU); mpfr_add(error, error, scale, MPFR_RNDU);
        if (mode == NUMBER_GAMMA_UPPER || mode == NUMBER_GAMMA_Q) mpc_sub(sum, complete, sum, MPC_RNDNN);
    }
    number_mpc_abs_lower(scale, sum);
    mpfr_mul_2si(scale, scale, -target - 16, MPFR_RNDD);
    ok = mpfr_lessequal_p(error, scale);
    if (mode == NUMBER_GAMMA_P || mode == NUMBER_GAMMA_Q) mpc_div(sum, sum, complete, MPC_RNDNN);
    if (ok) mpc_set(out, sum, rounding);
done:
    if (!ok) mpc_set_nan(out);
    mpc_clear(a); mpc_clear(z); mpc_clear(term); mpc_clear(sum); mpc_clear(denominator);
    mpc_clear(factor); mpc_clear(complete);
    mpfr_clears(radius, order_size, ratio, tail, scale, absolute_sum, error, (mpfr_ptr)0);
    return ok ? 0 : -1;
}
