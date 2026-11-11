/**
 * @file number_zeta_tail.c
 * @brief Shared Euler–Maclaurin tails for real and complex zeta functions and their derivatives.
 *
 * Extends a finite Dirichlet sum with the integral, endpoint and Bernoulli corrections. A scaled rising product
 * advances through a quadratic recurrence, avoiding separate factorial products and negative endpoint powers.
 * Analytic differentiation advances alongside the value without division by a possibly zero rising product.
 * The owning number module supplies guarded MPC accumulators and its existing correction and accuracy budgets.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

static bool zeta_tail_small(mpc_srcptr term, mpfr_prec_t target, unsigned guard)
{
    mpfr_exp_t bound = -(mpfr_exp_t)(target + guard) - 1;
    return mpfr_number_p(mpc_realref(term)) && mpfr_number_p(mpc_imagref(term)) &&
           (mpfr_zero_p(mpc_realref(term)) || mpfr_get_exp(mpc_realref(term)) <= bound) &&
           (mpfr_zero_p(mpc_imagref(term)) || mpfr_get_exp(mpc_imagref(term)) <= bound);
}

/* Add one guarded tail, using each endpoint exponential only once. */
int number_zeta_tail(mpc_ptr sum, mpc_ptr derivative, mpc_srcptr argument, mpc_srcptr endpoint,
                     mpfr_prec_t target, size_t budget, unsigned guard)
{
    mpfr_prec_t work = mpc_get_prec(sum);
    mpc_t logarithm, base, inverse, inverse_square, pole, term, scaled, scaled_derivative;
    mpc_t factor, increment, second_difference, factor_derivative, derivative_increment, temporary;
    mpfr_t bernoulli;
    mpc_init2(logarithm, work); mpc_init2(base, work); mpc_init2(inverse, work);
    mpc_init2(inverse_square, work); mpc_init2(pole, work); mpc_init2(term, work);
    mpc_init2(scaled, work); mpc_init2(scaled_derivative, work); mpc_init2(factor, work);
    mpc_init2(increment, work); mpc_init2(second_difference, work); mpc_init2(factor_derivative, work);
    mpc_init2(derivative_increment, work); mpc_init2(temporary, work);
    mpfr_init2(bernoulli, work);
    int status = -1;

    mpc_sub_ui(pole, argument, 1u, MPC_RNDNN);
    if (mpc_cmp_si_si(pole, 0, 0) == 0) {
        mpfr_set_inf(mpc_realref(sum), 1);
        mpfr_set_zero(mpc_imagref(sum), 1);
        if (derivative) {
            mpfr_set_inf(mpc_realref(derivative), -1);
            mpfr_set_zero(mpc_imagref(derivative), 1);
        }
        status = 0;
        goto done;
    }
    mpc_log(logarithm, endpoint, MPC_RNDNN);
    mpc_mul(base, argument, logarithm, MPC_RNDNN);
    mpc_neg(base, base, MPC_RNDNN);
    mpc_exp(base, base, MPC_RNDNN);
    mpc_ui_div(inverse, 1u, endpoint, MPC_RNDNN);
    mpc_sqr(inverse_square, inverse, MPC_RNDNN);

    mpc_mul(term, base, endpoint, MPC_RNDNN);
    mpc_div(term, term, pole, MPC_RNDNN);
    mpc_add(sum, sum, term, MPC_RNDNN);
    if (derivative) {
        mpc_ui_div(temporary, 1u, pole, MPC_RNDNN);
        mpc_add(temporary, temporary, logarithm, MPC_RNDNN);
        mpc_mul(temporary, temporary, term, MPC_RNDNN);
        mpc_sub(derivative, derivative, temporary, MPC_RNDNN);
    }
    mpc_div_2ui(term, base, 1u, MPC_RNDNN);
    mpc_add(sum, sum, term, MPC_RNDNN);
    if (derivative) {
        mpc_mul(temporary, term, logarithm, MPC_RNDNN);
        mpc_sub(derivative, derivative, temporary, MPC_RNDNN);
    }

    /* U_k = (s)_(2k-1) z^(-s-2k+1), Q_k = (s+2k-1)(s+2k)/z^2. */
    mpc_mul(base, base, inverse, MPC_RNDNN);
    mpc_mul(scaled, base, argument, MPC_RNDNN);
    if (derivative) {
        mpc_mul(temporary, argument, logarithm, MPC_RNDNN);
        mpc_ui_sub(temporary, 1u, temporary, MPC_RNDNN);
        mpc_mul(scaled_derivative, base, temporary, MPC_RNDNN);
        mpc_mul_ui(factor_derivative, argument, 2u, MPC_RNDNN);
        mpc_add_ui(factor_derivative, factor_derivative, 3u, MPC_RNDNN);
        mpc_mul(factor_derivative, factor_derivative, inverse_square, MPC_RNDNN);
        mpc_mul_ui(derivative_increment, inverse_square, 4u, MPC_RNDNN);
    }
    mpc_add_ui(factor, argument, 1u, MPC_RNDNN);
    mpc_add_ui(temporary, argument, 2u, MPC_RNDNN);
    mpc_mul(factor, factor, temporary, MPC_RNDNN);
    mpc_mul(factor, factor, inverse_square, MPC_RNDNN);
    mpc_mul_ui(increment, argument, 4u, MPC_RNDNN);
    mpc_add_ui(increment, increment, 10u, MPC_RNDNN);
    mpc_mul(increment, increment, inverse_square, MPC_RNDNN);
    mpc_mul_ui(second_difference, inverse_square, 8u, MPC_RNDNN);

    for (size_t k = 1u; k <= budget; ++k) {
        if (number_coefficient_normalised(bernoulli, k)) break;
        mpc_mul_fr(term, scaled, bernoulli, MPC_RNDNN);
        mpc_add(sum, sum, term, MPC_RNDNN);
        if (derivative) {
            mpc_mul_fr(term, scaled_derivative, bernoulli, MPC_RNDNN);
            mpc_add(derivative, derivative, term, MPC_RNDNN);
        }
        if (k >= 4u && zeta_tail_small(term, target, guard)) {
            status = 0;
            break;
        }
        if (derivative) {
            mpc_mul(temporary, scaled, factor_derivative, MPC_RNDNN);
            mpc_mul(scaled_derivative, scaled_derivative, factor, MPC_RNDNN);
            mpc_add(scaled_derivative, scaled_derivative, temporary, MPC_RNDNN);
            mpc_add(factor_derivative, factor_derivative, derivative_increment, MPC_RNDNN);
        }
        mpc_mul(scaled, scaled, factor, MPC_RNDNN);
        mpc_add(factor, factor, increment, MPC_RNDNN);
        mpc_add(increment, increment, second_difference, MPC_RNDNN);
    }

done:
    mpfr_clear(bernoulli);
    mpc_clear(temporary); mpc_clear(derivative_increment); mpc_clear(factor_derivative);
    mpc_clear(second_difference); mpc_clear(increment); mpc_clear(factor);
    mpc_clear(scaled_derivative); mpc_clear(scaled); mpc_clear(term); mpc_clear(pole);
    mpc_clear(inverse_square); mpc_clear(inverse); mpc_clear(base); mpc_clear(logarithm);
    return status;
}
