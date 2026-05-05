#include "test_qfloat.h"

static int qf_close_value(qfloat_t got, qfloat_t expected, double tol)
{
    if (qf_close(got, expected, tol))
        return 1;
    if (qf_eq(expected, qf_from_double(0.0)))
        return 0;
    return qf_close_rel(got, expected, tol);
}

static void test_qf_productlog_definition(void)
{
    printf(C_CYAN "TEST: qf_productlog definition  W(x)*exp(W(x)) = x\n" C_RESET);

    double xs[] = { -0.3, -0.1, 0.0, 0.1, 1.0, 2.0, 5.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);
        qfloat_t w = qf_productlog(x);

        qfloat_t lhs = qf_mul(w, qf_exp(w));

        int ok = qf_close(lhs, x, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: W(%g)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: W(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_q("W(x)", w);
            print_q("W*exp(W)", lhs);
            print_q("x", x);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Consistency with qf_lambertw
   ------------------------------------------------------------------------- */
static void test_qf_productlog_consistency(void)
{
    printf(C_CYAN "TEST: qf_productlog consistency with qf_lambert_w0\n" C_RESET);

    double xs[] = { -0.2, -0.05, 0.0, 0.2, 1.0, 3.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);

        qfloat_t p = qf_productlog(x);
        qfloat_t w = qf_lambert_w0(x);

        int ok = qf_close(p, w, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: ProductLog(%g) = W(%g)\n" C_RESET, xs[i], xs[i]);
        } else {
            printf(C_RED "  FAIL: ProductLog(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_q("ProductLog", p);
            print_q("LambertW", w);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Special values
   ------------------------------------------------------------------------- */
static void test_qf_productlog_special(void)
{
    printf(C_CYAN "TEST: qf_productlog special values\n" C_RESET);

    qfloat_t zero = qf_from_double(0.0);
    qfloat_t one  = qf_from_double(1.0);

    /* W(0) = 0 */
    qfloat_t w0 = qf_productlog(zero);
    int ok1 = qf_close(w0, zero, 1e-30);

    /* W(-1/e) = -1 */
    qfloat_t x = qf_neg(qf_exp(qf_neg(one))); /* -1/e */
    qfloat_t w = qf_productlog(x);
    int ok2 = qf_close_rel(w, qf_neg(one), 1e-30);

    /* W(e) = 1 */
    qfloat_t xe = qf_exp(one);
    qfloat_t we = qf_productlog(xe);
    int ok3 = qf_close(we, one, 1e-30);

    if (ok1 && ok2 && ok3) {
        printf(C_GREEN "  OK: special values\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: special values  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        print_q("W(0)", w0);
        print_q("W(-1/e)", w);
        print_q("W(e)", we);
        TEST_FAIL();
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 4: Monotonicity  (principal branch is strictly increasing)
   ------------------------------------------------------------------------- */
static void test_qf_productlog_monotonicity(void)
{
    printf(C_CYAN "TEST: qf_productlog monotonicity\n" C_RESET);

    double xs[] = { -0.3, -0.1, 0.0, 0.2, 1.0, 3.0 };

    qfloat_t prev = qf_productlog(qf_from_double(xs[0]));

    for (int i = 1; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);
        qfloat_t w = qf_productlog(x);

        if (qf_lt(w, prev)) {
            printf(C_RED "  FAIL: W(%g) < W(%g)  [%s:%d]\n" C_RESET, xs[i], xs[i-1], __FILE__, __LINE__);
            print_q("W(prev)", prev);
            print_q("W(curr)", w);
            TEST_FAIL();
            return;
        }

        prev = w;
    }

    printf(C_GREEN "  OK: monotonicity\n" C_RESET);
    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
void test_qf_productlog_all(void)
{
    RUN_SUBTEST(test_qf_productlog_definition);
    RUN_SUBTEST(test_qf_productlog_consistency);
    RUN_SUBTEST(test_qf_productlog_special);
    RUN_SUBTEST(test_qf_productlog_monotonicity);
}

/* -------------------------------------------------------------------------
   TEST 1: Regularized identity  P(s,x) + Q(s,x) = 1
   ------------------------------------------------------------------------- */
static void test_qf_gammainc_PQ_identity(void)
{
    printf(C_CYAN "TEST: qf_gammainc_P / qf_gammainc_Q  (P + Q = 1)\n" C_RESET);

    double ss[] = { 0.5, 1.0, 2.5, 5.0 };
    double xs[] = { 0.1, 1.0, 2.0, 5.0, 10.0 };

    for (int i = 0; i < (int)(sizeof(ss)/sizeof(ss[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(xs)/sizeof(xs[0])); ++j) {

            qfloat_t s = qf_from_double(ss[i]);
            qfloat_t x = qf_from_double(xs[j]);

            qfloat_t P = qf_gammainc_P(s, x);
            qfloat_t Q = qf_gammainc_Q(s, x);
            qfloat_t sum = qf_add(P, Q);

            int ok = qf_close_rel(sum, qf_from_double(1.0), 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: P(%g,%g) + Q(%g,%g)\n" C_RESET,
                       ss[i], xs[j], ss[i], xs[j]);
            } else {
                printf(C_RED "  FAIL: P(%g,%g) + Q(%g,%g)  [%s:%d]\n" C_RESET, ss[i], xs[j], ss[i], xs[j], __FILE__, __LINE__);
                print_q("P", P);
                print_q("Q", Q);
                print_q("sum", sum);
                print_q("expected", qf_from_double(1.0));
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Unregularized identity  γ(s,x) + Γ(s,x) = Γ(s)
   ------------------------------------------------------------------------- */
static void test_qf_gammainc_lower_upper_identity(void)
{
    printf(C_CYAN "TEST: qf_gammainc_lower / qf_gammainc_upper  (γ + Γ = Γ(s))\n" C_RESET);

    double ss[] = { 0.5, 1.0, 2.5, 5.0 };
    double xs[] = { 0.1, 1.0, 2.0, 5.0, 10.0 };

    for (int i = 0; i < (int)(sizeof(ss)/sizeof(ss[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(xs)/sizeof(xs[0])); ++j) {

            qfloat_t s = qf_from_double(ss[i]);
            qfloat_t x = qf_from_double(xs[j]);

            qfloat_t gl = qf_gammainc_lower(s, x);
            qfloat_t gu = qf_gammainc_upper(s, x);
            qfloat_t sum = qf_add(gl, gu);

            qfloat_t gs = qf_gamma(s);

            int ok = qf_close_rel(sum, gs, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: γ(%g,%g) + Γ(%g,%g)\n" C_RESET,
                       ss[i], xs[j], ss[i], xs[j]);
            } else {
                printf(C_RED "  FAIL: γ(%g,%g) + Γ(%g,%g)  [%s:%d]\n" C_RESET, ss[i], xs[j], ss[i], xs[j], __FILE__, __LINE__);
                print_q("lower γ", gl);
                print_q("upper Γ", gu);
                print_q("sum", sum);
                print_q("expected Γ(s)", gs);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Special case s = 1
      γ(1,x) = 1 - e^{-x}
      Γ(1,x) = e^{-x}
      P(1,x) = 1 - e^{-x}
      Q(1,x) = e^{-x}
   ------------------------------------------------------------------------- */
static void test_qf_gammainc_special_s1(void)
{
    printf(C_CYAN "TEST: incomplete gamma special case s = 1\n" C_RESET);

    double xs[] = { 0.1, 0.5, 1.0, 2.0, 5.0 };

    for (int j = 0; j < (int)(sizeof(xs)/sizeof(xs[0])); ++j) {

        qfloat_t x = qf_from_double(xs[j]);
        qfloat_t s = qf_from_double(1.0);

        qfloat_t e_minus_x = qf_exp(qf_neg(x));
        qfloat_t expected_lower = qf_sub(qf_from_double(1.0), e_minus_x);
        qfloat_t expected_upper = e_minus_x;

        qfloat_t gl = qf_gammainc_lower(s, x);
        qfloat_t gu = qf_gammainc_upper(s, x);

        int ok1 = qf_close_rel(gl, expected_lower, 1e-30);
        int ok2 = qf_close_rel(gu, expected_upper, 1e-30);

        if (ok1 && ok2) {
            printf(C_GREEN "  OK: γ(1,%g) and Γ(1,%g)\n" C_RESET, xs[j], xs[j]);
        } else {
            printf(C_RED "  FAIL: γ(1,%g) or Γ(1,%g)  [%s:%d]\n" C_RESET, xs[j], xs[j], __FILE__, __LINE__);
            print_q("γ(1,x)", gl);
            print_q("expected", expected_lower);
            print_q("Γ(1,x)", gu);
            print_q("expected", expected_upper);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_gammainc_all(void)
{
    RUN_SUBTEST(test_qf_gammainc_PQ_identity);
    RUN_SUBTEST(test_qf_gammainc_lower_upper_identity);
    RUN_SUBTEST(test_qf_gammainc_special_s1);
}

/* Approximate derivative using symmetric difference:
 *   f'(x) ≈ (f(x+h) - f(x-h)) / (2h)
 */
static qfloat_t qf_deriv_sym(qfloat_t (*f)(qfloat_t), qfloat_t x, double h_d)
{
    qfloat_t h = qf_from_double(h_d);

    /* x ± k h */
    qfloat_t x1p = qf_add(x, h);
    qfloat_t x1m = qf_sub(x, h);

    qfloat_t x2p = qf_add(x, qf_mul(qf_from_double(2.0), h));
    qfloat_t x2m = qf_sub(x, qf_mul(qf_from_double(2.0), h));

    qfloat_t x3p = qf_add(x, qf_mul(qf_from_double(3.0), h));
    qfloat_t x3m = qf_sub(x, qf_mul(qf_from_double(3.0), h));

    qfloat_t x4p = qf_add(x, qf_mul(qf_from_double(4.0), h));
    qfloat_t x4m = qf_sub(x, qf_mul(qf_from_double(4.0), h));

    /* function evaluations */
    qfloat_t f1p = f(x1p), f1m = f(x1m);
    qfloat_t f2p = f(x2p), f2m = f(x2m);
    qfloat_t f3p = f(x3p), f3m = f(x3m);
    qfloat_t f4p = f(x4p), f4m = f(x4m);

    /* coefficients */
    qfloat_t c1 = qf_from_double(672.0);
    qfloat_t c2 = qf_from_double(-168.0);
    qfloat_t c3 = qf_from_double(32.0);
    qfloat_t c4 = qf_from_double(-3.0);
    qfloat_t cden = qf_from_double(840.0);

    /* numerator */
    qfloat_t num = {0.0, 0.0};

    num = qf_add(num, qf_mul(c1, qf_sub(f1p, f1m)));
    num = qf_add(num, qf_mul(c2, qf_sub(f2p, f2m)));
    num = qf_add(num, qf_mul(c3, qf_sub(f3p, f3m)));
    num = qf_add(num, qf_mul(c4, qf_sub(f4p, f4m)));

    /* denominator = 840 h */
    qfloat_t den = qf_mul(cden, h);

    return qf_div(num, den);
}

/* Test Ei(x) via derivative: Ei'(x) = exp(x)/x */
static void test_qf_ei_deriv(void)
{
    printf(C_CYAN "TEST: qf_ei (derivative residual)\n" C_RESET);

    double xs[] = { 0.1, 0.5, 1.0, 2.0, 5.0 };
    int n = (int)(sizeof(xs) / sizeof(xs[0]));

    for (int i = 0; i < n; ++i) {
        qfloat_t x   = qf_from_double(xs[i]);
        qfloat_t dEi = qf_deriv_sym(qf_ei, x, 1e-8);

        qfloat_t ex  = qf_exp(x);
        qfloat_t rhs = qf_div(ex, x); /* e^x / x */

        qfloat_t res = qf_sub(dEi, rhs);

        int ok = qf_close_rel(dEi, rhs, 1.02e-22);

        if (ok) {
            printf(C_GREEN "  OK: Ei'(%g)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: Ei'(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_q("residual", res);
            print_q("dEi",      dEi);
            print_q("rhs",      rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* Test E1(x) via derivative: E1'(x) = -exp(-x)/x */
static void test_qf_e1_deriv(void)
{
    printf(C_CYAN "TEST: qf_e1 (derivative residual)\n" C_RESET);

    double xs[] = { 0.1, 0.5, 1.0, 2.0, 5.0 };
    int n = (int)(sizeof(xs) / sizeof(xs[0]));

    for (int i = 0; i < n; ++i) {
        qfloat_t x    = qf_from_double(xs[i]);
        qfloat_t dE1  = qf_deriv_sym(qf_e1, x, 1e-8);

        qfloat_t nx   = qf_neg(x);
        qfloat_t emx  = qf_exp(nx);          /* e^{-x} */
        qfloat_t rhs  = qf_div(emx, x);      /* e^{-x} / x */
        rhs         = qf_neg(rhs);         /* -e^{-x} / x */

        qfloat_t res  = qf_sub(dE1, rhs);

        int ok = qf_close_rel(dE1, rhs, 1e-20);

        if (ok) {
            printf(C_GREEN "  OK: E1'(%g)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: E1'(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_q("residual", res);
            print_q("dE1",      dE1);
            print_q("rhs",      rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* Test identity: E1(x) + Ei(-x) = 0 for x > 0 */
static void test_qf_ei_e1_identity(void)
{
    printf(C_CYAN "TEST: qf_ei / qf_e1 identity  E1(x) = -Ei(-x)\n" C_RESET);

    double xs[] = { 0.1, 0.5, 1.0, 2.0, 5.0, 10.0 };
    int n = (int)(sizeof(xs) / sizeof(xs[0]));

    for (int i = 0; i < n; ++i) {
        qfloat_t x   = qf_from_double(xs[i]);
        qfloat_t nx  = qf_neg(x);

        qfloat_t e1  = qf_e1(x);
        qfloat_t ei  = qf_ei(nx);

        qfloat_t sum = qf_add(e1, ei); /* should be ~0 */

        int ok = qf_close(sum, qf_from_double(0.0), 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: E1(%g) + Ei(%g)\n" C_RESET, xs[i], -xs[i]);
        } else {
            printf(C_RED "  FAIL: E1(%g) + Ei(%g)  [%s:%d]\n" C_RESET, xs[i], -xs[i], __FILE__, __LINE__);
            print_q("E1(x)", e1);
            print_q("Ei(-x)", ei);
            print_q("sum",   sum);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_ei_values(void)
{
    printf("TEST: qf_ei (full‑precision value tests)\n");

    struct {
        const char *x_str;     /* x as full‑precision string */
        const char *ref_str;   /* Ei(x) reference value */
        double tolerance;      /* relative tolerance */
    } cases[] = {

        /* small x */
        { "0.1",  "-1.6228128139692766749656829992274752542", 1e-30 },
        { "0.5",  "0.4542199048631735799205238126628023652814", 1e-30 },
        { "1.0",  "1.8951178163559367554665209343316342694e+0", 1e-30 },

        /* moderate x */
        { "2.0",  "4.95423435600189016337950513022703527552", 1e-30 },
        { "5.0",  "40.18527535580317745509142179379586709542", 1e-30 },

        /* large x */
        { "10.0", "2492.22897624187775913844014399852484899", 1e-30 },

        /* negative x */
        { "-0.1", "-1.822923958419390666080913658291830939119", 1e-30 },
        { "-0.5", "-0.5597735947761608117467959393150852352268", 1e-30 },
        { "-1.0", "-0.2193839343955202736771637754601216490310", 1e-30 },
        { "-2.0", "-4.8900510708061119567239835228449969723e-2", 1e-30 },
        { "-5.0", "-1.1482955912753257973305619701930277775e-3", 1e-30 },
        { "-10.0","-4.156968929685324277402859810278180384346e-6", 1e-30 }
    };

    int n = sizeof(cases)/sizeof(cases[0]);

    char buf_x[128];
    char buf_ref[128];
    char buf_got[128];
    char buf_err[128];

    for (int i = 0; i < n; ++i) {

        qfloat_t x   = qf_from_string(cases[i].x_str);
        qfloat_t ref = qf_from_string(cases[i].ref_str);
        qfloat_t got = qf_ei(x);

        qfloat_t diff = qf_sub(got, ref);
        qfloat_t ad   = qf_abs(diff);

        /* qfloat_t-level tolerance */
        int pass = qf_close_value(got, ref, cases[i].tolerance);

        if (pass) {
            printf("  \x1b[32mOK\x1b[0m: Ei(%s)\n", cases[i].x_str);
        } else {
            printf("  \x1b[31mFAIL\x1b[0m: Ei(%s)  [%s:%d]\n", cases[i].x_str, __FILE__, __LINE__);
            TEST_FAIL();
        }

        /* full-precision printing using qf_sprintf */
        qf_sprintf(buf_x,   sizeof(buf_x),   "%.40Q", x);
        qf_sprintf(buf_ref, sizeof(buf_ref), "%.40Q", ref);
        qf_sprintf(buf_got, sizeof(buf_got), "%.40Q", got);
        qf_sprintf(buf_err, sizeof(buf_err), "%.40Q", ad);

        printf("    x        = %s\n", buf_x);
        printf("    expected = %s\n", buf_ref);
        printf("    got      = %s\n", buf_got);
        printf("    error    = %s\n", buf_err);
    }

    printf("\n");
}

static void test_qf_ei_e1_all(void)
{
    RUN_SUBTEST(test_qf_ei_deriv);
    RUN_SUBTEST(test_qf_e1_deriv);
    RUN_SUBTEST(test_qf_ei_e1_identity);
    RUN_SUBTEST(test_ei_values);
}

static void test_qf_add_double(void) {
    printf(C_CYAN "TEST: qf_add_double\n" C_RESET);
    char buf[256], buf_exp[256];

    struct { const char *xs; double y; const char *expected; } tests[] = {
        { "1",                   0.5,   "1.5"                              },
        { "0",                   1.0,   "1"                                },
        { "-3",                  3.0,   "0"                                },
        { "1.23456789012345678", 1e-15, "1.23456789012345778"              },
        { "0",                   0.0,   "0"                                },
        { NULL, 0.0, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_add_double(x, tests[i].y);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close_value(got, exp, 1e-30)) {
            printf("%s  OK: add_double(%s, %.17g)%s\n", C_GREEN, tests[i].xs, tests[i].y, C_RESET);
        } else {
            printf("%s  FAIL: add_double(%s, %.17g)%s  [%s:%d]\n", C_RED, tests[i].xs, tests[i].y, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_mul_double(void) {
    printf(C_CYAN "TEST: qf_mul_double\n" C_RESET);
    char buf[256], buf_exp[256];

    struct { const char *xs; double y; const char *expected; } tests[] = {
        { "2",   3.0,   "6"   },
        { "0",   5.0,   "0"   },
        { "-1",  4.0,  "-4"   },
        { "1",   0.5,   "0.5" },
        { "3",  -2.0,  "-6"   },
        { NULL, 0.0, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_mul_double(x, tests[i].y);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close_value(got, exp, 1e-30)) {
            printf("%s  OK: mul_double(%s, %.17g)%s\n", C_GREEN, tests[i].xs, tests[i].y, C_RESET);
        } else {
            printf("%s  FAIL: mul_double(%s, %.17g)%s  [%s:%d]\n", C_RED, tests[i].xs, tests[i].y, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_cmp(void) {
    printf(C_CYAN "TEST: qf_cmp / qf_lt / qf_le / qf_gt / qf_ge\n" C_RESET);

    struct { const char *as; const char *bs; int want_cmp; } tests[] = {
        { "1",  "2",  -1 },
        { "2",  "1",   1 },
        { "3",  "3",   0 },
        { "-1", "0",  -1 },
        { "0",  "-1",  1 },
        { NULL, NULL,  0 }
    };

    for (int i = 0; tests[i].as != NULL; i++) {
        qfloat_t a = qf_from_string(tests[i].as);
        qfloat_t b = qf_from_string(tests[i].bs);
        int c = qf_cmp(a, b);

        int ok = (c == tests[i].want_cmp)
              && (qf_lt(a, b) == (tests[i].want_cmp < 0))
              && (qf_le(a, b) == (tests[i].want_cmp <= 0))
              && (qf_gt(a, b) == (tests[i].want_cmp > 0))
              && (qf_ge(a, b) == (tests[i].want_cmp >= 0));

        if (ok) {
            printf("%s  OK: cmp(%s, %s) = %d%s\n", C_GREEN, tests[i].as, tests[i].bs, c, C_RESET);
        } else {
            printf("%s  FAIL: cmp(%s, %s): got %d want %d%s  [%s:%d]\n",
                   C_RED, tests[i].as, tests[i].bs, c, tests[i].want_cmp, C_RESET, __FILE__, __LINE__);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_floor(void) {
    printf(C_CYAN "TEST: qf_floor\n" C_RESET);
    char buf[256], buf_exp[256];

    struct { const char *xs; const char *expected; } tests[] = {
        { "2.9",  "2"  },
        { "2.0",  "2"  },
        { "-2.1", "-3" },
        { "-2.0", "-2" },
        { "0.5",  "0"  },
        { "-0.5", "-1" },
        { NULL, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_floor(x);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close(got, exp, 1e-30)) {
            printf("%s  OK: floor(%s) = %s%s\n", C_GREEN, tests[i].xs, buf, C_RESET);
        } else {
            printf("%s  FAIL: floor(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_ldexp(void) {
    printf(C_CYAN "TEST: qf_ldexp\n" C_RESET);
    char buf[256], buf_exp[256];

    struct { const char *xs; int k; const char *expected; } tests[] = {
        { "1",   3,   "8"    },
        { "1",  -1,   "0.5"  },
        { "3",   2,   "12"   },
        { "1",   0,   "1"    },
        { "-5",  1,  "-10"   },
        { NULL,  0,   NULL   }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_ldexp(x, tests[i].k);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close(got, exp, 1e-30)) {
            printf("%s  OK: ldexp(%s, %d) = %s%s\n", C_GREEN, tests[i].xs, tests[i].k, buf, C_RESET);
        } else {
            printf("%s  FAIL: ldexp(%s, %d)%s  [%s:%d]\n", C_RED, tests[i].xs, tests[i].k, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_sqr(void) {
    printf(C_CYAN "TEST: qf_sqr\n" C_RESET);
    char buf[256], buf_exp[256];

    struct { const char *xs; const char *expected; } tests[] = {
        { "3",    "9"   },
        { "-4",   "16"  },
        { "0.5",  "0.25" },
        { "0",    "0"   },
        { "1",    "1"   },
        { NULL, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_sqr(x);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qfloat_t ref = qf_mul(x, x);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close(got, exp, 1e-30) && qf_close(got, ref, 1e-30)) {
            printf("%s  OK: sqr(%s) = %s%s\n", C_GREEN, tests[i].xs, buf, C_RESET);
        } else {
            printf("%s  FAIL: sqr(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_mul_pow10(void) {
    printf(C_CYAN "TEST: qf_mul_pow10\n" C_RESET);
    char buf[256], buf_exp[256];

    struct { const char *xs; int k; const char *expected; double tol; } tests[] = {
        { "1",    3,  "1000",  1e-30 },
        { "1",   -3,  "0.001", 1e-15 },  /* non-exact binary fraction */
        { "2.5",  2,  "250",   1e-30 },
        { "3",    0,  "3",     1e-30 },
        { "-7",   1,  "-70",   1e-30 },
        { NULL,   0,  NULL,    0.0   }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_mul_pow10(x, tests[i].k);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close_value(got, exp, tests[i].tol)) {
            printf("%s  OK: mul_pow10(%s, %d) = %s%s\n", C_GREEN, tests[i].xs, tests[i].k, buf, C_RESET);
        } else {
            printf("%s  FAIL: mul_pow10(%s, %d)%s  [%s:%d]\n", C_RED, tests[i].xs, tests[i].k, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_signbit(void) {
    printf(C_CYAN "TEST: qf_signbit\n" C_RESET);

    struct { const char *xs; int expected; } tests[] = {
        { "1",    0 },
        { "-1",   1 },
        { "0",    0 },
        { "1e30", 0 },
        { "-1e30",1 },
        { NULL,   0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x = qf_from_string(tests[i].xs);
        int got    = qf_signbit(x);
        if (got == tests[i].expected) {
            printf("%s  OK: signbit(%s) = %d%s\n", C_GREEN, tests[i].xs, got, C_RESET);
        } else {
            printf("%s  FAIL: signbit(%s): got %d want %d%s  [%s:%d]\n",
                   C_RED, tests[i].xs, got, tests[i].expected, C_RESET, __FILE__, __LINE__);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_isinf(void) {
    printf(C_CYAN "TEST: qf_isinf\n" C_RESET);

    qfloat_t pos_inf = qf_from_double(1.0 / 0.0);
    qfloat_t neg_inf = qf_from_double(-1.0 / 0.0);
    qfloat_t normal  = qf_from_string("1");
    qfloat_t nan_val = qf_from_double(0.0 / 0.0);

    struct { const char *label; qfloat_t x; bool expected; } tests[] = {
        { "+INF",   pos_inf, true  },
        { "-INF",   neg_inf, true  },
        { "1",      normal,  false },
        { "NaN",    nan_val, false },
    };

    int n = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < n; i++) {
        bool got = qf_isinf(tests[i].x);
        if (got == tests[i].expected) {
            printf("%s  OK: isinf(%s) = %s%s\n", C_GREEN, tests[i].label, got ? "true" : "false", C_RESET);
        } else {
            printf("%s  FAIL: isinf(%s): got %s want %s%s  [%s:%d]\n",
                   C_RED, tests[i].label, got ? "true" : "false",
                   tests[i].expected ? "true" : "false", C_RESET, __FILE__, __LINE__);
            TEST_FAIL();
        }
    }
    printf("\n");
}

static void test_qf_vsprintf(void) {
    printf(C_CYAN "TEST: qf_vsprintf\n" C_RESET);

    /* qf_sprintf is the public wrapper around qf_vsprintf; test via it */
    char buf[256];
    qfloat_t x = qf_from_string("3.14159265358979323846264338327950288");
    qfloat_t y = qf_from_string("-2.71828182845904523536028747135266249");

    /* %q should round-trip: qf_from_string(qf_sprintf(...)) == x */
    qf_sprintf(buf, sizeof(buf), "%q", x);
    qfloat_t rt = qf_from_string(buf);
    if (qf_close_value(rt, x, 1e-30)) {
        printf("%s  OK: vsprintf %%q round-trips correctly%s\n", C_GREEN, C_RESET);
    } else {
        printf("%s  FAIL: vsprintf %%q round-trip%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
        printf("    formatted = %s\n", buf);
        TEST_FAIL();
    }

    /* negative value round-trip */
    qf_sprintf(buf, sizeof(buf), "%q", y);
    rt = qf_from_string(buf);
    if (qf_close_value(rt, y, 1e-30)) {
        printf("%s  OK: vsprintf %%q negative round-trips%s\n", C_GREEN, C_RESET);
    } else {
        printf("%s  FAIL: vsprintf %%q negative round-trip%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
        printf("    formatted = %s\n", buf);
        TEST_FAIL();
    }

    /* mixing %q with surrounding text: prefix and suffix must appear */
    qf_sprintf(buf, sizeof(buf), "val=%q!", x);
    char inner[256];
    qf_sprintf(inner, sizeof(inner), "%q", x);
    char expected[512];
    snprintf(expected, sizeof(expected), "val=%s!", inner);
    if (strcmp(buf, expected) == 0) {
        printf("%s  OK: vsprintf surrounding text%s\n", C_GREEN, C_RESET);
    } else {
        printf("%s  FAIL: vsprintf surrounding text%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", expected);
        TEST_FAIL();
    }
    printf("\n");
}

void test_qf_trigamma(void) {
    printf(C_CYAN "TEST: qf_trigamma\n" C_RESET);
    char buf[256], buf_exp[256];

    /* ψ₁(n) = π²/6 - sum_{k=1}^{n-1} 1/k²; ψ₁(1/2) = π²/2 */
    struct { const char *xs; const char *expected; double tol; } tests[] = {
        /* ψ₁(1) = π²/6 */
        { "1",   "1.6449340668482264364724151666460251892189499012068", 1e-30 },
        /* ψ₁(2) = ψ₁(1) - 1 */
        { "2",   "0.6449340668482264364724151666460251892189499012068", 1e-30 },
        /* ψ₁(1/2) = π²/2 = 4.9348022005446793094172454999380755676568... */
        { "0.5", "4.9348022005446793094172454999380755676568497036204", 1e-30 },
        /* ψ₁(3) = ψ₁(1) - 1 - 1/4 */
        { "3",   "0.3949340668482264364724151666460251892189499012068", 1e-30 },
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_trigamma(x);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close_value(got, exp, tests[i].tol)) {
            printf("%s  OK: trigamma(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: trigamma(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

void test_qf_tetragamma(void) {
    printf(C_CYAN "TEST: qf_tetragamma\n" C_RESET);
    char buf[256], buf_exp[256];

    /* ψ''(x); recurrence ψ''(x+1) = ψ''(x) + 2/x³
       ψ''(1) = -2ζ(3) = -2 * 1.2020569031595942853997... = -2.4041138063191885707994... */
    struct { const char *xs; const char *expected; double tol; } tests[] = {
        /* ψ''(1) = -2ζ(3) */
        { "1",  "-2.4041138063191885707994763230228999815299725846810", 1e-30 },
        /* ψ''(2) = ψ''(1) + 2 */
        { "2",  "-0.4041138063191885707994763230228999815299725846810", 1e-30 },
        /* ψ''(3) = ψ''(2) + 2/8 */
        { "3",  "-0.1541138063191885707994763230228999815299725846810", 1e-30 },
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {
        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_tetragamma(x);
        qfloat_t exp = qf_from_string(tests[i].expected);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        if (qf_close_value(got, exp, tests[i].tol)) {
            printf("%s  OK: tetragamma(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: tetragamma(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
    printf("\n");
}

void test_arithmetic_extensions(void) {
    RUN_SUBTEST(test_qf_add_double);
    RUN_SUBTEST(test_qf_mul_double);
    RUN_SUBTEST(test_qf_sqr);
    RUN_SUBTEST(test_qf_ldexp);
    RUN_SUBTEST(test_qf_mul_pow10);
    RUN_SUBTEST(test_qf_floor);
    RUN_SUBTEST(test_qf_cmp);
    RUN_SUBTEST(test_qf_signbit);
    RUN_SUBTEST(test_qf_isinf);
}

void test_vsprintf(void) {
    RUN_SUBTEST(test_qf_vsprintf);
}

void test_gammainc_ei_e1(void) {
    RUN_SUBTEST(test_qf_gammainc_all);
    RUN_SUBTEST(test_qf_ei_e1_all);
}
