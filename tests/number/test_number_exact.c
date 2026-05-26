#include <stdio.h>

#include "test_number.h"

static void run_number_exact_integer_construction_tests(void)
{
    number_t zero = num_create_from_long(0);
    number_t neg = num_create_from_long(-42);
    number_t wide = num_create_from_string("18446744073709551616");
    number_t clone = num_clone(wide);
    number_t from_hex_like_decimal = num_create_from_string("511");
    number_t set_target = num_create_from_double(1.25);

    assert_number_string("num_create_from_long(0)", zero, "0");
    assert_number_string("num_create_from_long(-42)", neg, "-42");
    assert_number_string("wide integer from string", wide, "18446744073709551616");
    assert_number_string("num_clone(wide integer)", clone, "18446744073709551616");
    assert_number_string("decimal counterpart of 0x1ff", from_hex_like_decimal, "511");

    ASSERT_TRUE(num_is_exact(zero));
    ASSERT_TRUE(num_is_exact(neg));
    ASSERT_TRUE(num_is_exact(wide));
    ASSERT_TRUE(num_is_integer(wide));
    ASSERT_TRUE(num_eq(wide, clone));
    ASSERT_EQ_INT(num_get_sign(neg), -1);
    ASSERT_EQ_INT(num_get_sign(wide), 1);
    ASSERT_EQ_INT(num_get_exponent2(wide), 64);
    ASSERT_EQ_INT((int)num_get_prec_bits(wide), 0);

    ASSERT_EQ_INT(num_set_long(&set_target, 123456789L), 0);
    assert_number_string("num_set_long changes storage to exact integer",
                         set_target,
                         "123456789");
    ASSERT_TRUE(num_is_exact(set_target));
    ASSERT_EQ_INT((int)num_get_prec_bits(set_target), 0);

    num_destroy(&zero);
    num_destroy(&neg);
    num_destroy(&wide);
    num_destroy(&clone);
    num_destroy(&from_hex_like_decimal);
    num_destroy(&set_target);
}

static void run_number_exact_rational_construction_tests(void)
{
    number_t zero = num_create_from_frac(0, 7);
    number_t whole = num_create_from_frac(-84, 2);
    number_t frac = num_create_from_frac(6, -8);
    number_t parsed = num_create_from_string("  -10/20  ");
    number_t parsed_general = num_create_from_string("355/113");
    number_t parsed_unicode = num_create_from_string("³⁵⁵⁄₁₁₃");
    number_t set_target = num_create_from_double(2.5);
    number_t expected_neg_three_quarters = num_create_from_frac(-3, 4);
    number_t expected_two_thirds = num_create_from_frac(2, 3);

    assert_number_string("num_create_from_frac(0, 7)", zero, "0");
    assert_number_string("num_create_from_frac(-84, 2)", whole, "-42");
    assert_number_string("num_create_from_frac(6, -8)", frac, "-¾");
    assert_number_string("num_create_from_string(\"  -10/20  \")",
                         parsed,
                         "-½");
    assert_number_string("num_create_from_string(\"355/113\")",
                         parsed_general,
                         "³⁵⁵⁄₁₁₃");
    assert_number_string("num_create_from_string(\"³⁵⁵⁄₁₁₃\")",
                         parsed_unicode,
                         "³⁵⁵⁄₁₁₃");

    ASSERT_TRUE(num_is_exact(frac));
    ASSERT_TRUE(!num_is_integer(frac));
    ASSERT_TRUE(num_eq(parsed_general, parsed_unicode));
    ASSERT_TRUE(num_eq(frac, expected_neg_three_quarters));

    ASSERT_EQ_INT(num_set_frac(&set_target, 14, 21), 0);
    assert_number_string("num_set_frac changes storage to exact rational",
                         set_target,
                         "⅔");
    ASSERT_TRUE(num_is_exact(set_target));
    ASSERT_EQ_INT((int)num_get_prec_bits(set_target), 0);
    ASSERT_TRUE(num_eq(set_target, expected_two_thirds));

    num_destroy(&zero);
    num_destroy(&whole);
    num_destroy(&frac);
    num_destroy(&parsed);
    num_destroy(&parsed_general);
    num_destroy(&parsed_unicode);
    num_destroy(&set_target);
    num_destroy(&expected_neg_three_quarters);
    num_destroy(&expected_two_thirds);
}

static void run_number_exact_arithmetic_tests(void)
{
    number_t half = num_create_from_frac(1, 2);
    number_t third = num_create_from_frac(1, 3);
    number_t zero = num_create_from_long(0);
    number_t sum = num_add(half, third);
    number_t diff = num_sub(sum, third);
    number_t prod = num_mul(diff, third);
    number_t quot = num_div(prod, third);
    number_t neg = num_neg(quot);
    number_t abs_value = num_abs(neg);
    number_t inv = num_inv(abs_value);
    number_t inv_zero = num_inv(zero);
    number_t big = num_create_from_string("18446744073709551616/3");
    number_t five_sevenths = num_create_from_frac(5, 7);
    number_t big_prod = num_mul(big, five_sevenths);
    number_t big_roundtrip = num_div(big_prod, five_sevenths);

    assert_number_string("num_add(1/2, 1/3)", sum, "⅚");
    assert_number_string("num_sub(5/6, 1/3)", diff, "½");
    assert_number_string("num_mul(1/2, 1/3)", prod, "⅙");
    assert_number_string("num_div(1/6, 1/3)", quot, "½");
    assert_number_string("num_neg(1/2)", neg, "-½");
    assert_number_string("num_abs(-1/2)", abs_value, "½");
    assert_number_string("num_inv(1/2)", inv, "2");
    ASSERT_TRUE(num_is_nan(inv_zero));
    assert_number_string("large rational product",
                         big_prod,
                         "⁹²²³³⁷²⁰³⁶⁸⁵⁴⁷⁷⁵⁸⁰⁸⁰⁄₂₁");
    assert_number_string("large rational division roundtrip",
                         big_roundtrip,
                         "¹⁸⁴⁴⁶⁷⁴⁴⁰⁷³⁷⁰⁹⁵⁵¹⁶¹⁶⁄₃");

    ASSERT_TRUE(num_lt(third, half));
    ASSERT_TRUE(num_le(third, half));
    ASSERT_TRUE(num_gt(half, third));
    ASSERT_TRUE(num_ge(half, third));
    ASSERT_EQ_INT(num_cmp(half, quot), 0);

    num_destroy(&half);
    num_destroy(&third);
    num_destroy(&zero);
    num_destroy(&sum);
    num_destroy(&diff);
    num_destroy(&prod);
    num_destroy(&quot);
    num_destroy(&neg);
    num_destroy(&abs_value);
    num_destroy(&inv);
    num_destroy(&inv_zero);
    num_destroy(&big);
    num_destroy(&five_sevenths);
    num_destroy(&big_prod);
    num_destroy(&big_roundtrip);
}

static void run_number_exact_integer_math_tests(void)
{
    number_t two = num_create_from_long(2);
    number_t three = num_create_from_long(3);
    number_t ten = num_create_from_long(10);
    number_t wide_square = num_create_from_string("18446744073709551616");
    number_t fifty = num_create_from_long(50);
    number_t sqrt_fifty;
    number_t five = num_create_from_long(5);
    number_t fifty_two = num_create_from_long(52);
    number_t pow = num_pow_int(two, 65);
    number_t pow10 = num_pow10(3);
    number_t shifted = num_ldexp(three, 10);
    number_t sqrt_perfect = num_sqrt(wide_square);
    number_t sqrt_floor;
    number_t neg_one = num_create_from_long(-1);
    number_t neg_four = num_create_from_long(-4);
    number_t sqrt_neg_one = num_sqrt(neg_one);
    number_t sqrt_neg_four = num_sqrt(neg_four);
    number_t nine_fourths = num_create_from_frac(9, 4);
    number_t neg_nine_fourths = num_create_from_frac(-9, 4);
    number_t sqrt_neg_nine_fourths = num_sqrt(neg_nine_fourths);
    number_t ceil_rat = num_ceil(nine_fourths);
    number_t floor_neg_rat = num_floor(neg_nine_fourths);
    number_t bin_small = num_binomial(five, two);
    number_t bin_large = num_binomial(fifty_two, five);

    sqrt_fifty = num_sqrt(fifty);
    sqrt_floor = num_floor(sqrt_fifty);

    assert_number_string("num_pow_int(2, 65)",
                         pow,
                         "36893488147419103232");
    assert_number_string("num_pow10(3)", pow10, "1000");
    assert_number_string("num_ldexp(3, 10)", shifted, "3072");
    assert_number_string("num_sqrt(2^64)", sqrt_perfect, "4294967296");
    assert_number_string("floor(sqrt(50))", sqrt_floor, "7");
    assert_number_string("num_sqrt(-1)", sqrt_neg_one, "i");
    assert_number_string("num_sqrt(-4)", sqrt_neg_four, "2i");
    assert_number_string("num_sqrt(-9/4)", sqrt_neg_nine_fourths, "1.5i");
    assert_number_string("ceil(9/4)", ceil_rat, "3");
    assert_number_string("floor(-9/4)", floor_neg_rat, "-3");
    assert_number_string("num_binomial(5, 2)", bin_small, "10");
    assert_number_string("num_binomial(52, 5)", bin_large, "2598960");

    ASSERT_EQ_INT(num_get_exponent2(pow), 65);

    num_destroy(&two);
    num_destroy(&three);
    num_destroy(&ten);
    num_destroy(&wide_square);
    num_destroy(&fifty);
    num_destroy(&sqrt_fifty);
    num_destroy(&five);
    num_destroy(&fifty_two);
    num_destroy(&pow);
    num_destroy(&pow10);
    num_destroy(&shifted);
    num_destroy(&sqrt_perfect);
    num_destroy(&sqrt_floor);
    num_destroy(&neg_one);
    num_destroy(&neg_four);
    num_destroy(&sqrt_neg_one);
    num_destroy(&sqrt_neg_four);
    num_destroy(&nine_fourths);
    num_destroy(&neg_nine_fourths);
    num_destroy(&sqrt_neg_nine_fourths);
    num_destroy(&ceil_rat);
    num_destroy(&floor_neg_rat);
    num_destroy(&bin_small);
    num_destroy(&bin_large);
}

static void assert_number_factor(const number_factor_t *factor,
                                 const char *expected_prime,
                                 unsigned long expected_exponent)
{
    ASSERT_NOT_NULL(factor);
    assert_number_string("factor prime", factor->prime, expected_prime);
    ASSERT_EQ_LONG((long)factor->exponent, (long)expected_exponent);
}

static void run_number_exact_number_theory_tests(void)
{
    NUM_SCOPE(scope);
    number_t factorial0 = num_factorial(0u);
    number_t factorial10 = num_factorial(10u);
    number_t fib0 = num_fibonacci(0u);
    number_t fib1 = num_fibonacci(1u);
    number_t fib50 = num_fibonacci(50u);
    number_t partition0 = num_partition(num_create_from_long(0));
    number_t partition5 = num_partition(num_create_from_long(5));
    number_t partition50 = num_partition(num_create_from_long(50));
    number_t partition_negative = num_partition(num_create_from_long(-3));
    number_t partition_fraction = num_partition(num_create_from_frac(3, 2));
    number_t base = num_create_from_long(4);
    number_t exp = num_create_from_long(13);
    number_t mod = num_create_from_long(497);
    number_t powmod = num_powmod(base, exp, mod);
    number_t a = num_create_from_long(84);
    number_t b = num_create_from_long(30);
    number_t gcd = num_gcd(a, b);
    number_t c = num_create_from_long(-21);
    number_t d = num_create_from_long(6);
    number_t lcm = num_lcm(c, d);
    number_t inv = num_modinv(num_create_from_long(3), num_create_from_long(11));
    number_t check = num_mod(num_mul(num_create_from_long(3), inv),
                             num_create_from_long(11));
    number_t div_q = num_new();
    number_t div_r = num_new();
    number_t ext_g = num_new();
    number_t ext_x = num_new();
    number_t ext_y = num_new();
    number_t ext_a = num_create_from_long(240);
    number_t ext_b = num_create_from_long(46);
    number_t ext_lhs;
    number_t ext_rhs;
    number_t ext_sum;
    number_t next_from_14 = num_next_prime(num_create_from_long(14));
    number_t next_from_neg = num_next_prime(num_create_from_long(-10));
    number_t prev_from_14 = num_prev_prime(num_create_from_long(14));
    number_t prev_from_4 = num_prev_prime(num_create_from_long(4));
    number_t factors_input = num_create_from_long(360);
    number_t big_factors_input = num_create_from_string("1000036000099");
    number_factors_t *factors;
    number_factors_t *big_factors;

    assert_number_string("num_factorial(0)", factorial0, "1");
    assert_number_string("num_factorial(10)", factorial10, "3628800");
    assert_number_string("num_fibonacci(0)", fib0, "0");
    assert_number_string("num_fibonacci(1)", fib1, "1");
    assert_number_string("num_fibonacci(50)", fib50, "12586269025");
    assert_number_string("num_partition(0)", partition0, "1");
    assert_number_string("num_partition(5)", partition5, "7");
    assert_number_string("num_partition(50)", partition50, "204226");
    assert_number_string("num_partition(-3)", partition_negative, "0");
    ASSERT_TRUE(num_is_nan(partition_fraction));
    assert_number_string("num_powmod(4, 13, 497)", powmod, "445");
    assert_number_string("num_gcd(84, 30)", gcd, "6");
    assert_number_string("num_lcm(-21, 6)", lcm, "42");
    assert_number_string("num_modinv(3, 11)", inv, "4");
    assert_number_string("3 * inv mod 11", check, "1");

    ASSERT_EQ_INT(num_divmod(num_create_from_long(123),
                             num_create_from_long(10),
                             &div_q,
                             &div_r), 0);
    assert_number_string("num_divmod(123, 10).quotient", div_q, "12");
    assert_number_string("num_divmod(123, 10).remainder", div_r, "3");

    ASSERT_EQ_INT(num_gcdext(ext_a, ext_b, &ext_g, &ext_x, &ext_y), 0);
    assert_number_string("num_gcdext(240, 46).gcd", ext_g, "2");
    ext_lhs = num_mul(ext_a, ext_x);
    ext_rhs = num_mul(ext_b, ext_y);
    ext_sum = num_add(ext_lhs, ext_rhs);
    assert_number_string("gcdext Bezout identity", ext_sum, "2");

    ASSERT_TRUE(num_is_prime(num_create_from_long(97)));
    ASSERT_TRUE(!num_is_prime(num_create_from_long(221)));
    ASSERT_TRUE(num_is_prime(num_create_from_long(999983)));
    ASSERT_TRUE(!num_is_prime(num_create_from_long(999985)));
    ASSERT_EQ_INT(num_prove_prime(num_create_from_long(97)),
                  NUMBER_PRIMALITY_PRIME);
    ASSERT_EQ_INT(num_prove_prime(num_create_from_long(221)),
                  NUMBER_PRIMALITY_COMPOSITE);
    assert_number_string("num_next_prime(14)", next_from_14, "17");
    assert_number_string("num_next_prime(-10)", next_from_neg, "2");
    assert_number_string("num_prev_prime(14)", prev_from_14, "13");
    assert_number_string("num_prev_prime(4)", prev_from_4, "3");

    factors = num_factors(factors_input);
    ASSERT_NOT_NULL(factors);
    ASSERT_EQ_LONG((long)factors->count, 3);
    assert_number_factor(&factors->items[0], "2", 3);
    assert_number_factor(&factors->items[1], "3", 2);
    assert_number_factor(&factors->items[2], "5", 1);

    big_factors = num_factors(big_factors_input);
    ASSERT_NOT_NULL(big_factors);
    ASSERT_EQ_LONG((long)big_factors->count, 2);
    assert_number_factor(&big_factors->items[0], "1000003", 1);
    assert_number_factor(&big_factors->items[1], "1000033", 1);

    num_factors_free(big_factors);
    num_factors_free(factors);
    num_destroy(&big_factors_input);
    num_destroy(&factors_input);
    num_destroy(&prev_from_4);
    num_destroy(&prev_from_14);
    num_destroy(&next_from_neg);
    num_destroy(&next_from_14);
    num_destroy(&ext_sum);
    num_destroy(&ext_rhs);
    num_destroy(&ext_lhs);
    num_destroy(&ext_b);
    num_destroy(&ext_a);
    num_destroy(&ext_y);
    num_destroy(&ext_x);
    num_destroy(&ext_g);
    num_destroy(&div_r);
    num_destroy(&div_q);
    num_destroy(&check);
    num_destroy(&inv);
    num_destroy(&lcm);
    num_destroy(&d);
    num_destroy(&c);
    num_destroy(&gcd);
    num_destroy(&b);
    num_destroy(&a);
    num_destroy(&powmod);
    num_destroy(&mod);
    num_destroy(&exp);
    num_destroy(&base);
    num_destroy(&fib50);
    num_destroy(&fib1);
    num_destroy(&fib0);
    num_destroy(&factorial10);
    num_destroy(&factorial0);
}

static void run_number_exact_bit_tests(void)
{
    NUM_SCOPE(scope);
    number_t thirteen = num_create_from_long(13);
    number_t eleven = num_create_from_long(11);
    number_t eight = num_create_from_long(8);
    number_t and_value = num_bit_and(thirteen, eleven);
    number_t or_value = num_bit_or(thirteen, eleven);
    number_t xor_value = num_bit_xor(thirteen, eight);
    number_t not_value = num_bit_not(eight);
    number_t bits = num_create_from_long(0);
    number_t bit4 = num_set_bit(bits, 4u);
    number_t bit4_and_0 = num_set_bit(bit4, 0u);
    number_t cleared = num_clear_bit(bit4_and_0, 4u);
    number_t shifted_left = num_shl(num_create_from_long(3), 65);
    number_t shifted_right = num_shr(num_create_from_string("18446744073709551616"),
                                     64);
    number_t shifted_neg = num_shr(num_create_from_long(-9), 1);
    number_t sqrt_144 = num_isqrt(num_create_from_long(144));
    number_t sqrt_200 = num_isqrt(num_create_from_long(200));

    assert_number_string("num_bit_and(13, 11)", and_value, "9");
    assert_number_string("num_bit_or(13, 11)", or_value, "15");
    assert_number_string("num_bit_xor(13, 8)", xor_value, "5");
    assert_number_string("num_bit_not(8)", not_value, "7");
    assert_number_string("num_set_bit(0, 4)", bit4, "16");
    assert_number_string("num_set_bit(16, 0)", bit4_and_0, "17");
    assert_number_string("num_clear_bit(17, 4)", cleared, "1");
    assert_number_string("num_shl(3, 65)", shifted_left, "110680464442257309696");
    assert_number_string("num_shr(2^64, 64)", shifted_right, "1");
    assert_number_string("num_shr(-9, 1)", shifted_neg, "-4");
    assert_number_string("num_isqrt(144)", sqrt_144, "12");
    assert_number_string("num_isqrt(200)", sqrt_200, "14");

    ASSERT_EQ_LONG((long)num_bit_length(num_create_from_long(0)), 0);
    ASSERT_EQ_LONG((long)num_bit_length(num_create_from_string("18446744073709551616")),
                   65);
    ASSERT_TRUE(num_test_bit(num_create_from_string("18446744073709551616"), 64u));
    ASSERT_TRUE(!num_test_bit(num_create_from_string("18446744073709551616"), 63u));

    num_destroy(&sqrt_200);
    num_destroy(&sqrt_144);
    num_destroy(&shifted_neg);
    num_destroy(&shifted_right);
    num_destroy(&shifted_left);
    num_destroy(&cleared);
    num_destroy(&bit4_and_0);
    num_destroy(&bit4);
    num_destroy(&bits);
    num_destroy(&not_value);
    num_destroy(&xor_value);
    num_destroy(&or_value);
    num_destroy(&and_value);
    num_destroy(&eight);
    num_destroy(&eleven);
    num_destroy(&thirteen);
}

void run_number_exact_backend_tests(void)
{
    printf(C_CYAN "Testing exact integer and rational backends...\n" C_RESET);

    run_number_exact_integer_construction_tests();
    run_number_exact_rational_construction_tests();
    run_number_exact_arithmetic_tests();
    run_number_exact_integer_math_tests();
    run_number_exact_number_theory_tests();
    run_number_exact_bit_tests();

    {
        number_t two_from_long;
        number_t three_from_long;
        number_t half_from_frac;
        number_t third_from_frac;
        number_t sum;
        number_t diff;
        number_t prod;
        number_t quot;
        number_t frac_sum;
        number_t inv_two;
        number_t zero_const;
        number_t one_const;
        number_t half_const;
        number_t ten_const;

        two_from_long = num_create_from_long(2);
        three_from_long = num_create_from_long(3);
        half_from_frac = num_create_from_frac(1, 2);
        third_from_frac = num_create_from_frac(1, 3);

        sum = num_add(two_from_long, three_from_long);
        diff = num_sub(three_from_long, two_from_long);
        prod = num_mul(two_from_long, three_from_long);
        quot = num_div(two_from_long, three_from_long);
        frac_sum = num_add(half_from_frac, third_from_frac);
        inv_two = num_inv(two_from_long);
        zero_const = NUM_ZERO;
        one_const = NUM_ONE;
        half_const = NUM_HALF;
        ten_const = NUM_TEN;

        assert_number_string("NUM_ZERO", zero_const, "0");
        assert_number_string("NUM_ONE", one_const, "1");
        assert_number_string("num_add(integer 2, integer 3)", sum, "5");
        assert_number_string("num_sub(integer 3, integer 2)", diff, "1");
        assert_number_string("num_mul(integer 2, integer 3)", prod, "6");
        assert_number_string("num_div(integer 2, integer 3)", quot, "⅔");
        assert_number_string("num_add(1/2, 1/3)", frac_sum, "⅚");
        assert_number_string("num_inv(integer 2)", inv_two, "½");
        assert_number_string("NUM_HALF", half_const, "½");
        assert_number_string("NUM_TEN", ten_const, "10");

        ASSERT_TRUE(num_is_exact(zero_const));
        ASSERT_TRUE(num_is_exact(one_const));
        ASSERT_TRUE(num_is_exact(two_from_long));
        ASSERT_TRUE(num_is_exact(half_from_frac));
        ASSERT_TRUE(num_is_exact(half_const));
        ASSERT_TRUE(!num_is_integer(half_from_frac));
        ASSERT_TRUE(num_is_real(two_from_long));
        ASSERT_TRUE(num_is_real(half_from_frac));
        ASSERT_EQ_INT(num_get_sign(two_from_long), 1);
        ASSERT_EQ_INT(num_get_sign(half_from_frac), 1);
        ASSERT_EQ_INT(num_get_exponent2(two_from_long), 1);
        ASSERT_EQ_INT(num_get_exponent2(half_from_frac), -1);
        ASSERT_EQ_INT((int)num_get_prec_bits(two_from_long), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(half_from_frac), 0);
        ASSERT_EQ_INT(num_set_prec_bits(&two_from_long, 512u), 0);
        ASSERT_EQ_INT(num_set_prec_bits(&half_from_frac, 512u), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(two_from_long), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(half_from_frac), 0);

        num_destroy(&two_from_long);
        num_destroy(&three_from_long);
        num_destroy(&half_from_frac);
        num_destroy(&third_from_frac);
        num_destroy(&sum);
        num_destroy(&diff);
        num_destroy(&prod);
        num_destroy(&quot);
        num_destroy(&frac_sum);
        num_destroy(&inv_two);
        num_destroy(&zero_const);
        num_destroy(&one_const);
        num_destroy(&half_const);
        num_destroy(&ten_const);
    }
}
