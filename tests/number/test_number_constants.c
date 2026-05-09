#include <stdio.h>

#include "test_number.h"

void run_number_constant_tests(void)
{
    printf(C_CYAN "Testing named constants and decimal powers...\n" C_RESET);

    {
        number_t pi = num_pi();
        number_t e = num_e();
        number_t gamma = num_euler_mascheroni();
        number_t million = num_pow10(6);
        number_t huge = num_max();

        assert_number_string_prefix("num_pi()", pi, "3.14159");
        assert_number_string_prefix("num_e()", e, "2.71828");
        assert_number_string_prefix("num_euler_mascheroni()", gamma, "0.57721");
        assert_number_string("num_pow10(6)", million, "1000000");

        ASSERT_TRUE(num_is_real(pi));
        ASSERT_TRUE(num_is_real(e));
        ASSERT_TRUE(num_is_real(gamma));
        ASSERT_TRUE(!num_is_exact(pi));
        ASSERT_TRUE(!num_is_exact(e));
        ASSERT_TRUE(!num_is_exact(gamma));
        ASSERT_TRUE(num_get_precision(pi) > 0u);
        ASSERT_EQ_INT((int)num_get_precision(huge), 1024);

        num_clear(&pi);
        num_clear(&e);
        num_clear(&gamma);
        num_clear(&million);
        num_clear(&huge);
    }
}
