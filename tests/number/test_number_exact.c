#include <stdio.h>

#include "test_number.h"

void run_number_exact_backend_tests(void)
{
    printf(C_CYAN "Testing exact integer and rational backends...\n" C_RESET);

    {
        mint_t *mi_two = mi_create_long(2);
        mint_t *mi_three = mi_create_long(3);
        mrational_t *mr_half = mr_create_frac_long(1, 2);
        mrational_t *mr_third = mr_create_frac_long(1, 3);

        number_t two_from_mint;
        number_t three_from_mint;
        number_t half_from_mr;
        number_t third_from_mr;
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

        ASSERT_NOT_NULL(mi_two);
        ASSERT_NOT_NULL(mi_three);
        ASSERT_NOT_NULL(mr_half);
        ASSERT_NOT_NULL(mr_third);

        two_from_mint = num_create_mint(mi_two);
        three_from_mint = num_create_mint(mi_three);
        half_from_mr = num_create_mrational(mr_half);
        third_from_mr = num_create_mrational(mr_third);

        sum = num_add(two_from_mint, three_from_mint);
        diff = num_sub(three_from_mint, two_from_mint);
        prod = num_mul(two_from_mint, three_from_mint);
        quot = num_div(two_from_mint, three_from_mint);
        frac_sum = num_add(half_from_mr, third_from_mr);
        inv_two = num_inv(two_from_mint);
        zero_const = NUM_ZERO;
        one_const = NUM_ONE;
        half_const = NUM_HALF;
        ten_const = NUM_TEN;

        assert_number_string("NUM_ZERO", zero_const, "0");
        assert_number_string("NUM_ONE", one_const, "1");
        assert_number_string("num_add(mint 2, mint 3)", sum, "5");
        assert_number_string("num_sub(mint 3, mint 2)", diff, "1");
        assert_number_string("num_mul(mint 2, mint 3)", prod, "6");
        assert_number_string("num_div(mint 2, mint 3)", quot, "2/3");
        assert_number_string("num_add(1/2, 1/3)", frac_sum, "5/6");
        assert_number_string("num_inv(mint 2)", inv_two, "1/2");
        assert_number_string("NUM_HALF", half_const, "1/2");
        assert_number_string("NUM_TEN", ten_const, "10");

        ASSERT_TRUE(num_is_exact(zero_const));
        ASSERT_TRUE(num_is_exact(one_const));
        ASSERT_TRUE(num_is_exact(two_from_mint));
        ASSERT_TRUE(num_is_exact(half_from_mr));
        ASSERT_TRUE(num_is_exact(half_const));
        ASSERT_TRUE(!num_is_integer(half_from_mr));
        ASSERT_TRUE(num_is_real(two_from_mint));
        ASSERT_TRUE(num_is_real(half_from_mr));
        ASSERT_EQ_INT(num_get_sign(two_from_mint), 1);
        ASSERT_EQ_INT(num_get_sign(half_from_mr), 1);
        ASSERT_EQ_INT(num_get_exponent2(two_from_mint), 1);
        ASSERT_EQ_INT(num_get_exponent2(half_from_mr), -1);
        ASSERT_EQ_INT((int)num_get_precision(two_from_mint), 0);
        ASSERT_EQ_INT((int)num_get_precision(half_from_mr), 0);
        ASSERT_EQ_INT(num_set_precision(&two_from_mint, 512u), 0);
        ASSERT_EQ_INT(num_set_precision(&half_from_mr, 512u), 0);
        ASSERT_EQ_INT((int)num_get_precision(two_from_mint), 0);
        ASSERT_EQ_INT((int)num_get_precision(half_from_mr), 0);

        num_clear(&two_from_mint);
        num_clear(&three_from_mint);
        num_clear(&half_from_mr);
        num_clear(&third_from_mr);
        num_clear(&sum);
        num_clear(&diff);
        num_clear(&prod);
        num_clear(&quot);
        num_clear(&frac_sum);
        num_clear(&inv_two);
        num_clear(&zero_const);
        num_clear(&one_const);
        num_clear(&half_const);
        num_clear(&ten_const);

        mi_free(mi_two);
        mi_free(mi_three);
        mr_free(mr_half);
        mr_free(mr_third);
    }
}
