#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "internal/mrational_internal.h"
#include "mfloat_internal.h"
#include "mrational/mrational_internal.h"

static void mfloat_prepare_constant(const mfloat_t *mfloat, mpfr_prec_t precision)
{
    if (mfloat && mfloat->constant_id != MFCONST_NONE)
        mfloat_constant_ensure(mfloat, precision);
}

static mpfr_prec_t mfloat_binary_prec(const mfloat_t *a, const mfloat_t *b)
{
    mpfr_prec_t prec = (mpfr_prec_t)mf_get_default_precision();

    if (a && a->constant_id == MFCONST_NONE)
        prec = mpfr_get_prec(a->value);
    if (b && b->constant_id == MFCONST_NONE && mpfr_get_prec(b->value) > prec)
        prec = mpfr_get_prec(b->value);
    return prec;
}

static int mfloat_prepare_mutable(mfloat_t *mfloat)
{
    if (!mfloat || mfloat->constant_id != MFCONST_NONE)
        return -1;
    return 0;
}

static int mfloat_apply_unary_mpfr(mfloat_t *mfloat,
                                   int (*op)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t))
{
    if (mfloat_prepare_mutable(mfloat) != 0 || !op)
        return -1;
    op(mfloat->value, mfloat->value, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_binary_mpfr(mfloat_t *mfloat, const mfloat_t *other,
                                    int (*op)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr,
                                              mpfr_rnd_t))
{
    if (mfloat_prepare_mutable(mfloat) != 0 || !other || !op)
        return -1;

    mfloat_prepare_constant(other, mpfr_get_prec(mfloat->value));
    op(mfloat->value, mfloat->value, other->value, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_logbeta_mpfr(mfloat_t *mfloat, const mfloat_t *other)
{
    mpfr_t beta;

    if (mfloat_prepare_mutable(mfloat) != 0 || !other)
        return -1;
    mfloat_prepare_constant(other, mpfr_get_prec(mfloat->value));

    mpfr_init2(beta, mpfr_get_prec(mfloat->value));
    mpfr_beta(beta, mfloat->value, other->value, MPFR_RNDN);
    mpfr_log(mfloat->value, beta, MPFR_RNDN);
    mpfr_clear(beta);
    return 0;
}

static int mfloat_apply_binomial_mpfr(mfloat_t *mfloat, const mfloat_t *other)
{
    mpfr_t a, b, t;
    mpfr_prec_t prec;

    if (mfloat_prepare_mutable(mfloat) != 0 || !other)
        return -1;
    mfloat_prepare_constant(other, mfloat_binary_prec(mfloat, other));

    prec = mpfr_get_prec(mfloat->value);
    if (mpfr_get_prec(other->value) > prec)
        prec = mpfr_get_prec(other->value);

    mpfr_init2(a, prec);
    mpfr_init2(b, prec);
    mpfr_init2(t, prec);

    mpfr_set(a, mfloat->value, MPFR_RNDN);
    mpfr_set(b, other->value, MPFR_RNDN);
    mpfr_add_ui(a, a, 1u, MPFR_RNDN);
    mpfr_lngamma(mfloat->value, a, MPFR_RNDN);
    mpfr_add_ui(b, b, 1u, MPFR_RNDN);
    mpfr_lngamma(t, b, MPFR_RNDN);
    mpfr_sub(mfloat->value, mfloat->value, t, MPFR_RNDN);
    mpfr_sub(t, a, b, MPFR_RNDN);
    mpfr_add_ui(t, t, 1u, MPFR_RNDN);
    mpfr_lngamma(t, t, MPFR_RNDN);
    mpfr_sub(mfloat->value, mfloat->value, t, MPFR_RNDN);
    mpfr_exp(mfloat->value, mfloat->value, MPFR_RNDN);

    mpfr_clear(a);
    mpfr_clear(b);
    mpfr_clear(t);
    return 0;
}

static int mfloat_apply_gammainc_mpfr(mfloat_t *mfloat, const mfloat_t *other, int mode)
{
    mpfr_t gamma_a, upper, tmp;
    mpfr_prec_t prec;

    if (mfloat_prepare_mutable(mfloat) != 0 || !other)
        return -1;
    mfloat_prepare_constant(other, mfloat_binary_prec(mfloat, other));

    prec = mpfr_get_prec(mfloat->value);
    if (mpfr_get_prec(other->value) > prec)
        prec = mpfr_get_prec(other->value);

    mpfr_init2(gamma_a, prec);
    mpfr_init2(upper, prec);
    mpfr_init2(tmp, prec);

    mpfr_gamma(gamma_a, mfloat->value, MPFR_RNDN);
    mpfr_gamma_inc(upper, mfloat->value, other->value, MPFR_RNDN);

    switch (mode) {
        case 0: /* lower */
            mpfr_sub(mfloat->value, gamma_a, upper, MPFR_RNDN);
            break;
        case 1: /* upper */
            mpfr_set(mfloat->value, upper, MPFR_RNDN);
            break;
        case 2: /* P */
            mpfr_sub(tmp, gamma_a, upper, MPFR_RNDN);
            mpfr_div(mfloat->value, tmp, gamma_a, MPFR_RNDN);
            break;
        case 3: /* Q */
            mpfr_div(mfloat->value, upper, gamma_a, MPFR_RNDN);
            break;
        default:
            mpfr_clear(gamma_a);
            mpfr_clear(upper);
            mpfr_clear(tmp);
            return -1;
    }

    mpfr_clear(gamma_a);
    mpfr_clear(upper);
    mpfr_clear(tmp);
    return 0;
}

static int mfloat_apply_e1_mpfr(mfloat_t *mfloat)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_neg(mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_eint(mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_neg(mfloat->value, mfloat->value, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_normal_pdf_mpfr(mfloat_t *mfloat)
{
    mfloat_prepare_constant(MF_INV_SQRT_2PI, mpfr_get_prec(mfloat->value));
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_mul(mfloat->value, mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_div_2ui(mfloat->value, mfloat->value, 1u, MPFR_RNDN);
    mpfr_neg(mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_exp(mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_mul(mfloat->value, mfloat->value, MF_INV_SQRT_2PI->value, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_normal_cdf_mpfr(mfloat_t *mfloat)
{
    mfloat_prepare_constant(MF_SQRT2, mpfr_get_prec(mfloat->value));
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_div(mfloat->value, mfloat->value, MF_SQRT2->value, MPFR_RNDN);
    mpfr_erf(mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_add_ui(mfloat->value, mfloat->value, 1u, MPFR_RNDN);
    mpfr_div_2ui(mfloat->value, mfloat->value, 1u, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_normal_logpdf_mpfr(mfloat_t *mfloat)
{
    mfloat_prepare_constant(MF_LOG_SQRT_2PI, mpfr_get_prec(mfloat->value));
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_mul(mfloat->value, mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_div_2ui(mfloat->value, mfloat->value, 1u, MPFR_RNDN);
    mpfr_neg(mfloat->value, mfloat->value, MPFR_RNDN);
    mpfr_sub(mfloat->value, mfloat->value, MF_LOG_SQRT_2PI->value, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_logbeta_pdf_mpfr(mfloat_t *mfloat,
                                         const mfloat_t *a,
                                         const mfloat_t *b)
{
    mpfr_t x, alpha, beta, t;
    mpfr_prec_t prec;

    if (mfloat_prepare_mutable(mfloat) != 0 || !a || !b)
        return -1;
    mfloat_prepare_constant(a, mfloat_binary_prec(a, b));
    mfloat_prepare_constant(b, mfloat_binary_prec(a, b));

    prec = mpfr_get_prec(mfloat->value);
    if (mpfr_get_prec(a->value) > prec)
        prec = mpfr_get_prec(a->value);
    if (mpfr_get_prec(b->value) > prec)
        prec = mpfr_get_prec(b->value);

    mpfr_init2(x, prec);
    mpfr_init2(alpha, prec);
    mpfr_init2(beta, prec);
    mpfr_init2(t, prec);

    mpfr_set(x, mfloat->value, MPFR_RNDN);
    mpfr_set(alpha, a->value, MPFR_RNDN);
    mpfr_set(beta, b->value, MPFR_RNDN);

    mpfr_sub_ui(alpha, alpha, 1u, MPFR_RNDN);
    mpfr_log(mfloat->value, x, MPFR_RNDN);
    mpfr_mul(mfloat->value, mfloat->value, alpha, MPFR_RNDN);

    mpfr_ui_sub(t, 1u, x, MPFR_RNDN);
    mpfr_sub_ui(beta, beta, 1u, MPFR_RNDN);
    mpfr_log(t, t, MPFR_RNDN);
    mpfr_mul(t, t, beta, MPFR_RNDN);
    mpfr_add(mfloat->value, mfloat->value, t, MPFR_RNDN);

    mpfr_beta(t, a->value, b->value, MPFR_RNDN);
    mpfr_log(t, t, MPFR_RNDN);
    mpfr_sub(mfloat->value, mfloat->value, t, MPFR_RNDN);

    mpfr_clear(x);
    mpfr_clear(alpha);
    mpfr_clear(beta);
    mpfr_clear(t);
    return 0;
}

static int mfloat_apply_beta_pdf_mpfr(mfloat_t *mfloat,
                                      const mfloat_t *a,
                                      const mfloat_t *b)
{
    if (mfloat_apply_logbeta_pdf_mpfr(mfloat, a, b) != 0)
        return -1;
    mpfr_exp(mfloat->value, mfloat->value, MPFR_RNDN);
    return 0;
}

static int mfloat_apply_erfinv_mpfr(mfloat_t *mfloat, int erfc_mode)
{
    mpfr_t x, y, err, deriv, tmp;
    int i;

    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;

    mfloat_prepare_constant(MF_2_SQRTPI, mpfr_get_prec(mfloat->value));
    mpfr_init2(x, mpfr_get_prec(mfloat->value));
    mpfr_init2(y, mpfr_get_prec(mfloat->value));
    mpfr_init2(err, mpfr_get_prec(mfloat->value));
    mpfr_init2(deriv, mpfr_get_prec(mfloat->value));
    mpfr_init2(tmp, mpfr_get_prec(mfloat->value));

    mpfr_set(x, mfloat->value, MPFR_RNDN);
    if (erfc_mode)
        mpfr_ui_sub(x, 1u, x, MPFR_RNDN);
    mpfr_set(y, x, MPFR_RNDN);
    mpfr_abs(tmp, x, MPFR_RNDN);
    if (mpfr_cmp_d(tmp, 0.75) > 0) {
        mpfr_ui_sub(tmp, 1u, tmp, MPFR_RNDN);
        mpfr_log(tmp, tmp, MPFR_RNDN);
        mpfr_neg(tmp, tmp, MPFR_RNDN);
        mpfr_sqrt(y, tmp, MPFR_RNDN);
        if (mpfr_sgn(x) < 0)
            mpfr_neg(y, y, MPFR_RNDN);
    }

    for (i = 0; i < 8; ++i) {
        mpfr_erf(err, y, MPFR_RNDN);
        mpfr_sub(err, err, x, MPFR_RNDN);

        mpfr_mul(deriv, y, y, MPFR_RNDN);
        mpfr_neg(deriv, deriv, MPFR_RNDN);
        mpfr_exp(deriv, deriv, MPFR_RNDN);
        mpfr_mul(deriv, deriv, MF_2_SQRTPI->value, MPFR_RNDN);

        mpfr_div(tmp, err, deriv, MPFR_RNDN);
        mpfr_sub(y, y, tmp, MPFR_RNDN);
    }

    mpfr_set(mfloat->value, y, MPFR_RNDN);

    mpfr_clear(x);
    mpfr_clear(y);
    mpfr_clear(err);
    mpfr_clear(deriv);
    mpfr_clear(tmp);
    return 0;
}

static int mfloat_apply_trigamma_mpfr(mfloat_t *mfloat)
{
    size_t bernoulli_terms;
    mpfr_prec_t prec;
    mpfr_t y, inv, inv2, power, term, sum;
    size_t n;

    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;

    if (!mpfr_number_p(mfloat->value) || mpfr_sgn(mfloat->value) <= 0) {
        mpfr_set_nan(mfloat->value);
        return 0;
    }

    prec = mpfr_get_prec(mfloat->value) + 128;
    mpfr_inits2(prec, y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    mpfr_set(y, mfloat->value, MPFR_RNDN);
    mpfr_set_zero(sum, 1);

    while (mpfr_cmp_ui(y, 256u) < 0) {
        mpfr_sqr(term, y, MPFR_RNDN);
        mpfr_ui_div(term, 1u, term, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        mpfr_add_ui(y, y, 1u, MPFR_RNDN);
    }

    mpfr_ui_div(inv, 1u, y, MPFR_RNDN);
    mpfr_sqr(inv2, inv, MPFR_RNDN);
    mpfr_add(sum, sum, inv, MPFR_RNDN);
    mpfr_div_2ui(term, inv2, 1u, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    mpfr_mul(power, inv2, inv, MPFR_RNDN);

    bernoulli_terms = mr_bernoulli_even_term_count();
    for (n = 1u; n <= bernoulli_terms; ++n) {
        const mrational_t *bernoulli = mr_bernoulli_even_term(n);

        if (!bernoulli) {
            mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
            return -1;
        }

        mpfr_set_q(term, bernoulli->value, MPFR_RNDN);
        mpfr_mul(term, term, power, MPFR_RNDN);
        mpfr_add(sum, sum, term, MPFR_RNDN);
        mpfr_mul(power, power, inv2, MPFR_RNDN);
    }

    mpfr_set(mfloat->value, sum, MPFR_RNDN);

    mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    return 0;
}

static int mfloat_apply_tetragamma_mpfr(mfloat_t *mfloat)
{
    size_t bernoulli_terms;
    mpfr_prec_t prec;
    mpfr_t y, inv, inv2, power, term, sum;
    size_t n;

    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;

    if (!mpfr_number_p(mfloat->value) || mpfr_sgn(mfloat->value) <= 0) {
        mpfr_set_nan(mfloat->value);
        return 0;
    }

    prec = mpfr_get_prec(mfloat->value) + 128;
    mpfr_inits2(prec, y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    mpfr_set(y, mfloat->value, MPFR_RNDN);
    mpfr_set_zero(sum, 1);

    while (mpfr_cmp_ui(y, 256u) < 0) {
        mpfr_sqr(term, y, MPFR_RNDN);
        mpfr_mul(term, term, y, MPFR_RNDN);
        mpfr_ui_div(term, 2u, term, MPFR_RNDN);
        mpfr_sub(sum, sum, term, MPFR_RNDN);
        mpfr_add_ui(y, y, 1u, MPFR_RNDN);
    }

    mpfr_ui_div(inv, 1u, y, MPFR_RNDN);
    mpfr_sqr(inv2, inv, MPFR_RNDN);
    mpfr_neg(term, inv2, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
    mpfr_mul(power, inv2, inv, MPFR_RNDN);
    mpfr_sub(sum, sum, power, MPFR_RNDN);
    mpfr_mul(power, power, inv, MPFR_RNDN);

    bernoulli_terms = mr_bernoulli_even_term_count();
    for (n = 1u; n <= bernoulli_terms; ++n) {
        const mrational_t *bernoulli = mr_bernoulli_even_term(n);

        if (!bernoulli) {
            mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
            return -1;
        }

        mpfr_set_q(term, bernoulli->value, MPFR_RNDN);
        mpfr_mul_ui(term, term, (unsigned long)(2 * n + 1), MPFR_RNDN);
        mpfr_mul(term, term, power, MPFR_RNDN);
        mpfr_sub(sum, sum, term, MPFR_RNDN);
        mpfr_mul(power, power, inv2, MPFR_RNDN);
    }

    mpfr_set(mfloat->value, sum, MPFR_RNDN);

    mpfr_clears(y, inv, inv2, power, term, sum, (mpfr_ptr)0);
    return 0;
}

static int mfloat_apply_gammainv_mpfr(mfloat_t *mfloat)
{
    mpfr_t y, x, lower, upper, fx, dfx, step, tmp;
    mpfr_prec_t prec;
    int i;
    const double x_min = 1.4616321449683623;

    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;

    prec = mpfr_get_prec(mfloat->value) + 128;
    mpfr_inits2(prec, y, x, lower, upper, fx, dfx, step, tmp, (mpfr_ptr)0);

    mpfr_set(y, mfloat->value, MPFR_RNDN);
    if (!mpfr_number_p(y) || mpfr_sgn(y) <= 0) {
        mpfr_set_nan(mfloat->value);
        goto cleanup;
    }

    mpfr_set_d(lower, x_min, MPFR_RNDN);
    mpfr_gamma(tmp, lower, MPFR_RNDN);
    if (mpfr_cmp(y, tmp) < 0) {
        mpfr_set_nan(mfloat->value);
        goto cleanup;
    }

    mpfr_set_ui(upper, 2u, MPFR_RNDN);
    mpfr_gamma(tmp, upper, MPFR_RNDN);
    while (mpfr_cmp(tmp, y) < 0) {
        mpfr_mul_2ui(upper, upper, 1u, MPFR_RNDN);
        mpfr_gamma(tmp, upper, MPFR_RNDN);
    }

    mpfr_add(x, lower, upper, MPFR_RNDN);
    mpfr_div_2ui(x, x, 1u, MPFR_RNDN);

    for (i = 0; i < 1200; ++i) {
        mpfr_gamma(fx, x, MPFR_RNDN);
        mpfr_sub(fx, fx, y, MPFR_RNDN);

        if (mpfr_zero_p(fx))
            break;

        if (mpfr_sgn(fx) < 0)
            mpfr_set(lower, x, MPFR_RNDN);
        else
            mpfr_set(upper, x, MPFR_RNDN);

        mpfr_gamma(dfx, x, MPFR_RNDN);
        mpfr_digamma(tmp, x, MPFR_RNDN);
        mpfr_mul(dfx, dfx, tmp, MPFR_RNDN);

        if (mpfr_zero_p(dfx) || !mpfr_number_p(dfx)) {
            mpfr_add(x, lower, upper, MPFR_RNDN);
            mpfr_div_2ui(x, x, 1u, MPFR_RNDN);
            continue;
        }

        mpfr_div(step, fx, dfx, MPFR_RNDN);
        mpfr_sub(tmp, x, step, MPFR_RNDN);
        if (mpfr_cmp(tmp, lower) <= 0 || mpfr_cmp(tmp, upper) >= 0 || !mpfr_number_p(tmp)) {
            mpfr_add(x, lower, upper, MPFR_RNDN);
            mpfr_div_2ui(x, x, 1u, MPFR_RNDN);
        } else {
            mpfr_set(x, tmp, MPFR_RNDN);
        }
    }

    mpfr_set(mfloat->value, x, MPFR_RNDN);

cleanup:
    mpfr_clears(y, x, lower, upper, fx, dfx, step, tmp, (mpfr_ptr)0);
    return 0;
}

static int mfloat_apply_lambert_w_mpfr(mfloat_t *mfloat, int branch_m1)
{
    mpfr_t x, w, e, f, denom, tmp;
    int i;

    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;

    mpfr_init2(x, mpfr_get_prec(mfloat->value));
    mpfr_init2(w, mpfr_get_prec(mfloat->value));
    mpfr_init2(e, mpfr_get_prec(mfloat->value));
    mpfr_init2(f, mpfr_get_prec(mfloat->value));
    mpfr_init2(denom, mpfr_get_prec(mfloat->value));
    mpfr_init2(tmp, mpfr_get_prec(mfloat->value));

    mpfr_set(x, mfloat->value, MPFR_RNDN);
    if (branch_m1) {
        mpfr_neg(w, x, MPFR_RNDN);
        mpfr_log(w, w, MPFR_RNDN);
        if (mpfr_cmp_si(w, -1) > 0)
            mpfr_set_si(w, -2, MPFR_RNDN);
    } else if (mpfr_cmp_ui(x, 1u) < 0) {
        mpfr_set(w, x, MPFR_RNDN);
    } else {
        mpfr_log(w, x, MPFR_RNDN);
    }

    for (i = 0; i < 12; ++i) {
        mpfr_exp(e, w, MPFR_RNDN);
        mpfr_mul(f, w, e, MPFR_RNDN);
        mpfr_sub(f, f, x, MPFR_RNDN);

        mpfr_add_ui(denom, w, 1u, MPFR_RNDN);
        mpfr_mul(denom, denom, e, MPFR_RNDN);

        mpfr_add_ui(tmp, w, 2u, MPFR_RNDN);
        mpfr_mul(tmp, tmp, f, MPFR_RNDN);
        mpfr_add_ui(e, w, 1u, MPFR_RNDN);
        mpfr_mul_2ui(e, e, 1u, MPFR_RNDN);
        mpfr_div(tmp, tmp, e, MPFR_RNDN);
        mpfr_sub(denom, denom, tmp, MPFR_RNDN);

        mpfr_div(tmp, f, denom, MPFR_RNDN);
        mpfr_sub(w, w, tmp, MPFR_RNDN);
    }

    mpfr_set(mfloat->value, w, MPFR_RNDN);

    mpfr_clear(x);
    mpfr_clear(w);
    mpfr_clear(e);
    mpfr_clear(f);
    mpfr_clear(denom);
    mpfr_clear(tmp);
    return 0;
}

mfloat_t *mf_pi(void)
{
    return mf_const(MF_PI);
}

mfloat_t *mf_e(void)
{
    return mf_const(MF_E);
}

mfloat_t *mf_euler_mascheroni(void)
{
    return mf_const(MF_EULER_MASCHERONI);
}

mfloat_t *mf_phi(void)
{
    return mf_const(MF_PHI);
}

mfloat_t *mf_max(void)
{
    return mf_create_double(DBL_MAX);
}

int mf_exp(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_exp); }
int mf_log(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_log); }
int mf_log10(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_log10); }
int mf_sin(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_sin); }
int mf_cos(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_cos); }
int mf_tan(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_tan); }
int mf_atan(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_atan); }
int mf_asin(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_asin); }
int mf_acos(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_acos); }
int mf_sinh(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_sinh); }
int mf_cosh(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_cosh); }
int mf_tanh(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_tanh); }
int mf_asinh(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_asinh); }
int mf_acosh(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_acosh); }
int mf_atanh(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_atanh); }
int mf_gamma(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_gamma); }
int mf_erf(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_erf); }
int mf_erfc(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_erfc); }
int mf_atan2(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_binary_mpfr(mfloat, other, mpfr_atan2); }
int mf_pow(mfloat_t *mfloat, const mfloat_t *exponent) { return mfloat_apply_binary_mpfr(mfloat, exponent, mpfr_pow); }
int mf_hypot(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_binary_mpfr(mfloat, other, mpfr_hypot); }

int mf_sincos(const mfloat_t *x, mfloat_t *sin_out, mfloat_t *cos_out)
{
    mpfr_prec_t prec;

    if (!x || !sin_out || !cos_out)
        return -1;
    if (sin_out->constant_id != MFCONST_NONE || cos_out->constant_id != MFCONST_NONE)
        return -1;
    prec = mpfr_get_prec(sin_out->value);
    if (mpfr_get_prec(cos_out->value) > prec)
        prec = mpfr_get_prec(cos_out->value);
    mfloat_prepare_constant(x, prec);
    mpfr_sin_cos(sin_out->value, cos_out->value, x->value, MPFR_RNDN);
    return 0;
}

int mf_sinhcosh(const mfloat_t *x, mfloat_t *sinh_out, mfloat_t *cosh_out)
{
    mpfr_prec_t prec;

    if (!x || !sinh_out || !cosh_out)
        return -1;
    if (sinh_out->constant_id != MFCONST_NONE || cosh_out->constant_id != MFCONST_NONE)
        return -1;
    prec = mpfr_get_prec(sinh_out->value);
    if (mpfr_get_prec(cosh_out->value) > prec)
        prec = mpfr_get_prec(cosh_out->value);
    mfloat_prepare_constant(x, prec);
    mpfr_sinh_cosh(sinh_out->value, cosh_out->value, x->value, MPFR_RNDN);
    return 0;
}

mfloat_t *mf_pow10(int exponent10)
{
    mfloat_t *value = mf_new_prec(mf_get_default_precision());
    if (!value)
        return NULL;
    if (mf_set_long(value, 10) != 0 || mf_pow_int(value, exponent10) != 0) {
        mf_free(value);
        return NULL;
    }
    return value;
}

int mf_sqr(mfloat_t *mfloat)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_mul(mfloat->value, mfloat->value, mfloat->value, MPFR_RNDN);
    return 0;
}

int mf_floor(mfloat_t *mfloat)
{
    if (mfloat_prepare_mutable(mfloat) != 0)
        return -1;
    mpfr_floor(mfloat->value, mfloat->value);
    return 0;
}

int mf_mul_pow10(mfloat_t *mfloat, int exponent10)
{
    mfloat_t *pow10 = mf_pow10(exponent10);
    int rc;

    if (!pow10)
        return -1;
    rc = mf_mul(mfloat, pow10);
    mf_free(pow10);
    return rc;
}

int mf_erfinv(mfloat_t *mfloat) { return mfloat_apply_erfinv_mpfr(mfloat, 0); }
int mf_erfcinv(mfloat_t *mfloat) { return mfloat_apply_erfinv_mpfr(mfloat, 1); }
int mf_lgamma(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_lngamma); }
int mf_digamma(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_digamma); }
int mf_trigamma(mfloat_t *mfloat) { return mfloat_apply_trigamma_mpfr(mfloat); }
int mf_tetragamma(mfloat_t *mfloat) { return mfloat_apply_tetragamma_mpfr(mfloat); }
int mf_gammainv(mfloat_t *mfloat) { return mfloat_apply_gammainv_mpfr(mfloat); }
int mf_lambert_w0(mfloat_t *mfloat) { return mfloat_apply_lambert_w_mpfr(mfloat, 0); }
int mf_lambert_wm1(mfloat_t *mfloat) { return mfloat_apply_lambert_w_mpfr(mfloat, 1); }
int mf_beta(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_binary_mpfr(mfloat, other, mpfr_beta); }
int mf_logbeta(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_logbeta_mpfr(mfloat, other); }
int mf_binomial(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_binomial_mpfr(mfloat, other); }
int mf_beta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b) { return mfloat_apply_beta_pdf_mpfr(mfloat, a, b); }
int mf_logbeta_pdf(mfloat_t *mfloat, const mfloat_t *a, const mfloat_t *b) { return mfloat_apply_logbeta_pdf_mpfr(mfloat, a, b); }
int mf_normal_pdf(mfloat_t *mfloat) { return mfloat_apply_normal_pdf_mpfr(mfloat); }
int mf_normal_cdf(mfloat_t *mfloat) { return mfloat_apply_normal_cdf_mpfr(mfloat); }
int mf_normal_logpdf(mfloat_t *mfloat) { return mfloat_apply_normal_logpdf_mpfr(mfloat); }
int mf_productlog(mfloat_t *mfloat) { return mfloat_apply_lambert_w_mpfr(mfloat, 0); }
int mf_gammainc_lower(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_gammainc_mpfr(mfloat, other, 0); }
int mf_gammainc_upper(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_gammainc_mpfr(mfloat, other, 1); }
int mf_gammainc_P(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_gammainc_mpfr(mfloat, other, 2); }
int mf_gammainc_Q(mfloat_t *mfloat, const mfloat_t *other) { return mfloat_apply_gammainc_mpfr(mfloat, other, 3); }
int mf_ei(mfloat_t *mfloat) { return mfloat_apply_unary_mpfr(mfloat, mpfr_eint); }
int mf_e1(mfloat_t *mfloat) { return mfloat_apply_e1_mpfr(mfloat); }
