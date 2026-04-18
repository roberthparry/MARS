#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "qfloat.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"

/* Helper to print qfloat_t */
static void print_q(const char *label, qfloat_t x) {
    char buf[256];
    qf_to_string(x, buf, sizeof(buf));
    printf("%s = %s\n", label, buf);
}

/* Compare qfloat_t to double with tolerance */
static int approx_equal(qfloat_t a, double b, double tol) {
    double diff = fabs(qf_to_double(a) - b);
    return diff < tol;
}

static int qf_close(qfloat_t a, qfloat_t b, double rel)
{
    return qf_abs(qf_sub(a, b)).hi <= rel;
}

static int qf_close_rel(qfloat_t a, qfloat_t b, double rel)
{
    return qf_abs(qf_sub(qf_div(a,b), (qfloat_t){1,0})).hi <= rel;
}

/* -----------------------------------------------------------
   Arithmetic tests
   ----------------------------------------------------------- */

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

    if (qf_close(got, expected, 1e-28)) {
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
          qf_from_string("0.000045399929762484851535591515564965808490280944") },

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

        if (qf_close_rel(got, exp_tests[i].expected, 1e-28)) {
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

        if (qf_close(got, log_tests[i].expected, 1e-29)) {
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

/* -----------------------------------------------------------
   String conversion tests
   ----------------------------------------------------------- */


static void test_qf_to_string(void)
{
    static struct {
        const char *label;
        qfloat_t      x;
        const char *expected;
    } tests[] = {

        /* Zero */
        { "zero", { 0.0, 0.0 }, "0" },

        /* NAN */
        { "NAN", { NAN, NAN }, "NAN" },
        { "-NAN", { -NAN, -NAN }, "-NAN" },

        /* Inf */
        { "Inf", { INFINITY, INFINITY }, "INF" },
        { "-Inf", { -INFINITY, -INFINITY }, "-INF" },

        /* Simple doubles */
        { "1.0", { 1.0, 0.0 }, "1.000000000000000000000000000000000e+0" },
        { "10.0", { 10.0, 0.0 }, "1.000000000000000000000000000000000e+1" },

        /* Negative double */
        { "-pi (double only)", { -3.141592653589793, 0.0 }, "-3.141592653589793115997963468544185e+0" },

        /* Full quad-double π */
        { "pi (full qfloat_t)", { 3.14159265358979312e+00, 1.22464679914735321e-16 }, "3.141592653589793238462643383279503e+0" },
                              
        /* Tiny numbers */
        { "1e-29 (double only)", { 9.99999999999999943e-30, 5.67934258248957217e-46 }, "1.000000000000000000000000000000000e-29" },
        { "9.9999999999999999999999999999999e-30 (quad-double)", { 9.99999999999999943e-30, 5.67934258248957139e-46 },
          "9.999999999999999999999999999999900e-30" },

        /* Huge numbers */
        {"sqrt(2)*10^200", { 1.4142135623730952e+300, -4.5949334009680563e+283 }, "1.414213562373095048801688724209698e300" },
        { "1.2345678901234567890123456789012e200 (double only)", { 1.2345678901234567890123456789012e200, 0.0 }, 
          "1.234567890123456749809226093848665e+200" },
        { "1.2345678901234567890123456789012e200 (quad-double)", { 1.23456789012345675e+200, 3.92031195850516905e+183 },
          "1.234567890123456789012345678901200e+200" },
    };

    printf("\n=== TEST: qf_to_string (raw qfloat_t inputs) ===\n\n");

    const int N = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < N; i++) {

        char buf[256];
        qf_to_string(tests[i].x, buf, sizeof(buf));
        qfloat_t x = qf_from_string(buf);
        qfloat_t err = qf_abs(qf_sub(qf_div(x, tests[i].x), (qfloat_t){1,0}));

        bool ok = (strcmp(buf, tests[i].expected) == 0 || err.hi < 1e-30 || (tests[i].x.hi == x.hi && tests[i].x.lo == x.lo));

        printf("  %s\n", tests[i].label);
        printf("    input     = hi=%.17g  lo=%.17g\n", tests[i].x.hi, tests[i].x.lo);
        printf("    got       = %s\n", buf);
        printf("    expected  = %s\n", tests[i].expected);
        printf("    rel error = %.17g\n", err.hi);

        if (ok) {
            printf("    \x1b[32mOK\x1b[0m\n");
        } else {
            printf("    \x1b[31mFAIL\x1b[0m\n");
            TEST_FAIL();
        }
    }
}


static void test_qf_from_string(void)
{
    static struct {
        const char *label;
        const char *input;
        qfloat_t      expected;
    } tests[] = {

        /* Zero */
        { "zero", "0", { 0.0, 0.0 } },
        { "zero with exponent", "0e100", { 0.0, 0.0 }},

        /* Simple doubles */
        { "1.0", "1.0", { 1.0, 0.0 } },
        { "10.0", "10.0", { 10.0, 0.0 } },
        { "-2.5", "-2.5", { -2.5, 0.0 } },

        /* Full quad-double π */
        { "pi (full qfloat_t)", "3.141592653589793238462643383279503", { 3.14159265358979312e+00, 1.22464679914735321e-16 } },

        /* Tiny numbers */
        { "1e-29", "1e-29", { 9.99999999999999943e-30, 5.67934258248957217e-46 } },
        { "9.9999999999999999999999999999999e-30", "9.9999999999999999999999999999999e-30", 
          { 9.99999999999999943e-30, 5.67934258248957139e-46 } },

        /* Huge numbers */
        { "1.2345678901234567890123456789012e200", "1.2345678901234567890123456789012e200",
          { 1.23456789012345675e+200, 3.92031195850516905e+183 } },

        /* Edge-case exponents */
        { "1e308", "1e308", { 1.00000000000000001e+308, -1.09790636294404549e+291 } },
        { "1e-308", "1e-308", { 9.99999999999999909e-309, 0.00000000000000000e+00 } },
        { "-1e-308", "-1e-308", { -9.99999999999999909e-309, -0.00000000000000000e+00 } }, 
        
        /* Rounding-boundary cases */
        { "0.999999999999999999999999999999999", "0.9999999999999999999999999999999",
          { 1.00000000000000000e+00, -1.00000000000000008e-31 } },
        { "1.000000000000000000000000000000001", "1.000000000000000000000000000000001",
          { 1.00000000000000000e+00, 1.00000000000000006e-33 } },
    };

    printf("\n=== TEST: qf_from_string (raw string inputs) ===\n\n");

    const int N = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < N; i++) {
        qfloat_t x = qf_from_string(tests[i].input);
        qfloat_t err = qf_abs(qf_sub(qf_div(x, tests[i].expected), (qfloat_t){1,0}));
        bool ok = ((err.hi < 1e-30) || (x.hi == tests[i].expected.hi && x.lo == tests[i].expected.lo));

        printf("  %s\n", tests[i].label);
        printf("    input     = \"%s\"\n", tests[i].input);
        printf("    got       = hi=%.17g  lo=%.17g\n", x.hi, x.lo);
        printf("    expected  = hi=%.17g  lo=%.17g\n", tests[i].expected.hi, tests[i].expected.lo);
        printf("    rel error = %.17g\n", err.hi);

        if (ok) {
            printf("    \x1b[32mOK\x1b[0m\n");
        } else {
            printf("    \x1b[31mFAIL\x1b[0m\n");
            TEST_FAIL();
        }
    }
}

static void test_from_string_basic() {
    printf(C_CYAN "TEST: qf_from_string (basic)\n" C_RESET);

    const char *s = "3.1415926535897932384626433832795";
    qfloat_t x = qf_from_string(s);

    char buf[256];
    qf_to_string(x, buf, sizeof(buf));

    if (strncmp(buf, "3.14159265358979323846264338327", 30) == 0) {
        printf(C_GREEN "  OK: parse basic\n" C_RESET);
        printf("    input    = %s\n", s);
        printf("    got      = %s\n", buf);
    } else {
        printf(C_RED "  FAIL: parse basic  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("    input    = %s\n", s);
        printf("    got      = %s\n", buf);
        TEST_FAIL();
    }
}

static void test_from_string_scientific() {
    printf(C_CYAN "TEST: qf_from_string (scientific)\n" C_RESET);

    const char *s = "1.2345678901234567890123456789012e+20";
    qfloat_t x = qf_from_string(s);

    char buf[256];
    qf_to_string(x, buf, sizeof(buf));

    if (strncmp(buf, "1.23456789012345678901234567890", 30) == 0) {
        printf(C_GREEN "  OK: parse scientific\n" C_RESET);
        printf("    input    = %s\n", s);
        printf("    got      = %s\n", buf);
    } else {
        printf(C_RED "  FAIL: parse scientific  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("    input    = %s\n", s);
        printf("    got      = %s\n", buf);
        TEST_FAIL();
    }
}

static void test_round_trip(void)
{
    struct {
        const char *input;
    } cases[] = {
        { "3.1415926535897932384626433832795" },
        { "2.718281828459045235360287471352713e+0" },
        { "1.000000000000000000000000000000000e+0" },
        { "1.000000000000000000000000000000000e-29" },
        { "1.234567890123456789012345678901207e+40" },
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {

        qfloat_t x = qf_from_string(cases[i].input);

        char trimmed[256];
        memset(trimmed, 0, sizeof(trimmed));
        qf_to_string(x, trimmed, sizeof(trimmed));

        qfloat_t y = qf_from_string(trimmed);

        // ---- numeric closeness check ----

        // diff = |x - y|
        qfloat_t diff = qf_abs(qf_sub(x, y));

        // tolerance: choose something smaller than your precision
        // ~1e-33 works well for your ~36-digit qfloat_t
        qfloat_t eps = qf_from_string("1e-31");

        if (qf_lt(diff, eps)) {
            printf(C_GREEN "  OK: round-trip\n" C_RESET);
            printf("    input    = %s\n", cases[i].input);
            printf("    got      = %s\n", trimmed);
        } else {
            printf(C_RED "  FAIL: round-trip  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
            printf("    input    = %s\n", cases[i].input);
            printf("    got      = %s\n", trimmed);
            TEST_FAIL();
        }
    }
}

/* -----------------------------------------------------------
   qf_sprintf tests
   ----------------------------------------------------------- */

static void test_qd_sprintf_basic(void)
{
    printf(C_CYAN "TEST: qf_sprintf (basic)\n" C_RESET);

    qfloat_t x = qf_from_string("3.1415926535897932384626433832795");

    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%Q", x);

    char expected[256];
    qf_to_string(x, expected, sizeof(expected));

    /* %Q uses uppercase E */
    char *e = strchr(expected, 'e');
    if (e) *e = 'E';

    if (strcmp(buf, expected) == 0) {
        printf(C_GREEN "  OK: basic %%Q\n" C_RESET);
        printf("  got      = %s\n", buf);
        printf("  expected = %s\n", expected);
    } else {
        printf(C_RED "  FAIL: basic %%Q  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("  got      = %s\n", buf);
        printf("  expected = %s\n", expected);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_multiple(void)
{
    printf(C_CYAN "TEST: qf_sprintf (multiple %%Q)\n" C_RESET);

    qfloat_t a = qf_from_string("1.2345678901234567890123456789012");
    qfloat_t b = qf_from_string("9.8765432109876543210987654321098");

    char buf[256];
    qf_sprintf(buf, sizeof(buf), "A=%Q B=%Q", a, b);

    char ea[64], eb[64];
    qf_to_string(a, ea, sizeof(ea));
    qf_to_string(b, eb, sizeof(eb));

    /* %Q uses uppercase E */
    char *e1 = strchr(ea, 'e');
    if (e1) *e1 = 'E';
    char *e2 = strchr(eb, 'e');
    if (e2) *e2 = 'E';

    char expected[256];
    snprintf(expected, sizeof(expected), "A=%s B=%s", ea, eb);

    if (strcmp(buf, expected) == 0) {
        printf(C_GREEN "  OK: multiple %%Q\n" C_RESET);
        printf("  got      = %s\n", buf);
        printf("  expected = %s\n", expected);
    } else {
        printf(C_RED "  FAIL: multiple %%Q  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("  got      = %s\n", buf);
        printf("  expected = %s\n", expected);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_mixed(void)
{
    printf(C_CYAN "TEST: qf_sprintf (mixed specifiers)\n" C_RESET);

    qfloat_t x = qf_from_string("2.5");

    char buf[256];
    qf_sprintf(buf, sizeof(buf), "x=%Q int=%d str=%s", x, 42, "hello");

    char expected[256];
    snprintf(expected, sizeof(expected), "x=2.500000000000000000000000000000000E+0 int=42 str=hello");

    if (strcmp(buf, expected) == 0) {
        printf(C_GREEN "  OK: mixed specifiers\n" C_RESET);
        printf("  got      = %s\n", buf);
        printf("  expected = %s\n", expected);
    } else {
        printf(C_RED "  FAIL: mixed specifiers  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("  got      = %s\n", buf);
        printf("  expected = %s\n", expected);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_buffer_limit(void)
{
    printf(C_CYAN "TEST: qf_sprintf (buffer limit)\n" C_RESET);

    qfloat_t x = qf_from_string("1.2345678901234567890123456789012");

    char buf[16];
    qf_sprintf(buf, sizeof(buf), "%Q", x);

    if (buf[15] == '\0') {
        printf(C_GREEN "  OK: buffer limit (null-terminated)\n" C_RESET);
        printf("  buf = \"%s\"\n", buf);
    } else {
        printf(C_RED "  FAIL: buffer limit (missing terminator)  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_edge_cases(void)
{
    printf(C_CYAN "TEST: qf_sprintf (edge cases)\n" C_RESET);

    struct { char *input; double tolerance;} tests[] = {
        { "0", 1e-31 },
        { "-3.1415926535897932384626433832795", 2e-31 },
        { "9.9999999999999999999999999999999E-30", 1e-31 },
        { "1.2345678901234567890123456789012E+200", 7e-31 },
    };

    for (int i = 0; i < 4; i++) {
        qfloat_t x = qf_from_string(tests[i].input);

        char buf[128];
        qf_sprintf(buf, sizeof(buf), "%q", x);

        /* Parse the printed value back into a qfloat_t */
        qfloat_t y = qf_from_string(buf);

        printf("  input     = %s\n", tests[i].input);
        printf("  got       = %s\n", buf);

        /* For debugging, show the reparsed value */
        char reparsed[128];
        qf_to_string(y, reparsed, sizeof(reparsed));
        printf("  reparsed  = %s\n", reparsed);
        qfloat_t err = qf_abs(qf_sub(qf_div(x, y), (qfloat_t){1,0}));
        printf("  rel error = %.17g\n", err.hi);

        if (qf_close_rel(x, y, tests[i].tolerance || qf_close(x, y, tests[i].tolerance)) || qf_eq(x, y)) {
            printf(C_GREEN "    OK\n" C_RESET);
        } else {
            printf(C_RED "    FAIL  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
            TEST_FAIL();
        }
    }
}

static void test_qd_sprintf_q_precision(void)
{
    printf(C_CYAN "TEST: qf_sprintf %%.Nq (explicit precision)\n" C_RESET);

    qfloat_t x = qf_from_string("3.1415926535897932384626433832795");

    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%.10q", x);

    const char *expected = "3.1415926536";

    if (strcmp(buf, expected) == 0) {
        printf(C_GREEN "  OK: precision %%.10q\n" C_RESET);
        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", expected);
    } else {
        printf(C_RED "  FAIL: precision %%.10q  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", expected);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_q_zero_precision(void)
{
    printf(C_CYAN "TEST: qf_sprintf %%.0q (zero precision)\n" C_RESET);

    qfloat_t x = qf_from_string("3.1415926535897932384626433832795");

    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%.0q", x);

    const char *expected = "3";

    if (strcmp(buf, expected) == 0) {
        printf(C_GREEN "  OK: %%.0q\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: %%.0q  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("  got      = %s\n", buf);
        printf("  expected = %s\n", expected);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_q_flags(void)
{
    printf(C_CYAN "TEST: qf_sprintf %%q flags (+, space, #)\n" C_RESET);

    qfloat_t x = qf_from_string("3");

    char buf[256];

    qf_sprintf(buf, sizeof(buf), "%+q", x);
    if (strcmp(buf, "+3") == 0) {
        printf(C_GREEN "  OK: + flag\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: + flag (got %s)  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }

    qf_sprintf(buf, sizeof(buf), "% q", x);
    if (strcmp(buf, " 3") == 0) {
        printf(C_GREEN "  OK: space flag\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: space flag (got %s)  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }

    qf_sprintf(buf, sizeof(buf), "%#q", x);
    if (strcmp(buf, "3.") == 0) {
        printf(C_GREEN "  OK: # flag\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: # flag (got %s)  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_q_width(void)
{
    printf(C_CYAN "TEST: qf_sprintf %%q width and padding\n" C_RESET);

    qfloat_t x = qf_from_string("3.14159");

    char buf[256];

    qf_sprintf(buf, sizeof(buf), "%10.5q", x);
    if (strcmp(buf, "   3.14159") == 0) {
        printf(C_GREEN "  OK: width right-align\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: width right-align (got '%s')  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }

    qf_sprintf(buf, sizeof(buf), "%-10.5q", x);
    if (strcmp(buf, "3.14159   ") == 0) {
        printf(C_GREEN "  OK: width left-align\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: width left-align (got '%s')  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }

    qf_sprintf(buf, sizeof(buf), "%010.5q", x);
    if (strcmp(buf, "0003.14159") == 0) {
        printf(C_GREEN "  OK: zero padding\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: zero padding (got '%s')  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_q_fallback(void)
{
    printf(C_CYAN "TEST: qf_sprintf %%q fallback to scientific\n" C_RESET);

    qfloat_t x = qf_from_string("1.2345678901234567890123456789012e+200");

    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%q", x);

    if (strstr(buf, "e+200")) {
        printf(C_GREEN "  OK: fallback to scientific\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: fallback to scientific (got %s)  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }
}

static void test_qd_sprintf_q_fallback_width(void)
{
    printf(C_CYAN "TEST: qf_sprintf %%q fallback preserves width\n" C_RESET);

    qfloat_t x = qf_from_string("1.234e+200");

    char buf[256];
    qf_sprintf(buf, sizeof(buf), "%40q", x);

    int leading_spaces = 0;
    while (buf[leading_spaces] == ' ') leading_spaces++;

    if (strstr(buf, "e+200") && strlen(buf) >= 40) {
        printf(C_GREEN "  OK: fallback width preserved\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: fallback width preserved (got '%s')  [%s:%d]\n" C_RESET, buf, __FILE__, __LINE__);
        TEST_FAIL();
    }
}

static void test_qf_sprintf_q_concise(void)
{
    printf(C_CYAN "TEST: qf_sprintf %%q (concise fixed-format)\n" C_RESET);

    struct { char *input; char *expected; double tolerance; } tests[] = {

        /* --- integers --- */
        { "0",                     "0",          1e-31 },
        { "1",                     "1",          1e-31 },
        { "-1",                    "-1",         1e-31 },
        { "42",                    "42",         1e-31 },
        { "-42",                   "-42",        1e-31 },

        /* --- simple fractions --- */
        { "0.5",                   "0.5",        1e-31 },
        { "-0.5",                  "-0.5",       1e-31 },
        { "1.5",                   "1.5",        1e-31 },
        { "2.25",                  "2.25",       1e-31 },
        { "-2.25",                 "-2.25",      1e-31 },

        /* --- trimming trailing zeros --- */
        { "1.2500",                "1.25",       1e-31 },
        { "3.140000",              "3.14",       1e-31 },
        { "-10.000",               "-10",        1e-31 },

        /* --- leading zeros after decimal --- */
        { "0.000123",              "0.000123",   1e-31 },
        { "-0.000123",             "-0.000123",  1e-31 },

        /* --- fixed-format exponent boundary (still fixed) --- */
        { "1e32",                  "100000000000000000000000000000000", 1e-31 },
        { "1e-6",                  "0.000001",   1e-31 },

        /* --- scientific fallback (outside fixed window) --- */
        { "1e33",                  "1e+33",      1e-31 },
        { "1e-7",                  "1e-7",       1e-31 },

        /* --- reconstruction tests --- */
        { "9.999999999999999e+1",  "99.99999999999999", 1e-31 },
        { "1.23456789e+3",         "1234.56789",        1e-31 },
        { "1.23456789e-3",         "0.00123456789",     1e-31 },
    };

    int N = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < N; i++) {

        qfloat_t x = qf_from_string(tests[i].input);

        char buf[256];
        qf_sprintf(buf, sizeof(buf), "%q", x);

        /* Parse the printed value back into a qfloat_t */
        qfloat_t y = qf_from_string(buf);

        printf("  input     = %s\n", tests[i].input);
        printf("  expected  = %s\n", tests[i].expected);
        printf("  got       = %s\n", buf);

        /* reparsed value */
        char reparsed[256];
        qf_to_string(y, reparsed, sizeof(reparsed));
        printf("  reparsed  = %s\n", reparsed);

        qfloat_t err = qf_abs(qf_sub(qf_div(x, y), (qfloat_t){1,0}));
        printf("  rel error = %.17g\n", err.hi);

        if (strcmp(buf, tests[i].expected) == 0 &&
            (qf_close_rel(x, y, tests[i].tolerance) ||
             qf_close(x, y, tests[i].tolerance) ||
             qf_eq(x, y)))
        {
            printf(C_GREEN "    OK\n" C_RESET);
        } else {
            printf(C_RED "    FAIL  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
            TEST_FAIL();
        }

        printf("\n");
    }
}

static void test_qf_sprintf_null_safe_new(void)
{
    printf(C_CYAN "TEST: qf_sprintf (NULL‑safe sizing, new)\n" C_RESET);

    qfloat_t x = qf_from_string("3.1415926535897932384626433832795");

    /* First, get the canonical %Q string via qf_to_string + 'E' */
    char core[128];
    qf_to_string(x, core, sizeof(core));   /* produces ...e+0 */

    char expected[160];
    strcpy(expected, "x=");
    strcat(expected, core);
    char *e = strchr(expected, 'e');
    if (e) *e = 'E';

    int needed = qf_sprintf(NULL, 0, "x=%Q", x);

    char *buf = malloc((size_t)needed + 1);
    int written = qf_sprintf(buf, (size_t)needed + 1, "x=%Q", x);

    if (written == needed && strcmp(buf, expected) == 0) {
        printf(C_GREEN "  OK: NULL‑safe sizing\n" C_RESET);
    } else {
        printf(C_RED "  FAIL: NULL‑safe sizing  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("written = %d, needed = %d\n", written, needed);
        printf("     got = %s\n", buf);
        printf("expected = %s\n", expected);
        TEST_FAIL();
    }

    free(buf);
}

static void test_qf_sprintf_two_pass_new(void)
{
    printf(C_CYAN "TEST: qf_sprintf (two‑pass correctness, new)\n" C_RESET);

    qfloat_t x = qf_from_string("9.9999999999999999999999999999999");

    /* Canonical expected string via qf_to_string + 'E' */
    char core[128];
    qf_to_string(x, core, sizeof(core));

    char expected[160];
    strcpy(expected, "x=");
    strcat(expected, core);
    char *e = strchr(expected, 'e');
    if (e) *e = 'E';

    int needed = qf_sprintf(NULL, 0, "x=%Q", x);

    char *buf = malloc((size_t)needed + 1);
    int written = qf_sprintf(buf, (size_t)needed + 1, "x=%Q", x);

    if (written == needed && strcmp(buf, expected) == 0) {
        printf(C_GREEN "  OK: two‑pass\n" C_RESET);
    } else {
        printf(C_RED   "  FAIL: two‑pass  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("written = %d, needed = %d\n", written, needed);
        printf("     got = %s\n", buf);
        printf("expected = %s\n", expected);
        TEST_FAIL();
    }

    free(buf);
}

static void test_qf_printf_stdout(void)
{
    printf(C_CYAN "TEST: qf_printf (stdout)\n" C_RESET);

    qfloat_t x = qf_from_double(1.5);

    /* Save original stdout */
    int saved_stdout = dup(fileno(stdout));
    if (saved_stdout < 0) {
        printf(C_RED "  FAIL: could not save stdout\n" C_RESET);
        TEST_FAIL();
        return;
    }

    /* Redirect stdout to a file */
    fflush(stdout);
    if (!freopen("test_output.txt", "w", stdout)) {
        printf(C_RED "  FAIL: could not redirect stdout\n" C_RESET);
        TEST_FAIL();
        return;
    }

    int n = qf_printf("value=%Q\n", x);
    fflush(stdout);

    /* Restore stdout */
    dup2(saved_stdout, fileno(stdout));
    close(saved_stdout);

    /* Read back the output */
    FILE *in = fopen("test_output.txt", "r");
    if (!in) {
        printf(C_RED "  FAIL: could not open test_output.txt  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        TEST_FAIL();
        return;
    }

    char buf[256];
    char *p = fgets(buf, sizeof(buf), in);
    fclose(in);

    const char *expected =
        "value=1.500000000000000000000000000000000E+0\n";

    if (p && strcmp(buf, expected) == 0 && n == (int)strlen(expected)) {
        printf(C_GREEN "  OK: qf_printf\n" C_RESET);
        printf("    expected = %s", expected);
        printf("    got      = %s", buf);
        printf("    n        = %d\n", n);
    } else {
        printf(C_RED "  FAIL: qf_printf  [%s:%d]\n" C_RESET, __FILE__, __LINE__);
        printf("    expected = %s", expected);
        printf("    got      = %s", buf);
        printf("    n        = %d\n", n);
        TEST_FAIL();
    }
}

void test_qf_sprintf_and_printf(void)
{
    /* Original tests */
    RUN_TEST(test_qd_sprintf_basic, __func__);
    RUN_TEST(test_qd_sprintf_multiple, __func__);
    RUN_TEST(test_qd_sprintf_mixed, __func__);
    RUN_TEST(test_qd_sprintf_buffer_limit, __func__);
    RUN_TEST(test_qd_sprintf_edge_cases, __func__);

    RUN_TEST(test_qd_sprintf_q_precision, __func__);
    RUN_TEST(test_qd_sprintf_q_zero_precision, __func__);
    RUN_TEST(test_qd_sprintf_q_flags, __func__);
    RUN_TEST(test_qd_sprintf_q_width, __func__);
    RUN_TEST(test_qd_sprintf_q_fallback, __func__);
    RUN_TEST(test_qd_sprintf_q_fallback_width, __func__);
    RUN_TEST(test_qf_sprintf_q_concise, __func__);

    /* New tests */
    RUN_TEST(test_qf_sprintf_null_safe_new, __func__);
    RUN_TEST(test_qf_sprintf_two_pass_new, __func__);
    RUN_TEST(test_qf_printf_stdout, __func__);
}

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
static void test_qf_hypot(void)
{
    printf(C_CYAN "TEST: qf_hypot\n" C_RESET);

    char buf[128], buf_ref[128];

    /* 1) Full-precision qfloat_t reference (safe magnitudes) */
    struct {
        const char *xs, *ys, *label;
    } precise[] = {
        {"1", "0", "hypot(1,0)"},
        {"0", "1", "hypot(0,1)"},
        {"3", "4", "hypot(3,4)"},
        {"-3", "4", "hypot(-3,4)"},
        {"3", "-4", "hypot(3,-4)"},
        {"-3", "-4", "hypot(-3,-4)"},

        {"1", "1e-30", "hypot(1,1e-30)"},
        {"1e-30", "1", "hypot(1e-30,1)"},

        {"1e100", "3e99", "hypot(1e100,3e99)"},
        {"5e-100", "7e-100", "hypot(5e-100,7e-100)"},
    };

    int Np = (int)(sizeof(precise)/sizeof(precise[0]));

    for (int i = 0; i < Np; i++) {
        qfloat_t x = qf_from_string(precise[i].xs);
        qfloat_t y = qf_from_string(precise[i].ys);

        qfloat_t got = qf_hypot(x, y);

        /* direct qfloat_t reference: sqrt(x*x + y*y) */
        qfloat_t xx  = qf_mul(x, x);
        qfloat_t yy  = qf_mul(y, y);
        qfloat_t sum = qf_add(xx, yy);
        qfloat_t ref = qf_sqrt(sum);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(ref, buf_ref, sizeof(buf_ref));

        if (qf_close_rel(got, ref, 1e-31)) {
            printf("%s  OK: %s%s\n", C_GREEN, precise[i].label, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_ref);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, precise[i].label, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_ref);
            TEST_FAIL();
        }
    }

    /* symmetry (full precision) */
    {
        qfloat_t a = qf_from_double(1.2345);
        qfloat_t b = qf_from_double(9.8765);

        qfloat_t h1 = qf_hypot(a, b);
        qfloat_t h2 = qf_hypot(b, a);

        if (qf_close(h1, h2, 1e-32)) {
            printf("%s  OK: symmetry%s\n", C_GREEN, C_RESET);
        } else {
            printf("%s  FAIL: symmetry%s  [%s:%d]\n", C_RED, C_RESET, __FILE__, __LINE__);
            TEST_FAIL();
        }
    }

    /* Extreme magnitudes: test qf_hypot via double projection only */
    struct {
        double x, y;
        const char *label;
    } extreme[] = {
        {1e300, 1e300,   "hypot(1e300,1e300)"},
        {1e300, 1e-300,  "hypot(1e300,1e-300)"},
        {1e200, 1e200,   "hypot(1e200,1e200)"},
        {1e200, 1e-200,  "hypot(1e200,1e-200)"},
    };

    int Ne = (int)(sizeof(extreme)/sizeof(extreme[0]));

    for (int i = 0; i < Ne; i++) {
        double xd = extreme[i].x;
        double yd = extreme[i].y;

        qfloat_t x = qf_from_double(xd);
        qfloat_t y = qf_from_double(yd);

        qfloat_t hq = qf_hypot(x, y);
        double  hq_d = qf_to_double(hq);
        double  href = hypot(xd, yd);
        double  err  = hq_d - href;

        qf_to_string(hq, buf, sizeof(buf));

        if (fabs(err) <= 1e-15 * fabs(href)) {
            printf("%s  OK: %s%s\n", C_GREEN, extreme[i].label, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    double   = %.17g\n", href);
            printf("    err      = %.3e\n", err);
        } else {
            printf("%s  FAIL: %s%s  [%s:%d]\n", C_RED, extreme[i].label, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    double   = %.17g\n", href);
            printf("    err      = %.3e\n", err);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_gamma(void)
{
    printf(C_CYAN "TEST: qf_gamma\n" C_RESET);

    char buf[256], buf_exp[256];

    /* ------------------------------------------------------------
       1. High-precision expected values (fill these in yourself)
       ------------------------------------------------------------ */
    struct {
        const char *xs;      /* input x */
        const char *expected;/* expected gamma(x) as qfloat_t string */
        double tol;
    } tests[] = {

        /* Positive integers */
        { "1",   "1", 1e-28 },
        { "2",   "1", 1e-28 },
        { "3",   "2", 1e-28 },
        { "4",   "6", 1e-28 },
        { "5",   "24", 1e-28 },

        /* Half-integers */
        { "0.5",  "1.7724538509055160272981674833411451827975494561", 1e-28 },
        { "1.5",  "0.8862269254527580136490837416705725913987747281", 1e-28 },
        { "2.5",  "1.3293403881791370204736256125058588870981620921", 1e-28 },
        { "3.5",  "3.3233509704478425511840640312646472177454052302", 1e-28 },

        /* Positive non-integers */
        { "1.1",  "0.9513507698668731836292487177265402192550578626", 1e-28 },
        { "1.7",  "0.9086387328532904499768198254069681324488988194", 1e-28 },
        { "2.3",  "1.1667119051981603450418814412029179385339943497", 1e-28 },
        { "3.8",  "4.6941742057404232025016460230984254507483898522", 1e-28 },
        { "4.2",  "7.7566895357931776386947595830098952250022722531", 1e-28 },
        { "7.1",  "868.95685880064040628832030517751832751591170159", 1e-27 },
        { "9.9",  "289867.70384010940678398620753134453492316044242", 1e-22 },

        /* Negative non-integers */
        { "-0.3", "-4.326851108825192618937237263842705392613803902", 1e-28 },
        { "-0.7", "-4.273669982410843754732166451292739701589722893", 1e-28 },
        { "-1.2", "4.8509571405220973901513372427852445547581740370", 1e-28 },
        { "-2.3", "-1.447107394255917263858607780549399796860803981", 1e-28 },
        { "-3.8", "0.2996321345028458550807199167142564747437676221", 1e-28 },

        /* End marker */
        { NULL, NULL, 0.0 },
    };

    /* ------------------------------------------------------------
       2. Compare qf_gamma(x) against expected
       ------------------------------------------------------------ */
    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_gamma(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close(got, exp, tests[i].tol)) {
            printf("%s  OK: gamma(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: gamma(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    /* ------------------------------------------------------------
       3. Poles (should be NaN)
       ------------------------------------------------------------ */
    const char *poles[] = { "0", "-1", "-2", "-3", "-4" };
    for (size_t i = 0; i < sizeof(poles)/sizeof(poles[0]); i++) {
        qfloat_t x = qf_from_string(poles[i]);
        qfloat_t g = qf_gamma(x);

        if (isnan(g.hi)) {
            printf("%s  OK: gamma(%s) is NaN%s\n", C_GREEN, poles[i], C_RESET);
        } else {
            qf_to_string(g, buf, sizeof(buf));
            printf("%s  FAIL: gamma(%s) should be NaN%s  [%s:%d]\n", C_RED, poles[i], C_RESET, __FILE__, __LINE__);
            printf("    got = %s\n", buf);
            TEST_FAIL();
        }
    }

    /* ------------------------------------------------------------
       4. Extreme magnitudes (should be NaN)
       ------------------------------------------------------------ */
    const char *extreme[] = { "1e300", "-1e300" };
    for (size_t i = 0; i < sizeof(extreme)/sizeof(extreme[0]); i++) {
        qfloat_t x = qf_from_string(extreme[i]);
        qfloat_t g = qf_gamma(x);

        if (isnan(g.hi)) {
            printf("%s  OK: gamma(%s) extreme -> NaN%s\n",
                   C_GREEN, extreme[i], C_RESET);
        } else {
            qf_to_string(g, buf, sizeof(buf));
            printf("%s  FAIL: gamma(%s) should be NaN%s  [%s:%d]\n", C_RED, extreme[i], C_RESET, __FILE__, __LINE__);
            printf("    got = %s\n", buf);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_erf(void)
{
    printf(C_CYAN "TEST: qf_erf\n" C_RESET);

    char buf[256], buf_exp[256];

    /* ------------------------------------------------------------
       1. Expected values (fill these in yourself)
       ------------------------------------------------------------ */
    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erf(x) as qfloat_t string */
    } tests[] = {

        /* Basic values */
        { "0",     "0" },
        { "0.5",   "0.5204998778130465376827466538919645287364515758" },
        { "1.0",   "0.8427007929497148693412206350826092592960669980" },
        { "1.2",   "0.9103139782296353802384057757153736772278970560" },
        { "1.5",   "0.9661051464753107270669762616459478586814104793" },
        { "2.0",   "0.9953222650189527341620692563672529286108917970" },
        { "2.5",   "0.9995930479825550410604357842600250872796513226" },
        { "3.0",   "0.9999779095030014145586272238704176796201522929" },

        /* Negative values */
        { "-0.5",  "-0.520499877813046537682746653891964528736451576" },
        { "-1.0",  "-0.842700792949714869341220635082609259296066998" },
        { "-1.7",  "-0.983790458590774563626242588121881213432720496" },
        { "-2.3",  "-0.998856823402643348534652540619230859805851309" },
        { "-3.0",  "-0.999977909503001414558627223870417679620152293" },

        /* Values near the cutoff (1.5) */
        { "1.49",  "0.9648978648432042121029072375635501433975444954" },
        { "1.51",  "0.9672767481287116359138259912486972279263387115" },

        /* Large but safe values */
        { "5.0",   "0.9999999999984625402055719651498116565146166211" },
        { "-5.0",  "-0.999999999998462540205571965149811656514616621" },

        /* extreme values */
        { "26.0",   "1" },
        { "30.0",   "1" },
        { "-30.0",  "-1" },
        { "1e300",  "1" },
        { "-1e300",  "-1" },

        /* End marker */
        { NULL, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erf(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close(got, exp, 1e-29)) {
            printf("%s  OK: erf(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: erf(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_erfc(void)
{
    printf(C_CYAN "TEST: qf_erfc\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erfc(x) as qfloat_t string */
    } tests[] = {

        /* Basic values */
        { "0",     "1" },
        { "0.5",   "0.47950012218695346231725334610803547" },
        { "1.0",   "0.15729920705028513065877936491739074" },
        { "1.2",   "0.089686021770364619761594224284626322" },
        { "1.5",   "0.0338948535246892729330237383540521413" },
        { "2.0",   "0.00467773498104726583793074363274707139" },
        { "2.5",   "0.0004069520174449589395642157399749127203" },
        { "3.0",   "0.00002209049699858544137277612958232037985" },

        /* Negative values */
        { "-0.5",  "1.52049987781304653768274665389196453" },
        { "-1.0",  "1.84270079294971486934122063508260926" },
        { "-1.7",  "1.98379045859077456362624258812188121" },
        { "-2.3",  "1.99885682340264334853465254061923086" },
        { "-3.0",  "1.99997790950300141455862722387041768" },

        /* Values near the cutoff (1.5) */
        { "1.49",  "0.0351021351567957878970927624364498566" },
        { "1.51",  "0.0327232518712883640861740087513027720" },

        /* Larger but safe values */
        { "5.0",   "1.53745979442803485018834348538337889e-12" },
        { "-5.0",  "1.99999999999846254020557196514981166" },

        /* extreme values */
        { "26.0",   "5.6631924088561428464757278969260925e-296" },
        { "30.0",   "0" },
        { "-30.0",  "2" },
        { "1e300",  "0" },
        { "-1e300",  "2" },

        /* End marker */
        { NULL, NULL }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erfc(x);
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        
        if (qf_close(got, exp, 1e-29)) {
            printf("%s  OK: erfc(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: erfc(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_erfinv(void) {
    printf(C_CYAN "TEST: qf_erfinv\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erfc(x) as qfloat_t string */
        double acceptable_error;
    } tests[] = {

        /* Basic values */
        { "-0.9",  "-1.163087153676674086726254260562948", 1e-29 },
        { "-0.87", "-1.070631712142947435214622665410316", 1e-29 },
        { "-0.85", "-1.017902464832027643608729928245034", 1e-29 },
        { "-0.8",  "-0.9061938024368232200711627030956629", 1e-29 },
        { "-0.7",  "-0.7328690779592168522188174610580155", 1e-29 },
        { "-0.3",  "-0.2724627147267543556219575985875658", 1e-29 },
        { "-0.1",  "-0.08885599049425768701573725056779177", 1e-29 },
        { "0",     "0", 1e-29 },
        { "0.1",   "0.08885599049425768701573725056779177", 1e-29 },
        { "0.3",   "0.2724627147267543556219575985875658", 1e-29 },
        { "0.7",   "0.7328690779592168522188174610580155", 1e-29 },
        { "0.8",   "0.9061938024368232200711627030956629", 1e-29 },
        { "0.85",  "1.017902464832027643608729928245034", 1e-29 },
        { "0.87",  "1.070631712142947435214622665410316", 1e-29 },
        { "0.9",   "1.163087153676674086726254260562948", 1e-29 },

        /* Values near the cutoff */
        { "-0.9999999999",  "-4.572824967389485278741043673140672", 1e-22 },
        { "-0.99999999",  "-4.0522372438713892052307738897049963634", 1e-24 },
        { "-0.9999",  "-2.75106390571206079614551316854267817", 1e-28 },
        { "0.9999",  "2.75106390571206079614551316854267817", 1e-28 },
        { "0.99999999",  "4.0522372438713892052307738897049963634", 1e-24 },
        { "0.9999999999",  "4.572824967389485278741043673140672", 1e-22 },

        /* extreme values */
        { "1",  "NAN", 0.0 },
        { "-1",   "NAN", 0.0 },

        /* End marker */
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erfinv(x);
        if (strcmp(tests[i].expected, "NAN") == 0) {
            if (qf_isnan(got)) {
                printf("%s  OK: erfinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            } else {
                printf("%s  FAIL: erfinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    expected = NAN\n");
                TEST_FAIL();
            }
            continue;
        }
        else {
            qfloat_t exp = qf_from_string(tests[i].expected);

            qf_to_string(got, buf, sizeof(buf));
            qf_to_string(exp, buf_exp, sizeof(buf_exp));

            if (qf_close(got, exp, tests[i].acceptable_error)) {
                printf("%s  OK: erfinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
                printf("    got      = %s\n", buf);
                printf("    expected = %s\n", buf_exp);
            } else {
                printf("%s  FAIL: erfinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    got      = %s\n", buf);
                printf("    expected = %s\n", buf_exp);
                TEST_FAIL();
            }
        }
    }

    printf("\n");
}

static void test_qf_erfcinv(void) {
    printf(C_CYAN "TEST: qf_erfcinv\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;       /* input x */
        const char *expected; /* expected erfcinv(x) as qfloat_t string */
        double acceptable_error;
    } tests[] = {

        /* Basic values */
        { "0.1",   "1.163087153676674086726254260562948", 1e-29 },
        { "0.13",  "1.070631712142947435214622665410316", 1e-29 },
        { "0.15",  "1.017902464832027643608729928245034", 1e-29 },
        { "0.2",   "0.9061938024368232200711627030956629", 1e-29 },
        { "0.3",   "0.7328690779592168522188174610580155", 1e-29 },
        { "0.7",   "0.2724627147267543556219575985875658", 1e-29 },
        { "0.9",   "0.08885599049425768701573725056779177", 1e-29 },
        { "1",     "0", 1e-29 },
        { "1.1",   "-0.08885599049425768701573725056779177", 1e-29 },
        { "1.3",   "-0.2724627147267543556219575985875658", 1e-29 },
        { "1.7",   "-0.7328690779592168522188174610580155", 1e-29 },
        { "1.8",   "-0.9061938024368232200711627030956629", 1e-29 },
        { "1.85",  "-1.017902464832027643608729928245034", 1e-29 },
        { "1.87",  "-1.070631712142947435214622665410316", 1e-29 },
        { "1.9",   "-1.163087153676674086726254260562948", 1e-29 },

        /* Values near the cutoff */
        { "2e-10", "4.4981472895292597414585341748556", 1e-29 },
        { "2e-8",  "3.9682841357833348003564647370728", 1e-29 },
        { "2e-4",  "2.6297417762102729206203505927957", 1e-29 },
        { "1.9998", "-2.6297417762102729206203505927277", 1e-28 },
        { "1.99999998", "-3.9682841357833348003564636926937", 1e-23 },
        { "1.9999999998", "-4.4981472895292597414584255650214", 1e-21 },

        /* extreme values */
        { "0",  "POSINF", 0.0 },
        { "2",  "NEGINF", 0.0 },

        /* End marker */
        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_erfcinv(x);

        /* Handle infinities */
        if (strcmp(tests[i].expected, "POSINF") == 0) {
            if (qf_isposinf(got)) {
                printf("%s  OK: erfcinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            } else {
                printf("%s  FAIL: erfcinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    expected = +INF\n");
                TEST_FAIL();
            }
            continue;
        }

        if (strcmp(tests[i].expected, "NEGINF") == 0) {
            if (qf_isneginf(got)) {
                printf("%s  OK: erfcinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            } else {
                printf("%s  FAIL: erfcinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
                printf("    expected = -INF\n");
                TEST_FAIL();
            }
            continue;
        }

        /* Normal numeric case */
        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close(got, exp, tests[i].acceptable_error)) {
            printf("%s  OK: erfcinv(%s)%s\n", C_GREEN, tests[i].xs, C_RESET);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
        } else {
            printf("%s  FAIL: erfcinv(%s)%s  [%s:%d]\n", C_RED, tests[i].xs, C_RESET, __FILE__, __LINE__);
            printf("    got      = %s\n", buf);
            printf("    expected = %s\n", buf_exp);
            TEST_FAIL();
        }
    }

    printf("\n");
}

static void test_qf_lgamma(void) {
    printf(C_CYAN "TEST: qf_lgamma\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;
        const char *expected;
        double acceptable_error;
    } tests[] = {

        /* Basic values */
        { "0.5", "0.572364942924700087071713675676529",  1e-30 },
        { "1",    "0",                                   1e-32 },
        { "2",    "0",                                   1e-32 },
        { "3",    "0.693147180559945309417232121458176", 1e-30 },
        { "4",    "1.79175946922805500081247735838070",  1e-30 },

        /* Non-integers */
        { "1.5",  "-0.120782237635245222345518445781647", 1e-30 },
        { "2.5",   "0.284682870472919159632494669682701", 1e-30 },

        /* Larger x */
        { "4.5",   "2.4537365708424422205041425034357162", 1e-28 },
        { "5.5",   "3.957813967618716293877400855822591", 1e-28 },
        { "6.5",   "5.6625620598571415285221123123295437", 1e-28 },
        { "7.5",   "7.5343642367587329551583676324366858", 1e-28 },
        { "8.0",   "8.5251613610654143001655310363471251", 1e-28 },
        { "9.0",  "10.6046029027452502284172274007216548", 1e-28 },
        { "10",   "12.8018274800814696112077178745667", 1e-28 },
        { "12",   "17.5023078458738858392876529072162", 1e-28 },
        { "14",   "22.5521638531234228855708498286204", 1e-28 },
        { "16",   "27.899271383840891566089439263670467", 1e-28 },
        { "18",   "33.505073450136888884007902367376300", 1e-28 },
        { "20",   "39.3398841871994940362246523945670", 1e-28 },

        /* Poles */
        { "0",    "NAN", 0.0 },
        { "-1",   "NAN", 0.0 },
        { "-2",   "NAN", 0.0 },

        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_lgamma(x);

        if (strcmp(tests[i].expected, "NAN") == 0) {
            if (qf_isnan(got)) {
                printf(C_GREEN "  OK: lgamma(%s)\n" C_RESET, tests[i].xs);
            } else {
                printf(C_RED "  FAIL: lgamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
                printf("    expected = NAN\n");
                qf_to_string(got, buf, sizeof(buf));
                printf("    got      = %s\n", buf);
                TEST_FAIL();
            }
            continue;
        }

        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));
        
        if (qf_close(got, exp, tests[i].acceptable_error)) {
            printf(C_GREEN "  OK: lgamma(%s)\n" C_RESET, tests[i].xs);
        } else {
            printf(C_RED "  FAIL: lgamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            TEST_FAIL();
        }

        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
    }

    printf("\n");
}

static void test_qf_digamma(void) {
    printf(C_CYAN "TEST: qf_digamma\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;
        const char *expected;
        double acceptable_error;
    } tests[] = {

        /* Known values */
        { "1",    "-0.577215664901532860606512090082402", 1e-29 },
        { "2",     "0.422784335098467139393487909917598", 1e-30 },
        { "3",     "0.922784335098467139393487909917598", 1e-30 },
        { "4",     "1.25611766843180047272682124325093",  1e-30 },

        /* Non-integers */
        { "1.5",   "0.036489973978576520559023667001244", 1e-30 },
        { "2.5",   "0.703156640645243187225690333667911", 1e-30 },

        /* Larger x */
        { "4.5", "1.38887092635952890151140461938", 1e-29 },
        { "5.5", "1.61109314858175112373362684160", 1e-29 },
        { "6.5", "1.79291133039993294191544502342", 1e-29 },
        { "7.5", "1.94675748424608678806929117727", 1e-29 },
        { "8.0", "2.01564147795560999653634505277", 1e-29 },
        { "9.0", "2.14064147795560999653634505277", 1e-29 },
        { "10",  "2.25175258906672110764745616389", 1e-29 },
        { "12",  "2.44266167997581201673836525479", 1e-29 },
        { "14",  "2.60291809023222227314862166505", 1e-29 },
        { "16",  "2.74101332832746036838671690315", 1e-29 },
        { "18",  "2.86233685773922507426906984432", 1e-29 },
        { "20",  "2.97052399224214905087725697883", 1e-29 },

        /* Poles */
        { "0",     "NAN", 0.0 },
        { "-1",    "NAN", 0.0 },
        { "-2",    "NAN", 0.0 },

        { NULL, NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t got = qf_digamma(x);

        if (strcmp(tests[i].expected, "NAN") == 0) {
            if (qf_isnan(got)) {
                printf(C_GREEN "  OK: digamma(%s)\n" C_RESET, tests[i].xs);
            } else {
                printf(C_RED "  FAIL: digamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
                printf("    expected = NAN\n");
                qf_to_string(got, buf, sizeof(buf));
                printf("    got      = %s\n", buf);
                TEST_FAIL();
            }
            continue;
        }

        qfloat_t exp = qf_from_string(tests[i].expected);

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close(got, exp, tests[i].acceptable_error)) {
            printf(C_GREEN "  OK: digamma(%s)\n" C_RESET, tests[i].xs);
        } else {
            printf(C_RED "  FAIL: digamma(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            TEST_FAIL();
        }

        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
    }

    printf("\n");
}

static void test_qf_gammainv(void) {
    printf(C_CYAN "TEST: qf_gammainv\n" C_RESET);

    char buf[256], buf_exp[256];

    struct {
        const char *xs;
        double acceptable_error;
    } tests[] = {

        { "1",    1e-30 },
        { "3",    1e-30 },
        { "4",    1e-30 },
        { "5",    1e-30 },
        { "7",    1e-30 },
        { "9",    1e-30 },
        { "9.5",  1e-30 },
        { "10",   1e-28 },
        { "12",   1e-28 },
        { "15",   1e-28 },
        { "20",   1e-28 },

        { NULL, 0.0 }
    };

    for (int i = 0; tests[i].xs != NULL; i++) {

        qfloat_t x   = qf_from_string(tests[i].xs);
        qfloat_t y   = qf_gamma(x);
        qfloat_t got = qf_gammainv(y);

        qfloat_t exp = x;

        qf_to_string(got, buf, sizeof(buf));
        qf_to_string(exp, buf_exp, sizeof(buf_exp));

        if (qf_close(got, exp, tests[i].acceptable_error)) {
            printf(C_GREEN "  OK: gammainv(gamma(%s))\n" C_RESET, tests[i].xs);
        } else {
            printf(C_RED "  FAIL: gammainv(%s)  [%s:%d]\n" C_RESET, tests[i].xs, __FILE__, __LINE__);
            TEST_FAIL();
        }

        printf("    got      = %s\n", buf);
        printf("    expected = %s\n", buf_exp);
    }

    printf("\n");
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

        if (qf_close(got, exp, tests[i].acceptable_error)) {
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

        if (qf_close(got, exp, tests[i].acceptable_error)) {
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

            int ok = qf_close(logB, logB_expected, 1e-29);

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

        int ok1 = qf_close(logB1v, logB1v_expected, 1e-28);
        int ok2 = qf_close(logBv1, logBv1_expected, 1e-28);

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

/* -------------------------------------------------------------------------
   TEST 1: Definition check  W(x)*exp(W(x)) = x
   ------------------------------------------------------------------------- */
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
            print_qf("W(x)", w);
            print_qf("W*exp(W)", lhs);
            print_qf("x", x);
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
            print_qf("ProductLog", p);
            print_qf("LambertW", w);
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
        print_qf("W(0)", w0);
        print_qf("W(-1/e)", w);
        print_qf("W(e)", we);
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
            print_qf("W(prev)", prev);
            print_qf("W(curr)", w);
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
static void test_qf_productlog_all(void)
{
    RUN_TEST(test_qf_productlog_definition, __func__);
    RUN_TEST(test_qf_productlog_consistency, __func__);
    RUN_TEST(test_qf_productlog_special, __func__);
    RUN_TEST(test_qf_productlog_monotonicity, __func__);
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
                print_qf("P", P);
                print_qf("Q", Q);
                print_qf("sum", sum);
                print_qf("expected", qf_from_double(1.0));
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
                print_qf("lower γ", gl);
                print_qf("upper Γ", gu);
                print_qf("sum", sum);
                print_qf("expected Γ(s)", gs);
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
            print_qf("γ(1,x)", gl);
            print_qf("expected", expected_lower);
            print_qf("Γ(1,x)", gu);
            print_qf("expected", expected_upper);
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
    RUN_TEST(test_qf_gammainc_PQ_identity, __func__);
    RUN_TEST(test_qf_gammainc_lower_upper_identity, __func__);
    RUN_TEST(test_qf_gammainc_special_s1, __func__);
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
            print_qf("residual", res);
            print_qf("dEi",      dEi);
            print_qf("rhs",      rhs);
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
            print_qf("residual", res);
            print_qf("dE1",      dE1);
            print_qf("rhs",      rhs);
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
            print_qf("E1(x)", e1);
            print_qf("Ei(-x)", ei);
            print_qf("sum",   sum);
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
        { "2.0",  "4.95423435600189016337950513022703527552", 1e-29 },
        { "5.0",  "40.18527535580317745509142179379586709542", 1e-28 },

        /* large x */
        { "10.0", "2492.22897624187775913844014399852484899", 1e-27 },

        /* negative x */
        { "-0.1", "-1.822923958419390666080913658291830939119", 1e-29 },
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
        int pass = (ad.hi < cases[i].tolerance);

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
    RUN_TEST(test_qf_ei_deriv, __func__);
    RUN_TEST(test_qf_e1_deriv, __func__);
    RUN_TEST(test_qf_ei_e1_identity, __func__);
    RUN_TEST(test_ei_values, __func__);
}

static void test_arithmetic(void) {
    RUN_TEST(test_add, __func__);
    RUN_TEST(test_mul, __func__);
    RUN_TEST(test_div, __func__);
    RUN_TEST(test_sqrt, __func__);
    RUN_TEST(test_exp_log, __func__);
    RUN_TEST(test_qf_exp, __func__);
    RUN_TEST(test_qf_log, __func__);
    RUN_TEST(test_stability, __func__);
}

void test_strings(void) {
    RUN_TEST(test_qf_to_string, __func__);
    RUN_TEST(test_qf_from_string, __func__);
    RUN_TEST(test_from_string_basic, __func__);
    RUN_TEST(test_from_string_scientific, __func__);
    RUN_TEST(test_round_trip, __func__);
}

void test_printf(void) {
    RUN_TEST(test_qf_sprintf_and_printf, __func__);
}

void test_power(void) {
    RUN_TEST(test_qf_pow_int, __func__);
    RUN_TEST(test_qf_pow, __func__);
    RUN_TEST(test_qf_pow10, __func__);
}

void test_trigonometric(void) {
    RUN_TEST(test_qf_trig, __func__);
    RUN_TEST(test_qf_atan, __func__);
    RUN_TEST(test_qf_atan2, __func__);
    RUN_TEST(test_qf_asin, __func__);
    RUN_TEST(test_qf_acos, __func__);
}

void test_hyperbolic(void) {
    RUN_TEST(test_qf_cosh, __func__);
    RUN_TEST(test_qf_sinh, __func__);
    RUN_TEST(test_qf_cosh, __func__);
    RUN_TEST(test_qf_tanh, __func__);
    RUN_TEST(test_qf_asinh, __func__);
    RUN_TEST(test_qf_acosh, __func__);
    RUN_TEST(test_qf_atanh, __func__);
}

void test_hypotenus(void) {
    RUN_TEST(test_qf_hypot, __func__);
}

void test_gamma_erf_erfc_erfinv_erfcinv_digamma(void) {
    RUN_TEST(test_qf_gamma, __func__);
    RUN_TEST(test_qf_erf, __func__);
    RUN_TEST(test_qf_erfc, __func__);
    RUN_TEST(test_qf_erfinv, __func__);
    RUN_TEST(test_qf_erfcinv, __func__);
    RUN_TEST(test_qf_lgamma, __func__);
    RUN_TEST(test_qf_digamma, __func__);
    RUN_TEST(test_qf_gammainv, __func__);
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

void test_gammainc_ei_e1(void) {
    RUN_TEST(test_qf_gammainc_all, __func__);
    RUN_TEST(test_qf_ei_e1_all, __func__);    
}

void test_readme_examples(void) {
    /* Compute W0(x) for several representative values */
    const char *inputs[] = {
        "0",
        "1e-6",
        "0.1",
        "1",
        "5",
        "-0.3678794411714423215955237701614609", /* -1/e */
        NULL
    };

    for (int i = 0; inputs[i] != NULL; i++) {
        /* Parse x from a decimal string */
        qfloat_t x = qf_from_string(inputs[i]);

        /* Compute the principal branch W0(x) */
        qfloat_t w = qf_lambert_w0(x);

        qf_printf("W0(%s) = %q\n", inputs[i], w);
    }    
}
/* -----------------------------------------------------------
   Main
   ----------------------------------------------------------- */

int tests_main() {
    // qfloat_t x = qf_from_string("1.7724538509055160272981674833411451827975494561223871282138");
    // char* s = "QF_SQRT_PI";
    // printf("const qfloat_t %s = {\n    %.17g,\n    %.17g\n};\n", s, x.hi, x.lo);
    // printf("extern const qfloat_t %s;\n", s);
    // exit(0);

    printf(C_YELLOW "Running qfloat_t tests...\n\n" C_RESET);

    RUN_TEST(test_arithmetic, NULL);
    RUN_TEST(test_strings, NULL);
    RUN_TEST(test_printf, NULL);
    RUN_TEST(test_power, NULL);
    RUN_TEST(test_trigonometric, NULL);
    RUN_TEST(test_hyperbolic, NULL);
    RUN_TEST(test_hypotenus, NULL);
    RUN_TEST(test_gamma_erf_erfc_erfinv_erfcinv_digamma, NULL);
    RUN_TEST(test_lambert_w, NULL);
    RUN_TEST(test_beta_logbeta_binomial_beta_pdf_logbeta_pdf_normal_pdf_cdf_logpdf, NULL);
    RUN_TEST(test_gammainc_ei_e1, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    RUN_TEST(test_readme_examples, NULL);

    printf("\n" C_YELLOW "Done.\n" C_RESET);

    return tests_failed;
}
