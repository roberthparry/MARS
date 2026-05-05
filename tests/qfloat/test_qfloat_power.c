#include "test_qfloat.h"

static void test_qf_pow_int()
{
    printf("%sTEST: qf_pow_int%s\n", C_CYAN, C_RESET);

    struct {
        const char *x;
        int n;
        const char *expected;
    } cases[] = {
        { "2",   0,  "1" },
        { "2",   1,  "2" },
        { "2",   2,  "4" },
        { "2",   3,  "8" },
        { "2",  10,  "1024" },
        { "10", -1,  "0.1" },
        { "10", -2,  "0.01" },
        { "5",   3,  "125" },
        { "0",   5,  "0" },
        { "1", 123,  "1" },
    };

    char buf[256];

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {

        qfloat_t x        = qf_from_string(cases[i].x);
        qfloat_t r        = qf_pow_int(x, cases[i].n);
        qfloat_t expected = qf_from_string(cases[i].expected);

        /* THIS LINE WAS MISSING */
        qf_to_string(r, buf, sizeof(buf));

        if (qf_close(r, expected, 1e-30)) {
            printf("%s  OK: %s^%d = %s%s\n",
                   C_GREEN, cases[i].x, cases[i].n, buf, C_RESET);
        } else {
            printf("%s  FAIL: %s^%d%s  [%s:%d]\n", C_RED, cases[i].x, cases[i].n, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", cases[i].expected);
            TEST_FAIL();
        }
    }
}

static void test_qf_pow()
{
    printf("%sTEST: qf_pow%s\n", C_CYAN, C_RESET);

    char buf[256];

    /* 1) 2^3 = 8 */
    {
        qfloat_t r = qf_pow(qf_from_double(2.0), qf_from_double(3.0));
        qfloat_t expected = qf_from_double(8.0);
        qf_to_string(r, buf, sizeof(buf));

        if (qf_close(r, expected, 1e-30)) {
            printf("%s  OK: 2^3 = %s%s\n", C_GREEN, buf, C_RESET);
        } else {
            printf("%s  FAIL: 2^3%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 8\n");
            TEST_FAIL();
        }
    }

    /* 2) 9^0.5 = 3 */
    {
        qfloat_t r = qf_pow(qf_from_double(9.0), qf_from_double(0.5));
        qfloat_t expected = qf_from_double(3.0);
        qf_to_string(r, buf, sizeof(buf));

        if (qf_close(r, expected, 1e-30)) {
            printf("%s  OK: 9^0.5 = %s%s\n", C_GREEN, buf, C_RESET);
        } else {
            printf("%s  FAIL: 9^0.5%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 3\n");
            TEST_FAIL();
        }
    }

    /* 3) 0^5 = 0 */
    {
        qfloat_t r = qf_pow(qf_from_double(0.0), qf_from_double(5.0));
        qfloat_t expected = qf_from_double(0.0);
        qf_to_string(r, buf, sizeof(buf));

        if (qf_close(r, expected, 1e-30)) {
            printf("%s  OK: 0^5 = %s%s\n", C_GREEN, buf, C_RESET);
        } else {
            printf("%s  FAIL: 0^5%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = 0\n");
            TEST_FAIL();
        }
    }

    /* 4) 0^-1 → NaN */
    {
        qfloat_t r = qf_pow(qf_from_double(0.0), qf_from_double(-1.0));

        if (qf_isnan(r)) {
            printf("%s  OK: 0^-1 = NaN%s\n", C_GREEN, C_RESET);
        } else {
            qf_to_string(r, buf, sizeof(buf));
            printf("%s  FAIL: 0^-1%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = NaN\n");
            TEST_FAIL();
        }
    }

    /* 5) (-2)^3 = -8 */
    {
        qfloat_t r = qf_pow(qf_from_double(-2.0), qf_from_double(3.0));
        qfloat_t expected = qf_from_double(-8.0);
        qf_to_string(r, buf, sizeof(buf));

        if (qf_close(r, expected, 1e-30)) {
            printf("%s  OK: (-2)^3 = %s%s\n", C_GREEN, buf, C_RESET);
        } else {
            printf("%s  FAIL: (-2)^3%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = -8\n");
            TEST_FAIL();
        }
    }

    /* 6) (-2)^0.5 → NaN */
    {
        qfloat_t r = qf_pow(qf_from_double(-2.0), qf_from_double(0.5));

        if (qf_isnan(r)) {
            printf("%s  OK: (-2)^0.5 = NaN%s\n", C_GREEN, C_RESET);
        } else {
            qf_to_string(r, buf, sizeof(buf));
            printf("%s  FAIL: (-2)^0.5%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = NaN\n");
            TEST_FAIL();
        }
    }
}

static void test_qf_pow10(void)
{
    printf("%sTEST: qf_pow10%s\n", C_CYAN, C_RESET);

    char buf[256];

    struct {
        int n;
        const char *expected;
    } cases[] = {
        {  0,  "1"    },
        {  1,  "10"   },
        {  2,  "100"  },
        { -1,  "0.1"  },
        { -2,  "0.01" },
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        qfloat_t r        = qf_pow10(cases[i].n);
        qfloat_t expected = qf_from_string(cases[i].expected);

        qf_to_string(r, buf, sizeof(buf));

        if (qf_close(r, expected, 1e-30)) {
            printf("%s  OK: 10^%d = %s%s\n",
                   C_GREEN, cases[i].n, buf, C_RESET);
        } else {
            printf("%s  FAIL: 10^%d%s  [%s:%d]\n", C_RED, cases[i].n, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", cases[i].expected);
            TEST_FAIL();
        }
    }
}

void test_power(void) {
    RUN_SUBTEST(test_qf_pow_int);
    RUN_SUBTEST(test_qf_pow);
    RUN_SUBTEST(test_qf_pow10);
}
