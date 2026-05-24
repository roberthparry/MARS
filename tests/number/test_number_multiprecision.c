#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

void run_number_multiprecision_tests(void)
{
    printf(C_CYAN "Testing multiprecision real and complex backends...\n" C_RESET);

    {
        number_t default_real;
        number_t bits_real;
        number_t digits_real;
        number_t default_complex;
        number_t bits_complex;
        number_t digits_complex;
        number_t fixed_double;
        number_t fixed_qfloat;
        number_t fixed_qcomplex;
        number_t double_prec;
        number_t qfloat_prec;
        number_t qcomplex_prec;
        number_t qfloat_pi;
        number_t qfloat_pi_prec;
        number_t qcomplex_pi_e;
        number_t qcomplex_pi_e_prec;
        number_t clone_real;
        number_t log_real;
        number_t sqrt_complex;
        char *qcomplex_pi_e_text;
        char *set_real_text;
        char *set_complex_text;
        char *set_qfloat_real_text;
        char *set_qfloat_complex_text;
        char *set_qcomplex_text;

        default_real = num_create_from_string("1.25");
        bits_real = num_const_prec(default_real, 512u);
        digits_real = num_const_prec_digits(default_real, 50u);

        default_complex = num_create_from_string("1 + 2i");
        bits_complex = num_const_prec(default_complex, 384u);
        digits_complex = num_const_prec_digits(default_complex, 40u);

        fixed_double = num_create_from_double(1.25);
        fixed_qfloat = num_create_from_qfloat(qf_from_double(1.25));
        fixed_qcomplex = num_create_from_qcomplex(qc_make(QF_ONE, QF_ONE));
        double_prec = num_const_prec(fixed_double, 512u);
        qfloat_prec = num_const_prec(fixed_qfloat, 640u);
        qcomplex_prec = num_const_prec(fixed_qcomplex, 384u);
        qfloat_pi = num_create_from_qfloat(QF_PI);
        qfloat_pi_prec = num_const_prec(qfloat_pi, 640u);
        qcomplex_pi_e = num_create_from_qcomplex(qc_make(QF_PI, QF_E));
        qcomplex_pi_e_prec = num_const_prec(qcomplex_pi_e, 384u);

        clone_real = num_clone(default_real);
        log_real = num_log(default_real);
        sqrt_complex = num_sqrt(default_complex);
        qcomplex_pi_e_text = num_to_string(qcomplex_pi_e_prec);
        set_real_text = NULL;
        set_complex_text = NULL;
        set_qfloat_real_text = NULL;
        set_qfloat_complex_text = NULL;
        set_qcomplex_text = NULL;

        ASSERT_EQ_INT((int)num_get_prec_bits(default_real), 1024);
        ASSERT_EQ_INT((int)num_get_prec_bits(bits_real), 512);
        ASSERT_EQ_INT((int)num_get_prec_bits(digits_real), 167);
        ASSERT_EQ_INT((int)num_get_prec_bits(default_complex), 1024);
        ASSERT_EQ_INT((int)num_get_prec_bits(bits_complex), 384);
        ASSERT_EQ_INT((int)num_get_prec_bits(digits_complex), 133);
        ASSERT_EQ_INT((int)num_get_prec_bits(fixed_qfloat), 106);
        ASSERT_EQ_INT((int)num_get_prec_bits(fixed_qcomplex), 106);
        ASSERT_EQ_INT((int)num_get_prec_bits(double_prec), 512);
        ASSERT_EQ_INT((int)num_get_prec_bits(qfloat_prec), 640);
        ASSERT_EQ_INT((int)num_get_prec_bits(qcomplex_prec), 384);
        ASSERT_EQ_INT((int)num_get_prec_bits(qfloat_pi_prec), 640);
        ASSERT_EQ_INT((int)num_get_prec_bits(qcomplex_pi_e_prec), 384);

        ASSERT_TRUE(num_is_real(default_real));
        ASSERT_TRUE(!num_is_real(default_complex));
        ASSERT_NUMBER_EQ(default_real, clone_real);
        ASSERT_TRUE(num_is_real(log_real));
        ASSERT_TRUE(!num_is_real(sqrt_complex));
        assert_number_string_prefix("num_const_prec(QF_PI, 640)",
                                    qfloat_pi_prec,
                                    "3.1415926535897932384626433832795");
        ASSERT_NOT_NULL(qcomplex_pi_e_text);
        ASSERT_TRUE(strstr(qcomplex_pi_e_text,
                           "3.1415926535897932384626433832795") != NULL);
        ASSERT_TRUE(strstr(qcomplex_pi_e_text,
                           "2.7182818284590452353602874713526") != NULL);

        ASSERT_EQ_INT(num_set_prec_bits(&clone_real, 256u), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(default_real), 1024);
        ASSERT_EQ_INT((int)num_get_prec_bits(clone_real), 256);
        ASSERT_EQ_INT(num_set_double(&clone_real, 2.5), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(clone_real), 256);
        set_real_text = num_to_string(clone_real);
        ASSERT_NOT_NULL(set_real_text);
        ASSERT_TRUE(strcmp(set_real_text, "2.5") == 0);

        ASSERT_EQ_INT(num_set_double(&bits_complex, 2.5), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(bits_complex), 384);
        set_complex_text = num_to_string(bits_complex);
        ASSERT_NOT_NULL(set_complex_text);
        ASSERT_TRUE(strcmp(set_complex_text, "2.5") == 0);

        ASSERT_EQ_INT(num_set_qfloat(&clone_real, qf_from_double(3.25)), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(clone_real), 256);
        set_qfloat_real_text = num_to_string(clone_real);
        ASSERT_NOT_NULL(set_qfloat_real_text);
        ASSERT_TRUE(strcmp(set_qfloat_real_text, "3.25") == 0);

        ASSERT_EQ_INT(num_set_qfloat(&bits_complex, qf_from_double(3.25)), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(bits_complex), 384);
        set_qfloat_complex_text = num_to_string(bits_complex);
        ASSERT_NOT_NULL(set_qfloat_complex_text);
        ASSERT_TRUE(strcmp(set_qfloat_complex_text, "3.25") == 0);

        ASSERT_EQ_INT(num_set_qcomplex(
            &bits_complex,
            qc_make(qf_from_double(1.5), qf_from_double(0.25))), 0);
        ASSERT_EQ_INT((int)num_get_prec_bits(bits_complex), 384);
        set_qcomplex_text = num_to_string(bits_complex);
        ASSERT_NOT_NULL(set_qcomplex_text);
        ASSERT_TRUE(strcmp(set_qcomplex_text, "1.5 + 0.25i") == 0);

        num_destroy(&default_real);
        num_destroy(&bits_real);
        num_destroy(&digits_real);
        num_destroy(&default_complex);
        num_destroy(&bits_complex);
        num_destroy(&digits_complex);
        num_destroy(&fixed_double);
        num_destroy(&fixed_qfloat);
        num_destroy(&fixed_qcomplex);
        num_destroy(&double_prec);
        num_destroy(&qfloat_prec);
        num_destroy(&qcomplex_prec);
        num_destroy(&qfloat_pi);
        num_destroy(&qfloat_pi_prec);
        num_destroy(&qcomplex_pi_e);
        num_destroy(&qcomplex_pi_e_prec);
        num_destroy(&clone_real);
        num_destroy(&log_real);
        num_destroy(&sqrt_complex);
        free(qcomplex_pi_e_text);
        free(set_real_text);
        free(set_complex_text);
        free(set_qfloat_real_text);
        free(set_qfloat_complex_text);
        free(set_qcomplex_text);
    }

    {
        enum {
            MATH_PRECISION_BITS = 256,
            MATH_PRECISION_DIGITS = 78,
            PI_2048_BITS = 2048,
            PI_2048_DIGITS = 617
        };
        static const struct {
            const char *x;
            const char *prefix;
        } heegner_cases[] = {
            {
                "19.0",
                "885479.777680154319497537893481719626820714286501855357152657711012",
            },
            {
                "43.0",
                "884736743.999777466034906661937462078585376847399127139160917514627",
            },
            {
                "67.0",
                "147197952743.999998662454224506829261312578628508183312503816712633",
            },
            {
                "163.0",
                "262537412640768743.999999999999250072597198185688879353856337336990862",
            },
        };
        NUM_SCOPE(scope);
        char buf[1024];
        number_t pi_256 = num_const_prec(NUM_PI, MATH_PRECISION_BITS);
        number_t seven = num_create_from_string("7");
        number_t x = num_div(pi_256, seven);
        number_t sin_x = num_sin(x);
        number_t cos_x = num_cos(x);
        number_t tan_x = num_tan(x);
        number_t sin_sq = num_mul(sin_x, sin_x);
        number_t cos_sq = num_mul(cos_x, cos_x);
        number_t trig_identity = num_add(sin_sq, cos_sq);
        number_t tan_identity = num_div(sin_x, cos_x);
        number_t exp_x = num_exp(x);
        number_t log_exp_x = num_log(exp_x);
        number_t sqrt_x = num_sqrt(x);
        number_t sqrt_identity = num_mul(sqrt_x, sqrt_x);
        number_t pi_2048 = num_const_prec(NUM_PI, PI_2048_BITS);
        int written;

        ASSERT_EQ_INT((int)num_get_prec_bits(pi_256), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(x), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(sin_x), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(cos_x), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(tan_x), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(exp_x), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(log_exp_x), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(sqrt_x), MATH_PRECISION_BITS);
        ASSERT_EQ_INT((int)num_get_prec_bits(pi_2048), PI_2048_BITS);

        assert_number_string_prefix("num_const_prec(NUM_PI, 256)",
                                    pi_256,
                                    "3.14159265358979323846264338327950288419716939937510");
        assert_number_string_prefix("num_const_prec(NUM_PI, 256) / 7",
                                    x,
                                    "0.44879895051282760549466334046850041202816705705358");
        assert_number_string_prefix("num_sin(NUM_PI / 7) at 256 bits",
                                    sin_x,
                                    "0.43388373911755812047576833284835875460999072778745");
        assert_number_string_prefix("num_cos(NUM_PI / 7) at 256 bits",
                                    cos_x,
                                    "0.90096886790241912623610231950744505116591916213185");
        assert_number_string_prefix("num_tan(NUM_PI / 7) at 256 bits",
                                    tan_x,
                                    "0.481574618807528644332162353056970575219078891");

        ASSERT_NUMBER_EQ(trig_identity, NUM_ONE);
        ASSERT_NUMBER_EQ(tan_x, tan_identity);
        ASSERT_NUMBER_EQ(sqrt_identity, x);
        assert_number_string_prefix("num_log(num_exp(NUM_PI / 7)) at 256 bits",
                                    log_exp_x,
                                    "0.448798950512827605494663340468500412028167057");

        written = num_sprintf(buf, sizeof(buf), "%.78n", exp_x);
        ASSERT_TRUE(written > 0);
        printf(C_WHITE C_BOLD "num_exp(NUM_PI / 7) at 256 bits" C_RESET "\n");
        printf("    %.78s\n\n", buf);
        ASSERT_TRUE(strstr(buf,
                           "1.5664296956520810256736029896701996511763171681836")
                    == buf);

        for (size_t i = 0u;
             i < sizeof(heegner_cases) / sizeof(heegner_cases[0]);
             ++i) {
            number_t heegner_x = num_create_from_string(heegner_cases[i].x);
            number_t heegner_sqrt;
            number_t heegner_arg;
            number_t heegner_exp;

            ASSERT_EQ_INT(num_set_prec_bits(&heegner_x, MATH_PRECISION_BITS),
                          0);
            heegner_sqrt = num_sqrt(heegner_x);
            heegner_arg = num_mul(pi_256, heegner_sqrt);
            heegner_exp = num_exp(heegner_arg);

            ASSERT_EQ_INT((int)num_get_prec_bits(heegner_sqrt), MATH_PRECISION_BITS);
            ASSERT_EQ_INT((int)num_get_prec_bits(heegner_arg), MATH_PRECISION_BITS);
            ASSERT_EQ_INT((int)num_get_prec_bits(heegner_exp), MATH_PRECISION_BITS);

            written = num_sprintf(buf, sizeof(buf), "%.78n", heegner_exp);
            ASSERT_TRUE(written > 0);
            printf(C_WHITE C_BOLD "num_exp(NUM_PI * num_sqrt(%s)) at 256 bits" C_RESET "\n",
                   heegner_cases[i].x);
            printf("    %s\n\n", buf);
            ASSERT_TRUE(strstr(buf, heegner_cases[i].prefix) == buf);
        }

        written = num_sprintf(buf, sizeof(buf), "%.617n", pi_2048);
        ASSERT_TRUE(written > 600);
        printf(C_WHITE C_BOLD "num_const_prec(NUM_PI, 2048)" C_RESET "\n");
        printf("    precision = %d bits (~%d digits)\n", PI_2048_BITS, PI_2048_DIGITS);
        printf("    %s\n\n", buf);
        ASSERT_TRUE(strstr(buf,
                           "3.14159265358979323846264338327950288419716939937510")
                    == buf);
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
        e_768 = num_const_prec(NUM_E, 768u);
        ln2_768 = num_const_prec(NUM_LN2, 768u);
        pi_768 = num_const_prec(NUM_PI, 768u);
        pi_2_768 = num_const_prec(NUM_PI_2, 768u);
        i_768 = num_const_prec(NUM_I, 768u);
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
        ASSERT_NUMBER_EQ(one_pow_one, two);
        assert_number_string("num_pow(2, -1)", one_pow_neg_one, "½");
        assert_number_string("num_pow(2, 2)", one_pow_two, "4");
        ASSERT_NUMBER_EQ(pow_half, sqrt_two);
        ASSERT_NUMBER_EQ(pow_quarter, sqrt_sqrt_two);
        ASSERT_NUMBER_EQ(pow_eighth, sqrt_sqrt_sqrt_two);
        ASSERT_NUMBER_EQ(complex_pow_two_exact, complex_pow_two_expected);
        ASSERT_NUMBER_EQ(complex_pow_two_qfloat, complex_pow_two_expected);
        ASSERT_NUMBER_EQ(complex_pow_two_text, complex_pow_two_expected);
        ASSERT_NUMBER_EQ(exp_half, sqrt_e);
        ASSERT_NUMBER_EQ(exp_quarter, sqrt_sqrt_e);
        ASSERT_NUMBER_EQ(exp_eighth, sqrt_sqrt_sqrt_e);
        ASSERT_NUMBER_EQ(exp_i_pi_2, NUM_I);
        ASSERT_NUMBER_EQ(log_one, NUM_ZERO);
        ASSERT_NUMBER_EQ(log_e, NUM_ONE);
        ASSERT_NUMBER_EQ(log_inv_e, NUM_NEG_ONE);
        ASSERT_NUMBER_EQ(log_two, ln2_768);
        ASSERT_NUMBER_EQ(log_half, neg_ln2);
        ASSERT_NUMBER_EQ(log_i, log_i_expected);
        ASSERT_NUMBER_EQ(log_neg_one, i_pi);
        ASSERT_NUMBER_EQ(log_neg_i, neg_i_pi_2);
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
