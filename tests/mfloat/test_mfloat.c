#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "mfloat.h"

#ifndef TEST_MFLOAT_MATHS_PRECISION
/* Keep maths-only precision configurable without affecting core object tests. */
#define TEST_MFLOAT_MATHS_PRECISION 1024u
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
static int mfloat_matches_expected_bits(const mfloat_t *got,
                                        const mfloat_t *expected,
                                        int relative_mode,
                                        size_t precision_bits);
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
    if (precision_bits > 1u)
        precision_bits -= 1u;
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
    if (!got)
        return 0;
    return mfloat_matches_expected_bits(got, expected, relative_mode, mf_get_precision(got));
}

static int mfloat_matches_expected_bits(const mfloat_t *got,
                                        const mfloat_t *expected,
                                        int relative_mode,
                                        size_t precision_bits)
{
    mfloat_t *error = NULL;
    mfloat_t *tol = NULL;
    long tol_exp2;
    int ok = 0;

    if (!got || !expected)
        return 0;

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
                             "2.71828182845904523536028747135266249775724709369995957496696762772407663035354759457138217852516642742746639193200305992181741359662904357290033429526059563073813232862794349076323382988075319525101901157383418793070215408914993488416750924");
    ASSERT_TRUE(mfloat_meets_precision(
        a,
        "2.71828182845904523536028747135266249775724709369995957496696762772407663035354759457138217852516642742746639193200305992181741359662904357290033429526059563073813232862794349076323382988075319525101901157383418793070215408914993488416750924",
        1));
    ASSERT_TRUE(fabs(mf_to_double(a) - 2.718281828459045) < 1e-12);
    expected_e = mf_e();
    ASSERT_NOT_NULL(expected_e);
    ASSERT_TRUE(mfloat_matches_expected(a, expected_e, 1));
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
                             "0.479425538604203000273287935215571388081803367940600675188616613125535000287814832209631274684348269086132091084505717417811093748609940282780153962046191924609957293932281400533546338188055228595670135699854233639121071720777380152979871377");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.479425538604203000273287935215571388081803367940600675188616613125535000287814832209631274684348269086132091084505717417811093748609940282780153962046191924609957293932281400533546338188055228595670135699854233639121071720777380152979871377",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sin(0.5)) < 1e-12);
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sincos(c, sin_pair, cos_pair), 0);
    print_mfloat_error_check("sincos(sin, 0.5) mfloat error", sin_pair,
                             "0.479425538604203000273287935215571388081803367940600675188616613125535000287814832209631274684348269086132091084505717417811093748609940282780153962046191924609957293932281400533546338188055228595670135699854233639121071720777380152979871377");
    print_mfloat_error_check("sincos(cos, 0.5) mfloat error", cos_pair,
                             "0.877582561890372716116281582603829651991645197109744052997610868315950763274213947405794184084682258355478400593109053993413827976833280266799756120950224015587629156878590723476939310989616739677014408997649128570213468218384543818393316169");
    ASSERT_TRUE(mfloat_meets_precision(
        sin_pair,
        "0.479425538604203000273287935215571388081803367940600675188616613125535000287814832209631274684348269086132091084505717417811093748609940282780153962046191924609957293932281400533546338188055228595670135699854233639121071720777380152979871377",
        1));
    ASSERT_TRUE(mfloat_meets_precision(
        cos_pair,
        "0.877582561890372716116281582603829651991645197109744052997610868315950763274213947405794184084682258355478400593109053993413827976833280266799756120950224015587629156878590723476939310989616739677014408997649128570213468218384543818393316169",
        1));
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cos(c), 0);
    print_mfloat_value("c after cos", c);
    print_double_check("cos(0.5)", "0.5", cos(0.5), mf_to_double(c));
    print_mfloat_error_check("cos(0.5) mfloat error", c,
                             "0.877582561890372716116281582603829651991645197109744052997610868315950763274213947405794184084682258355478400593109053993413827976833280266799756120950224015587629156878590723476939310989616739677014408997649128570213468218384543818393316169");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.877582561890372716116281582603829651991645197109744052997610868315950763274213947405794184084682258355478400593109053993413827976833280266799756120950224015587629156878590723476939310989616739677014408997649128570213468218384543818393316169",
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
                             "0.546302489843790513255179465780285383297551720179791246164091385932907510518025815715180648270656218589104862600264114265493230091168402843217390929910914216636940743788474268957410401257911756878745999724508918212237750843839160813748299366173416451377715864413140089240189414931448648058650051967435");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.546302489843790513255179465780285383297551720179791246164091385932907510518025815715180648270656218589104862600264114265493230091168402843217390929910914216636940743788474268957410401257911756878745999724508918212237750843839160813748299366173416451377715864413140089240189414931448648058650051967435",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sinh(c), 0);
    print_mfloat_value("c after sinh", c);
    print_double_check("sinh(0.5)", "0.5", sinh(0.5), mf_to_double(c));
    print_mfloat_error_check("sinh(0.5) mfloat error", c,
                             "0.521095305493747361622425626411491559105928982611480527946093576452802250890233592317064454274188593488221423981134135914066679444828331313249895814771191186110920706290777986723716282905794344826240166742832663616998433669072057778674830161");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.521095305493747361622425626411491559105928982611480527946093576452802250890233592317064454274188593488221423981134135914066679444828331313249895814771191186110920706290777986723716282905794344826240166742832663616998433669072057778674830161",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sinh(0.5)) < 1e-9);
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sinhcosh(c, sinh_pair, cosh_pair), 0);
    print_mfloat_error_check("sinhcosh(sinh, 0.5) mfloat error", sinh_pair,
                             "0.521095305493747361622425626411491559105928982611480527946093576452802250890233592317064454274188593488221423981134135914066679444828331313249895814771191186110920706290777986723716282905794344826240166742832663616998433669072057778674830161");
    print_mfloat_error_check("sinhcosh(cosh, 0.5) mfloat error", cosh_pair,
                             "1.127625965206380785226225161402672012547847118098667483628985735187858770303982016315712065782178049514645213775173661090604487530391277846591075637718868610818501952807625927996232181753694900070628738593585802103842632987787742311");
    ASSERT_TRUE(mfloat_meets_precision(
        sinh_pair,
        "0.521095305493747361622425626411491559105928982611480527946093576452802250890233592317064454274188593488221423981134135914066679444828331313249895814771191186110920706290777986723716282905794344826240166742832663616998433669072057778674830161",
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
                             "0.463647609000806116214256231461214402028537054286120263810933088720197864165741705300600283984887892556529852251190837513505818181625011155471530569944105620719336266164880101532502755987925805516853889167478237286538793918012517199484013956");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.463647609000806116214256231461214402028537054286120263810933088720197864165741705300600283984887892556529852251190837513505818181625011155471530569944105620719336266164880101532502755987925805516853889167478237286538793918012517199484013956",
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
                             "0.462117157260009758502318483643672548730289280330113038552731815838080906140409278774949064151962490584348932986281549132882265461869597895957144611615878563329132704166776939197372567930770270037301448608599262409581783611892899146703802769");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.462117157260009758502318483643672548730289280330113038552731815838080906140409278774949064151962490584348932986281549132882265461869597895957144611615878563329132704166776939197372567930770270037301448608599262409581783611892899146703802769",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tanh(0.5)) < 1e-9);

    {
        mfloat_t *asinh_expected = mf_create_string(
            "0.481211825059603447497758913424368423135184334385660519661018168840163867608221774412009429122723474997231839958293656411272568323726737622753059241864409754182417007211837150223823937469187275243279193018797079003561726796944545752305345434188765285532564902073997024822647784061343876976503325242753386755304");
        ASSERT_NOT_NULL(asinh_expected);
        ASSERT_EQ_INT(mf_set_precision(asinh_expected, TEST_MFLOAT_MATHS_PRECISION), 0);
        ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
        ASSERT_EQ_INT(mf_asinh(c), 0);
        print_mfloat_value("c after asinh", c);
        print_double_check("asinh(0.5)", "0.5", asinh(0.5), mf_to_double(c));
        print_mfloat_error_check_value("asinh(0.5) mfloat error", c, asinh_expected);
        ASSERT_TRUE(mfloat_meets_precision_value(c, asinh_expected, 1));
        ASSERT_TRUE(fabs(mf_to_double(c) - asinh(0.5)) < 1e-9);
        mf_free(asinh_expected);
    }

    {
        mfloat_t *acosh_expected = mf_create_string(
            "1.31695789692481670862504634730796844402698197146751647976847225692046018541644397607421901345010178355646543656560497931980981686210637153272676334570992067690583112877625695817047043733686371194095565044679673200082593747537791289042677209263334442156084433067881666631245198970237618535389129661579567435373");
        ASSERT_NOT_NULL(acosh_expected);
        ASSERT_EQ_INT(mf_set_precision(acosh_expected, TEST_MFLOAT_MATHS_PRECISION), 0);
        ASSERT_EQ_INT(mf_set_string(c, "2"), 0);
        ASSERT_EQ_INT(mf_acosh(c), 0);
        print_mfloat_value("c after acosh", c);
        print_double_check("acosh(2)", "2", acosh(2.0), mf_to_double(c));
        print_mfloat_error_check_value("acosh(2) mfloat error", c, acosh_expected);
        ASSERT_TRUE(mfloat_meets_precision_value(c, acosh_expected, 1));
        ASSERT_TRUE(fabs(mf_to_double(c) - acosh(2.0)) < 1e-9);
        mf_free(acosh_expected);
    }

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atanh(c), 0);
    print_mfloat_value("c after atanh", c);
    print_double_check("atanh(0.5)", "0.5", atanh(0.5), mf_to_double(c));
    print_mfloat_error_check("atanh(0.5) mfloat error", c,
                             "0.549306144334054845697622618461262852323745278911374725867347166818747146609304483436807877406866044393985014532978932871184002112965259910526400935383638705301581384591690683589686849422180479951871285158397955760572795958875335673527470083");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.549306144334054845697622618461262852323745278911374725867347166818747146609304483436807877406866044393985014532978932871184002112965259910526400935383638705301581384591690683589686849422180479951871285158397955760572795958875335673527470083",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - atanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_gammainc_P(f, MF_ONE), 0);
    print_mfloat_value("f after gammainc_P(f, 1)", f);
    print_double_check("gammainc_P(1,1)", "(1,1)", 1.0 - exp(-1.0), mf_to_double(f));
    print_mfloat_error_check("gammainc_P(1,1) mfloat error", f,
                             "0.632120558828557678404476229838539132554188868968232165492163198302538504255100196642852725654080356253372674723156004791753024207209870991373346410505901216907805632622661884951361008874854383655012280021315524042060252697450107504546760634");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.632120558828557678404476229838539132554188868968232165492163198302538504255100196642852725654080356253372674723156004791753024207209870991373346410505901216907805632622661884951361008874854383655012280021315524042060252697450107504546760634",
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
                             "1.16671190519816034504188144120291793853399434971946889397020666387299161947176488501620138149748372051435599446219538872652317282337381100365497949675272864175281964803820361131105602436105717058785053760500478346172910833186951627753510112");
    ASSERT_TRUE(mfloat_meets_precision(
        i,
        "1.16671190519816034504188144120291793853399434971946889397020666387299161947176488501620138149748372051435599446219538872652317282337381100365497949675272864175281964803820361131105602436105717058785053760500478346172910833186951627753510112",
        1));

    ASSERT_EQ_INT(mf_set_string(i, "2.3"), 0);
    ASSERT_EQ_INT(mf_lgamma(i), 0);
    print_mfloat_value("lgamma(2.3)", i);
    print_mfloat_error_check("lgamma(2.3) mfloat error", i,
                             "0.154189454959630581089917911489223172695703976089614022725707685564068576919212520053129322945435398603159318707949799410944453720441746787693447435390406887198132790644913150771190560471513700936385240165256577876731081498697313859634303643140983037748511192855353481897134681271350952065398986186677542395203");
    expected_calc = mf_new_prec(mf_get_precision(i));
    ASSERT_NOT_NULL(expected_calc);
    ASSERT_EQ_INT(mf_set_string(expected_calc,
                                "0.154189454959630581089917911489223172695703976089614022725707685564068576919212520053129322945435398603159318707949799410944453720441746787693447435390406887198132790644913150771190560471513700936385240165256577876731081498697313859634303643140983037748511192855353481897134681271350952065398986186677542395203"),
                  0);
    ASSERT_TRUE(mfloat_matches_expected(i, expected_calc, 1));
    mf_free(expected_calc);
    expected_calc = NULL;

    ASSERT_EQ_INT(mf_digamma(j), 0);
    print_mfloat_value("digamma(1)", j);
    print_mfloat_error_check("digamma(1) mfloat error", j,
                             "-0.577215664901532860606512090082402431042159335939923598805767234884867726777664670936947063291746749514631447249807082480960504014486542836224173997644923536253500333742937337737673942792595258247094916008735203948165670853233151776611528621");
    ASSERT_TRUE(mfloat_meets_precision(
        j,
        "-0.577215664901532860606512090082402431042159335939923598805767234884867726777664670936947063291746749514631447249807082480960504014486542836224173997644923536253500333742937337737673942792595258247094916008735203948165670853233151776611528621",
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
                             "0.520499877813046537682746653891964528736451575757963700058805725647193521716853570914788218734787757032966124386194391236065414690590890774606218098025036974170019197111861974461665405441098900000000000000000000000000000000000000000000000000");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.520499877813046537682746653891964528736451575757963700058805725647193521716853570914788218734787757032966124386194391236065414690590890774606218098025036974170019197111861974461665405441098900000000000000000000000000000000000000000000000000",
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
                             "0.367879441171442321595523770161460867445811131031767834507836801697461495744899803357147274345919643746627325276843995208246975792790129008626653589494098783092194367377338115048638991125145616344987719978684475957939747302549892495453239366");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.367879441171442321595523770161460867445811131031767834507836801697461495744899803357147274345919643746627325276843995208246975792790129008626653589494098783092194367377338115048638991125145616344987719978684475957939747302549892495453239366",
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
                             "1.89511781635593675546652093433163426901706058173270759164622843188251383453380415354890071012613895697181109531794465374258814916416306468808818668253882866928411741202607861768033725832315304980062472488761474257394752325542221341934052454");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "1.89511781635593675546652093433163426901706058173270759164622843188251383453380415354890071012613895697181109531794465374258814916416306468808818668253882866928411741202607861768033725832315304980062472488761474257394752325542221341934052454",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_e1(one), 0);
    print_mfloat_value("e1(1)", one);
    print_mfloat_error_check("e1(1) mfloat error", one,
                             "0.219383934395520273677163775460121649031047293406908207577978613073568698559141544722210251035137249954758234630874109590176378520537096009956704487876777412931347260795733865892805139788129537181134360059345012824765585462368324969488073343679827470707634455339786303962657522117753827032411866948006272810955");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.219383934395520273677163775460121649031047293406908207577978613073568698559141544722210251035137249954758234630874109590176378520537096009956704487876777412931347260795733865892805139788129537181134360059345012824765585462368324969488073343679827470707634455339786303962657522117753827032411866948006272810955",
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
    mfloat_t *expected_calc = NULL;

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
    expected_calc = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    ASSERT_NOT_NULL(expected_calc);
    ASSERT_EQ_INT(mf_set_long(expected_calc, 0), 0);
    ASSERT_TRUE(mfloat_matches_expected_bits(lhs, expected_calc, 0, TEST_MFLOAT_MATHS_PRECISION - 8u));
    mf_free(expected_calc);
    expected_calc = NULL;

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
    expected_calc = mf_new_prec(TEST_MFLOAT_MATHS_PRECISION);
    ASSERT_NOT_NULL(expected_calc);
    ASSERT_EQ_INT(mf_set_long(expected_calc, 0), 0);
    ASSERT_TRUE(mfloat_matches_expected_bits(rhs, expected_calc, 0, TEST_MFLOAT_MATHS_PRECISION));
    mf_free(expected_calc);
    expected_calc = NULL;

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
    RUN_TEST_CASE(test_new_and_precision);
    RUN_TEST_CASE(test_set_long_normalises);
    RUN_TEST_CASE(test_clone_and_clear);
    RUN_TEST_CASE(test_set_string_and_basic_arithmetic);
    RUN_TEST_CASE(test_division_and_power);
    RUN_TEST_CASE(test_string_roundtrip);
    RUN_TEST_CASE(test_printf_family);
    RUN_TEST_CASE(test_constants_and_named_values);
    RUN_TEST_CASE(test_conversion_to_double_and_qfloat);
    RUN_TEST_CASE(test_conversion_from_double_and_qfloat);
    RUN_TEST_CASE(test_extended_math_wrappers);
    RUN_TEST_CASE(test_remaining_special_mfloat_functions);
    RUN_TEST_CASE(test_difficult_mfloat_cases);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    readme_examples();

    return 0;
}
