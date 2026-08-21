#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

void run_number_public_api_tests(void)
{
    printf(C_CYAN "Testing number_t as a standalone public API...\n" C_RESET);

    {
        number_t a = num_create_from_string("2");
        number_t b = num_create_from_string("3");
        number_t frac = num_create_from_string("5/6");
        number_t dec = num_create_from_string("1.25");
        number_t z = num_create_from_string("1 + 2i");
        number_t complex_lhs = num_create_from_string("2 + 3i");
        number_t complex_rhs = num_create_from_string("1 + 8i");
        number_t div_lhs = num_create_from_string("5 + i");
        number_t div_rhs = num_create_from_string("5 - i");
        number_t wide_complex_lhs = num_create_from_string("18446744073709551616 + i");
        number_t wide_complex_rhs = num_create_from_string("18446744073709551616 - i");
        number_t wide_complex_expected = num_create_from_string("340282366920938463463374607431768211457");

        number_t sum = num_add(a, b);
        number_t quot = num_div(a, b);
        number_t product = num_mul(sum, frac);
        number_t complex_product = num_mul(complex_lhs, complex_rhs);
        number_t complex_quot = num_div(div_lhs, div_rhs);
        number_t wide_complex_product = num_mul(wide_complex_lhs, wide_complex_rhs);
        number_t logged = num_log(dec);
        number_t ln_dec = num_ln(dec);
        number_t thousand = num_create_from_long(1000);
        number_t log10_thousand = num_log10(thousand);
        number_t lg_thousand = num_lg(thousand);
        number_t rooted = num_sqrt(z);
        number_t cube_radicand = num_create_from_long(-8);
        number_t cube_root = num_cubrt(cube_radicand);
        number_t cube_round_trip = num_pow_int(cube_root, 3);
        number_t cube_difference = num_sub(cube_round_trip, cube_radicand);
        number_t cube_error = num_abs(cube_difference);
        number_t fourth_radicand = num_create_from_long(81);
        number_t fourth_order = num_create_from_long(4);
        number_t fourth_root = num_root(fourth_radicand, fourth_order);
        number_t fourth_difference = num_sub(fourth_root, b);
        number_t fourth_error = num_abs(fourth_difference);
        number_t root_tolerance = num_create_from_string("1e-25");
        number_t floored = num_floor(dec);
        number_t ceiled = num_ceil(dec);
        number_t constant = NUM_PI;
        number_t i_constant = NUM_I;
        number_t cloned = num_clone(product);
        number_t pi_cloned = num_clone(constant);
        number_t i_cloned = num_clone(i_constant);

        assert_number_string("num_add(\"2\", \"3\")", sum, "5");
        assert_number_string("num_div(\"2\", \"3\")", quot, "⅔");
        assert_number_string("num_mul(\"5\", \"5/6\")", product, "²⁵⁄₆");
        assert_number_string("num_mul(\"2 + 3i\", \"1 + 8i\")", complex_product, "-22 + 19i");
        assert_number_string("num_div(\"5 + i\", \"5 - i\")", complex_quot, "¹²⁄₁₃ + ⁵⁄₁₃i");
        ASSERT_NUMBER_EQ(wide_complex_product, wide_complex_expected);
        ASSERT_TRUE(num_is_exact(wide_complex_product));
        assert_number_string("num_clone(\"25/6\")", cloned, "²⁵⁄₆");
        assert_number_string("num_log10(1000)", log10_thousand, "3");
        ASSERT_NUMBER_EQ(ln_dec, logged);
        ASSERT_NUMBER_EQ(lg_thousand, log10_thousand);
        assert_number_string("num_floor(1.25)", floored, "1");
        assert_number_string("num_ceil(1.25)", ceiled, "2");
        assert_number_string_prefix("NUM_PI", constant, "3.141592653589793238462643383279");
        assert_number_string_prefix("num_clone(NUM_PI)", pi_cloned, "3.141592653589793238462643383279");
        assert_number_string("num_clone(NUM_I)", i_cloned, "i");

        ASSERT_TRUE(num_is_real(a));
        ASSERT_TRUE(num_is_real(frac));
        ASSERT_TRUE(num_is_real(dec));
        ASSERT_TRUE(!num_is_real(z));
        ASSERT_TRUE(num_is_real(logged));
        ASSERT_TRUE(!num_is_real(rooted));
        ASSERT_TRUE(!num_is_real(cube_root));
        ASSERT_TRUE(num_lt(cube_error, root_tolerance));
        ASSERT_TRUE(num_lt(fourth_error, root_tolerance));
        ASSERT_TRUE(num_is_exact(a));
        ASSERT_TRUE(num_is_exact(frac));
        ASSERT_TRUE(!num_is_exact(dec));
        ASSERT_NUMBER_EQ(product, cloned);

        num_destroy(&a);
        num_destroy(&b);
        num_destroy(&frac);
        num_destroy(&dec);
        num_destroy(&z);
        num_destroy(&complex_lhs);
        num_destroy(&complex_rhs);
        num_destroy(&div_lhs);
        num_destroy(&div_rhs);
        num_destroy(&wide_complex_lhs);
        num_destroy(&wide_complex_rhs);
        num_destroy(&wide_complex_expected);
        num_destroy(&sum);
        num_destroy(&quot);
        num_destroy(&product);
        num_destroy(&complex_product);
        num_destroy(&complex_quot);
        num_destroy(&wide_complex_product);
        num_destroy(&logged);
        num_destroy(&ln_dec);
        num_destroy(&thousand);
        num_destroy(&log10_thousand);
        num_destroy(&lg_thousand);
        num_destroy(&rooted);
        num_destroy(&cube_radicand);
        num_destroy(&cube_root);
        num_destroy(&cube_round_trip);
        num_destroy(&cube_difference);
        num_destroy(&cube_error);
        num_destroy(&fourth_radicand);
        num_destroy(&fourth_order);
        num_destroy(&fourth_root);
        num_destroy(&fourth_difference);
        num_destroy(&fourth_error);
        num_destroy(&root_tolerance);
        num_destroy(&floored);
        num_destroy(&ceiled);
        num_destroy(&constant);
        num_destroy(&i_constant);
        num_destroy(&cloned);
        num_destroy(&pi_cloned);
        num_destroy(&i_cloned);
    }

    {
        num_scope_t *outer = NULL;
        num_scope_t *inner = NULL;
        number_t one_third = num_create_from_string("1/3");
        number_t one_sixth = num_create_from_string("1/6");
        number_t two = num_create_from_long(2);
        number_t five = num_create_from_long(5);
        number_t one = num_create_from_long(1);
        number_t sum;
        number_t detached;
        number_t outer_sum;
        number_t inner_sum;
        number_t inner_promoted;

        ASSERT_TRUE(!num_scope_is_active());

        outer = num_scope_enter();
        ASSERT_TRUE(num_scope_is_active());

        sum = num_add(one_third, one_sixth);
        detached = num_scope_detach(sum);
        assert_number_string("num_scope_detach(scoped 1/3 + 1/6)", detached, "½");

        num_scope_leave(&outer);
        ASSERT_TRUE(!num_scope_is_active());

        assert_number_string("detached scoped result survives leave", detached, "½");
        num_destroy(&detached);

        outer = num_scope_enter();
        outer_sum = num_add(two, five);
        inner = num_scope_enter();
        inner_sum = num_add(outer_sum, one);
        inner_promoted = num_scope_detach(inner_sum);
        num_scope_leave(&inner);

        assert_number_string("nested detached scoped result", inner_promoted, "8");

        num_destroy(&outer_sum);
        num_scope_leave(&outer);
        ASSERT_TRUE(!num_scope_is_active());

        num_destroy(&inner_promoted);
        num_destroy(&one);
        num_destroy(&five);
        num_destroy(&two);
        num_destroy(&one_third);
        num_destroy(&one_sixth);
    }

    {
        number_t a = num_create_from_string("2");
        number_t b = num_create_from_string("5/6");
        number_t c = num_add(a, b);
        string_t *text = num_to_string(c);

        ASSERT_NOT_NULL(text);
        printf(C_WHITE C_BOLD "README example" C_RESET "\n");
        printf("    2 + 5/6 = %s\n\n", text ? string_c_str(text) : "(null)");
        ASSERT_TRUE(text && strcmp(string_c_str(text), "¹⁷⁄₆") == 0);

        string_free(text);
        num_destroy(&a);
        num_destroy(&b);
        num_destroy(&c);
    }
}
