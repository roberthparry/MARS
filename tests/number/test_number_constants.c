#include <stdio.h>

#include "test_number.h"

void run_number_constant_tests(void)
{
    printf(C_CYAN "Testing named constants and decimal powers...\n" C_RESET);

    {
        number_t pi = NUM_PI;
        number_t e = NUM_E;
        number_t gamma = NUM_EULER_MASCHERONI;
        number_t phi = NUM_PHI;
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
        number_t sin_pi_6_pair = NUM_ZERO;
        number_t cos_pi_6_pair = NUM_ZERO;
        number_t i_pi_6 = num_mul(NUM_I, NUM_PI_6);
        number_t i_pi_3 = num_mul(NUM_I, NUM_PI_3);
        number_t i_pi_4 = num_mul(NUM_I, NUM_PI_4);
        number_t i_3pi_4 = num_mul(NUM_I, NUM_3PI_4);
        number_t sinh_i_pi_6 = num_sinh(i_pi_6);
        number_t cosh_i_pi_3 = num_cosh(i_pi_3);
        number_t tanh_i_pi_4 = num_tanh(i_pi_4);
        number_t tanh_i_3pi_4 = num_tanh(i_3pi_4);
        number_t sinh_i_pi_3_pair = NUM_ZERO;
        number_t cosh_i_pi_3_pair = NUM_ZERO;
        number_t i_half_sqrt3 = num_mul(NUM_I, NUM_SQRT3_OVER_TWO);
        number_t half_sqrt2 = NUM_SQRT2_OVER_TWO;
        number_t half_sqrt3 = NUM_SQRT3_OVER_TWO;
        number_t neg_half_sqrt2 = num_neg(NUM_SQRT2_OVER_TWO);
        number_t inv_sqrt3 = num_div(NUM_ONE, NUM_SQRT3);
        number_t i_half = num_mul(NUM_I, NUM_HALF);
        number_t i_value = NUM_I;
        number_t neg_i = num_neg(NUM_I);

        assert_number_string_prefix("NUM_PI", pi,
                                    "3.141592653589793238462643383279");
        assert_number_string_prefix("NUM_E", e,
                                    "2.718281828459045235360287471352");
        assert_number_string_prefix("NUM_EULER_MASCHERONI", gamma,
                                    "0.577215664901532860606512090082");
        assert_number_string_prefix("NUM_PHI", phi,
                                    "1.618033988749894848204586834365");
        assert_number_string_prefix("NUM_SQRT3", sqrt3,
                                    "1.732050807568877293527446341505");
        assert_number_string_prefix("NUM_SQRT2_OVER_TWO", half_sqrt2,
                                    "0.707106781186547524400844362104");
        assert_number_string_prefix("NUM_SQRT3_OVER_TWO", half_sqrt3,
                                    "0.866025403784438646763723170752");
        assert_number_string("NUM_I", i, "i");
        assert_number_string("num_pow10(6)", million, "1000000");

        ASSERT_TRUE(num_is_real(pi));
        ASSERT_TRUE(num_is_real(e));
        ASSERT_TRUE(num_is_real(gamma));
        ASSERT_TRUE(num_is_real(phi));
        ASSERT_TRUE(num_is_real(sqrt3));
        ASSERT_TRUE(!num_is_real(i));
        ASSERT_TRUE(!num_is_exact(pi));
        ASSERT_TRUE(!num_is_exact(e));
        ASSERT_TRUE(!num_is_exact(gamma));
        ASSERT_TRUE(!num_is_exact(phi));
        ASSERT_TRUE(num_get_prec_bits(pi) > 0u);
        ASSERT_EQ_INT((int)num_get_prec_bits(phi), 1088);
        ASSERT_NUMBER_EQ(sin_pi_6, NUM_HALF);
        ASSERT_NUMBER_EQ(sin_pi_4, half_sqrt2);
        ASSERT_NUMBER_EQ(sin_pi_3, half_sqrt3);
        ASSERT_NUMBER_EQ(sin_pi_2, NUM_ONE);
        ASSERT_NUMBER_EQ(cos_pi_6, half_sqrt3);
        ASSERT_NUMBER_EQ(cos_pi_4, half_sqrt2);
        ASSERT_NUMBER_EQ(cos_pi_3, NUM_HALF);
        ASSERT_NUMBER_EQ(cos_pi_2, NUM_ZERO);
        ASSERT_NUMBER_EQ(cos_3pi_4, neg_half_sqrt2);
        ASSERT_NUMBER_EQ(tan_pi_6, inv_sqrt3);
        ASSERT_NUMBER_EQ(tan_pi_4, NUM_ONE);
        ASSERT_NUMBER_EQ(tan_pi_3, NUM_SQRT3);
        ASSERT_NUMBER_EQ(tan_3pi_4, NUM_NEG_ONE);
        ASSERT_EQ_INT(num_sincos(NUM_PI_6, &sin_pi_6_pair, &cos_pi_6_pair), 0);
        ASSERT_NUMBER_EQ(sin_pi_6_pair, NUM_HALF);
        ASSERT_NUMBER_EQ(cos_pi_6_pair, half_sqrt3);
        ASSERT_NUMBER_EQ(sinh_i_pi_6, i_half);
        ASSERT_NUMBER_EQ(cosh_i_pi_3, NUM_HALF);
        ASSERT_NUMBER_EQ(tanh_i_pi_4, i_value);
        ASSERT_NUMBER_EQ(tanh_i_3pi_4, neg_i);
        ASSERT_EQ_INT(num_sinhcosh(i_pi_3, &sinh_i_pi_3_pair, &cosh_i_pi_3_pair), 0);
        ASSERT_NUMBER_EQ(sinh_i_pi_3_pair, i_half_sqrt3);
        ASSERT_NUMBER_EQ(cosh_i_pi_3_pair, NUM_HALF);

        num_destroy(&pi);
        num_destroy(&e);
        num_destroy(&gamma);
        num_destroy(&phi);
        num_destroy(&sqrt3);
        num_destroy(&i);
        num_destroy(&million);
        num_destroy(&sin_pi_6);
        num_destroy(&sin_pi_4);
        num_destroy(&sin_pi_3);
        num_destroy(&sin_pi_2);
        num_destroy(&cos_pi_6);
        num_destroy(&cos_pi_4);
        num_destroy(&cos_pi_3);
        num_destroy(&cos_pi_2);
        num_destroy(&cos_3pi_4);
        num_destroy(&tan_pi_6);
        num_destroy(&tan_pi_4);
        num_destroy(&tan_pi_3);
        num_destroy(&tan_3pi_4);
        num_destroy(&sin_pi_6_pair);
        num_destroy(&cos_pi_6_pair);
        num_destroy(&i_pi_6);
        num_destroy(&i_pi_3);
        num_destroy(&i_pi_4);
        num_destroy(&i_3pi_4);
        num_destroy(&sinh_i_pi_6);
        num_destroy(&cosh_i_pi_3);
        num_destroy(&tanh_i_pi_4);
        num_destroy(&tanh_i_3pi_4);
        num_destroy(&sinh_i_pi_3_pair);
        num_destroy(&cosh_i_pi_3_pair);
        num_destroy(&i_half_sqrt3);
        num_destroy(&half_sqrt2);
        num_destroy(&half_sqrt3);
        num_destroy(&neg_half_sqrt2);
        num_destroy(&inv_sqrt3);
        num_destroy(&i_half);
        num_destroy(&i_value);
        num_destroy(&neg_i);
    }
}
