/**
 * @file number_bessel.c
 * @brief Principal Bessel K and Y evaluation for real and complex orders.
 *
 * Evaluates the modified Bessel K family, including the supported order-dependent branches. It supplies a
 * decaying-solution counterpart to Bessel I while respecting this numeric layer's domain. Integer orders use
 * local MPC scratch values for their harmonic series and recurrence, avoiding per-term numeric wrapper allocation.
 *
 * Evaluates the Bessel function of the second kind using the supported order and argument paths. Integer and
 * non-integer order behaviour must remain consistent with the layer's real or complex domain.
 *
 * This belongs to number_t's shared scalar layer. Preserve exactness, precision policy and the distinction between
 * borrowed aliases and independently owned numeric values.
 */
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static void replace_value(number_t *value, number_t next)
{
    num_destroy(value);
    *value = next;
}

/* Explicitly promote both Cartesian components: merely tagging an exact complex value can leave
 * later transcendental dispatch on a fixed-precision component. */
static number_t working_value(number_t value, size_t precision)
{
    NUM_SCOPE(scope);
    if (num_is_real(value))
        return num_scope_detach(num_as_inexact_real_prec(num_real_part(value), precision));
    number_t real = num_as_inexact_real_prec(num_real_part(value), precision);
    number_t imag = num_as_inexact_real_prec(num_imag_part(value), precision);
    complex_t *z = number_complex_create(real, imag);
    if (!z)
        return num_clone(NUM_NAN);
    number_t result = number_take(number_wrap_complex(z));
    num_set_prec_bits(&result, precision);
    return num_scope_detach(result);
}

static bool small_term(number_t term, number_t sum, number_t tolerance)
{
    NUM_SCOPE(scope);
    return num_le(num_abs(term), num_mul(tolerance, num_add(NUM_ONE, num_abs(sum))));
}

/* Convergent I-series with the leading power and gamma factor removed. */
static number_t reduced_i_series(number_t order, number_t square, number_t tolerance)
{
    NUM_SCOPE(scope);
    number_t term = num_clone(NUM_ONE), sum = num_clone(NUM_ONE);
    for (long k = 1; k <= 20000; ++k) {
        number_t index = num_create_from_long(k);
        replace_value(&term, num_div(num_mul(term, square), num_mul(index, num_add(index, order))));
        replace_value(&sum, num_add(sum, term));
        if (!num_is_finite(sum))
            return num_clone(NUM_NAN);
        if (small_term(term, sum, tolerance))
            return num_scope_detach(sum);
    }
    return num_clone(NUM_NAN);
}

/* Integer orders avoid the removable singularity in the connection formula for I_nu and I_-nu. */
static bool integer_k_small(mpc_srcptr term, mpc_srcptr sum, mpfr_srcptr tolerance,
                            mpfr_ptr magnitude, mpfr_ptr threshold)
{
    number_mpc_abs_upper(magnitude, term);
    number_mpc_abs_lower(threshold, sum);
    mpfr_add_ui(threshold, threshold, 1u, MPFR_RNDD);
    mpfr_mul(threshold, threshold, tolerance, MPFR_RNDD);
    return mpfr_lessequal_p(magnitude, threshold);
}

static number_t integer_k(long order, number_t argument, size_t precision)
{
    mpc_t z, square, logarithm, term, i0, weighted, derivative, contribution, differentiated;
    mpc_t previous, current, next, scratch;
    mpc_init2(z, precision); mpc_init2(square, precision); mpc_init2(logarithm, precision);
    mpc_init2(term, precision); mpc_init2(i0, precision); mpc_init2(weighted, precision);
    mpc_init2(derivative, precision); mpc_init2(contribution, precision); mpc_init2(differentiated, precision);
    mpc_init2(previous, precision); mpc_init2(current, precision); mpc_init2(next, precision);
    mpc_init2(scratch, precision);
    mpfr_t harmonic, coefficient, tolerance, magnitude, threshold;
    mpfr_inits2(precision, harmonic, coefficient, tolerance, magnitude, threshold, (mpfr_ptr)0);
    number_t result = number_invalid();
    number_t *promoted = number_coerce(&argument, NUMBER_COMPLEX);
    bool ready = promoted && !number_complex_get_mpc(z, number_impl_const(promoted)->value.cx, precision);
    number_box_free(promoted);
    if (!ready) goto done;
    mpc_div_2ui(square, z, 1u, MPC_RNDNN);
    mpc_log(logarithm, square, MPC_RNDNN);
    mpfr_const_euler(coefficient, MPFR_RNDN);
    mpc_add_fr(logarithm, logarithm, coefficient, MPC_RNDNN);
    mpc_sqr(square, square, MPC_RNDNN);
    mpc_set_ui(term, 1u, MPC_RNDNN); mpc_set_ui(i0, 1u, MPC_RNDNN);
    mpc_set_ui(weighted, 0u, MPC_RNDNN); mpc_set_ui(derivative, 0u, MPC_RNDNN);
    mpfr_set_zero(harmonic, 1);
    mpfr_set_ui_2exp(tolerance, 1u, -(long)precision + 16, MPFR_RNDN);
    bool converged = false;
    for (long k = 1; k <= 20000; ++k) {
        mpfr_set_ui(coefficient, (unsigned long)k, MPFR_RNDN);
        mpfr_ui_div(coefficient, 1u, coefficient, MPFR_RNDN);
        mpfr_add(harmonic, harmonic, coefficient, MPFR_RNDN);
        mpc_mul(term, term, square, MPC_RNDNN);
        mpc_div_ui(term, term, (unsigned long)k, MPC_RNDNN);
        mpc_div_ui(term, term, (unsigned long)k, MPC_RNDNN);
        mpc_add(i0, i0, term, MPC_RNDNN);
        mpc_mul_fr(contribution, term, harmonic, MPC_RNDNN);
        mpc_add(weighted, weighted, contribution, MPC_RNDNN);
        mpc_sub_fr(scratch, logarithm, harmonic, MPC_RNDNN);
        mpc_mul(differentiated, term, scratch, MPC_RNDNN);
        mpc_mul_ui(differentiated, differentiated, (unsigned long)k, MPC_RNDNN);
        mpc_add(derivative, derivative, differentiated, MPC_RNDNN);
        if (!mpfr_number_p(mpc_realref(i0)) || !mpfr_number_p(mpc_imagref(i0))) goto done;
        if (integer_k_small(term, i0, tolerance, magnitude, threshold) &&
            integer_k_small(contribution, weighted, tolerance, magnitude, threshold) &&
            integer_k_small(differentiated, derivative, tolerance, magnitude, threshold)) {
            converged = true;
            break;
        }
    }
    if (!converged) goto done;
    mpc_mul(previous, logarithm, i0, MPC_RNDNN);
    mpc_sub(previous, weighted, previous, MPC_RNDNN);
    if (!order) {
        number_t *wrapped = number_wrap_complex_mpc(previous, precision);
        if (wrapped) result = number_take(wrapped);
        goto done;
    }
    mpc_mul_2ui(current, derivative, 1u, MPC_RNDNN);
    mpc_add(current, current, i0, MPC_RNDNN);
    mpc_div(current, current, z, MPC_RNDNN);
    for (long n = 1; n < order; ++n) {
        mpc_mul_ui(next, current, (unsigned long)(2 * n), MPC_RNDNN);
        mpc_div(next, next, z, MPC_RNDNN);
        mpc_add(next, next, previous, MPC_RNDNN);
        mpc_swap(previous, current); mpc_swap(current, next);
    }
    number_t *wrapped = number_wrap_complex_mpc(current, precision);
    if (wrapped) result = number_take(wrapped);
done:
    mpfr_clears(harmonic, coefficient, tolerance, magnitude, threshold, (mpfr_ptr)0);
    mpc_clear(z); mpc_clear(square); mpc_clear(logarithm); mpc_clear(term); mpc_clear(i0);
    mpc_clear(weighted); mpc_clear(derivative); mpc_clear(contribution); mpc_clear(differentiated);
    mpc_clear(previous); mpc_clear(current); mpc_clear(next); mpc_clear(scratch);
    return result;
}

/* Evaluate the principal modified Bessel K with guard precision for cancellation in its convergent series. */
number_t num_bessel_K(const number_t order, const number_t argument)
{
    NUM_SCOPE(scope);
    if (!num_is_finite(order) || !num_is_finite(argument) || num_is_zero(argument))
        return number_scope_result(&scope, num_clone(NUM_NAN));
    double size = num_to_double(num_abs(argument)), degree = num_to_double(num_abs(order));
    if (!isfinite(size) || !isfinite(degree) || size > 1000 || degree > 1000)
        return number_scope_result(&scope, num_clone(NUM_NAN));
    size_t precision = number_cylindrical_precision(order, argument);
    size_t work = precision + 96u + (size_t)ceil(4*(size+degree));
    bool integral = num_is_real(order) && num_is_integer(order);
    /* Near integral orders the two gamma terms cancel; retain the distance's significant bits. */
    if (!integral) {
        number_t nearest = num_floor(num_add(num_real_part(order), NUM_HALF));
        number_t distance = num_abs(num_sub(order, nearest));
        double lost = -num_to_double(num_log(distance))/log(2.0);
        if (!isfinite(lost) || lost > 65536)
            return number_scope_result(&scope, num_clone(NUM_NAN));
        if (lost > 0)
            work += (size_t)ceil(lost);
    }
    number_t result;
    if (integral) {
        result = integer_k((long)degree, argument, work);
    } else {
        number_t z = working_value(argument, work), nu = working_value(order, work);
        number_t half_z = working_value(num_mul(NUM_HALF, z), work);
        number_t square = working_value(num_mul(half_z, half_z), work);
        number_t tolerance = num_pow(NUM_TWO, num_create_from_long(-(long)work+16));
        number_t minus = num_neg(nu);
        number_t left = num_mul(num_gamma(nu), num_pow(half_z, minus));
        number_t right = num_mul(num_gamma(minus), num_pow(half_z, nu));
        number_t left_series = reduced_i_series(minus, square, tolerance);
        number_t right_series = reduced_i_series(nu, square, tolerance);

        left = num_mul(left, left_series);
        right = num_mul(right, right_series);
        num_destroy(&right_series);
        num_destroy(&left_series);
        result = num_mul(NUM_HALF, num_add(left, right));
        num_destroy(&square); num_destroy(&half_z); num_destroy(&nu); num_destroy(&z);
    }
    if (num_is_real(order) && num_is_real(argument) && num_gt(argument, NUM_ZERO))
        replace_value(&result, num_real_part(result));
    num_set_prec_bits(&result, precision);
    return number_scope_result(&scope, result);
}

enum { BESSEL_Y_MAX_WORK_BITS = 65536, BESSEL_Y_MAX_TERMS = 20000 };

/* Exact components must reach the working precision before any inexact conversion. */
static bool bessel_Y_set_real(mpfr_ptr out, number_t value)
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

static bool bessel_Y_set_complex(mpc_ptr out, number_t value)
{
    NUM_SCOPE(scope);
    return bessel_Y_set_real(mpc_realref(out), num_real_part(value)) &&
           bessel_Y_set_real(mpc_imagref(out), num_imag_part(value));
}

static bool bessel_Y_finite(mpc_srcptr value)
{
    return mpfr_number_p(mpc_realref(value)) && mpfr_number_p(mpc_imagref(value));
}

/* A conservative accumulated-roundoff allowance must fit well inside the requested precision. */
static bool bessel_Y_resolved(mpc_srcptr value, mpfr_srcptr absolute_sum, size_t precision)
{
    mpfr_prec_t work = mpc_get_prec(value);
    mpfr_t error, threshold;
    mpfr_inits2(work, error, threshold, (mpfr_ptr)0);
    mpfr_mul_2si(error, absolute_sum, -work + 32, MPFR_RNDU);
    mpc_abs(threshold, value, MPFR_RNDD);
    mpfr_mul_2si(threshold, threshold, -(long)precision - 16, MPFR_RNDD);
    bool ok = bessel_Y_finite(value) && mpfr_lessequal_p(error, threshold);
    mpfr_clears(error, threshold, (mpfr_ptr)0);
    return ok;
}

/* DLMF 10.8.1: combine the logarithmic J series with its harmonic-number weights. */
static bool bessel_Y_integer(mpc_ptr out, unsigned long n, mpc_srcptr z, size_t precision)
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
        if (!bessel_Y_finite(out) || !bessel_Y_finite(term))
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
            ok = bessel_Y_resolved(out, absolute_sum, precision);
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
    ok = ok && bessel_Y_finite(out);
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
static bool bessel_Y_j(mpc_ptr out, mpc_srcptr nu, mpc_srcptr z)
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
    number_t value = num_bessel_I(order, argument);
    bool ok = !num_is_nan(value) && bessel_Y_set_complex(out, value);
    if (ok) {
        mpc_set_ui(phase, 0u, MPC_RNDNN);
        mpfr_const_pi(mpc_imagref(phase), MPFR_RNDN);
        mpfr_div_2ui(mpc_imagref(phase), mpc_imagref(phase), 1u, MPFR_RNDN);
        if (rotation > 0)
            mpfr_neg(mpc_imagref(phase), mpc_imagref(phase), MPFR_RNDN);
        mpc_mul(phase, phase, nu, MPC_RNDNN);
        mpc_exp(phase, phase, MPC_RNDNN);
        mpc_mul(out, out, phase, MPC_RNDNN);
        ok = bessel_Y_finite(out);
    }
    mpc_clear(rotated);
    mpc_clear(phase);
    return ok;
}

/* DLMF 10.2.3, with a cancellation check before dividing by sin(pi*nu). */
static bool bessel_Y_connection(mpc_ptr out, mpc_srcptr nu, mpc_srcptr z, size_t precision, bool half_integral)
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
        bool ok = bessel_Y_j(out, opposite, z);
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
    bool ok = bessel_Y_j(positive, nu, z) && bessel_Y_j(negative, opposite, z);
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
        ok = bessel_Y_resolved(out, absolute_sum, precision);
        mpc_div(out, out, sine, MPC_RNDNN);
        ok = ok && bessel_Y_finite(out);
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
number_t num_bessel_Y(const number_t order, const number_t argument)
{
    NUM_SCOPE(scope);
    if (!num_is_finite(order) || !num_is_finite(argument))
        return number_scope_result(&scope, num_clone(NUM_NAN));
    if (num_is_zero(argument)) {
        /* Y_(-m-1/2) is a signed J_(m+1/2), whose principal limit at zero vanishes. */
        if (num_is_real(order) && num_lt(order, NUM_ZERO) && num_is_integer(num_add(order, NUM_HALF)))
            return number_scope_result(&scope, num_clone(NUM_ZERO));
        return number_scope_result(&scope, num_clone(NUM_NAN));
    }
    size_t precision = number_cylindrical_precision(order, argument);
    double size = num_to_double(num_abs(argument)), degree = num_to_double(num_abs(order));
    bool integral = num_is_real(order) && num_is_integer(order);
    bool real_argument = num_is_real(argument);
    if (!isfinite(degree) || degree > 1000 ||
        ((!integral || !real_argument) && (!isfinite(size) || size > 1000)))
        return number_scope_result(&scope, num_clone(NUM_NAN));
    size_t guard = 96u;
    if (!integral || !real_argument)
        guard += (size_t)ceil(4 * (size + degree));
    if (integral && real_argument && num_is_exact(argument)) {
        /* An exact large rational still needs absolute argument accuracy for phase reduction. */
        long exponent = num_get_exponent2(num_abs(argument));
        if (exponent > BESSEL_Y_MAX_WORK_BITS)
            return number_scope_result(&scope, num_clone(NUM_NAN));
        if (exponent > 0)
            guard += (size_t)exponent;
    }
    if (!integral) {
        number_t nearest = num_floor(num_add(num_real_part(order), NUM_HALF));
        number_t distance = num_abs(num_sub(order, nearest));
        long exponent = num_get_exponent2(distance);
        if (exponent < -BESSEL_Y_MAX_WORK_BITS)
            return number_scope_result(&scope, num_clone(NUM_NAN));
        if (exponent < 0)
            guard += (size_t)(-exponent);
    }
    if (guard >= BESSEL_Y_MAX_WORK_BITS || precision > BESSEL_Y_MAX_WORK_BITS - guard)
        return number_scope_result(&scope, num_clone(NUM_NAN));
    mpfr_prec_t work = (mpfr_prec_t)(precision + guard);
    mpc_t nu, z, result;
    mpc_init2(nu, work);
    mpc_init2(z, work);
    mpc_init2(result, work);
    bool ok = bessel_Y_set_complex(nu, order) && bessel_Y_set_complex(z, argument);
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
        ok = bessel_Y_finite(result);
    } else if (ok && integral) {
        long n = mpfr_get_si(mpc_realref(nu), MPFR_RNDN);
        ok = bessel_Y_integer(result, (unsigned long)(n < 0 ? -n : n), z, precision);
        if (n < 0 && n % 2 != 0)
            mpc_neg(result, result, MPC_RNDNN);
    } else if (ok) {
        bool half_integral = num_is_real(order) && num_is_integer(num_mul(NUM_TWO, order));
        ok = bessel_Y_connection(result, nu, z, precision, half_integral);
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
    return number_scope_result(&scope, value);
}
