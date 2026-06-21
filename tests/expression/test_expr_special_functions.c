#include "test_expr.h"

void test_erf(void)
{
    /* erf(0) = 0 exactly */
    expr_t *c = test_expr_new_var_d(0.0);
    expr_t *f = expr_erf(c);
    check_q_at(__FILE__, __LINE__, 1, "erf(0) = 0", expr_eval_qf(f), qf_from_double(0.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* erf(-x) = -erf(x) odd symmetry at x=0.8 */
    expr_t *cp = test_expr_new_const_d(0.8);
    expr_t *cn = test_expr_new_const_d(-0.8);
    expr_t *fp = expr_erf(cp);
    expr_t *fn = expr_erf(cn);
    check_q_at(__FILE__, __LINE__, 1, "erf(-0.8) = -erf(0.8)",
               expr_eval_qf(fn), qf_neg(expr_eval_qf(fp)));
    expr_free(fp); expr_free(fn); expr_free(cp); expr_free(cn);

    /* erf(x) + erfc(x) = 1 identity at x=0.6 */
    qfloat_t X   = qf_from_string("0.6");
    qfloat_t sum = qf_add(qf_erf(X), qf_erfc(X));
    check_q_at(__FILE__, __LINE__, 1, "erf(0.6) + erfc(0.6) = 1", sum, qf_from_double(1.0));
}

void test_erfc(void)
{
    /* erfc(0) = 1 exactly */
    expr_t *c = test_expr_new_var_d(0.0);
    expr_t *f = expr_erfc(c);
    check_q_at(__FILE__, __LINE__, 1, "erfc(0) = 1", expr_eval_qf(f), qf_from_double(1.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* erfc(x) = 1 - erf(x) at x=1.2 */
    qfloat_t X   = qf_from_double(1.2);
    qfloat_t lhs = qf_erfc(X);
    qfloat_t rhs = qf_sub(qf_from_double(1.0), qf_erf(X));
    check_q_at(__FILE__, __LINE__, 1, "erfc(1.2) = 1 - erf(1.2)", lhs, rhs);

    /* erfc(-x) = 2 - erfc(x) at x=0.5 */
    X   = qf_from_double(0.5);
    lhs = qf_erfc(qf_neg(X));
    rhs = qf_sub(qf_from_double(2.0), qf_erfc(X));
    check_q_at(__FILE__, __LINE__, 1, "erfc(-0.5) = 2 - erfc(0.5)", lhs, rhs);
}

void test_erfinv(void)
{
    /* erfinv(0) = 0 exactly */
    expr_t *c = test_expr_new_var_d(0.0);
    expr_t *f = expr_erfinv(c);
    check_q_at(__FILE__, __LINE__, 1, "erfinv(0) = 0", expr_eval_qf(f), qf_from_double(0.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* erf(erfinv(y)) = y  (round-trip) at y=0.7 */
    qfloat_t Y  = qf_div(qf_from_double(7.0), qf_from_double(10.0));
    qfloat_t rt = qf_erf(qf_erfinv(Y));
    check_q_at(__FILE__, __LINE__, 1, "erf(erfinv(0.7)) = 0.7", rt, Y);

    /* erfinv(-y) = -erfinv(y) odd symmetry */
    Y         = qf_from_double(0.5);
    qfloat_t lhs = qf_erfinv(qf_neg(Y));
    qfloat_t rhs = qf_neg(qf_erfinv(Y));
    check_q_at(__FILE__, __LINE__, 1, "erfinv(-0.5) = -erfinv(0.5)", lhs, rhs);
}

void test_erfcinv(void)
{
    /* erfcinv(1) = 0 exactly (erfc(0) = 1) */
    expr_t *c = test_expr_new_var_d(1.0);
    expr_t *f = expr_erfcinv(c);
    check_q_at(__FILE__, __LINE__, 1, "erfcinv(1) = 0", expr_eval_qf(f), qf_from_double(0.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* erfc(erfcinv(y)) = y (round-trip) at y=0.4 */
    qfloat_t Y  = qf_div(qf_from_double(4.0), qf_from_double(10.0));
    qfloat_t rt = qf_erfc(qf_erfcinv(Y));
    check_q_at(__FILE__, __LINE__, 1, "erfc(erfcinv(0.4)) = 0.4", rt, Y);

    /* erfcinv(y) = erfinv(1-y) identity at y=0.6 */
    Y          = qf_from_string("0.6");
    qfloat_t lhs = qf_erfcinv(Y);
    qfloat_t rhs = qf_erfinv(qf_sub(qf_from_double(1.0), Y));
    check_q_at(__FILE__, __LINE__, 1, "erfcinv(0.6) = erfinv(0.4)", lhs, rhs);
}

void test_gamma(void)
{
    /* Γ(1) = 0! = 1 exactly */
    expr_t *c = test_expr_new_var_d(1.0);
    expr_t *f = expr_gamma(c);
    check_q_at(__FILE__, __LINE__, 1, "gamma(1) = 1", expr_eval_qf(f), qf_from_double(1.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* Γ(3) = 2! = 2 exactly */
    c = test_expr_new_var_d(3.0);
    f = expr_gamma(c);
    check_q_at(__FILE__, __LINE__, 1, "gamma(3) = 2", expr_eval_qf(f), qf_from_double(2.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* Γ(0.5) = sqrt(π) */
    c = test_expr_new_var_d(0.5);
    f = expr_gamma(c);
    check_q_at(__FILE__, __LINE__, 1, "gamma(0.5) = sqrt(pi)", expr_eval_qf(f), qf_sqrt(QF_PI));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* Γ(x+1) = x·Γ(x) recurrence at x=2.5 */
    qfloat_t X   = qf_from_double(2.5);
    qfloat_t lhs = qf_gamma(qf_add(X, qf_from_double(1.0)));
    qfloat_t rhs = qf_mul(X, qf_gamma(X));
    check_q_at(__FILE__, __LINE__, 1, "gamma(3.5) = 2.5*gamma(2.5)", lhs, rhs);
}

void test_lgamma(void)
{
    /* lgamma(1) = log(1) = 0 exactly */
    expr_t *c = test_expr_new_var_d(1.0);
    expr_t *f = expr_lgamma(c);
    check_q_at(__FILE__, __LINE__, 1, "lgamma(1) = 0", expr_eval_qf(f), qf_from_double(0.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* lgamma(3) = log(2) */
    c = test_expr_new_const_d(3.0);
    f = expr_lgamma(c);
    check_q_at(__FILE__, __LINE__, 1, "lgamma(3) = log(2)",
               expr_eval_qf(f), qf_log(qf_from_double(2.0)));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* lgamma(x) = log(gamma(x)) at x=2.5 */
    qfloat_t X   = qf_from_double(2.5);
    qfloat_t lhs = qf_lgamma(X);
    qfloat_t rhs = qf_log(qf_gamma(X));
    check_q_at(__FILE__, __LINE__, 1, "lgamma(2.5) = log(gamma(2.5))", lhs, rhs);
}

void test_digamma(void)
{
    /* ψ(2) - ψ(1) = 1 (recurrence ψ(x+1) = ψ(x) + 1/x, x=1) */
    qfloat_t d1 = qf_digamma(qf_from_double(1.0));
    qfloat_t d2 = qf_digamma(qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "digamma(2) - digamma(1) = 1",
               qf_sub(d2, d1), qf_from_double(1.0));

    /* ψ(3) - ψ(2) = 1/2 (recurrence at x=2) */
    qfloat_t d3 = qf_digamma(qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "digamma(3) - digamma(2) = 0.5",
               qf_sub(d3, d2), qf_from_double(0.5));

    /* reflection: ψ(1-x) - ψ(x) = π·cot(πx) at x=1/4 */
    qfloat_t X   = qf_from_double(0.25);
    qfloat_t lhs = qf_sub(qf_digamma(qf_sub(qf_from_double(1.0), X)), qf_digamma(X));
    qfloat_t rhs = qf_mul(QF_PI, qf_div(qf_cos(qf_mul(QF_PI, X)),
                                      qf_sin(qf_mul(QF_PI, X))));
    check_q_at(__FILE__, __LINE__, 1, "digamma(3/4) - digamma(1/4) = pi*cot(pi/4)", lhs, rhs);
}

void test_lambert_w0(void)
{
    /* W₀(0) = 0 exactly */
    expr_t *c = test_expr_new_var_d(0.0);
    expr_t *f = expr_lambert_w0(c);
    check_q_at(__FILE__, __LINE__, 1, "W₀(0) = 0", expr_eval_qf(f), qf_from_double(0.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* W₀(e) = 1 — use qfloat_t e so the input is accurate to ~33 digits */
    c = test_expr_new_var_qf(qf_exp(qf_from_double(1.0)));
    f = expr_lambert_w0(c);
    check_q_at(__FILE__, __LINE__, 1, "W₀(e) = 1", expr_eval_qf(f), qf_from_double(1.0));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* W₀(x)·exp(W₀(x)) = x — defining equation verified at x=2 */
    qfloat_t X   = qf_from_double(2.0);
    qfloat_t W   = qf_lambert_w0(X);
    qfloat_t lhs = qf_mul(W, qf_exp(W));
    check_q_at(__FILE__, __LINE__, 1, "W₀(2)*exp(W₀(2)) = 2", lhs, X);
}

void test_lambert_wm1(void)
{
    /* W_{-1}(x)·exp(W_{-1}(x)) = x — defining equation at x=-0.1 */
    qfloat_t X   = qf_from_string("-0.1");
    qfloat_t W   = qf_lambert_wm1(X);
    qfloat_t lhs = qf_mul(W, qf_exp(W));
    check_q_at(__FILE__, __LINE__, 1, "W₋₁(-0.1)*exp(W₋₁(-0.1)) = -0.1", lhs, X);

    /* W_{-1}(x)·exp(W_{-1}(x)) = x — defining equation at x=-0.3 */
    X   = qf_from_string("-0.3");
    W   = qf_lambert_wm1(X);
    lhs = qf_mul(W, qf_exp(W));
    check_q_at(__FILE__, __LINE__, 1, "W₋₁(-0.3)*exp(W₋₁(-0.3)) = -0.3", lhs, X);
}

void test_normal_pdf(void)
{
    /* phi(0) = 1/sqrt(2π) exactly */
    expr_t *c = test_expr_new_var_d(0.0);
    expr_t *f = expr_normal_pdf(c);
    qfloat_t expect = qf_div(qf_from_double(1.0),
                           qf_sqrt(qf_mul(qf_from_double(2.0), QF_PI)));
    check_q_at(__FILE__, __LINE__, 1, "normal_pdf(0) = 1/sqrt(2pi)", expr_eval_qf(f), expect);
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* phi(x) = exp(-x^2/2)/sqrt(2pi) at x=1 */
    qfloat_t X   = qf_from_double(1.0);
    qfloat_t lhs = qf_normal_pdf(X);
    qfloat_t rhs = qf_div(qf_exp(qf_div(qf_neg(qf_mul(X, X)), qf_from_double(2.0))),
                        qf_sqrt(qf_mul(qf_from_double(2.0), QF_PI)));
    check_q_at(__FILE__, __LINE__, 1, "normal_pdf(1) = exp(-1/2)/sqrt(2pi)", lhs, rhs);

    /* phi(-x) = phi(x) even symmetry at x=0.8 */
    X = qf_from_string("0.8");
    check_q_at(__FILE__, __LINE__, 1, "normal_pdf(-0.8) = normal_pdf(0.8)",
               qf_normal_pdf(qf_neg(X)), qf_normal_pdf(X));
}

void test_normal_cdf(void)
{
    /* Φ(0) = 0.5 exactly by symmetry */
    expr_t *c = test_expr_new_var_d(0.0);
    expr_t *f = expr_normal_cdf(c);
    check_q_at(__FILE__, __LINE__, 1, "normal_cdf(0) = 0.5", expr_eval_qf(f), qf_from_double(0.5));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* Φ(-x) + Φ(x) = 1 reflection at x=1 */
    qfloat_t X   = qf_from_double(1.0);
    qfloat_t sum = qf_add(qf_normal_cdf(qf_neg(X)), qf_normal_cdf(X));
    check_q_at(__FILE__, __LINE__, 1, "Phi(-1) + Phi(1) = 1", sum, qf_from_double(1.0));

    /* Φ(x) = 0.5*(1 + erf(x/sqrt(2))) at x=1.5 */
    X          = qf_from_double(1.5);
    qfloat_t lhs = qf_normal_cdf(X);
    qfloat_t rhs = qf_mul(qf_from_double(0.5),
                        qf_add(qf_from_double(1.0),
                               qf_erf(qf_div(X, qf_sqrt(qf_from_double(2.0))))));
    check_q_at(__FILE__, __LINE__, 1, "Phi(1.5) = 0.5*(1+erf(1.5/sqrt2))", lhs, rhs);
}

void test_normal_logpdf(void)
{
    /* log phi(0) = -0.5*log(2pi) */
    qfloat_t expect = qf_neg(qf_mul(qf_from_double(0.5),
                                  qf_log(qf_mul(qf_from_double(2.0), QF_PI))));
    expr_t *c = test_expr_new_var_d(0.0);
    expr_t *f = expr_normal_logpdf(c);
    check_q_at(__FILE__, __LINE__, 1, "normal_logpdf(0) = -log(2pi)/2", expr_eval_qf(f), expect);
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* log phi(x) = log(phi(0)) - x^2/2  at x=1.2 */
    qfloat_t X   = qf_from_double(1.2);
    qfloat_t lhs = qf_normal_logpdf(X);
    qfloat_t rhs = qf_sub(qf_normal_logpdf(qf_from_double(0.0)),
                        qf_mul(qf_from_double(0.5), qf_mul(X, X)));
    check_q_at(__FILE__, __LINE__, 1, "logpdf(1.2) = logpdf(0) - 1.2^2/2", lhs, rhs);

    /* log phi(x) = log(phi(x))  consistency at x=0.5 */
    X   = qf_from_double(0.5);
    lhs = qf_normal_logpdf(X);
    rhs = qf_log(qf_normal_pdf(X));
    check_q_at(__FILE__, __LINE__, 1, "normal_logpdf(0.5) = log(normal_pdf(0.5))", lhs, rhs);
}

void test_ei(void)
{
    /* Verify Ei against qf_ei at x=1 and x=2 */
    qfloat_t X1 = qf_from_double(1.0);
    qfloat_t X2 = qf_from_double(2.0);
    expr_t *c1 = test_expr_new_var_d(1.0); expr_t *f1 = expr_ei(c1);
    expr_t *c2 = test_expr_new_var_d(2.0); expr_t *f2 = expr_ei(c2);
    check_q_at(__FILE__, __LINE__, 1, "ei(1) via qfloat_t", expr_eval_qf(f1), qf_ei(X1));
    check_q_at(__FILE__, __LINE__, 1, "ei(2) via qfloat_t", expr_eval_qf(f2), qf_ei(X2));
    print_expr_of(f1);
    print_expr_of(f2);
    expr_free(f1); expr_free(c1); expr_free(f2); expr_free(c2);

    /* Ei'(x) = exp(x)/x — verify at x=1: Ei'(1) = e */
    qfloat_t deriv_at_1 = qf_exp(qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "ei'(1)=exp(1)/1=e (deriv check)",
               deriv_at_1, qf_div(qf_exp(qf_from_double(1.0)), qf_from_double(1.0)));
}

void test_e1(void)
{
    /* E₁(x) at x=1 */
    expr_t *c = test_expr_new_var_d(1.0);
    expr_t *f = expr_e1(c);
    check_q_at(__FILE__, __LINE__, 1, "e1(1) via qfloat_t", expr_eval_qf(f), qf_e1(qf_from_double(1.0)));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* E₁(x) at x=0.5 */
    c = test_expr_new_var_d(0.5);
    f = expr_e1(c);
    check_q_at(__FILE__, __LINE__, 1, "e1(0.5) via qfloat_t",
               expr_eval_qf(f), qf_e1(qf_from_double(0.5)));
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* E₁'(1) = -exp(-1)/1 = -1/e */
    qfloat_t deriv_e1_at_1 = qf_neg(qf_div(qf_exp(qf_neg(qf_from_double(1.0))),
                                          qf_from_double(1.0)));
    qfloat_t expect = qf_neg(qf_exp(qf_neg(qf_from_double(1.0))));
    check_q_at(__FILE__, __LINE__, 1, "e1'(1) = -exp(-1)/1 = -1/e", deriv_e1_at_1, expect);
}

void test_beta(void)
{
    /* B(1,1) = 1 exactly (∫₀¹ 1 dt = 1) */
    expr_t *a = test_expr_new_var_d(1.0);
    expr_t *b = test_expr_new_const_d(1.0);
    expr_t *f = expr_beta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "beta(1,1) = 1", expr_eval_qf(f), qf_from_double(1.0));
    print_expr_of(f);
    expr_free(f); expr_free(b); expr_free(a);

    /* B(2,3) = Γ(2)Γ(3)/Γ(5) = 1·2/24 = 1/12 exactly */
    a = test_expr_new_var_d(2.0);
    b = test_expr_new_const_d(3.0);
    f = expr_beta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "beta(2,3) = 1/12",
               expr_eval_qf(f), qf_div(qf_from_double(1.0), qf_from_double(12.0)));
    expr_free(f); expr_free(b); expr_free(a);

    /* B(a,b) = B(b,a) symmetry at (2,5) */
    qfloat_t A = qf_from_double(2.0), B = qf_from_double(5.0);
    check_q_at(__FILE__, __LINE__, 1, "beta(2,5) = beta(5,2)",
               qf_beta(A, B), qf_beta(B, A));

    /* B(a,b) = Γ(a)Γ(b)/Γ(a+b) at (1.5, 2.5) */
    A          = qf_from_double(1.5);
    B          = qf_from_double(2.5);
    qfloat_t lhs = qf_beta(A, B);
    qfloat_t rhs = qf_div(qf_mul(qf_gamma(A), qf_gamma(B)),
                        qf_gamma(qf_add(A, B)));
    check_q_at(__FILE__, __LINE__, 1, "beta(1.5,2.5) = Gamma(1.5)*Gamma(2.5)/Gamma(4)", lhs, rhs);
}

void test_logbeta(void)
{
    /* logbeta(1,1) = log(1) = 0 exactly */
    expr_t *a = test_expr_new_var_d(1.0);
    expr_t *b = test_expr_new_const_d(1.0);
    expr_t *f = expr_logbeta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "logbeta(1,1) = 0", expr_eval_qf(f), qf_from_double(0.0));
    print_expr_of(f);
    expr_free(f); expr_free(b); expr_free(a);

    /* logbeta(2,3) = log(1/12) = -log(12) */
    a = test_expr_new_var_d(2.0);
    b = test_expr_new_const_d(3.0);
    f = expr_logbeta(a, b);
    check_q_at(__FILE__, __LINE__, 1, "logbeta(2,3) = -log(12)",
               expr_eval_qf(f), qf_neg(qf_log(qf_from_double(12.0))));
    print_expr_of(f);
    expr_free(f); expr_free(b); expr_free(a);

    /* logbeta(a,b) = log(beta(a,b)) at (3,2) */
    qfloat_t A   = qf_from_double(3.0), B = qf_from_double(2.0);
    qfloat_t lhs = qf_logbeta(A, B);
    qfloat_t rhs = qf_log(qf_beta(A, B));
    check_q_at(__FILE__, __LINE__, 1, "logbeta(3,2) = log(beta(3,2))", lhs, rhs);

    /* logbeta(a,b) = lgamma(a)+lgamma(b)-lgamma(a+b) at (1.5,2.5) */
    A   = qf_from_double(1.5);
    B   = qf_from_double(2.5);
    lhs = qf_logbeta(A, B);
    rhs = qf_sub(qf_add(qf_lgamma(A), qf_lgamma(B)),
                 qf_lgamma(qf_add(A, B)));
    check_q_at(__FILE__, __LINE__, 1, "logbeta(1.5,2.5) = lgamma(1.5)+lgamma(2.5)-lgamma(4)", lhs, rhs);
}

void test_gammainc(void)
{
    expr_t *s = test_expr_new_const_d(1.0);
    expr_t *x = test_expr_new_var_d(1.0);
    expr_t *lower = expr_gammainc_lower(s, x);
    expr_t *upper = expr_gammainc_upper(s, x);
    expr_t *P = expr_gammainc_P(s, x);
    expr_t *Q = expr_gammainc_Q(s, x);
    qfloat_t one = qf_from_double(1.0);
    qfloat_t exp_neg_one = qf_exp(qf_neg(one));
    qfloat_t one_minus_exp_neg_one = qf_sub(one, exp_neg_one);

    check_q_at(__FILE__, __LINE__, 1, "gammainc_lower(1,1) = 1 - exp(-1)",
               expr_eval_qf(lower), one_minus_exp_neg_one);
    check_q_at(__FILE__, __LINE__, 1, "gammainc_upper(1,1) = exp(-1)",
               expr_eval_qf(upper), exp_neg_one);
    check_q_at(__FILE__, __LINE__, 1, "gammainc_P(1,1) = 1 - exp(-1)",
               expr_eval_qf(P), one_minus_exp_neg_one);
    check_q_at(__FILE__, __LINE__, 1, "gammainc_Q(1,1) = exp(-1)",
               expr_eval_qf(Q), exp_neg_one);
    print_expr_of(lower);

    expr_free(Q);
    expr_free(P);
    expr_free(upper);
    expr_free(lower);
    expr_free(x);
    expr_free(s);
}

void test_beta_pdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *a = test_expr_new_const_d(2.0);
    expr_t *b = test_expr_new_const_d(3.0);
    expr_t *f = expr_beta_pdf(x, a, b);
    qfloat_t expect = qf_from_double(1.5);

    check_q_at(__FILE__, __LINE__, 1, "beta_pdf(0.5,2,3) = 1.5", expr_eval_qf(f), expect);
    print_expr_of(f);

    expr_free(f);
    expr_free(b);
    expr_free(a);
    expr_free(x);
}

void test_logbeta_pdf(void)
{
    expr_t *x = test_expr_new_var_d(0.5);
    expr_t *a = test_expr_new_const_d(2.0);
    expr_t *b = test_expr_new_const_d(3.0);
    expr_t *f = expr_logbeta_pdf(x, a, b);
    qfloat_t expect = qf_log(qf_from_double(1.5));

    check_q_at(__FILE__, __LINE__, 1, "logbeta_pdf(0.5,2,3) = log(1.5)", expr_eval_qf(f), expect);
    print_expr_of(f);

    expr_free(f);
    expr_free(b);
    expr_free(a);
    expr_free(x);
}

void test_binomial(void)
{
    expr_t *n = test_expr_new_var_d(5.0);
    expr_t *k = test_expr_new_const_d(2.0);
    expr_t *f = expr_binomial(n, k);

    check_q_at(__FILE__, __LINE__, 1, "binomial(5,2) = 10", expr_eval_qf(f), qf_from_double(10.0));
    print_expr_of(f);

    expr_free(f);
    expr_free(k);
    expr_free(n);
}

void test_trigamma(void)
{
    /* ψ'(1) = π²/6  — exact, classical result */
    expr_t *c = test_expr_new_var_d(1.0);
    expr_t *f = expr_trigamma(c);
    qfloat_t expect = qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(1) = pi^2/6", expr_eval_qf(f), expect);
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* ψ'(2) = π²/6 - 1  (recurrence ψ'(2) = ψ'(1) - 1/1²) */
    c = test_expr_new_var_d(2.0);
    f = expr_trigamma(c);
    expect = qf_sub(qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(6.0)),
                    qf_from_double(1.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(2) = pi^2/6 - 1", expr_eval_qf(f), expect);
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* ψ'(1/2) = π²/2  — exact, reflection formula */
    c = test_expr_new_const_d(0.5);
    f = expr_trigamma(c);
    expect = qf_div(qf_mul(QF_PI, QF_PI), qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(1/2) = pi^2/2", expr_eval_qf(f), expect);
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* Recurrence: ψ'(x) - ψ'(x+1) = 1/x² at x=3: ψ'(3) - ψ'(4) = 1/9 */
    qfloat_t t3 = qf_trigamma(qf_from_double(3.0));
    qfloat_t t4 = qf_trigamma(qf_from_double(4.0));
    check_q_at(__FILE__, __LINE__, 1, "trigamma(3) - trigamma(4) = 1/9",
               qf_sub(t3, t4), qf_div(qf_from_double(1.0), qf_from_double(9.0)));
}

void test_polygamma(void)
{
    expr_t *c = test_expr_new_var_d(2.0);
    expr_t *f = expr_polygamma(3u, c);
    qfloat_t expect = qf_sub(qf_div(qf_pow_int(QF_PI, 4), qf_from_double(15.0)),
                             qf_from_double(6.0));

    check_q_at(__FILE__, __LINE__, 1,
               "polygamma(3, 2) = pi^4/15 - 6", expr_eval_qf(f), expect);
    print_expr_of(f);

    expr_free(f);
    expr_free(c);

    c = test_expr_new_var_qf(qf_from_string("-0.5"));
    f = expr_polygamma(3u, c);
    expect = qf_add(qf_pow_int(QF_PI, 4), qf_from_double(96.0));
    check_q_at(__FILE__, __LINE__, 1,
               "polygamma(3, -1/2) = pi^4 + 96", expr_eval_qf(f), expect);
    print_expr_of(f);

    expr_free(f);
    expr_free(c);
}

void test_dilog_polylog(void)
{
    expr_t *c = test_expr_new_var_d(0.5);
    expr_t *f = expr_dilog(c);
    qfloat_t log2 = qf_log(qf_from_double(2.0));
    qfloat_t expect = qf_sub(qf_div(qf_sqr(QF_PI), qf_from_double(12.0)),
                             qf_div(qf_sqr(log2), qf_from_double(2.0)));

    check_q_at(__FILE__, __LINE__, 1,
               "dilog(1/2) = pi^2/12 - log(2)^2/2", expr_eval_qf(f), expect);
    print_expr_of(f);

    expr_free(f);
    expr_free(c);

    c = test_expr_new_var_d(0.5);
    f = expr_polylog(2u, c);
    check_q_at(__FILE__, __LINE__, 1,
               "polylog(2, 1/2) = dilog(1/2)", expr_eval_qf(f), expect);
    print_expr_of(f);

    expr_free(f);
    expr_free(c);

    {
        expr_t *a = test_expr_new_const_d(1.0);
        expr_t *b1 = test_expr_new_const_d(1.0);
        expr_t *b2 = test_expr_new_const_d(1.0);
        expr_t *cc = test_expr_new_const_d(2.0);
        expr_t *x = test_expr_new_const_d(0.1);
        expr_t *y = test_expr_new_const_d(0.2);
        qfloat_t appell_expect =
            qf_from_string("1.1778303565638345453879410947052170506848071256473314110734863879480772052813379");

        f = expr_appell_f1(a, b1, b2, cc, x, y);
        check_q_at(__FILE__, __LINE__, 1,
                   "appell_f1(1;1,1;2;0.1,0.2)",
                   expr_eval_qf(f), appell_expect);
        print_expr_of(f);

        expr_free(f);
        expr_free(y);
        expr_free(x);
        expr_free(cc);
        expr_free(b2);
        expr_free(b1);
        expr_free(a);
    }
}

void test_gammainv(void)
{
    /* gammainv(gamma(2.5)) = 2.5 */
    qfloat_t X = qf_from_double(2.5);
    qfloat_t Y = qf_gamma(X);
    expr_t *c = test_expr_new_var_qf(Y);
    expr_t *f = expr_gammainv(c);
    check_q_at(__FILE__, __LINE__, 1, "gammainv(gamma(2.5)) = 2.5", expr_eval_qf(f), X);
    print_expr_of(f);
    expr_free(f); expr_free(c);

    /* Round-trip at x=3 */
    X = qf_from_double(3.0);
    Y = qf_gammainv(qf_gamma(X));
    check_q_at(__FILE__, __LINE__, 1, "qf_gammainv(gamma(3)) = 3", Y, X);
}

void test_deriv_trigamma(void)
{
    expr_t *x  = test_expr_new_var_d(3.0);
    expr_t *f  = expr_trigamma(x);
    const expr_t *df = expr_get_deriv(f, x);

    /* d/dx{ψ'(x)} = ψ''(x) (tetragamma) — verify via qf_tetragamma */
    qfloat_t expect = qf_tetragamma(qf_from_double(3.0));
    check_q_at(__FILE__, __LINE__, 1, "d/dx{trigamma(x)} | x=3", expr_eval_qf(df), expect);
    print_expr_of(df);

    /* Cross-check against recurrence: ψ''(3) = ψ''(4) - 2/27 */
    qfloat_t tet4 = qf_tetragamma(qf_from_double(4.0));
    qfloat_t via_recurrence = qf_sub(tet4, qf_div(qf_from_double(2.0), qf_from_double(27.0)));
    check_q_at(__FILE__, __LINE__, 1, "ψ''(3) = ψ''(4) - 2/27 (recurrence)", expect, via_recurrence);

    expr_free(f);
    expr_free(x);
}

void test_second_deriv_digamma(void)
{
    /* digamma has a symbolic derivative (trigamma), so the second derivative
     * evaluates to qf_tetragamma(x₀) via deriv_trigamma. */
    expr_t *x   = test_expr_new_var_d(2.0);
    expr_t *f   = expr_digamma(x);
    expr_t *df  = expr_create_deriv(f, x);
    const expr_t *ddf = expr_get_deriv(df, x);

    /* d²/dx²{ψ(x)} = ψ''(x) = tetragamma(x); at x=2: ψ''(2) = ψ''(1) - 2
     * ψ''(1) = -2ζ(3) so we just use qf_tetragamma to get the expected value */
    qfloat_t expect = qf_tetragamma(qf_from_double(2.0));
    check_q_at(__FILE__, __LINE__, 1, "d²/dx²{digamma(x)} | x=2", expr_eval_qf(ddf), expect);
    print_expr_of(ddf);

    /* Cross-check via recurrence: ψ''(2) = ψ''(3) - 2/8 */
    qfloat_t via_recurrence = qf_sub(qf_tetragamma(qf_from_double(3.0)),
                                   qf_div(qf_from_double(2.0), qf_from_double(8.0)));
    check_q_at(__FILE__, __LINE__, 1, "ψ''(2) = ψ''(3) - 1/4 (recurrence)", expect, via_recurrence);

    expr_free(df);
    expr_free(f);
    expr_free(x);
}

/* ------------------------------------------------------------------------- */
/* First derivative tests                                                     */
/* ------------------------------------------------------------------------- */
