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

enum { GAMMA_BLOCK = 16 };

/* Evaluate rising-factorial blocks with exact integer coefficients and reusable powers of the initial base.
 * For Re(base)>0 their absolute condition number is at most 2^(B/2); the 128 guard bits cover B<=16.
 * A block's argument advance is less than one radian, so the existing negative-axis crossing count stays valid. */
static bool gamma_rising_product(mpc_ptr product, mpc_ptr z, unsigned long limit, unsigned *shifts, long *turns)
{
    mpfr_prec_t work = mpc_get_prec(product);
    mpc_t powers[GAMMA_BLOCK + 1], polynomial, term;
    mpz_t coefficients[GAMMA_BLOCK + 1];
    for (unsigned j = 0; j <= GAMMA_BLOCK; ++j) {
        mpc_init2(powers[j], work);
        mpz_init(coefficients[j]);
    }
    mpc_init2(polynomial, work); mpc_init2(term, work);
    mpfr_t angle_bound;
    mpfr_init2(angle_bound, 64);
    mpc_set_ui(powers[0], 1u, MPC_RNDNN);
    bool powers_ready = false, ok = true;
    unsigned long offset = 0;
    int direction = mpfr_sgn(mpc_imagref(z));
    while (mpfr_cmp_ui(mpc_realref(z), limit) < 0) {
        mpfr_abs(angle_bound, mpc_imagref(z), MPFR_RNDU);
        mpfr_mul_ui(angle_bound, angle_bound, GAMMA_BLOCK, MPFR_RNDU);
        unsigned width = work >= 640 && mpfr_less_p(angle_bound, mpc_realref(z)) ? GAMMA_BLOCK : 1u;
        if (*shifts > 100000u - width) { ok = false; break; }
        if (width == 1) mpc_set(polynomial, z, MPC_RNDNN);
        else {
            if (!powers_ready) {
                mpc_sub_ui(powers[1], z, offset, MPC_RNDNN);
                for (unsigned j = 2; j <= GAMMA_BLOCK; ++j)
                    mpc_mul(powers[j], powers[j - 1], powers[1], MPC_RNDNN);
                powers_ready = true;
            }
            mpz_set_ui(coefficients[0], 1u);
            for (unsigned j = 1; j <= width; ++j) mpz_set_ui(coefficients[j], 0u);
            for (unsigned j = 0; j < width; ++j) {
                for (unsigned degree = j + 1; degree > 0; --degree) {
                    mpz_mul_ui(coefficients[degree], coefficients[degree], offset + j);
                    mpz_add(coefficients[degree], coefficients[degree], coefficients[degree - 1]);
                }
                mpz_mul_ui(coefficients[0], coefficients[0], offset + j);
            }
            mpc_set_z(polynomial, coefficients[0], MPC_RNDNN);
            for (unsigned degree = 1; degree <= width; ++degree) {
                mpfr_mul_z(mpc_realref(term), mpc_realref(powers[degree]), coefficients[degree], MPFR_RNDN);
                mpfr_mul_z(mpc_imagref(term), mpc_imagref(powers[degree]), coefficients[degree], MPFR_RNDN);
                mpc_add(polynomial, polynomial, term, MPC_RNDNN);
            }
        }
        int previous = mpfr_sgn(mpc_imagref(product));
        mpc_mul(term, product, polynomial, MPC_RNDNN);
        int next = mpfr_sgn(mpc_imagref(term));
        if (mpfr_sgn(mpc_realref(product)) < 0 || mpfr_sgn(mpc_realref(term)) < 0) {
            if (direction > 0 && previous >= 0 && next < 0) ++*turns;
            if (direction < 0 && previous <= 0 && next > 0) --*turns;
        }
        mpc_set(product, term, MPC_RNDNN);
        mpc_add_ui(z, z, width, MPC_RNDNN);
        offset += width;
        *shifts += width;
    }
    mpfr_clear(angle_bound);
    mpc_clear(term); mpc_clear(polynomial);
    for (unsigned j = 0; j <= GAMMA_BLOCK; ++j) { mpc_clear(powers[j]); mpz_clear(coefficients[j]); }
    return ok;
}

/* Rectangular splitting removes most full-width complex products from the Stirling correction polynomial.
 * Directed scalar magnitudes retain the original first-omitted-term and sector stopping bound. */
static bool gamma_stirling_correction(mpc_ptr out, mpc_srcptr inverse, mpfr_prec_t target)
{
    mpfr_prec_t work = mpc_get_prec(out);
    unsigned width = work >= 640 ? GAMMA_BLOCK : 1u;
    mpc_t baby[GAMMA_BLOCK], square, giant, factor, polynomial, term;
    for (unsigned j = 0; j < width; ++j) mpc_init2(baby[j], work);
    mpc_init2(square, work); mpc_init2(giant, work); mpc_init2(factor, work);
    mpc_init2(polynomial, work); mpc_init2(term, work);
    mpc_sqr(square, inverse, MPC_RNDNN);
    mpc_set_ui(baby[0], 1u, MPC_RNDNN);
    for (unsigned j = 1; j < width; ++j) mpc_mul(baby[j], baby[j - 1], square, MPC_RNDNN);
    mpc_mul(giant, baby[width - 1], square, MPC_RNDNN);
    mpc_set(factor, inverse, MPC_RNDNN); mpc_set_ui(polynomial, 0u, MPC_RNDNN);
    mpc_set_ui(out, 0u, MPC_RNDNN);
    mpfr_t coefficient, inverse_size, square_size, power_size, factorial_size, bound;
    mpfr_init2(coefficient, work);
    mpfr_inits2(64, inverse_size, square_size, power_size, factorial_size, bound, (mpfr_ptr)0);
    mpc_abs(inverse_size, inverse, MPFR_RNDU);
    mpfr_sqr(square_size, inverse_size, MPFR_RNDU); mpfr_set(power_size, inverse_size, MPFR_RNDU);
    mpfr_set_ui(factorial_size, 1u, MPFR_RNDN);
    bool ok = false;
    unsigned index = 0;
    for (unsigned long n = 1; n < (unsigned long)work; ++n) {
        /* |B_(2n)/(2n(2n-1))| < 4*(2n-2)!/32^n. Only the coefficient loses bits;
         * all complex powers and accumulators retain work precision. */
        if (n > 1) mpfr_mul_ui(factorial_size, factorial_size, (2 * n - 2) * (2 * n - 3), MPFR_RNDU);
        mpfr_mul(bound, factorial_size, power_size, MPFR_RNDU);
        mpfr_exp_t adjustment = mpfr_number_p(bound) && !mpfr_zero_p(bound) ?
            mpfr_get_exp(bound) - 5 * (mpfr_exp_t)n + 34 : 0;
        mpfr_prec_t precision = work;
        if (adjustment < 0) precision = adjustment < 128 - work ? 128 : work + adjustment;
        if (mpfr_get_prec(coefficient) != precision) mpfr_set_prec(coefficient, precision);
        if (number_coefficient_bernoulli(coefficient, n)) break;
        mpfr_div_ui(coefficient, coefficient, 2 * n * (2 * n - 1), MPFR_RNDN);
        mpfr_abs(bound, coefficient, MPFR_RNDU); mpfr_mul(bound, bound, power_size, MPFR_RNDU);
        if (mpfr_cmp_ui_2exp(bound, 1u, -target - 48 - (long)n) < 0) { ok = true; break; }
        mpc_mul_fr(term, baby[index], coefficient, MPC_RNDNN);
        mpc_add(polynomial, polynomial, term, MPC_RNDNN);
        if (++index == width) {
            mpc_mul(term, polynomial, factor, MPC_RNDNN); mpc_add(out, out, term, MPC_RNDNN);
            mpc_set_ui(polynomial, 0u, MPC_RNDNN); mpc_mul(factor, factor, giant, MPC_RNDNN);
            index = 0;
        }
        mpfr_mul(power_size, power_size, square_size, MPFR_RNDU);
    }
    if (ok && index) { mpc_mul(term, polynomial, factor, MPC_RNDNN); mpc_add(out, out, term, MPC_RNDNN); }
    mpfr_clear(coefficient); mpfr_clears(inverse_size, square_size, power_size, factorial_size, bound, (mpfr_ptr)0);
    mpc_clear(term); mpc_clear(polynomial); mpc_clear(factor); mpc_clear(giant); mpc_clear(square);
    for (unsigned j = 0; j < width; ++j) mpc_clear(baby[j]);
    return ok;
}

/* Evaluate log Gamma continuously on the plane cut along the non-positive real axis. */
int number_mpc_lgamma(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out), work = target + 128;
    if (work > 65536 || !mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input))) {
        mpc_set_nan(out);
        return -1;
    }
    mpc_t z, product, correction, sum, inverse, term;
    mpc_init2(z, work); mpc_init2(product, work); mpc_init2(correction, work); mpc_init2(sum, work);
    mpc_init2(inverse, work); mpc_init2(term, work);
    mpfr_t coefficient, pi;
    mpfr_inits2(work, coefficient, pi, (mpfr_ptr)0);
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
    unsigned long limit = (unsigned long)work / 3 + 16;
    if (!gamma_rising_product(product, z, limit, &shifts, &turns)) goto done;
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
    ok = gamma_stirling_correction(term, inverse, target);
    if (ok) mpc_add(sum, sum, term, MPC_RNDNN);
    if (ok) {
        mpc_sub(sum, sum, correction, MPC_RNDNN);
        mpc_set(out, sum, rounding);
    }
done:
    if (!ok) mpc_set_nan(out);
    mpfr_clears(coefficient, pi, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(product); mpc_clear(correction); mpc_clear(sum);
    mpc_clear(inverse); mpc_clear(term);
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
