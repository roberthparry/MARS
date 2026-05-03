#include "test_qfloat.h"

static int qf_close_value(qfloat_t got, qfloat_t expected, double tol)
{
    if (qf_close(got, expected, tol))
        return 1;
    if (qf_eq(expected, qf_from_double(0.0)))
        return 0;
    return qf_close_rel(got, expected, tol);
}

static void test_qf_lambert_w0(void) {
    printf(C_CYAN "TEST: qf_lambert_w0\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected W0(x) as qfloat_t string */
        double acceptable_error;
    } tests[] = {

        /* Basic values */
        { "0",     "0", 1e-29 },
        { "1e-6",  "9.999990000014999973333385416558667e-7", 1e-29 },
                    
        { "1e-3",  "0.0009990014973385308899578278741077856", 1e-29 },
        { "0.1",   "0.09127652716086226429989572142317956",   1e-29 },
        { "1",     "0.56714329040978387299996866221035555",   1e-29 },
        { "2",     "0.8526055020137254913464724146953175",    1e-29 },
        { "5",     "1.3267246652422002236350992977580797",    1e-29 },
        { "10",    "1.7455280027406993830743012648753899",    1e-29 },
        { "20",    "2.2050032780240599704930659773870498",    1e-29 },

        /* Endpoint */
        { "-0.3678794411714423215955237701614609", "-1", 1e-29 }, /* -1/e */

        /* End marker */
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_lambert_w0(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, exp, 1e-30)) {
            printf("%s  OK: W0(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf(C_RED "  FAIL: W0(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_lambert_wm1(void) {
    printf(C_CYAN "TEST: qf_lambert_wm1\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected Wm1(x) as qfloat_t string */
        double acceptable_error;
    } tests[] = {

        /* Endpoint */
        { "-0.3678794411714423215955237701614609", "-1", 1e-29 }, /* -1/e */

        /* Values near 0− */
        { "-0.1",   "-3.577152063957297218409391963511995", 1e-29 },
        { "-0.01",  "-6.472775124394004694741057892724488", 1e-29 },
        { "-0.001", "-9.118006470402740121258337182046814", 1e-29 },
        { "-1e-6",  "-16.62650890137247338770643216398468", 1e-29 },

        /* End marker */
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_lambert_wm1(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, exp, 1e-30)) {
            printf("%s  OK: Wm1(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf(C_RED "  FAIL: Wm1(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Colourised tests for qf_beta
   ------------------------------------------------------------------------- */

static void print_qf(const char *label, qfloat_t x)
{
    char buf[128];
    qf_sprintf(buf, sizeof(buf), "%Q", x);
    printf("    %s = %s\n", label, buf);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check  B(a,b) = Γ(a)Γ(b) / Γ(a+b)
   ------------------------------------------------------------------------- */
static void test_qf_beta_definition(void)
{
    printf(C_CYAN "TEST: qf_beta definition  B(a,b) = Γ(a)Γ(b)/Γ(a+b)\n" C_RESET);

    double as[] = { 0.5, 1.0, 2.0, 5.0 };
    double bs[] = { 0.5, 1.0, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(as)/sizeof(as[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(bs)/sizeof(bs[0])); ++j) {

            qfloat_t a = qf_from_double(as[i]);
            qfloat_t b = qf_from_double(bs[j]);

            qfloat_t B  = qf_beta(a, b);
            qfloat_t ga = qf_gamma(a);
            qfloat_t gb = qf_gamma(b);
            qfloat_t gab = qf_gamma(qf_add(a, b));

            qfloat_t rhs = qf_div(qf_mul(ga, gb), gab);

            int ok = qf_close_rel(B, rhs, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: B(%g,%g)\n" C_RESET, as[i], bs[j]);
            } else {
                printf(C_RED "  FAIL: B(%g,%g)\n" C_RESET, as[i], bs[j]);
                print_qf("B(a,b)", B);
                print_qf("Γ(a)Γ(b)/Γ(a+b)", rhs);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Symmetry  B(a,b) = B(b,a)
   ------------------------------------------------------------------------- */
static void test_qf_beta_symmetry(void)
{
    printf(C_CYAN "TEST: qf_beta symmetry  B(a,b) = B(b,a)\n" C_RESET);

    double as[] = { 0.5, 1.0, 2.0, 5.0 };
    double bs[] = { 0.5, 1.0, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(as)/sizeof(as[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(bs)/sizeof(bs[0])); ++j) {

            qfloat_t a = qf_from_double(as[i]);
            qfloat_t b = qf_from_double(bs[j]);

            qfloat_t Bab = qf_beta(a, b);
            qfloat_t Bba = qf_beta(b, a);

            int ok = qf_close_rel(Bab, Bba, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: B(%g,%g) = B(%g,%g)\n" C_RESET,
                       as[i], bs[j], bs[j], as[i]);
            } else {
                printf(C_RED "  FAIL: B(%g,%g) = B(%g,%g) [%s:%d]\n" C_RESET, as[i], bs[j], bs[j], as[i], __FILE__, __LINE__);
                print_qf("B(a,b)", Bab);
                print_qf("B(b,a)", Bba);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Special cases  B(1,b)=1/b,  B(a,1)=1/a
   ------------------------------------------------------------------------- */
static void test_qf_beta_special_cases(void)
{
    printf(C_CYAN "TEST: qf_beta special cases  B(1,b)=1/b, B(a,1)=1/a\n" C_RESET);

    double vals[] = { 0.5, 1.0, 2.0, 5.0 };

    for (int i = 0; i < (int)(sizeof(vals)/sizeof(vals[0])); ++i) {

        qfloat_t b = qf_from_double(vals[i]);
        qfloat_t a = qf_from_double(vals[i]);

        qfloat_t B1b = qf_beta(qf_from_double(1.0), b);
        qfloat_t B1b_expected = qf_div(qf_from_double(1.0), b);

        qfloat_t Ba1 = qf_beta(a, qf_from_double(1.0));
        qfloat_t Ba1_expected = qf_div(qf_from_double(1.0), a);

        int ok1 = qf_close_rel(B1b, B1b_expected, 1e-30);
        int ok2 = qf_close_rel(Ba1, Ba1_expected, 1e-30);

        if (ok1 && ok2) {
            printf(C_GREEN "  OK: B(1,%g) and B(%g,1)\n" C_RESET, vals[i], vals[i]);
        } else {
            printf(C_RED "  FAIL: B(1,%g) or B(%g,1)  [%s:%d]\n" C_RESET, vals[i], vals[i], __FILE__, __LINE__);
            print_qf("B(1,b)", B1b);
            print_qf("expected", B1b_expected);
            print_qf("B(a,1)", Ba1);
            print_qf("expected", Ba1_expected);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_beta_all(void)
{
    RUN_TEST(test_qf_beta_definition, __func__);
    RUN_TEST(test_qf_beta_symmetry, __func__);
    RUN_TEST(test_qf_beta_special_cases, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check
      log B(a,b) = lgamma(a) + lgamma(b) - lgamma(a+b)
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_definition(void)
{
    printf(C_CYAN "TEST: qf_logbeta definition  logB = lgamma(a)+lgamma(b)-lgamma(a+b)\n" C_RESET);

    double as[] = { 0.5, 1.0, 2.0, 5.0 };
    double bs[] = { 0.5, 1.0, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(as)/sizeof(as[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(bs)/sizeof(bs[0])); ++j) {

            qfloat_t a = qf_from_double(as[i]);
            qfloat_t b = qf_from_double(bs[j]);

            qfloat_t logB = qf_logbeta(a, b);

            qfloat_t lg_a  = qf_lgamma(a);
            qfloat_t lg_b  = qf_lgamma(b);
            qfloat_t lg_ab = qf_lgamma(qf_add(a, b));

            qfloat_t rhs = qf_add(lg_a, lg_b);
            rhs        = qf_sub(rhs, lg_ab);

            int ok = qf_close_rel(logB, rhs, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: logB(%g,%g)\n" C_RESET, as[i], bs[j]);
            } else {
                printf(C_RED "  FAIL: logB(%g,%g)  [%s:%d]\n" C_RESET, as[i], bs[j], __FILE__, __LINE__);
                print_qf("logB", logB);
                print_qf("rhs", rhs);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Consistency with Beta
      log B(a,b) = log(qf_beta(a,b))
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_consistency(void)
{
    printf(C_CYAN "TEST: qf_logbeta consistency with qf_beta\n" C_RESET);

    double as[] = { 0.5, 1.0, 2.0, 5.0 };
    double bs[] = { 0.5, 1.0, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(as)/sizeof(as[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(bs)/sizeof(bs[0])); ++j) {

            qfloat_t a = qf_from_double(as[i]);
            qfloat_t b = qf_from_double(bs[j]);

            qfloat_t logB = qf_logbeta(a, b);
            qfloat_t B    = qf_beta(a, b);
            qfloat_t logB_expected = qf_log(B);

            int ok = qf_close_value(logB, logB_expected, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: logB(%g,%g) matches log(beta)\n" C_RESET,
                       as[i], bs[j]);
            } else {
                printf(C_RED "  FAIL: logB(%g,%g) mismatch  [%s:%d]\n" C_RESET, as[i], bs[j], __FILE__, __LINE__);
                print_qf("logB", logB);
                print_qf("log(beta)", logB_expected);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Symmetry  logB(a,b) = logB(b,a)
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_symmetry(void)
{
    printf(C_CYAN "TEST: qf_logbeta symmetry  logB(a,b) = logB(b,a)\n" C_RESET);

    double as[] = { 0.5, 1.0, 2.0, 5.0 };
    double bs[] = { 0.5, 1.0, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(as)/sizeof(as[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(bs)/sizeof(bs[0])); ++j) {

            qfloat_t a = qf_from_double(as[i]);
            qfloat_t b = qf_from_double(bs[j]);

            qfloat_t lab = qf_logbeta(a, b);
            qfloat_t lba = qf_logbeta(b, a);

            int ok = qf_close_rel(lab, lba, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: logB(%g,%g) = logB(%g,%g)\n" C_RESET,
                       as[i], bs[j], bs[j], as[i]);
            } else {
                printf(C_RED "  FAIL: logB(%g,%g) = logB(%g,%g)  [%s:%d]\n" C_RESET, as[i], bs[j], bs[j], as[i], __FILE__, __LINE__);
                print_qf("logB(a,b)", lab);
                print_qf("logB(b,a)", lba);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 4: Special cases
      B(1,b) = 1/b  → logB(1,b) = -log(b)
      B(a,1) = 1/a  → logB(a,1) = -log(a)
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_special_cases(void)
{
    printf(C_CYAN "TEST: qf_logbeta special cases\n" C_RESET);

    double vals[] = { 0.5, 1.0, 2.0, 5.0 };

    for (int i = 0; i < (int)(sizeof(vals)/sizeof(vals[0])); ++i) {

        qfloat_t v = qf_from_double(vals[i]);

        qfloat_t logB1v = qf_logbeta(qf_from_double(1.0), v);
        qfloat_t logB1v_expected = qf_neg(qf_log(v));

        qfloat_t logBv1 = qf_logbeta(v, qf_from_double(1.0));
        qfloat_t logBv1_expected = qf_neg(qf_log(v));

        int ok1 = qf_close_value(logB1v, logB1v_expected, 1e-30);
        int ok2 = qf_close_value(logBv1, logBv1_expected, 1e-30);

        if (ok1 && ok2) {
            printf(C_GREEN "  OK: logB(1,%g) and logB(%g,1)\n" C_RESET,
                   vals[i], vals[i]);
        } else {
            printf(C_RED "  FAIL: logB(1,%g) or logB(%g,1)  [%s:%d]\n" C_RESET, vals[i], vals[i], __FILE__, __LINE__);
            print_qf("logB(1,v)", logB1v);
            print_qf("expected", logB1v_expected);
            print_qf("logB(v,1)", logBv1);
            print_qf("expected", logBv1_expected);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_all(void)
{
    RUN_TEST(test_qf_logbeta_definition, __func__);
    RUN_TEST(test_qf_logbeta_consistency, __func__);
    RUN_TEST(test_qf_logbeta_symmetry, __func__);
    RUN_TEST(test_qf_logbeta_special_cases, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check
      C(a,b) = Γ(a+1) / ( Γ(b+1) Γ(a-b+1) )
   ------------------------------------------------------------------------- */
static void test_qf_binomial_definition(void)
{
    printf(C_CYAN "TEST: qf_binomial definition  C(a,b)=Γ(a+1)/(Γ(b+1)Γ(a-b+1))\n" C_RESET);

    double as[] = { 2.0, 5.0, 10.0, 2.5 };
    double bs[] = { 0.0, 1.0, 2.0, 1.5 };

    for (int i = 0; i < (int)(sizeof(as)/sizeof(as[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(bs)/sizeof(bs[0])); ++j) {

            qfloat_t a = qf_from_double(as[i]);
            qfloat_t b = qf_from_double(bs[j]);

            qfloat_t C = qf_binomial(a, b);

            qfloat_t ga1   = qf_gamma(qf_add(a, qf_from_double(1.0)));
            qfloat_t gb1   = qf_gamma(qf_add(b, qf_from_double(1.0)));
            qfloat_t gamb1 = qf_gamma(qf_add(qf_sub(a, b), qf_from_double(1.0)));

            qfloat_t rhs = qf_div(ga1, qf_mul(gb1, gamb1));

            int ok = qf_close_rel(C, rhs, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: C(%g,%g)\n" C_RESET, as[i], bs[j]);
            } else {
                printf(C_RED "  FAIL: C(%g,%g)  [%s:%d]\n" C_RESET, as[i], bs[j], __FILE__, __LINE__);
                print_qf("C(a,b)", C);
                print_qf("Γ(a+1)/(Γ(b+1)Γ(a-b+1))", rhs);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Integer symmetry  C(n,k) = C(n,n-k)
   ------------------------------------------------------------------------- */
static void test_qf_binomial_symmetry(void)
{
    printf(C_CYAN "TEST: qf_binomial symmetry  C(n,k)=C(n,n-k)\n" C_RESET);

    int ns[] = { 2, 5, 10, 20 };

    for (int i = 0; i < (int)(sizeof(ns)/sizeof(ns[0])); ++i) {
        int n = ns[i];

        for (int k = 0; k <= n; ++k) {

            qfloat_t nq = qf_from_double((double)n);
            qfloat_t kq = qf_from_double((double)k);
            qfloat_t nkq = qf_from_double((double)(n-k));

            qfloat_t C1 = qf_binomial(nq, kq);
            qfloat_t C2 = qf_binomial(nq, nkq);

            int ok = qf_close_rel(C1, C2, 1e-30);

            if (ok) {
                printf(C_GREEN "  OK: C(%d,%d) = C(%d,%d)\n" C_RESET,
                       n, k, n, n-k);
            } else {
                printf(C_RED "  FAIL: C(%d,%d) = C(%d,%d)  [%s:%d]\n" C_RESET, n, k, n, n-k, __FILE__, __LINE__);
                print_qf("C(n,k)", C1);
                print_qf("C(n,n-k)", C2);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Special cases  C(n,0)=1, C(n,1)=n
   ------------------------------------------------------------------------- */
static void test_qf_binomial_special_cases(void)
{
    printf(C_CYAN "TEST: qf_binomial special cases\n" C_RESET);

    int ns[] = { 1, 2, 5, 10, 20 };

    for (int i = 0; i < (int)(sizeof(ns)/sizeof(ns[0])); ++i) {
        int n = ns[i];
        qfloat_t nq = qf_from_double((double)n);

        qfloat_t Cn0 = qf_binomial(nq, qf_from_double(0.0));
        qfloat_t Cn1 = qf_binomial(nq, qf_from_double(1.0));

        int ok1 = qf_close_rel(Cn0, qf_from_double(1.0), 1e-30);
        int ok2 = qf_close_rel(Cn1, nq, 1e-30);

        if (ok1 && ok2) {
            printf(C_GREEN "  OK: C(%d,0)=1 and C(%d,1)=%d\n" C_RESET, n, n, n);
        } else {
            printf(C_RED "  FAIL: C(%d,0) or C(%d,1)  [%s:%d]\n" C_RESET, n, n, __FILE__, __LINE__);
            print_qf("C(n,0)", Cn0);
            print_qf("C(n,1)", Cn1);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_binomial_all(void)
{
    RUN_TEST(test_qf_binomial_definition, __func__);
    RUN_TEST(test_qf_binomial_symmetry, __func__);
    RUN_TEST(test_qf_binomial_special_cases, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check
      f = x^(a-1) (1-x)^(b-1) / B(a,b)
   ------------------------------------------------------------------------- */
static void test_qf_beta_pdf_definition(void)
{
    printf(C_CYAN "TEST: qf_beta_pdf definition\n" C_RESET);

    double xs[] = { 0.1, 0.3, 0.5, 0.8 };
    double as[] = { 0.5, 1.0, 2.0, 5.0 };
    double bs[] = { 0.5, 1.0, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(as)/sizeof(as[0])); ++j) {
            for (int k = 0; k < (int)(sizeof(bs)/sizeof(bs[0])); ++k) {

                qfloat_t x = qf_from_double(xs[i]);
                qfloat_t a = qf_from_double(as[j]);
                qfloat_t b = qf_from_double(bs[k]);

                qfloat_t pdf = qf_beta_pdf(x, a, b);

                /* RHS = x^(a-1) (1-x)^(b-1) / B(a,b) */
                qfloat_t xpow   = qf_exp(qf_mul(qf_sub(a, qf_from_double(1.0)), qf_log(x)));
                qfloat_t omxpow = qf_exp(qf_mul(qf_sub(b, qf_from_double(1.0)),
                                              qf_log(qf_sub(qf_from_double(1.0), x))));
                qfloat_t B      = qf_beta(a, b);

                qfloat_t rhs = qf_div(qf_mul(xpow, omxpow), B);

                int ok = qf_close_rel(pdf, rhs, 1e-30);

                if (ok) {
                    printf(C_GREEN "  OK: f(%g; %g,%g)\n" C_RESET, xs[i], as[j], bs[k]);
                } else {
                    printf(C_RED "  FAIL: f(%g; %g,%g)  [%s:%d]\n" C_RESET, xs[i], as[j], bs[k], __FILE__, __LINE__);
                    print_qf("pdf", pdf);
                    print_qf("rhs", rhs);
                    TEST_FAIL();
                }
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Log-form consistency
      log f = (a-1)log x + (b-1)log(1-x) - logB
   ------------------------------------------------------------------------- */
static void test_qf_beta_pdf_logform(void)
{
    printf(C_CYAN "TEST: qf_beta_pdf log-form consistency\n" C_RESET);

    double xs[] = { 0.2, 0.4, 0.7 };
    double as[] = { 0.5, 2.0, 5.0 };
    double bs[] = { 0.5, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(as)/sizeof(as[0])); ++j) {
            for (int k = 0; k < (int)(sizeof(bs)/sizeof(bs[0])); ++k) {

                qfloat_t x = qf_from_double(xs[i]);
                qfloat_t a = qf_from_double(as[j]);
                qfloat_t b = qf_from_double(bs[k]);

                qfloat_t pdf = qf_beta_pdf(x, a, b);
                qfloat_t logpdf = qf_log(pdf);

                qfloat_t log_x   = qf_log(x);
                qfloat_t log_1mx = qf_log(qf_sub(qf_from_double(1.0), x));

                qfloat_t term1 = qf_mul(qf_sub(a, qf_from_double(1.0)), log_x);
                qfloat_t term2 = qf_mul(qf_sub(b, qf_from_double(1.0)), log_1mx);
                qfloat_t logB  = qf_logbeta(a, b);

                qfloat_t rhs = qf_sub(qf_add(term1, term2), logB);

                int ok = qf_close_rel(logpdf, rhs, 1e-30);

                if (ok) {
                    printf(C_GREEN "  OK: log f(%g; %g,%g)\n" C_RESET, xs[i], as[j], bs[k]);
                } else {
                    printf(C_RED "  FAIL: log f(%g; %g,%g)  [%s:%d]\n" C_RESET, xs[i], as[j], bs[k], __FILE__, __LINE__);
                    print_qf("logpdf", logpdf);
                    print_qf("rhs", rhs);
                    TEST_FAIL();
                }
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Symmetry
      f(x; a,b) = f(1-x; b,a)
   ------------------------------------------------------------------------- */
static void test_qf_beta_pdf_symmetry(void)
{
    printf(C_CYAN "TEST: qf_beta_pdf symmetry  f(x;a,b)=f(1-x;b,a)\n" C_RESET);

    double xs[] = { 0.1, 0.3, 0.6, 0.8 };
    double as[] = { 0.5, 2.0, 5.0 };
    double bs[] = { 0.5, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(as)/sizeof(as[0])); ++j) {
            for (int k = 0; k < (int)(sizeof(bs)/sizeof(bs[0])); ++k) {

                qfloat_t x = qf_from_double(xs[i]);
                qfloat_t a = qf_from_double(as[j]);
                qfloat_t b = qf_from_double(bs[k]);

                qfloat_t f1 = qf_beta_pdf(x, a, b);
                qfloat_t f2 = qf_beta_pdf(qf_sub(qf_from_double(1.0), x), b, a);

                int ok = qf_close_rel(f1, f2, 1e-30);

                if (ok) {
                    printf(C_GREEN "  OK: symmetry (%g; %g,%g)\n" C_RESET,
                           xs[i], as[j], bs[k]);
                } else {
                    printf(C_RED "  FAIL: symmetry (%g; %g,%g)  [%s:%d]\n" C_RESET, xs[i], as[j], bs[k], __FILE__, __LINE__);
                    print_qf("f(x;a,b)", f1);
                    print_qf("f(1-x;b,a)", f2);
                    TEST_FAIL();
                }
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_beta_pdf_all(void)
{
    RUN_TEST(test_qf_beta_pdf_definition, __func__);
    RUN_TEST(test_qf_beta_pdf_logform, __func__);
    RUN_TEST(test_qf_beta_pdf_symmetry, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check
      log f = (a-1)log x + (b-1)log(1-x) - logB
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_pdf_definition(void)
{
    printf(C_CYAN "TEST: qf_logbeta_pdf definition\n" C_RESET);

    double xs[] = { 0.1, 0.3, 0.6 };
    double as[] = { 0.5, 1.0, 2.0, 5.0 };
    double bs[] = { 0.5, 1.0, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(as)/sizeof(as[0])); ++j) {
            for (int k = 0; k < (int)(sizeof(bs)/sizeof(bs[0])); ++k) {

                qfloat_t x = qf_from_double(xs[i]);
                qfloat_t a = qf_from_double(as[j]);
                qfloat_t b = qf_from_double(bs[k]);

                qfloat_t logpdf = qf_logbeta_pdf(x, a, b);

                qfloat_t log_x   = qf_log(x);
                qfloat_t log_1mx = qf_log(qf_sub(qf_from_double(1.0), x));

                qfloat_t term1 = qf_mul(qf_sub(a, qf_from_double(1.0)), log_x);
                qfloat_t term2 = qf_mul(qf_sub(b, qf_from_double(1.0)), log_1mx);
                qfloat_t logB  = qf_logbeta(a, b);

                qfloat_t rhs = qf_sub(qf_add(term1, term2), logB);

                int ok = qf_close_rel(logpdf, rhs, 1e-30);

                if (ok) {
                    printf(C_GREEN "  OK: log f(%g; %g,%g)\n" C_RESET,
                           xs[i], as[j], bs[k]);
                } else {
                    printf(C_RED "  FAIL: log f(%g; %g,%g)  [%s:%d]\n" C_RESET, xs[i], as[j], bs[k], __FILE__, __LINE__);
                    print_qf("logpdf", logpdf);
                    print_qf("rhs", rhs);
                    TEST_FAIL();
                }
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Consistency with qf_beta_pdf
      log f = log(pdf)
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_pdf_consistency(void)
{
    printf(C_CYAN "TEST: qf_logbeta_pdf consistency with qf_beta_pdf\n" C_RESET);

    double xs[] = { 0.2, 0.4, 0.7 };
    double as[] = { 0.5, 2.0, 5.0 };
    double bs[] = { 0.5, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(as)/sizeof(as[0])); ++j) {
            for (int k = 0; k < (int)(sizeof(bs)/sizeof(bs[0])); ++k) {

                qfloat_t x = qf_from_double(xs[i]);
                qfloat_t a = qf_from_double(as[j]);
                qfloat_t b = qf_from_double(bs[k]);

                qfloat_t pdf    = qf_beta_pdf(x, a, b);
                qfloat_t logpdf = qf_logbeta_pdf(x, a, b);
                qfloat_t rhs    = qf_log(pdf);

                int ok = qf_close_rel(logpdf, rhs, 1e-30);

                if (ok) {
                    printf(C_GREEN "  OK: log f matches log(pdf)\n" C_RESET);
                } else {
                    printf(C_RED "  FAIL: log f mismatch  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
                    print_qf("logpdf", logpdf);
                    print_qf("log(pdf)", rhs);
                    TEST_FAIL();
                }
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Symmetry
      log f(x;a,b) = log f(1-x; b,a)
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_pdf_symmetry(void)
{
    printf(C_CYAN "TEST: qf_logbeta_pdf symmetry\n" C_RESET);

    double xs[] = { 0.1, 0.3, 0.6, 0.8 };
    double as[] = { 0.5, 2.0, 5.0 };
    double bs[] = { 0.5, 3.0, 4.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {
        for (int j = 0; j < (int)(sizeof(as)/sizeof(as[0])); ++j) {
            for (int k = 0; k < (int)(sizeof(bs)/sizeof(bs[0])); ++k) {

                qfloat_t x = qf_from_double(xs[i]);
                qfloat_t a = qf_from_double(as[j]);
                qfloat_t b = qf_from_double(bs[k]);

                qfloat_t f1 = qf_logbeta_pdf(x, a, b);
                qfloat_t f2 = qf_logbeta_pdf(qf_sub(qf_from_double(1.0), x), b, a);

                int ok = qf_close_rel(f1, f2, 1e-30);

                if (ok) {
                    printf(C_GREEN "  OK: symmetry (%g; %g,%g)\n" C_RESET,
                           xs[i], as[j], bs[k]);
                } else {
                    printf(C_RED "  FAIL: symmetry (%g; %g,%g)  [%s:%d]\n" C_RESET, xs[i], as[j], bs[k], __FILE__, __LINE__);
                    print_qf("log f(x;a,b)", f1);
                    print_qf("log f(1-x;b,a)", f2);
                    TEST_FAIL();
                }
            }
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_logbeta_pdf_all(void)
{
    RUN_TEST(test_qf_logbeta_pdf_definition, __func__);
    RUN_TEST(test_qf_logbeta_pdf_consistency, __func__);
    RUN_TEST(test_qf_logbeta_pdf_symmetry, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check
      φ(x) = exp(-x^2/2) / sqrt(2π)
   ------------------------------------------------------------------------- */
static void test_qf_normal_pdf_definition(void)
{
    printf(C_CYAN "TEST: qf_normal_pdf definition\n" C_RESET);

    double xs[] = { -3.0, -1.0, -0.5, 0.0, 0.5, 1.0, 3.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);
        qfloat_t pdf = qf_normal_pdf(x);

        /* RHS = exp(-x^2/2) / sqrt(2π) */
        qfloat_t x2   = qf_mul(x, x);
        qfloat_t expo = qf_mul(qf_neg(x2), qf_from_double(0.5));
        qfloat_t e    = qf_exp(expo);

        qfloat_t inv_sqrt_2pi = qf_from_string("0.3989422804014326779399460599343819");
        qfloat_t rhs = qf_mul(inv_sqrt_2pi, e);

        int ok = qf_close_rel(pdf, rhs, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: φ(%g)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: φ(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_qf("pdf", pdf);
            print_qf("rhs", rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Symmetry  φ(x) = φ(-x)
   ------------------------------------------------------------------------- */
static void test_qf_normal_pdf_symmetry(void)
{
    printf(C_CYAN "TEST: qf_normal_pdf symmetry\n" C_RESET);

    double xs[] = { 0.1, 0.5, 1.0, 2.0, 3.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x  = qf_from_double(xs[i]);
        qfloat_t nx = qf_neg(x);

        qfloat_t fx  = qf_normal_pdf(x);
        qfloat_t fnx = qf_normal_pdf(nx);

        int ok = qf_close_rel(fx, fnx, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: φ(%g) = φ(%g)\n" C_RESET, xs[i], -xs[i]);
        } else {
            printf(C_RED "  FAIL: φ(%g) = φ(%g)  [%s:%d]\n" C_RESET, xs[i], -xs[i], __FILE__, __LINE__);
            print_qf("φ(x)", fx);
            print_qf("φ(-x)", fnx);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: φ(0) = 1/sqrt(2π)
   ------------------------------------------------------------------------- */
static void test_qf_normal_pdf_at_zero(void)
{
    printf(C_CYAN "TEST: qf_normal_pdf at x=0\n" C_RESET);

    qfloat_t pdf0 = qf_normal_pdf(qf_from_double(0.0));
    qfloat_t expected = qf_from_string("0.3989422804014326779399460599343819");

    int ok = qf_close_rel(pdf0, expected, 1e-30);

    if (ok) {
        printf(C_GREEN "  OK: φ(0)\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: φ(0)  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        print_qf("φ(0)", pdf0);
        print_qf("expected", expected);
        TEST_FAIL();
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 4: Log-form consistency
      log φ(x) = -0.5*log(2π) - x^2/2
   ------------------------------------------------------------------------- */
static void test_qf_normal_pdf_logform(void)
{
    printf(C_CYAN "TEST: qf_normal_pdf log-form consistency\n" C_RESET);

    double xs[] = { -2.0, -1.0, -0.5, 0.5, 1.0, 2.0 };

    qfloat_t log_2pi = qf_log(QF_2PI);

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);

        qfloat_t pdf    = qf_normal_pdf(x);
        qfloat_t logpdf = qf_log(pdf);

        qfloat_t x2     = qf_mul(x, x);
        qfloat_t rhs    = qf_sub(qf_mul(qf_from_double(-0.5), log_2pi),
                               qf_mul(qf_from_double(0.5), x2));

        int ok = qf_close_rel(logpdf, rhs, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: log φ(%g)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: log φ(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_qf("logpdf", logpdf);
            print_qf("rhs", rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_normal_pdf_all(void)
{
    RUN_TEST(test_qf_normal_pdf_definition, __func__);
    RUN_TEST(test_qf_normal_pdf_symmetry, __func__);
    RUN_TEST(test_qf_normal_pdf_at_zero, __func__);
    RUN_TEST(test_qf_normal_pdf_logform, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check
      Φ(x) = 0.5 * (1 + erf(x / sqrt(2)))
   ------------------------------------------------------------------------- */
static void test_qf_normal_cdf_definition(void)
{
    printf(C_CYAN "TEST: qf_normal_cdf definition\n" C_RESET);

    double xs[] = { -3.0, -1.0, -0.5, 0.0, 0.5, 1.0, 3.0 };

    qfloat_t inv_sqrt2 = QF_SQRT_HALF;
    qfloat_t half = qf_from_double(0.5);
    qfloat_t one  = qf_from_double(1.0);

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);
        qfloat_t cdf = qf_normal_cdf(x);

        qfloat_t t = qf_mul(x, inv_sqrt2);
        qfloat_t rhs = qf_mul(half, qf_add(one, qf_erf(t)));

        int ok = qf_close_rel(cdf, rhs, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: Φ(%g)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: Φ(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_qf("cdf", cdf);
            print_qf("rhs", rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Symmetry  Φ(-x) = 1 - Φ(x)
   ------------------------------------------------------------------------- */
static void test_qf_normal_cdf_symmetry(void)
{
    printf(C_CYAN "TEST: qf_normal_cdf symmetry\n" C_RESET);

    double xs[] = { 0.1, 0.5, 1.0, 2.0, 3.0 };

    qfloat_t one = qf_from_double(1.0);

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x  = qf_from_double(xs[i]);
        qfloat_t nx = qf_neg(x);

        qfloat_t Fx  = qf_normal_cdf(x);
        qfloat_t Fnx = qf_normal_cdf(nx);

        qfloat_t rhs = qf_sub(one, Fx);

        int ok = qf_close_rel(Fnx, rhs, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: Φ(%g) + Φ(%g) = 1\n" C_RESET, xs[i], -xs[i]);
        } else {
            printf(C_RED "  FAIL: Φ(%g) + Φ(%g) = 1  [%s:%d]\n" C_RESET, xs[i], -xs[i], __FILE__, __LINE__);
            print_qf("Φ(-x)", Fnx);
            print_qf("1 - Φ(x)", rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Known values
      Φ(0) = 0.5
   ------------------------------------------------------------------------- */
static void test_qf_normal_cdf_known_values(void)
{
    printf(C_CYAN "TEST: qf_normal_cdf known values\n" C_RESET);

    qfloat_t F0 = qf_normal_cdf(qf_from_double(0.0));
    qfloat_t expected = qf_from_double(0.5);

    int ok = qf_close_rel(F0, expected, 1e-30);

    if (ok) {
        printf(C_GREEN "  OK: Φ(0) = 0.5\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: Φ(0)\n" C_RESET);
        print_qf("Φ(0)", F0);
        print_qf("expected", expected);
        TEST_FAIL();
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 4: PDF/CDF consistency using 9-point central difference
      Φ'(x) = φ(x)
   ------------------------------------------------------------------------- */
static void test_qf_normal_cdf_pdf_consistency(void)
{
    printf(C_CYAN "TEST: qf_normal_cdf derivative consistency\n" C_RESET);

    double xs[] = { -2.0, -1.0, -0.5, 0.5, 1.0, 2.0 };

    /* h chosen to avoid cancellation but still small */
    qfloat_t h = qf_from_double(1e-9);

    qfloat_t two   = qf_from_double(2.0);
    qfloat_t three = qf_from_double(3.0);
    qfloat_t four  = qf_from_double(4.0);

    qfloat_t c3    = qf_from_double(3.0);
    qfloat_t c32   = qf_from_double(32.0);
    qfloat_t c168  = qf_from_double(168.0);
    qfloat_t c672  = qf_from_double(672.0);
    qfloat_t c840  = qf_from_double(840.0);

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);

        /* Evaluate Φ at shifted points */
        qfloat_t Fm4 = qf_normal_cdf(qf_sub(x, qf_mul(four, h)));
        qfloat_t Fm3 = qf_normal_cdf(qf_sub(x, qf_mul(three, h)));
        qfloat_t Fm2 = qf_normal_cdf(qf_sub(x, qf_mul(two, h)));
        qfloat_t Fm1 = qf_normal_cdf(qf_sub(x, h));

        qfloat_t Fp1 = qf_normal_cdf(qf_add(x, h));
        qfloat_t Fp2 = qf_normal_cdf(qf_add(x, qf_mul(two, h)));
        qfloat_t Fp3 = qf_normal_cdf(qf_add(x, qf_mul(three, h)));
        qfloat_t Fp4 = qf_normal_cdf(qf_add(x, qf_mul(four, h)));

        /* 9-point stencil numerator */
        qfloat_t num = {0.0, 0.0};

        num = qf_add(num, qf_mul(c3,   Fm4));
        num = qf_sub(num, qf_mul(c32,  Fm3));
        num = qf_add(num, qf_mul(c168, Fm2));
        num = qf_sub(num, qf_mul(c672, Fm1));

        num = qf_add(num, qf_mul(c672, Fp1));
        num = qf_sub(num, qf_mul(c168, Fp2));
        num = qf_add(num, qf_mul(c32,  Fp3));
        num = qf_sub(num, qf_mul(c3,   Fp4));

        /* denominator = 840 h */
        qfloat_t denom = qf_mul(c840, h);

        qfloat_t dF = qf_div(num, denom);
        qfloat_t pdf = qf_normal_pdf(x);

        int ok = qf_close_rel(dF, pdf, 1e-22);

        if (ok) {
            printf(C_GREEN "  OK: Φ'(%g) = φ(%g)\n" C_RESET, xs[i], xs[i]);
        } else {
            printf(C_RED "  FAIL: Φ'(%g) = φ(%g)  [%s:%d]\n" C_RESET, xs[i], xs[i], __FILE__, __LINE__);
            print_qf("dΦ/dx", dF);
            print_qf(" φ(x)", pdf);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_normal_cdf_all(void)
{
    RUN_TEST(test_qf_normal_cdf_definition, __func__);
    RUN_TEST(test_qf_normal_cdf_symmetry, __func__);
    RUN_TEST(test_qf_normal_cdf_known_values, __func__);
    RUN_TEST(test_qf_normal_cdf_pdf_consistency, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check
      log φ(x) = -0.5*log(2π) - 0.5*x^2
   ------------------------------------------------------------------------- */
static void test_qf_normal_logpdf_definition(void)
{
    printf(C_CYAN "TEST: qf_normal_logpdf definition\n" C_RESET);

    double xs[] = { -3.0, -1.0, -0.5, 0.0, 0.5, 1.0, 3.0 };

    qfloat_t log_2pi = QF_LN_2PI;
    qfloat_t half = qf_from_double(0.5);

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);
        qfloat_t logpdf = qf_normal_logpdf(x);

        qfloat_t x2 = qf_mul(x, x);
        qfloat_t rhs = qf_neg(qf_add(qf_mul(half, log_2pi),
                                   qf_mul(half, x2)));

        int ok = qf_close_rel(logpdf, rhs, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: log φ(%g)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: log φ(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_qf("logpdf", logpdf);
            print_qf("rhs", rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 2: Consistency with qf_normal_pdf
      log φ(x) = log(pdf)
   ------------------------------------------------------------------------- */
static void test_qf_normal_logpdf_consistency(void)
{
    printf(C_CYAN "TEST: qf_normal_logpdf consistency with qf_normal_pdf\n" C_RESET);

    double xs[] = { -2.0, -1.0, -0.5, 0.5, 1.0, 2.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x = qf_from_double(xs[i]);

        qfloat_t logpdf = qf_normal_logpdf(x);
        qfloat_t pdf    = qf_normal_pdf(x);
        qfloat_t rhs    = qf_log(pdf);

        int ok = qf_close_rel(logpdf, rhs, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: log φ(%g) matches log(pdf)\n" C_RESET, xs[i]);
        } else {
            printf(C_RED "  FAIL: log φ(%g)  [%s:%d]\n" C_RESET, xs[i], __FILE__, __LINE__);
            print_qf("logpdf", logpdf);
            print_qf("log(pdf)", rhs);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 3: Symmetry  log φ(x) = log φ(-x)
   ------------------------------------------------------------------------- */
static void test_qf_normal_logpdf_symmetry(void)
{
    printf(C_CYAN "TEST: qf_normal_logpdf symmetry\n" C_RESET);

    double xs[] = { 0.1, 0.5, 1.0, 2.0, 3.0 };

    for (int i = 0; i < (int)(sizeof(xs)/sizeof(xs[0])); ++i) {

        qfloat_t x  = qf_from_double(xs[i]);
        qfloat_t nx = qf_neg(x);

        qfloat_t fx  = qf_normal_logpdf(x);
        qfloat_t fnx = qf_normal_logpdf(nx);

        int ok = qf_close_rel(fx, fnx, 1e-30);

        if (ok) {
            printf(C_GREEN "  OK: log φ(%g) = log φ(%g)\n" C_RESET, xs[i], -xs[i]);
        } else {
            printf(C_RED "  FAIL: log φ(%g) = log φ(%g)  [%s:%d]\n" C_RESET, xs[i], -xs[i], __FILE__, __LINE__);
            print_qf("log φ(x)", fx);
            print_qf("log φ(-x)", fnx);
            TEST_FAIL();
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   TEST 4: Known value at x = 0
      log φ(0) = -0.5 * log(2π)
   ------------------------------------------------------------------------- */
static void test_qf_normal_logpdf_at_zero(void)
{
    printf(C_CYAN "TEST: qf_normal_logpdf at x=0\n" C_RESET);

    qfloat_t logpdf0 = qf_normal_logpdf(qf_from_double(0.0));
    qfloat_t expected = qf_neg(qf_mul(qf_from_double(0.5), QF_LN_2PI));

    int ok = qf_close_rel(logpdf0, expected, 1e-30);

    if (ok) {
        printf(C_GREEN "  OK: log φ(0)\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: log φ(0)  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        print_qf("log φ(0)", logpdf0);
        print_qf("expected", expected);
        TEST_FAIL();
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
   Master test driver
   ------------------------------------------------------------------------- */
static void test_qf_normal_logpdf_all(void)
{
    RUN_TEST(test_qf_normal_logpdf_definition, __func__);
    RUN_TEST(test_qf_normal_logpdf_consistency, __func__);
    RUN_TEST(test_qf_normal_logpdf_symmetry, __func__);
    RUN_TEST(test_qf_normal_logpdf_at_zero, __func__);
}

void test_lambert_w(void) {
    RUN_TEST(test_qf_lambert_w0, __func__);
    RUN_TEST(test_qf_lambert_wm1, __func__);
    RUN_TEST(test_qf_productlog_all, __func__);
}

void test_beta_logbeta_binomial_beta_pdf_logbeta_pdf_normal_pdf_cdf_logpdf(void) {
    RUN_TEST(test_qf_beta_all, __func__);
    RUN_TEST(test_qf_logbeta_all, __func__);
    RUN_TEST(test_qf_binomial_all, __func__);
    RUN_TEST(test_qf_beta_pdf_all, __func__);
    RUN_TEST(test_qf_logbeta_pdf_all, __func__);
    RUN_TEST(test_qf_normal_pdf_all, __func__);
    RUN_TEST(test_qf_normal_cdf_all, __func__);
    RUN_TEST(test_qf_normal_logpdf_all, __func__);
}

/* -------------------------------------------------------------------------
   TEST 1: Definition check  W(x)*exp(W(x)) = x
   ------------------------------------------------------------------------- */
