#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "mfloat.h"

#ifndef TEST_MFLOAT_MATHS_PRECISION
/* Keep maths-only precision configurable without affecting core object tests. */
#define TEST_MFLOAT_MATHS_PRECISION 768u
#endif

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

static int format_mfloat_value(char *buf, size_t buf_size, const mfloat_t *value);
static int format_mfloat_decimal_value(char *buf, size_t buf_size, const mfloat_t *value);
static void print_mfloat_error_check_mode(const char *label,
                                          const mfloat_t *got,
                                          const char *expected_text,
                                          int use_decimal_format);
static void print_mfloat_error_check_value(const char *label,
                                           const mfloat_t *got,
                                           const mfloat_t *expected);
static int mfloat_meets_precision_value(const mfloat_t *got,
                                        const mfloat_t *expected,
                                        int relative_mode);
static int qfloat_is_normalized(qfloat_t value)
{
    double hi = value.hi;
    double lo = value.lo;
    double abs_hi, ulp;
    int exp2;

    if (isnan(hi) || isnan(lo) || isinf(hi))
        return 1;
    if (hi == 0.0)
        return lo == 0.0;

    abs_hi = fabs(hi);
    if (abs_hi < DBL_MIN) {
        ulp = ldexp(1.0, -1074);
    } else {
        frexp(abs_hi, &exp2);
        ulp = ldexp(1.0, exp2 - 53);
    }

    return fabs(lo) <= 0.5 * ulp;
}

static void print_mfloat_value(const char *label, const mfloat_t *value)
{
    char buf[1024];

    if (!value) {
        printf(C_CYAN "%s" C_RESET " = <null>\n", label);
        return;
    }
    if (format_mfloat_value(buf, sizeof(buf), value) < 0) {
        printf(C_CYAN "%s" C_RESET " = <format-error>\n", label);
        return;
    }
    printf(C_CYAN "%s" C_RESET " = " C_WHITE "%s" C_RESET "\n", label, buf);
}

static int format_mfloat_value(char *buf, size_t buf_size, const mfloat_t *value)
{
    char fmt[32];
    size_t precision_bits;
    int decimal_digits;
    int sci_precision;

    if (!buf || buf_size == 0 || !value)
        return -1;
    precision_bits = mf_get_precision(value);
    decimal_digits = (int)ceil((double)precision_bits * 0.3010299956639812);
    if (decimal_digits < 1)
        decimal_digits = 1;
    if (decimal_digits > 240)
        decimal_digits = 240;
    sci_precision = decimal_digits - 1;

    snprintf(fmt, sizeof(fmt), "%%.%dMF", sci_precision);
    return mf_sprintf(buf, buf_size, fmt, value);
}

static int format_mfloat_decimal_value(char *buf, size_t buf_size, const mfloat_t *value)
{
    if (!buf || buf_size == 0 || !value)
        return -1;
    return mf_sprintf(buf, buf_size, "%mf", value);
}

static void print_mfloat_error_check(const char *label,
                                     const mfloat_t *got,
                                     const char *expected_text)
{
    print_mfloat_error_check_mode(label, got, expected_text, 0);
}

static void print_mfloat_error_check_decimal(const char *label,
                                             const mfloat_t *got,
                                             const char *expected_text)
{
    print_mfloat_error_check_mode(label, got, expected_text, 1);
}

static void print_mfloat_error_check_mode(const char *label,
                                          const mfloat_t *got,
                                          const char *expected_text,
                                          int use_decimal_format)
{
    mfloat_t *expected = NULL;
    mfloat_t *error = NULL;
    char expected_buf[1024];
    char got_buf[1024];
    char err_buf[1024];
    size_t precision_bits = 0;
    int decimal_digits = 0;

    if (!got || !expected_text) {
        printf(C_CYAN "%s" C_RESET "\n", label);
        printf("    precision = <unknown>\n");
        printf("    expected = %s\n", expected_text ? expected_text : "<null>");
        printf("    got      = <null>\n");
        printf("    error    = <null>\n");
        return;
    }

    precision_bits = mf_get_precision(got);
    decimal_digits = (int)ceil((double)precision_bits * 0.3010299956639812);
    if (decimal_digits < 1)
        decimal_digits = 1;

    expected = mf_new_prec(precision_bits);
    error = mf_clone(got);
    if (!expected || !error || mf_set_string(expected, expected_text) != 0 ||
        mf_sub(error, expected) != 0 || mf_abs(error) != 0 ||
        (use_decimal_format
             ? format_mfloat_decimal_value(expected_buf, sizeof(expected_buf), expected)
             : format_mfloat_value(expected_buf, sizeof(expected_buf), expected)) < 0 ||
        (use_decimal_format
             ? format_mfloat_decimal_value(got_buf, sizeof(got_buf), got)
             : format_mfloat_value(got_buf, sizeof(got_buf), got)) < 0 ||
        mf_sprintf(err_buf, sizeof(err_buf), "%.6MF", error) < 0) {
        printf(C_CYAN "%s" C_RESET "\n", label);
        printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
        printf("    expected = <format-error>\n");
        printf("    got      = <format-error>\n");
        printf("    error    = <format-error>\n");
        mf_free(expected);
        mf_free(error);
        return;
    }

    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
    printf("    expected = %s\n", expected_buf);
    printf("    got      = %s\n", got_buf);
    printf("    error    = %s\n", err_buf);

    mf_free(expected);
    mf_free(error);
}

static void print_mfloat_error_check_value(const char *label,
                                           const mfloat_t *got,
                                           const mfloat_t *expected)
{
    mfloat_t *error = NULL;
    char expected_buf[1024];
    char got_buf[1024];
    char err_buf[1024];
    size_t precision_bits = 0;
    int decimal_digits = 0;

    if (!got || !expected) {
        printf(C_CYAN "%s" C_RESET "\n", label);
        printf("    precision = <unknown>\n");
        printf("    expected = <null>\n");
        printf("    got      = <null>\n");
        printf("    error    = <null>\n");
        return;
    }

    precision_bits = mf_get_precision(got);
    decimal_digits = (int)ceil((double)precision_bits * 0.3010299956639812);
    if (decimal_digits < 1)
        decimal_digits = 1;

    error = mf_clone(got);
    if (!error || mf_sub(error, expected) != 0 || mf_abs(error) != 0 ||
        format_mfloat_value(expected_buf, sizeof(expected_buf), expected) < 0 ||
        format_mfloat_value(got_buf, sizeof(got_buf), got) < 0 ||
        mf_sprintf(err_buf, sizeof(err_buf), "%.6MF", error) < 0) {
        printf(C_CYAN "%s" C_RESET "\n", label);
        printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
        printf("    expected = <format-error>\n");
        printf("    got      = <format-error>\n");
        printf("    error    = <format-error>\n");
        mf_free(error);
        return;
    }

    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
    printf("    expected = %s\n", expected_buf);
    printf("    got      = %s\n", got_buf);
    printf("    error    = %s\n", err_buf);

    mf_free(error);
}

static int mfloat_meets_precision(const mfloat_t *got,
                                  const char *expected_text,
                                  int relative_mode)
{
    mfloat_t *expected = NULL;
    mfloat_t *error = NULL;
    mfloat_t *tol = NULL;
    size_t precision_bits;
    size_t sig_digits = 0;
    long tol_exp2;
    int ok = 0;
    const char *p;

    if (!got || !expected_text)
        return 0;

    precision_bits = mf_get_precision(got);
    expected = mf_create_string(expected_text);
    if (!expected)
        goto cleanup;

    for (p = expected_text; *p; ++p) {
        if (*p >= '0' && *p <= '9')
            sig_digits++;
    }
    if (!mf_is_zero(expected) && sig_digits > 0) {
        size_t oracle_bits = (size_t)floor((double)sig_digits * 3.3219280948873623);

        if (oracle_bits > 4u)
            oracle_bits -= 4u;
        if (oracle_bits < 1u)
            oracle_bits = 1u;
        if (oracle_bits < precision_bits)
            precision_bits = oracle_bits;
    }
    error = mf_clone(got);
    tol = mf_create_long(1);
    if (!expected || !error || !tol)
        goto cleanup;

    if (mf_sub(error, expected) != 0 || mf_abs(error) != 0)
        goto cleanup;

    if (relative_mode && !mf_is_zero(expected)) {
        tol_exp2 = mf_get_exponent2(expected) +
                   (long)mf_get_mantissa_bits(expected) - 1l -
                   (long)precision_bits;
    } else {
        tol_exp2 = -(long)precision_bits;
    }

    if (mf_ldexp(tol, (int)tol_exp2) != 0)
        goto cleanup;

    ok = mf_le(error, tol);

cleanup:
    mf_free(expected);
    mf_free(error);
    mf_free(tol);
    return ok;
}

static int mfloat_meets_precision_value(const mfloat_t *got,
                                        const mfloat_t *expected,
                                        int relative_mode)
{
    mfloat_t *error = NULL;
    mfloat_t *tol = NULL;
    size_t precision_bits;
    long tol_exp2;
    int ok = 0;

    if (!got || !expected)
        return 0;

    precision_bits = mf_get_precision(got);
    error = mf_clone(got);
    tol = mf_create_long(1);
    if (!error || !tol)
        goto cleanup;

    if (mf_sub(error, expected) != 0 || mf_abs(error) != 0)
        goto cleanup;

    if (relative_mode && !mf_is_zero(expected)) {
        tol_exp2 = mf_get_exponent2(expected) +
                   (long)mf_get_mantissa_bits(expected) - 1l -
                   (long)precision_bits;
    } else {
        tol_exp2 = -(long)precision_bits;
    }

    if (mf_ldexp(tol, (int)tol_exp2) != 0)
        goto cleanup;

    ok = mf_le(error, tol);

cleanup:
    mf_free(error);
    mf_free(tol);
    return ok;
}

static int mfloat_matches_expected(const mfloat_t *got,
                                   const mfloat_t *expected,
                                   int relative_mode)
{
    mfloat_t *error = NULL;
    mfloat_t *tol = NULL;
    size_t precision_bits;
    long tol_exp2;
    int ok = 0;

    if (!got || !expected)
        return 0;

    precision_bits = mf_get_precision(got);
    error = mf_clone(got);
    tol = mf_create_long(1);
    if (!error || !tol)
        goto cleanup;

    if (mf_sub(error, expected) != 0 || mf_abs(error) != 0)
        goto cleanup;

    if (relative_mode && !mf_is_zero(expected)) {
        tol_exp2 = mf_get_exponent2(expected) +
                   (long)mf_get_mantissa_bits(expected) - 1l -
                   (long)precision_bits;
    } else {
        tol_exp2 = -(long)precision_bits;
    }

    if (mf_ldexp(tol, (int)tol_exp2) != 0)
        goto cleanup;

    ok = mf_le(error, tol);

cleanup:
    mf_free(error);
    mf_free(tol);
    return ok;
}

static void print_string_check(const char *label,
                               const char *input,
                               const char *expected,
                               const char *got)
{
    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    input    = %s\n", input ? input : "<none>");
    printf("    expected = %s\n", expected ? expected : "<null>");
    printf("    got      = %s\n", got ? got : "<null>");
}

static void print_double_check(const char *label,
                               const char *input,
                               double expected,
                               double got)
{
    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    input    = %s\n", input ? input : "<none>");
    printf("    expected = %.17g\n", expected);
    printf("    got      = %.17g\n", got);
    printf("    error    = %.17g\n", fabs(got - expected));
}

static void print_precision_check(const char *label,
                                  const char *input,
                                  long expected,
                                  long got)
{
    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    input    = %s\n", input ? input : "<none>");
    printf("    expected = %ld\n", expected);
    printf("    got      = %ld\n", got);
}

static void print_ulong_check(const char *label,
                              const char *input,
                              unsigned long long expected,
                              unsigned long long got)
{
    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    input    = %s\n", input ? input : "<none>");
    printf("    expected = %llu\n", expected);
    printf("    got      = %llu\n", got);
}

void test_new_and_precision(void)
{
    size_t saved_default = mf_get_default_precision();
    mfloat_t *a = mf_new();
    mfloat_t *b = mf_new_prec(512);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_TRUE(mf_is_zero(a));
    print_precision_check("mf_new() precision", "mf_new()", 256, (long)mf_get_precision(a));
    ASSERT_EQ_LONG((long)mf_get_precision(a), 256);
    print_precision_check("mf_new_prec(512) precision", "mf_new_prec(512)", 512, (long)mf_get_precision(b));
    ASSERT_EQ_LONG((long)mf_get_precision(b), 512);

    ASSERT_EQ_INT(mf_set_precision(a, 320), 0);
    print_precision_check("mf_set_precision(a, 320)", "a", 320, (long)mf_get_precision(a));
    ASSERT_EQ_LONG((long)mf_get_precision(a), 320);

    ASSERT_EQ_INT(mf_set_default_precision(384), 0);
    print_precision_check("mf_set_default_precision(384)", "default", 384, (long)mf_get_default_precision());
    ASSERT_EQ_LONG((long)mf_get_default_precision(), 384);

    ASSERT_EQ_INT(mf_set_precision_digits(a, 50), 0);
    ASSERT_EQ_LONG((long)mf_get_precision_digits(a), 50);
    ASSERT_EQ_INT(mf_set_default_precision_digits(60), 0);
    ASSERT_EQ_LONG((long)mf_get_default_precision_digits(), 60);
    mf_free(a);
    a = mf_new();
    ASSERT_NOT_NULL(a);
    ASSERT_EQ_LONG((long)mf_get_precision_digits(a), 60);

    mf_free(a);
    mf_free(b);
    ASSERT_EQ_INT(mf_set_default_precision(saved_default), 0);
}

void test_set_long_normalises(void)
{
    mfloat_t *a = mf_new();
    mfloat_t *b = mf_new();
    uint64_t mantissa = 0;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);

    ASSERT_EQ_INT(mf_set_long(a, 12), 0);
    print_mfloat_value("a after mf_set_long(12)", a);
    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), 2);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    print_ulong_check("a mantissa", "12", 3ull, (unsigned long long)mantissa);
    ASSERT_EQ_LONG((long)mantissa, 3);
    ASSERT_EQ_LONG((long)mf_get_mantissa_bits(a), 2);

    ASSERT_EQ_INT(mf_set_long(b, -40), 0);
    print_mfloat_value("b after mf_set_long(-40)", b);
    ASSERT_EQ_LONG((long)mf_get_sign(b), -1);
    ASSERT_EQ_LONG(mf_get_exponent2(b), 3);
    ASSERT_TRUE(mf_get_mantissa_u64(b, &mantissa));
    print_ulong_check("b mantissa", "-40", 5ull, (unsigned long long)mantissa);
    ASSERT_EQ_LONG((long)mantissa, 5);

    mf_free(a);
    mf_free(b);
}

void test_clone_and_clear(void)
{
    mfloat_t *a = mf_create_long(18);
    mfloat_t *b;
    uint64_t mantissa = 0;

    ASSERT_NOT_NULL(a);
    b = mf_clone(a);
    ASSERT_NOT_NULL(b);
    print_mfloat_value("clone source a", a);
    print_mfloat_value("clone alias b", b);

    ASSERT_EQ_LONG((long)mf_get_sign(b), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(b), 1);
    ASSERT_TRUE(mf_get_mantissa_u64(b, &mantissa));
    print_ulong_check("clone mantissa", "18", 9ull, (unsigned long long)mantissa);
    ASSERT_EQ_LONG((long)mantissa, 9);

    mf_clear(b);
    print_mfloat_value("b after mf_clear", b);
    ASSERT_TRUE(mf_is_zero(b));
    ASSERT_EQ_LONG((long)mf_get_sign(b), 0);
    ASSERT_EQ_LONG(mf_get_exponent2(b), 0);

    mf_free(a);
    mf_free(b);
}

void test_set_string_and_basic_arithmetic(void)
{
    mfloat_t *a = mf_create_string("1.5");
    mfloat_t *b = mf_create_string("0.25");
    mfloat_t *c = mf_create_string("2.25");
    uint64_t mantissa = 0;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    print_mfloat_value("a from \"1.5\"", a);
    print_mfloat_value("b from \"0.25\"", b);
    print_mfloat_value("c from \"2.25\"", c);

    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), -1);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 3);

    ASSERT_EQ_INT(mf_add(a, b), 0);
    print_mfloat_value("a after a += b", a);
    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), -2);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 7);

    ASSERT_EQ_INT(mf_mul(b, c), 0);
    print_mfloat_value("b after b *= c", b);
    ASSERT_EQ_LONG((long)mf_get_sign(b), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(b), -4);
    ASSERT_TRUE(mf_get_mantissa_u64(b, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 9);

    ASSERT_EQ_INT(mf_sqrt(c), 0);
    print_mfloat_value("c after sqrt(c)", c);
    ASSERT_EQ_LONG((long)mf_get_sign(c), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(c), -1);
    ASSERT_TRUE(mf_get_mantissa_u64(c, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 3);

    mf_free(a);
    mf_free(b);
    mf_free(c);
}

void test_division_and_power(void)
{
    mfloat_t *a = mf_create_long(1);
    mfloat_t *b = mf_create_long(8);
    mfloat_t *c = mf_create_string("1.5");
    uint64_t mantissa = 0;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    print_mfloat_value("a before division", a);
    print_mfloat_value("b before division", b);
    print_mfloat_value("c before pow_int", c);

    ASSERT_EQ_INT(mf_div(a, b), 0);
    print_mfloat_value("a after a /= b", a);
    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), -3);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 1);

    ASSERT_EQ_INT(mf_pow_int(c, 2), 0);
    print_mfloat_value("c after c^=2", c);
    ASSERT_EQ_LONG((long)mf_get_sign(c), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(c), -2);
    ASSERT_TRUE(mf_get_mantissa_u64(c, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 9);

    ASSERT_EQ_INT(mf_ldexp(c, 3), 0);
    print_mfloat_value("c after ldexp(c, 3)", c);
    ASSERT_EQ_LONG(mf_get_exponent2(c), 1);

    mf_free(a);
    mf_free(b);
    mf_free(c);
}

void test_string_roundtrip(void)
{
    mfloat_t *a = mf_create_string("1.5");
    mfloat_t *b = mf_create_string("0.25");
    mfloat_t *c = mf_create_long(-40);
    mfloat_t *d = mf_create_string("18446744073709551616");
    char *sa = NULL;
    char *sb = NULL;
    char *sc = NULL;
    char *sd = NULL;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);

    sa = mf_to_string(a);
    sb = mf_to_string(b);
    sc = mf_to_string(c);
    sd = mf_to_string(d);

    ASSERT_NOT_NULL(sa);
    ASSERT_NOT_NULL(sb);
    ASSERT_NOT_NULL(sc);
    ASSERT_NOT_NULL(sd);

    print_string_check("roundtrip a", "1.5", "1.5", sa);
    ASSERT_TRUE(strcmp(sa, "1.5") == 0);
    print_string_check("roundtrip b", "0.25", "0.25", sb);
    ASSERT_TRUE(strcmp(sb, "0.25") == 0);
    print_string_check("roundtrip c", "-40", "-40", sc);
    ASSERT_TRUE(strcmp(sc, "-40") == 0);
    print_string_check("roundtrip d", "18446744073709551616", "18446744073709551616", sd);
    ASSERT_TRUE(strcmp(sd, "18446744073709551616") == 0);

    free(sa);
    free(sb);
    free(sc);
    free(sd);
    mf_free(a);
    mf_free(b);
    mf_free(c);
    mf_free(d);
}

void test_printf_family(void)
{
    mfloat_t *a = mf_create_string("1.5");
    mfloat_t *b = mf_create_string("0.000000125");
    char buf[256];
    int n;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);

    n = mf_sprintf(buf, sizeof(buf), "%mf", a);
    print_string_check("mf_sprintf(\"%mf\")", "1.5", "1.5", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "1.5") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%8mf", a);
    print_string_check("mf_sprintf(\"%8mf\")", "1.5", "     1.5", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "     1.5") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%08mf", a);
    print_string_check("mf_sprintf(\"%08mf\")", "1.5", "000001.5", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "000001.5") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%.4mf", a);
    print_string_check("mf_sprintf(\"%.4mf\")", "1.5", "1.5000", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "1.5000") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%MF", a);
    print_string_check("mf_sprintf(\"%MF\")", "1.5", "1.5E+0", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "1.5E+0") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%mf", b);
    print_string_check("mf_sprintf(\"%mf\") tiny", "0.000000125", "0.000000125", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "0.000000125") == 0);

    mf_free(a);
    mf_free(b);
}

void test_constants_and_named_values(void)
{
    size_t saved_default = mf_get_default_precision();
    mfloat_t *pi = mf_pi();
    mfloat_t *e = mf_e();
    mfloat_t *gamma = mf_euler_mascheroni();
    mfloat_t *pi_hi = NULL;
    mfloat_t *e_hi = NULL;
    mfloat_t *gamma_hi = NULL;
    mfloat_t *mx = mf_max();
    mfloat_t *inv = mf_create_long(8);
    char *s_half = NULL;
    char *s_pi_hi = NULL;
    char *s_e_hi = NULL;
    char *s_gamma_hi = NULL;
    char buf[256];
    char tenth_buf[256];
    char tenth_fmt[32];
    int n = 0;
    int tenth_digits = 0;

    ASSERT_NOT_NULL(pi);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(gamma);
    ASSERT_NOT_NULL(mx);
    ASSERT_NOT_NULL(inv);
    print_mfloat_value("mf_pi()", pi);
    print_mfloat_value("mf_e()", e);
    print_mfloat_value("mf_euler_mascheroni()", gamma);
    print_mfloat_value("mf_max()", mx);

    s_half = mf_to_string(MF_HALF);
    ASSERT_NOT_NULL(s_half);
    print_string_check("MF_HALF string", "MF_HALF", "0.5", s_half);
    ASSERT_TRUE(strcmp(s_half, "0.5") == 0);
    tenth_digits = (int)ceil((double)mf_get_precision(MF_TENTH) * 0.3010299956639812);
    if (tenth_digits < 1)
        tenth_digits = 1;
    if (tenth_digits > 120)
        tenth_digits = 120;
    snprintf(tenth_fmt, sizeof(tenth_fmt), "%%.%dmf", tenth_digits);
    ASSERT_TRUE(mf_sprintf(tenth_buf, sizeof(tenth_buf), tenth_fmt, MF_TENTH) >= 0);
    print_string_check("MF_TENTH rounded display", "MF_TENTH", "0.1...", tenth_buf);
    ASSERT_TRUE(strncmp(tenth_buf, "0.1", 3) == 0);

    ASSERT_EQ_LONG((long)mf_get_precision(pi), (long)saved_default);
    ASSERT_EQ_LONG((long)mf_get_precision(e), (long)saved_default);
    ASSERT_EQ_LONG((long)mf_get_precision(gamma), (long)saved_default);

    ASSERT_EQ_INT(mf_set_default_precision(384), 0);
    pi_hi = mf_pi();
    e_hi = mf_e();
    gamma_hi = mf_euler_mascheroni();
    ASSERT_NOT_NULL(pi_hi);
    ASSERT_NOT_NULL(e_hi);
    ASSERT_NOT_NULL(gamma_hi);
    print_mfloat_value("mf_pi() at 384 bits", pi_hi);
    print_mfloat_value("mf_e() at 384 bits", e_hi);
    print_mfloat_value("mf_euler_mascheroni() at 384 bits", gamma_hi);
    ASSERT_EQ_LONG((long)mf_get_precision(pi_hi), 384);
    ASSERT_EQ_LONG((long)mf_get_precision(e_hi), 384);
    ASSERT_EQ_LONG((long)mf_get_precision(gamma_hi), 384);

    s_pi_hi = mf_to_string(pi_hi);
    s_e_hi = mf_to_string(e_hi);
    s_gamma_hi = mf_to_string(gamma_hi);
    ASSERT_NOT_NULL(s_pi_hi);
    ASSERT_NOT_NULL(s_e_hi);
    ASSERT_NOT_NULL(s_gamma_hi);
    ASSERT_TRUE(strncmp(s_pi_hi, "3.141592653589793238462643383279502", 35) == 0);
    ASSERT_TRUE(strncmp(s_e_hi, "2.718281828459045235360287471352662", 35) == 0);
    ASSERT_TRUE(strncmp(s_gamma_hi, "0.577215664901532860606512090082402", 35) == 0);
    ASSERT_TRUE(strlen(s_pi_hi) > 60);
    ASSERT_TRUE(strlen(s_e_hi) > 60);
    ASSERT_TRUE(strlen(s_gamma_hi) > 60);

    ASSERT_EQ_INT(mf_inv(inv), 0);
    ASSERT_EQ_INT(mf_sprintf(buf, sizeof(buf), "%.6mf", inv), 8);
    print_string_check("mf_inv(8)", "8", "0.125000", buf);
    ASSERT_TRUE(strcmp(buf, "0.125000") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%MF", MF_INF);
    print_string_check("MF_INF formatting", "MF_INF", "INF", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "INF") == 0);
    n = mf_sprintf(buf, sizeof(buf), "%MF", MF_NINF);
    print_string_check("MF_NINF formatting", "MF_NINF", "-INF", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "-INF") == 0);
    n = mf_sprintf(buf, sizeof(buf), "%MF", MF_NAN);
    print_string_check("MF_NAN formatting", "MF_NAN", "NAN", buf);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "NAN") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%MF", pi);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strstr(buf, "E") != NULL);
    n = mf_sprintf(buf, sizeof(buf), "%mf", e);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strncmp(buf, "2.718281828", 10) == 0);
    n = mf_sprintf(buf, sizeof(buf), "%mf", gamma);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strncmp(buf, "0.577215664", 10) == 0);
    n = mf_sprintf(buf, sizeof(buf), "%MF", mx);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strstr(buf, "E+") != NULL);

    free(s_half);
    free(s_pi_hi);
    free(s_e_hi);
    free(s_gamma_hi);
    mf_free(pi);
    mf_free(e);
    mf_free(gamma);
    mf_free(pi_hi);
    mf_free(e_hi);
    mf_free(gamma_hi);
    mf_free(mx);
    mf_free(inv);
    ASSERT_EQ_INT(mf_set_default_precision(saved_default), 0);
}

void test_conversion_to_double_and_qfloat(void)
{
    mfloat_t *a = mf_create_string("1.5");
    mfloat_t *b = mf_create_string("0.25");
    mfloat_t *c = mf_create_string("1024");
    mfloat_t *d = mf_create_string("-0.125");
    mfloat_t *e = mf_create_string("9007199254740992");
    qfloat_t qsrc_a = qf_from_string("1.5");
    qfloat_t qsrc_b = qf_from_string("0.25");
    qfloat_t qsrc_c = qf_from_string("1024");
    qfloat_t qsrc_d = qf_from_string("-0.125");
    qfloat_t qsrc_e = qf_from_string("9007199254740992");
    double da, db;
    qfloat_t qa, qb, qc, qd, qe, qinf, qninf, qnan;
    char got_buf[256];
    char expected_buf[256];

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(e);
    print_mfloat_value("a before mf_to_double", a);
    print_mfloat_value("b before mf_to_double", b);

    da = mf_to_double(a);
    db = mf_to_double(b);
    print_double_check("mf_to_double(a)", "1.5", 1.5, da);
    ASSERT_TRUE(fabs(da - 1.5) < 1e-15);
    print_double_check("mf_to_double(b)", "0.25", 0.25, db);
    ASSERT_TRUE(fabs(db - 0.25) < 1e-15);
    ASSERT_TRUE(isinf(mf_to_double(MF_INF)));
    ASSERT_TRUE(isinf(mf_to_double(MF_NINF)));
    ASSERT_TRUE(mf_to_double(MF_NINF) < 0.0);
    ASSERT_TRUE(isnan(mf_to_double(MF_NAN)));

    qa = mf_to_qfloat(a);
    qb = mf_to_qfloat(b);
    qc = mf_to_qfloat(c);
    qd = mf_to_qfloat(d);
    qe = mf_to_qfloat(e);
    qinf = mf_to_qfloat(MF_INF);
    qninf = mf_to_qfloat(MF_NINF);
    qnan = mf_to_qfloat(MF_NAN);

    print_double_check("mf_to_qfloat(a)", "1.5", 1.5, qf_to_double(qa));
    ASSERT_TRUE(fabs(qf_to_double(qa) - 1.5) < 1e-15);
    print_double_check("mf_to_qfloat(b)", "0.25", 0.25, qf_to_double(qb));
    ASSERT_TRUE(fabs(qf_to_double(qb) - 0.25) < 1e-15);
    ASSERT_TRUE(qfloat_is_normalized(qa));
    ASSERT_TRUE(qfloat_is_normalized(qb));

    qf_to_string(qa, got_buf, sizeof(got_buf));
    qf_to_string(qsrc_a, expected_buf, sizeof(expected_buf));
    print_string_check("mf_to_qfloat(\"1.5\")", "1.5", expected_buf, got_buf);
    ASSERT_TRUE(strcmp(got_buf, expected_buf) == 0);

    qf_to_string(qb, got_buf, sizeof(got_buf));
    qf_to_string(qsrc_b, expected_buf, sizeof(expected_buf));
    print_string_check("mf_to_qfloat(\"0.25\")", "0.25", expected_buf, got_buf);
    ASSERT_TRUE(strcmp(got_buf, expected_buf) == 0);

    qf_to_string(qc, got_buf, sizeof(got_buf));
    qf_to_string(qsrc_c, expected_buf, sizeof(expected_buf));
    print_string_check("mf_to_qfloat(\"1024\")", "1024", expected_buf, got_buf);
    ASSERT_TRUE(strcmp(got_buf, expected_buf) == 0);
    ASSERT_TRUE(qfloat_is_normalized(qc));

    qf_to_string(qd, got_buf, sizeof(got_buf));
    qf_to_string(qsrc_d, expected_buf, sizeof(expected_buf));
    print_string_check("mf_to_qfloat(\"-0.125\")", "-0.125", expected_buf, got_buf);
    ASSERT_TRUE(strcmp(got_buf, expected_buf) == 0);
    ASSERT_TRUE(qfloat_is_normalized(qd));

    qf_to_string(qe, got_buf, sizeof(got_buf));
    qf_to_string(qsrc_e, expected_buf, sizeof(expected_buf));
    print_string_check("mf_to_qfloat(\"9007199254740992\")",
                       "9007199254740992",
                       expected_buf,
                       got_buf);
    ASSERT_TRUE(strcmp(got_buf, expected_buf) == 0);
    ASSERT_TRUE(qfloat_is_normalized(qe));

    ASSERT_TRUE(qf_isposinf(qinf));
    ASSERT_TRUE(qf_isneginf(qninf));
    ASSERT_TRUE(qf_isnan(qnan));

    mf_free(a);
    mf_free(b);
    mf_free(c);
    mf_free(d);
    mf_free(e);
}

void test_conversion_from_double_and_qfloat(void)
{
    mfloat_t *a = mf_create_double(1.5);
    mfloat_t *b = mf_create_double(0.25);
    mfloat_t *c = mf_new();
    mfloat_t *d = mf_new();
    qfloat_t q = qf_from_string("3.1415926535897932384626433832795");
    qfloat_t qc;
    qfloat_t qerr;
    char *sa = NULL;
    char *sb = NULL;
    char *sc = NULL;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);

    sa = mf_to_string(a);
    sb = mf_to_string(b);
    ASSERT_NOT_NULL(sa);
    ASSERT_NOT_NULL(sb);
    print_string_check("mf_create_double(1.5)", "1.5", "1.5", sa);
    ASSERT_TRUE(strcmp(sa, "1.5") == 0);
    print_string_check("mf_create_double(0.25)", "0.25", "0.25", sb);
    ASSERT_TRUE(strcmp(sb, "0.25") == 0);

    ASSERT_EQ_INT(mf_set_qfloat(c, q), 0);
    print_mfloat_value("mf_set_qfloat(pi-ish)", c);
    sc = mf_to_string(c);
    ASSERT_NOT_NULL(sc);
    ASSERT_TRUE(strncmp(sc, "3.141592653589793238462643383279", 32) == 0);
    qc = mf_to_qfloat(c);
    qerr = qf_abs(qf_sub(qc, q));
    print_double_check("qfloat roundtrip error", "qfloat pi", 0.0, qf_to_double(qerr));
    ASSERT_TRUE(qf_to_double(qerr) < 1e-30);

    ASSERT_EQ_INT(mf_set_double(d, INFINITY), 0);
    ASSERT_TRUE(isinf(mf_to_double(d)));
    ASSERT_EQ_INT(mf_set_double(d, -INFINITY), 0);
    ASSERT_TRUE(isinf(mf_to_double(d)));
    ASSERT_TRUE(mf_to_double(d) < 0.0);
    ASSERT_EQ_INT(mf_set_double(d, NAN), 0);
    ASSERT_TRUE(isnan(mf_to_double(d)));

    free(sa);
    free(sb);
    free(sc);
    mf_free(a);
    mf_free(b);
    mf_free(c);
    mf_free(d);
}

void test_extended_math_wrappers(void)
{
    size_t saved_default = mf_get_default_precision();
    mfloat_t *a = NULL;
    mfloat_t *b = NULL;
    mfloat_t *c = NULL;
    mfloat_t *d = NULL;
    mfloat_t *e = NULL;
    mfloat_t *f = NULL;
    mfloat_t *i = NULL;
    mfloat_t *j = NULL;
    mfloat_t *k = NULL;
    mfloat_t *h = NULL;
    mfloat_t *p = NULL;
    mfloat_t *sin_pair = NULL;
    mfloat_t *cos_pair = NULL;
    mfloat_t *sinh_pair = NULL;
    mfloat_t *cosh_pair = NULL;
    mfloat_t *expected_e = NULL;
    mfloat_t *expected_calc = NULL;
    mfloat_t *g = NULL;
    char *sa = NULL;

    ASSERT_EQ_INT(mf_set_default_precision(TEST_MFLOAT_MATHS_PRECISION), 0);
    a = mf_create_long(1);
    b = mf_create_long(4);
    c = mf_create_string("0.5");
    d = mf_create_long(2);
    e = mf_create_long(3);
    f = mf_create_long(1);
    i = mf_create_long(5);
    j = mf_create_long(1);
    k = mf_create_long(-1);
    h = mf_create_long(2);
    p = mf_create_long(10);
    sin_pair = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    cos_pair = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    sinh_pair = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    cosh_pair = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(f);
    ASSERT_NOT_NULL(i);
    ASSERT_NOT_NULL(j);
    ASSERT_NOT_NULL(k);
    ASSERT_NOT_NULL(h);
    ASSERT_NOT_NULL(p);
    ASSERT_NOT_NULL(sin_pair);
    ASSERT_NOT_NULL(cos_pair);
    ASSERT_NOT_NULL(sinh_pair);
    ASSERT_NOT_NULL(cosh_pair);
    print_mfloat_value("a initial", a);
    print_mfloat_value("b initial", b);
    print_mfloat_value("c initial", c);
    print_mfloat_value("d initial", d);
    print_mfloat_value("e initial", e);
    print_mfloat_value("f initial", f);
    print_mfloat_value("i initial", i);
    print_mfloat_value("j initial", j);
    print_mfloat_value("k initial", k);
    print_mfloat_value("h initial", h);
    print_mfloat_value("p initial", p);

    ASSERT_EQ_INT(mf_exp(a), 0);
    print_mfloat_value("a after exp", a);
    print_double_check("exp(1)", "1", 2.718281828459045, mf_to_double(a));
    print_mfloat_error_check("exp(1) mfloat error",
                             a,
                             "2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427427466391932003059921817413596629043572900334295260595630738132328627943490763233829880753195251019011573834187930702154089149934884");
    ASSERT_TRUE(mfloat_meets_precision(
        a,
        "2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427427466391932003059921817413596629043572900334295260595630738132328627943490763233829880753195251019011573834187930702154089149934884",
        1));
    ASSERT_TRUE(fabs(mf_to_double(a) - 2.718281828459045) < 1e-12);
    expected_e = mf_e();
    ASSERT_NOT_NULL(expected_e);
    ASSERT_TRUE(mf_eq(a, expected_e));
    ASSERT_EQ_INT(mf_log(a), 0);
    print_mfloat_value("a after log(exp(1))", a);
    print_double_check("log(exp(1))", "exp(1)", 1.0, mf_to_double(a));
    print_mfloat_error_check_decimal("log(exp(1)) mfloat error", a, "1");
    ASSERT_TRUE(mfloat_meets_precision(a, "1", 0));
    ASSERT_TRUE(fabs(mf_to_double(a) - 1.0) < 1e-12);

    ASSERT_EQ_INT(mf_sqrt(b), 0);
    print_mfloat_value("b after sqrt", b);
    print_double_check("sqrt(4)", "4", 2.0, mf_to_double(b));
    print_mfloat_error_check_decimal("sqrt(4) mfloat error", b, "2");
    ASSERT_TRUE(mfloat_meets_precision(b, "2", 0));
    ASSERT_TRUE(fabs(mf_to_double(b) - 2.0) < 1e-12);

    ASSERT_EQ_INT(mf_sin(c), 0);
    print_mfloat_value("c after sin", c);
    print_double_check("sin(0.5)", "0.5", sin(0.5), mf_to_double(c));
    print_mfloat_error_check("sin(0.5) mfloat error", c,
                             "0.4794255386042030002732879352155713880818033679406006751886166131255350002878148322096312746843482690861320910845057174178110937486099402827801539620461919246099572939322814005335463381880552285956701356998542336391210717207773801529");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.4794255386042030002732879352155713880818033679406006751886166131255350002878148322096312746843482690861320910845057174178110937486099402827801539620461919246099572939322814005335463381880552285956701356998542336391210717207773801529",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sin(0.5)) < 1e-12);
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sincos(c, sin_pair, cos_pair), 0);
    print_mfloat_error_check("sincos(sin, 0.5) mfloat error", sin_pair,
                             "0.4794255386042030002732879352155713880818033679406006751886166131255350002878148322096312746843482690861320910845057174178110937486099402827801539620461919246099572939322814005335463381880552285956701356998542336391210717207773801529");
    print_mfloat_error_check("sincos(cos, 0.5) mfloat error", cos_pair,
                             "0.8775825618903727161162815826038296519916451971097440529976108683159507632742139474057941840846822583554784005931090539934138279768332802667997561209502240155876291568785907234769393109896167396770144089976491285702134682183845438182");
    ASSERT_TRUE(mfloat_meets_precision(
        sin_pair,
        "0.4794255386042030002732879352155713880818033679406006751886166131255350002878148322096312746843482690861320910845057174178110937486099402827801539620461919246099572939322814005335463381880552285956701356998542336391210717207773801529",
        1));
    ASSERT_TRUE(mfloat_meets_precision(
        cos_pair,
        "0.8775825618903727161162815826038296519916451971097440529976108683159507632742139474057941840846822583554784005931090539934138279768332802667997561209502240155876291568785907234769393109896167396770144089976491285702134682183845438182",
        1));
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cos(c), 0);
    print_mfloat_value("c after cos", c);
    print_double_check("cos(0.5)", "0.5", cos(0.5), mf_to_double(c));
    print_mfloat_error_check("cos(0.5) mfloat error", c,
                             "0.8775825618903727161162815826038296519916451971097440529976108683159507632742139474057941840846822583554784005931090539934138279768332802667997561209502240155876291568785907234769393109896167396770144089976491285702134682183845438182");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.8775825618903727161162815826038296519916451971097440529976108683159507632742139474057941840846822583554784005931090539934138279768332802667997561209502240155876291568785907234769393109896167396770144089976491285702134682183845438182",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - cos(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_beta(d, e), 0);
    print_mfloat_value("d after beta(d, e)", d);
    print_double_check("beta(2, 3)", "(2,3)", 1.0 / 12.0, mf_to_double(d));
    print_mfloat_error_check("beta(2, 3) mfloat error", d,
                             "0.08333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333336");
    expected_calc = mf_create_long(1);
    ASSERT_NOT_NULL(expected_calc);
    {
        mfloat_t *twelve = mf_create_long(12);
        ASSERT_NOT_NULL(twelve);
        ASSERT_EQ_INT(mf_div(expected_calc, twelve), 0);
        mf_free(twelve);
    }
    ASSERT_TRUE(mfloat_matches_expected(d, expected_calc, 1));
    mf_free(expected_calc);
    expected_calc = NULL;
    ASSERT_TRUE(fabs(mf_to_double(d) - (1.0 / 12.0)) < 1e-12);

    ASSERT_EQ_INT(mf_pow(h, p), 0);
    print_mfloat_value("h after h^p", h);
    print_double_check("2^10", "(2,10)", 1024.0, mf_to_double(h));
    print_mfloat_error_check_decimal("2^10 mfloat error", h, "1024");
    ASSERT_TRUE(mfloat_meets_precision(h, "1024", 0));
    ASSERT_TRUE(fabs(mf_to_double(h) - 1024.0) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_tan(c), 0);
    print_mfloat_value("c after tan", c);
    print_double_check("tan(0.5)", "0.5", tan(0.5), mf_to_double(c));
    print_mfloat_error_check("tan(0.5) mfloat error", c,
                             "0.5463024898437905132551794657802853832975517201797912461640913859329075105180258157151806482706562185891048626002641142654932300911684028432173909299109142166369407437884742689574104012579117568787459997245089182122377508438391608137");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.5463024898437905132551794657802853832975517201797912461640913859329075105180258157151806482706562185891048626002641142654932300911684028432173909299109142166369407437884742689574104012579117568787459997245089182122377508438391608137",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sinh(c), 0);
    print_mfloat_value("c after sinh", c);
    print_double_check("sinh(0.5)", "0.5", sinh(0.5), mf_to_double(c));
    print_mfloat_error_check("sinh(0.5) mfloat error", c,
                             "0.5210953054937473616224256264114915591059289826114805279460935764528022508902335923170644542741885934882214239811341359140666794448283313132498958147711911861109207062907779867237162829057943448262401667428326636169984336690720577785");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.5210953054937473616224256264114915591059289826114805279460935764528022508902335923170644542741885934882214239811341359140666794448283313132498958147711911861109207062907779867237162829057943448262401667428326636169984336690720577785",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sinh(0.5)) < 1e-9);
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sinhcosh(c, sinh_pair, cosh_pair), 0);
    print_mfloat_error_check("sinhcosh(sinh, 0.5) mfloat error", sinh_pair,
                             "0.5210953054937473616224256264114915591059289826114805279460935764528022508902335923170644542741885934882214239811341359140666794448283313132498958147711911861109207062907779867237162829057943448262401667428326636169984336690720577785");
    print_mfloat_error_check("sinhcosh(cosh, 0.5) mfloat error", cosh_pair,
                             "1.127625965206380785226225161402672012547847118098667483628985735187858770303982016315712065782178049514645213775173661090604487530391277846591075637718868610818501952807625927996232181753694900070628738593585802103842632987787742311");
    ASSERT_TRUE(mfloat_meets_precision(
        sinh_pair,
        "0.5210953054937473616224256264114915591059289826114805279460935764528022508902335923170644542741885934882214239811341359140666794448283313132498958147711911861109207062907779867237162829057943448262401667428326636169984336690720577785",
        1));
    ASSERT_TRUE(mfloat_meets_precision(
        cosh_pair,
        "1.127625965206380785226225161402672012547847118098667483628985735187858770303982016315712065782178049514645213775173661090604487530391277846591075637718868610818501952807625927996232181753694900070628738593585802103842632987787742311",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cosh(c), 0);
    print_mfloat_value("c after cosh", c);
    print_double_check("cosh(0.5)", "0.5", cosh(0.5), mf_to_double(c));
    print_mfloat_error_check("cosh(0.5) mfloat error", c,
                             "1.127625965206380785226225161402672012547847118098667483628985735187858770303982016315712065782178049514645213775173661090604487530391277846591075637718868610818501952807625927996232181753694900070628738593585802103842632987787742311");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "1.127625965206380785226225161402672012547847118098667483628985735187858770303982016315712065782178049514645213775173661090604487530391277846591075637718868610818501952807625927996232181753694900070628738593585802103842632987787742311",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - cosh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atan(c), 0);
    print_mfloat_value("c after atan", c);
    print_double_check("atan(0.5)", "0.5", atan(0.5), mf_to_double(c));
    print_mfloat_error_check("atan(0.5) mfloat error", c,
                             "0.4636476090008061162142562314612144020285370542861202638109330887201978641657417053006002839848878925565298522511908375135058181816250111554715305699441056207193362661648801015325027559879258055168538891674782372865387939180125171996");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.4636476090008061162142562314612144020285370542861202638109330887201978641657417053006002839848878925565298522511908375135058181816250111554715305699441056207193362661648801015325027559879258055168538891674782372865387939180125171996",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - atan(0.5)) < 1e-9);

    {
        mfloat_t *asin_expected = mf_pi();
        mfloat_t *six = mf_create_long(6);
        ASSERT_NOT_NULL(asin_expected);
        ASSERT_NOT_NULL(six);
        ASSERT_EQ_INT(mf_set_precision(asin_expected, TEST_MFLOAT_MATHS_PRECISION), 0);
        ASSERT_EQ_INT(mf_div(asin_expected, six), 0);
        ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
        ASSERT_EQ_INT(mf_asin(c), 0);
        print_mfloat_value("c after asin", c);
        print_double_check("asin(0.5)", "0.5", asin(0.5), mf_to_double(c));
        print_mfloat_error_check_value("asin(0.5) mfloat error", c, asin_expected);
        ASSERT_TRUE(mfloat_meets_precision_value(c, asin_expected, 1));
        ASSERT_TRUE(fabs(mf_to_double(c) - asin(0.5)) < 1e-9);
        mf_free(six);
        mf_free(asin_expected);
    }

    {
        mfloat_t *acos_expected = mf_pi();
        mfloat_t *three = mf_create_long(3);
        ASSERT_NOT_NULL(acos_expected);
        ASSERT_NOT_NULL(three);
        ASSERT_EQ_INT(mf_set_precision(acos_expected, TEST_MFLOAT_MATHS_PRECISION), 0);
        ASSERT_EQ_INT(mf_div(acos_expected, three), 0);
        ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
        ASSERT_EQ_INT(mf_acos(c), 0);
        print_mfloat_value("c after acos", c);
        print_double_check("acos(0.5)", "0.5", acos(0.5), mf_to_double(c));
        print_mfloat_error_check_value("acos(0.5) mfloat error", c, acos_expected);
        ASSERT_TRUE(mfloat_meets_precision_value(c, acos_expected, 1));
        ASSERT_TRUE(fabs(mf_to_double(c) - acos(0.5)) < 1e-9);
        mf_free(three);
        mf_free(acos_expected);
    }

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_tanh(c), 0);
    print_mfloat_value("c after tanh", c);
    print_double_check("tanh(0.5)", "0.5", tanh(0.5), mf_to_double(c));
    print_mfloat_error_check("tanh(0.5) mfloat error", c,
                             "0.4621171572600097585023184836436725487302892803301130385527318158380809061404092787749490641519624905843489329862815491328822654618695978959571446116158785633291327041667769391973725679307702700373014486085992624095817836118928991466");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.4621171572600097585023184836436725487302892803301130385527318158380809061404092787749490641519624905843489329862815491328822654618695978959571446116158785633291327041667769391973725679307702700373014486085992624095817836118928991466",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tanh(0.5)) < 1e-9);

    {
        mfloat_t *asinh_expected = mf_create_string("0.5");
        mfloat_t *tmp = mf_create_string("0.5");
        ASSERT_NOT_NULL(asinh_expected);
        ASSERT_NOT_NULL(tmp);
        ASSERT_EQ_INT(mf_mul(tmp, tmp), 0);
        ASSERT_EQ_INT(mf_add(tmp, MF_ONE), 0);
        ASSERT_EQ_INT(mf_sqrt(tmp), 0);
        ASSERT_EQ_INT(mf_add(asinh_expected, tmp), 0);
        ASSERT_EQ_INT(mf_log(asinh_expected), 0);
        ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
        ASSERT_EQ_INT(mf_asinh(c), 0);
        print_mfloat_value("c after asinh", c);
        print_double_check("asinh(0.5)", "0.5", asinh(0.5), mf_to_double(c));
        print_mfloat_error_check_value("asinh(0.5) mfloat error", c, asinh_expected);
        ASSERT_TRUE(mfloat_meets_precision_value(c, asinh_expected, 1));
        ASSERT_TRUE(fabs(mf_to_double(c) - asinh(0.5)) < 1e-9);
        mf_free(tmp);
        mf_free(asinh_expected);
    }

    {
        mfloat_t *acosh_expected = mf_create_string("2");
        mfloat_t *tmp = mf_create_string("2");
        ASSERT_NOT_NULL(acosh_expected);
        ASSERT_NOT_NULL(tmp);
        ASSERT_EQ_INT(mf_mul(tmp, tmp), 0);
        ASSERT_EQ_INT(mf_sub(tmp, MF_ONE), 0);
        ASSERT_EQ_INT(mf_sqrt(tmp), 0);
        ASSERT_EQ_INT(mf_add(acosh_expected, tmp), 0);
        ASSERT_EQ_INT(mf_log(acosh_expected), 0);
        ASSERT_EQ_INT(mf_set_string(c, "2"), 0);
        ASSERT_EQ_INT(mf_acosh(c), 0);
        print_mfloat_value("c after acosh", c);
        print_double_check("acosh(2)", "2", acosh(2.0), mf_to_double(c));
        print_mfloat_error_check_value("acosh(2) mfloat error", c, acosh_expected);
        ASSERT_TRUE(mfloat_meets_precision_value(c, acosh_expected, 1));
        ASSERT_TRUE(fabs(mf_to_double(c) - acosh(2.0)) < 1e-9);
        mf_free(tmp);
        mf_free(acosh_expected);
    }

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atanh(c), 0);
    print_mfloat_value("c after atanh", c);
    print_double_check("atanh(0.5)", "0.5", atanh(0.5), mf_to_double(c));
    print_mfloat_error_check("atanh(0.5) mfloat error", c,
                             "0.5493061443340548456976226184612628523237452789113747258673471668187471466093044834368078774068660443939850145329789328711840021129652599105264009353836387053015813845916906835896868494221804799518712851583979557605727959588753356733");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.5493061443340548456976226184612628523237452789113747258673471668187471466093044834368078774068660443939850145329789328711840021129652599105264009353836387053015813845916906835896868494221804799518712851583979557605727959588753356733",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - atanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_gammainc_P(f, MF_ONE), 0);
    print_mfloat_value("f after gammainc_P(f, 1)", f);
    print_double_check("gammainc_P(1,1)", "(1,1)", 1.0 - exp(-1.0), mf_to_double(f));
    print_mfloat_error_check("gammainc_P(1,1) mfloat error", f,
                             "0.6321205588285576784044762298385391325541888689682321654921631983025385042551001966428527256540803562533726747231560047917530242072098709913733464105059012169078056326226618849513610088748543836550122800213155240420602526974501075048");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.6321205588285576784044762298385391325541888689682321654921631983025385042551001966428527256540803562533726747231560047917530242072098709913733464105059012169078056326226618849513610088748543836550122800213155240420602526974501075048",
        1));
    ASSERT_TRUE(fabs(mf_to_double(f) - (1.0 - exp(-1.0))) < 1e-12);

    g = mf_pow10(3);
    ASSERT_NOT_NULL(g);
    sa = mf_to_string(g);
    ASSERT_NOT_NULL(sa);
    print_string_check("mf_pow10(3)", "3", "1000", sa);
    print_mfloat_error_check_decimal("mf_pow10(3) mfloat error", g, "1000");
    ASSERT_TRUE(mfloat_meets_precision(g, "1000", 0));
    ASSERT_TRUE(strcmp(sa, "1000") == 0);

    ASSERT_EQ_INT(mf_set_string(c, "1"), 0);
    ASSERT_EQ_INT(mf_atan2(c, k), 0);
    print_mfloat_value("atan2(1, -1)", c);
    print_mfloat_error_check("atan2(1, -1) mfloat error", c,
                             "2.356194490192344928846982537459627163147877049531329365731208444230862304714656748971026119006587800986611064884961729985320383457162936673794019556096360838087713077026453890829169733467211716197786473321608231749450084596356736175");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "2.356194490192344928846982537459627163147877049531329365731208444230862304714656748971026119006587800986611064884961729985320383457162936673794019556096360838087713077026453890829169733467211716197786473321608231749450084596356736175",
        1));

    ASSERT_EQ_INT(mf_gamma(i), 0);
    print_mfloat_value("gamma(5)", i);
    print_mfloat_error_check_decimal("gamma(5) mfloat error", i, "24");
    ASSERT_TRUE(mfloat_meets_precision(i, "24", 0));

    ASSERT_EQ_INT(mf_set_long(i, 5), 0);
    ASSERT_EQ_INT(mf_lgamma(i), 0);
    print_mfloat_value("lgamma(5)", i);
    print_mfloat_error_check("lgamma(5) mfloat error", i,
                             "3.178053830347945619646941601297055408873990960903515214096734362117675159127693113691205735802988151413974472127669922943424564933204911492150814125672505296395345481668495672750294814716035980317112620662889405386862953343268716072");
    ASSERT_TRUE(mfloat_meets_precision(
        i,
        "3.178053830347945619646941601297055408873990960903515214096734362117675159127693113691205735802988151413974472127669922943424564933204911492150814125672505296395345481668495672750294814716035980317112620662889405386862953343268716072",
        1));

    ASSERT_EQ_INT(mf_set_string(i, "2.3"), 0);
    ASSERT_EQ_INT(mf_gamma(i), 0);
    print_mfloat_value("gamma(2.3)", i);
    print_mfloat_error_check("gamma(2.3) mfloat error", i,
                             "1.166711905198160345041881441202917938533994349719468893970206663872991619471764885016201381497483720514355994462195388726523172823373811003654979496752728641752819561490433843500680095943047859953908745889592053158605845700529554192");
    ASSERT_TRUE(mfloat_meets_precision(
        i,
        "1.166711905198160345041881441202917938533994349719468893970206663872991619471764885016201381497483720514355994462195388726523172823373811003654979496752728641752819561490433843500680095943047859953908745889592053158605845700529554192",
        1));

    ASSERT_EQ_INT(mf_set_string(i, "2.3"), 0);
    ASSERT_EQ_INT(mf_lgamma(i), 0);
    print_mfloat_value("lgamma(2.3)", i);
    print_mfloat_error_check("lgamma(2.3) mfloat error", i,
                             "0.1541894549596305810899179114892231726957039760896140227257076855640685769192125200531293229454353986031593187079497994109444537204417467876934474353904068871981327164639869236486019795955478385710122972590070620576848506585504604812");
    expected_calc = mf_new_prec(mf_get_precision(i));
    ASSERT_NOT_NULL(expected_calc);
    ASSERT_EQ_INT(mf_set_string(expected_calc,
                                "0.1541894549596305810899179114892231726957039760896140227257076855640685769192125200531293229454353986031593187079497994109444537204417467876934474353904068871981327164639869236486019795955478385710122972590070620576848506585504604812"),
                  0);
    ASSERT_TRUE(mfloat_matches_expected(i, expected_calc, 1));
    mf_free(expected_calc);
    expected_calc = NULL;

    ASSERT_EQ_INT(mf_digamma(j), 0);
    print_mfloat_value("digamma(1)", j);
    print_mfloat_error_check("digamma(1) mfloat error", j,
                             "-0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467495146314472498070824809605040144865428362241739976449235362535003337429373377376739427925952582470949160087352039481656708532331517767");
    ASSERT_TRUE(mfloat_meets_precision(
        j,
        "-0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467495146314472498070824809605040144865428362241739976449235362535003337429373377376739427925952582470949160087352039481656708532331517767",
        1));

    ASSERT_EQ_INT(mf_set_long(j, 1), 0);
    ASSERT_EQ_INT(mf_trigamma(j), 0);
    print_mfloat_value("trigamma(1)", j);
    print_mfloat_error_check("trigamma(1) mfloat error", j,
                             "1.644934066848226436472415166646025189218949901206798437735558229370007470403200873833628900619758705304004318962337190679628724687005007787935102946330866276831733309367762605095251006872140054796811558794890360823277761919840756456");
    ASSERT_TRUE(mfloat_meets_precision(
        j,
        "1.644934066848226436472415166646025189218949901206798437735558229370007470403200873833628900619758705304004318962337190679628724687005007787935102946330866276831733309367762605095251006872140054796811558794890360823277761919840756456",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_erf(c), 0);
    print_mfloat_value("erf(0.5)", c);
    print_mfloat_error_check("erf(0.5) mfloat error", c,
                             "0.5204998778130465376827466538919645287364515757579637000588057256471935217168535709147882187347877570329661243861943912360654146905908907746062180980250369741700191971118619744616654054410988999999999999999999999999999999999999999997");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.5204998778130465376827466538919645287364515757579637000588057256471935217168535709147882187347877570329661243861943912360654146905908907746062180980250369741700191971118619744616654054410988999999999999999999999999999999999999999997",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_erfc(c), 0);
    print_mfloat_value("erfc(0.5)", c);
    print_mfloat_error_check("erfc(0.5) mfloat error", c,
                             "0.4795001221869534623172533461080354712635484242420362999411942743528064782831464290852117812652122429670338756138056087639345853094091092253937819019749630258299808028881380255383345945589011");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.4795001221869534623172533461080354712635484242420362999411942743528064782831464290852117812652122429670338756138056087639345853094091092253937819019749630258299808028881380255383345945589011",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "1"), 0);
    ASSERT_EQ_INT(mf_lambert_w0(c), 0);
    print_mfloat_value("lambert_w0(1)", c);
    print_mfloat_error_check("lambert_w0(1) mfloat error", c,
                             "0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522945576380249728667897854523584659400729956085164392899946143115714929598");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.5671432904097838729999686622103555497538157871865125081351310792230457930866845666932194469617522945576380249728667897854523584659400729956085164392899946143115714929598",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0"), 0);
    ASSERT_EQ_INT(mf_normal_cdf(c), 0);
    print_mfloat_value("normal_cdf(0)", c);
    print_mfloat_error_check_decimal("normal_cdf(0) mfloat error", c, "0.5");
    ASSERT_TRUE(mfloat_meets_precision(c, "0.5", 0));

    ASSERT_EQ_INT(mf_set_long(f, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_Q(f, MF_ONE), 0);
    print_mfloat_value("gammainc_Q(1,1)", f);
    print_mfloat_error_check("gammainc_Q(1,1) mfloat error", f,
                             "0.3678794411714423215955237701614608674458111310317678345078368016974614957448998033571472743459196437466273252768439952082469757927901290086266535894940987830921943673773381150486389911251456163449877199786844759579397473025498924955");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.3678794411714423215955237701614608674458111310317678345078368016974614957448998033571472743459196437466273252768439952082469757927901290086266535894940987830921943673773381150486389911251456163449877199786844759579397473025498924955",
        1));

    free(sa);
    mf_free(a);
    mf_free(b);
    mf_free(c);
    mf_free(d);
    mf_free(e);
    mf_free(f);
    mf_free(i);
    mf_free(j);
    mf_free(k);
    mf_free(h);
    mf_free(p);
    mf_free(sin_pair);
    mf_free(cos_pair);
    mf_free(sinh_pair);
    mf_free(cosh_pair);
    mf_free(expected_e);
    mf_free(expected_calc);
    mf_free(g);
    ASSERT_EQ_INT(mf_set_default_precision(saved_default), 0);
}

void test_remaining_special_mfloat_functions(void)
{
    size_t saved_default = mf_get_default_precision();
    mfloat_t *x = NULL;
    mfloat_t *a = NULL;
    mfloat_t *b = NULL;
    mfloat_t *y = NULL;
    mfloat_t *neg = NULL;
    mfloat_t *one = NULL;
    mfloat_t *expected_calc = NULL;

    ASSERT_EQ_INT(mf_set_default_precision(TEST_MFLOAT_MATHS_PRECISION), 0);
    x = mf_create_string("0.5");
    a = mf_create_string("2.5");
    b = mf_create_string("3.5");
    y = mf_create_string("3");
    neg = mf_create_string("-0.1");
    one = mf_create_long(1);

    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(y);
    ASSERT_NOT_NULL(neg);
    ASSERT_NOT_NULL(one);

    ASSERT_EQ_INT(mf_erfinv(x), 0);
    print_mfloat_value("erfinv(0.5)", x);
    print_mfloat_error_check("erfinv(0.5) mfloat error", x,
                             "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_erfcinv(x), 0);
    print_mfloat_value("erfcinv(0.5)", x);
    print_mfloat_error_check("erfcinv(0.5) mfloat error", x,
                             "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.4769362762044698733814183536431305598089697490594706447038826959193834477746467334886959158699890099480330386734708686181554200754487317906163612464948982829944987416840339015704779831127126",
        1));

    ASSERT_EQ_INT(mf_tetragamma(one), 0);
    print_mfloat_value("tetragamma(1)", one);
    print_mfloat_error_check("tetragamma(1) mfloat error", one,
                             "-2.4041138063191885707994763230228999815299725846809977635845431106836764115726261803729117472186705162923983155905214388369839919973465664275527936744158003229078835658987");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "-2.4041138063191885707994763230228999815299725846809977635845431106836764115726261803729117472186705162923983155905214388369839919973465664275527936744158003229078835658987",
        1));

    ASSERT_EQ_INT(mf_gammainv(y), 0);
    print_mfloat_value("gammainv(3)", y);
    print_mfloat_error_check("gammainv(3) mfloat error", y,
                             "3.4058699863095669246999292183755580095953957993289813943129416262771480469249543971039352849144736866311861261393708590757492573265835991956905840323578868826789864361657");
    ASSERT_TRUE(mfloat_meets_precision(
        y,
        "3.4058699863095669246999292183755580095953957993289813943129416262771480469249543971039352849144736866311861261393708590757492573265835991956905840323578868826789864361657",
        1));

    ASSERT_EQ_INT(mf_set_string(y, "1.3293403881791370204736256125058588870981620920917903461603558423896834634432742359133252053858306082197751479117489164834722524954275023497889051327665156821461717608501"), 0);
    ASSERT_EQ_INT(mf_gammainv(y), 0);
    print_mfloat_value("gammainv(gamma(2.5))", y);
    print_mfloat_error_check("gammainv(gamma(2.5)) mfloat error", y,
                             "2.5");
    ASSERT_TRUE(mfloat_meets_precision(y, "2.5", 1));

    ASSERT_EQ_INT(mf_lambert_wm1(neg), 0);
    print_mfloat_value("lambert_wm1(-0.1)", neg);
    print_mfloat_error_check("lambert_wm1(-0.1) mfloat error", neg,
                             "-3.577152063957297218409391963511994880401796257793075923683527755791687236350575462861463655620846808017732465627597059470558844569051750534584923541827063499452631656593265232240273452302009544089866198954722805115875488714857591771");
    ASSERT_TRUE(mfloat_meets_precision(
        neg,
        "-3.577152063957297218409391963511994880401796257793075923683527755791687236350575462861463655620846808017732465627597059470558844569051750534584923541827063499452631656593265232240273452302009544089866198954722805115875488714857591771",
        1));

    ASSERT_EQ_INT(mf_logbeta(a, b), 0);
    print_mfloat_value("logbeta(2.5,3.5)", a);
    print_mfloat_error_check("logbeta(2.5,3.5) mfloat error", a,
                             "-3.301835269962052609799184383389828128309215704143981009717122670837516912654122678189667590882127703846032006869909630968067952132211068047929483063503043459566263883177244211440597836787902490076041110975295315326755455855413553872");
    ASSERT_TRUE(mfloat_meets_precision(
        a,
        "-3.301835269962052609799184383389828128309215704143981009717122670837516912654122678189667590882127703846032006869909630968067952132211068047929483063503043459566263883177244211440597836787902490076041110975295315326755455855413553872",
        1));

    ASSERT_EQ_INT(mf_set_string(a, "5.5"), 0);
    ASSERT_EQ_INT(mf_set_string(b, "2.5"), 0);
    ASSERT_EQ_INT(mf_binomial(a, b), 0);
    print_mfloat_value("binomial(5.5,2.5)", a);
    print_mfloat_error_check("binomial(5.5,2.5) mfloat error", a,
                             "14.4375");
    ASSERT_TRUE(mfloat_meets_precision(a, "14.4375", 1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_set_string(a, "2.5"), 0);
    ASSERT_EQ_INT(mf_set_string(b, "3.5"), 0);
    ASSERT_EQ_INT(mf_beta_pdf(x, a, b), 0);
    print_mfloat_value("beta_pdf(0.5,2.5,3.5)", x);
    print_mfloat_error_check("beta_pdf(0.5,2.5,3.5) mfloat error", x,
                             "1.697652726313550248201426809306819861700902887898202119975118336628232508098416374294547229506699583531443655752084885726857726358555505630698319047804798285111892980665696827446162628233869948679652190892985575058062532162578385377");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "1.697652726313550248201426809306819861700902887898202119975118336628232508098416374294547229506699583531443655752084885726857726358555505630698319047804798285111892980665696827446162628233869948679652190892985575058062532162578385377",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_logbeta_pdf(x, a, b), 0);
    print_mfloat_value("logbeta_pdf(0.5,2.5,3.5)", x);
    print_mfloat_error_check("logbeta_pdf(0.5,2.5,3.5) mfloat error", x,
                             "0.529246547722271372130255897557121856007215166702959993234402632863942424775343815766214282896452953678026082787626888033325871189178545819798800056962739611843353599863758470679369682292335796191547710513837323505798973954722827573");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.529246547722271372130255897557121856007215166702959993234402632863942424775343815766214282896452953678026082787626888033325871189178545819798800056962739611843353599863758470679369682292335796191547710513837323505798973954722827573",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_normal_pdf(x), 0);
    print_mfloat_value("normal_pdf(0.5)", x);
    print_mfloat_error_check("normal_pdf(0.5) mfloat error", x,
                             "0.3520653267642994777746804415965176531103151803757119496554690179882231978367166074876695830352912992493563315873991226985732202993620550355837505163524984117606205891629138560900857072618466033122881469544464486893328511817392660585");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.3520653267642994777746804415965176531103151803757119496554690179882231978367166074876695830352912992493563315873991226985732202993620550355837505163524984117606205891629138560900857072618466033122881469544464486893328511817392660585",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_normal_logpdf(x), 0);
    print_mfloat_value("normal_logpdf(0.5)", x);
    print_mfloat_error_check("normal_logpdf(0.5) mfloat error", x,
                             "-1.043938533204672741780329736405617639861397473637783412817151540482765695927260397694743298635954197622005646624634337446366862881840793572155875915222681393603560742547358669046395905991380805630163234873094627374625518251694954477");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "-1.043938533204672741780329736405617639861397473637783412817151540482765695927260397694743298635954197622005646624634337446366862881840793572155875915222681393603560742547358669046395905991380805630163234873094627374625518251694954477",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_lower(one, MF_ONE), 0);
    print_mfloat_value("gammainc_lower(1,1)", one);
    print_mfloat_error_check("gammainc_lower(1,1) mfloat error", one,
                             "0.632120558828557678404476229838539132554188868968232165492163198302538504255100196642852725654080356253372674723156004791753024207209870991373346410505901216907805632622618849513610088748543836550122800213155240420602526974501075048");
    expected_calc = mf_create_long(1);
    ASSERT_NOT_NULL(expected_calc);
    ASSERT_EQ_INT(mf_neg(expected_calc), 0);
    ASSERT_EQ_INT(mf_exp(expected_calc), 0);
    ASSERT_EQ_INT(mf_neg(expected_calc), 0);
    ASSERT_EQ_INT(mf_add_long(expected_calc, 1), 0);
    ASSERT_TRUE(mfloat_matches_expected(one, expected_calc, 1));
    mf_free(expected_calc);
    expected_calc = NULL;

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_upper(one, MF_ONE), 0);
    print_mfloat_value("gammainc_upper(1,1)", one);
    print_mfloat_error_check("gammainc_upper(1,1) mfloat error", one,
                             "0.367879441171442321595523770161460867445811131031767834507836801697461495744899803357147274345919643746627325276843995208246975792790129008626653589494098783092194367377381150486389911251456163449877199786844759579397473025498924955");
    expected_calc = mf_create_long(1);
    ASSERT_NOT_NULL(expected_calc);
    ASSERT_EQ_INT(mf_neg(expected_calc), 0);
    ASSERT_EQ_INT(mf_exp(expected_calc), 0);
    ASSERT_TRUE(mf_eq(one, expected_calc));
    mf_free(expected_calc);
    expected_calc = NULL;

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_ei(one), 0);
    print_mfloat_value("ei(1)", one);
    print_mfloat_error_check("ei(1) mfloat error", one,
                             "1.895117816355936755466520934331634269017060581732707591646228431882513834533804153548900710126138956971811095317944653742588149164163064688088186682538828669632338545095227555258481392212166459936359948543306285455761625228166868119");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "1.895117816355936755466520934331634269017060581732707591646228431882513834533804153548900710126138956971811095317944653742588149164163064688088186682538828669632338545095227555258481392212166459936359948543306285455761625228166868119",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_e1(one), 0);
    print_mfloat_value("e1(1)", one);
    print_mfloat_error_check("e1(1) mfloat error", one,
                             "0.2193839343955202736771637754601216490310472934069082075779786130735686985591415447222102510351372499547582346308741095901763785205370960099567044878767774129313472607957338658928051397881295371811343600593450128247655854623683249695");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.2193839343955202736771637754601216490310472934069082075779786130735686985591415447222102510351372499547582346308741095901763785205370960099567044878767774129313472607957338658928051397881295371811343600593450128247655854623683249695",
        1));

    mf_free(x);
    mf_free(a);
    mf_free(b);
    mf_free(y);
    mf_free(neg);
    mf_free(one);
    mf_free(expected_calc);
    ASSERT_EQ_INT(mf_set_default_precision(saved_default), 0);
}

void readme_examples(void)
{
    size_t saved_default = mf_get_default_precision();
    mfloat_t *x = NULL;
    mfloat_t *y = NULL;
    char buf[256];

    mf_set_default_precision(256u);
    x = mf_create_string("2.3");
    y = mf_create_string("2.3");

    if (x && mf_gamma(x) == 0 && mf_sprintf(buf, sizeof(buf), "%.77mf", x) >= 0)
        printf("gamma(2.3)  = %s\n", buf);

    if (y && mf_lgamma(y) == 0 && mf_sprintf(buf, sizeof(buf), "%.77mf", y) >= 0)
        printf("lgamma(2.3) = %s\n", buf);

    mf_free(x);
    mf_free(y);
    mf_set_default_precision(saved_default);
}

void test_difficult_mfloat_cases(void)
{
    mfloat_t *x = NULL;
    mfloat_t *lhs = NULL;
    mfloat_t *rhs = NULL;
    mfloat_t *tmp = NULL;

    x = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    lhs = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    rhs = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    tmp = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(rhs);
    ASSERT_NOT_NULL(tmp);

    ASSERT_EQ_INT(mf_set_string(lhs, "3.3"), 0);
    ASSERT_EQ_INT(mf_lgamma(lhs), 0);
    ASSERT_EQ_INT(mf_set_string(rhs, "2.3"), 0);
    ASSERT_EQ_INT(mf_lgamma(rhs), 0);
    ASSERT_EQ_INT(mf_set_string(tmp, "2.3"), 0);
    ASSERT_EQ_INT(mf_log(tmp), 0);
    ASSERT_EQ_INT(mf_sub(lhs, rhs), 0);
    ASSERT_EQ_INT(mf_sub(lhs, tmp), 0);
    ASSERT_EQ_INT(mf_abs(lhs), 0);
    print_mfloat_error_check("lgamma(3.3) - lgamma(2.3) - log(2.3) mfloat error", lhs, "0");
    ASSERT_TRUE(mfloat_meets_precision(lhs, "0", 0));

    ASSERT_EQ_INT(mf_set_string(x, "1e-20"), 0);
    ASSERT_EQ_INT(mf_log(x), 0);
    ASSERT_EQ_INT(mf_exp(x), 0);
    print_mfloat_error_check_decimal("exp(log(1e-20)) mfloat error", x, "1e-20");
    ASSERT_TRUE(mfloat_meets_precision(x, "1e-20", 1));

    ASSERT_EQ_INT(mf_set_string(lhs, "0.5"), 0);
    ASSERT_EQ_INT(mf_set_string(rhs, "1"), 0);
    ASSERT_EQ_INT(mf_gammainc_P(lhs, rhs), 0);
    ASSERT_EQ_INT(mf_set_string(tmp, "0.5"), 0);
    ASSERT_EQ_INT(mf_set_string(x, "1"), 0);
    ASSERT_EQ_INT(mf_gammainc_Q(tmp, x), 0);
    ASSERT_EQ_INT(mf_add(lhs, tmp), 0);
    print_mfloat_error_check("gammainc_P(0.5,1) + gammainc_Q(0.5,1) - 1 mfloat error", lhs, "1");
    ASSERT_TRUE(mfloat_meets_precision(lhs, "1", 1));

    ASSERT_EQ_INT(mf_set_string(x, "-0.35"), 0);
    ASSERT_EQ_INT(mf_productlog(x), 0);
    ASSERT_EQ_INT(mf_set_string(lhs, "-0.35"), 0);
    mf_free(rhs);
    rhs = mf_clone(x);
    ASSERT_NOT_NULL(rhs);
    ASSERT_EQ_INT(mf_exp(rhs), 0);
    ASSERT_EQ_INT(mf_mul(rhs, x), 0);
    ASSERT_EQ_INT(mf_sub(rhs, lhs), 0);
    ASSERT_EQ_INT(mf_abs(rhs), 0);
    print_mfloat_error_check("productlog(-0.35) * exp(productlog(-0.35)) - (-0.35) mfloat error", rhs, "0");
    ASSERT_TRUE(mfloat_meets_precision(rhs, "0", 0));

    ASSERT_EQ_INT(mf_set_string(lhs, "2.5"), 0);
    ASSERT_EQ_INT(mf_set_string(rhs, "3.5"), 0);
    ASSERT_EQ_INT(mf_logbeta(lhs, rhs), 0);
    ASSERT_EQ_INT(mf_exp(lhs), 0);
    ASSERT_EQ_INT(mf_set_string(rhs, "2.5"), 0);
    ASSERT_EQ_INT(mf_set_string(tmp, "3.5"), 0);
    ASSERT_EQ_INT(mf_beta(rhs, tmp), 0);
    ASSERT_EQ_INT(mf_sub(lhs, rhs), 0);
    ASSERT_EQ_INT(mf_abs(lhs), 0);
    print_mfloat_error_check("exp(logbeta(2.5,3.5)) - beta(2.5,3.5) mfloat error", lhs, "0");
    ASSERT_TRUE(mfloat_meets_precision(lhs, "0", 0));

    mf_free(x);
    mf_free(lhs);
    mf_free(rhs);
    mf_free(tmp);
}

int tests_main(void)
{
    RUN_TEST(test_new_and_precision, NULL);
    RUN_TEST(test_set_long_normalises, NULL);
    RUN_TEST(test_clone_and_clear, NULL);
    RUN_TEST(test_set_string_and_basic_arithmetic, NULL);
    RUN_TEST(test_division_and_power, NULL);
    RUN_TEST(test_string_roundtrip, NULL);
    RUN_TEST(test_printf_family, NULL);
    RUN_TEST(test_constants_and_named_values, NULL);
    RUN_TEST(test_conversion_to_double_and_qfloat, NULL);
    RUN_TEST(test_conversion_from_double_and_qfloat, NULL);
    RUN_TEST(test_extended_math_wrappers, NULL);
    RUN_TEST(test_remaining_special_mfloat_functions, NULL);
    RUN_TEST(test_difficult_mfloat_cases, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    readme_examples();

    return 0;
}
