
#include "test_qfloat.h"

/* -----------------------------------------------------------
   Arithmetic tests
   ----------------------------------------------------------- */

static int qf_close_value(qfloat_t got, qfloat_t expected, double tol)
{
    if (qf_close(got, expected, tol))
        return 1;
    if (qf_eq(expected, qf_from_double(0.0)))
        return 0;
    return qf_close_rel(got, expected, tol);
}

static void test_add() {
    printf(C_CYAN "TEST: addition\n" C_RESET);

    qfloat_t a = qf_from_string("1.2345678901234561234567891234567");
    qfloat_t b = qf_from_string("9.8765432109876541234567891234567");
    qfloat_t got = qf_add(a, b);

    char buf[64];
    qf_to_string(got, buf, sizeof(buf));
    char buf_exp[64] = "11.1111111011111102469135782469134";
    qfloat_t expected = qf_from_string(buf_exp);

    if (qf_close(got, expected, 1e-30)) {
        printf("%s  OK: %s = %s%s\n", C_GREEN, "1.2345678901234561234567891234567 + 9.8765432109876541234567891234567", "11.1111111011111102469135782469134", C_RESET);
    } else {
        printf("FAIL: %s:%d:1: addition\n", __FILE__, __LINE__);
        printf("%s  FAIL: %s%s\n", C_RED, "1.2345678901234561234567891234567 + 9.8765432109876541234567891234567", C_RESET);
        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
        TEST_FAIL();
    }
}

static void test_mul() {
    printf(C_CYAN "TEST: multiplication\n" C_RESET);

    char a_buf[64] = "9.8765431209876543171934981073984";
    char b_buf[64] = "0.1256789123456789012345678901234567";
    qfloat_t a = qf_from_string(a_buf);
    qfloat_t b = qf_from_string(b_buf);
    qfloat_t r = qf_mul(a, b);
    char r_buf[64];
    qf_to_string(r, r_buf, sizeof(r_buf));

    char buf_exp[64] = "1.2412731971809253340758239961506";
    qfloat_t expected = qf_from_string(buf_exp);

    char buf_name[256];
    sprintf(buf_name, "%s * %s", a_buf, b_buf);

    if (qf_close(r, expected, 1e-30)) {
        printf("%s  OK: %s = %s%s\n", C_GREEN, buf_name, buf_exp, C_RESET);
        printf("    got      = %s\n", r_buf);
        printf("    expected = %s\n", buf_exp);
    } else {
        printf("FAIL: %s:%d:1: multiplication\n", __FILE__, __LINE__);
        printf("%s  FAIL: %s%s\n", C_RED, buf_name, C_RESET);
        printf("    got      = %s\n", r_buf);
        printf("    expected = %s\n", buf_exp);
        TEST_FAIL();
    }
}

static void test_div() {
    printf(C_CYAN "TEST: division\n" C_RESET);

    qfloat_t a = qf_from_string("1.2412731971809253340758239961506");
    qfloat_t b = qf_from_string("9.8765431209876543171934981073984");
    qfloat_t got = qf_div(a, b);

    char buf[64];
    char buf_exp[64] = "0.1256789123456789012345678901234567";
    qfloat_t expected = qf_from_string(buf_exp);
    qf_to_string(got, buf, sizeof(buf));

    if (qf_close(got, expected, 1e-30)) {
        printf("%s  OK: %s = %s%s\n", C_GREEN, "1.2412731971809253340758239961506 / 9.8765431209876543171934981073984", "0.1256789123456789012345678901234567", C_RESET);
        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
    } else {
        printf("FAIL: %s:%d:1: division\n", __FILE__, __LINE__);
        printf("%s  FAIL: %s%s\n", C_RED, "1.2412731971809253340758239961506 / 9.8765431209876543171934981073984", C_RESET);
        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
        TEST_FAIL();
    }
}

static void test_sqrt() {
    printf(C_CYAN "TEST: sqrt\n" C_RESET);

    qfloat_t x = qf_from_double(0.5);
    qfloat_t got = qf_sqrt(x);

    char buf[64];
    char buf_exp[64] = "0.707106781186547524400844362104849";
    qfloat_t expected = qf_from_string(buf_exp);
    qf_to_string(got, buf, sizeof(buf));

    if (qf_close(got, expected, 1e-30)) {
        printf(C_GREEN "  OK: sqrt\n" C_RESET);
        print_q("       got", got);
        printf("  expected = %s\n", buf_exp);
    }
    else {
        printf("FAIL: %s:%d:1: sqrt\n", __FILE__, __LINE__);
        printf(C_RED "  FAIL: sqrt\n" C_RESET);
        print_q("       got", got);
        printf("  expected = %s\n", buf_exp);
        TEST_FAIL();
    }
}

static void test_exp_log() {
    printf(C_CYAN "TEST: exp/log\n" C_RESET);

    qfloat_t expected = qf_from_string("1.2345678912345678912345678912346");
    qfloat_t e = qf_exp(expected);
    qfloat_t got = qf_log(e);

    char buf_exp[64];
    qf_to_string(expected, buf_exp, sizeof(buf_exp));

    if (qf_close_value(got, expected, 1e-30)) {
        printf(C_GREEN "  OK: exp/log\n" C_RESET);
        print_q("  log(exp(x))", got);
        printf("  expected    = %s\n", buf_exp);
    } else {
        printf("FAIL: %s:%d:1: exp/log\n", __FILE__, __LINE__);
        printf(C_RED "  FAIL: exp/log\n" C_RESET);
        print_q("  log(exp(x))", got);
        printf("  expected    = %s\n", buf_exp);
        TEST_FAIL();
    }
}

static void test_qf_exp(void)
{
    printf("\n== qf_exp ==\n");

    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } exp_tests[] = {

        { {1.0,0.0}, "exp(1)",
          qf_from_string("2.71828182845904523536028747135266249775724709369996") },

        { {-1.0,0.0}, "exp(-1)",
          qf_from_string("0.36787944117144232159552377016146086744581113103177") },

        { {0.125,0.0}, "exp(0.125)",
          qf_from_string("1.13314845306682631682900722781179387256550313174518") },

        { {-0.125,0.0}, "exp(-0.125)",
          qf_from_string("0.88249690258459540286489214322905073622200482499065") },

        { {0.5,0.0}, "exp(0.5)",
          qf_from_string("1.64872127070012814684865078781416357165377610071015") },

        { {-0.5,0.0}, "exp(-0.5)",
          qf_from_string("0.60653065971263342360379953499118045344191813548719") },

        { {10.0,0.0}, "exp(10)",
          qf_from_string("22026.465794806716516957900645284244366353512618556") },

        { {-10.0,0.0}, "exp(-10)",
          qf_from_string("0.00004539992976248485153559151556054979") },

        { {20.0,0.0}, "exp(20)",
          qf_from_string("485165195.409790277969106830541540558964462294471") },

        { {-20.0,0.0}, "exp(-20)",
          qf_from_string("2.0611536224385578279659403801558e-09") },

        { {40.0,0.0}, "exp(40)",
          qf_from_string("2.353852668370199854078999107490348045088716172546e+17") },

        { {-40.0,0.0}, "exp(-40)",
          qf_from_string("4.2483542552915889953292347828586580178795655542e-18") },
    };

    int N = sizeof(exp_tests)/sizeof(exp_tests[0]);
    char buf[64], buf_exp[64];

    for (int i = 0; i < N; ++i) {
        qfloat_t got = qf_exp(exp_tests[i].arg);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp_tests[i].expected, buf_exp, sizeof(buf_exp));

        if (qf_close_rel(got, exp_tests[i].expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n",
                   C_GREEN, exp_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s\n",
                   C_RED, exp_tests[i].name, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* exp(x)*exp(-x) ≈ 1 */
    qfloat_t x = qf_from_double(10.0);
    qfloat_t ex  = qf_exp(x);
    qfloat_t emx = qf_exp(qf_neg(x));
    qfloat_t prod = qf_mul(ex, emx);

    qfloat_t one = qf_from_double(1.0);
    qf_to_string(prod, buf, sizeof(buf));

    if (qf_close(prod, one, 1e-30)) {
        printf("%s  OK: exp(x)*exp(-x) = 1%s\n", C_GREEN, C_RESET);
    } else {
        printf("%s  FAIL: exp(x)*exp(-x) != 1%s\n", C_RED, C_RESET);
        printf("    got = %s\n", buf);
        TEST_FAIL();
    }
}

static void test_qf_log(void)
{
    printf("\n== qf_log ==\n");

    struct {
        qfloat_t arg;
        const char *name;
        qfloat_t expected;
    } log_tests[] = {

        { {1.0,0.0}, "log(1)",
          qf_from_string("0") },

        { {2.718281828459045091e+00, 1.445646891729250158e-16}, "log(e)",
          qf_from_string("1") },

        { {2.0,0.0}, "log(2)",
          qf_from_string("0.69314718055994530941723212145817656807550013436025") },

        { {0.5,0.0}, "log(0.5)",
          qf_from_string("-0.69314718055994530941723212145817656807550013436025") },

        { {10.0,0.0}, "log(10)",
          qf_from_string("2.30258509299404568401799145468436420760110148862877") },

        /* cancellation test */
        { qf_add(qf_from_double(1.0), qf_from_string("1e-10")), "log(1+1e-10)",
          qf_from_string("9.9999999995000000000333333333308333333335333333e-11") },
    };

    int N = sizeof(log_tests)/sizeof(log_tests[0]);
    char buf[64], buf_exp[64];

    for (int i = 0; i < N; ++i) {
        qfloat_t got = qf_log(log_tests[i].arg);
        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(log_tests[i].expected, buf_exp, sizeof(buf_exp));

        if (qf_close_value(got, log_tests[i].expected, 1e-30)) {
            printf("%s  OK: %s = %s%s\n",
                   C_GREEN, log_tests[i].name, buf, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: %s%s\n",
                   C_RED, log_tests[i].name, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* Round-trip: log(exp(x)) ≈ x */
    qfloat_t x = qf_from_double(5.0);
    qfloat_t ex = qf_exp(x);
    qfloat_t lx = qf_log(ex);

    qf_to_string(lx, buf, sizeof(buf));
    if (qf_close(lx, x, 1e-30)) {
        printf("%s  OK: log(exp(x)) = x%s\n", C_GREEN, C_RESET);
    } else {
        printf("%s  FAIL: log(exp(x)) != x%s\n", C_RED, C_RESET);
        printf("    got = %s\n", buf);
        TEST_FAIL();
    }

    /* Round-trip: exp(log(x)) ≈ x */
    qfloat_t y = qf_from_double(7.0);
    qfloat_t ly = qf_log(y);
    qfloat_t ey = qf_exp(ly);

    qf_to_string(ey, buf, sizeof(buf));
    if (qf_close(ey, y, 1e-30)) {
        printf("%s  OK: exp(log(x)) = x%s\n", C_GREEN, C_RESET);
    } else {
        printf("%s  FAIL: exp(log(x)) != x%s\n", C_RED, C_RESET);
        printf("    got = %s\n", buf);
        TEST_FAIL();
    }
}

static void test_stability() {
    printf(C_CYAN "TEST: stability (catastrophic cancellation)\n" C_RESET);

    double a = 1e16;
    double b = 1.0;

    qfloat_t qa = qf_from_double(a);
    qfloat_t qb = qf_from_double(b);

    qfloat_t r = qf_sub(qa, qb);

    double expected = a - b;

    if (approx_equal(r, expected, 1e-30)) {
        printf(C_GREEN "  OK: stable subtraction\n" C_RESET);
        print_q("  got", r);
        printf("  expected = %.32g\n", expected);
    } else {
        printf(C_RED "  FAIL: stable subtraction\n" C_RESET);
        print_q("  got", r);
        printf("  expected = %.32g\n", expected);
        TEST_FAIL();
    }
}

void test_arithmetic(void) {
    RUN_TEST(test_add, __func__);
    RUN_TEST(test_mul, __func__);
    RUN_TEST(test_div, __func__);
    RUN_TEST(test_sqrt, __func__);
    RUN_TEST(test_exp_log, __func__);
    RUN_TEST(test_qf_exp, __func__);
    RUN_TEST(test_qf_log, __func__);
    RUN_TEST(test_stability, __func__);
}

/* -----------------------------------------------------------
   String conversion tests
   ----------------------------------------------------------- */
