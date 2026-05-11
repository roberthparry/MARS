#include <stdio.h>

#include "test_number.h"

void run_number_public_api_tests(void)
{
    printf(C_CYAN "Testing number_t as a standalone public API...\n" C_RESET);

    {
        number_t a = num_create_from_string("2");
        number_t b = num_create_from_string("3");
        number_t frac = num_create_from_string("5/6");
        number_t dec = num_create_from_string("1.25");
        number_t z = num_create_from_string("1 + 2i");

        number_t sum = num_add(a, b);
        number_t quot = num_div(a, b);
        number_t product = num_mul(sum, frac);
        number_t logged = num_log(dec);
        number_t rooted = num_sqrt(z);
        number_t constant = NUM_PI;
        number_t i_constant = NUM_I;
        number_t cloned = num_clone(product);
        number_t pi_cloned = num_clone(constant);
        number_t i_cloned = num_clone(i_constant);

        assert_number_string("num_add(\"2\", \"3\")", sum, "5");
        assert_number_string("num_div(\"2\", \"3\")", quot, "2/3");
        assert_number_string("num_mul(\"5\", \"5/6\")", product, "25/6");
        assert_number_string("num_clone(\"25/6\")", cloned, "25/6");
        assert_number_string_prefix("NUM_PI", constant, "3.14159");
        assert_number_string_prefix("num_clone(NUM_PI)", pi_cloned, "3.14159");
        assert_number_string("num_clone(NUM_I)", i_cloned, "0 + 1i");

        ASSERT_TRUE(num_is_real(a));
        ASSERT_TRUE(num_is_real(frac));
        ASSERT_TRUE(num_is_real(dec));
        ASSERT_TRUE(!num_is_real(z));
        ASSERT_TRUE(num_is_real(logged));
        ASSERT_TRUE(!num_is_real(rooted));
        ASSERT_TRUE(num_is_exact(a));
        ASSERT_TRUE(num_is_exact(frac));
        ASSERT_TRUE(!num_is_exact(dec));
        ASSERT_TRUE(num_eq(product, cloned));

        num_destroy(&a);
        num_destroy(&b);
        num_destroy(&frac);
        num_destroy(&dec);
        num_destroy(&z);
        num_destroy(&sum);
        num_destroy(&quot);
        num_destroy(&product);
        num_destroy(&logged);
        num_destroy(&rooted);
        num_destroy(&constant);
        num_destroy(&i_constant);
        num_destroy(&cloned);
        num_destroy(&pi_cloned);
        num_destroy(&i_cloned);
    }
}
