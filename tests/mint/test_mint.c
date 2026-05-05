#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "mint.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

static void assert_mint_string(const mint_t *mint, const char *expected)
{
    char *got = mi_to_string(mint);

    ASSERT_NOT_NULL(got);
    printf("    expected = %s\n", expected);
    printf("    got      = %s\n", got);
    ASSERT_TRUE(strcmp(got, expected) == 0);
    free(got);
}

static void assert_mint_hex(const mint_t *mint, const char *expected)
{
    char *got = mi_to_hex(mint);

    ASSERT_NOT_NULL(got);
    printf("    expected hex = %s\n", expected);
    printf("    got hex      = %s\n", got);
    ASSERT_TRUE(strcmp(got, expected) == 0);
    free(got);
}

static void print_mint_input(const char *label, const mint_t *mint)
{
    char *text = mi_to_string(mint);

    ASSERT_NOT_NULL(text);
    printf("%s%s%s = %s%s%s\n", C_CYAN, label, C_RESET, C_WHITE, text, C_RESET);
    free(text);
}

static void assert_factor_pair(const mint_factor_t *factor,
                               const char *expected_prime,
                               unsigned long expected_exponent)
{
    char *prime = mi_to_string(factor->prime);

    ASSERT_NOT_NULL(prime);
    printf("    expected prime    = %s\n", expected_prime);
    printf("    got prime         = %s\n", prime);
    printf("    expected exponent = %lu\n", expected_exponent);
    printf("    got exponent      = %lu\n", factor->exponent);
    ASSERT_TRUE(strcmp(prime, expected_prime) == 0);
    ASSERT_EQ_LONG((long)factor->exponent, (long)expected_exponent);
    free(prime);
}

void test_create_and_to_string(void)
{
    mint_t *zero = mi_new();
    mint_t *pow2_zero = mi_create_2pow(0);
    mint_t *pos = mi_create_long(65);
    mint_t *pow2_small = mi_create_2pow(5);
    mint_t *pow2_big = mi_create_2pow(64);
    mint_t *pow2_huge = mi_create_2pow(127);
    mint_t *neg = mi_create_long(-12345);
    mint_t *big = mi_create_string("18446744073709551616");

    ASSERT_NOT_NULL(zero);
    ASSERT_NOT_NULL(pow2_zero);
    ASSERT_NOT_NULL(pos);
    ASSERT_NOT_NULL(pow2_small);
    ASSERT_NOT_NULL(pow2_big);
    ASSERT_NOT_NULL(pow2_huge);
    ASSERT_NOT_NULL(neg);
    ASSERT_NOT_NULL(big);

    print_mint_input("zero", zero);
    assert_mint_string(zero, "0");

    print_mint_input("pow2_zero", pow2_zero);
    assert_mint_string(pow2_zero, "1");

    print_mint_input("pos", pos);
    assert_mint_string(pos, "65");

    print_mint_input("pow2_small", pow2_small);
    assert_mint_string(pow2_small, "32");

    print_mint_input("pow2_big", pow2_big);
    assert_mint_string(pow2_big, "18446744073709551616");

    print_mint_input("pow2_huge", pow2_huge);
    assert_mint_string(pow2_huge, "170141183460469231731687303715884105728");

    print_mint_input("neg", neg);
    assert_mint_string(neg, "-12345");

    print_mint_input("big", big);
    assert_mint_string(big, "18446744073709551616");

    mi_free(zero);
    mi_free(pow2_zero);
    mi_free(pos);
    mi_free(pow2_small);
    mi_free(pow2_big);
    mi_free(pow2_huge);
    mi_free(neg);
    mi_free(big);
}

void test_setters_and_clear(void)
{
    mint_t *mint = mi_new();

    ASSERT_NOT_NULL(mint);
    ASSERT_EQ_INT(mi_set_long(mint, -42), 0);
    print_mint_input("set_long(-42)", mint);
    assert_mint_string(mint, "-42");

    ASSERT_EQ_INT(mi_set_string(mint, "  +900000000000000000000 "), 0);
    print_mint_input("set_string(\"  +900000000000000000000 \")", mint);
    assert_mint_string(mint, "900000000000000000000");

    ASSERT_EQ_INT(mi_set_string(mint, "not-a-number"), -1);
    print_mint_input("after failed set_string(\"not-a-number\")", mint);
    assert_mint_string(mint, "0");

    mi_clear(mint);
    print_mint_input("after mi_clear", mint);
    assert_mint_string(mint, "0");
    mi_free(mint);
}

void test_ulong_and_hex_constructors(void)
{
    mint_t *u = mi_create_ulong(ULONG_MAX);
    mint_t *h = mi_create_hex("0x10000000000000000");
    mint_t *m = mi_new();

    ASSERT_NOT_NULL(u);
    ASSERT_NOT_NULL(h);
    ASSERT_NOT_NULL(m);

    print_mint_input("u from create_ulong(ULONG_MAX)", u);
    assert_mint_string(u, "18446744073709551615");

    print_mint_input("h from create_hex", h);
    assert_mint_string(h, "18446744073709551616");
    assert_mint_hex(h, "10000000000000000");

    ASSERT_EQ_INT(mi_set_ulong(m, 42ul), 0);
    print_mint_input("m after set_ulong(42)", m);
    assert_mint_string(m, "42");

    mi_free(u);
    mi_free(h);
    mi_free(m);
}

void test_clone_copies_value(void)
{
    mint_t *a = mi_create_long(7);
    mint_t *b;

    ASSERT_NOT_NULL(a);
    b = mi_clone(a);
    ASSERT_NOT_NULL(b);

    print_mint_input("clone source a", a);
    print_mint_input("clone alias b", b);
    ASSERT_EQ_INT(mi_add(b, MI_ONE), 0);
    print_mint_input("a after mi_add(b, 1)", a);
    print_mint_input("b after mi_add(b, 1)", b);
    assert_mint_string(a, "7");
    assert_mint_string(b, "8");

    mi_free(b);
    mi_free(a);
}

void test_addition(void)
{
    mint_t *a = mi_create_string("18446744073709551616");
    mint_t *b = mi_create_long(5);
    mint_t *c = mi_create_long(-100);
    mint_t *d = mi_create_long(30);
    mint_t *e = mi_create_long(50);
    mint_t *f = mi_create_long(-50);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(f);

    print_mint_input("a before add", a);
    print_mint_input("b before add", b);
    ASSERT_EQ_INT(mi_add(a, b), 0);
    print_mint_input("a after a += b", a);
    assert_mint_string(a, "18446744073709551621");

    print_mint_input("c before add", c);
    print_mint_input("d before add", d);
    ASSERT_EQ_INT(mi_add(c, d), 0);
    print_mint_input("c after c += d", c);
    assert_mint_string(c, "-70");

    print_mint_input("e before add", e);
    print_mint_input("f before add", f);
    ASSERT_EQ_INT(mi_add(e, f), 0);
    print_mint_input("e after e += f", e);
    assert_mint_string(e, "0");

    print_mint_input("e before add_long", e);
    ASSERT_EQ_INT(mi_add_long(e, 123), 0);
    print_mint_input("e after e += 123", e);
    assert_mint_string(e, "123");

    print_mint_input("e before add_long(-200)", e);
    ASSERT_EQ_INT(mi_add_long(e, -200), 0);
    print_mint_input("e after e += -200", e);
    assert_mint_string(e, "-77");

    mi_free(a);
    mi_free(b);
    mi_free(c);
    mi_free(d);
    mi_free(e);
    mi_free(f);
}

void test_multiplication(void)
{
    mint_t *a = mi_create_string("18446744073709551616");
    mint_t *b = mi_create_long(10);
    mint_t *c = mi_create_long(-12);
    mint_t *d = mi_create_long(-12);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);

    print_mint_input("a before mul", a);
    print_mint_input("b before mul", b);
    ASSERT_EQ_INT(mi_mul(a, b), 0);
    print_mint_input("a after a *= b", a);
    assert_mint_string(a, "184467440737095516160");

    print_mint_input("c before mul", c);
    print_mint_input("d before mul", d);
    ASSERT_EQ_INT(mi_mul(c, d), 0);
    print_mint_input("c after c *= d", c);
    assert_mint_string(c, "144");

    print_mint_input("c before mul_long(-3)", c);
    ASSERT_EQ_INT(mi_mul_long(c, -3), 0);
    print_mint_input("c after c *= -3", c);
    assert_mint_string(c, "-432");

    mi_free(a);
    mi_free(b);
    mi_free(c);
    mi_free(d);
}

void test_division_and_modulo(void)
{
    mint_t *a = mi_create_string("18446744073709551621");
    mint_t *b = mi_create_long(10);
    mint_t *rem = mi_new();
    mint_t *c = mi_create_long(-17);
    mint_t *d = mi_create_long(5);
    mint_t *modv = mi_create_long(100);
    mint_t *modd = mi_create_long(9);
    long rem_long = 0;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(rem);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(modv);
    ASSERT_NOT_NULL(modd);

    print_mint_input("a before div", a);
    print_mint_input("b before div", b);
    ASSERT_EQ_INT(mi_div(a, b, rem), 0);
    print_mint_input("a after a /= b", a);
    print_mint_input("rem after a % b", rem);
    assert_mint_string(a, "1844674407370955162");
    assert_mint_string(rem, "1");

    print_mint_input("c before div", c);
    print_mint_input("d before div", d);
    ASSERT_EQ_INT(mi_div(c, d, rem), 0);
    print_mint_input("c after c /= d", c);
    print_mint_input("rem after c % d", rem);
    assert_mint_string(c, "-3");
    assert_mint_string(rem, "-2");

    print_mint_input("modv before mod", modv);
    print_mint_input("modd before mod", modd);
    ASSERT_EQ_INT(mi_mod(modv, modd), 0);
    print_mint_input("modv after modv %= modd", modv);
    assert_mint_string(modv, "1");

    print_mint_input("modv before div_long", modv);
    ASSERT_EQ_INT(mi_add_long(modv, 100), 0);
    print_mint_input("modv after +100", modv);
    ASSERT_EQ_INT(mi_div_long(modv, -10, &rem_long), 0);
    print_mint_input("modv after /= -10", modv);
    assert_mint_string(modv, "-10");
    printf("    expected rem_long = 1\n");
    printf("    got rem_long      = %ld\n", rem_long);
    ASSERT_EQ_LONG(rem_long, 1);

    mi_free(a);
    mi_free(b);
    mi_free(rem);
    mi_free(c);
    mi_free(d);
    mi_free(modv);
    mi_free(modd);
}

void test_pow_and_pow10(void)
{
    mint_t *a = mi_create_long(12);
    mint_t *b = mi_create_long(-2);
    mint_t *c = mi_create_long(123);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    print_mint_input("a before pow", a);
    ASSERT_EQ_INT(mi_pow(a, 5), 0);
    print_mint_input("a after a^=5", a);
    assert_mint_string(a, "248832");

    print_mint_input("b before pow", b);
    ASSERT_EQ_INT(mi_pow(b, 9), 0);
    print_mint_input("b after b^=9", b);
    assert_mint_string(b, "-512");

    print_mint_input("c before pow10", c);
    ASSERT_EQ_INT(mi_pow10(c, 3), 0);
    print_mint_input("c after c *= 10^3", c);
    assert_mint_string(c, "123000");

    mi_free(a);
    mi_free(b);
    mi_free(c);
}

void test_factorial_fibonacci_and_binomial(void)
{
    mint_t *fact0 = mi_new();
    mint_t *fact10 = mi_new();
    mint_t *fib0 = mi_new();
    mint_t *fib1 = mi_new();
    mint_t *fib50 = mi_new();
    mint_t *bin_small = mi_new();
    mint_t *bin_large = mi_new();

    ASSERT_NOT_NULL(fact0);
    ASSERT_NOT_NULL(fact10);
    ASSERT_NOT_NULL(fib0);
    ASSERT_NOT_NULL(fib1);
    ASSERT_NOT_NULL(fib50);
    ASSERT_NOT_NULL(bin_small);
    ASSERT_NOT_NULL(bin_large);

    ASSERT_EQ_INT(mi_factorial(fact0, 0), 0);
    print_mint_input("0!", fact0);
    assert_mint_string(fact0, "1");

    ASSERT_EQ_INT(mi_factorial(fact10, 10), 0);
    print_mint_input("10!", fact10);
    assert_mint_string(fact10, "3628800");

    ASSERT_EQ_INT(mi_fibonacci(fib0, 0), 0);
    print_mint_input("F(0)", fib0);
    assert_mint_string(fib0, "0");

    ASSERT_EQ_INT(mi_fibonacci(fib1, 1), 0);
    print_mint_input("F(1)", fib1);
    assert_mint_string(fib1, "1");

    ASSERT_EQ_INT(mi_fibonacci(fib50, 50), 0);
    print_mint_input("F(50)", fib50);
    assert_mint_string(fib50, "12586269025");

    ASSERT_EQ_INT(mi_binomial(bin_small, 5, 2), 0);
    print_mint_input("C(5, 2)", bin_small);
    assert_mint_string(bin_small, "10");

    ASSERT_EQ_INT(mi_binomial(bin_large, 52, 5), 0);
    print_mint_input("C(52, 5)", bin_large);
    assert_mint_string(bin_large, "2598960");

    ASSERT_EQ_INT(mi_binomial(bin_large, 5, 6), -1);

    mi_free(fact0);
    mi_free(fact10);
    mi_free(fib0);
    mi_free(fib1);
    mi_free(fib50);
    mi_free(bin_small);
    mi_free(bin_large);
}

void test_compare_and_negate(void)
{
    mint_t *a = mi_create_long(5);
    mint_t *b = mi_create_long(-7);
    mint_t *c = mi_create_long(5);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    print_mint_input("a before cmp", a);
    print_mint_input("b before cmp", b);
    ASSERT_EQ_INT(mi_cmp(a, b), 1);

    print_mint_input("a before cmp with c", a);
    print_mint_input("c before cmp", c);
    ASSERT_EQ_INT(mi_cmp(a, c), 0);

    print_mint_input("b before neg", b);
    ASSERT_EQ_INT(mi_neg(b), 0);
    print_mint_input("b after neg", b);
    assert_mint_string(b, "7");

    mi_free(a);
    mi_free(b);
    mi_free(c);
}

void test_inc_dec_and_bits(void)
{
    mint_t *a = mi_create_long(0);
    mint_t *b = mi_create_string("18446744073709551616");
    mint_t *c = mi_create_long(-2);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    print_mint_input("a before inc", a);
    ASSERT_EQ_INT(mi_inc(a), 0);
    print_mint_input("a after inc", a);
    assert_mint_string(a, "1");

    ASSERT_EQ_INT(mi_dec(a), 0);
    print_mint_input("a after dec", a);
    assert_mint_string(a, "0");

    print_mint_input("c before dec", c);
    ASSERT_EQ_INT(mi_dec(c), 0);
    print_mint_input("c after dec", c);
    assert_mint_string(c, "-3");

    print_mint_input("b for bit tests", b);
    ASSERT_EQ_LONG((long)mi_bit_length(a), 0);
    ASSERT_EQ_LONG((long)mi_bit_length(b), 65);
    ASSERT_TRUE(mi_test_bit(b, 64));
    ASSERT_TRUE(!mi_test_bit(b, 63));
    ASSERT_TRUE(!mi_test_bit(c, 100));

    mi_free(a);
    mi_free(b);
    mi_free(c);
}

void test_shifts(void)
{
    mint_t *a = mi_create_long(3);
    mint_t *b = mi_create_string("18446744073709551616");
    mint_t *c = mi_create_long(-9);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    print_mint_input("a before shl", a);
    ASSERT_EQ_INT(mi_shl(a, 65), 0);
    print_mint_input("a after a <<= 65", a);
    assert_mint_string(a, "110680464442257309696");

    print_mint_input("b before shr", b);
    ASSERT_EQ_INT(mi_shr(b, 64), 0);
    print_mint_input("b after b >>= 64", b);
    assert_mint_string(b, "1");

    print_mint_input("c before shr", c);
    ASSERT_EQ_INT(mi_shr(c, 1), 0);
    print_mint_input("c after c >>= 1", c);
    assert_mint_string(c, "-4");

    mi_free(a);
    mi_free(b);
    mi_free(c);
}

void test_integer_sqrt(void)
{
    mint_t *a = mi_create_long(144);
    mint_t *b = mi_create_long(200);
    mint_t *c = mi_create_string("18446744073709551616");

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    print_mint_input("a before sqrt", a);
    ASSERT_EQ_INT(mi_sqrt(a), 0);
    print_mint_input("a after isqrt", a);
    assert_mint_string(a, "12");

    print_mint_input("b before sqrt", b);
    ASSERT_EQ_INT(mi_sqrt(b), 0);
    print_mint_input("b after isqrt", b);
    assert_mint_string(b, "14");

    print_mint_input("c before sqrt", c);
    ASSERT_EQ_INT(mi_sqrt(c), 0);
    print_mint_input("c after isqrt", c);
    assert_mint_string(c, "4294967296");

    mi_free(a);
    mi_free(b);
    mi_free(c);
}

void test_bitwise(void)
{
    mint_t *a = mi_create_long(13);
    mint_t *b = mi_create_long(11);
    mint_t *c = mi_create_long(13);
    mint_t *d = mi_create_long(11);
    mint_t *e = mi_create_long(13);
    mint_t *f = mi_create_long(8);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(f);

    print_mint_input("a before and", a);
    print_mint_input("b before and", b);
    ASSERT_EQ_INT(mi_and(a, b), 0);
    print_mint_input("a after a &= b", a);
    assert_mint_string(a, "9");

    print_mint_input("c before or", c);
    print_mint_input("d before or", d);
    ASSERT_EQ_INT(mi_or(c, d), 0);
    print_mint_input("c after c |= d", c);
    assert_mint_string(c, "15");

    print_mint_input("e before xor", e);
    print_mint_input("f before xor", f);
    ASSERT_EQ_INT(mi_xor(e, f), 0);
    print_mint_input("e after e ^= f", e);
    assert_mint_string(e, "5");

    print_mint_input("f before not", f);
    ASSERT_EQ_INT(mi_not(f), 0);
    print_mint_input("f after ~f over active width", f);
    assert_mint_string(f, "7");

    mi_free(a);
    mi_free(b);
    mi_free(c);
    mi_free(d);
    mi_free(e);
    mi_free(f);
}

void test_powmod_isprime_and_factors(void)
{
    mint_t *base = mi_create_long(4);
    mint_t *exp = mi_create_long(13);
    mint_t *mod = mi_create_long(497);
    mint_t *prime = mi_create_long(97);
    mint_t *composite = mi_create_long(221);
    mint_t *big_prime = mi_create_long(999983);
    mint_t *big_composite = mi_create_long(999985);
    mint_t *ec_prime = mi_create_long(137);
    mint_t *carmichael = mi_create_long(561);
    mint_t *fermat_composite = mi_create_string("18446744073709551617");
    mint_t *mersenne_127 = mi_create_2pow(127);
    mint_t *mersenne_67 = mi_create_2pow(67);
    mint_t *minus_one = mi_create_long(-1);
    mint_t *n = mi_create_long(360);
    mint_t *big_n = mi_create_string("1000036000099");
    mint_factors_t *factors;

    ASSERT_NOT_NULL(base);
    ASSERT_NOT_NULL(exp);
    ASSERT_NOT_NULL(mod);
    ASSERT_NOT_NULL(prime);
    ASSERT_NOT_NULL(composite);
    ASSERT_NOT_NULL(big_prime);
    ASSERT_NOT_NULL(big_composite);
    ASSERT_NOT_NULL(ec_prime);
    ASSERT_NOT_NULL(carmichael);
    ASSERT_NOT_NULL(fermat_composite);
    ASSERT_NOT_NULL(mersenne_127);
    ASSERT_NOT_NULL(mersenne_67);
    ASSERT_NOT_NULL(minus_one);
    ASSERT_NOT_NULL(n);
    ASSERT_NOT_NULL(big_n);

    print_mint_input("base before powmod", base);
    print_mint_input("exp before powmod", exp);
    print_mint_input("mod before powmod", mod);
    ASSERT_EQ_INT(mi_powmod(base, exp, mod), 0);
    print_mint_input("base after base^exp mod mod", base);
    assert_mint_string(base, "445");

    print_mint_input("prime candidate", prime);
    ASSERT_TRUE(mi_isprime(prime));
    ASSERT_EQ_INT(mi_prove_prime(prime), MI_PRIMALITY_PRIME);

    print_mint_input("composite candidate", composite);
    ASSERT_TRUE(!mi_isprime(composite));
    ASSERT_EQ_INT(mi_prove_prime(composite), MI_PRIMALITY_COMPOSITE);

    print_mint_input("large prime candidate", big_prime);
    ASSERT_TRUE(mi_isprime(big_prime));

    print_mint_input("large composite candidate", big_composite);
    ASSERT_TRUE(!mi_isprime(big_composite));

    print_mint_input("n+1 EC candidate", ec_prime);
    ASSERT_TRUE(mi_isprime(ec_prime));
    ASSERT_EQ_INT(mi_prove_prime(ec_prime), MI_PRIMALITY_PRIME);

    print_mint_input("carmichael candidate", carmichael);
    ASSERT_TRUE(!mi_isprime(carmichael));
    ASSERT_EQ_INT(mi_prove_prime(carmichael), MI_PRIMALITY_COMPOSITE);

    print_mint_input("F5 composite candidate", fermat_composite);
    ASSERT_TRUE(!mi_isprime(fermat_composite));
    ASSERT_EQ_INT(mi_prove_prime(fermat_composite), MI_PRIMALITY_COMPOSITE);

    ASSERT_EQ_INT(mi_add(mersenne_127, minus_one), 0);
    print_mint_input("M_127 candidate", mersenne_127);
    ASSERT_TRUE(mi_isprime(mersenne_127));
    ASSERT_EQ_INT(mi_prove_prime(mersenne_127), MI_PRIMALITY_PRIME);

    ASSERT_EQ_INT(mi_add(mersenne_67, minus_one), 0);
    print_mint_input("M_67 candidate", mersenne_67);
    ASSERT_TRUE(!mi_isprime(mersenne_67));
    ASSERT_EQ_INT(mi_prove_prime(mersenne_67), MI_PRIMALITY_COMPOSITE);

    print_mint_input("n before factors", n);
    factors = mi_factors(n);
    ASSERT_NOT_NULL(factors);
    ASSERT_EQ_LONG((long)factors->count, 3);
    assert_factor_pair(&factors->items[0], "2", 3);
    assert_factor_pair(&factors->items[1], "3", 2);
    assert_factor_pair(&factors->items[2], "5", 1);
    mi_factors_free(factors);

    print_mint_input("big_n before factors", big_n);
    factors = mi_factors(big_n);
    ASSERT_NOT_NULL(factors);
    ASSERT_EQ_LONG((long)factors->count, 2);
    assert_factor_pair(&factors->items[0], "1000003", 1);
    assert_factor_pair(&factors->items[1], "1000033", 1);

    mi_factors_free(factors);
    mi_free(base);
    mi_free(exp);
    mi_free(mod);
    mi_free(prime);
    mi_free(composite);
    mi_free(big_prime);
    mi_free(big_composite);
    mi_free(ec_prime);
    mi_free(carmichael);
    mi_free(fermat_composite);
    mi_free(mersenne_127);
    mi_free(mersenne_67);
    mi_free(minus_one);
    mi_free(n);
    mi_free(big_n);
}

void test_sub_abs_predicates_conversions_gcd_lcm(void)
{
    mint_t *a = mi_create_long(100);
    mint_t *b = mi_create_long(30);
    mint_t *c = mi_create_long(-42);
    mint_t *d = mi_create_string("9223372036854775808");
    mint_t *g = mi_create_long(84);
    mint_t *h = mi_create_long(30);
    mint_t *l = mi_create_long(-21);
    mint_t *m = mi_create_long(6);
    long value = 0;

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(g);
    ASSERT_NOT_NULL(h);
    ASSERT_NOT_NULL(l);
    ASSERT_NOT_NULL(m);

    print_mint_input("a before sub", a);
    print_mint_input("b before sub", b);
    ASSERT_EQ_INT(mi_sub(a, b), 0);
    print_mint_input("a after a -= b", a);
    assert_mint_string(a, "70");

    print_mint_input("c before abs", c);
    ASSERT_EQ_INT(mi_abs(c), 0);
    print_mint_input("c after abs", c);
    assert_mint_string(c, "42");

    ASSERT_TRUE(!mi_is_zero(a));
    ASSERT_TRUE(mi_is_zero(MI_ZERO));
    ASSERT_TRUE(!mi_is_negative(a));
    ASSERT_TRUE(mi_is_even(c));
    ASSERT_TRUE(!mi_is_odd(c));

    ASSERT_TRUE(mi_fits_long(c));
    ASSERT_TRUE(mi_get_long(c, &value));
    printf("    expected long = 42\n");
    printf("    got long      = %ld\n", value);
    ASSERT_EQ_LONG(value, 42);

    ASSERT_TRUE(!mi_fits_long(d));
    ASSERT_TRUE(!mi_get_long(d, &value));

    print_mint_input("g before gcd", g);
    print_mint_input("h before gcd", h);
    ASSERT_EQ_INT(mi_gcd(g, h), 0);
    print_mint_input("g after gcd(g, h)", g);
    assert_mint_string(g, "6");

    print_mint_input("l before lcm", l);
    print_mint_input("m before lcm", m);
    ASSERT_EQ_INT(mi_lcm(l, m), 0);
    print_mint_input("l after lcm(l, m)", l);
    assert_mint_string(l, "42");

    mi_free(a);
    mi_free(b);
    mi_free(c);
    mi_free(d);
    mi_free(g);
    mi_free(h);
    mi_free(l);
    mi_free(m);
}

void test_long_hex_bits_and_nextprime(void)
{
    mint_t *a = mi_create_long(100);
    mint_t *b = mi_create_string("18446744073709551616");
    mint_t *c = mi_new();
    mint_t *d = mi_create_long(14);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);

    print_mint_input("a before sub_long", a);
    ASSERT_EQ_INT(mi_sub_long(a, 125), 0);
    print_mint_input("a after -= 125", a);
    assert_mint_string(a, "-25");

    print_mint_input("a before mod_long", a);
    ASSERT_EQ_INT(mi_mod_long(a, 7), 0);
    print_mint_input("a after %= 7", a);
    assert_mint_string(a, "-4");

    ASSERT_EQ_INT(mi_cmp_long(a, -4), 0);
    ASSERT_TRUE(mi_cmp_long(a, 0) < 0);
    ASSERT_TRUE(mi_cmp_long(b, LONG_MAX) > 0);

    ASSERT_EQ_INT(mi_set_hex(c, "  -0x1fF  "), 0);
    print_mint_input("c from hex", c);
    assert_mint_string(c, "-511");
    assert_mint_hex(c, "-1ff");

    ASSERT_EQ_INT(mi_set_long(c, 0), 0);
    ASSERT_EQ_INT(mi_set_bit(c, 4), 0);
    ASSERT_EQ_INT(mi_set_bit(c, 0), 0);
    print_mint_input("c after set bits 4 and 0", c);
    assert_mint_string(c, "17");
    ASSERT_EQ_INT(mi_clear_bit(c, 4), 0);
    print_mint_input("c after clear bit 4", c);
    assert_mint_string(c, "1");

    print_mint_input("d before nextprime", d);
    ASSERT_EQ_INT(mi_nextprime(d), 0);
    print_mint_input("d after nextprime", d);
    assert_mint_string(d, "17");

    ASSERT_EQ_INT(mi_set_long(d, -10), 0);
    ASSERT_EQ_INT(mi_nextprime(d), 0);
    print_mint_input("d after nextprime from -10", d);
    assert_mint_string(d, "2");

    ASSERT_EQ_INT(mi_set_long(d, 14), 0);
    ASSERT_EQ_INT(mi_prevprime(d), 0);
    print_mint_input("d after prevprime from 14", d);
    assert_mint_string(d, "13");

    ASSERT_EQ_INT(mi_set_long(d, 4), 0);
    ASSERT_EQ_INT(mi_prevprime(d), 0);
    print_mint_input("d after prevprime from 4", d);
    assert_mint_string(d, "3");

    ASSERT_EQ_INT(mi_set_long(d, 1000000), 0);
    ASSERT_EQ_INT(mi_nextprime(d), 0);
    print_mint_input("d after nextprime from 1000000", d);
    assert_mint_string(d, "1000003");

    ASSERT_EQ_INT(mi_set_long(d, 1000000), 0);
    ASSERT_EQ_INT(mi_prevprime(d), 0);
    print_mint_input("d after prevprime from 1000000", d);
    assert_mint_string(d, "999983");

    mi_free(a);
    mi_free(b);
    mi_free(c);
    mi_free(d);
}

void test_divmod_square_gcdext_modinv(void)
{
    mint_t *n = mi_create_long(123);
    mint_t *d = mi_create_long(10);
    mint_t *q = mi_new();
    mint_t *r = mi_new();
    mint_t *sq = mi_create_long(-12);
    mint_t *a = mi_create_long(240);
    mint_t *b = mi_create_long(46);
    mint_t *g = mi_new();
    mint_t *x = mi_new();
    mint_t *y = mi_new();
    mint_t *lhs = mi_clone(a);
    mint_t *rhs = mi_clone(b);
    mint_t *inv = mi_create_long(3);
    mint_t *mod = mi_create_long(11);
    mint_t *check = mi_clone(inv);

    ASSERT_NOT_NULL(n);
    ASSERT_NOT_NULL(d);
    ASSERT_NOT_NULL(q);
    ASSERT_NOT_NULL(r);
    ASSERT_NOT_NULL(sq);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(g);
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(y);
    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(rhs);
    ASSERT_NOT_NULL(inv);
    ASSERT_NOT_NULL(mod);
    ASSERT_NOT_NULL(check);

    print_mint_input("n before divmod", n);
    print_mint_input("d before divmod", d);
    ASSERT_EQ_INT(mi_divmod(n, d, q, r), 0);
    print_mint_input("q after divmod", q);
    print_mint_input("r after divmod", r);
    assert_mint_string(q, "12");
    assert_mint_string(r, "3");

    print_mint_input("sq before square", sq);
    ASSERT_EQ_INT(mi_square(sq), 0);
    print_mint_input("sq after square", sq);
    assert_mint_string(sq, "144");

    print_mint_input("a before gcdext", a);
    print_mint_input("b before gcdext", b);
    ASSERT_EQ_INT(mi_gcdext(g, x, y, a, b), 0);
    print_mint_input("g after gcdext", g);
    print_mint_input("x after gcdext", x);
    print_mint_input("y after gcdext", y);
    assert_mint_string(g, "2");

    ASSERT_EQ_INT(mi_mul(lhs, x), 0);
    ASSERT_EQ_INT(mi_mul(rhs, y), 0);
    ASSERT_EQ_INT(mi_add(lhs, rhs), 0);
    print_mint_input("a*x + b*y", lhs);
    assert_mint_string(lhs, "2");

    print_mint_input("inv before modinv", inv);
    print_mint_input("mod before modinv", mod);
    ASSERT_EQ_INT(mi_modinv(inv, mod), 0);
    print_mint_input("inv after modinv", inv);
    assert_mint_string(inv, "4");

    ASSERT_EQ_INT(mi_mul(check, inv), 0);
    ASSERT_EQ_INT(mi_mod(check, mod), 0);
    print_mint_input("3*inv mod 11", check);
    assert_mint_string(check, "1");

    mi_free(n);
    mi_free(d);
    mi_free(q);
    mi_free(r);
    mi_free(sq);
    mi_free(a);
    mi_free(b);
    mi_free(g);
    mi_free(x);
    mi_free(y);
    mi_free(lhs);
    mi_free(rhs);
    mi_free(inv);
    mi_free(mod);
    mi_free(check);
}

void test_readme_mersenne_prime_search(void)
{
    static const unsigned expected_exponents[] = {
        2, 3, 5, 7,
        13, 17, 19, 31,
        61, 89, 107, 127,
        521, 607, 1279, 2203,
        2281, 3217, 4253, 4423
    };
    unsigned found = 0;
    unsigned p;

    for (p = 2; p <= 4423; ++p) {
        mint_t *exp = mi_create_long((long)p);
        mint_t *mersenne = NULL;
        mint_t *minus_one = mi_create_long(-1);

        ASSERT_NOT_NULL(exp);
        ASSERT_NOT_NULL(minus_one);

        if (mi_isprime(exp)) {
            mersenne = mi_create_2pow(p);
            ASSERT_NOT_NULL(mersenne);
            ASSERT_EQ_INT(mi_add(mersenne, minus_one), 0);

            if (mi_isprime(mersenne)) {
                ASSERT_TRUE(found < (sizeof(expected_exponents) / sizeof(expected_exponents[0])));
                ASSERT_EQ_INT((int)p, (int)expected_exponents[found]);
                if ((found % 4) == 3)
                    printf("M_%-4u is prime\n", p);
                else
                    printf("M_%-4u is prime    ", p);
                found++;
            }
        }

        mi_free(exp);
        mi_free(mersenne);
        mi_free(minus_one);
    }
    ASSERT_EQ_INT((int)found,
                  (int)(sizeof(expected_exponents) / sizeof(expected_exponents[0])));
}

int tests_main(void)
{
    RUN_TEST_CASE(test_create_and_to_string);
    RUN_TEST_CASE(test_setters_and_clear);
    RUN_TEST_CASE(test_ulong_and_hex_constructors);
    RUN_TEST_CASE(test_clone_copies_value);
    RUN_TEST_CASE(test_addition);
    RUN_TEST_CASE(test_multiplication);
    RUN_TEST_CASE(test_division_and_modulo);
    RUN_TEST_CASE(test_pow_and_pow10);
    RUN_TEST_CASE(test_factorial_fibonacci_and_binomial);
    RUN_TEST_CASE(test_compare_and_negate);
    RUN_TEST_CASE(test_inc_dec_and_bits);
    RUN_TEST_CASE(test_shifts);
    RUN_TEST_CASE(test_integer_sqrt);
    RUN_TEST_CASE(test_bitwise);
    RUN_TEST_CASE(test_sub_abs_predicates_conversions_gcd_lcm);
    RUN_TEST_CASE(test_long_hex_bits_and_nextprime);
    RUN_TEST_CASE(test_divmod_square_gcdext_modinv);
    RUN_TEST_CASE(test_powmod_isprime_and_factors);
    RUN_TEST_CASE(test_readme_mersenne_prime_search);
    return TESTS_EXIT_CODE();
}
