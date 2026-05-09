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

        default_real = num_create_mfloat(base_real);
        bits_real = num_create_mfloat_prec(base_real, 512u);
        digits_real = num_create_mfloat_digits(base_real, 50u);

        default_complex = num_create_mcomplex(base_complex);
        bits_complex = num_create_mcomplex_prec(base_complex, 384u);
        digits_complex = num_create_mcomplex_digits(base_complex, 40u);

        clone_real = num_clone(default_real);
        log_real = num_log(default_real);
        sqrt_complex = num_sqrt(default_complex);

        ASSERT_EQ_INT((int)num_get_precision(default_real), 1024);
        ASSERT_EQ_INT((int)num_get_precision(bits_real), 512);
        ASSERT_EQ_INT((int)num_get_precision(digits_real), 167);
        ASSERT_EQ_INT((int)num_get_precision(default_complex), 1024);
        ASSERT_EQ_INT((int)num_get_precision(bits_complex), 384);
        ASSERT_EQ_INT((int)num_get_precision(digits_complex), 133);

        ASSERT_TRUE(num_is_real(default_real));
        ASSERT_TRUE(!num_is_real(default_complex));
        ASSERT_TRUE(num_eq(default_real, clone_real));
        ASSERT_TRUE(num_is_real(log_real));
        ASSERT_TRUE(!num_is_real(sqrt_complex));

        ASSERT_EQ_INT(num_set_precision(&clone_real, 256u), 0);
        ASSERT_EQ_INT((int)num_get_precision(default_real), 1024);
        ASSERT_EQ_INT((int)num_get_precision(clone_real), 256);

        num_clear(&default_real);
        num_clear(&bits_real);
        num_clear(&digits_real);
        num_clear(&default_complex);
        num_clear(&bits_complex);
        num_clear(&digits_complex);
        num_clear(&clone_real);
        num_clear(&log_real);
        num_clear(&sqrt_complex);

        mf_free(base_real);
        mc_free(base_complex);
    }
}
