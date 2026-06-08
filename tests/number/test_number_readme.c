#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_number.h"

static void number_readme_rational_basic(void)
{
    number_t a = num_create_from_frac(2, 3);
    number_t b = num_create_from_string("5/4");
    number_t product = num_mul(a, b);
    number_t expected = num_create_from_frac(5, 6);
    string_t *text = num_to_string(product);

    if (!text) {
        test_mark_failure(__FILE__, __LINE__,
                          "failed to format rational README example");
    } else {
        printf("(2/3) * (5/4) = %s\n", string_c_str(text));
        ASSERT_TRUE(num_eq(product, expected));
    }

    string_free(text);
    num_destroy(&a);
    num_destroy(&b);
    num_destroy(&product);
    num_destroy(&expected);
}

static void number_readme_binomial_cardinality(void)
{
    number_t n = num_create_from_long(52);
    number_t k = num_create_from_long(5);
    number_t c = num_binomial(n, k);
    number_t expected = num_create_from_long(2598960);
    string_t *text = num_to_string(c);

    if (!text) {
        test_mark_failure(__FILE__, __LINE__,
                          "failed to format binomial README example");
    } else {
        printf("C(52, 5) = %s\n", string_c_str(text));
        ASSERT_TRUE(num_eq(c, expected));
    }

    string_free(text);
    num_destroy(&n);
    num_destroy(&k);
    num_destroy(&c);
    num_destroy(&expected);
}

void run_number_readme_mersenne_prime_search(void)
{
    static const unsigned expected_exponents[] = {
        2, 3, 5, 7,
        13, 17, 19, 31,
        61, 89, 107, 127,
        521, 607, 1279, 2203,
        2281, 3217, 4253, 4423
    };
    number_t two = num_create_from_long(2);
    number_t one = num_create_from_long(1);
    unsigned found = 0u;

    for (unsigned p = 2u; p <= 4423u; ++p) {
        number_t exponent = num_create_from_long((long)p);

        if (num_is_prime(exponent)) {
            number_t mersenne_base = num_pow_int(two, (int)p);
            number_t mersenne = num_sub(mersenne_base, one);

            if (num_is_prime(mersenne)) {
                ASSERT_TRUE(found < (sizeof(expected_exponents) /
                                     sizeof(expected_exponents[0])));
                ASSERT_EQ_INT((int)p, (int)expected_exponents[found]);
                if ((found % 4u) == 3u)
                    printf("M_%-4u is prime\n", p);
                else
                    printf("M_%-4u is prime    ", p);
                found++;
            }

            num_destroy(&mersenne);
            num_destroy(&mersenne_base);
        }

        num_destroy(&exponent);
    }

    ASSERT_EQ_INT((int)found,
                  (int)(sizeof(expected_exponents) /
                        sizeof(expected_exponents[0])));

    num_destroy(&two);
    num_destroy(&one);
}

static void number_readme_multiprecision_example(void)
{
    size_t saved_precision = num_get_default_prec_bits();
    const char *gamma_prefix =
        "1.19929782941531928552681533588795691209235255849";
    const char *lgamma_prefix =
        "0.18173624337757203797862933229995978550118791690";
    number_t x;
    number_t y;
    number_t gamma_x;
    number_t lgamma_y;
    char buf[256];

    ASSERT_EQ_INT(num_set_default_prec_bits(256u), 0);
    x = num_create_from_string("2.345");
    y = num_create_from_string("2.345");
    gamma_x = num_gamma(x);
    lgamma_y = num_lgamma(y);

    ASSERT_TRUE(num_sprintf(buf, sizeof(buf), "%.77n", gamma_x) > 0);
    printf("gamma(2.345)  = %s\n", buf);
    ASSERT_TRUE(strncmp(buf, gamma_prefix, strlen(gamma_prefix)) == 0);

    ASSERT_TRUE(num_sprintf(buf, sizeof(buf), "%.77n", lgamma_y) > 0);
    printf("lgamma(2.345) = %s\n", buf);
    ASSERT_TRUE(strncmp(buf, lgamma_prefix, strlen(lgamma_prefix)) == 0);

    num_destroy(&x);
    num_destroy(&y);
    num_destroy(&gamma_x);
    num_destroy(&lgamma_y);
    ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
}

static void number_readme_complex_example(void)
{
    size_t saved_digits = num_get_default_prec_digits();
    number_t z;
    number_t exp_z;
    char buf[256];

    ASSERT_EQ_INT(num_set_default_prec_digits(50u), 0);
    z = num_create_from_string("1 + 1i");
    exp_z = num_exp(z);

    ASSERT_TRUE(!num_is_real(exp_z));
    ASSERT_TRUE(num_sprintf(buf, sizeof(buf), "%n", exp_z) > 0);
    printf("exp(1 + i) = %s\n", buf);

    num_destroy(&z);
    num_destroy(&exp_z);
    ASSERT_EQ_INT(num_set_default_prec_digits(saved_digits), 0);
}

void run_number_readme_example_tests(void)
{
    number_readme_rational_basic();
    number_readme_binomial_cardinality();
    number_readme_multiprecision_example();
    number_readme_complex_example();
}
