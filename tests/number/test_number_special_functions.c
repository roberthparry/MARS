#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_number.h"

static void assert_number_close_text(const char *label,
                                     number_t got,
                                     const char *expected_text,
                                     const char *tolerance_text)
{
    number_t expected = num_create_from_string(expected_text);
    number_t diff = num_sub(got, expected);
    number_t error = num_abs(diff);
    number_t tolerance = num_create_from_string(tolerance_text);
    char *error_text = num_to_string(error);

    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected_text);
    printf("    tolerance = %s\n", tolerance_text);
    printf("    error    = %s\n\n", error_text ? error_text : "(num_to_string failed)");
    ASSERT_TRUE(num_lt(error, tolerance));

    free(error_text);
    num_destroy(&tolerance);
    num_destroy(&error);
    num_destroy(&diff);
    num_destroy(&expected);
}

static void assert_number_close_number(const char *label,
                                       number_t got,
                                       number_t expected,
                                       const char *tolerance_text)
{
    number_t diff = num_sub(got, expected);
    number_t error = num_abs(diff);
    number_t tolerance = num_create_from_string(tolerance_text);
    char *expected_text = num_to_string(expected);
    char *error_text = num_to_string(error);

    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected_text ? expected_text : "(num_to_string failed)");
    printf("    tolerance = %s\n", tolerance_text);
    printf("    error    = %s\n\n", error_text ? error_text : "(num_to_string failed)");
    ASSERT_TRUE(num_lt(error, tolerance));

    free(error_text);
    free(expected_text);
    num_destroy(&tolerance);
    num_destroy(&error);
    num_destroy(&diff);
}

void run_number_special_function_tests(void)
{
    printf(C_CYAN "Testing special functions and extended dispatch...\n" C_RESET);

    {
        number_t zero = num_create_from_string("0");
        number_t one = num_create_from_string("1");
        number_t two = num_create_from_string("2");
        number_t five = num_create_from_string("5");
        number_t fifty_two = num_create_from_string("52");
        number_t lgamma5 = num_lgamma(five);
        number_t digamma1 = num_digamma(one);
        number_t trigamma1 = num_trigamma(one);
        number_t polygamma0_1 = num_polygamma(0u, one);
        number_t polygamma1_1 = num_polygamma(1u, one);
        number_t polygamma3_2 = num_polygamma(3u, two);
        number_t neg_half = num_create_from_string("-1/2");
        number_t polygamma3_neg_half = num_polygamma(3u, neg_half);
        number_t fifteen = num_create_from_long(15);
        number_t six = num_create_from_long(6);
        number_t ninety_six = num_create_from_long(96);
        number_t pi4 = num_pow_int(NUM_PI, 4);
        number_t pi4_over_15 = num_div(pi4, fifteen);
        number_t polygamma3_2_expect = num_sub(pi4_over_15, six);
        number_t polygamma3_neg_half_expect = num_add(pi4, ninety_six);
        number_t erf1 = num_erf(one);
        number_t erfc1 = num_erfc(one);
        number_t w0_1 = num_lambert_w0(one);
        number_t neg_inv_e = num_neg(NUM_INV_E);
        number_t w0_branch = num_lambert_w0(NUM_NEG_INV_E);
        number_t wm1_branch = num_lambert_wm1(NUM_NEG_INV_E);
        number_t productlog_branch = num_productlog(NUM_NEG_INV_E);
        number_t neg_two = num_create_from_string("-2");
        number_t productlog_neg_two = num_productlog(neg_two);
        number_t productlog_exp = num_exp(productlog_neg_two);
        number_t productlog_check = num_mul(productlog_neg_two, productlog_exp);
        number_t i_over_13 = num_create_from_string("1/13i");
        number_t exp_i_over_13 = num_exp(i_over_13);
        number_t productlog_inverse_arg = num_mul(i_over_13, exp_i_over_13);
        number_t productlog_inverse = num_productlog(productlog_inverse_arg);
        number_t beta22 = num_beta(two, two);
        number_t logbeta22 = num_logbeta(two, two);
        number_t binom = num_binomial(fifty_two, five);
        number_t normal_pdf0 = num_normal_pdf(zero);
        number_t normal_cdf0 = num_normal_cdf(zero);
        number_t e1_1 = num_e1(one);

        assert_number_string_prefix("num_lgamma(5)", lgamma5,
                                    "3.178053830347945619646941601297");
        assert_number_string_prefix("num_digamma(1)", digamma1,
                                    "-0.577215664901532860606512090082");
        assert_number_string_prefix("num_trigamma(1)", trigamma1,
                                    "1.644934066848226436472415166646");
        assert_number_close_text("num_polygamma(0, 1) = num_digamma(1)",
                                 polygamma0_1,
                                 "-0.577215664901532860606512090082",
                                 "1e-30");
        assert_number_close_text("num_polygamma(1, 1) = num_trigamma(1)",
                                 polygamma1_1,
                                 "1.644934066848226436472415166646",
                                 "1e-30");
        assert_number_close_number("num_polygamma(3, 2) = pi^4/15 - 6",
                                   polygamma3_2, polygamma3_2_expect,
                                   "1e-25");
        assert_number_close_number("num_polygamma(3, -1/2) = pi^4 + 96",
                                   polygamma3_neg_half,
                                   polygamma3_neg_half_expect,
                                   "1e-25");
        assert_number_string_prefix("num_erf(1)", erf1,
                                    "0.842700792949714869341220635082");
        assert_number_string_prefix("num_erfc(1)", erfc1,
                                    "0.157299207050285130658779364917");
        assert_number_string_prefix("num_lambert_w0(1)", w0_1,
                                    "0.567143290409783872999968662210");
        ASSERT_TRUE(num_eq(neg_inv_e, NUM_NEG_INV_E));
        assert_number_string("num_lambert_w0(-1/e)", w0_branch, "-1");
        assert_number_string("num_lambert_wm1(-1/e)", wm1_branch, "-1");
        assert_number_string("num_productlog(-1/e)", productlog_branch, "-1");
        ASSERT_TRUE(!num_is_real(productlog_neg_two));
        assert_number_close_text("num_productlog(-2) satisfies w*exp(w) = -2",
                                 productlog_check, "-2", "1e-25");
        assert_number_close_text("num_productlog((i/13)*exp(i/13)) = i/13",
                                 productlog_inverse, "1/13i", "1e-30");
        assert_number_string_prefix("num_beta(2, 2)", beta22,
                                    "0.166666666666666666666666666666");
        assert_number_string_prefix("num_logbeta(2, 2)", logbeta22,
                                    "-1.791759469228055000812477358380");
        assert_number_string("num_binomial(52, 5)", binom, "2598960");
        assert_number_string_prefix("num_normal_pdf(0)", normal_pdf0,
                                    "0.398942280401432677939946059934");
        assert_number_string("num_normal_cdf(0)", normal_cdf0, "0.5");
        assert_number_string_prefix("num_e1(1)", e1_1,
                                    "0.219383934395520273677163775460");

        ASSERT_TRUE(num_is_real(lgamma5));
        ASSERT_TRUE(num_is_real(digamma1));
        ASSERT_TRUE(num_is_real(trigamma1));
        ASSERT_TRUE(num_is_real(polygamma3_2));
        ASSERT_TRUE(num_is_real(polygamma3_neg_half));
        ASSERT_TRUE(num_is_real(beta22));
        ASSERT_TRUE(num_is_real(logbeta22));

        num_destroy(&zero);
        num_destroy(&one);
        num_destroy(&two);
        num_destroy(&five);
        num_destroy(&fifty_two);
        num_destroy(&lgamma5);
        num_destroy(&digamma1);
        num_destroy(&trigamma1);
        num_destroy(&polygamma0_1);
        num_destroy(&polygamma1_1);
        num_destroy(&polygamma3_2);
        num_destroy(&neg_half);
        num_destroy(&polygamma3_neg_half);
        num_destroy(&fifteen);
        num_destroy(&six);
        num_destroy(&ninety_six);
        num_destroy(&pi4);
        num_destroy(&pi4_over_15);
        num_destroy(&polygamma3_2_expect);
        num_destroy(&polygamma3_neg_half_expect);
        num_destroy(&erf1);
        num_destroy(&erfc1);
        num_destroy(&w0_1);
        num_destroy(&neg_inv_e);
        num_destroy(&w0_branch);
        num_destroy(&wm1_branch);
        num_destroy(&productlog_branch);
        num_destroy(&neg_two);
        num_destroy(&productlog_neg_two);
        num_destroy(&productlog_exp);
        num_destroy(&productlog_check);
        num_destroy(&i_over_13);
        num_destroy(&exp_i_over_13);
        num_destroy(&productlog_inverse_arg);
        num_destroy(&productlog_inverse);
        num_destroy(&beta22);
        num_destroy(&logbeta22);
        num_destroy(&binom);
        num_destroy(&normal_pdf0);
        num_destroy(&normal_cdf0);
        num_destroy(&e1_1);
    }

    {
        size_t saved_precision = num_get_default_prec_bits();
        number_t five;
        number_t gamma_inv_5;
        number_t lgamma_gamma_inv_5;
        number_t log_5;
        number_t residual;
        number_t residual_mag;
        number_t tolerance;
        char *residual_text;
        bool within_tolerance;

        ASSERT_EQ_INT(num_set_default_prec_digits(201u), 0);
        five = num_create_from_string("5");
        gamma_inv_5 = num_gammainv(five);
        lgamma_gamma_inv_5 = num_lgamma(gamma_inv_5);
        log_5 = num_log(five);
        residual = num_sub(lgamma_gamma_inv_5, log_5);
        residual_mag = num_abs(residual);
        tolerance = num_pow10(-193);
        residual_text = num_to_string(residual_mag);

        printf(C_WHITE C_BOLD
               "lgamma(gammainv(5)) - ln(5) at 193 displayed digits"
               C_RESET "\n");
        printf("    tolerance = 1e-193\n");
        printf("    residual  = %s\n\n",
               residual_text ? residual_text : "(num_to_string failed)");
        within_tolerance = num_lt(residual_mag, tolerance);

        free(residual_text);
        num_destroy(&tolerance);
        num_destroy(&residual_mag);
        num_destroy(&residual);
        num_destroy(&log_5);
        num_destroy(&lgamma_gamma_inv_5);
        num_destroy(&gamma_inv_5);
        num_destroy(&five);
        ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
        ASSERT_TRUE(within_tolerance);
    }

    {
        size_t saved_precision = num_get_default_prec_bits();
        number_t one = num_create_from_long(1);
        number_t six = num_create_from_long(6);
        number_t fifteen = num_create_from_long(15);
        number_t z;
        number_t z_plus_one;
        number_t z4;
        number_t psi3_1;
        number_t pi;
        number_t pi4;
        number_t pi4_over_15;
        number_t psi3_z;
        number_t psi3_z_plus_one;
        number_t lhs;
        number_t rhs;
        number_t residual;
        number_t residual_mag;
        number_t tolerance;
        char *residual_text;
        bool within_tolerance;

        ASSERT_EQ_INT(num_set_default_prec_bits(640u), 0);
        z = num_create_from_string("1.25 + 0.75i");
        z_plus_one = num_add(z, one);
        z4 = num_pow_int(z, 4);
        psi3_1 = num_polygamma(3u, one);
        pi = num_const_prec(NUM_PI, 640u);
        pi4 = num_pow_int(pi, 4);
        pi4_over_15 = num_div(pi4, fifteen);
        psi3_z = num_polygamma(3u, z);
        psi3_z_plus_one = num_polygamma(3u, z_plus_one);
        lhs = num_sub(psi3_z, psi3_z_plus_one);
        rhs = num_div(six, z4);
        residual = num_sub(lhs, rhs);
        residual_mag = num_abs(residual);
        tolerance = num_pow10(-180);
        residual_text = num_to_string(residual_mag);

        assert_number_close_number("high-precision num_polygamma(3, 1) = pi^4/15",
                                   psi3_1, pi4_over_15, "1e-180");
        printf(C_WHITE C_BOLD
               "high-precision complex recurrence: ψ⁽³⁾(z)-ψ⁽³⁾(z+1)=6/z^4"
               C_RESET "\n");
        printf("    tolerance = 1e-180\n");
        printf("    residual  = %s\n\n",
               residual_text ? residual_text : "(num_to_string failed)");
        within_tolerance = num_lt(residual_mag, tolerance);

        free(residual_text);
        num_destroy(&tolerance);
        num_destroy(&residual_mag);
        num_destroy(&residual);
        num_destroy(&rhs);
        num_destroy(&lhs);
        num_destroy(&psi3_z_plus_one);
        num_destroy(&psi3_z);
        num_destroy(&pi4_over_15);
        num_destroy(&pi4);
        num_destroy(&pi);
        num_destroy(&psi3_1);
        num_destroy(&z4);
        num_destroy(&z_plus_one);
        num_destroy(&z);
        num_destroy(&fifteen);
        num_destroy(&six);
        num_destroy(&one);
        ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
        ASSERT_TRUE(within_tolerance);
    }
}
