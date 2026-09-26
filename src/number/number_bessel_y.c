#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include <math.h>

enum { BESSEL_Y_MAX_WORK_BITS = 65536, BESSEL_Y_MAX_TERMS = 20000 };

/* Exact components must reach the working precision before any inexact conversion. */
static bool bessel_y_set_real(mpfr_ptr out, number_t value)
{
    const number_private_t *impl = number_impl_const(&value);
    if (impl->kind == NUMBER_MPZ) {
        mpfr_set_z(out, number_mpz_value(impl->value.mpz), MPFR_RNDN);
    } else if (impl->kind == NUMBER_MPQ) {
        mpfr_set_q(out, number_mpq_value(impl->value.mpq), MPFR_RNDN);
    } else if (impl->kind == NUMBER_MPFR) {
        if (number_mpfr_ensure(impl->value.mpfr, (size_t)mpfr_get_prec(out)) != 0)
            return false;
        mpfr_set(out, number_mpfr_value(impl->value.mpfr), MPFR_RNDN);
    } else {
        qfloat_t q = num_to_qfloat(value);
        mpfr_set_d(out, q.hi, MPFR_RNDN);
        mpfr_add_d(out, out, q.lo, MPFR_RNDN);
    }
    return mpfr_number_p(out);
}

static bool bessel_y_set_complex(mpc_ptr out, number_t value)
{
    NUM_SCOPE(scope);
    return bessel_y_set_real(mpc_realref(out), num_real_part(value)) &&
           bessel_y_set_real(mpc_imagref(out), num_imag_part(value));
}

static bool bessel_y_finite(mpc_srcptr value)
{
    return mpfr_number_p(mpc_realref(value)) && mpfr_number_p(mpc_imagref(value));
}

/* A conservative accumulated-roundoff allowance must fit well inside the requested precision. */
static bool bessel_y_resolved(mpc_srcptr value, mpfr_srcptr absolute_sum, size_t precision)
{
    mpfr_prec_t work = mpc_get_prec(value);
    mpfr_t error, threshold;
    mpfr_inits2(work, error, threshold, (mpfr_ptr)0);
    mpfr_mul_2si(error, absolute_sum, -work + 32, MPFR_RNDU);
    mpc_abs(threshold, value, MPFR_RNDD);
    mpfr_mul_2si(threshold, threshold, -(long)precision - 16, MPFR_RNDD);
    bool ok = bessel_y_finite(value) && mpfr_lessequal_p(error, threshold);
    mpfr_clears(error, threshold, (mpfr_ptr)0);
    return ok;
}

/* DLMF 10.8.1: combine the logarithmic J series with its harmonic-number weights. */
static bool bessel_y_integer(mpc_ptr out, unsigned long n, mpc_srcptr z, size_t precision)
{
    mpfr_prec_t work = mpc_get_prec(out);
    mpc_t half_z, square, term, weight, contribution;
    mpfr_t pi, scratch, magnitude, absolute_sum, weight_bound, square_size, ratio, tail, threshold;
    mpc_init2(half_z, work);
    mpc_init2(square, work);
    mpc_init2(term, work);
    mpc_init2(weight, work);
    mpc_init2(contribution, work);
    mpfr_inits2(work, pi, scratch, magnitude, absolute_sum, weight_bound, square_size, ratio, tail, threshold,
                (mpfr_ptr)0);
    bool ok = false;
    mpfr_const_pi(pi, MPFR_RNDN);
    mpc_div_2ui(half_z, z, 1u, MPC_RNDNN);
    mpc_sqr(square, half_z, MPC_RNDNN);
    mpc_abs(square_size, square, MPFR_RNDU);
    mpc_set_ui(out, 0u, MPC_RNDNN);
    mpfr_set_zero(absolute_sum, 1);

    /* Finite singular part; n=0 has an empty sum. */
    if (n != 0u) {
        mpc_pow_ui(term, half_z, n, MPC_RNDNN);
        mpc_ui_div(term, 1u, term, MPC_RNDNN);
        mpfr_fac_ui(scratch, n - 1u, MPFR_RNDN);
        mpc_mul_fr(term, term, scratch, MPC_RNDNN);
        for (unsigned long k = 0; k < n; ++k) {
            mpc_sub(out, out, term, MPC_RNDNN);
            mpc_abs(magnitude, term, MPFR_RNDU);
            mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
            if (k + 1u < n) {
                mpc_mul(term, term, square, MPC_RNDNN);
                mpc_div_ui(term, term, (k + 1u) * (n - k - 1u), MPC_RNDNN);
            }
        }
    }

    mpc_log(weight, half_z, MPC_RNDNN);
    mpc_mul_2ui(weight, weight, 1u, MPC_RNDNN);
    mpc_abs(weight_bound, weight, MPFR_RNDU);
    mpfr_const_euler(scratch, MPFR_RNDN);
    mpfr_mul_2ui(scratch, scratch, 1u, MPFR_RNDN);
    mpfr_add(mpc_realref(weight), mpc_realref(weight), scratch, MPFR_RNDN);
    mpfr_add(weight_bound, weight_bound, scratch, MPFR_RNDU);
    mpfr_add_ui(weight_bound, weight_bound, 1u, MPFR_RNDU);
    for (unsigned long k = 1; k <= n; ++k) {
        mpfr_set_ui(scratch, k, MPFR_RNDN);
        mpfr_ui_div(scratch, 1u, scratch, MPFR_RNDN);
        mpfr_sub(mpc_realref(weight), mpc_realref(weight), scratch, MPFR_RNDN);
        mpfr_add(weight_bound, weight_bound, scratch, MPFR_RNDU);
    }
    mpc_pow_ui(term, half_z, n, MPC_RNDNN);
    mpfr_fac_ui(scratch, n, MPFR_RNDN);
    mpc_div_fr(term, term, scratch, MPC_RNDNN);
    for (unsigned long k = 0; k < BESSEL_Y_MAX_TERMS; ++k) {
        mpc_mul(contribution, term, weight, MPC_RNDNN);
        mpc_add(out, out, contribution, MPC_RNDNN);
        if (!bessel_y_finite(out) || !bessel_y_finite(term))
            goto done;
        mpc_abs(magnitude, term, MPFR_RNDU);
        mpfr_mul(scratch, magnitude, weight_bound, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, scratch, MPFR_RNDU);

        unsigned long next = k + 1u;
        mpfr_div_ui(ratio, square_size, next * (n + next), MPFR_RNDU);
        /* Subsequent term ratios decrease. With ratio <= 1/2 and harmonic increments <= 2,
         * the entire remaining weighted tail is at most |term| * (|weight| + 4). */
        mpc_abs(tail, weight, MPFR_RNDU);
        mpfr_add_ui(tail, tail, 4u, MPFR_RNDU);
        mpfr_mul(tail, tail, magnitude, MPFR_RNDU);
        mpc_abs(threshold, out, MPFR_RNDD);
        mpfr_mul_2si(threshold, threshold, -(long)precision - 32, MPFR_RNDD);
        if (mpfr_cmp_d(ratio, 0.5) <= 0 && mpfr_lessequal_p(tail, threshold)) {
            ok = bessel_y_resolved(out, absolute_sum, precision);
            break;
        }
        mpc_mul(term, term, square, MPC_RNDNN);
        mpc_div_ui(term, term, next * (n + next), MPC_RNDNN);
        mpc_neg(term, term, MPC_RNDNN);
        mpfr_set_ui(scratch, next, MPFR_RNDN);
        mpfr_ui_div(scratch, 1u, scratch, MPFR_RNDN);
        mpfr_sub(mpc_realref(weight), mpc_realref(weight), scratch, MPFR_RNDN);
        mpfr_add(weight_bound, weight_bound, scratch, MPFR_RNDU);
        mpfr_set_ui(scratch, n + next, MPFR_RNDN);
        mpfr_ui_div(scratch, 1u, scratch, MPFR_RNDN);
        mpfr_sub(mpc_realref(weight), mpc_realref(weight), scratch, MPFR_RNDN);
        mpfr_add(weight_bound, weight_bound, scratch, MPFR_RNDU);
    }
    mpc_div_fr(out, out, pi, MPC_RNDNN);
    ok = ok && bessel_y_finite(out);
done:
    mpfr_clears(pi, scratch, magnitude, absolute_sum, weight_bound, square_size, ratio, tail, threshold, (mpfr_ptr)0);
    mpc_clear(half_z);
    mpc_clear(square);
    mpc_clear(term);
    mpc_clear(weight);
    mpc_clear(contribution);
    return ok;
}

/* Recover J from the native arbitrary-precision I kernel without fixed-precision complex gamma.
 * Rotate towards the right half-plane, so the phase correction is always +/- i*pi/2. */
static bool bessel_y_j(mpc_ptr out, mpc_srcptr nu, mpc_srcptr z)
{
    NUM_SCOPE(scope);
    mpfr_prec_t work = mpc_get_prec(out);
    mpc_t rotated, phase;
    mpc_init2(rotated, work);
    mpc_init2(phase, work);
    int rotation = mpfr_sgn(mpc_imagref(z)) < 0 ? 1 : -1;
    mpc_mul_i(rotated, z, rotation, MPC_RNDNN);
    number_t order = number_take(number_wrap_complex_mpc(nu, (size_t)work));
    number_t argument = number_take(number_wrap_complex_mpc(rotated, (size_t)work));
    number_t value = num_bessel_i(order, argument);
    bool ok = !num_is_nan(value) && bessel_y_set_complex(out, value);
    if (ok) {
        mpc_set_ui(phase, 0u, MPC_RNDNN);
        mpfr_const_pi(mpc_imagref(phase), MPFR_RNDN);
        mpfr_div_2ui(mpc_imagref(phase), mpc_imagref(phase), 1u, MPFR_RNDN);
        if (rotation > 0)
            mpfr_neg(mpc_imagref(phase), mpc_imagref(phase), MPFR_RNDN);
        mpc_mul(phase, phase, nu, MPC_RNDNN);
        mpc_exp(phase, phase, MPC_RNDNN);
        mpc_mul(out, out, phase, MPC_RNDNN);
        ok = bessel_y_finite(out);
    }
    mpc_clear(rotated);
    mpc_clear(phase);
    return ok;
}

/* DLMF 10.2.3, with a cancellation check before dividing by sin(pi*nu). */
static bool bessel_y_connection(mpc_ptr out, mpc_srcptr nu, mpc_srcptr z, size_t precision, bool half_integral)
{
    mpfr_prec_t work = mpc_get_prec(out);
    mpc_t opposite, positive, negative, sine, cosine;
    mpfr_t pi, magnitude, absolute_sum;
    mpc_init2(opposite, work);
    mpc_init2(positive, work);
    mpc_init2(negative, work);
    mpc_init2(sine, work);
    mpc_init2(cosine, work);
    mpfr_inits2(work, pi, magnitude, absolute_sum, (mpfr_ptr)0);
    mpc_neg(opposite, nu, MPC_RNDNN);
    if (half_integral) {
        /* cos(pi*nu) is exactly zero. In particular, tiny negative-half-order values must not
         * inherit a spurious cos(pi*nu)*J_nu term which can be arbitrarily larger than Y_nu. */
        bool ok = bessel_y_j(out, opposite, z);
        mpfr_mul_2ui(pi, mpc_realref(nu), 1u, MPFR_RNDN);
        long twice = mpfr_get_si(pi, MPFR_RNDN);
        if (twice % 4 == 1 || twice % 4 == -3)
            mpc_neg(out, out, MPC_RNDNN);
        mpfr_clears(pi, magnitude, absolute_sum, (mpfr_ptr)0);
        mpc_clear(opposite);
        mpc_clear(positive);
        mpc_clear(negative);
        mpc_clear(sine);
        mpc_clear(cosine);
        return ok;
    }
    bool ok = bessel_y_j(positive, nu, z) && bessel_y_j(negative, opposite, z);
    if (ok) {
        mpfr_const_pi(pi, MPFR_RNDN);
        mpc_mul_fr(opposite, nu, pi, MPC_RNDNN);
        mpc_sin_cos(sine, cosine, opposite, MPC_RNDNN, MPC_RNDNN);
        /* Include J_nu itself when cos(pi*nu) nearly vanishes. */
        mpc_abs(absolute_sum, positive, MPFR_RNDU);
        mpc_mul(positive, positive, cosine, MPC_RNDNN);
        mpc_abs(magnitude, positive, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        mpc_abs(magnitude, negative, MPFR_RNDU);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDU);
        mpc_sub(out, positive, negative, MPC_RNDNN);
        ok = bessel_y_resolved(out, absolute_sum, precision);
        mpc_div(out, out, sine, MPC_RNDNN);
        ok = ok && bessel_y_finite(out);
    }
    mpfr_clears(pi, magnitude, absolute_sum, (mpfr_ptr)0);
    mpc_clear(opposite);
    mpc_clear(positive);
    mpc_clear(negative);
    mpc_clear(sine);
    mpc_clear(cosine);
    return ok;
}

/* Evaluate principal Bessel Y at the widest input precision, including integer and complex orders. */
number_t num_bessel_y(const number_t order, const number_t argument)
{
    NUM_SCOPE(scope);
    if (!num_is_finite(order) || !num_is_finite(argument))
        return num_clone(NUM_NAN);
    if (num_is_zero(argument)) {
        /* Y_(-m-1/2) is a signed J_(m+1/2), whose principal limit at zero vanishes. */
        if (num_is_real(order) && num_lt(order, NUM_ZERO) && num_is_integer(num_add(order, NUM_HALF)))
            return num_clone(NUM_ZERO);
        return num_clone(NUM_NAN);
    }
    size_t precision = number_cylindrical_precision(order, argument);
    double size = num_to_double(num_abs(argument)), degree = num_to_double(num_abs(order));
    bool integral = num_is_real(order) && num_is_integer(order);
    bool real_argument = num_is_real(argument);
    if (!isfinite(degree) || degree > 1000 ||
        ((!integral || !real_argument) && (!isfinite(size) || size > 1000)))
        return num_clone(NUM_NAN);
    size_t guard = 96u;
    if (!integral || !real_argument)
        guard += (size_t)ceil(4 * (size + degree));
    if (integral && real_argument && num_is_exact(argument)) {
        /* An exact large rational still needs absolute argument accuracy for phase reduction. */
        long exponent = num_get_exponent2(num_abs(argument));
        if (exponent > BESSEL_Y_MAX_WORK_BITS)
            return num_clone(NUM_NAN);
        if (exponent > 0)
            guard += (size_t)exponent;
    }
    if (!integral) {
        number_t nearest = num_floor(num_add(num_real_part(order), NUM_HALF));
        number_t distance = num_abs(num_sub(order, nearest));
        long exponent = num_get_exponent2(distance);
        if (exponent < -BESSEL_Y_MAX_WORK_BITS)
            return num_clone(NUM_NAN);
        if (exponent < 0)
            guard += (size_t)(-exponent);
    }
    if (guard >= BESSEL_Y_MAX_WORK_BITS || precision > BESSEL_Y_MAX_WORK_BITS - guard)
        return num_clone(NUM_NAN);
    mpfr_prec_t work = (mpfr_prec_t)(precision + guard);
    mpc_t nu, z, result;
    mpc_init2(nu, work);
    mpc_init2(z, work);
    mpc_init2(result, work);
    bool ok = bessel_y_set_complex(nu, order) && bessel_y_set_complex(z, argument);
    number_t value = num_clone(NUM_NAN);
    if (mpfr_zero_p(mpc_imagref(z)))
        mpfr_set_zero(mpc_imagref(z), 1);
    if (ok && integral && real_argument) {
        /* On the cut, Y_n(-x+i0) = (-1)^n (Y_n(x) + 2*i*J_n(x)). */
        long n = mpfr_get_si(mpc_realref(nu), MPFR_RNDN);
        bool negative = mpfr_sgn(mpc_realref(z)) < 0;
        mpfr_abs(mpc_realref(z), mpc_realref(z), MPFR_RNDN);
        mpfr_yn(mpc_realref(result), n, mpc_realref(z), MPFR_RNDN);
        if (negative) {
            mpfr_jn(mpc_imagref(result), n, mpc_realref(z), MPFR_RNDN);
            mpfr_mul_2ui(mpc_imagref(result), mpc_imagref(result), 1u, MPFR_RNDN);
            if (n % 2 != 0)
                mpc_neg(result, result, MPC_RNDNN);
        } else {
            mpfr_set_zero(mpc_imagref(result), 1);
        }
        ok = bessel_y_finite(result);
    } else if (ok && integral) {
        long n = mpfr_get_si(mpc_realref(nu), MPFR_RNDN);
        ok = bessel_y_integer(result, (unsigned long)(n < 0 ? -n : n), z, precision);
        if (n < 0 && n % 2 != 0)
            mpc_neg(result, result, MPC_RNDNN);
    } else if (ok) {
        bool half_integral = num_is_real(order) && num_is_integer(num_mul(NUM_TWO, order));
        ok = bessel_y_connection(result, nu, z, precision, half_integral);
    }
    if (ok) {
        if (num_is_real(order) && real_argument && num_gt(argument, NUM_ZERO))
            value = number_take_mpfr(number_mpfr_from_mpfr(mpc_realref(result), precision));
        else
            value = number_take(number_wrap_complex_mpc(result, precision));
    }
    mpc_clear(nu);
    mpc_clear(z);
    mpc_clear(result);
    return num_scope_detach(value);
}
