/**
 * @file number_polygamma_coefficients.c
 * @brief Absolute precision budgets for polygamma asymptotic coefficients.
 *
 * Bounds Bernoulli-weighted powers before requesting their coefficients from the number module's shared cache.
 * The inequality |B_(2n)| < 4*(2n)!/32^n bounds every polygamma order up to sixty-four, including digamma.
 * A caller-owned factorial recurrence and directed scalar bounds shorten coefficients while preserving full-width
 * powers, terms and accumulators. Unsupported budgets retain the original coefficient precision. This helper is
 * private to the number module and retains no state between calls.
 */
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"

/* Keep each shortened coefficient's weighted absolute error below 2^(-work-32). */
mpfr_prec_t number_polygamma_coefficient_precision(mpfr_ptr factorial, mpfr_ptr bound, mpfr_srcptr power_upper,
                                                   size_t index, unsigned order, mpfr_prec_t work)
{
    if (work < 640 || work > 65536 || !index || index > 65536 || order > 64 ||
        !mpfr_number_p(power_upper) || mpfr_sgn(power_upper) <= 0) return work;
    if (index == 1) mpfr_fac_ui(factorial, order + 1ul, MPFR_RNDU);
    else {
        mpfr_mul_ui(factorial, factorial, 2ul * index + order - 2ul, MPFR_RNDU);
        mpfr_mul_ui(factorial, factorial, 2ul * index + order - 1ul, MPFR_RNDU);
    }
    mpfr_mul(bound, factorial, power_upper, MPFR_RNDU);
    if (!mpfr_number_p(bound) || mpfr_sgn(bound) <= 0) return work;
    /* For order m, the coefficient bound is 4*(2n+m-1)!/32^n. Forty extra bits cover the
     * Bernoulli conversion and up to 64 rounded integer factors, leaving 32 absolute guard bits. */
    mpfr_exp_t adjustment = mpfr_get_exp(bound) - 5 * (mpfr_exp_t)index + 40;
    if (adjustment >= 0) return work;
    return adjustment < 128 - work ? 128 : work + adjustment;
}
