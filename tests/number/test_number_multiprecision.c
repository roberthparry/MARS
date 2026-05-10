#include <stdio.h>

#include "test_number.h"

void run_number_multiprecision_tests(void)
{
    printf(C_CYAN "Testing multiprecision real and complex backends...\n" C_RESET);

    {
        mfloat_t *base_real = mf_create_string("1.25");
        mcomplex_t *base_complex = mc_create_string("1 + 2i");

        number_t default_real;
        number_t bits_real;
        number_t digits_real;
        number_t default_complex;
        number_t bits_complex;
        number_t digits_complex;
        number_t clone_real;
        number_t log_real;
        number_t sqrt_complex;

        ASSERT_NOT_NULL(base_real);
        ASSERT_NOT_NULL(base_complex);

        default_real = num_create_from_mfloat(base_real);
        bits_real = num_create_from_mfloat_with_prec_bits(base_real, 512u);
        digits_real = num_create_from_mfloat_with_prec_digits(base_real, 50u);

        default_complex = num_create_from_mcomplex(base_complex);
        bits_complex = num_create_from_mcomplex_with_prec_bits(base_complex, 384u);
        digits_complex = num_create_from_mcomplex_with_prec_digits(base_complex, 40u);

        clone_real = num_clone(default_real);
        log_real = num_log(default_real);
        sqrt_complex = num_sqrt(default_complex);

        ASSERT_EQ_INT((int)num_get_prec_bits(default_real), 1024);
        ASSERT_EQ_INT((int)num_get_prec_bits(bits_real), 512);
        ASSERT_EQ_INT((int)num_get_prec_bits(digits_real), 167);
        ASSERT_EQ_INT((int)num_get_prec_bits(default_complex), 1024);
        ASSERT_EQ_INT((int)num_get_prec_bits(bits_complex), 384);
        ASSERT_EQ_INT((int)num_get_prec_bits(digits_complex), 133);

        ASSERT_TRUE(num_is_real(default_real));
        ASSERT_TRUE(!num_is_real(default_complex));
        ASSERT_TRUE(num_eq(default_real, clone_real));
        ASSERT_TRUE(num_is_real(log_real));
        ASSERT_TRUE(!num_is_real(sqrt_complex));

        ASSERT_EQ_INT(num_set_prec_bits(&clone_real, 256u), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(default_real), 1024);
        ASSERT_EQ_INT((int)num_get_prec_bits(clone_real), 256);

        num_destroy(&default_real);
        num_destroy(&bits_real);
        num_destroy(&digits_real);
        num_destroy(&default_complex);
        num_destroy(&bits_complex);
        num_destroy(&digits_complex);
        num_destroy(&clone_real);
        num_destroy(&log_real);
        num_destroy(&sqrt_complex);

        mf_free(base_real);
        mc_free(base_complex);
    }

    {
        number_t exact_int;
        number_t exact_rat;
        number_t ei_int;
        number_t log_rat;
        number_t two;
        number_t half;
        number_t zero;
        number_t one;
        number_t neg_one;
        number_t one_pow_zero;
        number_t one_pow_one;
        number_t one_pow_neg_one;
        number_t one_pow_two;
        number_t pow_half;
        number_t pow_quarter;
        number_t pow_eighth;
        number_t complex_base;
        number_t complex_exponent_exact;
        number_t complex_exponent_qfloat;
        number_t complex_exponent_text;
        number_t complex_pow_two_exact;
        number_t complex_pow_two_qfloat;
        number_t complex_pow_two_text;
        number_t complex_pow_two_expected;
        number_t exp_half;
        number_t exp_quarter;
        number_t exp_eighth;
        number_t exp_i_pi_2;
        number_t log_one;
        number_t log_e;
        number_t log_inv_e;
        number_t log_two;
        number_t log_half;
        number_t log_i;
        number_t log_neg_one;
        number_t log_neg_i;
        number_t neg_ln2;
        number_t i_pi;
        number_t e_768;
        number_t ln2_768;
        number_t pi_768;
        number_t pi_2_768;
        number_t i_768;
        number_t three;
        number_t pow_three;
        number_t sqrt_two;
        number_t sqrt_sqrt_two;
        number_t sqrt_sqrt_sqrt_two;
        number_t sqrt_e;
        number_t sqrt_sqrt_e;
        number_t sqrt_sqrt_sqrt_e;
        number_t i_pi_2;
        number_t log_i_expected;
        number_t neg_i;
        number_t neg_i_pi_2;

        ASSERT_EQ_INT(num_set_default_prec_bits(768u), 0);

        exact_int = num_create_from_string("1");
        exact_rat = num_create_from_string("3/2");
        ei_int = num_ei(exact_int);
        log_rat = num_log(exact_rat);
        two = num_create_from_string("2");
        half = num_create_from_string("1/2");
        zero = NUM_ZERO;
        one = NUM_ONE;
        neg_one = NUM_NEG_ONE;
        one_pow_zero = num_pow(two, zero);
        one_pow_one = num_pow(two, one);
        one_pow_neg_one = num_pow(two, neg_one);
        one_pow_two = num_pow(two, NUM_TWO);
        pow_half = num_pow(two, half);
        pow_quarter = num_pow(two, NUM_QUARTER);
        pow_eighth = num_pow(two, NUM_ONE_EIGHTH);
        complex_base = num_create_from_string("1 + 2i");
        complex_exponent_exact = num_create_from_string("2");
        complex_exponent_qfloat = num_create_from_qfloat(qf_from_double(2.0));
        complex_exponent_text = num_create_from_string("2.0");
        complex_pow_two_exact = num_pow(complex_base, complex_exponent_exact);
        complex_pow_two_qfloat = num_pow(complex_base, complex_exponent_qfloat);
        complex_pow_two_text = num_pow(complex_base, complex_exponent_text);
        complex_pow_two_expected = num_create_from_string("-3 + 4i");
        exp_half = num_exp(NUM_HALF);
        exp_quarter = num_exp(NUM_QUARTER);
        exp_eighth = num_exp(NUM_ONE_EIGHTH);
        log_one = num_log(NUM_ONE);
        log_e = num_log(NUM_E);
        log_inv_e = num_log(NUM_INV_E);
        log_two = num_log(NUM_TWO);
        log_half = num_log(NUM_HALF);
        log_i = num_log(NUM_I);
        log_neg_one = num_log(NUM_NEG_ONE);
        neg_i = num_neg(NUM_I);
        log_neg_i = num_log(neg_i);
        e_768 = num_create_from_mfloat_with_prec_bits(MF_E, 768u);
        ln2_768 = num_create_from_mfloat_with_prec_bits(MF_LN2, 768u);
        pi_768 = num_create_from_mfloat_with_prec_bits(MF_PI, 768u);
        pi_2_768 = num_create_from_mfloat_with_prec_bits(MF_PI_2, 768u);
        i_768 = num_create_from_mcomplex_with_prec_bits(MC_I, 768u);
        neg_ln2 = num_neg(ln2_768);
        i_pi = num_mul(i_768, pi_768);
        three = num_create_from_long(3);
        pow_three = num_pow(two, three);
        sqrt_two = num_sqrt(two);
        sqrt_sqrt_two = num_sqrt(sqrt_two);
        sqrt_sqrt_sqrt_two = num_sqrt(sqrt_sqrt_two);
        sqrt_e = num_sqrt(e_768);
        sqrt_sqrt_e = num_sqrt(sqrt_e);
        sqrt_sqrt_sqrt_e = num_sqrt(sqrt_sqrt_e);
        i_pi_2 = num_mul(i_768, pi_2_768);
        log_i_expected = num_mul(NUM_I, NUM_PI_2);
        neg_i_pi_2 = num_mul(neg_i, NUM_PI_2);
        exp_i_pi_2 = num_exp(i_pi_2);

        ASSERT_EQ_INT((int)num_get_prec_bits(exact_int), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(exact_rat), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(ei_int), 768);
        ASSERT_EQ_INT((int)num_get_prec_bits(log_rat), 768);
        assert_number_string("num_pow(2, 0)", one_pow_zero, "1");
        ASSERT_TRUE(num_eq(one_pow_one, two));
        assert_number_string("num_pow(2, -1)", one_pow_neg_one, "1/2");
        assert_number_string("num_pow(2, 2)", one_pow_two, "4");
        ASSERT_TRUE(num_eq(pow_half, sqrt_two));
        ASSERT_TRUE(num_eq(pow_quarter, sqrt_sqrt_two));
        ASSERT_TRUE(num_eq(pow_eighth, sqrt_sqrt_sqrt_two));
        ASSERT_TRUE(num_eq(complex_pow_two_exact, complex_pow_two_expected));
        ASSERT_TRUE(num_eq(complex_pow_two_qfloat, complex_pow_two_expected));
        ASSERT_TRUE(num_eq(complex_pow_two_text, complex_pow_two_expected));
        ASSERT_TRUE(num_eq(exp_half, sqrt_e));
        ASSERT_TRUE(num_eq(exp_quarter, sqrt_sqrt_e));
        ASSERT_TRUE(num_eq(exp_eighth, sqrt_sqrt_sqrt_e));
        ASSERT_TRUE(num_eq(exp_i_pi_2, NUM_I));
        ASSERT_TRUE(num_eq(log_one, NUM_ZERO));
        ASSERT_TRUE(num_eq(log_e, NUM_ONE));
        ASSERT_TRUE(num_eq(log_inv_e, NUM_NEG_ONE));
        ASSERT_TRUE(num_eq(log_two, ln2_768));
        ASSERT_TRUE(num_eq(log_half, neg_ln2));
        ASSERT_TRUE(num_eq(log_i, log_i_expected));
        ASSERT_TRUE(num_eq(log_neg_one, i_pi));
        ASSERT_TRUE(num_eq(log_neg_i, neg_i_pi_2));
        assert_number_string("num_pow(2, 3)", pow_three, "8");
        ASSERT_EQ_INT((int)num_get_prec_bits(pow_half), 768);

        num_destroy(&exact_int);
        num_destroy(&exact_rat);
        num_destroy(&ei_int);
        num_destroy(&log_rat);
        num_destroy(&two);
        num_destroy(&half);
        num_destroy(&zero);
        num_destroy(&one);
        num_destroy(&neg_one);
        num_destroy(&one_pow_zero);
        num_destroy(&one_pow_one);
        num_destroy(&one_pow_neg_one);
        num_destroy(&one_pow_two);
        num_destroy(&pow_half);
        num_destroy(&pow_quarter);
        num_destroy(&pow_eighth);
        num_destroy(&complex_base);
        num_destroy(&complex_exponent_exact);
        num_destroy(&complex_exponent_qfloat);
        num_destroy(&complex_exponent_text);
        num_destroy(&complex_pow_two_exact);
        num_destroy(&complex_pow_two_qfloat);
        num_destroy(&complex_pow_two_text);
        num_destroy(&complex_pow_two_expected);
        num_destroy(&exp_half);
        num_destroy(&exp_quarter);
        num_destroy(&exp_eighth);
        num_destroy(&exp_i_pi_2);
        num_destroy(&log_one);
        num_destroy(&log_e);
        num_destroy(&log_inv_e);
        num_destroy(&log_two);
        num_destroy(&log_half);
        num_destroy(&log_i);
        num_destroy(&log_neg_one);
        num_destroy(&log_neg_i);
        num_destroy(&neg_ln2);
        num_destroy(&i_pi);
        num_destroy(&e_768);
        num_destroy(&ln2_768);
        num_destroy(&pi_768);
        num_destroy(&pi_2_768);
        num_destroy(&i_768);
        num_destroy(&three);
        num_destroy(&pow_three);
        num_destroy(&sqrt_two);
        num_destroy(&sqrt_sqrt_two);
        num_destroy(&sqrt_sqrt_sqrt_two);
        num_destroy(&sqrt_e);
        num_destroy(&sqrt_sqrt_e);
        num_destroy(&sqrt_sqrt_sqrt_e);
        num_destroy(&i_pi_2);
        num_destroy(&log_i_expected);
        num_destroy(&neg_i);
        num_destroy(&neg_i_pi_2);

        ASSERT_EQ_INT(num_set_default_prec_bits(1024u), 0);
    }
}
