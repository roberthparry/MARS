#include <limits.h>
#include <math.h>
#include <stddef.h>

#include "internal/bessel_mpfr.h"

enum {
    BESSEL_GUARD_BITS = 64,
    BESSEL_MAX_EXTRA_BITS = 65536,
    BESSEL_MAX_SERIES_TERMS = 200000
};

static mpfr_prec_t bessel_work_precision(mpfr_srcptr out,
                                         mpfr_srcptr order,
                                         mpfr_srcptr argument)
{
    mpfr_prec_t precision = mpfr_get_prec(out);
    double magnitude;
    long extra = BESSEL_GUARD_BITS;

    if (mpfr_get_prec(order) > precision)
        precision = mpfr_get_prec(order);
    if (mpfr_get_prec(argument) > precision)
        precision = mpfr_get_prec(argument);

    magnitude = fabs(mpfr_get_d(argument, MPFR_RNDN));
    if (isfinite(magnitude) && magnitude > 1.0) {
        double cancellation_bits = ceil(1.5 * magnitude);

        if (cancellation_bits > BESSEL_MAX_EXTRA_BITS)
            cancellation_bits = BESSEL_MAX_EXTRA_BITS;
        extra += (long)cancellation_bits;
    }
    return precision + (mpfr_prec_t)extra;
}

static int bessel_integer_order(mpfr_srcptr order, long *value)
{
    if (!mpfr_integer_p(order) ||
        !mpfr_fits_slong_p(order, MPFR_RNDN))
        return 0;
    *value = mpfr_get_si(order, MPFR_RNDN);
    return 1;
}

static int bessel_series_converged(mpfr_srcptr term, mpfr_srcptr sum,
                                   mpfr_prec_t target_precision)
{
    if (mpfr_zero_p(term))
        return 1;
    if (mpfr_zero_p(sum))
        return 0;
    return mpfr_get_exp(term) <
        mpfr_get_exp(sum) - (mpfr_exp_t)target_precision - 16;
}

int mars_mpfr_bessel_j(mpfr_ptr out, mpfr_srcptr order,
                       mpfr_srcptr argument, mpfr_rnd_t rounding)
{
    mpfr_prec_t target_precision;
    mpfr_prec_t work_precision;
    mpfr_t nu, x, half_x, q, gamma, term, sum, denominator, k_value;
    size_t maximum_terms = 10000u;
    long integer_order;
    int status = -1;

    if (!out || !order || !argument)
        return -1;
    if (mpfr_nan_p(order) || mpfr_nan_p(argument)) {
        mpfr_set_nan(out);
        return 0;
    }
    if (bessel_integer_order(order, &integer_order)) {
        mpfr_jn(out, integer_order, argument, rounding);
        return 0;
    }
    if (mpfr_sgn(argument) < 0 || mpfr_inf_p(argument)) {
        mpfr_set_nan(out);
        return 0;
    }
    if (mpfr_zero_p(argument)) {
        if (mpfr_sgn(order) > 0) {
            mpfr_set_zero(out, 1);
        } else {
            mpfr_t order_plus_one;

            mpfr_init2(order_plus_one, mpfr_get_prec(order) + 16);
            mpfr_add_ui(order_plus_one, order, 1u, MPFR_RNDN);
            mpfr_gamma(order_plus_one, order_plus_one, MPFR_RNDN);
            mpfr_set_inf(out, mpfr_sgn(order_plus_one) < 0 ? -1 : 1);
            mpfr_clear(order_plus_one);
        }
        return 0;
    }

    target_precision = mpfr_get_prec(out);
    work_precision = bessel_work_precision(out, order, argument);
    mpfr_inits2(work_precision, nu, x, half_x, q, gamma, term, sum,
                denominator, k_value, (mpfr_ptr)0);
    mpfr_set(nu, order, MPFR_RNDN);
    mpfr_set(x, argument, MPFR_RNDN);
    mpfr_div_2ui(half_x, x, 1u, MPFR_RNDN);
    mpfr_mul(q, half_x, half_x, MPFR_RNDN);

    mpfr_add_ui(gamma, nu, 1u, MPFR_RNDN);
    mpfr_gamma(gamma, gamma, MPFR_RNDN);
    if (mpfr_nan_p(gamma) || mpfr_zero_p(gamma) || mpfr_inf_p(gamma))
        goto done;
    mpfr_pow(term, half_x, nu, MPFR_RNDN);
    mpfr_div(term, term, gamma, MPFR_RNDN);
    mpfr_set(sum, term, MPFR_RNDN);

    {
        double magnitude = fabs(mpfr_get_d(x, MPFR_RNDN));

        if (isfinite(magnitude) && magnitude > 1.0) {
            double requested = 2.0 * magnitude +
                (double)target_precision + 64.0;

            if (requested > (double)maximum_terms)
                maximum_terms = requested > BESSEL_MAX_SERIES_TERMS
                    ? BESSEL_MAX_SERIES_TERMS : (size_t)requested;
        }
    }

    for (size_t k = 1u; k <= maximum_terms; ++k) {
        mpfr_set_ui(k_value, k, MPFR_RNDN);
        mpfr_add(denominator, nu, k_value, MPFR_RNDN);
        mpfr_mul(denominator, denominator, k_value, MPFR_RNDN);
        if (mpfr_zero_p(denominator))
            goto done;
        mpfr_mul(term, term, q, MPFR_RNDN);
        mpfr_div(term, term, denominator, MPFR_RNDN);
        mpfr_neg(term, term, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        if (bessel_series_converged(term, sum, target_precision)) {
            mpfr_set(out, sum, rounding);
            status = 0;
            goto done;
        }
    }

done:
    if (status != 0)
        mpfr_set_nan(out);
    mpfr_clears(nu, x, half_x, q, gamma, term, sum, denominator,
                k_value, (mpfr_ptr)0);
    return status;
}

int mars_mpfr_bessel_y(mpfr_ptr out, mpfr_srcptr order,
                       mpfr_srcptr argument, mpfr_rnd_t rounding)
{
    mpfr_prec_t work_precision;
    mpfr_t nu, x, negative_nu, j_positive, j_negative;
    mpfr_t pi_nu, sine, cosine, numerator;
    long integer_order;
    int status = -1;

    if (!out || !order || !argument)
        return -1;
    if (mpfr_nan_p(order) || mpfr_nan_p(argument)) {
        mpfr_set_nan(out);
        return 0;
    }
    if (bessel_integer_order(order, &integer_order)) {
        mpfr_yn(out, integer_order, argument, rounding);
        return 0;
    }
    if (mpfr_sgn(argument) <= 0 || mpfr_inf_p(argument)) {
        mpfr_set_nan(out);
        return 0;
    }

    work_precision = bessel_work_precision(out, order, argument) + 64;
    mpfr_inits2(work_precision, nu, x, negative_nu, j_positive, j_negative,
                pi_nu, sine, cosine, numerator, (mpfr_ptr)0);
    mpfr_set(nu, order, MPFR_RNDN);
    mpfr_set(x, argument, MPFR_RNDN);
    mpfr_neg(negative_nu, nu, MPFR_RNDN);
    if (mars_mpfr_bessel_j(j_positive, nu, x, MPFR_RNDN) != 0 ||
        mars_mpfr_bessel_j(j_negative, negative_nu, x, MPFR_RNDN) != 0)
        goto done;

    mpfr_const_pi(pi_nu, MPFR_RNDN);
    mpfr_mul(pi_nu, pi_nu, nu, MPFR_RNDN);
    mpfr_sin_cos(sine, cosine, pi_nu, MPFR_RNDN);
    if (mpfr_zero_p(sine))
        goto done;
    mpfr_mul(numerator, cosine, j_positive, MPFR_RNDN);
    mpfr_sub(numerator, numerator, j_negative, MPFR_RNDN);
    mpfr_div(out, numerator, sine, rounding);
    status = 0;

done:
    if (status != 0)
        mpfr_set_nan(out);
    mpfr_clears(nu, x, negative_nu, j_positive, j_negative, pi_nu,
                sine, cosine, numerator, (mpfr_ptr)0);
    return status;
}
