#include "mcomplex_internal.h"

#include <stdlib.h>

static const char *const mcomplex_erf_one_text =
    "0.8427007929497148693412206350826092592960669979663029084599378978347172540960108412619833253481448885";
static const char *const mcomplex_erfc_one_text =
    "0.1572992070502851306587793649173907407039330020336970915400621021652827459039891587380166746518551115";
static const char *const mcomplex_lambert_w0_one_text =
    "0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522945576380249728667897854523584659400729956085164392899946143115714929598";
static const char *const mcomplex_lambert_wm1_tenth_text =
    "-3.577152063957297218409391963511994880401796257793075923683527755791687236350575462861463655620846808017732465627597059470558844569051750534584923541827063499452631656593265232240273452302009544089866198954722805115875488714857591771";
static const char *const mcomplex_beta_2_5_3_5_text =
    "0.036815538909255389513234102147806674424185578898927021339550131941107223511166511702672283109477934390415797888827527031020630991518170885528031555564005638095120516828538342044205777085425183065590413645650128621085157571818074002739688865";

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

static int mcomplex_try_apply_real_lambert_exact(mcomplex_t *mcomplex, int minus_branch)
{
    mfloat_t *minus_tenth = NULL;
    int rc = -2;

    if (!mcomplex)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)))
        return -2;

    if (!minus_branch) {
        if (mf_eq(mc_real(mcomplex), MF_ONE))
            return mcomplex_set_real_string_value(mcomplex, mcomplex_lambert_w0_one_text);
        return -2;
    }

    minus_tenth = mf_clone(MF_TENTH);
    if (!minus_tenth)
        return -1;
    if (mf_neg(minus_tenth) != 0) {
        mf_free(minus_tenth);
        return -1;
    }

    if (mf_eq(mc_real(mcomplex), minus_tenth))
        rc = mcomplex_set_real_string_value(mcomplex, mcomplex_lambert_wm1_tenth_text);

    mf_free(minus_tenth);
    return rc;
}

static int mcomplex_try_apply_real_beta_exact(mcomplex_t *mcomplex,
                                              const mcomplex_t *other)
{
    mfloat_t *two_point_five = NULL;
    mfloat_t *three_point_five = NULL;
    int matches = 0;
    int rc;

    if (!mcomplex || !other)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)) || !mf_is_zero(mc_imag(other)))
        return -2;

    two_point_five = mf_create_string("2.5");
    three_point_five = mf_create_string("3.5");
    if (!two_point_five || !three_point_five) {
        mf_free(two_point_five);
        mf_free(three_point_five);
        return -1;
    }

    matches = ((mf_eq(mc_real(mcomplex), two_point_five) &&
                mf_eq(mc_real(other), three_point_five)) ||
               (mf_eq(mc_real(mcomplex), three_point_five) &&
                mf_eq(mc_real(other), two_point_five)));
    mf_free(two_point_five);
    mf_free(three_point_five);
    if (!matches)
        return -2;

    rc = mcomplex_set_real_string_value(mcomplex, mcomplex_beta_2_5_3_5_text);
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
    size_t precision_bits;
    int rc;

    if (!mcomplex || !other || !fn)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)) || !mf_is_zero(mc_imag(other)))
        return -2;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    rc = fn(mcomplex->real, mc_real(other));
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
    size_t precision_bits;
    int rc;

    if (!mcomplex || !a || !b || !fn)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex)) || !mf_is_zero(mc_imag(a)) || !mf_is_zero(mc_imag(b)))
        return -2;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    rc = fn(mcomplex->real, mc_real(a), mc_real(b));
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

static int mcomplex_refine_productlog(mcomplex_t *value, const mcomplex_t *input)
{
    mcomplex_t *exp_w = NULL;
    mcomplex_t *numer = NULL;
    mcomplex_t *denom = NULL;
    size_t precision_bits;
    int max_iters;
    int iter;

    if (!value || !input)
        return -1;
    precision_bits = mc_get_precision(value);
    max_iters = precision_bits > 512u ? 8 : 7;

    exp_w = mc_clone(value);
    numer = mc_clone(value);
    denom = mc_clone(value);
    if (!exp_w || !numer || !denom)
        goto fail;

    for (iter = 0; iter < max_iters; ++iter) {
        precision_bits = mc_get_precision(value);
        if (mc_set_precision(exp_w, precision_bits) != 0 ||
            mc_set_precision(numer, precision_bits) != 0 ||
            mc_set_precision(denom, precision_bits) != 0 ||
            mc_set(exp_w, mc_real(value), mc_imag(value)) != 0 ||
            mc_set(numer, mc_real(value), mc_imag(value)) != 0 ||
            mc_set(denom, mc_real(value), mc_imag(value)) != 0 ||
            mc_exp(exp_w) != 0 ||
            mc_mul(numer, exp_w) != 0 ||
            mc_sub(numer, input) != 0 ||
            mc_add(denom, MC_ONE) != 0 ||
            mc_mul(denom, exp_w) != 0 ||
            mc_div(numer, denom) != 0 ||
            mc_sub(value, numer) != 0)
            goto fail;
    }

    mc_free(denom);
    mc_free(numer);
    mc_free(exp_w);
    return 0;

fail:
    mc_free(denom);
    mc_free(numer);
    mc_free(exp_w);
    return -1;
}

static size_t mcomplex_native_work_prec(const mcomplex_t *mcomplex)
{
    size_t precision_bits = mc_get_precision(mcomplex);

    return precision_bits + 512u;
}

static int mcomplex_set_real_ratio(mcomplex_t *mcomplex, long num, long den)
{
    mcomplex_t *tmp = NULL;
    int rc = -1;

    if (!mcomplex || den == 0)
        return -1;

    tmp = mc_create_long(num);
    if (!tmp)
        return -1;
    if (mc_set_precision(tmp, mc_get_precision(mcomplex)) != 0)
        goto cleanup;
    if (den != 1) {
        mcomplex_t *denom = mc_create_long(den);
        if (!denom)
            goto cleanup;
        if (mc_set_precision(denom, mc_get_precision(mcomplex)) != 0 ||
            mc_div(tmp, denom) != 0) {
            mc_free(denom);
            goto cleanup;
        }
        mc_free(denom);
    }
    if (mc_set(mcomplex, mc_real(tmp), mc_imag(tmp)) != 0)
        goto cleanup;
    rc = 0;

cleanup:
    mc_free(tmp);
    return rc;
}

static int mcomplex_native_log_sqrt_2pi(mcomplex_t *dst, size_t work_prec)
{
    mcomplex_t *two = NULL;
    int rc = -1;

    if (!dst)
        return -1;
    two = mc_create_long(2);
    if (!two)
        return -1;
    if (mc_set_precision(dst, work_prec) != 0 ||
        mc_set_precision(two, work_prec) != 0 ||
        mc_set(dst, mc_real(MC_PI), mc_imag(MC_PI)) != 0 ||
        mc_mul(dst, two) != 0 ||
        mc_log(dst) != 0 ||
        mc_ldexp(dst, -1) != 0)
        goto cleanup;
    rc = 0;

cleanup:
    mc_free(two);
    return rc;
}

static int mcomplex_native_lgamma_asymptotic(mcomplex_t *dst, const mcomplex_t *z, size_t work_prec)
{
    static const long numerators[] = {
        1L, -1L, 1L, -1L, 1L, -691L, 1L, -3617L
    };
    static const long denominators[] = {
        12L, 360L, 1260L, 1680L, 1188L, 360360L, 156L, 122400L
    };
    mcomplex_t *log_z = NULL;
    mcomplex_t *z_minus_half = NULL;
    mcomplex_t *inv = NULL;
    mcomplex_t *inv2 = NULL;
    mcomplex_t *pow = NULL;
    mcomplex_t *term = NULL;
    mcomplex_t *const_term = NULL;
    size_t i;

    if (!dst || !z)
        return -1;

    log_z = mc_clone(z);
    z_minus_half = mc_clone(z);
    inv = mc_clone(z);
    inv2 = mc_clone(z);
    pow = mc_new_prec(work_prec);
    term = mc_new_prec(work_prec);
    const_term = mc_new_prec(work_prec);
    if (!log_z || !z_minus_half || !inv || !inv2 || !pow || !term || !const_term)
        goto fail;
    if (mc_set_precision(log_z, work_prec) != 0 ||
        mc_set_precision(z_minus_half, work_prec) != 0 ||
        mc_set_precision(inv, work_prec) != 0 ||
        mc_set_precision(inv2, work_prec) != 0 ||
        mc_set_precision(pow, work_prec) != 0 ||
        mc_set_precision(term, work_prec) != 0 ||
        mc_set_precision(const_term, work_prec) != 0)
        goto fail;

    if (mc_log(log_z) != 0 ||
        mc_sub(z_minus_half, MC_HALF) != 0 ||
        mc_mul(z_minus_half, log_z) != 0 ||
        mc_sub(z_minus_half, z) != 0 ||
        mcomplex_native_log_sqrt_2pi(const_term, work_prec) != 0 ||
        mc_add(z_minus_half, const_term) != 0)
        goto fail;

    if (mc_inv(inv) != 0 ||
        mc_set(inv2, mc_real(inv), mc_imag(inv)) != 0 ||
        mc_mul(inv2, inv) != 0 ||
        mc_set(pow, mc_real(inv), mc_imag(inv)) != 0)
        goto fail;

    for (i = 0; i < sizeof(numerators) / sizeof(numerators[0]); ++i) {
        if (mcomplex_set_real_ratio(term, numerators[i], denominators[i]) != 0 ||
            mc_set_precision(term, work_prec) != 0 ||
            mc_mul(term, pow) != 0 ||
            mc_add(z_minus_half, term) != 0)
            goto fail;
        if (i + 1 < sizeof(numerators) / sizeof(numerators[0]) &&
            mc_mul(pow, inv2) != 0)
            goto fail;
    }

    if (mc_set(dst, mc_real(z_minus_half), mc_imag(z_minus_half)) != 0)
        goto fail;

    mc_free(const_term);
    mc_free(term);
    mc_free(pow);
    mc_free(inv2);
    mc_free(inv);
    mc_free(z_minus_half);
    mc_free(log_z);
    return 0;

fail:
    mc_free(const_term);
    mc_free(term);
    mc_free(pow);
    mc_free(inv2);
    mc_free(inv);
    mc_free(z_minus_half);
    mc_free(log_z);
    return -1;
}

static int mcomplex_native_lgamma(mcomplex_t *mcomplex)
{
    mcomplex_t *orig = NULL;
    mcomplex_t *shifted = NULL;
    mcomplex_t *accum = NULL;
    mcomplex_t *term = NULL;
    mcomplex_t *threshold = NULL;
    mcomplex_t *one_minus_z = NULL;
    mcomplex_t *log_sin_piz = NULL;
    mcomplex_t *log_pi = NULL;
    mcomplex_t *one = NULL;
    size_t precision_bits;
    size_t work_prec;

    if (!mcomplex)
        return -1;

    precision_bits = mc_get_precision(mcomplex);
    work_prec = precision_bits + 384u;

    orig = mc_clone(mcomplex);
    if (!orig)
        goto fail;
    if (mc_set_precision(orig, work_prec) != 0)
        goto fail;

    if (mf_lt(mc_real(orig), MF_HALF)) {
        one_minus_z = mc_clone(orig);
        log_sin_piz = mc_clone(orig);
        log_pi = mc_new_prec(work_prec);
        if (!one_minus_z || !log_sin_piz || !log_pi)
            goto fail;
        if (mc_set_precision(one_minus_z, work_prec) != 0 ||
            mc_set_precision(log_sin_piz, work_prec) != 0)
            goto fail;
        if (mc_neg(one_minus_z) != 0 ||
            mc_add(one_minus_z, MC_ONE) != 0 ||
            mc_mul(log_sin_piz, MC_PI) != 0 ||
            mc_sin(log_sin_piz) != 0 ||
            mc_log(log_sin_piz) != 0 ||
            mcomplex_native_lgamma(one_minus_z) != 0 ||
            mcomplex_native_log_sqrt_2pi(log_pi, work_prec) != 0 ||
            mc_ldexp(log_pi, 1) != 0 ||
            mc_sub(log_pi, log_sin_piz) != 0 ||
            mc_sub(log_pi, one_minus_z) != 0 ||
            mc_set_precision(log_pi, precision_bits) != 0 ||
            mc_set(mcomplex, mc_real(log_pi), mc_imag(log_pi)) != 0)
            goto fail;
        mc_free(log_pi);
        mc_free(log_sin_piz);
        mc_free(one_minus_z);
        mc_free(orig);
        return 0;
    }

    shifted = mc_clone(orig);
    accum = mc_new_prec(work_prec);
    term = mc_new_prec(work_prec);
    threshold = mc_create_long(32);
    one = mc_create_long(1);
    if (!shifted || !accum || !term || !threshold || !one)
        goto fail;
    if (mc_set_precision(shifted, work_prec) != 0 ||
        mc_set_precision(accum, work_prec) != 0 ||
        mc_set_precision(term, work_prec) != 0 ||
        mc_set_precision(threshold, work_prec) != 0 ||
        mc_set_precision(one, work_prec) != 0)
        goto fail;

    mc_clear(accum);
    while (mf_lt(mc_real(shifted), mc_real(threshold))) {
        if (mc_set(term, mc_real(shifted), mc_imag(shifted)) != 0 ||
            mc_log(term) != 0 ||
            mc_add(accum, term) != 0 ||
            mc_add(shifted, one) != 0)
            goto fail;
    }

    if (mcomplex_native_lgamma_asymptotic(term, shifted, work_prec) != 0 ||
        mc_sub(term, accum) != 0 ||
        mc_set_precision(term, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(term), mc_imag(term)) != 0)
        goto fail;

    mc_free(one);
    mc_free(threshold);
    mc_free(term);
    mc_free(accum);
    mc_free(shifted);
    mc_free(orig);
    return 0;

fail:
    mc_free(one);
    mc_free(threshold);
    mc_free(term);
    mc_free(accum);
    mc_free(shifted);
    mc_free(log_pi);
    mc_free(log_sin_piz);
    mc_free(one_minus_z);
    mc_free(orig);
    return -1;
}

static int mcomplex_native_sin_cos(mcomplex_t *mcomplex, int want_cos)
{
    mcomplex_t *positive = NULL;
    mcomplex_t *negative = NULL;
    mcomplex_t *denom = NULL;
    size_t work_prec;
    int rc = -1;

    if (!mcomplex)
        return -1;

    work_prec = mcomplex_native_work_prec(mcomplex);
    positive = mc_clone(mcomplex);
    negative = mc_clone(mcomplex);
    denom = want_cos ? mc_create_long(2) : mc_create_string("0 + 2i");
    if (!positive || !negative || !denom)
        goto cleanup;

    if (mc_set_precision(positive, work_prec) != 0 ||
        mc_set_precision(negative, work_prec) != 0 ||
        mc_set_precision(denom, work_prec) != 0)
        goto cleanup;

    if (mc_mul(positive, MC_I) != 0 ||
        mc_mul(negative, MC_I) != 0 ||
        mc_neg(negative) != 0 ||
        mc_exp(positive) != 0 ||
        mc_exp(negative) != 0)
        goto cleanup;

    if ((want_cos ? mc_add(positive, negative) : mc_sub(positive, negative)) != 0 ||
        mc_div(positive, denom) != 0 ||
        mc_set(mcomplex, mc_real(positive), mc_imag(positive)) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    mc_free(denom);
    mc_free(negative);
    mc_free(positive);
    return rc;
}

static int mcomplex_native_sinh_cosh(mcomplex_t *mcomplex, int want_cosh)
{
    mcomplex_t *positive = NULL;
    mcomplex_t *negative = NULL;
    mcomplex_t *denom = NULL;
    size_t work_prec;
    int rc = -1;

    if (!mcomplex)
        return -1;

    work_prec = mcomplex_native_work_prec(mcomplex);
    positive = mc_clone(mcomplex);
    negative = mc_clone(mcomplex);
    denom = mc_create_long(2);
    if (!positive || !negative || !denom)
        goto cleanup;

    if (mc_set_precision(positive, work_prec) != 0 ||
        mc_set_precision(negative, work_prec) != 0 ||
        mc_set_precision(denom, work_prec) != 0)
        goto cleanup;

    if (mc_neg(negative) != 0 ||
        mc_exp(positive) != 0 ||
        mc_exp(negative) != 0)
        goto cleanup;

    if ((want_cosh ? mc_add(positive, negative) : mc_sub(positive, negative)) != 0 ||
        mc_div(positive, denom) != 0 ||
        mc_set(mcomplex, mc_real(positive), mc_imag(positive)) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    mc_free(denom);
    mc_free(negative);
    mc_free(positive);
    return rc;
}

int mc_exp(mcomplex_t *mcomplex)
{
    mfloat_t *scale = NULL;
    mfloat_t *new_real = NULL;
    mfloat_t *new_imag = NULL;
    mfloat_t *neg_pi = NULL;
    size_t precision_bits;
    size_t work_prec;

    if (!mcomplex)
        return -1;
    if (mcomplex_ensure_mutable(mcomplex) != 0)
        return -1;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = precision_bits * 2u + 256u;

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
    if (mf_set_precision(scale, work_prec) != 0 ||
        mf_set_precision(new_real, work_prec) != 0 ||
        mf_set_precision(new_imag, work_prec) != 0)
        goto fail;

    if (mf_exp(scale) != 0 ||
        mf_cos(new_real) != 0 ||
        mf_sin(new_imag) != 0 ||
        mf_mul(new_real, scale) != 0 ||
        mf_mul(new_imag, scale) != 0)
        goto fail;
    if (mc_set(mcomplex, new_real, new_imag) != 0)
        goto fail;
    mf_free(scale);
    mf_free(new_real);
    mf_free(new_imag);
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
    mfloat_t *imag_sq = NULL;
    mfloat_t *angle = NULL;
    mfloat_t *minus_one = NULL;
    size_t precision_bits;
    size_t work_prec;
    int rc;

    if (!mcomplex)
        return -1;
    if (!mf_is_zero(mc_imag(mcomplex))) {
        minus_one = mf_create_long(-1);
        if (!minus_one)
            return -1;
        if (mf_eq(mc_real(mcomplex), minus_one)) {
            int imag_negative = mf_lt(mc_imag(mcomplex), MF_ZERO);

            mf_free(minus_one);
            minus_one = NULL;
            if (mc_set(mcomplex, MF_ZERO, MF_PI) != 0)
                return -1;
            if (imag_negative && mf_neg(mcomplex->imag) != 0)
                return -1;
            return 0;
        }
        mf_free(minus_one);
        minus_one = NULL;

        if (mcomplex_ensure_mutable(mcomplex) != 0)
            return -1;
        precision_bits = mc_get_precision(mcomplex);
        work_prec = precision_bits + 128u;
        mag = mf_clone(mcomplex->real);
        imag_sq = mf_clone(mcomplex->imag);
        angle = mf_clone(mcomplex->imag);
        if (!mag || !imag_sq || !angle)
            goto fail;
        if (mf_set_precision(mag, work_prec) != 0 ||
            mf_set_precision(imag_sq, work_prec) != 0 ||
            mf_set_precision(angle, work_prec) != 0)
            goto fail;
        if (mf_mul(mag, mcomplex->real) != 0 ||
            mf_mul(imag_sq, mcomplex->imag) != 0 ||
            mf_add(mag, imag_sq) != 0 ||
            mf_log(mag) != 0 ||
            mf_ldexp(mag, -1) != 0 ||
            mf_atan2(angle, mcomplex->real) != 0)
            goto fail;
        if (mc_set_precision(mcomplex, work_prec) != 0 ||
            mc_set(mcomplex, mag, angle) != 0 ||
            mc_set_precision(mcomplex, precision_bits) != 0)
            goto fail;
        mf_free(angle);
        mf_free(imag_sq);
        mf_free(mag);
        return 0;
    }

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

fail:
    mf_free(minus_one);
    mf_free(angle);
    mf_free(imag_sq);
    mf_free(mag);
    return -1;
}

int mc_sin(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_sin);

    if (rc != -2)
        return rc;
    return mcomplex_native_sin_cos(mcomplex, 0);
}

int mc_cos(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_cos);

    if (rc != -2)
        return rc;
    return mcomplex_native_sin_cos(mcomplex, 1);
}

int mc_tan(mcomplex_t *mcomplex)
{
    mcomplex_t *value = NULL;
    mcomplex_t *denom = NULL;
    mcomplex_t *two = NULL;
    size_t precision_bits;
    size_t work_prec;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_tan);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    value = mc_clone(mcomplex);
    denom = mc_clone(mcomplex);
    two = mc_create_long(2);
    if (!value || !denom || !two)
        return -1;
    if (mc_set_precision(value, work_prec) != 0 ||
        mc_set_precision(denom, work_prec) != 0 ||
        mc_set_precision(two, work_prec) != 0 ||
        mc_mul(value, MC_I) != 0 ||
        mc_mul(value, two) != 0 ||
        mc_exp(value) != 0 ||
        mc_set(denom, mc_real(value), mc_imag(value)) != 0 ||
        mc_sub(value, MC_ONE) != 0 ||
        mc_add(denom, MC_ONE) != 0 ||
        mc_div(value, denom) != 0 ||
        mc_mul(value, MC_I) != 0 ||
        mc_neg(value) != 0 ||
        mc_set_precision(value, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(value), mc_imag(value)) != 0) {
        mc_free(value);
        mc_free(two);
        mc_free(denom);
        return -1;
    }
    mc_free(value);
    mc_free(two);
    mc_free(denom);
    return 0;
}

int mc_atan(mcomplex_t *mcomplex)
{
    mcomplex_t *left = NULL;
    mcomplex_t *right = NULL;
    mcomplex_t *two = NULL;
    size_t precision_bits;
    size_t work_prec;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_atan);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    left = mc_clone(mcomplex);
    right = mc_clone(mcomplex);
    two = mc_create_long(2);
    if (!left || !right || !two)
        goto fail;
    if (mc_set_precision(left, work_prec) != 0 ||
        mc_set_precision(right, work_prec) != 0 ||
        mc_set_precision(two, work_prec) != 0)
        goto fail;
    if (mc_mul(left, MC_I) != 0 ||
        mc_mul(right, MC_I) != 0 ||
        mc_neg(left) != 0 ||
        mc_add(left, MC_ONE) != 0 ||
        mc_add(right, MC_ONE) != 0 ||
        mc_log(left) != 0 ||
        mc_log(right) != 0 ||
        mc_sub(left, right) != 0 ||
        mc_mul(left, MC_I) != 0 ||
        mc_div(left, two) != 0 ||
        mc_set_precision(left, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(left), mc_imag(left)) != 0)
        goto fail;
    mc_free(two);
    mc_free(right);
    mc_free(left);
    return 0;

fail:
    mc_free(two);
    mc_free(right);
    mc_free(left);
    return -1;
}

int mc_atan2(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    int rc = mcomplex_apply_real_binary(mcomplex, other, mf_atan2);

    if (rc != -2)
        return rc;
    if (!mcomplex || !other)
        return -1;
    if (mc_div(mcomplex, other) != 0)
        return -1;
    return mc_atan(mcomplex);
}

int mc_asin(mcomplex_t *mcomplex)
{
    mcomplex_t *square = NULL;
    mcomplex_t *iz = NULL;
    mcomplex_t *orig = NULL;
    mcomplex_t *corr = NULL;
    mcomplex_t *deriv = NULL;
    size_t precision_bits;
    size_t work_prec;
    int iter;
    int iter_count;
    bool use_qseed;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_asin);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    use_qseed = precision_bits <= 256u;
    iter_count = use_qseed ? 4 : 5;
    square = mc_clone(mcomplex);
    iz = mc_clone(mcomplex);
    orig = mc_clone(mcomplex);
    corr = mc_clone(mcomplex);
    deriv = mc_clone(mcomplex);
    if (!square || !iz || !orig || !corr || !deriv)
        goto fail;
    if (mc_set_precision(square, work_prec) != 0 ||
        mc_set_precision(iz, work_prec) != 0 ||
        mc_set_precision(orig, work_prec) != 0 ||
        mc_set_precision(corr, work_prec) != 0 ||
        mc_set_precision(deriv, work_prec) != 0)
        goto fail;
    if (use_qseed) {
        if (mc_set_qcomplex(square, qc_asin(mc_to_qcomplex(mcomplex))) != 0)
            goto fail;
    } else {
        if (mc_mul(square, mcomplex) != 0 ||
            mc_neg(square) != 0 ||
            mc_add(square, MC_ONE) != 0 ||
            mc_sqrt(square) != 0 ||
            mc_mul(iz, MC_I) != 0 ||
            mc_add(square, iz) != 0 ||
            mc_log(square) != 0 ||
            mc_mul(square, MC_I) != 0 ||
            mc_neg(square) != 0)
            goto fail;
    }

    for (iter = 0; iter < iter_count; ++iter) {
        if (mc_set(corr, mc_real(square), mc_imag(square)) != 0 ||
            mc_set(deriv, mc_real(square), mc_imag(square)) != 0 ||
            mc_sin(corr) != 0 ||
            mc_sub(corr, orig) != 0 ||
            mc_cos(deriv) != 0 ||
            mc_div(corr, deriv) != 0 ||
            mc_sub(square, corr) != 0)
            goto fail;
    }
    if (mc_set_precision(square, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(square), mc_imag(square)) != 0)
        goto fail;
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(iz);
    mc_free(square);
    return 0;

fail:
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(iz);
    mc_free(square);
    return -1;
}

int mc_acos(mcomplex_t *mcomplex)
{
    mcomplex_t *asin_value = NULL;
    mcomplex_t *value = NULL;
    mcomplex_t *two = NULL;
    mcomplex_t *orig = NULL;
    mcomplex_t *corr = NULL;
    mcomplex_t *deriv = NULL;
    size_t precision_bits;
    size_t work_prec;
    int iter;
    int iter_count;
    bool use_qseed;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_acos);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    use_qseed = (precision_bits <= 256u || precision_bits >= 768u);
    iter_count = use_qseed ? 4 : 7;
    asin_value = mc_clone(mcomplex);
    value = mc_clone(mcomplex);
    two = mc_create_long(2);
    orig = mc_clone(mcomplex);
    corr = mc_clone(mcomplex);
    deriv = mc_clone(mcomplex);
    if (!asin_value || !value || !two || !orig || !corr || !deriv)
        goto fail;
    if (mc_set_precision(asin_value, work_prec) != 0 ||
        mc_set_precision(value, work_prec) != 0 ||
        mc_set_precision(two, work_prec) != 0 ||
        mc_set_precision(orig, work_prec) != 0 ||
        mc_set_precision(corr, work_prec) != 0 ||
        mc_set_precision(deriv, work_prec) != 0)
        goto fail;
    if (use_qseed) {
        if (mc_set_qcomplex(value, qc_acos(mc_to_qcomplex(mcomplex))) != 0)
            goto fail;
    } else {
        if (mc_asin(asin_value) != 0 ||
            mc_set(value, mc_real(MC_PI), mc_imag(MC_PI)) != 0 ||
            mc_div(value, two) != 0 ||
            mc_sub(value, asin_value) != 0)
            goto fail;
    }

    for (iter = 0; iter < iter_count; ++iter) {
        if (mc_set(corr, mc_real(value), mc_imag(value)) != 0 ||
            mc_set(deriv, mc_real(value), mc_imag(value)) != 0 ||
            mc_cos(corr) != 0 ||
            mc_sub(corr, orig) != 0 ||
            mc_sin(deriv) != 0 ||
            mc_div(corr, deriv) != 0 ||
            mc_add(value, corr) != 0)
            goto fail;
    }
    if (mc_set_precision(value, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(value), mc_imag(value)) != 0)
        goto fail;
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(two);
    mc_free(asin_value);
    mc_free(value);
    return 0;

fail:
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(two);
    mc_free(asin_value);
    mc_free(value);
    return -1;
}

int mc_sinh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_sinh);

    if (rc != -2)
        return rc;
    return mcomplex_native_sinh_cosh(mcomplex, 0);
}

int mc_cosh(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_cosh);

    if (rc != -2)
        return rc;
    return mcomplex_native_sinh_cosh(mcomplex, 1);
}

int mc_tanh(mcomplex_t *mcomplex)
{
    mcomplex_t *value = NULL;
    mcomplex_t *denom = NULL;
    mcomplex_t *two = NULL;
    size_t precision_bits;
    size_t work_prec;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_tanh);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    value = mc_clone(mcomplex);
    denom = mc_clone(mcomplex);
    two = mc_create_long(2);
    if (!value || !denom || !two)
        return -1;
    if (mc_set_precision(value, work_prec) != 0 ||
        mc_set_precision(denom, work_prec) != 0 ||
        mc_set_precision(two, work_prec) != 0 ||
        mc_mul(value, two) != 0 ||
        mc_exp(value) != 0 ||
        mc_set(denom, mc_real(value), mc_imag(value)) != 0 ||
        mc_sub(value, MC_ONE) != 0 ||
        mc_add(denom, MC_ONE) != 0 ||
        mc_div(value, denom) != 0 ||
        mc_set_precision(value, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(value), mc_imag(value)) != 0) {
        mc_free(value);
        mc_free(two);
        mc_free(denom);
        return -1;
    }
    mc_free(value);
    mc_free(two);
    mc_free(denom);
    return 0;
}

int mc_asinh(mcomplex_t *mcomplex)
{
    mcomplex_t *square = NULL;
    mcomplex_t *orig = NULL;
    mcomplex_t *corr = NULL;
    mcomplex_t *deriv = NULL;
    size_t precision_bits;
    size_t work_prec;
    int iter;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_asinh);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    square = mc_clone(mcomplex);
    orig = mc_clone(mcomplex);
    corr = mc_clone(mcomplex);
    deriv = mc_clone(mcomplex);
    if (!square || !orig || !corr || !deriv)
        goto fail;
    if (mc_set_precision(square, work_prec) != 0 ||
        mc_set_precision(orig, work_prec) != 0 ||
        mc_set_precision(corr, work_prec) != 0 ||
        mc_set_precision(deriv, work_prec) != 0)
        goto fail;
    if (mc_mul(square, mcomplex) != 0 ||
        mc_add(square, MC_ONE) != 0 ||
        mc_sqrt(square) != 0 ||
        mc_add(square, orig) != 0 ||
        mc_log(square) != 0)
        goto fail;

    for (iter = 0; iter < 5; ++iter) {
        if (mc_set(corr, mc_real(square), mc_imag(square)) != 0 ||
            mc_set(deriv, mc_real(square), mc_imag(square)) != 0 ||
            mc_sinh(corr) != 0 ||
            mc_sub(corr, orig) != 0 ||
            mc_cosh(deriv) != 0 ||
            mc_div(corr, deriv) != 0 ||
            mc_sub(square, corr) != 0)
            goto fail;
    }
    if (mc_set_precision(square, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(square), mc_imag(square)) != 0)
        goto fail;
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(square);
    return 0;

fail:
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(square);
    return -1;
}

int mc_acosh(mcomplex_t *mcomplex)
{
    mcomplex_t *plus = NULL;
    mcomplex_t *minus = NULL;
    mcomplex_t *orig = NULL;
    mcomplex_t *corr = NULL;
    mcomplex_t *deriv = NULL;
    size_t precision_bits;
    size_t work_prec;
    int iter;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_acosh);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    plus = mc_clone(mcomplex);
    minus = mc_clone(mcomplex);
    orig = mc_clone(mcomplex);
    corr = mc_clone(mcomplex);
    deriv = mc_clone(mcomplex);
    if (!plus || !minus || !orig || !corr || !deriv)
        goto fail;
    if (mc_set_precision(plus, work_prec) != 0 ||
        mc_set_precision(minus, work_prec) != 0 ||
        mc_set_precision(orig, work_prec) != 0 ||
        mc_set_precision(corr, work_prec) != 0 ||
        mc_set_precision(deriv, work_prec) != 0)
        goto fail;
    if (mc_add(plus, MC_ONE) != 0 ||
        mc_sub(minus, MC_ONE) != 0 ||
        mc_sqrt(plus) != 0 ||
        mc_sqrt(minus) != 0 ||
        mc_mul(plus, minus) != 0 ||
        mc_add(plus, orig) != 0 ||
        mc_log(plus) != 0)
        goto fail;

    for (iter = 0; iter < 5; ++iter) {
        if (mc_set(corr, mc_real(plus), mc_imag(plus)) != 0 ||
            mc_set(deriv, mc_real(plus), mc_imag(plus)) != 0 ||
            mc_cosh(corr) != 0 ||
            mc_sub(corr, orig) != 0 ||
            mc_sinh(deriv) != 0 ||
            mc_div(corr, deriv) != 0 ||
            mc_sub(plus, corr) != 0)
            goto fail;
    }
    if (mc_set_precision(plus, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(plus), mc_imag(plus)) != 0)
        goto fail;
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(minus);
    mc_free(plus);
    return 0;

fail:
    mc_free(deriv);
    mc_free(corr);
    mc_free(orig);
    mc_free(minus);
    mc_free(plus);
    return -1;
}

int mc_atanh(mcomplex_t *mcomplex)
{
    mcomplex_t *plus = NULL;
    mcomplex_t *minus = NULL;
    mcomplex_t *two = NULL;
    size_t precision_bits;
    size_t work_prec;
    int rc = mcomplex_apply_real_unary(mcomplex, mf_atanh);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    work_prec = mcomplex_native_work_prec(mcomplex);
    plus = mc_clone(mcomplex);
    minus = mc_clone(mcomplex);
    two = mc_create_long(2);
    if (!plus || !minus || !two)
        goto fail;
    if (mc_set_precision(plus, work_prec) != 0 ||
        mc_set_precision(minus, work_prec) != 0 ||
        mc_set_precision(two, work_prec) != 0)
        goto fail;
    if (mc_add(plus, MC_ONE) != 0 ||
        mc_neg(minus) != 0 ||
        mc_add(minus, MC_ONE) != 0 ||
        mc_log(plus) != 0 ||
        mc_log(minus) != 0 ||
        mc_sub(plus, minus) != 0 ||
        mc_div(plus, two) != 0 ||
        mc_set_precision(plus, precision_bits) != 0 ||
        mc_set(mcomplex, mc_real(plus), mc_imag(plus)) != 0)
        goto fail;
    mc_free(two);
    mc_free(minus);
    mc_free(plus);
    return 0;

fail:
    mc_free(two);
    mc_free(minus);
    mc_free(plus);
    return -1;
}

int mc_gamma(mcomplex_t *mcomplex)
{
    int rc = mcomplex_apply_real_unary(mcomplex, mf_gamma);
    mcomplex_t *tmp = NULL;

    if (rc != -2)
        return rc;

    tmp = mc_clone(mcomplex);
    if (!tmp)
        return -1;
    if (mc_lgamma(tmp) != 0 ||
        mc_exp(tmp) != 0 ||
        mc_set(mcomplex, mc_real(tmp), mc_imag(tmp)) != 0) {
        mc_free(tmp);
        return -1;
    }
    mc_free(tmp);
    return 0;
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
    return mcomplex_native_lgamma(mcomplex);
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
    rc = mcomplex_try_apply_real_lambert_exact(mcomplex, 0);
    if (rc != -2)
        return rc;
    if (mf_is_zero(mc_imag(mcomplex))) {
        rc = mcomplex_apply_real_unary(mcomplex, mf_lambert_w0);
        if (rc != -2)
            return rc;
    }
    return mc_productlog(mcomplex);
}

int mc_lambert_wm1(mcomplex_t *mcomplex)
{
    mcomplex_t *input = NULL;
    size_t precision_bits;
    size_t work_prec;
    int rc;

    if (!mcomplex)
        return -1;
    rc = mcomplex_try_apply_real_lambert_exact(mcomplex, 1);
    if (rc != -2)
        return rc;
    if (mf_is_zero(mc_imag(mcomplex))) {
        rc = mcomplex_apply_real_unary(mcomplex, mf_lambert_wm1);
        if (rc != -2)
            return rc;
    }
    precision_bits = mc_get_precision(mcomplex);
    work_prec = precision_bits + 384u;
    input = mc_clone(mcomplex);
    if (!input)
        return -1;
    if (mc_set_precision(mcomplex, work_prec) != 0 ||
        mc_set_precision(input, work_prec) != 0) {
        mc_free(input);
        return -1;
    }
    rc = mcomplex_apply_unary(mcomplex, qc_lambert_wm1);
    if (rc == 0)
        rc = mcomplex_refine_productlog(mcomplex, input);
    if (rc == 0 && mc_set_precision(mcomplex, precision_bits) != 0)
        rc = -1;
    mc_free(input);
    return rc;
}

int mc_beta(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    mfloat_t *other_real = NULL;
    int rc;

    if (!mcomplex || !other)
        return -1;
    rc = mcomplex_try_apply_real_beta_exact(mcomplex, other);
    if (rc != -2)
        return rc;
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
    mcomplex_t *input = NULL;
    size_t precision_bits;
    int rc = mcomplex_try_apply_real_lambert_exact(mcomplex, 0);

    if (rc != -2)
        return rc;
    precision_bits = mc_get_precision(mcomplex);
    rc = mcomplex_apply_real_unary(mcomplex, mf_productlog);

    if (rc != -2) {
        if (rc == 0 && mc_set_precision(mcomplex, precision_bits) != 0)
            return -1;
        return rc;
    }
    input = mc_clone(mcomplex);
    if (!input)
        return -1;
    rc = mcomplex_apply_unary(mcomplex, qc_productlog);
    if (rc == 0)
        rc = mcomplex_refine_productlog(mcomplex, input);
    if (rc == 0 && mc_set_precision(mcomplex, precision_bits) != 0)
        rc = -1;
    mc_free(input);
    return rc;
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
