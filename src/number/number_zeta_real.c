/**
 * @file number_zeta_real.c
 * @brief MPFR Euler–Maclaurin evaluation of real zeta values and order derivatives.
 *
 * Keeps ordinary positive real sums, the integral and the Bernoulli recurrence in MPFR.
 * Moderate finite sums can share the number module's guarded binomial-block kernel.
 * Both outputs can share one evaluation. The correction budget and guard bits match the complex kernel;
 * unsupported real endpoints stay with its principal-branch continuation. Coefficients and shifted powers
 * use the number module's bounded caches, while all accumulators and temporary storage belong to this call.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static bool zeta_real_tail_small(mpfr_srcptr term, mpfr_prec_t target, unsigned guard)
{
    return mpfr_number_p(term) && (mpfr_zero_p(term) || mpfr_get_exp(term) <= -target - guard - 1);
}

/* |B_(2k)/(2k)!| < 4/32^k; only coefficient products lose bits, never the recurrence or accumulator. */
static mpfr_prec_t zeta_real_tail_precision(mpfr_srcptr scaled, mpfr_srcptr derivative,
                                           size_t index, mpfr_prec_t work)
{
    if (work < 640 || work > 65536 || index > 65536 || !mpfr_number_p(scaled)) return work;
    mpfr_exp_t exponent = mpfr_zero_p(scaled) ? 0 : mpfr_get_exp(scaled);
    if (derivative) {
        if (!mpfr_number_p(derivative)) return work;
        mpfr_exp_t other = mpfr_zero_p(derivative) ? 0 : mpfr_get_exp(derivative);
        if (other > exponent) exponent = other;
    }
    mpfr_exp_t adjustment = exponent - 5 * (mpfr_exp_t)index + 32;
    if (adjustment >= 0) return work;
    return adjustment < 128 - work ? 128 : work + adjustment;
}

/* Add one guarded tail, using each endpoint exponential only once. */
static int zeta_real_tail(mpfr_ptr sum, mpfr_ptr derivative, mpfr_srcptr argument, mpfr_srcptr endpoint,
                     mpfr_prec_t target, size_t budget, unsigned guard)
{
    mpfr_prec_t work = mpfr_get_prec(sum);
    mpfr_t logarithm, base, inverse, inverse_square, pole, term, scaled, scaled_derivative;
    mpfr_t factor, increment, second_difference, factor_derivative, derivative_increment, temporary;
    mpfr_t bernoulli;
    mpfr_init2(logarithm, work); mpfr_init2(base, work); mpfr_init2(inverse, work);
    mpfr_init2(inverse_square, work); mpfr_init2(pole, work); mpfr_init2(term, work);
    mpfr_init2(scaled, work); mpfr_init2(scaled_derivative, work); mpfr_init2(factor, work);
    mpfr_init2(increment, work); mpfr_init2(second_difference, work); mpfr_init2(factor_derivative, work);
    mpfr_init2(derivative_increment, work); mpfr_init2(temporary, work);
    mpfr_init2(bernoulli, work);
    int status = -1;

    mpfr_sub_ui(pole, argument, 1u, MPFR_RNDN);
    if (mpfr_zero_p(pole)) {
        mpfr_set_inf(sum, 1);
        if (derivative) {
            mpfr_set_inf(derivative, -1);
        }
        status = 0;
        goto done;
    }
    mpfr_log(logarithm, endpoint, MPFR_RNDN);
    mpfr_mul(base, argument, logarithm, MPFR_RNDN);
    mpfr_neg(base, base, MPFR_RNDN);
    mpfr_exp(base, base, MPFR_RNDN);
    mpfr_ui_div(inverse, 1u, endpoint, MPFR_RNDN);
    mpfr_sqr(inverse_square, inverse, MPFR_RNDN);

    mpfr_mul(term, base, endpoint, MPFR_RNDN);
    mpfr_div(term, term, pole, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    if (derivative) {
        mpfr_ui_div(temporary, 1u, pole, MPFR_RNDN);
        mpfr_add(temporary, temporary, logarithm, MPFR_RNDN);
        mpfr_mul(temporary, temporary, term, MPFR_RNDN);
        mpfr_sub(derivative, derivative, temporary, MPFR_RNDN);
    }
    mpfr_div_2ui(term, base, 1u, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    if (derivative) {
        mpfr_mul(temporary, term, logarithm, MPFR_RNDN);
        mpfr_sub(derivative, derivative, temporary, MPFR_RNDN);
    }

    /* U_k = (s)_(2k-1) z^(-s-2k+1), Q_k = (s+2k-1)(s+2k)/z^2. */
    mpfr_mul(base, base, inverse, MPFR_RNDN);
    mpfr_mul(scaled, base, argument, MPFR_RNDN);
    if (derivative) {
        mpfr_mul(temporary, argument, logarithm, MPFR_RNDN);
        mpfr_ui_sub(temporary, 1u, temporary, MPFR_RNDN);
        mpfr_mul(scaled_derivative, base, temporary, MPFR_RNDN);
        mpfr_mul_ui(factor_derivative, argument, 2u, MPFR_RNDN);
        mpfr_add_ui(factor_derivative, factor_derivative, 3u, MPFR_RNDN);
        mpfr_mul(factor_derivative, factor_derivative, inverse_square, MPFR_RNDN);
        mpfr_mul_ui(derivative_increment, inverse_square, 4u, MPFR_RNDN);
    }
    mpfr_add_ui(factor, argument, 1u, MPFR_RNDN);
    mpfr_add_ui(temporary, argument, 2u, MPFR_RNDN);
    mpfr_mul(factor, factor, temporary, MPFR_RNDN);
    mpfr_mul(factor, factor, inverse_square, MPFR_RNDN);
    mpfr_mul_ui(increment, argument, 4u, MPFR_RNDN);
    mpfr_add_ui(increment, increment, 10u, MPFR_RNDN);
    mpfr_mul(increment, increment, inverse_square, MPFR_RNDN);
    mpfr_mul_ui(second_difference, inverse_square, 8u, MPFR_RNDN);

    for (size_t k = 1u; k <= budget; ++k) {
        mpfr_prec_t precision = zeta_real_tail_precision(scaled, derivative ? scaled_derivative : NULL, k, work);
        if (mpfr_get_prec(bernoulli) != precision) mpfr_set_prec(bernoulli, precision);
        if (mpfr_get_prec(term) != precision) mpfr_set_prec(term, precision);
        if (number_coefficient_normalised(bernoulli, k)) break;
        mpfr_mul(term, scaled, bernoulli, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        bool small = zeta_real_tail_small(term, target, guard);
        if (derivative) {
            mpfr_mul(term, scaled_derivative, bernoulli, MPFR_RNDN);
            mpfr_add(derivative, derivative, term, MPFR_RNDN);
            small = small && zeta_real_tail_small(term, target, guard);
        }
        if (k >= 4u && small) {
            status = 0;
            break;
        }
        if (derivative) {
            mpfr_mul(temporary, scaled, factor_derivative, MPFR_RNDN);
            mpfr_mul(scaled_derivative, scaled_derivative, factor, MPFR_RNDN);
            mpfr_add(scaled_derivative, scaled_derivative, temporary, MPFR_RNDN);
            mpfr_add(factor_derivative, factor_derivative, derivative_increment, MPFR_RNDN);
        }
        mpfr_mul(scaled, scaled, factor, MPFR_RNDN);
        mpfr_add(factor, factor, increment, MPFR_RNDN);
        mpfr_add(increment, increment, second_difference, MPFR_RNDN);
    }

done:
    mpfr_clear(bernoulli);
    mpfr_clear(temporary); mpfr_clear(derivative_increment); mpfr_clear(factor_derivative);
    mpfr_clear(second_difference); mpfr_clear(increment); mpfr_clear(factor);
    mpfr_clear(scaled_derivative); mpfr_clear(scaled); mpfr_clear(term); mpfr_clear(pole);
    mpfr_clear(inverse_square); mpfr_clear(inverse); mpfr_clear(base); mpfr_clear(logarithm);
    return status;
}

/* Evaluate a real finite sum and its optional derivative before adding the shared guarded tail. */
int number_zeta_real(mpfr_ptr out, mpfr_ptr derivative, mpfr_srcptr argument, mpfr_srcptr offset,
                     size_t budget, unsigned guard)
{
    if (!mpfr_number_p(argument) || (offset && (!mpfr_number_p(offset) || mpfr_sgn(offset) <= 0))) return -1;
    mpfr_prec_t target = mpfr_get_prec(out), work = target + 128;
    unsigned long count = 128ul + (target > 512 ? (unsigned long)(target - 512) / 8ul : 0ul);
    mpfr_t sum, slope, endpoint, term;
    mpfr_inits2(work, sum, slope, endpoint, term, (mpfr_ptr)0);
    if (!offset) {
        number_zeta_finite_sum_real(sum, derivative ? slope : NULL, argument, count);
        mpfr_set_ui(endpoint, count, MPFR_RNDN);
    } else {
        mpc_t key_offset, key_exponent, value, log_value;
        mpc_init2(key_offset, work); mpc_init2(key_exponent, work);
        mpc_init2(value, work); mpc_init2(log_value, work);
        mpc_set_fr(key_offset, offset, MPC_RNDNN); mpc_set_fr(key_exponent, argument, MPC_RNDNN);
        mpfr_set_zero(sum, 1); mpfr_set_zero(slope, 1);
        bool blocked = (!mpfr_integer_p(argument) || derivative) &&
            number_zeta_shifted_sum(value, derivative ? log_value : NULL, key_exponent, key_offset, count);
        if (blocked) {
            mpfr_set(sum, mpc_realref(value), MPFR_RNDN);
            if (derivative) mpfr_set(slope, mpc_realref(log_value), MPFR_RNDN);
        }
        for (unsigned long n = 0; !blocked && n < count; ++n) {
            number_dirichlet_power(value, key_offset, key_exponent, n);
            mpfr_add(sum, sum, mpc_realref(value), MPFR_RNDN);
            if (derivative) {
                number_zeta_logarithm(log_value, key_offset, n);
                mpfr_mul(term, mpc_realref(value), mpc_realref(log_value), MPFR_RNDN);
                mpfr_sub(slope, slope, term, MPFR_RNDN);
            }
        }
        mpfr_add_ui(endpoint, offset, count, MPFR_RNDN);
        mpc_clear(log_value); mpc_clear(value); mpc_clear(key_exponent); mpc_clear(key_offset);
    }
    int status = zeta_real_tail(sum, derivative ? slope : NULL, argument, endpoint, target, budget, guard);
    if (!status) {
        mpfr_set(out, sum, MPFR_RNDN);
        if (derivative) mpfr_set(derivative, slope, MPFR_RNDN);
    }
    mpfr_clears(sum, slope, endpoint, term, (mpfr_ptr)0);
    return status;
}
