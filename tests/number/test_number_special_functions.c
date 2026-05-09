#include <stdio.h>

#include "test_number.h"

void run_number_special_function_tests(void)
{
    printf(C_CYAN "Testing special functions and extended dispatch...\n" C_RESET);

    {
        number_t zero = num_create_string("0");
        number_t one = num_create_string("1");
        number_t two = num_create_string("2");
        number_t five = num_create_string("5");
        number_t fifty_two = num_create_string("52");
        number_t lgamma5 = num_lgamma(five);
        number_t digamma1 = num_digamma(one);
        number_t trigamma1 = num_trigamma(one);
        number_t erf1 = num_erf(one);
        number_t erfc1 = num_erfc(one);
        number_t w0_1 = num_lambert_w0(one);
        number_t beta22 = num_beta(two, two);
        number_t logbeta22 = num_logbeta(two, two);
        number_t binom = num_binomial(fifty_two, five);
        number_t normal_pdf0 = num_normal_pdf(zero);
        number_t normal_cdf0 = num_normal_cdf(zero);
        number_t e1_1 = num_e1(one);

        assert_number_string_prefix("num_lgamma(5)", lgamma5, "3.17805");
        assert_number_string_prefix("num_digamma(1)", digamma1, "-0.57721");
        assert_number_string_prefix("num_trigamma(1)", trigamma1, "1.64493");
        assert_number_string_prefix("num_erf(1)", erf1, "0.84270");
        assert_number_string_prefix("num_erfc(1)", erfc1, "0.15729");
        assert_number_string_prefix("num_lambert_w0(1)", w0_1, "0.56714");
        assert_number_string_prefix("num_beta(2, 2)", beta22, "0.16666");
        assert_number_string_prefix("num_logbeta(2, 2)", logbeta22, "-1.79175");
        assert_number_string("num_binomial(52, 5)", binom, "2598960");
        assert_number_string_prefix("num_normal_pdf(0)", normal_pdf0, "0.39894");
        assert_number_string("num_normal_cdf(0)", normal_cdf0, "0.5");
        assert_number_string_prefix("num_e1(1)", e1_1, "0.21938");

        ASSERT_TRUE(num_is_real(lgamma5));
        ASSERT_TRUE(num_is_real(digamma1));
        ASSERT_TRUE(num_is_real(trigamma1));
        ASSERT_TRUE(num_is_real(beta22));
        ASSERT_TRUE(num_is_real(logbeta22));

        num_clear(&zero);
        num_clear(&one);
        num_clear(&two);
        num_clear(&five);
        num_clear(&fifty_two);
        num_clear(&lgamma5);
        num_clear(&digamma1);
        num_clear(&trigamma1);
        num_clear(&erf1);
        num_clear(&erfc1);
        num_clear(&w0_1);
        num_clear(&beta22);
        num_clear(&logbeta22);
        num_clear(&binom);
        num_clear(&normal_pdf0);
        num_clear(&normal_cdf0);
        num_clear(&e1_1);
    }
}
