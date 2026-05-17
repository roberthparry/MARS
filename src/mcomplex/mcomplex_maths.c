#include "mcomplex_internal.h"

enum {
    MC_LAMBERT_W_HALLEY_MAX_STEPS = 80
};

static void mcomplex_prepare_constant(const mcomplex_t *mcomplex, mpfr_prec_t precision)
{
    if (mcomplex && mcomplex->constant_id != MCCONST_NONE)
        mcomplex_constant_ensure(mcomplex, precision);
}

static int mcomplex_apply_unary_mpc(mcomplex_t *mcomplex,
                                    int (*op)(mpc_ptr, mpc_srcptr, mpc_rnd_t))
{
    if (mcomplex_prepare_mutable(mcomplex) != 0 || !op)
        return -1;
    op(mcomplex->value, mcomplex->value, MPC_RNDNN);
    return 0;
}

static int mcomplex_apply_unary_qc(mcomplex_t *mcomplex, qcomplex_t (*fn)(qcomplex_t))
{
    if (mcomplex_prepare_mutable(mcomplex) != 0 || !fn)
        return -1;
    return mc_set_qcomplex(mcomplex, fn(mc_to_qcomplex(mcomplex)));
}

static int mcomplex_is_real(const mcomplex_t *mcomplex)
{
    return mcomplex && mpfr_zero_p(mpc_imagref(mcomplex->value));
}

static int mcomplex_set_real_mfloat(mcomplex_t *mcomplex, const mfloat_t *real)
{
    if (!mcomplex || !real)
        return -1;
    return mc_set(mcomplex, real, MF_ZERO);
}

static int mcomplex_apply_real_unary_mf(mcomplex_t *mcomplex,
                                        int (*mf_fn)(mfloat_t *),
                                        qcomplex_t (*qc_fn)(qcomplex_t))
{
    mfloat_t *real = NULL;
    int rc = -1;

    if (mcomplex_prepare_mutable(mcomplex) != 0 || !mf_fn || !qc_fn)
        return -1;
    if (!mcomplex_is_real(mcomplex))
        return mcomplex_apply_unary_qc(mcomplex, qc_fn);

    real = mf_clone(mc_real(mcomplex));
    if (!real)
        goto cleanup;
    if (mf_fn(real) != 0)
        goto cleanup;
    rc = mcomplex_set_real_mfloat(mcomplex, real);

cleanup:
    mf_free(real);
    return rc;
}

static int mcomplex_apply_lambert_w_mpc(mcomplex_t *mcomplex, int branch_m1)
{
    mpc_t z, w, ew, wew, f, wp1, f1, f2, ratio, corr, denom, delta, logz;
    mpfr_t abs_delta, tol, pi;
    mpfr_prec_t prec;
    int i;

    if (mcomplex_prepare_mutable(mcomplex) != 0)
        return -1;

    if (mcomplex_is_real(mcomplex))
        return mcomplex_apply_real_unary_mf(
            mcomplex,
            branch_m1 ? mf_lambert_wm1 : mf_lambert_w0,
            branch_m1 ? qc_lambert_wm1 : qc_productlog);

    prec = mpc_get_prec(mcomplex->value) + 128;
    mpc_init2(z, prec);
    mpc_init2(w, prec);
    mpc_init2(ew, prec);
    mpc_init2(wew, prec);
    mpc_init2(f, prec);
    mpc_init2(wp1, prec);
    mpc_init2(f1, prec);
    mpc_init2(f2, prec);
    mpc_init2(ratio, prec);
    mpc_init2(corr, prec);
    mpc_init2(denom, prec);
    mpc_init2(delta, prec);
    mpc_init2(logz, prec);
    mpfr_init2(abs_delta, prec);
    mpfr_init2(tol, prec);
    mpfr_init2(pi, prec);

    mpc_set(z, mcomplex->value, MPC_RNDNN);
    mpc_set(w, mcomplex->value, MPC_RNDNN);
    if (mc_set_qcomplex(mcomplex, branch_m1
                                      ? qc_lambert_wm1(mc_to_qcomplex(mcomplex))
                                      : qc_productlog(mc_to_qcomplex(mcomplex))) == 0) {
        mpc_set(w, mcomplex->value, MPC_RNDNN);
    } else {
        mpc_log(w, z, MPC_RNDNN);
        if (branch_m1) {
            mpc_set(logz, z, MPC_RNDNN);
            mpc_log(logz, logz, MPC_RNDNN);
            mpfr_const_pi(pi, MPFR_RNDN);
            mpfr_mul_2ui(pi, pi, 1u, MPFR_RNDN);
            mpfr_sub(mpc_imagref(w), mpc_imagref(w), pi, MPFR_RNDN);
            mpc_sub(w, w, logz, MPC_RNDNN);
        }
    }

    mpfr_set_ui(tol, 1u, MPFR_RNDN);
    mpfr_div_2ui(tol, tol, (unsigned long)(mpc_get_prec(mcomplex->value) + 32u), MPFR_RNDN);

    for (i = 0; i < MC_LAMBERT_W_HALLEY_MAX_STEPS; ++i) {
        mpc_exp(ew, w, MPC_RNDNN);
        mpc_mul(wew, w, ew, MPC_RNDNN);
        mpc_sub(f, wew, z, MPC_RNDNN);
        mpc_add_ui(wp1, w, 1u, MPC_RNDNN);
        mpc_mul(f1, ew, wp1, MPC_RNDNN);
        mpc_add_ui(f2, w, 2u, MPC_RNDNN);
        mpc_mul(f2, ew, f2, MPC_RNDNN);
        mpc_div(ratio, f, f1, MPC_RNDNN);
        mpc_mul(corr, ratio, f2, MPC_RNDNN);
        mpc_div_2ui(corr, corr, 1u, MPC_RNDNN);
        mpc_sub(denom, f1, corr, MPC_RNDNN);
        mpc_div(delta, f, denom, MPC_RNDNN);
        mpc_sub(w, w, delta, MPC_RNDNN);
        mpc_abs(abs_delta, delta, MPFR_RNDN);
        if (mpfr_cmp(abs_delta, tol) <= 0)
            break;
    }

    mpc_set(mcomplex->value, w, MPC_RNDNN);

    mpfr_clear(tol);
    mpfr_clear(abs_delta);
    mpfr_clear(pi);
    mpc_clear(logz);
    mpc_clear(delta);
    mpc_clear(denom);
    mpc_clear(corr);
    mpc_clear(ratio);
    mpc_clear(f2);
    mpc_clear(f1);
    mpc_clear(wp1);
    mpc_clear(f);
    mpc_clear(wew);
    mpc_clear(ew);
    mpc_clear(w);
    mpc_clear(z);
    return 0;
}

static int mcomplex_apply_binary_qc(mcomplex_t *mcomplex,
                                    const mcomplex_t *other,
                                    qcomplex_t (*fn)(qcomplex_t, qcomplex_t))
{
    if (mcomplex_prepare_mutable(mcomplex) != 0 || !other || !fn)
        return -1;
    mcomplex_prepare_constant(other, mpc_get_prec(mcomplex->value));
    return mc_set_qcomplex(mcomplex, fn(mc_to_qcomplex(mcomplex), mc_to_qcomplex(other)));
}

static int mcomplex_apply_real_binary_mf(mcomplex_t *mcomplex,
                                         const mcomplex_t *other,
                                         int (*mf_fn)(mfloat_t *, const mfloat_t *),
                                         qcomplex_t (*qc_fn)(qcomplex_t, qcomplex_t))
{
    mfloat_t *real = NULL;
    mfloat_t *other_real = NULL;
    int rc = -1;

    if (mcomplex_prepare_mutable(mcomplex) != 0 || !other || !mf_fn || !qc_fn)
        return -1;
    mcomplex_prepare_constant(other, mpc_get_prec(mcomplex->value));
    if (!mcomplex_is_real(mcomplex) || !mcomplex_is_real(other))
        return mcomplex_apply_binary_qc(mcomplex, other, qc_fn);

    real = mf_clone(mc_real(mcomplex));
    other_real = mf_clone(mc_real(other));
    if (!real || !other_real)
        goto cleanup;
    if (mf_fn(real, other_real) != 0)
        goto cleanup;
    rc = mcomplex_set_real_mfloat(mcomplex, real);

cleanup:
    mf_free(other_real);
    mf_free(real);
    return rc;
}

static int mcomplex_apply_ternary_qc(mcomplex_t *mcomplex,
                                     const mcomplex_t *a,
                                     const mcomplex_t *b,
                                     qcomplex_t (*fn)(qcomplex_t, qcomplex_t, qcomplex_t))
{
    if (mcomplex_prepare_mutable(mcomplex) != 0 || !a || !b || !fn)
        return -1;
    mcomplex_prepare_constant(a, mpc_get_prec(mcomplex->value));
    mcomplex_prepare_constant(b, mpc_get_prec(mcomplex->value));
    return mc_set_qcomplex(
        mcomplex,
        fn(mc_to_qcomplex(mcomplex), mc_to_qcomplex(a), mc_to_qcomplex(b)));
}

static int mcomplex_apply_real_ternary_mf(mcomplex_t *mcomplex,
                                          const mcomplex_t *a,
                                          const mcomplex_t *b,
                                          int (*mf_fn)(mfloat_t *,
                                                       const mfloat_t *,
                                                       const mfloat_t *),
                                          qcomplex_t (*qc_fn)(qcomplex_t,
                                                              qcomplex_t,
                                                              qcomplex_t))
{
    mfloat_t *real = NULL;
    mfloat_t *a_real = NULL;
    mfloat_t *b_real = NULL;
    int rc = -1;

    if (mcomplex_prepare_mutable(mcomplex) != 0 || !a || !b || !mf_fn || !qc_fn)
        return -1;
    mcomplex_prepare_constant(a, mpc_get_prec(mcomplex->value));
    mcomplex_prepare_constant(b, mpc_get_prec(mcomplex->value));
    if (!mcomplex_is_real(mcomplex) || !mcomplex_is_real(a) || !mcomplex_is_real(b))
        return mcomplex_apply_ternary_qc(mcomplex, a, b, qc_fn);

    real = mf_clone(mc_real(mcomplex));
    a_real = mf_clone(mc_real(a));
    b_real = mf_clone(mc_real(b));
    if (!real || !a_real || !b_real)
        goto cleanup;
    if (mf_fn(real, a_real, b_real) != 0)
        goto cleanup;
    rc = mcomplex_set_real_mfloat(mcomplex, real);

cleanup:
    mf_free(b_real);
    mf_free(a_real);
    mf_free(real);
    return rc;
}

int mc_floor(mcomplex_t *mcomplex)
{
    return mcomplex_apply_unary_qc(mcomplex, qc_floor);
}

int mc_hypot(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    return mcomplex_apply_binary_qc(mcomplex, other, qc_hypot);
}

int mc_exp(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_exp); }
int mc_log(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_log); }
int mc_log10(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_log10); }
int mc_sin(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_sin); }
int mc_cos(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_cos); }
int mc_tan(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_tan); }
int mc_atan(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_atan); }
int mc_atan2(mcomplex_t *mcomplex, const mcomplex_t *other)
{
    mfloat_t *real = NULL;
    mfloat_t *other_real = NULL;
    int rc = -1;

    if (mcomplex_prepare_mutable(mcomplex) != 0 || !other)
        return -1;
    mcomplex_prepare_constant(other, mpc_get_prec(mcomplex->value));
    if (mcomplex_is_real(mcomplex) && mcomplex_is_real(other)) {
        real = mf_clone(mc_real(mcomplex));
        other_real = mf_clone(mc_real(other));
        if (!real || !other_real)
            goto cleanup;
        if (mf_atan2(real, other_real) != 0)
            goto cleanup;
        rc = mcomplex_set_real_mfloat(mcomplex, real);
        goto cleanup;
    }

    mpc_div(mcomplex->value, mcomplex->value, other->value, MPC_RNDNN);
    mpc_atan(mcomplex->value, mcomplex->value, MPC_RNDNN);
    rc = 0;

cleanup:
    mf_free(other_real);
    mf_free(real);
    return rc;
}
int mc_asin(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_asin); }
int mc_acos(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_acos); }
int mc_sinh(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_sinh); }
int mc_cosh(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_cosh); }
int mc_tanh(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_tanh); }
int mc_asinh(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_asinh); }
int mc_acosh(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_acosh); }
int mc_atanh(mcomplex_t *mcomplex) { return mcomplex_apply_unary_mpc(mcomplex, mpc_atanh); }

int mc_gamma(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_gamma, qc_gamma); }
int mc_erf(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_erf, qc_erf); }
int mc_erfc(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_erfc, qc_erfc); }
int mc_erfinv(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_erfinv, qc_erfinv); }
int mc_erfcinv(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_erfcinv, qc_erfcinv); }
int mc_lgamma(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_lgamma, qc_lgamma); }
int mc_digamma(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_digamma, qc_digamma); }
int mc_trigamma(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_trigamma, qc_trigamma); }
int mc_tetragamma(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_tetragamma, qc_tetragamma); }
int mc_gammainv(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_gammainv, qc_gammainv); }
int mc_lambert_w0(mcomplex_t *mcomplex) { return mcomplex_apply_lambert_w_mpc(mcomplex, 0); }
int mc_lambert_wm1(mcomplex_t *mcomplex) { return mcomplex_apply_lambert_w_mpc(mcomplex, 1); }
int mc_beta(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_real_binary_mf(mcomplex, other, mf_beta, qc_beta); }
int mc_logbeta(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_real_binary_mf(mcomplex, other, mf_logbeta, qc_logbeta); }
int mc_binomial(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_real_binary_mf(mcomplex, other, mf_binomial, qc_binomial); }
int mc_beta_pdf(mcomplex_t *mcomplex, const mcomplex_t *a, const mcomplex_t *b) { return mcomplex_apply_real_ternary_mf(mcomplex, a, b, mf_beta_pdf, qc_beta_pdf); }
int mc_logbeta_pdf(mcomplex_t *mcomplex, const mcomplex_t *a, const mcomplex_t *b) { return mcomplex_apply_real_ternary_mf(mcomplex, a, b, mf_logbeta_pdf, qc_logbeta_pdf); }
int mc_normal_pdf(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_normal_pdf, qc_normal_pdf); }
int mc_normal_cdf(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_normal_cdf, qc_normal_cdf); }
int mc_normal_logpdf(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_normal_logpdf, qc_normal_logpdf); }
int mc_productlog(mcomplex_t *mcomplex) { return mcomplex_apply_lambert_w_mpc(mcomplex, 0); }
int mc_gammainc_lower(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_real_binary_mf(mcomplex, other, mf_gammainc_lower, qc_gammainc_lower); }
int mc_gammainc_upper(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_real_binary_mf(mcomplex, other, mf_gammainc_upper, qc_gammainc_upper); }
int mc_gammainc_P(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_real_binary_mf(mcomplex, other, mf_gammainc_P, qc_gammainc_P); }
int mc_gammainc_Q(mcomplex_t *mcomplex, const mcomplex_t *other) { return mcomplex_apply_real_binary_mf(mcomplex, other, mf_gammainc_Q, qc_gammainc_Q); }
int mc_ei(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_ei, qc_ei); }
int mc_e1(mcomplex_t *mcomplex) { return mcomplex_apply_real_unary_mf(mcomplex, mf_e1, qc_e1); }
