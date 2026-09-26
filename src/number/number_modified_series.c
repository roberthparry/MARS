#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include <math.h>

enum { MODIFIED_MAX_WORK_BITS = 65536, MODIFIED_MAX_TERMS = 20000 };

/* Convert exact components directly at work precision, without a default-precision intermediate. */
static bool modified_set_real(mpfr_ptr out, number_t value)
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

static bool modified_set_complex(mpc_ptr out, number_t value)
{
    NUM_SCOPE(scope);
    return modified_set_real(mpc_realref(out), num_real_part(value)) &&
           modified_set_real(mpc_imagref(out), num_imag_part(value));
}

static bool modified_finite(mpc_srcptr value)
{
    return mpfr_number_p(mpc_realref(value)) && mpfr_number_p(mpc_imagref(value));
}

/* Log gamma in Re(z) >= 1. DLMF 5.11.1 with recurrence and a first-omitted-term bound at Re(w).
 * This private kernel avoids num_gamma's fixed-precision complex fallback. Bernoulli coefficients
 * use B_(2k) = (-1)^(k+1) 2 (2k)! zeta(2k)/(2*pi)^(2k). */
bool number_modified_log_gamma(mpc_ptr out, mpc_srcptr z)
{
    mpfr_prec_t work = mpc_get_prec(out);
    if (!modified_finite(z) || mpfr_cmp_ui(mpc_realref(z), 1u) < 0 || work < 64 || work > MODIFIED_MAX_WORK_BITS)
        return false;
    if (mpfr_zero_p(mpc_imagref(z))) {
        mpfr_lngamma(mpc_realref(out), mpc_realref(z), MPFR_RNDN);
        mpfr_set_zero(mpc_imagref(out), 1);
        return modified_finite(out);
    }
    mpc_t w, correction, term, ratio, scratch;
    mpfr_t pi_square, bound, bound_ratio, zeta, error, tolerance;
    mpc_init2(w, work);
    mpc_init2(correction, work);
    mpc_init2(term, work);
    mpc_init2(ratio, work);
    mpc_init2(scratch, work);
    mpfr_inits2(work, pi_square, bound, bound_ratio, zeta, error, tolerance, (mpfr_ptr)0);
    long shift = (long)(work / 2 + 16) - mpfr_get_si(mpc_realref(z), MPFR_RNDD);
    if (shift < 0)
        shift = 0;
    mpc_set_ui(correction, 0u, MPC_RNDNN);
    for (long j = 0; j < shift; ++j) {
        mpc_add_ui(w, z, (unsigned long)j, MPC_RNDNN);
        mpc_log(scratch, w, MPC_RNDNN);
        mpc_add(correction, correction, scratch, MPC_RNDNN);
    }
    mpc_add_ui(w, z, (unsigned long)shift, MPC_RNDNN);
    mpc_set(scratch, w, MPC_RNDNN);
    mpfr_sub_d(mpc_realref(scratch), mpc_realref(scratch), 0.5, MPFR_RNDN);
    mpc_log(out, w, MPC_RNDNN);
    mpc_mul(out, out, scratch, MPC_RNDNN);
    mpc_sub(out, out, w, MPC_RNDNN);
    mpfr_const_pi(pi_square, MPFR_RNDN);
    mpfr_mul_2ui(pi_square, pi_square, 1u, MPFR_RNDN);
    mpfr_log(error, pi_square, MPFR_RNDN);
    mpfr_div_2ui(error, error, 1u, MPFR_RNDN);
    mpfr_add(mpc_realref(out), mpc_realref(out), error, MPFR_RNDN);
    mpfr_sqr(pi_square, pi_square, MPFR_RNDN);
    mpc_mul_fr(term, w, pi_square, MPC_RNDNN);
    mpc_ui_div(term, 2u, term, MPC_RNDNN);
    mpc_sqr(ratio, w, MPC_RNDNN);
    mpc_mul_fr(ratio, ratio, pi_square, MPC_RNDNN);
    mpc_ui_div(ratio, 1u, ratio, MPC_RNDNN);
    mpc_neg(ratio, ratio, MPC_RNDNN);
    mpfr_mul(bound, mpc_realref(w), pi_square, MPFR_RNDN);
    mpfr_ui_div(bound, 2u, bound, MPFR_RNDN);
    mpfr_sqr(bound_ratio, mpc_realref(w), MPFR_RNDN);
    mpfr_mul(bound_ratio, bound_ratio, pi_square, MPFR_RNDN);
    mpfr_ui_div(bound_ratio, 1u, bound_ratio, MPFR_RNDN);
    mpfr_set_ui_2exp(tolerance, 1u, -work + 16, MPFR_RNDN);
    bool converged = false;
    for (unsigned long k = 1; k <= (unsigned long)work; ++k) {
        mpfr_zeta_ui(zeta, 2u * k, MPFR_RNDN);
        mpfr_mul(error, bound, zeta, MPFR_RNDN);
        if (mpfr_less_p(error, tolerance)) {
            converged = true;
            break;
        }
        mpc_mul_fr(scratch, term, zeta, MPC_RNDNN);
        mpc_add(out, out, scratch, MPC_RNDNN);
        unsigned long factor = (2u * k) * (2u * k - 1u);
        mpc_mul(term, term, ratio, MPC_RNDNN);
        mpc_mul_ui(term, term, factor, MPC_RNDNN);
        mpfr_mul(bound, bound, bound_ratio, MPFR_RNDN);
        mpfr_mul_ui(bound, bound, factor, MPFR_RNDN);
    }
    mpc_sub(out, out, correction, MPC_RNDNN);
    converged = converged && modified_finite(out);
    mpfr_clears(pi_square, bound, bound_ratio, zeta, error, tolerance, (mpfr_ptr)0);
    mpc_clear(w);
    mpc_clear(correction);
    mpc_clear(term);
    mpc_clear(ratio);
    mpc_clear(scratch);
    return converged;
}

/* DLMF 10.25.2, 11.2.1 and 11.2.2, normalised where both gamma arguments have positive real parts.
 * Sum (z/2)^(nu+power_offset+2k)/(Gamma(k+a) Gamma(k+nu+a)), with a=1 for I and a=3/2 for L/H.
 * H additionally includes (-1)^k; its leading power always uses the original argument's branch.
 * Backwards multiplication crosses reciprocal-gamma zeros without dividing by a pole. */
static bool modified_series(mpc_ptr out, mpc_srcptr nu, mpc_srcptr z, size_t precision,
                            double gamma_offset, unsigned power_offset, bool alternating)
{
    mpfr_prec_t work = mpc_get_prec(out);
    mpc_t half_z, square, term, sum, denominator, gamma_argument, logarithm, prefactor;
    mpfr_t absolute_sum, magnitude, square_size, ratio, threshold, tolerance, scratch;
    mpc_init2(half_z, work);
    mpc_init2(square, work);
    mpc_init2(term, work);
    mpc_init2(sum, work);
    mpc_init2(denominator, work);
    mpc_init2(gamma_argument, work);
    mpc_init2(logarithm, work);
    mpc_init2(prefactor, work);
    mpfr_inits2(work, absolute_sum, magnitude, square_size, ratio, threshold, tolerance, scratch, (mpfr_ptr)0);
    bool ok = false;
    mpfr_neg(scratch, mpc_realref(nu), MPFR_RNDN);
    mpfr_sub_d(scratch, scratch, gamma_offset - 1.0, MPFR_RNDN);
    long start = mpfr_sgn(scratch) > 0 ? mpfr_get_si(scratch, MPFR_RNDU) : 0;
    mpc_div_2ui(half_z, z, 1u, MPC_RNDNN);
    mpc_sqr(square, half_z, MPC_RNDNN);
    if (alternating)
        mpc_neg(square, square, MPC_RNDNN);
    mpc_abs(square_size, square, MPFR_RNDN);
    if (!mpfr_number_p(square_size) || mpfr_zero_p(square_size))
        goto done;
    mpc_set_ui(term, 1u, MPC_RNDNN);
    mpc_set_ui(sum, 1u, MPC_RNDNN);
    mpfr_set_ui(absolute_sum, 1u, MPFR_RNDN);
    for (long k = start; k > 0; --k) {
        mpc_add_ui(denominator, nu, (unsigned long)k, MPC_RNDNN);
        mpfr_add_d(mpc_realref(denominator), mpc_realref(denominator), gamma_offset - 1.0, MPFR_RNDN);
        mpc_mul_ui(denominator, denominator, (unsigned long)(2 * k + power_offset), MPC_RNDNN);
        mpc_div_2ui(denominator, denominator, 1u, MPC_RNDNN);
        if (mpfr_zero_p(mpc_realref(denominator)) && mpfr_zero_p(mpc_imagref(denominator)))
            break;
        mpc_mul(term, term, denominator, MPC_RNDNN);
        mpc_div(term, term, square, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        mpc_abs(magnitude, term, MPFR_RNDN);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDN);
        if (mpfr_zero_p(magnitude))
            goto done;
    }
    mpc_set_ui(term, 1u, MPC_RNDNN);
    mpfr_set_ui_2exp(tolerance, 1u, -(long)precision - 32, MPFR_RNDN);
    for (long k = start + 1; k <= MODIFIED_MAX_TERMS; ++k) {
        mpc_add_ui(denominator, nu, (unsigned long)k, MPC_RNDNN);
        mpfr_add_d(mpc_realref(denominator), mpc_realref(denominator), gamma_offset - 1.0, MPFR_RNDN);
        mpc_mul_ui(denominator, denominator, (unsigned long)(2 * k + power_offset), MPC_RNDNN);
        mpc_div_2ui(denominator, denominator, 1u, MPC_RNDNN);
        mpc_mul(term, term, square, MPC_RNDNN);
        mpc_div(term, term, denominator, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (!modified_finite(sum) || !modified_finite(term))
            goto done;
        mpc_abs(magnitude, term, MPFR_RNDN);
        mpfr_add(absolute_sum, absolute_sum, magnitude, MPFR_RNDN);
        mpc_abs(ratio, denominator, MPFR_RNDN);
        mpfr_div(ratio, square_size, ratio, MPFR_RNDN);
        mpc_abs(threshold, sum, MPFR_RNDN);
        mpfr_mul(threshold, threshold, tolerance, MPFR_RNDN);
        /* Subsequent ratios decrease: a ratio <= 1/2 bounds the entire remaining tail by this term. */
        if (mpfr_cmp_d(ratio, 0.5) <= 0 && mpfr_lessequal_p(magnitude, threshold)) {
            ok = true;
            break;
        }
    }
    if (!ok)
        goto done;
    /* Reject cancellation that consumes the guard bits, including unresolved zeros. */
    mpfr_mul_2si(scratch, absolute_sum, -work + 32, MPFR_RNDN);
    mpc_abs(threshold, sum, MPFR_RNDN);
    mpfr_mul_2si(threshold, threshold, -(long)precision - 16, MPFR_RNDN);
    ok = mpfr_lessequal_p(scratch, threshold);
    if (!ok)
        goto done;
    mpc_add_ui(gamma_argument, nu, (unsigned long)start, MPC_RNDNN);
    mpfr_add_d(mpc_realref(gamma_argument), mpc_realref(gamma_argument), gamma_offset, MPFR_RNDN);
    ok = number_modified_log_gamma(logarithm, gamma_argument);
    if (!ok)
        goto done;
    mpfr_set_d(scratch, (double)start + gamma_offset, MPFR_RNDN);
    mpfr_lngamma(scratch, scratch, MPFR_RNDN);
    mpfr_add(mpc_realref(logarithm), mpc_realref(logarithm), scratch, MPFR_RNDN);
    mpc_add_ui(gamma_argument, nu, (unsigned long)(2 * start + power_offset), MPC_RNDNN);
    mpc_log(prefactor, half_z, MPC_RNDNN);
    mpc_mul(prefactor, prefactor, gamma_argument, MPC_RNDNN);
    mpc_sub(prefactor, prefactor, logarithm, MPC_RNDNN);
    mpc_exp(prefactor, prefactor, MPC_RNDNN);
    if (alternating && start % 2 != 0)
        mpc_neg(prefactor, prefactor, MPC_RNDNN);
    mpc_mul(out, prefactor, sum, MPC_RNDNN);
    ok = modified_finite(out) && !(mpfr_zero_p(mpc_realref(out)) && mpfr_zero_p(mpc_imagref(out)));
done:
    mpfr_clears(absolute_sum, magnitude, square_size, ratio, threshold, tolerance, scratch, (mpfr_ptr)0);
    mpc_clear(half_z);
    mpc_clear(square);
    mpc_clear(term);
    mpc_clear(sum);
    mpc_clear(denominator);
    mpc_clear(gamma_argument);
    mpc_clear(logarithm);
    mpc_clear(prefactor);
    return ok;
}

/* Shared principal cylinder series, retaining the widest input precision. */
number_t number_modified_series(number_t order, number_t argument, number_modified_family_t family)
{
    NUM_SCOPE(scope);
    bool struve = family != NUMBER_MODIFIED_BESSEL_I;
    bool alternating = family == NUMBER_MODIFIED_STRUVE_H;
    double gamma_offset = struve ? 1.5 : 1.0;
    unsigned power_offset = struve ? 1u : 0u;
    if (!num_is_finite(order) || !num_is_finite(argument))
        return num_clone(NUM_NAN);
    double size = num_to_double(num_abs(argument)), degree = num_to_double(num_abs(order));
    if (!isfinite(size) || !isfinite(degree) || size > 1000 || degree > 1000)
        return num_clone(NUM_NAN);
    size_t precision = number_cylindrical_precision(order, argument);
    size_t guard = 96u + (size_t)ceil(4 * (size + degree));
    /* Exact rational orders can lie much closer to a shifted coefficient's zero than the requested
     * output precision. Retain that distance before any conversion or addition of a half-integer. */
    if (num_le(num_real_part(order), NUM_ZERO)) {
        number_t twice = num_mul(NUM_TWO, order);
        number_t nearest = num_floor(num_add(num_real_part(twice), NUM_HALF));
        number_t distance = num_abs(num_sub(twice, nearest));
        if (!num_is_zero(distance)) {
            long exponent = num_get_exponent2(distance);
            if (exponent < -MODIFIED_MAX_WORK_BITS)
                return num_clone(NUM_NAN);
            if (exponent < 0)
                guard += (size_t)(-exponent);
        }
    }
    if (guard >= MODIFIED_MAX_WORK_BITS || precision > MODIFIED_MAX_WORK_BITS - guard)
        return num_clone(NUM_NAN);
    if (num_is_zero(argument)) {
        number_t first_power = num_add(order, struve ? NUM_ONE : NUM_ZERO);
        number_t pole = num_add(order, struve ? NUM_ONE_AND_HALF : NUM_ONE);
        if (num_gt(num_real_part(first_power), NUM_ZERO) ||
            (num_is_real(pole) && num_is_integer(pole) && num_le(pole, NUM_ZERO)))
            return num_clone(NUM_ZERO);
        if (num_is_zero(first_power))
            return struve ? num_scope_detach(num_div(NUM_TWO, num_const_prec(NUM_PI, precision))) : num_clone(NUM_ONE);
        return num_clone(NUM_NAN);
    }
    mpc_t nu, z, result;
    mpc_init2(nu, (mpfr_prec_t)(precision + guard));
    mpc_init2(z, (mpfr_prec_t)(precision + guard));
    mpc_init2(result, (mpfr_prec_t)(precision + guard));
    number_t value = num_clone(NUM_NAN);
    bool ok = modified_set_complex(nu, order) && modified_set_complex(z, argument);
    bool real_result = num_is_real(order) && num_is_real(argument) &&
                       (num_gt(argument, NUM_ZERO) || num_is_integer(order));
    /* Exact points on the cut consistently use arg(z) = +pi, irrespective of signed zero. */
    if (mpfr_zero_p(mpc_imagref(z)))
        mpfr_set_zero(mpc_imagref(z), 1);
    if (ok && modified_series(result, nu, z, precision, gamma_offset, power_offset, alternating)) {
        if (real_result)
            value = number_take_mpfr(number_mpfr_from_mpfr(mpc_realref(result), precision));
        else
            value = number_take(number_wrap_complex_mpc(result, precision));
    }
    mpc_clear(nu);
    mpc_clear(z);
    mpc_clear(result);
    return num_scope_detach(value);
}
