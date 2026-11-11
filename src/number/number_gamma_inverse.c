/**
 * @file number_gamma_inverse.c
 * @brief Progressive-precision real inverse gamma refinement.
 *
 * Preserves the established fixed-precision seed and its real branch selection, then refines the logarithmic
 * defining equation with Newton iteration. Early stages use small significands; precision grows only after the
 * previous stage converges. The Newton slope needs only half-stage precision plus guard bits; the small residual
 * multiplies its rounding error. The final stage still checks both its correction and the full-precision logarithmic
 * residual. Seeds are never accepted as full-precision answers. This implementation is private to the number module.
 */
#include <math.h>

#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Refine the existing real inverse-gamma branch without doing early iterations at the final precision. */
int number_mpfr_gammainv_mut(mpfr_t value)
{
    mpfr_prec_t target = mpfr_get_prec(value), final = target + (target > 256 ? target / 4 : 64);
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
    mpfr_prec_t stage = final < 128 ? final : 128;
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
        mpfr_prec_t accuracy = stage == final ? target + 8 : stage - 32;
        bool converged = false;
        for (unsigned i = 0; i < 64; ++i) {
            mpfr_lngamma(log_gamma, x, MPFR_RNDN);
            mpfr_sub(residual, log_gamma, log_y, MPFR_RNDN);
            mpfr_digamma(derivative, x, MPFR_RNDN);
            if (mpfr_zero_p(derivative) || !mpfr_number_p(derivative)) break;
            mpfr_div(step, residual, derivative, MPFR_RNDN);
            mpfr_sub(x, x, step, MPFR_RNDN);
            if (!mpfr_number_p(x) || mpfr_sgn(x) <= 0) break;
            mpfr_abs(step, step, MPFR_RNDN);
            if (mpfr_zero_p(step) || mpfr_cmp_ui_2exp(step, 1u, -(long)accuracy) <= 0) {
                converged = true;
                break;
            }
        }
        if (!converged) break;
        if (stage == final) {
            mpfr_lngamma(log_gamma, x, MPFR_RNDN);
            mpfr_sub(residual, log_gamma, log_y, MPFR_RNDN);
            mpfr_abs(residual, residual, MPFR_RNDU);
            mpfr_abs(bound, log_y, MPFR_RNDD);
            if (mpfr_cmp_ui(bound, 1u) < 0) mpfr_set_ui(bound, 1u, MPFR_RNDN);
            mpfr_mul_2si(bound, bound, -(long)target - 8, MPFR_RNDD);
            complete = mpfr_lessequal_p(residual, bound);
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
