#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_number.h"

static void assert_number_close_text(const char *label, number_t got, const char *expected_text,
                                     const char *tolerance_text)
{
    number_t expected = num_create_from_string(expected_text);
    number_t diff = num_sub(got, expected);
    number_t error = num_abs(diff);
    number_t tolerance = num_create_from_string(tolerance_text);
    string_t *error_text = num_to_string(error);

    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected_text);
    printf("    tolerance = %s\n", tolerance_text);
    printf("    error    = %s\n\n", error_text ? string_c_str(error_text) : "(num_to_string failed)");
    ASSERT_TRUE(num_lt(error, tolerance));

    string_free(error_text);
    num_destroy(&tolerance);
    num_destroy(&error);
    num_destroy(&diff);
    num_destroy(&expected);
}

static void assert_number_close_number(const char *label, number_t got, number_t expected, const char *tolerance_text)
{
    number_t diff = num_sub(got, expected);
    number_t error = num_abs(diff);
    number_t tolerance = num_create_from_string(tolerance_text);
    string_t *expected_text = num_to_string(expected);
    string_t *error_text = num_to_string(error);

    printf(C_WHITE C_BOLD "%s" C_RESET "\n", label ? label : "<unspecified>");
    printf("    expected = %s\n", expected_text ? string_c_str(expected_text) : "(num_to_string failed)");
    printf("    tolerance = %s\n", tolerance_text);
    printf("    error    = %s\n\n", error_text ? string_c_str(error_text) : "(num_to_string failed)");
    ASSERT_TRUE(num_lt(error, tolerance));

    string_free(error_text);
    string_free(expected_text);
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
        number_t three = num_create_from_string("3");
        number_t four = num_create_from_string("4");
        number_t five = num_create_from_string("5");
        number_t fifty_two = num_create_from_string("52");
        number_t lgamma5 = num_lgamma(five);
        number_t digamma1 = num_digamma(one);
        number_t trigamma1 = num_trigamma(one);
        number_t trigamma_inf = num_trigamma(NUM_INF);
        number_t tetragamma_inf = num_tetragamma(NUM_INF);
        number_t polygamma0_1 = num_polygamma(0u, one);
        number_t polygamma1_1 = num_polygamma(1u, one);
        number_t polygamma3_2 = num_polygamma(3u, two);
        number_t polygamma3_inf = num_polygamma(3u, NUM_INF);
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
        number_t lambert_wn_two = num_lambert_wn(two, neg_two);
        number_t lambert_wn_two_exp = num_exp(lambert_wn_two);
        number_t lambert_wn_two_check = num_mul(lambert_wn_two, lambert_wn_two_exp);
        number_t i_over_13 = num_create_from_string("1/13i");
        number_t exp_i_over_13 = num_exp(i_over_13);
        number_t productlog_inverse_arg = num_mul(i_over_13, exp_i_over_13);
        number_t productlog_inverse = num_productlog(productlog_inverse_arg);
        number_t beta22 = num_beta(two, two);
        number_t logbeta22 = num_logbeta(two, two);
        number_t binom = num_binomial(fifty_two, five);
        number_t normal_pdf0 = num_normal_pdf(zero);
        number_t normal_cdf0 = num_normal_cdf(zero);
        number_t e1_1 = num_E1(one);
        number_t pi_over_three = num_div(NUM_PI, three);
        number_t pi_over_six = num_div(NUM_PI, six);
        number_t one_half = num_div(one, two);
        number_t dilog_half = num_dilog(one_half);
        number_t polylog2_half = num_polylog(two, one_half);
        number_t harmonic4_half = num_harmonic_poly(four, one_half);
        number_t dilog_two = num_dilog(two);
        number_t three_halves = num_div(three, two);
        number_t one_quarter = num_div(one, four);
        number_t three_quarters = num_div(three, four);
        number_t versin_pi3 = num_versin(pi_over_three);
        number_t vercos_pi3 = num_vercos(pi_over_three);
        number_t coversin_pi6 = num_coversin(pi_over_six);
        number_t covercos_pi6 = num_covercos(pi_over_six);
        number_t haversin_pi3 = num_haversin(pi_over_three);
        number_t havercos_pi3 = num_havercos(pi_over_three);
        number_t hacoversin_pi6 = num_hacoversin(pi_over_six);
        number_t hacovercos_pi6 = num_hacovercos(pi_over_six);
        number_t arcversin_half = num_arcversin(one_half);
        number_t arcvercos_three_halves = num_arcvercos(three_halves);
        number_t arccoversin_half = num_arccoversin(one_half);
        number_t arccovercos_three_halves = num_arccovercos(three_halves);
        number_t archaversin_quarter = num_archaversin(one_quarter);
        number_t archavercos_three_quarters = num_archavercos(three_quarters);
        number_t archacoversin_quarter = num_archacoversin(one_quarter);
        number_t archacovercos_three_quarters = num_archacovercos(three_quarters);

        assert_number_string_prefix("num_lgamma(5)", lgamma5, "3.178053830347945619646941601297");
        assert_number_string_prefix("num_digamma(1)", digamma1, "-0.577215664901532860606512090082");
        assert_number_string_prefix("num_trigamma(1)", trigamma1, "1.644934066848226436472415166646");
        assert_number_string("num_trigamma(inf)", trigamma_inf, "0");
        assert_number_string("num_tetragamma(inf)", tetragamma_inf, "0");
        assert_number_close_text("num_polygamma(0, 1) = num_digamma(1)", polygamma0_1,
                                 "-0.577215664901532860606512090082", "1e-30");
        assert_number_close_text("num_polygamma(1, 1) = num_trigamma(1)", polygamma1_1,
                                 "1.644934066848226436472415166646", "1e-30");
        assert_number_close_number("num_polygamma(3, 2) = pi^4/15 - 6", polygamma3_2, polygamma3_2_expect, "1e-25");
        assert_number_string("num_polygamma(3, inf)", polygamma3_inf, "0");
        assert_number_close_number("num_polygamma(3, -1/2) = pi^4 + 96", polygamma3_neg_half,
                                   polygamma3_neg_half_expect, "1e-25");
        assert_number_string_prefix("num_erf(1)", erf1, "0.842700792949714869341220635082");
        assert_number_string_prefix("num_erfc(1)", erfc1, "0.157299207050285130658779364917");
        assert_number_string_prefix("num_lambert_w0(1)", w0_1, "0.567143290409783872999968662210");
        ASSERT_TRUE(num_eq(neg_inv_e, NUM_NEG_INV_E));
        assert_number_string("num_lambert_w0(-1/e)", w0_branch, "-1");
        assert_number_string("num_lambert_wm1(-1/e)", wm1_branch, "-1");
        assert_number_string("num_productlog(-1/e)", productlog_branch, "-1");
        ASSERT_TRUE(!num_is_real(productlog_neg_two));
        assert_number_close_text("num_productlog(-2) satisfies w*exp(w) = -2", productlog_check, "-2", "1e-25");
        assert_number_close_text("num_lambert_wn(2, -2) satisfies w*exp(w) = -2", lambert_wn_two_check, "-2", "1e-25");
        assert_number_close_text("num_productlog((i/13)*exp(i/13)) = i/13", productlog_inverse, "1/13i", "1e-30");
        assert_number_string_prefix("num_beta(2, 2)", beta22, "0.166666666666666666666666666666");
        assert_number_string_prefix("num_logbeta(2, 2)", logbeta22, "-1.791759469228055000812477358380");
        assert_number_string("num_binomial(52, 5)", binom, "2598960");
        assert_number_string_prefix("num_normal_pdf(0)", normal_pdf0, "0.398942280401432677939946059934");
        assert_number_string("num_normal_cdf(0)", normal_cdf0, "0.5");
        assert_number_string_prefix("num_E1(1)", e1_1, "0.219383934395520273677163775460");
        assert_number_close_text("num_dilog(1/2) = pi^2/12 - log(2)^2/2", dilog_half,
                                 "0.58224052646501250590265632015968", "1e-30");
        assert_number_close_text("num_polylog(2, 1/2) = num_dilog(1/2)", polylog2_half,
                                 "0.58224052646501250590265632015968", "1e-30");
        assert_number_string("num_harmonic_poly(4, 1/2)", harmonic4_half, "¹³¹⁄₁₉₂");
        ASSERT_TRUE(!num_is_real(dilog_two));
        assert_number_close_number("num_versin(pi/3) = 1/2", versin_pi3, one_half, "1e-30");
        assert_number_close_number("num_vercos(pi/3) = 3/2", vercos_pi3, three_halves, "1e-30");
        assert_number_close_number("num_coversin(pi/6) = 1/2", coversin_pi6, one_half, "1e-30");
        assert_number_close_number("num_covercos(pi/6) = 3/2", covercos_pi6, three_halves, "1e-30");
        assert_number_close_number("num_haversin(pi/3) = 1/4", haversin_pi3, one_quarter, "1e-30");
        assert_number_close_number("num_havercos(pi/3) = 3/4", havercos_pi3, three_quarters, "1e-30");
        assert_number_close_number("num_hacoversin(pi/6) = 1/4", hacoversin_pi6, one_quarter, "1e-30");
        assert_number_close_number("num_hacovercos(pi/6) = 3/4", hacovercos_pi6, three_quarters, "1e-30");
        assert_number_close_number("num_arcversin(1/2) = pi/3", arcversin_half, pi_over_three, "1e-30");
        assert_number_close_number("num_arcvercos(3/2) = pi/3", arcvercos_three_halves, pi_over_three, "1e-30");
        assert_number_close_number("num_arccoversin(1/2) = pi/6", arccoversin_half, pi_over_six, "1e-30");
        assert_number_close_number("num_arccovercos(3/2) = pi/6", arccovercos_three_halves, pi_over_six, "1e-30");
        assert_number_close_number("num_archaversin(1/4) = pi/3", archaversin_quarter, pi_over_three, "1e-30");
        assert_number_close_number("num_archavercos(3/4) = pi/3", archavercos_three_quarters, pi_over_three, "1e-30");
        assert_number_close_number("num_archacoversin(1/4) = pi/6", archacoversin_quarter, pi_over_six, "1e-30");
        assert_number_close_number("num_archacovercos(3/4) = pi/6", archacovercos_three_quarters, pi_over_six, "1e-30");

        ASSERT_TRUE(num_is_real(lgamma5));
        ASSERT_TRUE(num_is_real(digamma1));
        ASSERT_TRUE(num_is_real(trigamma1));
        ASSERT_TRUE(num_is_real(trigamma_inf));
        ASSERT_TRUE(num_is_real(tetragamma_inf));
        ASSERT_TRUE(num_is_real(polygamma3_2));
        ASSERT_TRUE(num_is_real(polygamma3_inf));
        ASSERT_TRUE(num_is_real(polygamma3_neg_half));
        ASSERT_TRUE(num_is_real(beta22));
        ASSERT_TRUE(num_is_real(logbeta22));

        num_destroy(&zero);
        num_destroy(&one);
        num_destroy(&two);
        num_destroy(&three);
        num_destroy(&four);
        num_destroy(&five);
        num_destroy(&fifty_two);
        num_destroy(&lgamma5);
        num_destroy(&digamma1);
        num_destroy(&trigamma1);
        num_destroy(&trigamma_inf);
        num_destroy(&tetragamma_inf);
        num_destroy(&polygamma0_1);
        num_destroy(&polygamma1_1);
        num_destroy(&polygamma3_2);
        num_destroy(&polygamma3_inf);
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
        num_destroy(&lambert_wn_two);
        num_destroy(&lambert_wn_two_exp);
        num_destroy(&lambert_wn_two_check);
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
        num_destroy(&pi_over_three);
        num_destroy(&pi_over_six);
        num_destroy(&one_half);
        num_destroy(&dilog_half);
        num_destroy(&polylog2_half);
        num_destroy(&harmonic4_half);
        num_destroy(&dilog_two);
        num_destroy(&three_halves);
        num_destroy(&one_quarter);
        num_destroy(&three_quarters);
        num_destroy(&versin_pi3);
        num_destroy(&vercos_pi3);
        num_destroy(&coversin_pi6);
        num_destroy(&covercos_pi6);
        num_destroy(&haversin_pi3);
        num_destroy(&havercos_pi3);
        num_destroy(&hacoversin_pi6);
        num_destroy(&hacovercos_pi6);
        num_destroy(&arcversin_half);
        num_destroy(&arcvercos_three_halves);
        num_destroy(&arccoversin_half);
        num_destroy(&arccovercos_three_halves);
        num_destroy(&archaversin_quarter);
        num_destroy(&archavercos_three_quarters);
        num_destroy(&archacoversin_quarter);
        num_destroy(&archacovercos_three_quarters);
    }

    {
        size_t saved_precision = num_get_default_prec_bits();
        number_t z;
        number_t z_plus_one;
        number_t psi_z;
        number_t psi_z_plus_one;
        number_t reciprocal;
        number_t recurrence_lhs;
        number_t recurrence_difference;
        number_t conjugate_z;
        number_t psi_conjugate_z;
        number_t conjugate_psi_z;

        ASSERT_EQ_INT(num_set_default_prec_bits(1280u), 0);
        z = num_create_from_string("1+i");
        z_plus_one = num_add(z, NUM_ONE);
        psi_z = num_digamma(z);
        psi_z_plus_one = num_digamma(z_plus_one);
        reciprocal = num_inv(z);
        recurrence_lhs = num_sub(psi_z_plus_one, psi_z);
        recurrence_difference = num_sub(recurrence_lhs, reciprocal);
        conjugate_z = num_conj(z);
        psi_conjugate_z = num_digamma(conjugate_z);
        conjugate_psi_z = num_conj(psi_z);

        ASSERT_EQ_INT((int)num_get_prec_bits(psi_z), 1280);
        assert_number_close_text("1280-bit complex digamma recurrence", recurrence_difference, "0", "1e-380");
        assert_number_close_number("1280-bit complex digamma conjugation", psi_conjugate_z, conjugate_psi_z,
                                   "1e-380");

        num_destroy(&conjugate_psi_z);
        num_destroy(&psi_conjugate_z);
        num_destroy(&conjugate_z);
        num_destroy(&recurrence_difference);
        num_destroy(&recurrence_lhs);
        num_destroy(&reciprocal);
        num_destroy(&psi_z_plus_one);
        num_destroy(&psi_z);
        num_destroy(&z_plus_one);
        num_destroy(&z);
        ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
    }

    {
        size_t saved_precision = num_get_default_prec_bits();
        number_t order = num_create_from_string("0.5");
        number_t argument = num_create_from_string("1.25");
        number_t two;
        number_t pi;
        number_t pi_x;
        number_t quotient;
        number_t scale;
        number_t sine;
        number_t cosine;
        number_t scaled_cosine;
        number_t expected_j;
        number_t expected_y;
        number_t got_j;
        number_t got_y;
        number_t zero;
        number_t one;
        number_t lommel;
        number_t j0;
        number_t expected_lommel;

        ASSERT_EQ_INT(num_set_default_prec_bits(384u), 0);
        ASSERT_EQ_INT(num_set_prec_bits(&order, 384u), 0);
        ASSERT_EQ_INT(num_set_prec_bits(&argument, 384u), 0);
        two = num_create_from_long(2);
        pi = num_const_prec(NUM_PI, 384u);
        pi_x = num_mul(pi, argument);
        quotient = num_div(two, pi_x);
        scale = num_sqrt(quotient);
        sine = num_sin(argument);
        cosine = num_cos(argument);
        expected_j = num_mul(scale, sine);
        scaled_cosine = num_mul(scale, cosine);
        expected_y = num_neg(scaled_cosine);
        got_j = num_bessel_j(order, argument);
        got_y = num_bessel_y(order, argument);
        zero = num_create_from_long(0);
        one = num_create_from_long(1);
        lommel = num_lommel_s(one, zero, argument);
        j0 = num_bessel_j(zero, argument);
        expected_lommel = num_sub(one, j0);

        assert_number_close_number("high-precision num_bessel_j(1/2, x) half-order identity", got_j, expected_j,
                                   "1e-100");
        assert_number_close_number("high-precision num_bessel_y(1/2, x) half-order identity", got_y, expected_y,
                                   "1e-100");
        assert_number_close_number("high-precision num_lommel_s(1, 0, x) = 1 - J0(x)", lommel, expected_lommel,
                                   "1e-100");
        num_destroy(&expected_lommel);
        num_destroy(&j0);
        num_destroy(&lommel);
        num_destroy(&one);
        num_destroy(&zero);
        num_destroy(&got_y);
        num_destroy(&got_j);
        num_destroy(&expected_y);
        num_destroy(&expected_j);
        num_destroy(&scaled_cosine);
        num_destroy(&cosine);
        num_destroy(&sine);
        num_destroy(&scale);
        num_destroy(&quotient);
        num_destroy(&pi_x);
        num_destroy(&pi);
        num_destroy(&two);
        num_destroy(&argument);
        num_destroy(&order);
        ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
    }

    {
        size_t saved_precision = num_get_default_prec_bits();
        number_t one;
        number_t two;
        number_t half;
        number_t dilog_half;
        number_t polylog2_half;
        number_t pi;
        number_t pi2;
        number_t twelve;
        number_t pi2_over_12;
        number_t log2;
        number_t log2_sq;
        number_t log2_sq_half;
        number_t dilog_half_expected;
        number_t dilog_two;
        number_t dilog_two_real;
        number_t dilog_two_imag;
        number_t four;
        number_t pi2_over_4;
        number_t pi_log2;
        number_t neg_pi_log2;

        ASSERT_EQ_INT(num_set_default_prec_bits(640u), 0);
        one = num_create_from_long(1);
        two = num_create_from_long(2);
        four = num_create_from_long(4);
        twelve = num_create_from_long(12);
        half = num_div(one, two);
        pi = num_const_prec(NUM_PI, 640u);
        pi2 = num_mul(pi, pi);
        pi2_over_12 = num_div(pi2, twelve);
        log2 = num_log(two);
        log2_sq = num_mul(log2, log2);
        log2_sq_half = num_div(log2_sq, two);
        dilog_half_expected = num_sub(pi2_over_12, log2_sq_half);
        dilog_half = num_dilog(half);
        polylog2_half = num_polylog(two, half);
        dilog_two = num_dilog(two);
        dilog_two_real = num_real_part(dilog_two);
        dilog_two_imag = num_imag_part(dilog_two);
        pi2_over_4 = num_div(pi2, four);
        pi_log2 = num_mul(pi, log2);
        neg_pi_log2 = num_neg(pi_log2);

        assert_number_close_number("high-precision num_dilog(1/2)", dilog_half, dilog_half_expected, "1e-180");
        assert_number_close_number("high-precision num_polylog(2, 1/2)", polylog2_half, dilog_half_expected, "1e-180");
        ASSERT_TRUE(!num_is_real(dilog_two));
        assert_number_close_number("high-precision Re num_dilog(2) = pi^2/4", dilog_two_real, pi2_over_4, "1e-180");
        assert_number_close_number("high-precision Im num_dilog(2) = -pi ln 2", dilog_two_imag, neg_pi_log2, "1e-180");

        num_destroy(&neg_pi_log2);
        num_destroy(&pi_log2);
        num_destroy(&pi2_over_4);
        num_destroy(&dilog_two_imag);
        num_destroy(&dilog_two_real);
        num_destroy(&dilog_two);
        num_destroy(&polylog2_half);
        num_destroy(&dilog_half);
        num_destroy(&dilog_half_expected);
        num_destroy(&log2_sq_half);
        num_destroy(&log2_sq);
        num_destroy(&log2);
        num_destroy(&pi2_over_12);
        num_destroy(&twelve);
        num_destroy(&pi2);
        num_destroy(&pi);
        num_destroy(&half);
        num_destroy(&four);
        num_destroy(&two);
        num_destroy(&one);
        ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
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
        string_t *residual_text;
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

        printf(C_WHITE C_BOLD "lgamma(gammainv(5)) - ln(5) at 193 displayed digits" C_RESET "\n");
        printf("    tolerance = 1e-193\n");
        printf("    residual  = %s\n\n", residual_text ? string_c_str(residual_text) : "(num_to_string failed)");
        within_tolerance = num_lt(residual_mag, tolerance);

        string_free(residual_text);
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
        string_t *residual_text;
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

        assert_number_close_number("high-precision num_polygamma(3, 1) = pi^4/15", psi3_1, pi4_over_15, "1e-180");
        printf(C_WHITE C_BOLD "high-precision complex recurrence: ψ⁽³⁾(z)-ψ⁽³⁾(z+1)=6/z^4" C_RESET "\n");
        printf("    tolerance = 1e-180\n");
        printf("    residual  = %s\n\n", residual_text ? string_c_str(residual_text) : "(num_to_string failed)");
        within_tolerance = num_lt(residual_mag, tolerance);

        string_free(residual_text);
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

    {
        number_t z = num_create_from_string("0.2 + 0.1i");
        number_t hypergeometric = num_hypergeometric_pFq(NULL, 0u, NULL, 0u, z);
        number_t expected_hypergeometric = num_exp(z);
        number_t a = num_create_from_string("1.25");
        number_t b[] = {num_create_from_string("0.5"), num_create_from_string("1.5"), num_create_from_string("2")};
        number_t c = num_clone(a);
        number_t variables[] = {num_create_from_string("0.1"), num_create_from_string("0.2"),
                                num_create_from_string("0.15")};
        number_t lauricella = num_lauricella_f(a, b, c, variables, 3u);
        number_t expected_lauricella = num_create_from_long(1);

        for (size_t i = 0u; i < 3u; ++i) {
            number_t one_minus = num_sub(NUM_ONE, variables[i]);
            number_t negative_b = num_neg(b[i]);
            number_t factor = num_pow(one_minus, negative_b);
            number_t next = num_mul(expected_lauricella, factor);

            num_destroy(&expected_lauricella);
            expected_lauricella = next;
            num_destroy(&factor);
            num_destroy(&negative_b);
            num_destroy(&one_minus);
        }

        assert_number_close_number("num 0F0(z) = exp(z)", hypergeometric, expected_hypergeometric, "1e-27");
        assert_number_close_number("general num Lauricella FD product identity", lauricella, expected_lauricella,
                                   "1e-26");

        num_destroy(&expected_lauricella);
        num_destroy(&lauricella);
        for (size_t i = 0u; i < 3u; ++i) {
            num_destroy(&variables[i]);
            num_destroy(&b[i]);
        }
        num_destroy(&c);
        num_destroy(&a);
        num_destroy(&expected_hypergeometric);
        num_destroy(&hypergeometric);
        num_destroy(&z);
    }


    {
        number_t three = num_create_from_long(3);
        number_t zero = num_create_from_long(0);
        number_t complex_argument = num_create_from_string("2 + 3i");
        number_t zeta_three = num_zeta(three);
        number_t zeta_complex = num_zeta(complex_argument);
        number_t derivative_zero = num_zetap(zero);
        number_t derivative_nan = num_zetap(NUM_NAN);
        number_t two_and_half = num_create_from_string("2.5");
        number_t one_hundred_and_one = num_create_from_long(101);
        number_t hurwitz = num_zetah(two_and_half, one_hundred_and_one);
        number_t hurwitz_derivative = num_zatahp(two_and_half, one_hundred_and_one);

        assert_number_string_prefix("num_zeta(3)", zeta_three, "1.2020569031595942853997381615114499");
        assert_number_close_text("num_zeta(2 + 3i)", zeta_complex,
                                 "0.798021985146275720622294500724813 - 0.113744308052938500215913365857315i",
                                 "1e-30");
        assert_number_string_prefix("num_zetap(0)", derivative_zero,
                                    "-0.9189385332046727417803297364056176");
        TEST_ASSERT_TRUE(num_is_nan(derivative_nan), "num_zetap(NaN) remains NaN");
        assert_number_string_prefix("num_zetah(2.5, 101)", hurwitz,
                                    "0.000661687499453171542062211501479711744239330816315948606222019");
        assert_number_string_prefix("num_zatahp(2.5, 101)", hurwitz_derivative,
                                    "-0.00349161965653033810674455840434715303797124292360183374983833");

        num_destroy(&hurwitz_derivative);
        num_destroy(&hurwitz);
        num_destroy(&one_hundred_and_one);
        num_destroy(&two_and_half);
        num_destroy(&derivative_nan);
        num_destroy(&derivative_zero);
        num_destroy(&zeta_complex);
        num_destroy(&zeta_three);
        num_destroy(&complex_argument);
        num_destroy(&zero);
        num_destroy(&three);
    }

    {
        number_t q = num_create_from_string("1/2");
        number_t one = num_create_from_long(1L);
        number_t two = num_create_from_long(2L);
        number_t psi_two = number_qdigamma(q, two);
        number_t psi_one = num_qdigamma(q, one);
        number_t recurrence = num_sub(psi_two, psi_one);

        assert_number_close_text("number_qdigamma recurrence", recurrence,
                                 "0.69314718055994530941723212145817656807550013436026", "1e-29");
        num_destroy(&recurrence);
        num_destroy(&psi_one);
        num_destroy(&psi_two);
        num_destroy(&two);
        num_destroy(&one);
        num_destroy(&q);
    }
}
