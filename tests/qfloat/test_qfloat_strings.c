#include "test_qfloat.h"

static bool qfloat_string_value_matches(qfloat_t got, qfloat_t expected, double tol)
{
    if (qf_isnan(expected))
        return qf_isnan(got) && qf_signbit(got) == qf_signbit(expected);

    if (qf_isinf(expected))
        return qf_isinf(got) && qf_signbit(got) == qf_signbit(expected);

    if (qf_eq(expected, qf_from_double(0.0)))
        return qf_eq(got, expected) || qf_close(got, expected, tol);

    return qf_eq(got, expected) || qf_close(got, expected, tol) || qf_close_rel(got, expected, tol);
}

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
        {"sqrt(2)*10^200", { 1.4142135623730952e+300, -4.5949334009680563e+283 }, "1.414213562373095123054632766267823e+300" },
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
        bool ok = (strcmp(buf, tests[i].expected) == 0 ||
                   qfloat_string_value_matches(x, tests[i].x, 1e-30) ||
                   (tests[i].x.hi == x.hi && tests[i].x.lo == x.lo));

        printf("  %s\n", tests[i].label);
        printf("    input     = hi=%.17g  lo=%.17g\n", tests[i].x.hi, tests[i].x.lo);
        printf("    got       = %s\n", buf);
        printf("    expected  = %s\n", tests[i].expected);
        if (qf_isnan(tests[i].x) || qf_isinf(tests[i].x) || qf_eq(tests[i].x, qf_from_double(0.0))) {
            printf("    rel error = n/a\n");
        } else {
            qfloat_t err = qf_abs(qf_sub(qf_div(x, tests[i].x), (qfloat_t){1,0}));
            printf("    rel error = %.17g\n", err.hi);
        }

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
        bool ok = (qfloat_string_value_matches(x, tests[i].expected, 1e-30) ||
                   (x.hi == tests[i].expected.hi && x.lo == tests[i].expected.lo));

        printf("  %s\n", tests[i].label);
        printf("    input     = \"%s\"\n", tests[i].input);
        printf("    got       = hi=%.17g  lo=%.17g\n", x.hi, x.lo);
        printf("    expected  = hi=%.17g  lo=%.17g\n", tests[i].expected.hi, tests[i].expected.lo);
        if (qf_isnan(tests[i].expected) || qf_isinf(tests[i].expected) || qf_eq(tests[i].expected, qf_from_double(0.0))) {
            printf("    rel error = n/a\n");
        } else {
            qfloat_t err = qf_abs(qf_sub(qf_div(x, tests[i].expected), (qfloat_t){1,0}));
            printf("    rel error = %.17g\n", err.hi);
        }

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

void test_strings(void) {
    printf(C_CYAN "TEST GROUP: qfloat string conversions\n" C_RESET);
    printf("  entering test_qf_to_string\n");
    TEST_RUN_SUBTEST(test_qf_to_string, NULL);
    printf("  entering test_qf_from_string\n");
    TEST_RUN_SUBTEST(test_qf_from_string, NULL);
    printf("  entering test_from_string_basic\n");
    TEST_RUN_SUBTEST(test_from_string_basic, NULL);
    printf("  entering test_from_string_scientific\n");
    TEST_RUN_SUBTEST(test_from_string_scientific, NULL);
    printf("  entering test_round_trip\n");
    TEST_RUN_SUBTEST(test_round_trip, NULL);
}
