#include "test_qfloat.h"

static void test_qf_atanh(void);

static void test_qf_sinh(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } sinh_tests[] = {
        { { 0.0, 0.0 }, "sinh(0)",   { 0.0, 0.0 } },

        /* sinh(1) */
        { { 1.0, 0.0 }, "sinh(1)",   qf_from_string("1.1752011936438014568823818505956") },

        /* sinh(-1) */
        { { -1.0, 0.0 }, "sinh(-1)", qf_from_string("-1.1752011936438014568823818505956") },

        /* sinh(0.5) */
        { { 0.5, 0.0 }, "sinh(0.5)", qf_from_string("0.52109530549374736162242562641149") },
    };

    int N = sizeof(sinh_tests) / sizeof(sinh_tests[0]);
    char buf[128], buf_exp[128];

    printf("\n== qf_sinh ==\n");

    for (int i = 0; i < N; ++i) {
        qfloat_t x = sinh_tests[i].arg;
        qfloat_t expected = sinh_tests[i].expected;
        qfloat_t got = qf_sinh(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n",
                   C_GREEN, sinh_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, sinh_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
}

static void test_qf_cosh(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } cosh_tests[] = {
        { { 0.0, 0.0 }, "cosh(0)",   { 1.0, 0.0 } },

        /* cosh(1) */
        { { 1.0, 0.0 }, "cosh(1)",   qf_from_string("1.5430806348152437784779056207571") },

        /* cosh(-1) */
        { { -1.0, 0.0 }, "cosh(-1)", qf_from_string("1.5430806348152437784779056207571") },

        /* cosh(0.5) */
        { { 0.5, 0.0 }, "cosh(0.5)", qf_from_string("1.127625965206380785226225161402") },
    };

    int N = sizeof(cosh_tests) / sizeof(cosh_tests[0]);
    char buf[128], buf_exp[128];

    printf("\n== qf_cosh ==\n");

    for (int i = 0; i < N; ++i) {
        qfloat_t x = cosh_tests[i].arg;
        qfloat_t expected = cosh_tests[i].expected;
        qfloat_t got = qf_cosh(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, cosh_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, cosh_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
}

static void test_qf_tanh(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } tanh_tests[] = {
        { { 0.0, 0.0 }, "tanh(0)",     { 0.0, 0.0 } },

        /* tanh(1) */
        { { 1.0, 0.0 }, "tanh(1)",     qf_from_string("0.76159415595576488811945828260479") },

        /* tanh(-1) */
        { { -1.0, 0.0 }, "tanh(-1)",   qf_from_string("-0.76159415595576488811945828260479") },

        /* tanh(0.5) */
        { { 0.5, 0.0 }, "tanh(0.5)",   qf_from_string("0.46211715726000975850231848364367") },

        /* tanh(10) */
        { { 10.0, 0.0 }, "tanh(10)",   qf_from_string("0.99999999587769276361959283713828") },

        /* tanh(-10) */
        { { -10.0, 0.0 }, "tanh(-10)", qf_from_string("-0.9999999958776927636195928371383") },
    };

    int N = sizeof(tanh_tests) / sizeof(tanh_tests[0]);
    char buf[128], buf_exp[128];

    printf("\n== qf_tanh ==\n");

    for (int i = 0; i < N; ++i) {
        qfloat_t x = tanh_tests[i].arg;
        qfloat_t expected = tanh_tests[i].expected;
        qfloat_t got = qf_tanh(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, tanh_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, tanh_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
}

static void test_qf_asinh(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } asinh_tests[] = {
        { { 0.0, 0.0 }, "asinh(0)",   { 0.0, 0.0 } },

        /* asinh(1) = 0.88137358701954302523260932497979 */
        { { 1.0, 0.0 }, "asinh(1)",   qf_from_string("0.88137358701954302523260932497979") },

        /* asinh(-1) = -asinh(1) */
        { { -1.0, 0.0 }, "asinh(-1)", qf_from_string("-0.88137358701954302523260932497979") },

        /* asinh(0.5) = 0.48121182505960344749775891342437 */
        { { 0.5, 0.0 }, "asinh(0.5)", qf_from_string("0.48121182505960344749775891342437") },
    };

    int N = sizeof(asinh_tests) / sizeof(asinh_tests[0]);
    char buf[128], buf_exp[128];

    printf("\n== qf_asinh ==\n");

    for (int i = 0; i < N; ++i) {
        qfloat_t x = asinh_tests[i].arg;
        qfloat_t expected = asinh_tests[i].expected;
        qfloat_t got = qf_asinh(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, asinh_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, asinh_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
}

static void test_qf_acosh(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } acosh_tests[] = {
        /* acosh(1) = 0 */
        { { 1.0, 0.0 }, "acosh(1)",   { 0.0, 0.0 } },

        /* acosh(2) = 1.3169578969248167086250463473079 */
        { { 2.0, 0.0 }, "acosh(2)",   qf_from_string("1.3169578969248167086250463473079") },

        /* acosh(10) = 2.993222846126380897912667713774 */
        { { 10.0, 0.0 }, "acosh(10)", qf_from_string("2.993222846126380897912667713774") },
    };

    int N = sizeof(acosh_tests) / sizeof(acosh_tests[0]);
    char buf[128], buf_exp[128];

    printf("\n== qf_acosh ==\n");

    for (int i = 0; i < N; ++i) {
        qfloat_t x = acosh_tests[i].arg;
        qfloat_t expected = acosh_tests[i].expected;
        qfloat_t got = qf_acosh(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, acosh_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, acosh_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* Domain error: acosh(x<1) = NaN */
    qfloat_t r = qf_acosh(qf_from_double(0.5));
    if (isnan(qf_to_double(r))) {
        printf("%s  OK: acosh(0.5) = NaN%s\n", C_GREEN, C_RESET);
    } else {
        qf_to_string(r, buf, sizeof(buf));
        printf("%s  FAIL: acosh(0.5) should be NaN%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
        printf("    got = %s\n", buf);
        TEST_FAIL();
    }
}

void test_hyperbolic(void) {
    RUN_SUBTEST(test_qf_cosh);
    RUN_SUBTEST(test_qf_sinh);
    RUN_SUBTEST(test_qf_tanh);
    RUN_SUBTEST(test_qf_asinh);
    RUN_SUBTEST(test_qf_acosh);
    RUN_SUBTEST(test_qf_atanh);
}

static void test_qf_atanh(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } atanh_tests[] = {
        { { 0.0, 0.0 }, "atanh(0)",     { 0.0, 0.0 } },

        /* atanh(0.5) = 0.54930614433405484569762261846126 */
        { { 0.5, 0.0 }, "atanh(0.5)",   qf_from_string("0.54930614433405484569762261846126") },

        /* atanh(-0.5) = -atanh(0.5) */
        { { -0.5, 0.0 }, "atanh(-0.5)", qf_from_string("-0.54930614433405484569762261846126") },

        /* atanh(0.125) = 0.125657214140453038842568865200936 */
        { { 0.125, 0.0 }, "atanh(0.125)",   qf_from_string("0.125657214140453038842568865200936") },
    };

    int N = sizeof(atanh_tests) / sizeof(atanh_tests[0]);
    char buf[128], buf_exp[128];

    printf("\n== qf_atanh ==\n");

    for (int i = 0; i < N; ++i) {
        qfloat_t x = atanh_tests[i].arg;
        qfloat_t expected = atanh_tests[i].expected;
        qfloat_t got = qf_atanh(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, atanh_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, atanh_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* Domain error: |x| >= 1 → NaN */
    qfloat_t r = qf_atanh(qf_from_double(1.0));
    if (isnan(qf_to_double(r))) {
        printf("%s  OK: atanh(1) = NaN%s\n", C_GREEN, C_RESET);
    } else {
        qf_to_string(r, buf, sizeof(buf));
        printf("%s  FAIL: atanh(1) should be NaN%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
        printf("    got = %s\n", buf);
        TEST_FAIL();
    }
}

/* -----------------------------------------------------------
   qf_hypot tests (full qfloat_t precision)
   ----------------------------------------------------------- */
