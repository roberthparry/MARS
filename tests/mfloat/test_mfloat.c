#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mfloat.h"

#ifndef TEST_MFLOAT_MATHS_PRECISION
/* Keep maths-only precision configurable without affecting core object tests. */
#define TEST_MFLOAT_MATHS_PRECISION 256u
#endif

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

static int format_mfloat_value(char *buf, size_t buf_size, const mfloat_t *value);

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

static void print_mfloat_error_check(const char *label,
                                     const mfloat_t *got,
                                     const char *expected_text)
{
    mfloat_t *expected = NULL;
    mfloat_t *error = NULL;
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

    expected = mf_create_string(expected_text);
    error = mf_clone(got);
    if (!expected || !error || mf_sub(error, expected) != 0 || mf_abs(error) != 0 ||
        format_mfloat_value(got_buf, sizeof(got_buf), got) < 0 ||
        mf_sprintf(err_buf, sizeof(err_buf), "%.6MF", error) < 0) {
        printf(C_CYAN "%s" C_RESET "\n", label);
        printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
        printf("    expected = %s\n", expected_text);
        printf("    got      = <format-error>\n");
        printf("    error    = <format-error>\n");
        mf_free(expected);
        mf_free(error);
        return;
    }

    printf(C_CYAN "%s" C_RESET "\n", label);
    printf("    precision = %zu bits (~%d digits)\n", precision_bits, decimal_digits);
    printf("    expected = %s\n", expected_text);
    printf("    got      = %s\n", got_buf);
    printf("    error    = %s\n", err_buf);

    mf_free(expected);
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
    long tol_exp2;
    int ok = 0;

    if (!got || !expected_text)
        return 0;

    precision_bits = mf_get_precision(got);
    expected = mf_create_string(expected_text);
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
    mf_free(a);
    a = mf_new();
    ASSERT_NOT_NULL(a);
    print_precision_check("mf_new() after default precision change", "mf_new()", 384, (long)mf_get_precision(a));
    ASSERT_EQ_LONG((long)mf_get_precision(a), 384);

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
    double da, db;
    qfloat_t qa, qb, qinf, qninf, qnan;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
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
    qinf = mf_to_qfloat(MF_INF);
    qninf = mf_to_qfloat(MF_NINF);
    qnan = mf_to_qfloat(MF_NAN);

    print_double_check("mf_to_qfloat(a)", "1.5", 1.5, qf_to_double(qa));
    ASSERT_TRUE(fabs(qf_to_double(qa) - 1.5) < 1e-15);
    print_double_check("mf_to_qfloat(b)", "0.25", 0.25, qf_to_double(qb));
    ASSERT_TRUE(fabs(qf_to_double(qb) - 0.25) < 1e-15);
    ASSERT_TRUE(qf_isposinf(qinf));
    ASSERT_TRUE(qf_isneginf(qninf));
    ASSERT_TRUE(qf_isnan(qnan));

    mf_free(a);
    mf_free(b);
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
    mfloat_t *expected_e = NULL;
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
                             "2.7182818284590452353602874713526624977572470936999595749669676277240766303535475945713821785251664274274663919320030599218174135966290435729003342952605956307381323286279");
    ASSERT_TRUE(mfloat_meets_precision(
        a,
        "2.7182818284590452353602874713526624977572470936999595749669676277240766303535475945713821785251664274274663919320030599218174135966290435729003342952605956307381323286279",
        1));
    ASSERT_TRUE(fabs(mf_to_double(a) - 2.718281828459045) < 1e-12);
    expected_e = mf_e();
    ASSERT_NOT_NULL(expected_e);
    ASSERT_TRUE(mf_eq(a, expected_e));
    ASSERT_EQ_INT(mf_log(a), 0);
    print_mfloat_value("a after log(exp(1))", a);
    print_double_check("log(exp(1))", "exp(1)", 1.0, mf_to_double(a));
    print_mfloat_error_check("log(exp(1)) mfloat error", a, "1");
    ASSERT_TRUE(mfloat_meets_precision(a, "1", 0));
    ASSERT_TRUE(fabs(mf_to_double(a) - 1.0) < 1e-12);

    ASSERT_EQ_INT(mf_sqrt(b), 0);
    print_mfloat_value("b after sqrt", b);
    print_double_check("sqrt(4)", "4", 2.0, mf_to_double(b));
    print_mfloat_error_check("sqrt(4) mfloat error", b, "2");
    ASSERT_TRUE(mfloat_meets_precision(b, "2", 0));
    ASSERT_TRUE(fabs(mf_to_double(b) - 2.0) < 1e-12);

    ASSERT_EQ_INT(mf_sin(c), 0);
    print_mfloat_value("c after sin", c);
    print_double_check("sin(0.5)", "0.5", sin(0.5), mf_to_double(c));
    print_mfloat_error_check("sin(0.5) mfloat error", c,
                             "0.47942553860420300027328793521557138808180336794060067518861661312553500028781483220963127468434826908613209108450571741781109374860994028278015396204619192460995729393228");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.47942553860420300027328793521557138808180336794060067518861661312553500028781483220963127468434826908613209108450571741781109374860994028278015396204619192460995729393228",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sin(0.5)) < 1e-12);
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cos(c), 0);
    print_mfloat_value("c after cos", c);
    print_double_check("cos(0.5)", "0.5", cos(0.5), mf_to_double(c));
    print_mfloat_error_check("cos(0.5) mfloat error", c,
                             "0.87758256189037271611628158260382965199164519710974405299761086831595076327421394740579418408468225835547840059310905399341382797683328026679975612095022401558762915687859");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.87758256189037271611628158260382965199164519710974405299761086831595076327421394740579418408468225835547840059310905399341382797683328026679975612095022401558762915687859",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - cos(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_beta(d, e), 0);
    print_mfloat_value("d after beta(d, e)", d);
    print_double_check("beta(2, 3)", "(2,3)", 1.0 / 12.0, mf_to_double(d));
    print_mfloat_error_check("beta(2, 3) mfloat error", d,
                             "0.083333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333");
    ASSERT_TRUE(mfloat_meets_precision(
        d,
        "0.083333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333",
        1));
    ASSERT_TRUE(fabs(mf_to_double(d) - (1.0 / 12.0)) < 1e-12);

    ASSERT_EQ_INT(mf_pow(h, p), 0);
    print_mfloat_value("h after h^p", h);
    print_double_check("2^10", "(2,10)", 1024.0, mf_to_double(h));
    print_mfloat_error_check("2^10 mfloat error", h, "1024");
    ASSERT_TRUE(mfloat_meets_precision(h, "1024", 0));
    ASSERT_TRUE(fabs(mf_to_double(h) - 1024.0) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_tan(c), 0);
    print_mfloat_value("c after tan", c);
    print_double_check("tan(0.5)", "0.5", tan(0.5), mf_to_double(c));
    print_mfloat_error_check("tan(0.5) mfloat error", c,
                             "0.54630248984379051325517946578028538329755172017979124616409138593290751051802581571518064827065621858910486260026411426549323009116840284321739092991091421663694074378847");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.54630248984379051325517946578028538329755172017979124616409138593290751051802581571518064827065621858910486260026411426549323009116840284321739092991091421663694074378847",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sinh(c), 0);
    print_mfloat_value("c after sinh", c);
    print_double_check("sinh(0.5)", "0.5", sinh(0.5), mf_to_double(c));
    print_mfloat_error_check("sinh(0.5) mfloat error", c,
                             "0.52109530549374736162242562641149155910592898261148052794609357645280225089023359231706445427418859348822142398113413591406667944482833131324989581477119118611092070629078");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.52109530549374736162242562641149155910592898261148052794609357645280225089023359231706445427418859348822142398113413591406667944482833131324989581477119118611092070629078",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sinh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cosh(c), 0);
    print_mfloat_value("c after cosh", c);
    print_double_check("cosh(0.5)", "0.5", cosh(0.5), mf_to_double(c));
    print_mfloat_error_check("cosh(0.5) mfloat error", c,
                             "1.1276259652063807852262251614026720125478471180986674836289857351878587703039820163157120657821780495146452137751736610906044875303912778465910756377188686108185019528076");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "1.1276259652063807852262251614026720125478471180986674836289857351878587703039820163157120657821780495146452137751736610906044875303912778465910756377188686108185019528076",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - cosh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atan(c), 0);
    print_mfloat_value("c after atan", c);
    print_double_check("atan(0.5)", "0.5", atan(0.5), mf_to_double(c));
    print_mfloat_error_check("atan(0.5) mfloat error", c,
                             "0.46364760900080611621425623146121440202853705428612026381093308872019786416574170530060028398488789255652985225119083751350581818162501115547153056994410562071933626616488");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.46364760900080611621425623146121440202853705428612026381093308872019786416574170530060028398488789255652985225119083751350581818162501115547153056994410562071933626616488",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - atan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_asin(c), 0);
    print_mfloat_value("c after asin", c);
    print_double_check("asin(0.5)", "0.5", asin(0.5), mf_to_double(c));
    print_mfloat_error_check("asin(0.5) mfloat error", c,
                             "0.52359877559829887307710723054658381403286156656251763682915743205130273438103483310467247089035284466369134775221371777451564076825843037195422656802141351957504735045032");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.52359877559829887307710723054658381403286156656251763682915743205130273438103483310467247089035284466369134775221371777451564076825843037195422656802141351957504735045032",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - asin(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_acos(c), 0);
    print_mfloat_value("c after acos", c);
    print_double_check("acos(0.5)", "0.5", acos(0.5), mf_to_double(c));
    print_mfloat_error_check("acos(0.5) mfloat error", c,
                             "1.0471975511965977461542144610931676280657231331250352736583148641026054687620696662093449417807056893273826955044274355490312815365168607439084531360428270391500947009006");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "1.0471975511965977461542144610931676280657231331250352736583148641026054687620696662093449417807056893273826955044274355490312815365168607439084531360428270391500947009006",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - acos(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_tanh(c), 0);
    print_mfloat_value("c after tanh", c);
    print_double_check("tanh(0.5)", "0.5", tanh(0.5), mf_to_double(c));
    print_mfloat_error_check("tanh(0.5) mfloat error", c,
                             "0.46211715726000975850231848364367254873028928033011303855273181583808090614040927877494906415196249058434893298628154913288226546186959789595714461161587856332913270416678");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.46211715726000975850231848364367254873028928033011303855273181583808090614040927877494906415196249058434893298628154913288226546186959789595714461161587856332913270416678",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_asinh(c), 0);
    print_mfloat_value("c after asinh", c);
    print_double_check("asinh(0.5)", "0.5", asinh(0.5), mf_to_double(c));
    print_mfloat_error_check("asinh(0.5) mfloat error", c,
                             "0.48121182505960344749775891342436842313518433438566051966101816884016386760822177441200942912272347499723183995829365641127256832372673762275305924186440975418241700721184");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.48121182505960344749775891342436842313518433438566051966101816884016386760822177441200942912272347499723183995829365641127256832372673762275305924186440975418241700721184",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - asinh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "2"), 0);
    ASSERT_EQ_INT(mf_acosh(c), 0);
    print_mfloat_value("c after acosh", c);
    print_double_check("acosh(2)", "2", acosh(2.0), mf_to_double(c));
    print_mfloat_error_check("acosh(2) mfloat error", c,
                             "1.3169578969248167086250463473079684440269819714675164797684722569204601854164439760742190134501017835564654365656049793198098168621063715327267633457099206769058311287763");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "1.3169578969248167086250463473079684440269819714675164797684722569204601854164439760742190134501017835564654365656049793198098168621063715327267633457099206769058311287763",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - acosh(2.0)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atanh(c), 0);
    print_mfloat_value("c after atanh", c);
    print_double_check("atanh(0.5)", "0.5", atanh(0.5), mf_to_double(c));
    print_mfloat_error_check("atanh(0.5) mfloat error", c,
                             "0.54930614433405484569762261846126285232374527891137472586734716681874714660930448343680787740686604439398501453297893287118400211296525991052640093538363870530158138459169");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.54930614433405484569762261846126285232374527891137472586734716681874714660930448343680787740686604439398501453297893287118400211296525991052640093538363870530158138459169",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - atanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_gammainc_P(f, MF_ONE), 0);
    print_mfloat_value("f after gammainc_P(f, 1)", f);
    print_double_check("gammainc_P(1,1)", "(1,1)", 1.0 - exp(-1.0), mf_to_double(f));
    print_mfloat_error_check("gammainc_P(1,1) mfloat error", f,
                             "0.63212055882855767840447622983853913255418886896823216549216319830253850425510019664285272565408035625337267472315600479175302420720987099137334641050590121690780563262266");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.63212055882855767840447622983853913255418886896823216549216319830253850425510019664285272565408035625337267472315600479175302420720987099137334641050590121690780563262266",
        1));
    ASSERT_TRUE(fabs(mf_to_double(f) - (1.0 - exp(-1.0))) < 1e-12);

    g = mf_pow10(3);
    ASSERT_NOT_NULL(g);
    sa = mf_to_string(g);
    ASSERT_NOT_NULL(sa);
    print_string_check("mf_pow10(3)", "3", "1000", sa);
    print_mfloat_error_check("mf_pow10(3) mfloat error", g, "1000");
    ASSERT_TRUE(mfloat_meets_precision(g, "1000", 0));
    ASSERT_TRUE(strcmp(sa, "1000") == 0);

    ASSERT_EQ_INT(mf_set_string(c, "1"), 0);
    ASSERT_EQ_INT(mf_atan2(c, k), 0);
    print_mfloat_value("atan2(1, -1)", c);
    print_mfloat_error_check("atan2(1, -1) mfloat error", c,
                             "2.3561944901923449288469825374596271631478770495313293657312084442308623047146567489710261190065878009866110648849617299853203834571629366737940195560963608380877130770265");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "2.3561944901923449288469825374596271631478770495313293657312084442308623047146567489710261190065878009866110648849617299853203834571629366737940195560963608380877130770265",
        1));

    ASSERT_EQ_INT(mf_gamma(i), 0);
    print_mfloat_value("gamma(5)", i);
    print_mfloat_error_check("gamma(5) mfloat error", i, "24");
    ASSERT_TRUE(mfloat_meets_precision(i, "24", 0));

    ASSERT_EQ_INT(mf_set_long(i, 5), 0);
    ASSERT_EQ_INT(mf_lgamma(i), 0);
    print_mfloat_value("lgamma(5)", i);
    print_mfloat_error_check("lgamma(5) mfloat error", i,
                             "3.1780538303479456196469416012970554088739909609035152140967343621176751591276931136912057358029881514139744721276699229434245649332049114921508141256725052963953454816685");
    ASSERT_TRUE(mfloat_meets_precision(
        i,
        "3.1780538303479456196469416012970554088739909609035152140967343621176751591276931136912057358029881514139744721276699229434245649332049114921508141256725052963953454816685",
        1));

    ASSERT_EQ_INT(mf_set_string(i, "2.3"), 0);
    ASSERT_EQ_INT(mf_gamma(i), 0);
    print_mfloat_value("gamma(2.3)", i);
    print_mfloat_error_check("gamma(2.3) mfloat error", i,
                             "1.1667119051981603450418814412029179385339943497194688939702066638729916194717648850162013814974837205143559944621953887265231728233738110036549794967527286417528196480382");
    ASSERT_TRUE(mfloat_meets_precision(
        i,
        "1.1667119051981603450418814412029179385339943497194688939702066638729916194717648850162013814974837205143559944621953887265231728233738110036549794967527286417528196480382",
        1));

    ASSERT_EQ_INT(mf_set_string(i, "2.3"), 0);
    ASSERT_EQ_INT(mf_lgamma(i), 0);
    print_mfloat_value("lgamma(2.3)", i);
    print_mfloat_error_check("lgamma(2.3) mfloat error", i,
                             "0.15418945495963058108991791148922317269570397608961402272570768556406857691921252005312932294543539860315931870794979941094445372044174678769344743539040688719813279064491");
    ASSERT_TRUE(mfloat_meets_precision(
        i,
        "0.15418945495963058108991791148922317269570397608961402272570768556406857691921252005312932294543539860315931870794979941094445372044174678769344743539040688719813279064491",
        1));

    ASSERT_EQ_INT(mf_digamma(j), 0);
    print_mfloat_value("digamma(1)", j);
    print_mfloat_error_check("digamma(1) mfloat error", j,
                             "-0.57721566490153286060651209008240243104215933593992359880576723488486772677766467093694706329174674951463144724980708248096050401448654283622417399764492353625350033374294");
    ASSERT_TRUE(mfloat_meets_precision(
        j,
        "-0.57721566490153286060651209008240243104215933593992359880576723488486772677766467093694706329174674951463144724980708248096050401448654283622417399764492353625350033374294",
        1));

    ASSERT_EQ_INT(mf_set_long(j, 1), 0);
    ASSERT_EQ_INT(mf_trigamma(j), 0);
    print_mfloat_value("trigamma(1)", j);
    print_mfloat_error_check("trigamma(1) mfloat error", j,
                             "1.6449340668482264364724151666460251892189499012067984377355582293700074704032008738336289006197587053040043189623371906796287246870050077879351029463308662768317333093678");
    ASSERT_TRUE(mfloat_meets_precision(
        j,
        "1.6449340668482264364724151666460251892189499012067984377355582293700074704032008738336289006197587053040043189623371906796287246870050077879351029463308662768317333093678",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_erf(c), 0);
    print_mfloat_value("erf(0.5)", c);
    print_mfloat_error_check("erf(0.5) mfloat error", c,
                             "0.52049987781304653768274665389196452873645157575796370005880572564719352171685357091478821873478775703296612438619439123606541469059089077460621809802503697417001919711186");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.52049987781304653768274665389196452873645157575796370005880572564719352171685357091478821873478775703296612438619439123606541469059089077460621809802503697417001919711186",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_erfc(c), 0);
    print_mfloat_value("erfc(0.5)", c);
    print_mfloat_error_check("erfc(0.5) mfloat error", c,
                             "0.47950012218695346231725334610803547126354842424203629994119427435280647828314642908521178126521224296703387561380560876393458530940910922539378190197496302582998080288814");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.47950012218695346231725334610803547126354842424203629994119427435280647828314642908521178126521224296703387561380560876393458530940910922539378190197496302582998080288814",
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
    print_mfloat_error_check("normal_cdf(0) mfloat error", c, "0.5");
    ASSERT_TRUE(mfloat_meets_precision(c, "0.5", 0));

    ASSERT_EQ_INT(mf_set_long(f, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_Q(f, MF_ONE), 0);
    print_mfloat_value("gammainc_Q(1,1)", f);
    print_mfloat_error_check("gammainc_Q(1,1) mfloat error", f,
                             "0.36787944117144232159552377016146086744581113103176783450783680169746149574489980335714727434591964374662732527684399520824697579279012900862665358949409878309219436737734");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.36787944117144232159552377016146086744581113103176783450783680169746149574489980335714727434591964374662732527684399520824697579279012900862665358949409878309219436737734",
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
    mf_free(expected_e);
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
                             "0.47693627620446987338141835364313055980896974905947064470388269591938344777464673348869591586998900994803303867347086861815542007544873179061636124649489828299449874168403");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.47693627620446987338141835364313055980896974905947064470388269591938344777464673348869591586998900994803303867347086861815542007544873179061636124649489828299449874168403",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_erfcinv(x), 0);
    print_mfloat_value("erfcinv(0.5)", x);
    print_mfloat_error_check("erfcinv(0.5) mfloat error", x,
                             "0.47693627620446987338141835364313055980896974905947064470388269591938344777464673348869591586998900994803303867347086861815542007544873179061636124649489828299449874168403");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.47693627620446987338141835364313055980896974905947064470388269591938344777464673348869591586998900994803303867347086861815542007544873179061636124649489828299449874168403",
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

    ASSERT_EQ_INT(mf_lambert_wm1(neg), 0);
    print_mfloat_value("lambert_wm1(-0.1)", neg);
    print_mfloat_error_check("lambert_wm1(-0.1) mfloat error", neg,
                             "-3.5771520639572972184093919635119948804017962577930759236835277557916872363505754628614636556208468080177324656275970594705588445690517505345849235418270634994526316565933");
    ASSERT_TRUE(mfloat_meets_precision(
        neg,
        "-3.5771520639572972184093919635119948804017962577930759236835277557916872363505754628614636556208468080177324656275970594705588445690517505345849235418270634994526316565933",
        1));

    ASSERT_EQ_INT(mf_logbeta(a, b), 0);
    print_mfloat_value("logbeta(2.5,3.5)", a);
    print_mfloat_error_check("logbeta(2.5,3.5) mfloat error", a,
                             "-3.3018352699620526097991843833898281283092157041439810097171226708375169126541226781896675908821277038460320068699096309680679521322110680479294830635030434595662638831772");
    ASSERT_TRUE(mfloat_meets_precision(
        a,
        "-3.3018352699620526097991843833898281283092157041439810097171226708375169126541226781896675908821277038460320068699096309680679521322110680479294830635030434595662638831772",
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
                             "1.6976527263135502482014268093068198617009028878982021199751183366282325080984163742945472295066995835314436557520848857268577263585555056306983190478047982851118929806657");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "1.6976527263135502482014268093068198617009028878982021199751183366282325080984163742945472295066995835314436557520848857268577263585555056306983190478047982851118929806657",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_logbeta_pdf(x, a, b), 0);
    print_mfloat_value("logbeta_pdf(0.5,2.5,3.5)", x);
    print_mfloat_error_check("logbeta_pdf(0.5,2.5,3.5) mfloat error", x,
                             "0.52924654772227137213025589755712185600721516670295999323440263286394242477534381576621428289645295367802608278762688803332587118917854581979880005696273961184335359986376");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.52924654772227137213025589755712185600721516670295999323440263286394242477534381576621428289645295367802608278762688803332587118917854581979880005696273961184335359986376",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_normal_pdf(x), 0);
    print_mfloat_value("normal_pdf(0.5)", x);
    print_mfloat_error_check("normal_pdf(0.5) mfloat error", x,
                             "0.35206532676429947777468044159651765311031518037571194965546901798822319783671660748766958303529129924935633158739912269857322029936205503558375051635249841176062058916291");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.35206532676429947777468044159651765311031518037571194965546901798822319783671660748766958303529129924935633158739912269857322029936205503558375051635249841176062058916291",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_normal_logpdf(x), 0);
    print_mfloat_value("normal_logpdf(0.5)", x);
    print_mfloat_error_check("normal_logpdf(0.5) mfloat error", x,
                             "-1.0439385332046727417803297364056176398613974736377834128171515404827656959272603976947432986359541976220056466246343374463668628818407935721558759152226813936035607425474");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "-1.0439385332046727417803297364056176398613974736377834128171515404827656959272603976947432986359541976220056466246343374463668628818407935721558759152226813936035607425474",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_lower(one, MF_ONE), 0);
    print_mfloat_value("gammainc_lower(1,1)", one);
    print_mfloat_error_check("gammainc_lower(1,1) mfloat error", one,
                             "0.63212055882855767840447622983853913255418886896823216549216319830253850425510019664285272565408035625337267472315600479175302420720987099137334641050590121690780563262266");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.63212055882855767840447622983853913255418886896823216549216319830253850425510019664285272565408035625337267472315600479175302420720987099137334641050590121690780563262266",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_upper(one, MF_ONE), 0);
    print_mfloat_value("gammainc_upper(1,1)", one);
    print_mfloat_error_check("gammainc_upper(1,1) mfloat error", one,
                             "0.36787944117144232159552377016146086744581113103176783450783680169746149574489980335714727434591964374662732527684399520824697579279012900862665358949409878309219436737734");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.36787944117144232159552377016146086744581113103176783450783680169746149574489980335714727434591964374662732527684399520824697579279012900862665358949409878309219436737734",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_ei(one), 0);
    print_mfloat_value("ei(1)", one);
    print_mfloat_error_check("ei(1) mfloat error", one,
                             "1.8951178163559367554665209343316342690170605817327075916462284318825138345338041535489007101261389569718110953179446537425881491641630646880881866825388286696323385450952");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "1.8951178163559367554665209343316342690170605817327075916462284318825138345338041535489007101261389569718110953179446537425881491641630646880881866825388286696323385450952",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_e1(one), 0);
    print_mfloat_value("e1(1)", one);
    print_mfloat_error_check("e1(1) mfloat error", one,
                             "0.21938393439552027367716377546012164903104729340690820757797861307356869855914154472221025103513724995475823463087410959017637852053709600995670448787677741293134726079573");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.21938393439552027367716377546012164903104729340690820757797861307356869855914154472221025103513724995475823463087410959017637852053709600995670448787677741293134726079573",
        1));

    mf_free(x);
    mf_free(a);
    mf_free(b);
    mf_free(y);
    mf_free(neg);
    mf_free(one);
    ASSERT_EQ_INT(mf_set_default_precision(saved_default), 0);
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
    return 0;
}
