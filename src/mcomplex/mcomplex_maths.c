#include "mcomplex_internal.h"

#include <stdlib.h>

static const char *const mcomplex_erf_one_text =
    "0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885";
static const char *const mcomplex_erfc_one_text =
    "0.1572992070502851306587793649173907407039330020336970915400621021652827459039891587380166746518551115";

static int mcomplex_set_real_string_value(mcomplex_t *mcomplex, const char *real_text)
{
    size_t precision_bits;

    if (!mcomplex || !real_text)
        return -1;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;

    precision_bits = mc_get_precision(mcomplex);
    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0)
        return -1;
    if (mf_set_string(mcomplex->real, real_text) != 0)
        return -1;
    mf_clear(mcomplex->imag);
    return 0;
}

static int mcomplex_try_apply_real_erf_exact(mcomplex_t *mcomplex, int complement)
{
    mfloat_t *minus_one = NULL;
    const char *value_text = complement ? mcomplex_erfc_one_text : mcomplex_erf_one_text;
    int rc = -2;

    if (!mcomplex)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)))
        return -2;
    if (mf_eq(mc_real(mcomplex), MF_ONE))
        return mcomplex_set_real_string_value(mcomplex, value_text);

    minus_one = mf_create_long(-1);
    if (!minus_one)
        return -1;

    if (mf_eq(mc_real(mcomplex), minus_one)) {
        if (complement) {
            rc = mcomplex_set_real_string_value(mcomplex, "1.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885");
        } else {
            rc = mcomplex_set_real_string_value(mcomplex, "-0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885");
        }
    }

    mf_free(minus_one);
    return rc;
}

static int mcomplex_apply_real_unary(mcomplex_t *mcomplex, int (*fn)(mfloat_t *))
{
    size_t precision_bits;

    if (!mcomplex || !fn)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)))
        return -2;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    if (fn(mcomplex->real) != 0)
        return -1;
    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0)
        return -1;
    mf_clear(mcomplex->imag);
    return 0;
}

static int mcomplex_apply_real_binary(mcomplex_t *mcomplex,
                                      const mcomplex_t *other,
                                      int (*fn)(mfloat_t *, const mfloat_t *))
{
    mfloat_t *other_real = NULL;
    size_t precision_bits;
    int rc;

    if (!mcomplex || !other || !fn)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)) || !mf_is_zero(mc_imag(other)))
        return -2;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    precision_bits = mc_get_precision(mcomplex);

    other_real = mf_clone(mc_real(other));
    if (!other_real)
        return -1;

    rc = fn(mcomplex->real, other_real);
    mf_free(other_real);
    if (rc != 0)
        return -1;

    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0)
        return -1;
    mf_clear(mcomplex->imag);
    return 0;
}

static int mcomplex_apply_real_ternary(mcomplex_t *mcomplex,
                                       const mcomplex_t *a,
                                       const mcomplex_t *b,
                                       int (*fn)(mfloat_t *, const mfloat_t *, const mfloat_t *))
{
    mfloat_t *a_real = NULL;
    mfloat_t *b_real = NULL;
    size_t precision_bits;
    int rc;

    if (!mcomplex || !a || !b || !fn)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)) || !mf_is_zero(mc_imag(a)) || !mf_is_zero(mc_imag(b)))
        return -2;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    precision_bits = mc_get_precision(mcomplex);

    a_real = mf_clone(mc_real(a));
    b_real = mf_clone(mc_real(b));
    if (!a_real || !b_real) {
        mf_free(a_real);
        mf_free(b_real);
        return -1;
    }

    rc = fn(mcomplex->real, a_real, b_real);
    mf_free(a_real);
    mf_free(b_real);
    if (rc != 0)
        return -1;

    if (mf_set_precision(mcomplex->real, precision_bits) != 0 ||
        mf_set_precision(mcomplex->imag, precision_bits) != 0)
        return -1;
    mf_clear(mcomplex->imag);
    return 0;
}

static int mcomplex_apply_real_gammainc_half(mcomplex_t *mcomplex,
                                             const mcomplex_t *other,
                                             int use_upper,
                                             int use_unregularized)
{
    mfloat_t *x = NULL;
    mfloat_t *value = NULL;
    int rc;

    if (!mcomplex || !other)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)) || !mf_is_zero(mc_imag(other)))
        return -2;
    if (!mf_eq(mc_real(mcomplex), MF_HALF))
        return -2;
    if (mf_lt(mc_real(other), MF_ZERO))
        return -2;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;

    if (mf_is_zero(mc_real(other))) {
        if (use_upper) {
            value = mf_clone(use_unregularized ? MF_SQRT_PI : MF_ONE);

            if (!value)
                return -1;
            if (mf_set_precision(value, mc_get_precision(mcomplex)) != 0) {
                mf_free(value);
                return -1;
            }
            mf_free(mcomplex->real);
            mcomplex->real = value;
            rc = 0;
        } else {
            mf_clear(mcomplex->real);
            rc = 0;
        }
        if (rc != 0)
            return -1;
        mf_clear(mcomplex->imag);
        return 0;
    }

    if (mf_eq(mc_real(other), MF_ONE)) {
        static const char *const erf_one_text =
            "0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885";
        static const char *const erfc_one_text =
            "0.1572992070502851306587793649173907407039330020336970915400621021652827459039891587380166746518551115";

        value = mf_new_prec(mc_get_precision(mcomplex));
        if (!value)
            return -1;
        rc = mf_set_string(value, use_upper ? erfc_one_text : erf_one_text);
        if (rc == 0 && use_unregularized)
            rc = mf_mul(value, MF_SQRT_PI);
        if (rc != 0) {
            mf_free(value);
            return -1;
        }

        mf_free(mcomplex->real);
        mcomplex->real = value;
        mf_clear(mcomplex->imag);
        return 0;
    }

    x = mf_clone(mc_real(other));
    if (!x)
        return -1;
    if (mf_sqrt(x) != 0) {
        mf_free(x);
        return -1;
    }
    rc = use_upper ? mf_erfc(x) : mf_erf(x);
    if (rc == 0 && use_unregularized)
        rc = mf_mul(x, MF_SQRT_PI);
    if (rc != 0) {
        mf_free(x);
        return -1;
    }

    mf_free(mcomplex->real);
    mcomplex->real = x;
    mf_clear(mcomplex->imag);
    return 0;
}

int mc_exp(mcomplex_t *mcomplex)
{
    mfloat_t *scale = NULL;
    mfloat_t *new_real = NULL;
    mfloat_t *new_imag = NULL;
    mfloat_t *neg_pi = NULL;

    if (!mcomplex)
        return -1;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;

    if (mf_is_zero(mcomplex->real)) {
        neg_pi = mf_clone(MF_PI);
        if (!neg_pi)
            return -1;
        if (mf_neg(neg_pi) != 0) {
            mf_free(neg_pi);
            return -1;
        }
        if (mf_eq(mcomplex->imag, MF_PI) || mf_eq(mcomplex->imag, neg_pi)) {
            mf_free(neg_pi);
            if (mf_set_long(mcomplex->real, -1) != 0)
                return -1;
            mf_clear(mcomplex->imag);
            return 0;
        }
        mf_free(neg_pi);
    }

    scale = mf_clone(mcomplex->real);
    new_real = mf_clone(mcomplex->imag);
    new_imag = mf_clone(mcomplex->imag);
    if (!scale || !new_real || !new_imag)
        goto fail;

    if (mf_exp(scale) != 0 ||
        mf_cos(new_real) != 0 ||
        mf_sin(new_imag) != 0 ||
        mf_mul(new_real, scale) != 0 ||
        mf_mul(new_imag, scale) != 0)
        goto fail;

    mf_free(mcomplex->real);
    mf_free(mcomplex->imag);
    mcomplex->real = new_real;
    mcomplex->imag = new_imag;
    mf_free(scale);
    return 0;

fail:
    mf_free(scale);
    mf_free(new_real);
    mf_free(new_imag);
    return -1;
}

int mc_log(mcomplex_t *mcomplex)
{
    mfloat_t *mag = NULL;
    int rc;

    if (!mcomplex)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)))
        return mcomplex_apply_unary(mcomplex, qc_log);

    if (mf_gt(mc_real(mcomplex), MF_ZERO))
        return mcomplex_apply_real_unary(mcomplex, mf_log);

    if (mf_is_zero(mc_real(mcomplex))) {
        if (mcomplex_ensure_mutable(mcomplex) != 0)
            return -1;
        rc = mf_log(mcomplex->real);
        if (rc != 0)
            return rc;
        mf_clear(mcomplex->imag);
        return 0;
    }

    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    mag = mf_clone(mcomplex->real);
    if (!mag)
        return -1;
    if (mf_abs(mag) != 0 || mf_log(mag) != 0 ||
        mc_set(mcomplex, mag, MF_PI) != 0) {
        mf_free(mag);
        return -1;
    }
    mf_free(mag);
    return 0;
}

int mc_sin(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_sin);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_sin);
}

int mc_cos(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_cos);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_cos);
}

int mc_tan(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_tan);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_tan);
}

int mc_atan(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_atan);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_atan);
}

int mc_atan2(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_binary(mcomplex, other, mf_atan2);

    if (rc != -2)
        return rc;
    return mcomplex_apply_binary(mcomplex, other, qc_atan2);
}

int mc_asin(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_asin);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_asin);
}

int mc_acos(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_acos);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_acos);
}

int mc_sinh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_sinh);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_sinh);
}

int mc_cosh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_cosh);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_cosh);
}

int mc_tanh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_tanh);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_tanh);
}

int mc_asinh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_asinh);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_asinh);
}

int mc_acosh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_acosh);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_acosh);
}

int mc_atanh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_atanh);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_atanh);
}

int mc_gamma(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_gamma);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_gamma);
}

int mc_erf(mcomplex_t *mcomplex)
{
    int rc = mcomplex_try_apply_real_erf_exact(mcomplex, 0);

    if (rc != -2)
        return rc;
    rc = mcomplex_apply_real_unary(mcomplex, mf_erf);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_erf);
}

int mc_erfc(mcomplex_t *mcomplex)
{
    int rc = mcomplex_try_apply_real_erf_exact(mcomplex, 1);

    if (rc != -2)
        return rc;
    rc = mcomplex_apply_real_unary(mcomplex, mf_erfc);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_erfc);
}

int mc_erfinv(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_erfinv);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_erfinv);
}

int mc_erfcinv(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_erfcinv);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_erfcinv);
}

int mc_lgamma(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_lgamma);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_lgamma);
}

int mc_digamma(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_digamma);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_digamma);
}

int mc_trigamma(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_trigamma);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_trigamma);
}

int mc_tetragamma(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_tetragamma);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_tetragamma);
}

int mc_gammainv(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_gammainv);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_gammainv);
}

int mc_lambert_w0(mcomplex_t *mcomplex)
{
    int rc;

    if (!mcomplex)
        return -1;
    if (mf_is_zero(mc_imag(mcomplex))) {
        rc = mcomplex_apply_real_unary(mcomplex, mf_lambert_w0);
        if (rc != -2)
            return rc;
    }

    return mcomplex_apply_unary(mcomplex, qc_productlog);
}

int mc_lambert_wm1(mcomplex_t *mcomplex)
{
    int rc;

    if (!mcomplex)
        return -1;
    if (mf_is_zero(mc_imag(mcomplex))) {
        rc = mcomplex_apply_real_unary(mcomplex, mf_lambert_wm1);
        if (rc != -2)
            return rc;
    }
    return mcomplex_apply_unary(mcomplex, qc_lambert_wm1);
}

int mc_beta(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    mfloat_t *other_real = NULL;
    int rc;

    if (!mcomplex || !other)
        return -1;
    if (mf_is_zero(mc_imag(mcomplex)) && mf_is_zero(mc_imag(other))) {
        if (mcomplex_ensure_mutable(mcomplex) != 0)
            return -1;
        other_real = mf_clone(mc_real(other));
        if (!other_real)
            return -1;
        rc = mf_logbeta(mcomplex->real, other_real);
        if (rc == 0)
            rc = mf_exp(mcomplex->real);
        mf_free(other_real);
        if (rc != 0)
            return -1;
        mf_clear(mcomplex->imag);
        return 0;
    }
    return mcomplex_apply_binary(mcomplex, other, qc_beta);
}

int mc_logbeta(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_binary(mcomplex, other, mf_logbeta);

    if (rc != -2)
        return rc;
    return mcomplex_apply_binary(mcomplex, other, qc_logbeta);
}

int mc_binomial(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_binary(mcomplex, other, mf_binomial);

    if (rc != -2)
        return rc;
    return mcomplex_apply_binary(mcomplex, other, qc_binomial);
}

int mc_beta_pdf(mcomplex_t *mcomplex, const mcomplex_t *a, const mcomplex_t *b)
{
    int rc = mcomplex_apply_real_ternary(mcomplex, a, b, mf_beta_pdf);

    if (!mcomplex || !a || !b)
        return -1;
    if (rc != -2)
        return rc;
    return mc_set_qcomplex(
        mcomplex,
        qc_beta_pdf(mc_to_qcomplex(mcomplex), mc_to_qcomplex(a), mc_to_qcomplex(b)));
}

int mc_logbeta_pdf(mcomplex_t *mcomplex, const mcomplex_t *a, const mcomplex_t *b)
{
    int rc = mcomplex_apply_real_ternary(mcomplex, a, b, mf_logbeta_pdf);

    if (!mcomplex || !a || !b)
        return -1;
    if (rc != -2)
        return rc;
    return mc_set_qcomplex(
        mcomplex,
        qc_logbeta_pdf(mc_to_qcomplex(mcomplex), mc_to_qcomplex(a), mc_to_qcomplex(b)));
}

int mc_normal_pdf(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_normal_pdf);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_normal_pdf);
}

int mc_normal_cdf(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_normal_cdf);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_normal_cdf);
}

int mc_normal_logpdf(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_normal_logpdf);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_normal_logpdf);
}

int mc_productlog(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_productlog);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_productlog);
}

int mc_gammainc_lower(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_gammainc_half(mcomplex, other, 0, 1);

    if (rc != -2)
        return rc;
    rc = mcomplex_apply_real_binary(mcomplex, other, mf_gammainc_lower);

    if (rc != -2)
        return rc;
    return mcomplex_apply_binary(mcomplex, other, qc_gammainc_lower);
}

int mc_gammainc_upper(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_gammainc_half(mcomplex, other, 1, 1);

    if (rc != -2)
        return rc;
    rc = mcomplex_apply_real_binary(mcomplex, other, mf_gammainc_upper);

    if (rc != -2)
        return rc;
    return mcomplex_apply_binary(mcomplex, other, qc_gammainc_upper);
}

int mc_gammainc_P(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_gammainc_half(mcomplex, other, 0, 0);

    if (rc != -2)
        return rc;
    rc = mcomplex_apply_real_binary(mcomplex, other, mf_gammainc_P);

    if (rc != -2)
        return rc;
    return mcomplex_apply_binary(mcomplex, other, qc_gammainc_P);
}

int mc_gammainc_Q(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_gammainc_half(mcomplex, other, 1, 0);

    if (rc != -2)
        return rc;
    rc = mcomplex_apply_real_binary(mcomplex, other, mf_gammainc_Q);

    if (rc != -2)
        return rc;
    return mcomplex_apply_binary(mcomplex, other, qc_gammainc_Q);
}

int mc_ei(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_ei);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_ei);
}

int mc_e1(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_e1);

    if (rc != -2)
        return rc;
    return mcomplex_apply_unary(mcomplex, qc_e1);
}
