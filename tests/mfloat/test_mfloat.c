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
                             "2.71828182845904523536028747135266249775724709369995957496696762772407663035355");
    ASSERT_TRUE(mfloat_meets_precision(
        a,
        "2.71828182845904523536028747135266249775724709369995957496696762772407663035355",
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
                             "0.479425538604203000273287935215571388081803367940600675188616613125535000287814832209631275");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.479425538604203000273287935215571388081803367940600675188616613125535000287814832209631275",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sin(0.5)) < 1e-12);
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cos(c), 0);
    print_mfloat_value("c after cos", c);
    print_double_check("cos(0.5)", "0.5", cos(0.5), mf_to_double(c));
    print_mfloat_error_check("cos(0.5) mfloat error", c,
                             "0.877582561890372716116281582603829651991645197109744052997610868315950763274213947405794184");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.877582561890372716116281582603829651991645197109744052997610868315950763274213947405794184",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - cos(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_beta(d, e), 0);
    print_mfloat_value("d after beta(d, e)", d);
    print_double_check("beta(2, 3)", "(2,3)", 1.0 / 12.0, mf_to_double(d));
    print_mfloat_error_check("beta(2, 3) mfloat error", d,
                             "0.0833333333333333333333333333333333333333333333333333333333333333333333333333333333333333333");
    ASSERT_TRUE(mfloat_meets_precision(
        d,
        "0.0833333333333333333333333333333333333333333333333333333333333333333333333333333333333333333",
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
                             "0.546302489843790513255179465780285383297551720179791246164091385932907510518025815715180648");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.546302489843790513255179465780285383297551720179791246164091385932907510518025815715180648",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sinh(c), 0);
    print_mfloat_value("c after sinh", c);
    print_double_check("sinh(0.5)", "0.5", sinh(0.5), mf_to_double(c));
    print_mfloat_error_check("sinh(0.5) mfloat error", c,
                             "0.521095305493747361622425626411491559105928982611480527946093576452802250890233592317064454");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.521095305493747361622425626411491559105928982611480527946093576452802250890233592317064454",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - sinh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cosh(c), 0);
    print_mfloat_value("c after cosh", c);
    print_double_check("cosh(0.5)", "0.5", cosh(0.5), mf_to_double(c));
    print_mfloat_error_check("cosh(0.5) mfloat error", c,
                             "1.12762596520638078522622516140267201254784711809866748362898573518785877030398201631571207");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "1.12762596520638078522622516140267201254784711809866748362898573518785877030398201631571207",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - cosh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atan(c), 0);
    print_mfloat_value("c after atan", c);
    print_double_check("atan(0.5)", "0.5", atan(0.5), mf_to_double(c));
    print_mfloat_error_check("atan(0.5) mfloat error", c,
                             "0.463647609000806116214256231461214402028537054286120263810933088720197864165741705300600284");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.463647609000806116214256231461214402028537054286120263810933088720197864165741705300600284",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - atan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_asin(c), 0);
    print_mfloat_value("c after asin", c);
    print_double_check("asin(0.5)", "0.5", asin(0.5), mf_to_double(c));
    print_mfloat_error_check("asin(0.5) mfloat error", c,
                             "0.523598775598298873077107230546583814032861566562517636829157432051302734381034833104672471");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.523598775598298873077107230546583814032861566562517636829157432051302734381034833104672471",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - asin(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_acos(c), 0);
    print_mfloat_value("c after acos", c);
    print_double_check("acos(0.5)", "0.5", acos(0.5), mf_to_double(c));
    print_mfloat_error_check("acos(0.5) mfloat error", c,
                             "1.04719755119659774615421446109316762806572313312503527365831486410260546876206966620934494");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "1.04719755119659774615421446109316762806572313312503527365831486410260546876206966620934494",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - acos(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_tanh(c), 0);
    print_mfloat_value("c after tanh", c);
    print_double_check("tanh(0.5)", "0.5", tanh(0.5), mf_to_double(c));
    print_mfloat_error_check("tanh(0.5) mfloat error", c,
                             "0.462117157260009758502318483643672548730289280330113038552731815838080906140409278774949064");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.462117157260009758502318483643672548730289280330113038552731815838080906140409278774949064",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - tanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_asinh(c), 0);
    print_mfloat_value("c after asinh", c);
    print_double_check("asinh(0.5)", "0.5", asinh(0.5), mf_to_double(c));
    print_mfloat_error_check("asinh(0.5) mfloat error", c,
                             "0.481211825059603447497758913424368423135184334385660519661018168840163867608221774412009429");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.481211825059603447497758913424368423135184334385660519661018168840163867608221774412009429",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - asinh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "2"), 0);
    ASSERT_EQ_INT(mf_acosh(c), 0);
    print_mfloat_value("c after acosh", c);
    print_double_check("acosh(2)", "2", acosh(2.0), mf_to_double(c));
    print_mfloat_error_check("acosh(2) mfloat error", c,
                             "1.31695789692481670862504634730796844402698197146751647976847225692046018541644397607421901");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "1.31695789692481670862504634730796844402698197146751647976847225692046018541644397607421901",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - acosh(2.0)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atanh(c), 0);
    print_mfloat_value("c after atanh", c);
    print_double_check("atanh(0.5)", "0.5", atanh(0.5), mf_to_double(c));
    print_mfloat_error_check("atanh(0.5) mfloat error", c,
                             "0.549306144334054845697622618461262852323745278911374725867347166818747146609304483436807877");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.549306144334054845697622618461262852323745278911374725867347166818747146609304483436807877",
        1));
    ASSERT_TRUE(fabs(mf_to_double(c) - atanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_gammainc_P(f, MF_ONE), 0);
    print_mfloat_value("f after gammainc_P(f, 1)", f);
    print_double_check("gammainc_P(1,1)", "(1,1)", 1.0 - exp(-1.0), mf_to_double(f));
    print_mfloat_error_check("gammainc_P(1,1) mfloat error", f,
                             "0.632120558828557678404476229838539132554188868968232165492163198302538504255100196642852726");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.632120558828557678404476229838539132554188868968232165492163198302538504255100196642852726",
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
                             "2.356194490192344928846982537459627163147877049531329365731208444230862304714656749");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "2.356194490192344928846982537459627163147877049531329365731208444230862304714656749",
        1));

    ASSERT_EQ_INT(mf_gamma(i), 0);
    print_mfloat_value("gamma(5)", i);
    print_mfloat_error_check("gamma(5) mfloat error", i, "24");
    ASSERT_TRUE(mfloat_meets_precision(i, "24", 0));

    ASSERT_EQ_INT(mf_set_long(i, 5), 0);
    ASSERT_EQ_INT(mf_lgamma(i), 0);
    print_mfloat_value("lgamma(5)", i);
    print_mfloat_error_check("lgamma(5) mfloat error", i,
                             "3.178053830347945619646941601297055408873990960903515214096734362117675159127693114");
    ASSERT_TRUE(mfloat_meets_precision(
        i,
        "3.178053830347945619646941601297055408873990960903515214096734362117675159127693114",
        1));

    ASSERT_EQ_INT(mf_digamma(j), 0);
    print_mfloat_value("digamma(1)", j);
    print_mfloat_error_check("digamma(1) mfloat error", j,
                             "-0.577215664901532860606512090082402431042159335939923598805767234884867726777665");
    ASSERT_TRUE(mfloat_meets_precision(
        j,
        "-0.577215664901532860606512090082402431042159335939923598805767234884867726777665",
        1));

    ASSERT_EQ_INT(mf_set_long(j, 1), 0);
    ASSERT_EQ_INT(mf_trigamma(j), 0);
    print_mfloat_value("trigamma(1)", j);
    print_mfloat_error_check("trigamma(1) mfloat error", j,
                             "1.644934066848226436472415166646025189218949901206798437735558229370007470403200874");
    ASSERT_TRUE(mfloat_meets_precision(
        j,
        "1.644934066848226436472415166646025189218949901206798437735558229370007470403200874",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_erf(c), 0);
    print_mfloat_value("erf(0.5)", c);
    print_mfloat_error_check("erf(0.5) mfloat error", c,
                             "0.520499877813046537682746653891964528736451575757963700058805725647193521716853571");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.520499877813046537682746653891964528736451575757963700058805725647193521716853571",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_erfc(c), 0);
    print_mfloat_value("erfc(0.5)", c);
    print_mfloat_error_check("erfc(0.5) mfloat error", c,
                             "0.479500122186953462317253346108035471263548424242036299941194274352806478283146429");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.479500122186953462317253346108035471263548424242036299941194274352806478283146429",
        1));

    ASSERT_EQ_INT(mf_set_string(c, "1"), 0);
    ASSERT_EQ_INT(mf_lambert_w0(c), 0);
    print_mfloat_value("lambert_w0(1)", c);
    print_mfloat_error_check("lambert_w0(1) mfloat error", c,
                             "0.567143290409783872999968662210355549753815787186512508135131079223045793086684567");
    ASSERT_TRUE(mfloat_meets_precision(
        c,
        "0.567143290409783872999968662210355549753815787186512508135131079223045793086684567",
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
                             "0.367879441171442321595523770161460867445811131031767834507836801697461495744900");
    ASSERT_TRUE(mfloat_meets_precision(
        f,
        "0.367879441171442321595523770161460867445811131031767834507836801697461495744900",
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
                             "0.476936276204469873381418353643130559808969749059470644703882695919383447774646733488695916");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.476936276204469873381418353643130559808969749059470644703882695919383447774646733488695916",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_erfcinv(x), 0);
    print_mfloat_value("erfcinv(0.5)", x);
    print_mfloat_error_check("erfcinv(0.5) mfloat error", x,
                             "0.476936276204469873381418353643130559808969749059470644703882695919383447774646733488695916");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.476936276204469873381418353643130559808969749059470644703882695919383447774646733488695916",
        1));

    ASSERT_EQ_INT(mf_tetragamma(one), 0);
    print_mfloat_value("tetragamma(1)", one);
    print_mfloat_error_check("tetragamma(1) mfloat error", one,
                             "-2.404113806319188570799476323022899981529972584680997763584543110683676411572626180372911747");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "-2.404113806319188570799476323022899981529972584680997763584543110683676411572626180372911747",
        1));

    ASSERT_EQ_INT(mf_gammainv(y), 0);
    print_mfloat_value("gammainv(3)", y);
    print_mfloat_error_check("gammainv(3) mfloat error", y,
                             "3.405869986309566924699929218375558009595395799328981394312941626277148046924954397103935285");
    ASSERT_TRUE(mfloat_meets_precision(
        y,
        "3.405869986309566924699929218375558009595395799328981394312941626277148046924954397103935285",
        1));

    ASSERT_EQ_INT(mf_lambert_wm1(neg), 0);
    print_mfloat_value("lambert_wm1(-0.1)", neg);
    print_mfloat_error_check("lambert_wm1(-0.1) mfloat error", neg,
                             "-3.577152063957297218409391963511994880401796257793075923683527755791687236350575462861463656");
    ASSERT_TRUE(mfloat_meets_precision(
        neg,
        "-3.577152063957297218409391963511994880401796257793075923683527755791687236350575462861463656",
        1));

    ASSERT_EQ_INT(mf_logbeta(a, b), 0);
    print_mfloat_value("logbeta(2.5,3.5)", a);
    print_mfloat_error_check("logbeta(2.5,3.5) mfloat error", a,
                             "-3.301835269962052609799184383389828128309215704143981009717122670837516912654122678189667591");
    ASSERT_TRUE(mfloat_meets_precision(
        a,
        "-3.301835269962052609799184383389828128309215704143981009717122670837516912654122678189667591",
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
                             "1.697652726313550248201426809306819861700902887898202119975118336628232508098416374294547230");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "1.697652726313550248201426809306819861700902887898202119975118336628232508098416374294547230",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_logbeta_pdf(x, a, b), 0);
    print_mfloat_value("logbeta_pdf(0.5,2.5,3.5)", x);
    print_mfloat_error_check("logbeta_pdf(0.5,2.5,3.5) mfloat error", x,
                             "0.529246547722271372130255897557121856007215166702959993234402632863942424775343815766214283");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.529246547722271372130255897557121856007215166702959993234402632863942424775343815766214283",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_normal_pdf(x), 0);
    print_mfloat_value("normal_pdf(0.5)", x);
    print_mfloat_error_check("normal_pdf(0.5) mfloat error", x,
                             "0.352065326764299477774680441596517653110315180375711949655469017988223197836716607487669583");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "0.352065326764299477774680441596517653110315180375711949655469017988223197836716607487669583",
        1));

    ASSERT_EQ_INT(mf_set_string(x, "0.5"), 0);
    ASSERT_EQ_INT(mf_normal_logpdf(x), 0);
    print_mfloat_value("normal_logpdf(0.5)", x);
    print_mfloat_error_check("normal_logpdf(0.5) mfloat error", x,
                             "-1.043938533204672741780329736405617639861397473637783412817151540482765695927260397694743299");
    ASSERT_TRUE(mfloat_meets_precision(
        x,
        "-1.043938533204672741780329736405617639861397473637783412817151540482765695927260397694743299",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_lower(one, MF_ONE), 0);
    print_mfloat_value("gammainc_lower(1,1)", one);
    print_mfloat_error_check("gammainc_lower(1,1) mfloat error", one,
                             "0.632120558828557678404476229838539132554188868968232165492163198302538504255100196642852726");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.632120558828557678404476229838539132554188868968232165492163198302538504255100196642852726",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_gammainc_upper(one, MF_ONE), 0);
    print_mfloat_value("gammainc_upper(1,1)", one);
    print_mfloat_error_check("gammainc_upper(1,1) mfloat error", one,
                             "0.367879441171442321595523770161460867445811131031767834507836801697461495744899803357147274");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.367879441171442321595523770161460867445811131031767834507836801697461495744899803357147274",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_ei(one), 0);
    print_mfloat_value("ei(1)", one);
    print_mfloat_error_check("ei(1) mfloat error", one,
                             "1.895117816355936755466520934331634269017060581732707591646228431882513834533804153548900710");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "1.895117816355936755466520934331634269017060581732707591646228431882513834533804153548900710",
        1));

    ASSERT_EQ_INT(mf_set_long(one, 1), 0);
    ASSERT_EQ_INT(mf_e1(one), 0);
    print_mfloat_value("e1(1)", one);
    print_mfloat_error_check("e1(1) mfloat error", one,
                             "0.219383934395520273677163775460121649031047293406908207577978613073568698559141544722210251");
    ASSERT_TRUE(mfloat_meets_precision(
        one,
        "0.219383934395520273677163775460121649031047293406908207577978613073568698559141544722210251",
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
