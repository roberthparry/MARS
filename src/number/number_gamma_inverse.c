/**
 * @file number_gamma_inverse.c
 * @brief Progressive-precision real and complex inverse gamma refinement.
 *
 * Preserves the established fixed-precision seed and its real branch selection, then refines the logarithmic
 * defining equation with Newton iteration. Early stages use small significands; precision grows only after the
 * previous stage supplies half-stage accuracy. The Newton slope needs only half-stage precision plus guard bits;
 * the small residual multiplies its rounding error. The final stage still checks both its correction and the
 * full-precision logarithmic residual. Seeds are never accepted as full-precision answers. This implementation
 * is private to the number module.
 */
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Work backwards from the final precision so its penultimate stage is about half-sized, never almost full-sized. */
static mpfr_prec_t inverse_gamma_initial_precision(mpfr_prec_t final)
{
    while (final > 128) final = (final + 1) / 2;
    return final;
}

static bool inverse_gamma_real_residual(mpfr_srcptr residual, mpfr_srcptr logarithm, mpfr_prec_t target,
                                       mpfr_ptr magnitude, mpfr_ptr bound)
{
    mpfr_abs(magnitude, residual, MPFR_RNDU);
    mpfr_abs(bound, logarithm, MPFR_RNDD);
    if (mpfr_cmp_ui(bound, 1u) < 0) mpfr_set_ui(bound, 1u, MPFR_RNDN);
    mpfr_mul_2si(bound, bound, -target - 8, MPFR_RNDD);
    return mpfr_lessequal_p(magnitude, bound);
}

static bool inverse_gamma_complex_residual(mpc_srcptr residual, mpc_srcptr logarithm, mpfr_prec_t target,
                                          mpfr_ptr magnitude, mpfr_ptr bound)
{
    number_mpc_abs_upper(magnitude, residual);
    number_mpc_abs_lower(bound, logarithm);
    if (mpfr_cmp_ui(bound, 1u) < 0) mpfr_set_ui(bound, 1u, MPFR_RNDN);
    mpfr_mul_2si(bound, bound, -target - 16, MPFR_RNDD);
    return mpfr_lessequal_p(magnitude, bound);
}

/* Refine the existing real inverse-gamma branch without doing early iterations at the final precision. */
int number_mpfr_gammainv_mut(mpfr_t value)
{
    mpfr_prec_t target = mpfr_get_prec(value), final = target + 96;
    if (!mpfr_number_p(value) || mpfr_sgn(value) <= 0) { mpfr_set_nan(value); return 0; }
    mpfr_t source, minimum, x, log_y, log_gamma, derivative, residual, step, bound;
    mpfr_inits2(final, source, minimum, (mpfr_ptr)0);
    mpfr_set(source, value, MPFR_RNDN);
    mpfr_set_str(minimum, "0.885603194410888700278815900582588733207951533669903448871200165", 10, MPFR_RNDN);
    if (mpfr_cmp(source, minimum) < 0) {
        mpfr_set_nan(value);
        mpfr_clears(source, minimum, (mpfr_ptr)0);
        return 0;
    }
    mpfr_prec_t stage = inverse_gamma_initial_precision(final);
    mpfr_inits2(stage, x, log_y, log_gamma, derivative, residual, step, bound, (mpfr_ptr)0);
    mpfr_set_prec(derivative, stage / 2 + 32);
    qfloat_t seed = qf_gammainv(qf_from_double(mpfr_get_d(source, MPFR_RNDN)));
    double initial = qf_to_double(seed);
    if (isfinite(initial) && initial > 0) mpfr_set_d(x, initial, MPFR_RNDN);
    else if (mpfr_cmp_ui(source, 1u) <= 0) mpfr_set_ui(x, 1u, MPFR_RNDN);
    else {
        mpfr_log(x, source, MPFR_RNDN);
        if (mpfr_cmp_d(x, 1.5) < 0) mpfr_set_d(x, 1.5, MPFR_RNDN);
    }
    bool complete = false;
    for (;;) {
        mpfr_log(log_y, source, MPFR_RNDN);
        /* One Newton correction approximately doubles the known bits. Intermediate stages need only show
         * half-stage input accuracy before that correction; the final stage retains both original checks. */
        mpfr_prec_t accuracy = stage == final ? target + 8 : stage / 2 - 16;
        bool converged = false;
        for (unsigned i = 0; i < 64; ++i) {
            mpfr_lngamma(log_gamma, x, MPFR_RNDN);
            mpfr_sub(residual, log_gamma, log_y, MPFR_RNDN);
            mpfr_digamma(derivative, x, MPFR_RNDN);
            if (mpfr_zero_p(derivative) || !mpfr_number_p(derivative)) break;
            mpfr_div(step, residual, derivative, MPFR_RNDN);
            mpfr_abs(bound, step, MPFR_RNDN);
            bool small = mpfr_zero_p(bound) || mpfr_cmp_ui_2exp(bound, 1u, -(long)accuracy) <= 0;
            if (small && stage == final &&
                inverse_gamma_real_residual(residual, log_y, target, log_gamma, bound)) {
                /* This value already passed both checks. Keep it, rather than apply an immaterial correction
                 * and pay for another full-precision logarithmic gamma solely to verify the changed value. */
                complete = converged = true;
                break;
            }
            mpfr_sub(x, x, step, MPFR_RNDN);
            if (!mpfr_number_p(x) || mpfr_sgn(x) <= 0) break;
            if (small) {
                converged = true;
                break;
            }
        }
        if (!converged) break;
        if (stage == final) {
            if (complete) break;
            mpfr_lngamma(log_gamma, x, MPFR_RNDN);
            mpfr_sub(residual, log_gamma, log_y, MPFR_RNDN);
            complete = inverse_gamma_real_residual(residual, log_y, target, log_gamma, bound);
            break;
        }
        stage = stage > final / 2 ? final : stage * 2;
        mpfr_prec_round(x, stage, MPFR_RNDN);
        mpfr_set_prec(log_y, stage); mpfr_set_prec(log_gamma, stage);
        mpfr_set_prec(derivative, stage / 2 + 32);
        mpfr_set_prec(residual, stage); mpfr_set_prec(step, stage); mpfr_set_prec(bound, stage);
    }
    if (complete) mpfr_set(value, x, MPFR_RNDN);
    else mpfr_set_nan(value);
    mpfr_clears(source, minimum, x, log_y, log_gamma, derivative, residual, step, bound, (mpfr_ptr)0);
    return complete ? 0 : -1;
}

/* Refine the logarithmic branch at increasing precision and verify the final defining residual. */
int number_mpc_gammainv(mpc_ptr out, mpc_srcptr input, mpc_rnd_t rounding)
{
    mpfr_prec_t target = mpc_get_prec(out), final = target + 128;
    if (final > 65536 || !mpfr_number_p(mpc_realref(input)) || !mpfr_number_p(mpc_imagref(input)) ||
        (mpfr_zero_p(mpc_realref(input)) && mpfr_zero_p(mpc_imagref(input)))) {
        mpc_set_nan(out);
        return -1;
    }
    mpfr_prec_t stage = inverse_gamma_initial_precision(final);
    mpc_t value, logarithm, residual, derivative, step;
    mpc_init2(value, stage); mpc_init2(logarithm, stage); mpc_init2(residual, stage);
    mpc_init2(derivative, stage); mpc_init2(step, stage);
    mpc_set_prec(derivative, stage / 2 + 48);
    mpfr_t magnitude, scale;
    mpfr_inits2(stage, magnitude, scale, (mpfr_ptr)0);
    mpc_log(value, input, MPC_RNDNN);
    mpfr_add_d(mpc_realref(value), mpc_realref(value), 1.5, MPFR_RNDN);
    bool complete = false;
    for (;;) {
        mpc_log(logarithm, input, MPC_RNDNN);
        bool converged = false;
        for (unsigned i = 0; i < 48; ++i) {
            if (number_mpc_lgamma(residual, value, MPC_RNDNN) ||
                number_mpc_digamma_mut(derivative, value, MPC_RNDNN)) break;
            mpc_sub(residual, residual, logarithm, MPC_RNDNN);
            mpc_div(step, residual, derivative, MPC_RNDNN);
            number_mpc_abs_upper(magnitude, step);
            number_mpc_abs_lower(scale, value);
            if (mpfr_cmp_ui(scale, 1u) < 0) mpfr_set_ui(scale, 1u, MPFR_RNDN);
            mpfr_mul_2si(scale, scale, stage == final ? -target - 24 : -stage / 2 + 16, MPFR_RNDD);
            bool small = mpfr_number_p(magnitude) && mpfr_lessequal_p(magnitude, scale);
            if (small && stage == final &&
                inverse_gamma_complex_residual(residual, logarithm, target, magnitude, scale)) {
                complete = converged = true;
                break;
            }
            mpc_sub(value, value, step, MPC_RNDNN);
            if (small) { converged = true; break; }
        }
        if (!converged) break;
        if (stage == final) {
            if (complete) break;
            if (number_mpc_lgamma(residual, value, MPC_RNDNN)) break;
            mpc_sub(residual, residual, logarithm, MPC_RNDNN);
            complete = inverse_gamma_complex_residual(residual, logarithm, target, magnitude, scale);
            break;
        }
        stage = stage > final / 2 ? final : stage * 2;
        mpfr_prec_round(mpc_realref(value), stage, MPFR_RNDN);
        mpfr_prec_round(mpc_imagref(value), stage, MPFR_RNDN);
        mpc_set_prec(logarithm, stage); mpc_set_prec(residual, stage);
        mpc_set_prec(derivative, stage / 2 + 48);
        mpc_set_prec(step, stage); mpfr_set_prec(magnitude, stage); mpfr_set_prec(scale, stage);
    }
    if (complete) mpc_set(out, value, rounding);
    else mpc_set_nan(out);
    mpc_clear(value); mpc_clear(logarithm); mpc_clear(residual); mpc_clear(derivative); mpc_clear(step);
    mpfr_clears(magnitude, scale, (mpfr_ptr)0);
    return complete ? 0 : -1;
}
