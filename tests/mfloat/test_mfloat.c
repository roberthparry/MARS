#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mfloat.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

void test_new_and_precision(void)
{
    size_t saved_default = mf_get_default_precision();
    mfloat_t *a = mf_new();
    mfloat_t *b = mf_new_prec(512);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_TRUE(mf_is_zero(a));
    ASSERT_EQ_LONG((long)mf_get_precision(a), 256);
    ASSERT_EQ_LONG((long)mf_get_precision(b), 512);

    ASSERT_EQ_INT(mf_set_precision(a, 320), 0);
    ASSERT_EQ_LONG((long)mf_get_precision(a), 320);

    ASSERT_EQ_INT(mf_set_default_precision(384), 0);
    ASSERT_EQ_LONG((long)mf_get_default_precision(), 384);
    mf_free(a);
    a = mf_new();
    ASSERT_NOT_NULL(a);
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
    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), 2);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 3);
    ASSERT_EQ_LONG((long)mf_get_mantissa_bits(a), 2);

    ASSERT_EQ_INT(mf_set_long(b, -40), 0);
    ASSERT_EQ_LONG((long)mf_get_sign(b), -1);
    ASSERT_EQ_LONG(mf_get_exponent2(b), 3);
    ASSERT_TRUE(mf_get_mantissa_u64(b, &mantissa));
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

    ASSERT_EQ_LONG((long)mf_get_sign(b), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(b), 1);
    ASSERT_TRUE(mf_get_mantissa_u64(b, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 9);

    mf_clear(b);
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

    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), -1);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 3);

    ASSERT_EQ_INT(mf_add(a, b), 0);
    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), -2);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 7);

    ASSERT_EQ_INT(mf_mul(b, c), 0);
    ASSERT_EQ_LONG((long)mf_get_sign(b), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(b), -4);
    ASSERT_TRUE(mf_get_mantissa_u64(b, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 9);

    ASSERT_EQ_INT(mf_sqrt(c), 0);
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

    ASSERT_EQ_INT(mf_div(a, b), 0);
    ASSERT_EQ_LONG((long)mf_get_sign(a), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(a), -3);
    ASSERT_TRUE(mf_get_mantissa_u64(a, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 1);

    ASSERT_EQ_INT(mf_pow_int(c, 2), 0);
    ASSERT_EQ_LONG((long)mf_get_sign(c), 1);
    ASSERT_EQ_LONG(mf_get_exponent2(c), -2);
    ASSERT_TRUE(mf_get_mantissa_u64(c, &mantissa));
    ASSERT_EQ_LONG((long)mantissa, 9);

    ASSERT_EQ_INT(mf_ldexp(c, 3), 0);
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

    ASSERT_TRUE(strcmp(sa, "1.5") == 0);
    ASSERT_TRUE(strcmp(sb, "0.25") == 0);
    ASSERT_TRUE(strcmp(sc, "-40") == 0);
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
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "1.5") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%8mf", a);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "     1.5") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%08mf", a);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "000001.5") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%.4mf", a);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "1.5000") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%MF", a);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "1.5E+0") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%mf", b);
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
    char *s_tenth = NULL;
    char *s_pi_hi = NULL;
    char *s_e_hi = NULL;
    char *s_gamma_hi = NULL;
    char buf[256];
    int n = 0;

    ASSERT_NOT_NULL(pi);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(gamma);
    ASSERT_NOT_NULL(mx);
    ASSERT_NOT_NULL(inv);

    s_half = mf_to_string(MF_HALF);
    s_tenth = mf_to_string(MF_TENTH);
    ASSERT_NOT_NULL(s_half);
    ASSERT_NOT_NULL(s_tenth);
    ASSERT_TRUE(strcmp(s_half, "0.5") == 0);
    ASSERT_TRUE(strncmp(s_tenth, "0.1", 3) == 0);

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
    ASSERT_TRUE(strcmp(buf, "0.125000") == 0);

    n = mf_sprintf(buf, sizeof(buf), "%MF", MF_INF);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "INF") == 0);
    n = mf_sprintf(buf, sizeof(buf), "%MF", MF_NINF);
    ASSERT_TRUE(n >= 0);
    ASSERT_TRUE(strcmp(buf, "-INF") == 0);
    n = mf_sprintf(buf, sizeof(buf), "%MF", MF_NAN);
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
    free(s_tenth);
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

    da = mf_to_double(a);
    db = mf_to_double(b);
    ASSERT_TRUE(fabs(da - 1.5) < 1e-15);
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

    ASSERT_TRUE(fabs(qf_to_double(qa) - 1.5) < 1e-15);
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
    ASSERT_TRUE(strcmp(sa, "1.5") == 0);
    ASSERT_TRUE(strcmp(sb, "0.25") == 0);

    ASSERT_EQ_INT(mf_set_qfloat(c, q), 0);
    sc = mf_to_string(c);
    ASSERT_NOT_NULL(sc);
    ASSERT_TRUE(strncmp(sc, "3.14159265358979", 16) == 0);
    qc = mf_to_qfloat(c);
    qerr = qf_abs(qf_sub(qc, q));
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
    mfloat_t *a = mf_create_long(1);
    mfloat_t *b = mf_create_long(4);
    mfloat_t *c = mf_create_string("0.5");
    mfloat_t *d = mf_create_long(2);
    mfloat_t *e = mf_create_long(3);
    mfloat_t *f = mf_create_long(1);
    mfloat_t *h = mf_create_long(2);
    mfloat_t *p = mf_create_long(10);
    mfloat_t *g = NULL;
    char *sa = NULL;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(f);
    ASSERT_NOT_NULL(h);
    ASSERT_NOT_NULL(p);

    ASSERT_EQ_INT(mf_exp(a), 0);
    ASSERT_TRUE(fabs(mf_to_double(a) - 2.718281828459045) < 1e-12);
    ASSERT_EQ_INT(mf_log(a), 0);
    ASSERT_TRUE(fabs(mf_to_double(a) - 1.0) < 1e-12);

    ASSERT_EQ_INT(mf_sqrt(b), 0);
    ASSERT_TRUE(fabs(mf_to_double(b) - 2.0) < 1e-12);

    ASSERT_EQ_INT(mf_sin(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - sin(0.5)) < 1e-12);
    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cos(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - cos(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_beta(d, e), 0);
    ASSERT_TRUE(fabs(mf_to_double(d) - (1.0 / 12.0)) < 1e-12);

    ASSERT_EQ_INT(mf_pow(h, p), 0);
    ASSERT_TRUE(fabs(mf_to_double(h) - 1024.0) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_tan(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - tan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_sinh(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - sinh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_cosh(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - cosh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atan(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - atan(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_asin(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - asin(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_acos(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - acos(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_tanh(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - tanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_asinh(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - asinh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "2"), 0);
    ASSERT_EQ_INT(mf_acosh(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - acosh(2.0)) < 1e-9);

    ASSERT_EQ_INT(mf_set_string(c, "0.5"), 0);
    ASSERT_EQ_INT(mf_atanh(c), 0);
    ASSERT_TRUE(fabs(mf_to_double(c) - atanh(0.5)) < 1e-9);

    ASSERT_EQ_INT(mf_gammainc_P(f, MF_ONE), 0);
    ASSERT_TRUE(fabs(mf_to_double(f) - (1.0 - exp(-1.0))) < 1e-12);

    g = mf_pow10(3);
    ASSERT_NOT_NULL(g);
    sa = mf_to_string(g);
    ASSERT_NOT_NULL(sa);
    ASSERT_TRUE(strcmp(sa, "1000") == 0);

    free(sa);
    mf_free(a);
    mf_free(b);
    mf_free(c);
    mf_free(d);
    mf_free(e);
    mf_free(f);
    mf_free(h);
    mf_free(p);
    mf_free(g);
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
    return 0;
}
