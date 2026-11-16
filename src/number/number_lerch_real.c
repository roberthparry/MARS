/**
 * @file number_lerch_real.c
 * @brief Guarded MPFR defining sums for real Lerch parameters.
 *
 * Positive offsets keep every inverse power real. Short Horner blocks and decreasing coefficient precision
 * reduce arithmetic work while directed geometric-tail and roundoff bounds check the final relative accuracy.
 * The bounded inverse-power cache retains its exact MPC keys, but real cache misses and all accumulation use
 * MPFR. No input or cache storage escapes; unsupported domains remain the caller's responsibility.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Sum real defining terms, accepting a result only after bounding truncation and accumulated roundoff. */
bool number_lerch_real(mpfr_ptr out, mpc_srcptr argument, mpc_srcptr order, mpc_srcptr offset, mpfr_prec_t target)
{
    if (mpfr_sgn(mpc_realref(offset)) <= 0) return false;
    mpfr_prec_t work = target + 128;
    enum { LERCH_REAL_BLOCK = 16 };
    unsigned width = target >= 512 ? LERCH_REAL_BLOCK : 1u;
    mpfr_t coefficients[LERCH_REAL_BLOCK], sum, power, step, term, base;
    for (unsigned j = 0; j < width; ++j) mpfr_init2(coefficients[j], work);
    mpfr_inits2(work, sum, power, step, term, base, (mpfr_ptr)0);
    mpfr_set_zero(sum, 1); mpfr_set_ui(power, 1u, MPFR_RNDN);
    mpfr_pow_ui(step, mpc_realref(argument), width, MPFR_RNDN);
    mpc_t cached;
    mpc_init2(cached, work);
    mpfr_t radius, magnitude, absolute_sum, error, temporary, last, ratio, tail, tolerance;
    mpfr_inits2(64, radius, magnitude, absolute_sum, error, temporary, last, ratio, tail, tolerance, (mpfr_ptr)0);
    mpfr_abs(radius, mpc_realref(argument), MPFR_RNDU);
    mpfr_set_zero(absolute_sum, 1); mpfr_set_zero(error, 1);
    bool integral = mpfr_integer_p(mpc_realref(order)) && mpfr_fits_slong_p(mpc_realref(order), MPFR_RNDN);
    long exponent = integral ? mpfr_get_si(mpc_realref(order), MPFR_RNDN) : 0;
    bool complete = false;
    for (unsigned long k = 0; mpfr_cmp_ui(radius, 1u) < 0 && k < 200000; k += width) {
        mpfr_abs(magnitude, power, MPFR_RNDU);
        for (unsigned j = 0; j < width; ++j) {
            mpfr_exp_t size = mpfr_zero_p(magnitude) ? 0 : mpfr_get_exp(magnitude);
            mpfr_prec_t precision = size < 0 ? (size < 128 - work ? 128 : work + size) : work;
            if (mpfr_sgn(mpc_realref(order)) < 0) {
                /* base < 2^E implies base^(-s) <= 2^(-s*E) for negative real s. Account for that
                 * growth before shortening coefficients; powers of z alone can underestimate the work. */
                mpfr_add_ui(base, mpc_realref(offset), k + j, MPFR_RNDN);
                if (!mpfr_number_p(base)) goto done;
                mpfr_set_si(temporary, mpfr_get_exp(base), MPFR_RNDN);
                mpfr_mul(temporary, temporary, mpc_realref(order), MPFR_RNDD);
                mpfr_neg(temporary, temporary, MPFR_RNDU); mpfr_add_si(temporary, temporary, size, MPFR_RNDU);
                precision = mpfr_sgn(temporary) >= 0 ? work :
                    (mpfr_cmp_si(temporary, 128 - work) < 0 ? 128 : work + mpfr_get_si(temporary, MPFR_RNDU));
            }
            if (mpfr_get_prec(coefficients[j]) != precision) mpfr_set_prec(coefficients[j], precision);
            if (integral) {
                mpfr_add_ui(base, mpc_realref(offset), k + j, MPFR_RNDN);
                if (exponent < 0)
                    mpfr_pow_ui(coefficients[j], base, 0ul - (unsigned long)exponent, MPFR_RNDN);
                else {
                    mpfr_pow_si(base, base, exponent, MPFR_RNDN);
                    mpfr_ui_div(coefficients[j], 1u, base, MPFR_RNDN);
                }
            } else {
                if (mpc_get_prec(cached) != precision) mpc_set_prec(cached, precision);
                number_dirichlet_power(cached, offset, order, k + j);
                mpfr_set(coefficients[j], mpc_realref(cached), MPFR_RNDN);
            }
            mpfr_abs(last, coefficients[j], MPFR_RNDU);
            mpfr_mul(last, last, magnitude, MPFR_RNDU);
            mpfr_add(absolute_sum, absolute_sum, last, MPFR_RNDU);
            mpfr_mul_2si(temporary, last, -precision + 10, MPFR_RNDU);
            mpfr_add(error, error, temporary, MPFR_RNDU);
            if (j + 1 < width) mpfr_mul(magnitude, magnitude, radius, MPFR_RNDU);
        }
        mpfr_set(term, coefficients[width - 1], MPFR_RNDN);
        for (unsigned j = width - 1; j > 0; --j) {
            mpfr_mul(term, term, mpc_realref(argument), MPFR_RNDN);
            mpfr_add(term, term, coefficients[j - 1], MPFR_RNDN);
        }
        mpfr_mul(term, term, power, MPFR_RNDN); mpfr_add(sum, sum, term, MPFR_RNDN);
        if (!mpfr_number_p(sum)) break;
        mpfr_set(ratio, radius, MPFR_RNDU);
        if (mpfr_sgn(mpc_realref(order)) < 0) {
            mpfr_add_ui(temporary, mpc_realref(offset), k + width - 1, MPFR_RNDD);
            mpfr_ui_div(temporary, 1u, temporary, MPFR_RNDU);
            mpfr_add_ui(temporary, temporary, 1u, MPFR_RNDU);
            mpfr_neg(tail, mpc_realref(order), MPFR_RNDU);
            mpfr_pow(temporary, temporary, tail, MPFR_RNDU);
            mpfr_mul(ratio, ratio, temporary, MPFR_RNDU);
        }
        if (mpfr_number_p(ratio) && mpfr_cmp_ui(ratio, 1u) < 0) {
            mpfr_ui_sub(temporary, 1u, ratio, MPFR_RNDD);
            mpfr_div(tail, ratio, temporary, MPFR_RNDU); mpfr_mul(tail, tail, last, MPFR_RNDU);
            mpfr_add(tail, tail, error, MPFR_RNDU);
            mpfr_mul_ui(temporary, absolute_sum, (k + width) * 32, MPFR_RNDU);
            mpfr_mul_2si(temporary, temporary, -work + 8, MPFR_RNDU);
            mpfr_add(tail, tail, temporary, MPFR_RNDU);
            mpfr_abs(tolerance, sum, MPFR_RNDD); mpfr_mul_2si(tolerance, tolerance, -target - 8, MPFR_RNDD);
            if (mpfr_lessequal_p(tail, tolerance)) { complete = true; break; }
        }
        mpfr_mul(power, power, step, MPFR_RNDN);
    }
done:
    if (complete) mpfr_set(out, sum, MPFR_RNDN);
    mpfr_clears(radius, magnitude, absolute_sum, error, temporary, last, ratio, tail, tolerance, (mpfr_ptr)0);
    mpc_clear(cached);
    mpfr_clears(sum, power, step, term, base, (mpfr_ptr)0);
    for (unsigned j = 0; j < width; ++j) mpfr_clear(coefficients[j]);
    return complete;
}
