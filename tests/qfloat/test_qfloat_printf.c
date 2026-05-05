#include "test_qfloat.h"

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
    RUN_SUBTEST(test_qd_sprintf_basic);
    RUN_SUBTEST(test_qd_sprintf_multiple);
    RUN_SUBTEST(test_qd_sprintf_mixed);
    RUN_SUBTEST(test_qd_sprintf_buffer_limit);
    RUN_SUBTEST(test_qd_sprintf_edge_cases);

    RUN_SUBTEST(test_qd_sprintf_q_precision);
    RUN_SUBTEST(test_qd_sprintf_q_zero_precision);
    RUN_SUBTEST(test_qd_sprintf_q_flags);
    RUN_SUBTEST(test_qd_sprintf_q_width);
    RUN_SUBTEST(test_qd_sprintf_q_fallback);
    RUN_SUBTEST(test_qd_sprintf_q_fallback_width);
    RUN_SUBTEST(test_qf_sprintf_q_concise);

    /* New tests */
    RUN_SUBTEST(test_qf_sprintf_null_safe_new);
    RUN_SUBTEST(test_qf_sprintf_two_pass_new);
    RUN_SUBTEST(test_qf_printf_stdout);
}

void test_printf(void) {
    RUN_SUBTEST(test_qf_sprintf_and_printf);
}
