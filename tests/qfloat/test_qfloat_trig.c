#include "test_qfloat.h"

static void test_qf_trig()
{
    printf("%sTEST: qf_sin / qf_cos / qf_tan%s\n", C_CYAN, C_RESET);

    char buf[256];
    char buf_exp[256];

    /* 1) sin(0) = 0, cos(0) = 1, tan(0) = 0 */
    {
        qfloat_t x = qf_from_double(0.0);

        qfloat_t s = qf_sin(x);
        qfloat_t c = qf_cos(x);
        qfloat_t t = qf_tan(x);

        qfloat_t s_exp = qf_from_double(0.0);
        qfloat_t c_exp = qf_from_double(1.0);
        qfloat_t t_exp = qf_from_double(0.0);

        qf_to_string(s, buf, sizeof(buf));
        if (qf_close(s, s_exp, 1e-30)) {
            printf("%s  OK: sin(0) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
        } else {
            printf("%s  FAIL: sin(0)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
            TEST_FAIL();
        }

        qf_to_string(c, buf, sizeof(buf));
        if (qf_close(c, c_exp, 1e-30)) {
            printf("%s  OK: cos(0) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = 1\n");
        } else {
            printf("%s  FAIL: cos(0)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 1\n");
            TEST_FAIL();
        }

        qf_to_string(t, buf, sizeof(buf));
        if (qf_close(t, t_exp, 1e-30)) {
            printf("%s  OK: tan(0) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
        } else {
            printf("%s  FAIL: tan(0)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
            TEST_FAIL();
        }
    }

    /* 2) sin(pi/2) = 1, cos(pi/2) = 0 */
    {
        qfloat_t x = QF_PI_2;

        qfloat_t s = qf_sin(x);
        qfloat_t c = qf_cos(x);

        qfloat_t s_exp = qf_from_double(1.0);
        qfloat_t c_exp = qf_from_double(0.0);

        qf_to_string(s, buf, sizeof(buf));
        if (qf_close(s, s_exp, 1e-30)) {
            printf("%s  OK: sin(pi/2) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = 1\n");
        } else {
            printf("%s  FAIL: sin(pi/2)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 1\n");
            TEST_FAIL();
        }

        qf_to_string(c, buf, sizeof(buf));
        if (qf_close(c, c_exp, 1e-30)) {
            printf("%s  OK: cos(pi/2) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
        } else {
            printf("%s  FAIL: cos(pi/2)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
            TEST_FAIL();
        }
    }

    /* 3) sin(pi) = 0, cos(pi) = -1 */
    {
        qfloat_t x = QF_PI;

        qfloat_t s = qf_sin(x);
        qfloat_t c = qf_cos(x);

        qfloat_t s_exp = qf_from_double(0.0);
        qfloat_t c_exp = qf_from_double(-1.0);

        qf_to_string(s, buf, sizeof(buf));
        if (qf_close(s, s_exp, 1e-30)) {
            printf("%s  OK: sin(pi) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
        } else {
            printf("%s  FAIL: sin(pi)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
            TEST_FAIL();
        }

        qf_to_string(c, buf, sizeof(buf));
        if (qf_close(c, c_exp, 1e-30)) {
            printf("%s  OK: cos(pi) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = -1\n");
        } else {
            printf("%s  FAIL: cos(pi)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = -1\n");
            TEST_FAIL();
        }
    }

    /* 4) tan(pi/4) = 1 */
    {
        qfloat_t x = QF_PI_4;

        qfloat_t t = qf_tan(x);
        qfloat_t t_exp = qf_from_double(1.0);

        qf_to_string(t, buf, sizeof(buf));
        if (qf_close(t, t_exp, 1e-30)) {
            printf("%s  OK: tan(pi/4) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = 1\n");
        } else {
            printf("%s  FAIL: tan(pi/4)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 1\n");
            TEST_FAIL();
        }
    }

    /* 5) tan(pi/2) → NaN */
    {
        qfloat_t x = QF_PI_2;
        qfloat_t t = qf_tan(x);

        if (qf_isnan(t)) {
            printf("%s  OK: tan(pi/2) = NaN%s\n", C_GREEN, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = NaN\n");
        } else {
            qf_to_string(t, buf, sizeof(buf));
            printf("%s  FAIL: tan(pi/2)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = NaN\n");
            TEST_FAIL();
        }
    }

    /* 6) Random spot check: sin(1.0), cos(1.0), tan(1.0) */
    {
        qfloat_t x = qf_from_double(1.0);

        qfloat_t s = qf_sin(x);
        qfloat_t c = qf_cos(x);
        qfloat_t t = qf_tan(x);

        qfloat_t s_exp = qf_from_string("0.8414709848078965066525023216303");
        qfloat_t c_exp = qf_from_string("0.54030230586813971740093660744298");
        qfloat_t t_exp = qf_from_string("1.55740772465490223050697480745836");

        qf_to_string(s, buf, sizeof(buf));
        qf_to_string(s_exp, buf_exp, sizeof(buf_exp));
        if (qf_close(s, s_exp, 1e-30)) {
            printf("%s  OK: sin(1) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: sin(1)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }

        qf_to_string(c, buf, sizeof(buf));
        qf_to_string(c_exp, buf_exp, sizeof(buf_exp));
        if (qf_close(c, c_exp, 1e-30)) {
            printf("%s  OK: cos(1) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: cos(1)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }

        qf_to_string(t, buf, sizeof(buf));
        qf_to_string(t_exp, buf_exp, sizeof(buf_exp));
        if (qf_close(t, t_exp, 1e-30)) {
            printf("%s  OK: tan(1) = %s%s\n", C_GREEN, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: tan(1)%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
}

/* ============================================================
 *  Inverse Trigonometric Function Tests
 * ============================================================ */

static void test_qf_atan(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } atan_tests[] = {
        /* exact */
        { { 0.0, 0.0 }, "atan(0)",     { 0.0, 0.0 } },

        /* exact: atan(1) = pi/4 */
        { { 1.0, 0.0 }, "atan(1)",     QF_PI_4 },

        /* exact: atan(-1) = -pi/4 */
        { { -1.0, 0.0 }, "atan(-1)",   qf_neg(QF_PI_4) },

        /* numeric: atan(0.5) = 0.46364760900080611621425623146121 */
        { { 0.5, 0.0 }, "atan(0.5)", qf_from_string("0.46364760900080611621425623146121") },

        /* numeric: atan(2) = 1.1071487177940905030170654601785 */
        { { 2.0, 0.0 }, "atan(2)", qf_from_string("1.1071487177940905030170654601785") },
    };

    int N_ATAN_TESTS = sizeof(atan_tests) / sizeof(atan_tests[0]);

    char buf[128], buf_exp[128];

    printf("\n== qf_atan ==\n");

    for (int i = 0; i < N_ATAN_TESTS; ++i) {
        qfloat_t x = atan_tests[i].arg;
        qfloat_t expected = atan_tests[i].expected;
        qfloat_t got = qf_atan(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, atan_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, atan_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
}


/* ============================================================
 *  asin tests
 * ============================================================ */


static void test_qf_asin(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } asin_tests[] = {
        { { 0.0, 0.0 }, "asin(0)",   { 0.0, 0.0 } },

        /* exact: asin(0.5) = pi/6 */
        { { 0.5, 0.0 }, "asin(0.5)", QF_PI_6 },

        /* exact: asin(1) = pi/2 */
        { { 1.0, 0.0 }, "asin(1)",   QF_PI_2 },

        /* exact: asin(-1) = -pi/2 */
        { { -1.0, 0.0 }, "asin(-1)", qf_neg(QF_PI_2) },
    };

    int N_ASIN_TESTS = sizeof(asin_tests) / sizeof(asin_tests[0]);

    char buf[128], buf_exp[128];

    printf("\n== qf_asin ==\n");

    for (int i = 0; i < N_ASIN_TESTS; ++i) {
        qfloat_t x = asin_tests[i].arg;
        qfloat_t expected = asin_tests[i].expected;
        qfloat_t got = qf_asin(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, asin_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, asin_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* Domain error: asin(2) = NaN */
    qfloat_t r = qf_asin(qf_from_double(2.0));
    if (isnan(qf_to_double(r))) {
        printf("%s  OK: asin(2) = NaN%s\n", C_GREEN, C_RESET);
    } else {
        qf_to_string(r, buf, sizeof(buf));
        printf("%s  FAIL: asin(2) should be NaN%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
        printf("    got = %s\n", buf);
        TEST_FAIL();
    }
}


/* ============================================================
 *  acos tests
 * ============================================================ */

static void test_qf_acos(void)
{
    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } acos_tests[] = {
        /* exact */
        { { 1.0, 0.0 }, "acos(1)",   { 0.0, 0.0 } },

        /* exact: acos(0) = pi/2 */
        { { 0.0, 0.0 }, "acos(0)",   QF_PI_2 },

        /* exact: acos(-1) = pi */
        { { -1.0, 0.0 }, "acos(-1)", QF_PI },

        /* exact: acos(0.5) = pi/3 */
        { { 0.5, 0.0 }, "acos(0.5)", QF_PI_3 },
    };

    int N_ACOS_TESTS = sizeof(acos_tests) / sizeof(acos_tests[0]);

    char buf[128], buf_exp[128];

    printf("\n== qf_acos ==\n");

    for (int i = 0; i < N_ACOS_TESTS; ++i) {
        qfloat_t x = acos_tests[i].arg;
        qfloat_t expected = acos_tests[i].expected;
        qfloat_t got = qf_acos(x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, acos_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, acos_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* Domain error: acos(2) = NaN */
    qfloat_t r = qf_acos(qf_from_double(2.0));
    if (isnan(qf_to_double(r))) {
        printf("%s  OK: acos(2) = NaN%s\n", C_GREEN, C_RESET);
    } else {
        qf_to_string(r, buf, sizeof(buf));
        printf("%s  FAIL: acos(2) should be NaN%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
        printf("    got = %s\n", buf);
        TEST_FAIL();
    }
}


/* ============================================================
 *  atan2 tests
 * ============================================================ */

static void test_qf_atan2(void)
{
    struct {
        qfloat_t y, x;
        const char *name;
        qfloat_t expected;
    } atan2_tests[] = {
        /* exact */
        { {0.0,0.0}, {1.0,0.0}, "atan2(0,1)",   {0.0,0.0} },

        { {1.0,0.0}, {0.0,0.0}, "atan2(1,0)",   QF_PI_2 },
        { {-1.0,0.0},{0.0,0.0}, "atan2(-1,0)",  qf_neg(QF_PI_2) },

        { {1.0,0.0}, {1.0,0.0}, "atan2(1,1)",   QF_PI_4 },

        /* atan2(-1,-1) = -3π/4 */
        { {-1.0,0.0},{-1.0,0.0},"atan2(-1,-1)", qf_neg(qf_sub(QF_PI, QF_PI_4)) },

        /* atan2(1,-1) = 3π/4 */
        { {1.0,0.0}, {-1.0,0.0},"atan2(1,-1)",  qf_sub(QF_PI, QF_PI_4) },
    };

    int N_ATAN2_TESTS = sizeof(atan2_tests) / sizeof(atan2_tests[0]);

    char buf[128], buf_exp[128];

    printf("\n== qf_atan2 ==\n");

    for (int i = 0; i < N_ATAN2_TESTS; ++i) {
        qfloat_t y = atan2_tests[i].y;
        qfloat_t x = atan2_tests[i].x;
        qfloat_t expected = atan2_tests[i].expected;
        qfloat_t got = qf_atan2(y, x);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(expected, buf_exp, sizeof(buf_exp));

        if (qf_close(got, expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n", C_GREEN, atan2_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, atan2_tests[i].name, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }
}

void test_trigonometric(void) {
    RUN_SUBTEST(test_qf_trig);
    RUN_SUBTEST(test_qf_atan);
    RUN_SUBTEST(test_qf_atan2);
    RUN_SUBTEST(test_qf_asin);
    RUN_SUBTEST(test_qf_acos);
}
