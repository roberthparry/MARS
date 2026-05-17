#include <stdio.h>

#include "test_number.h"

void run_number_promotion_tests(void)
{
    printf(C_CYAN "Testing cross-backend promotion and dispatch...\n" C_RESET);

    {
        number_t exact_two = num_create_from_string("2");
        number_t qf_half = num_create_from_qfloat(QF_HALF);
        number_t z = num_create_from_qcomplex(qc_from_string("1 + 2i"));
        number_t exact_three = num_create_from_string("3");
        number_t mixed_real = num_add(exact_two, qf_half);
        number_t mixed_complex = num_add(z, exact_three);
        number_t exact_log = num_log(exact_two);
        number_t exact_sqrt = num_sqrt(exact_two);
        number_t exact_exp = num_exp(exact_two);
        number_t exact_inv = num_inv(exact_two);
        number_t expected_half = num_create_from_string("1/2");
        number_t exact_two_double = num_create_from_double(2.0);
        number_t conj_z = num_create_from_qcomplex(qc_from_string("1 - 2i"));
        number_t product = num_mul(z, conj_z);
        number_t expected_five = num_create_from_string("5");

        ASSERT_TRUE(!num_is_exact(mixed_real));
        ASSERT_TRUE(num_is_real(mixed_real));
        ASSERT_TRUE(num_get_prec_bits(mixed_real) > 0u);

        ASSERT_TRUE(!num_is_real(mixed_complex));
        ASSERT_TRUE(num_get_prec_bits(mixed_complex) > 0u);

        ASSERT_TRUE(num_is_real(exact_log));
        ASSERT_TRUE(num_is_real(exact_sqrt));
        ASSERT_TRUE(num_is_real(exact_exp));
        ASSERT_NUMBER_EQ(exact_inv, expected_half);
        ASSERT_TRUE(num_is_real(product));
        ASSERT_NUMBER_EQ(product, expected_five);
        ASSERT_NUMBER_EQ(exact_two, exact_two_double);
        ASSERT_TRUE(num_lt(expected_half, exact_two));
        ASSERT_TRUE(num_gt(exact_two, expected_half));
        ASSERT_TRUE(!num_lt(z, exact_three));
        ASSERT_TRUE(!num_gt(z, exact_three));
        ASSERT_EQ_INT(num_cmp(expected_half, exact_two), -1);
        ASSERT_EQ_INT(num_cmp(z, exact_three), 0);

        num_destroy(&exact_two);
        num_destroy(&qf_half);
        num_destroy(&z);
        num_destroy(&exact_three);
        num_destroy(&mixed_real);
        num_destroy(&mixed_complex);
        num_destroy(&exact_log);
        num_destroy(&exact_sqrt);
        num_destroy(&exact_exp);
        num_destroy(&exact_inv);
        num_destroy(&expected_half);
        num_destroy(&exact_two_double);
        num_destroy(&conj_z);
        num_destroy(&product);
        num_destroy(&expected_five);
    }
}
