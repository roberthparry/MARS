/**
 * @file benchmarks_operations.c
 * @brief Sorted dispatch catalogue for direct numeric mathematical benchmarks.
 *
 * Inputs stay in the real domain for real backends. Complex profiles perturb permitted operands while keeping
 * polynomial degrees and branch orders real. Small signature adapters cover integer orders and vector arguments;
 * these are not substitutes for the library algorithms. Unsupported direct backends remain explicitly absent.
 */
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "number-t.h"
#include "qfloat-t.h"
#include "qcomplex-t.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

static qcomplex_t benchmark_c_abs(qcomplex_t a)
{
    return qc_make(qc_abs(a), QF_ZERO);
}

static qcomplex_t benchmark_c_arg(qcomplex_t a)
{
    return qc_make(qc_arg(a), QF_ZERO);
}

static qcomplex_t benchmark_c_real_part(qcomplex_t a)
{
    return qc_make(a.re, QF_ZERO);
}

static qcomplex_t benchmark_c_imag_part(qcomplex_t a)
{
    return qc_make(a.im, QF_ZERO);
}

static qcomplex_t benchmark_c_inv(qcomplex_t a)
{
    return qc_div(qc_make(qf_from_double(1), QF_ZERO), a);
}

static qcomplex_t benchmark_c_sqr(qcomplex_t a)
{
    return qc_mul(a, a);
}

static qfloat_t benchmark_r_harmonic_poly(qfloat_t a, qfloat_t b)
{
    return qf_harmonic_poly((unsigned long)qf_to_double(a), b);
}

static qcomplex_t benchmark_c_harmonic_poly(qcomplex_t a, qcomplex_t b)
{
    return qc_harmonic_poly((unsigned long)qf_to_double(a.re), b);
}

static qfloat_t benchmark_r_chebyshev_t(qfloat_t a, qfloat_t b)
{
    return qf_chebyshev_t((unsigned long)qf_to_double(a), b);
}

static qcomplex_t benchmark_c_chebyshev_t(qcomplex_t a, qcomplex_t b)
{
    return qc_chebyshev_t((unsigned long)qf_to_double(a.re), b);
}

static qfloat_t benchmark_r_chebyshev_u(qfloat_t a, qfloat_t b)
{
    return qf_chebyshev_u((unsigned long)qf_to_double(a), b);
}

static qcomplex_t benchmark_c_chebyshev_u(qcomplex_t a, qcomplex_t b)
{
    return qc_chebyshev_u((unsigned long)qf_to_double(a.re), b);
}

static qfloat_t benchmark_r_hermite_h(qfloat_t a, qfloat_t b)
{
    return qf_hermite_h((unsigned long)qf_to_double(a), b);
}

static qcomplex_t benchmark_c_hermite_h(qcomplex_t a, qcomplex_t b)
{
    return qc_hermite_h((unsigned long)qf_to_double(a.re), b);
}

static qfloat_t benchmark_r_root(qfloat_t a, qfloat_t b)
{
    return qf_root(a, (unsigned)qf_to_double(b));
}

static qcomplex_t benchmark_c_root(qcomplex_t a, qcomplex_t b)
{
    return qc_root(a, (unsigned)qf_to_double(b.re));
}

static qcomplex_t benchmark_c_lambert_wn(qcomplex_t a, qcomplex_t b)
{
    return qc_lambert_wn((int)qf_to_double(a.re), b);
}

static number_t benchmark_n_factorial(number_t a)
{
    return num_factorial((unsigned long)num_to_double(a));
}

static number_t benchmark_n_fibonacci(number_t a)
{
    return num_fibonacci((unsigned long)num_to_double(a));
}

static number_t benchmark_n_polygamma(number_t a, number_t b)
{
    return num_polygamma((unsigned)num_to_double(a), b);
}

static qfloat_t benchmark_r_polygamma(qfloat_t a, qfloat_t b)
{
    return qf_polygamma((unsigned)qf_to_double(a), b);
}

static qcomplex_t benchmark_c_polygamma(qcomplex_t a, qcomplex_t b)
{
    return qc_polygamma((unsigned)qf_to_double(a.re), b);
}

static number_t benchmark_n_clausen(number_t a, number_t b)
{
    return num_clausen((unsigned long)num_to_double(a), b);
}

static qfloat_t benchmark_r_clausen(qfloat_t a, qfloat_t b)
{
    return qf_clausen((unsigned long)qf_to_double(a), b);
}

static qcomplex_t benchmark_c_clausen(qcomplex_t a, qcomplex_t b)
{
    return qc_clausen((unsigned long)qf_to_double(a.re), b);
}

static number_t benchmark_n_roundn(number_t a, number_t b)
{
    return num_roundn(a, (int)num_to_double(b));
}

static qfloat_t benchmark_r_roundn(qfloat_t a, qfloat_t b)
{
    return qf_roundn(a, (int)qf_to_double(b));
}

static qcomplex_t benchmark_c_roundn(qcomplex_t a, qcomplex_t b)
{
    return qc_roundn(a, (int)qf_to_double(b.re));
}

static number_t benchmark_n_rounds(number_t a, number_t b)
{
    return num_rounds(a, (unsigned)num_to_double(b));
}

static qfloat_t benchmark_r_rounds(qfloat_t a, qfloat_t b)
{
    return qf_rounds(a, (unsigned)qf_to_double(b));
}

static qcomplex_t benchmark_c_rounds(qcomplex_t a, qcomplex_t b)
{
    return qc_rounds(a, (unsigned)qf_to_double(b.re));
}

static number_t benchmark_n_pow_int(number_t a, number_t b)
{
    return num_pow_int(a, (int)num_to_double(b));
}

static qfloat_t benchmark_r_pow_int(qfloat_t a, qfloat_t b)
{
    return qf_pow_int(a, (int)qf_to_double(b));
}

static number_t benchmark_n_ldexp(number_t a, number_t b)
{
    return num_ldexp(a, (int)num_to_double(b));
}

static qfloat_t benchmark_r_ldexp(qfloat_t a, qfloat_t b)
{
    return qf_ldexp(a, (int)qf_to_double(b));
}

static qcomplex_t benchmark_c_ldexp(qcomplex_t a, qcomplex_t b)
{
    return qc_ldexp(a, (int)qf_to_double(b.re));
}

static number_t benchmark_n_mul_pow10(number_t a, number_t b)
{
    return num_mul_pow10(a, (int)num_to_double(b));
}

static qfloat_t benchmark_r_mul_pow10(qfloat_t a, qfloat_t b)
{
    return qf_mul_pow10(a, (int)qf_to_double(b));
}

static number_t benchmark_n_pow10(number_t a)
{
    return num_pow10((int)num_to_double(a));
}

static qfloat_t benchmark_r_pow10(qfloat_t a)
{
    return qf_pow10((int)qf_to_double(a));
}

static number_t benchmark_n_sincos(number_t a)
{
    number_t s = {0}, c = {0};
    if (num_sincos(a, &s, &c)) {
        num_destroy(&s); num_destroy(&c);
        errno = ENOTSUP;
        return num_create_from_double(NAN);
    }
    num_destroy(&c);
    return s;
}


static number_t benchmark_n_sinhcosh(number_t a)
{
    number_t s = {0}, c = {0};
    if (num_sinhcosh(a, &s, &c)) {
        num_destroy(&s); num_destroy(&c);
        errno = ENOTSUP;
        return num_create_from_double(NAN);
    }
    num_destroy(&c);
    return s;
}


static number_t benchmark_n_hypergeometric_pFq(number_t a, number_t b, number_t c)
{
    return num_hypergeometric_pFq(&a, 1, &b, 1, c);
}

static qfloat_t benchmark_r_hypergeometric_pFq(qfloat_t a, qfloat_t b, qfloat_t c)
{
    return qf_hypergeometric_pFq(&a, 1, &b, 1, c);
}

static qcomplex_t benchmark_c_hypergeometric_pFq(qcomplex_t a, qcomplex_t b, qcomplex_t c)
{
    return qc_hypergeometric_pFq(&a, 1, &b, 1, c);
}

static number_t benchmark_n_lauricella_f(number_t a, number_t b, number_t c)
{
    return num_lauricella_f(a, &b, c, &b, 1);
}

static qfloat_t benchmark_r_lauricella_f(qfloat_t a, qfloat_t b, qfloat_t c)
{
    return qf_lauricella_f(a, &b, c, &b, 1);
}

static qcomplex_t benchmark_c_lauricella_f(qcomplex_t a, qcomplex_t b, qcomplex_t c)
{
    return qc_lauricella_f(a, &b, c, &b, 1);
}

static number_t benchmark_n_add_long(number_t a, number_t b)
{
    return num_add_long(a, (long)num_to_double(b));
}

static number_t benchmark_n_mul_long(number_t a, number_t b)
{
    return num_mul_long(a, (long)num_to_double(b));
}

static number_t benchmark_n_set_bit(number_t a, number_t b)
{
    return num_set_bit(a, (size_t)num_to_double(b));
}

static number_t benchmark_n_clear_bit(number_t a, number_t b)
{
    return num_clear_bit(a, (size_t)num_to_double(b));
}

static number_t benchmark_n_shl(number_t a, number_t b)
{
    return num_shl(a, (long)num_to_double(b));
}

static number_t benchmark_n_shr(number_t a, number_t b)
{
    return num_shr(a, (long)num_to_double(b));
}

static number_t benchmark_n_is_prime(number_t a)
{
    return num_create_from_long(num_is_prime(a));
}

static number_t benchmark_n_prove_prime(number_t a)
{
    number_primality_t result = num_prove_prime(a);
    return result == NUMBER_PRIMALITY_UNKNOWN ? num_create_from_double(NAN) : num_create_from_long(result);
}

static number_t benchmark_n_test_bit(number_t a, number_t b)
{
    return num_create_from_long(num_test_bit(a, (size_t)num_to_double(b)));
}

static number_t benchmark_n_bit_length(number_t a)
{
    return num_create_from_long((long)num_bit_length(a));
}

static number_t benchmark_n_factors(number_t a)
{
    number_factors_t *factors = num_factors(a);
    number_t result = factors ? num_create_from_long((long)factors->count) : num_create_from_double(NAN);
    num_factors_free(factors);
    return result;
}

static number_t benchmark_n_divmod(number_t a, number_t b)
{
    number_t q = {0}, r = {0};
    int status = num_divmod(a, b, &q, &r);
    num_destroy(&r);
    if (!status) return q;
    num_destroy(&q);
    return num_create_from_double(NAN);
}

static number_t benchmark_n_gcdext(number_t a, number_t b)
{
    number_t g = {0}, x = {0}, y = {0};
    int status = num_gcdext(a, b, &g, &x, &y);
    num_destroy(&x); num_destroy(&y);
    if (!status) return g;
    num_destroy(&g);
    return num_create_from_double(NAN);
}

/* A dense 1,024-bit dividend; its GCD with the 512-bit divisor is 2 * (2^127 - 1). */
static const char benchmark_integer_wide[] =
    "9712853409152031141207811585727780469484705121206232579607434236969009533618825972019189714991570566"
    "4060543226969142780725822337496727884659619483452400511633617514882293623272276764857474830401626415"
    "8410359714586574421333707776078034640711011583851241862299950730454274489182279598930822829198639823"
    "07657776";

/* A dense 512-bit operand, also used as the modular exponent. */
static const char benchmark_integer_divisor[] =
    "1249884604836704592637071660260998283972164142922295912537646258267486940344399697264862869904768683"
    "9582654905384579341977328653591547736951147466435189662";

/* The 521-bit Mersenne prime 2^521 - 1. */
static const char benchmark_integer_prime[] =
    "6864797660130609714981900799081393217269435300143305409394463459185543183397656052122559640661454554"
    "977296311391480858037121987999716643812574028291115057151";

/* The 1,215-bit smooth composite 2^512 * 3^256 * 5^128; factorisation remains bounded. */
static const char benchmark_integer_smooth[] =
    "5477211901915463633986786204116344879097448413145986152068732780920991433983601252275954799541486775"
    "7111260059673826001181827972337081337753357883356580370650813156906229997603304130218983321574299265"
    "3063887339218309467131548996199881113600000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000000000000000000";

static const benchmark_operation_t operations[] = {
    {"E1", 1,
     {.n1 = num_E1},
     {.r1 = qf_E1}, {.c1 = qc_E1},
     {"2.345678"}, 1, false},
    {"Ei", 1,
     {.n1 = num_Ei},
     {.r1 = qf_Ei}, {.c1 = qc_Ei},
     {"2.345678"}, 1, false},
    {"Li", 1,
     {.n1 = num_Li},
     {.r1 = qf_Li}, {.c1 = qc_Li},
     {"2.345"}, 1, false},
    {"abs", 1,
     {.n1 = num_abs},
     {.r1 = qf_abs}, {.c1 = benchmark_c_abs},
     {"0.7"}, 1, false},
    {"acos", 1,
     {.n1 = num_acos},
     {.r1 = qf_acos}, {.c1 = qc_acos},
     {"0.7"}, 1, false},
    {"acosec", 1,
     {.n1 = num_acosec},
     {.r1 = qf_acosec}, {.c1 = qc_acosec},
     {"2.345"}, 1, false},
    {"acosech", 1,
     {.n1 = num_acosech},
     {.r1 = qf_acosech}, {.c1 = qc_acosech},
     {"0.7"}, 1, false},
    {"acosh", 1,
     {.n1 = num_acosh},
     {.r1 = qf_acosh}, {.c1 = qc_acosh},
     {"2.345"}, 1, false},
    {"acot", 1,
     {.n1 = num_acot},
     {.r1 = qf_acot}, {.c1 = qc_acot},
     {"0.7"}, 1, false},
    {"acoth", 1,
     {.n1 = num_acoth},
     {.r1 = qf_acoth}, {.c1 = qc_acoth},
     {"2.345"}, 1, false},
    {"add", 2,
     {.n2 = num_add},
     {.r2 = qf_add}, {.c2 = qc_add},
     {"0.7", "0.7"}, 3, false},
    {"add_long", 2,
     {.n2 = benchmark_n_add_long}, {0}, {0},
     {"17", "3"}, 1, false},
    {"appell_f1", 6,
     {.n6 = num_appell_f1},
     {.r6 = qf_appell_f1}, {.c6 = qc_appell_f1},
     {"0.5", "0.7", "0.8", "2.5", "0.2", "0.3"}, 63, false},
    {"arccovercos", 1,
     {.n1 = num_arccovercos},
     {.r1 = qf_arccovercos}, {.c1 = qc_arccovercos},
     {"0.5"}, 1, false},
    {"arccoversin", 1,
     {.n1 = num_arccoversin},
     {.r1 = qf_arccoversin}, {.c1 = qc_arccoversin},
     {"0.5"}, 1, false},
    {"archacovercos", 1,
     {.n1 = num_archacovercos},
     {.r1 = qf_archacovercos}, {.c1 = qc_archacovercos},
     {"0.5"}, 1, false},
    {"archacoversin", 1,
     {.n1 = num_archacoversin},
     {.r1 = qf_archacoversin}, {.c1 = qc_archacoversin},
     {"0.5"}, 1, false},
    {"archavercos", 1,
     {.n1 = num_archavercos},
     {.r1 = qf_archavercos}, {.c1 = qc_archavercos},
     {"0.5"}, 1, false},
    {"archaversin", 1,
     {.n1 = num_archaversin},
     {.r1 = qf_archaversin}, {.c1 = qc_archaversin},
     {"0.5"}, 1, false},
    {"arcvercos", 1,
     {.n1 = num_arcvercos},
     {.r1 = qf_arcvercos}, {.c1 = qc_arcvercos},
     {"0.5"}, 1, false},
    {"arcversin", 1,
     {.n1 = num_arcversin},
     {.r1 = qf_arcversin}, {.c1 = qc_arcversin},
     {"0.5"}, 1, false},
    {"arg", 1,
     {.n1 = num_arg}, {0}, {.c1 = benchmark_c_arg}, {"0.7"}, 1, false},
    {"asec", 1,
     {.n1 = num_asec},
     {.r1 = qf_asec}, {.c1 = qc_asec},
     {"2.345"}, 1, false},
    {"asech", 1,
     {.n1 = num_asech},
     {.r1 = qf_asech}, {.c1 = qc_asech},
     {"0.7"}, 1, false},
    {"asin", 1,
     {.n1 = num_asin},
     {.r1 = qf_asin}, {.c1 = qc_asin},
     {"0.7"}, 1, false},
    {"asinh", 1,
     {.n1 = num_asinh},
     {.r1 = qf_asinh}, {.c1 = qc_asinh},
     {"0.7"}, 1, false},
    {"atan", 1,
     {.n1 = num_atan},
     {.r1 = qf_atan}, {.c1 = qc_atan},
     {"0.7"}, 1, false},
    {"atan2", 2,
     {.n2 = num_atan2},
     {.r2 = qf_atan2}, {.c2 = qc_atan2},
     {"0.5", "-0.75"}, 3, false},
    {"atanh", 1,
     {.n1 = num_atanh},
     {.r1 = qf_atanh}, {.c1 = qc_atanh},
     {"0.7"}, 1, false},
    {"bessel_i", 2,
     {.n2 = num_bessel_i},
     {.r2 = qf_bessel_i}, {.c2 = qc_bessel_i},
     {"2", "0.7"}, 2, false},
    {"bessel_j", 2,
     {.n2 = num_bessel_j}, {.r2 = qf_bessel_j}, {.c2 = qc_bessel_j}, {"2", "0.7"}, 2, false},
    {"bessel_k", 2,
     {.n2 = num_bessel_k},
     {.r2 = qf_bessel_k}, {.c2 = qc_bessel_k},
     {"2", "0.7"}, 2, false},
    {"bessel_y", 2,
     {.n2 = num_bessel_y},
     {.r2 = qf_bessel_y}, {.c2 = qc_bessel_y},
     {"2", "0.7"}, 2, false},
    {"beta", 2,
     {.n2 = num_beta},
     {.r2 = qf_beta}, {.c2 = qc_beta},
     {"2.3", "4.5"}, 3, false},
    {"beta_pdf", 3,
     {.n3 = num_beta_pdf},
     {.r3 = qf_beta_pdf}, {.c3 = qc_beta_pdf},
     {"0.5", "2.5", "3.5"}, 7, false},
    {"binomial", 2,
     {.n2 = num_binomial},
     {.r2 = qf_binomial}, {.c2 = qc_binomial},
     {"5", "2"}, 3, false},
    {"bit_and", 2,
     {.n2 = num_bit_and}, {0}, {0}, {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"bit_length", 1,
     {.n1 = benchmark_n_bit_length}, {0}, {0},
     {benchmark_integer_wide}, 0, true},
    {"bit_not", 1,
     {.n1 = num_bit_not}, {0}, {0}, {benchmark_integer_wide}, 0, true},
    {"bit_or", 2,
     {.n2 = num_bit_or}, {0}, {0}, {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"bit_xor", 2,
     {.n2 = num_bit_xor}, {0}, {0}, {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"ceil", 1,
     {.n1 = num_ceil}, {0}, {0}, {"0.7"}, 1, false},
    {"chebyshev_t", 2,
     {.n2 = num_chebyshev_t},
     {.r2 = benchmark_r_chebyshev_t}, {.c2 = benchmark_c_chebyshev_t},
     {"2", "0.7"}, 2, false},
    {"chebyshev_u", 2,
     {.n2 = num_chebyshev_u},
     {.r2 = benchmark_r_chebyshev_u}, {.c2 = benchmark_c_chebyshev_u},
     {"2", "0.7"}, 2, false},
    {"circ", 1,
     {.n1 = num_circ},
     {.r1 = qf_circ}, {.c1 = qc_circ},
     {"0.7"}, 0, false},
    {"clausen", 2,
     {.n2 = benchmark_n_clausen},
     {.r2 = benchmark_r_clausen}, {.c2 = benchmark_c_clausen},
     {"3", "0.7"}, 2, false},
    {"clausen2", 1,
     {.n1 = num_clausen2},
     {.r1 = qf_clausen2}, {.c1 = qc_clausen2},
     {"0.7"}, 1, false},
    {"clear_bit", 2,
     {.n2 = benchmark_n_clear_bit}, {0}, {0},
     {benchmark_integer_wide, "701"}, 0, true},
    {"conj", 1,
     {.n1 = num_conj}, {0}, {.c1 = qc_conj}, {"0.7"}, 1, false},
    {"cos", 1,
     {.n1 = num_cos},
     {.r1 = qf_cos}, {.c1 = qc_cos},
     {"0.7"}, 1, false},
    {"cosec", 1,
     {.n1 = num_cosec},
     {.r1 = qf_cosec}, {.c1 = qc_cosec},
     {"0.7"}, 1, false},
    {"cosech", 1,
     {.n1 = num_cosech},
     {.r1 = qf_cosech}, {.c1 = qc_cosech},
     {"0.7"}, 1, false},
    {"cosh", 1,
     {.n1 = num_cosh},
     {.r1 = qf_cosh}, {.c1 = qc_cosh},
     {"0.7"}, 1, false},
    {"cot", 1,
     {.n1 = num_cot},
     {.r1 = qf_cot}, {.c1 = qc_cot},
     {"0.7"}, 1, false},
    {"coth", 1,
     {.n1 = num_coth},
     {.r1 = qf_coth}, {.c1 = qc_coth},
     {"0.7"}, 1, false},
    {"covercos", 1,
     {.n1 = num_covercos},
     {.r1 = qf_covercos}, {.c1 = qc_covercos},
     {"0.7"}, 1, false},
    {"coversin", 1,
     {.n1 = num_coversin},
     {.r1 = qf_coversin}, {.c1 = qc_coversin},
     {"0.7"}, 1, false},
    {"cubrt", 1,
     {.n1 = num_cubrt},
     {.r1 = qf_cubrt}, {.c1 = qc_cubrt},
     {"0.7"}, 1, false},
    {"digamma", 1,
     {.n1 = num_digamma},
     {.r1 = qf_digamma}, {.c1 = qc_digamma},
     {"2.345"}, 1, false},
    {"dilog", 1,
     {.n1 = num_dilog},
     {.r1 = qf_dilog}, {.c1 = qc_dilog},
     {"0.7"}, 1, false},
    {"div", 2,
     {.n2 = num_div},
     {.r2 = qf_div}, {.c2 = qc_div},
     {"0.7", "0.7"}, 3, false},
    {"divmod", 2,
     {.n2 = benchmark_n_divmod}, {0}, {0},
     {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"erf", 1,
     {.n1 = num_erf},
     {.r1 = qf_erf}, {.c1 = qc_erf},
     {"0.7"}, 1, false},
    {"erfc", 1,
     {.n1 = num_erfc},
     {.r1 = qf_erfc}, {.c1 = qc_erfc},
     {"0.7"}, 1, false},
    {"erfcinv", 1,
     {.n1 = num_erfcinv},
     {.r1 = qf_erfcinv}, {.c1 = qc_erfcinv},
     {"0.5"}, 1, false},
    {"erfinv", 1,
     {.n1 = num_erfinv},
     {.r1 = qf_erfinv}, {.c1 = qc_erfinv},
     {"0.5"}, 1, false},
    {"exp", 1,
     {.n1 = num_exp},
     {.r1 = qf_exp}, {.c1 = qc_exp},
     {"1.23456789"}, 1, false},
    {"factorial", 1,
     {.n1 = benchmark_n_factorial}, {0}, {0}, {"5000"}, 0, true},
    {"factors", 1,
     {.n1 = benchmark_n_factors}, {0}, {0},
     {benchmark_integer_smooth}, 0, true},
    {"fibonacci", 1,
     {.n1 = benchmark_n_fibonacci}, {0}, {0}, {"10000"}, 0, true},
    {"floor", 1,
     {.n1 = num_floor},
     {.r1 = qf_floor}, {.c1 = qc_floor},
     {"0.7"}, 1, false},
    {"gamma", 1,
     {.n1 = num_gamma},
     {.r1 = qf_gamma}, {.c1 = qc_gamma},
     {"2.345"}, 1, false},
    {"gammainc_P", 2,
     {.n2 = num_gammainc_P},
     {.r2 = qf_gammainc_P}, {.c2 = qc_gammainc_P},
     {"2.3", "1.4"}, 3, false},
    {"gammainc_Q", 2,
     {.n2 = num_gammainc_Q},
     {.r2 = qf_gammainc_Q}, {.c2 = qc_gammainc_Q},
     {"2.3", "1.4"}, 3, false},
    {"gammainc_lower", 2,
     {.n2 = num_gammainc_lower},
     {.r2 = qf_gammainc_lower}, {.c2 = qc_gammainc_lower},
     {"2.3", "1.4"}, 3, false},
    {"gammainc_upper", 2,
     {.n2 = num_gammainc_upper},
     {.r2 = qf_gammainc_upper}, {.c2 = qc_gammainc_upper},
     {"2.3", "1.4"}, 3, false},
    {"gammainv", 1,
     {.n1 = num_gammainv},
     {.r1 = qf_gammainv}, {.c1 = qc_gammainv},
     {"9.5"}, 1, false},
    {"gcd", 2,
     {.n2 = num_gcd}, {0}, {0}, {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"gcdext", 2,
     {.n2 = benchmark_n_gcdext}, {0}, {0},
     {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"hacovercos", 1,
     {.n1 = num_hacovercos},
     {.r1 = qf_hacovercos}, {.c1 = qc_hacovercos},
     {"0.7"}, 1, false},
    {"hacoversin", 1,
     {.n1 = num_hacoversin},
     {.r1 = qf_hacoversin}, {.c1 = qc_hacoversin},
     {"0.7"}, 1, false},
    {"harmonic_poly", 2,
     {.n2 = num_harmonic_poly},
     {.r2 = benchmark_r_harmonic_poly}, {.c2 = benchmark_c_harmonic_poly},
     {"2", "0.7"}, 2, false},
    {"havercos", 1,
     {.n1 = num_havercos},
     {.r1 = qf_havercos}, {.c1 = qc_havercos},
     {"0.7"}, 1, false},
    {"haversin", 1,
     {.n1 = num_haversin},
     {.r1 = qf_haversin}, {.c1 = qc_haversin},
     {"0.7"}, 1, false},
    {"hermite_h", 2,
     {.n2 = num_hermite_h},
     {.r2 = benchmark_r_hermite_h}, {.c2 = benchmark_c_hermite_h},
     {"2", "0.7"}, 2, false},
    {"hypergeometric_pFq", 3,
     {.n3 = benchmark_n_hypergeometric_pFq},
     {.r3 = benchmark_r_hypergeometric_pFq}, {.c3 = benchmark_c_hypergeometric_pFq},
     {"0.7", "2.5", "0.3"}, 7, false},
    {"hypot", 2,
     {.n2 = num_hypot},
     {.r2 = qf_hypot}, {.c2 = qc_hypot},
     {"0.7", "1.2"}, 3, false},
    {"imag_part", 1,
     {.n1 = num_imag_part}, {0}, {.c1 = benchmark_c_imag_part}, {"0.7"}, 1, false},
    {"inv", 1,
     {.n1 = num_inv}, {0}, {.c1 = benchmark_c_inv}, {"0.7"}, 1, false},
    {"is_prime", 1,
     {.n1 = benchmark_n_is_prime}, {0}, {0},
     {benchmark_integer_prime}, 0, true},
    {"isqrt", 1,
     {.n1 = num_isqrt}, {0}, {0}, {benchmark_integer_wide}, 0, true},
    {"lambert_w0", 1,
     {.n1 = num_lambert_w0},
     {.r1 = qf_lambert_w0}, {.c1 = qc_productlog},
     {"0.7"}, 1, false},
    {"lambert_wm1", 1,
     {.n1 = num_lambert_wm1},
     {.r1 = qf_lambert_wm1}, {.c1 = qc_lambert_wm1},
     {"-0.2"}, 1, false},
    {"lambert_wn", 2,
     {.n2 = num_lambert_wn}, {0}, {.c2 = benchmark_c_lambert_wn}, {"1", "0.7"}, 2, false},
    {"lauricella_f", 3,
     {.n3 = benchmark_n_lauricella_f},
     {.r3 = benchmark_r_lauricella_f}, {.c3 = benchmark_c_lauricella_f},
     {"0.5", "0.3", "2.5"}, 7, false},
    {"lcm", 2,
     {.n2 = num_lcm}, {0}, {0}, {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"ldexp", 2,
     {.n2 = benchmark_n_ldexp},
     {.r2 = benchmark_r_ldexp}, {.c2 = benchmark_c_ldexp},
     {"1.23456789", "3"}, 1, false},
    {"legendre_chi", 2,
     {.n2 = num_legendre_chi},
     {.r2 = qf_legendre_chi}, {.c2 = qc_legendre_chi},
     {"2", "0.7"}, 2, false},
    {"lerch_phi", 3,
     {.n3 = num_lerch_phi},
     {.r3 = qf_lerch_phi}, {.c3 = qc_lerch_phi},
     {"0.3", "2", "1.4"}, 7, false},
    {"lg", 1,
     {.n1 = num_lg},
     {.r1 = qf_lg}, {.c1 = qc_lg},
     {"2.345678"}, 1, false},
    {"lgamma", 1,
     {.n1 = num_lgamma},
     {.r1 = qf_lgamma}, {.c1 = qc_lgamma},
     {"2.345"}, 1, false},
    {"ln", 1,
     {.n1 = num_ln},
     {.r1 = qf_ln}, {.c1 = qc_ln},
     {"2.345678"}, 1, false},
    {"log", 1,
     {.n1 = num_log},
     {.r1 = qf_log}, {.c1 = qc_log},
     {"2.345678"}, 1, false},
    {"log10", 1,
     {.n1 = num_log10},
     {.r1 = qf_log10}, {.c1 = qc_log10},
     {"2.345678"}, 1, false},
    {"logbeta", 2,
     {.n2 = num_logbeta},
     {.r2 = qf_logbeta}, {.c2 = qc_logbeta},
     {"2.3", "4.5"}, 3, false},
    {"logbeta_pdf", 3,
     {.n3 = num_logbeta_pdf},
     {.r3 = qf_logbeta_pdf}, {.c3 = qc_logbeta_pdf},
     {"0.5", "2.5", "3.5"}, 7, false},
    {"lommel_s", 3,
     {.n3 = num_lommel_s}, {.r3 = qf_lommel_s}, {.c3 = qc_lommel_s}, {"0.5", "0.7", "1.2"}, 7, false},
    {"mod", 2,
     {.n2 = num_mod}, {0}, {0}, {benchmark_integer_wide, benchmark_integer_divisor}, 0, true},
    {"modinv", 2,
     {.n2 = num_modinv}, {0}, {0}, {benchmark_integer_wide, benchmark_integer_prime}, 0, true},
    {"mul", 2,
     {.n2 = num_mul},
     {.r2 = qf_mul}, {.c2 = qc_mul},
     {"0.7", "0.7"}, 3, false},
    {"mul_long", 2,
     {.n2 = benchmark_n_mul_long}, {0}, {0},
     {"17", "3"}, 1, false},
    {"mul_pow10", 2,
     {.n2 = benchmark_n_mul_pow10}, {.r2 = benchmark_r_mul_pow10}, {0}, {"1.23456789", "3"}, 1, false},
    {"neg", 1,
     {.n1 = num_neg},
     {.r1 = qf_neg}, {.c1 = qc_neg},
     {"0.7"}, 1, false},
    {"next_prime", 1,
     {.n1 = num_next_prime}, {0}, {0}, {benchmark_integer_divisor}, 0, true},
    {"normal_cdf", 1,
     {.n1 = num_normal_cdf},
     {.r1 = qf_normal_cdf}, {.c1 = qc_normal_cdf},
     {"0.7"}, 1, false},
    {"normal_logpdf", 1,
     {.n1 = num_normal_logpdf},
     {.r1 = qf_normal_logpdf}, {.c1 = qc_normal_logpdf},
     {"0.7"}, 1, false},
    {"normal_pdf", 1,
     {.n1 = num_normal_pdf},
     {.r1 = qf_normal_pdf}, {.c1 = qc_normal_pdf},
     {"0.7"}, 1, false},
    {"partition", 1,
     {.n1 = num_partition}, {0}, {0}, {"5000"}, 0, true},
    {"polygamma", 2,
     {.n2 = benchmark_n_polygamma},
     {.r2 = benchmark_r_polygamma}, {.c2 = benchmark_c_polygamma},
     {"3", "2.345"}, 2, false},
    {"polylog", 2,
     {.n2 = num_polylog},
     {.r2 = qf_polylog}, {.c2 = qc_polylog},
     {"2", "0.7"}, 2, false},
    {"polylog1", 1,
     {.n1 = num_polylog1},
     {.r1 = qf_polylog1}, {.c1 = qc_polylog1},
     {"0.7"}, 1, false},
    {"pow", 2,
     {.n2 = num_pow},
     {.r2 = qf_pow}, {.c2 = qc_pow},
     {"1.23456789", "3.5"}, 3, false},
    {"pow10", 1,
     {.n1 = benchmark_n_pow10}, {.r1 = benchmark_r_pow10}, {0}, {"3"}, 0, false},
    {"pow_int", 2,
     {.n2 = benchmark_n_pow_int}, {.r2 = benchmark_r_pow_int}, {0}, {"1.23456789", "3"}, 1, false},
    {"powmod", 3,
     {.n3 = num_powmod}, {0}, {0},
     {benchmark_integer_wide, benchmark_integer_divisor, benchmark_integer_prime}, 0, true},
    {"prev_prime", 1,
     {.n1 = num_prev_prime}, {0}, {0}, {benchmark_integer_divisor}, 0, true},
    {"productlog", 1,
     {.n1 = num_productlog},
     {.r1 = qf_productlog}, {.c1 = qc_productlog},
     {"0.7"}, 1, false},
    {"prove_prime", 1,
     {.n1 = benchmark_n_prove_prime}, {0}, {0},
     {benchmark_integer_prime}, 0, true},
    {"qdigamma", 2,
     {.n2 = num_qdigamma},
     {.r2 = qf_qdigamma}, {.c2 = qc_qdigamma},
     {"0.5", "2.3"}, 3, false},
    {"real_part", 1,
     {.n1 = num_real_part}, {0}, {.c1 = benchmark_c_real_part}, {"0.7"}, 1, false},
    {"rect", 1,
     {.n1 = num_rect},
     {.r1 = qf_rect}, {.c1 = qc_rect},
     {"0.7"}, 0, false},
    {"root", 2,
     {.n2 = num_root},
     {.r2 = benchmark_r_root}, {.c2 = benchmark_c_root},
     {"1.2345", "3"}, 1, false},
    {"round", 1,
     {.n1 = num_round},
     {.r1 = qf_round}, {.c1 = qc_round},
     {"0.7"}, 1, false},
    {"roundn", 2,
     {.n2 = benchmark_n_roundn},
     {.r2 = benchmark_r_roundn}, {.c2 = benchmark_c_roundn},
     {"1.23456789", "3"}, 1, false},
    {"rounds", 2,
     {.n2 = benchmark_n_rounds},
     {.r2 = benchmark_r_rounds}, {.c2 = benchmark_c_rounds},
     {"1.23456789", "3"}, 1, false},
    {"sec", 1,
     {.n1 = num_sec},
     {.r1 = qf_sec}, {.c1 = qc_sec},
     {"0.7"}, 1, false},
    {"sech", 1,
     {.n1 = num_sech},
     {.r1 = qf_sech}, {.c1 = qc_sech},
     {"0.7"}, 1, false},
    {"set_bit", 2,
     {.n2 = benchmark_n_set_bit}, {0}, {0},
     {benchmark_integer_wide, "700"}, 0, true},
    {"sgn", 1,
     {.n1 = num_sgn},
     {.r1 = qf_sgn}, {.c1 = qc_sgn},
     {"0.7"}, 0, false},
    {"shl", 2,
     {.n2 = benchmark_n_shl}, {0}, {0},
     {benchmark_integer_wide, "257"}, 0, true},
    {"shr", 2,
     {.n2 = benchmark_n_shr}, {0}, {0},
     {benchmark_integer_wide, "257"}, 0, true},
    {"sin", 1,
     {.n1 = num_sin},
     {.r1 = qf_sin}, {.c1 = qc_sin},
     {"0.567"}, 1, false},
    {"sinc", 1,
     {.n1 = num_sinc},
     {.r1 = qf_sinc}, {.c1 = qc_sinc},
     {"0.7"}, 1, false},
    {"sincos", 1,
     {.n1 = benchmark_n_sincos}, {0}, {0}, {"0.7"}, 1, false},
    {"sinh", 1,
     {.n1 = num_sinh},
     {.r1 = qf_sinh}, {.c1 = qc_sinh},
     {"0.7"}, 1, false},
    {"sinhcosh", 1,
     {.n1 = benchmark_n_sinhcosh}, {0}, {0}, {"0.7"}, 1, false},
    {"sqr", 1,
     {.n1 = num_sqr},
     {.r1 = qf_sqr}, {.c1 = benchmark_c_sqr},
     {"0.7"}, 1, false},
    {"sqrt", 1,
     {.n1 = num_sqrt},
     {.r1 = qf_sqrt}, {.c1 = qc_sqrt},
     {"1.23456789"}, 1, false},
    {"step", 1,
     {.n1 = num_step},
     {.r1 = qf_step}, {.c1 = qc_step},
     {"0.7"}, 0, false},
    {"struve_h", 2,
     {.n2 = num_struve_h},
     {.r2 = qf_struve_h}, {.c2 = qc_struve_h},
     {"2", "0.7"}, 2, false},
    {"struve_l", 2,
     {.n2 = num_struve_l},
     {.r2 = qf_struve_l}, {.c2 = qc_struve_l},
     {"2", "0.7"}, 2, false},
    {"sub", 2,
     {.n2 = num_sub},
     {.r2 = qf_sub}, {.c2 = qc_sub},
     {"0.7", "0.7"}, 3, false},
    {"tan", 1,
     {.n1 = num_tan},
     {.r1 = qf_tan}, {.c1 = qc_tan},
     {"0.7"}, 1, false},
    {"tanh", 1,
     {.n1 = num_tanh},
     {.r1 = qf_tanh}, {.c1 = qc_tanh},
     {"0.7"}, 1, false},
    {"test_bit", 2,
     {.n2 = benchmark_n_test_bit}, {0}, {0},
     {benchmark_integer_wide, "701"}, 0, true},
    {"tetragamma", 1,
     {.n1 = num_tetragamma},
     {.r1 = qf_tetragamma}, {.c1 = qc_tetragamma},
     {"2.345"}, 1, false},
    {"tri", 1,
     {.n1 = num_tri},
     {.r1 = qf_tri}, {.c1 = qc_tri},
     {"0.7"}, 0, false},
    {"trigamma", 1,
     {.n1 = num_trigamma},
     {.r1 = qf_trigamma}, {.c1 = qc_trigamma},
     {"2.345"}, 1, false},
    {"vercos", 1,
     {.n1 = num_vercos},
     {.r1 = qf_vercos}, {.c1 = qc_vercos},
     {"0.7"}, 1, false},
    {"versin", 1,
     {.n1 = num_versin},
     {.r1 = qf_versin}, {.c1 = qc_versin},
     {"0.7"}, 1, false},
    {"zatahp", 2,
     {.n2 = num_zatahp},
     {.r2 = qf_zatahp}, {.c2 = qc_zatahp},
     {"2.3", "1.4"}, 3, false},
    {"zeta", 1,
     {.n1 = num_zeta},
     {.r1 = qf_zeta}, {.c1 = qc_zeta},
     {"2.345"}, 1, false},
    {"zetah", 2,
     {.n2 = num_zetah},
     {.r2 = qf_zetah}, {.c2 = qc_zetah},
     {"2.3", "1.4"}, 3, false},
    {"zetap", 1,
     {.n1 = num_zetap},
     {.r1 = qf_zetap}, {.c1 = qc_zetap},
     {"2.345"}, 1, false},
};

static int benchmark_operation_compare(const void *key, const void *entry)
{
    return strcmp(key, ((const benchmark_operation_t *)entry)->name);
}

/* Return an indexed operation without scanning the mathematical function catalogue. */
const benchmark_operation_t *benchmark_operation(const char *name)
{
    return name ? bsearch(name, operations, sizeof operations / sizeof *operations,
                          sizeof *operations, benchmark_operation_compare) : NULL;
}

/* Expose the immutable catalogue to storage synchronisation. */
const benchmark_operation_t *benchmark_operations(size_t *count)
{
    *count = sizeof operations / sizeof *operations;
    return operations;
}

/* Inspect only the function-pointer member matching the operation's declared arity. */
bool benchmark_operation_supported(const benchmark_operation_t *op, bool complex_backend)
{
    if (!op) return false;
    if (op->arity == 1) return complex_backend ? op->complex_fn.c1 != NULL : op->real.r1 != NULL;
    if (op->arity == 2) return complex_backend ? op->complex_fn.c2 != NULL : op->real.r2 != NULL;
    if (op->arity == 3) return complex_backend ? op->complex_fn.c3 != NULL : op->real.r3 != NULL;
    return op->arity == 6 && (complex_backend ? op->complex_fn.c6 != NULL : op->real.r6 != NULL);
}
