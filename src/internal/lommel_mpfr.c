#include <math.h>
#include <stddef.h>

#include "internal/lommel_mpfr.h"

enum {
    LOMMEL_GUARD_BITS = 64,
    LOMMEL_MAX_EXTRA_BITS = 65536,
    LOMMEL_MAX_SERIES_TERMS = 200000
};

static mpfr_prec_t lommel_work_precision(mpfr_srcptr out,
                                         mpfr_srcptr mu,
                                         mpfr_srcptr nu,
                                         mpfr_srcptr argument)
{
    mpfr_prec_t precision = mpfr_get_prec(out);
    double magnitude;
    long extra = LOMMEL_GUARD_BITS;

    if (mpfr_get_prec(mu) > precision)
        precision = mpfr_get_prec(mu);
    if (mpfr_get_prec(nu) > precision)
        precision = mpfr_get_prec(nu);
    if (mpfr_get_prec(argument) > precision)
        precision = mpfr_get_prec(argument);

    magnitude = fabs(mpfr_get_d(argument, MPFR_RNDN));
    if (isfinite(magnitude) && magnitude > 1.0) {
        double cancellation_bits = ceil(1.5 * magnitude);

        if (cancellation_bits > LOMMEL_MAX_EXTRA_BITS)
            cancellation_bits = LOMMEL_MAX_EXTRA_BITS;
        extra += (long)cancellation_bits;
    }
    return precision + (mpfr_prec_t)extra;
}

static int lommel_series_converged(mpfr_srcptr term, mpfr_srcptr sum,
                                   mpfr_prec_t target_precision)
{
    if (mpfr_zero_p(term))
        return 1;
    if (mpfr_zero_p(sum))
        return 0;
    return mpfr_get_exp(term) <
        mpfr_get_exp(sum) - (mpfr_exp_t)target_precision - 16;
}

static int lommel_negative_argument_allowed(mpfr_srcptr mu)
{
    return mpfr_integer_p(mu);
}

static int lommel_parameters_singular(mpfr_srcptr mu, mpfr_srcptr nu)
{
    mpfr_prec_t precision = mpfr_get_prec(mu) > mpfr_get_prec(nu)
        ? mpfr_get_prec(mu) : mpfr_get_prec(nu);
    mpfr_t combination;
    mpz_t integer;
    int singular = 0;

    mpfr_init2(combination, precision + 1);
    mpz_init(integer);

    mpfr_add(combination, mu, nu, MPFR_RNDN);
    if (mpfr_sgn(combination) < 0 && mpfr_integer_p(combination)) {
        mpfr_get_z(integer, combination, MPFR_RNDN);
        singular = mpz_odd_p(integer);
    }
    if (!singular) {
        mpfr_sub(combination, mu, nu, MPFR_RNDN);
        if (mpfr_sgn(combination) < 0 && mpfr_integer_p(combination)) {
            mpfr_get_z(integer, combination, MPFR_RNDN);
            singular = mpz_odd_p(integer);
        }
    }

    mpz_clear(integer);
    mpfr_clear(combination);
    return singular;
}

static int mars_mpfr_lommel_s_series(mpfr_ptr out, mpfr_srcptr mu,
                                     mpfr_srcptr nu, mpfr_srcptr argument,
                                     mpfr_rnd_t rounding, int derivative)
{
    mpfr_prec_t target_precision;
    mpfr_prec_t work_precision;
    mpfr_t mu_value, nu_value, z, z_squared, exponent;
    mpfr_t denominator, term, contribution, sum, factor;
    size_t maximum_terms = 10000u;
    int status = -1;

    if (!out || !mu || !nu || !argument)
        return -1;
    if (mpfr_nan_p(mu) || mpfr_nan_p(nu) || mpfr_nan_p(argument) ||
        mpfr_inf_p(mu) || mpfr_inf_p(nu) || mpfr_inf_p(argument) ||
        lommel_parameters_singular(mu, nu) ||
        (mpfr_sgn(argument) < 0 && !lommel_negative_argument_allowed(mu))) {
        mpfr_set_nan(out);
        return 0;
    }

    target_precision = mpfr_get_prec(out);
    work_precision = lommel_work_precision(out, mu, nu, argument);
    mpfr_inits2(work_precision, mu_value, nu_value, z, z_squared,
                exponent, denominator, term, contribution, sum, factor,
                (mpfr_ptr)0);
    mpfr_set(mu_value, mu, MPFR_RNDN);
    mpfr_set(nu_value, nu, MPFR_RNDN);
    mpfr_set(z, argument, MPFR_RNDN);
    mpfr_mul(z_squared, z, z, MPFR_RNDN);

    mpfr_add_ui(exponent, mu_value, 1u, MPFR_RNDN);
    mpfr_mul(denominator, exponent, exponent, MPFR_RNDN);
    mpfr_mul(factor, nu_value, nu_value, MPFR_RNDN);
    mpfr_sub(denominator, denominator, factor, MPFR_RNDN);
    if (mpfr_zero_p(denominator))
        goto done;

    if (mpfr_zero_p(z)) {
        if (derivative) {
            int mu_sign = mpfr_sgn(mu_value);

            if (mu_sign > 0 || mpfr_cmp_si(mu_value, -1) == 0) {
                mpfr_set_zero(out, 1);
                status = 0;
            } else if (mu_sign == 0) {
                mpfr_div(out, exponent, denominator, rounding);
                status = 0;
            }
        } else {
            int exponent_sign = mpfr_sgn(exponent);

            if (exponent_sign > 0) {
                mpfr_set_zero(out, 1);
                status = 0;
            } else if (exponent_sign == 0) {
                mpfr_ui_div(out, 1u, denominator, rounding);
                status = 0;
            }
        }
        goto done;
    }

    mpfr_pow(term, z, exponent, MPFR_RNDN);
    if (mpfr_nan_p(term) || mpfr_inf_p(term))
        goto done;
    mpfr_div(term, term, denominator, MPFR_RNDN);
    if (derivative) {
        mpfr_mul(contribution, term, exponent, MPFR_RNDN);
        mpfr_div(contribution, contribution, z, MPFR_RNDN);
    } else {
        mpfr_set(contribution, term, MPFR_RNDN);
    }
    mpfr_set(sum, contribution, MPFR_RNDN);

    {
        double magnitude = fabs(mpfr_get_d(z, MPFR_RNDN));

        if (isfinite(magnitude) && magnitude > 1.0) {
            double requested = 2.0 * magnitude +
                (double)target_precision + 64.0;

            if (requested > (double)maximum_terms)
                maximum_terms = requested > LOMMEL_MAX_SERIES_TERMS
                    ? LOMMEL_MAX_SERIES_TERMS : (size_t)requested;
        }
    }

    for (size_t k = 0u; k < maximum_terms; ++k) {
        mpfr_set_ui(exponent, 2u * k + 3u, MPFR_RNDN);
        mpfr_add(exponent, exponent, mu_value, MPFR_RNDN);
        mpfr_mul(denominator, exponent, exponent, MPFR_RNDN);
        mpfr_mul(factor, nu_value, nu_value, MPFR_RNDN);
        mpfr_sub(denominator, denominator, factor, MPFR_RNDN);
        if (mpfr_zero_p(denominator))
            goto done;

        mpfr_mul(term, term, z_squared, MPFR_RNDN);
        mpfr_div(term, term, denominator, MPFR_RNDN);
        mpfr_neg(term, term, MPFR_RNDN);
        if (derivative) {
            mpfr_mul(contribution, term, exponent, MPFR_RNDN);
            mpfr_div(contribution, contribution, z, MPFR_RNDN);
        } else {
            mpfr_set(contribution, term, MPFR_RNDN);
        }
        mpfr_add(sum, sum, contribution, MPFR_RNDN);
        if ((!derivative || !mpfr_zero_p(contribution)) &&
            lommel_series_converged(contribution, sum, target_precision)) {
            mpfr_set(out, sum, rounding);
            status = 0;
            goto done;
        }
    }

done:
    if (status != 0)
        mpfr_set_nan(out);
    mpfr_clears(mu_value, nu_value, z, z_squared, exponent,
                denominator, term, contribution, sum, factor, (mpfr_ptr)0);
    return status;
}

int mars_mpfr_lommel_s(mpfr_ptr out, mpfr_srcptr mu, mpfr_srcptr nu,
                       mpfr_srcptr argument, mpfr_rnd_t rounding)
{
    return mars_mpfr_lommel_s_series(out, mu, nu, argument, rounding, 0);
}

int mars_mpfr_lommel_s_derivative(mpfr_ptr out, mpfr_srcptr mu,
                                  mpfr_srcptr nu, mpfr_srcptr argument,
                                  mpfr_rnd_t rounding)
{
    return mars_mpfr_lommel_s_series(out, mu, nu, argument, rounding, 1);
}
