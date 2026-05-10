#include <stdio.h>

#include "test_number.h"

void run_number_constant_tests(void)
{
    printf(C_CYAN "Testing named constants and decimal powers...\n" C_RESET);

    {
        number_t pi = NUM_PI;
        number_t e = NUM_E;
        number_t gamma = NUM_EULER_MASCHERONI;
        number_t i = NUM_I;
        number_t sqrt3 = NUM_SQRT3;
        number_t million = num_pow10(6);
        number_t sin_pi_6 = num_sin(NUM_PI_6);
        number_t sin_pi_4 = num_sin(NUM_PI_4);
        number_t sin_pi_3 = num_sin(NUM_PI_3);
        number_t sin_pi_2 = num_sin(NUM_PI_2);
        number_t cos_pi_6 = num_cos(NUM_PI_6);
        number_t cos_pi_4 = num_cos(NUM_PI_4);
        number_t cos_pi_3 = num_cos(NUM_PI_3);
        number_t cos_pi_2 = num_cos(NUM_PI_2);
        number_t cos_3pi_4 = num_cos(NUM_3PI_4);
        number_t tan_pi_6 = num_tan(NUM_PI_6);
        number_t tan_pi_4 = num_tan(NUM_PI_4);
        number_t tan_pi_3 = num_tan(NUM_PI_3);
        number_t tan_3pi_4 = num_tan(NUM_3PI_4);
        number_t i_pi_6 = num_mul(NUM_I, NUM_PI_6);
        number_t i_pi_3 = num_mul(NUM_I, NUM_PI_3);
        number_t i_pi_4 = num_mul(NUM_I, NUM_PI_4);
        number_t i_3pi_4 = num_mul(NUM_I, NUM_3PI_4);
        number_t sinh_i_pi_6 = num_sinh(i_pi_6);
        number_t cosh_i_pi_3 = num_cosh(i_pi_3);
        number_t tanh_i_pi_4 = num_tanh(i_pi_4);
        number_t tanh_i_3pi_4 = num_tanh(i_3pi_4);
        number_t half_sqrt2 = NUM_SQRT2_OVER_TWO;
        number_t half_sqrt3 = NUM_SQRT3_OVER_TWO;
        number_t neg_half_sqrt2 = num_neg(NUM_SQRT2_OVER_TWO);
        number_t inv_sqrt3 = num_div(NUM_ONE, NUM_SQRT3);
        number_t i_half = num_mul(NUM_I, NUM_HALF);
        number_t i_value = NUM_I;
        number_t neg_i = num_neg(NUM_I);

        assert_number_string_prefix("NUM_PI", pi, "3.14159");
        assert_number_string_prefix("NUM_E", e, "2.71828");
        assert_number_string_prefix("NUM_EULER_MASCHERONI", gamma, "0.57721");
        assert_number_string_prefix("NUM_SQRT3", sqrt3, "1.73205");
        assert_number_string_prefix("NUM_SQRT2_OVER_TWO", half_sqrt2, "0.70710");
        assert_number_string_prefix("NUM_SQRT3_OVER_TWO", half_sqrt3, "0.86602");
        assert_number_string("NUM_I", i, "0 + 1i");
        assert_number_string("num_pow10(6)", million, "1000000");

        ASSERT_TRUE(num_is_real(pi));
        ASSERT_TRUE(num_is_real(e));
        ASSERT_TRUE(num_is_real(gamma));
        ASSERT_TRUE(num_is_real(sqrt3));
        ASSERT_TRUE(!num_is_real(i));
        ASSERT_TRUE(!num_is_exact(pi));
        ASSERT_TRUE(!num_is_exact(e));
        ASSERT_TRUE(!num_is_exact(gamma));
        ASSERT_TRUE(num_get_precision(pi) > 0u);
        ASSERT_TRUE(num_eq(sin_pi_6, NUM_HALF));
        ASSERT_TRUE(num_eq(sin_pi_4, half_sqrt2));
        ASSERT_TRUE(num_eq(sin_pi_3, half_sqrt3));
        ASSERT_TRUE(num_eq(sin_pi_2, NUM_ONE));
        ASSERT_TRUE(num_eq(cos_pi_6, half_sqrt3));
        ASSERT_TRUE(num_eq(cos_pi_4, half_sqrt2));
        ASSERT_TRUE(num_eq(cos_pi_3, NUM_HALF));
        ASSERT_TRUE(num_eq(cos_pi_2, NUM_ZERO));
        ASSERT_TRUE(num_eq(cos_3pi_4, neg_half_sqrt2));
        ASSERT_TRUE(num_eq(tan_pi_6, inv_sqrt3));
        ASSERT_TRUE(num_eq(tan_pi_4, NUM_ONE));
        ASSERT_TRUE(num_eq(tan_pi_3, NUM_SQRT3));
        ASSERT_TRUE(num_eq(tan_3pi_4, NUM_NEG_ONE));
        ASSERT_TRUE(num_eq(sinh_i_pi_6, i_half));
        ASSERT_TRUE(num_eq(cosh_i_pi_3, NUM_HALF));
        ASSERT_TRUE(num_eq(tanh_i_pi_4, i_value));
        ASSERT_TRUE(num_eq(tanh_i_3pi_4, neg_i));

        num_clear(&pi);
        num_clear(&e);
        num_clear(&gamma);
        num_clear(&sqrt3);
        num_clear(&i);
        num_clear(&million);
        num_clear(&sin_pi_6);
        num_clear(&sin_pi_4);
        num_clear(&sin_pi_3);
        num_clear(&sin_pi_2);
        num_clear(&cos_pi_6);
        num_clear(&cos_pi_4);
        num_clear(&cos_pi_3);
        num_clear(&cos_pi_2);
        num_clear(&cos_3pi_4);
        num_clear(&tan_pi_6);
        num_clear(&tan_pi_4);
        num_clear(&tan_pi_3);
        num_clear(&tan_3pi_4);
        num_clear(&i_pi_6);
        num_clear(&i_pi_3);
        num_clear(&i_pi_4);
        num_clear(&i_3pi_4);
        num_clear(&sinh_i_pi_6);
        num_clear(&cosh_i_pi_3);
        num_clear(&tanh_i_pi_4);
        num_clear(&tanh_i_3pi_4);
        num_clear(&half_sqrt2);
        num_clear(&half_sqrt3);
        num_clear(&neg_half_sqrt2);
        num_clear(&inv_sqrt3);
        num_clear(&i_half);
        num_clear(&i_value);
        num_clear(&neg_i);
    }
}
