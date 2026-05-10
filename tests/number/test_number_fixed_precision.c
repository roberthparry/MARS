#include <stdio.h>

#include "test_number.h"

void run_number_fixed_precision_tests(void)
{
    printf(C_CYAN "Testing fixed-precision real and complex backends...\n" C_RESET);

    {
        number_t d = num_create_double(1.25);
        number_t d_neg = num_neg(d);
        number_t d_exp = num_exp(d);
        number_t qf_a = num_create_qfloat(QF_TWO);
        number_t qf_b = num_create_qfloat(QF_HALF);
        number_t qf_sum = num_add(qf_a, qf_b);
        number_t qf_sqrt = num_sqrt(qf_a);
        number_t qc_z = num_create_qcomplex(qc_from_string("1 + 2i"));
        number_t qc_conj = num_create_qcomplex(qc_from_string("1 - 2i"));
        number_t qc_prod = num_mul(qc_z, qc_conj);
        number_t qc_sqrt = num_sqrt(qc_z);
        number_t zero = num_create_string("0");
        number_t sin_zero = num_new();
        number_t cos_zero = num_new();
        number_t sinh_zero = num_new();
        number_t cosh_zero = num_new();

        assert_number_string("num_neg(double 1.25)", d_neg, "-1.25");
        ASSERT_TRUE(num_is_real(d));
        ASSERT_TRUE(num_is_real(d_exp));
        ASSERT_TRUE(!num_is_exact(d));
        ASSERT_EQ_INT((int)num_get_precision(d), 53);

        assert_number_string_prefix("num_add(qfloat 2, qfloat 1/2)", qf_sum, "2.5");
        ASSERT_TRUE(num_is_real(qf_sum));
        ASSERT_TRUE(num_is_real(qf_sqrt));
        ASSERT_EQ_INT((int)num_get_precision(qf_a), 106);
        ASSERT_EQ_INT((int)num_get_precision(qf_sum), 106);

        ASSERT_TRUE(num_is_real(qc_prod));
        ASSERT_TRUE(!num_is_real(qc_sqrt));
        ASSERT_EQ_INT((int)num_get_precision(qc_z), 106);
        ASSERT_EQ_INT(num_get_sign(d), 1);
        ASSERT_EQ_INT(num_get_exponent2(d), 0);

        ASSERT_EQ_INT(num_sincos(zero, &sin_zero, &cos_zero), 0);
        assert_number_string("num_sincos(0).sin", sin_zero, "0");
        assert_number_string("num_sincos(0).cos", cos_zero, "1");

        ASSERT_EQ_INT(num_sinhcosh(zero, &sinh_zero, &cosh_zero), 0);
        assert_number_string("num_sinhcosh(0).sinh", sinh_zero, "0");
        assert_number_string("num_sinhcosh(0).cosh", cosh_zero, "1");

        num_clear(&d);
        num_clear(&d_neg);
        num_clear(&d_exp);
        num_clear(&qf_a);
        num_clear(&qf_b);
        num_clear(&qf_sum);
        num_clear(&qf_sqrt);
        num_clear(&qc_z);
        num_clear(&qc_conj);
        num_clear(&qc_prod);
        num_clear(&qc_sqrt);
        num_clear(&zero);
        num_clear(&sin_zero);
        num_clear(&cos_zero);
        num_clear(&sinh_zero);
        num_clear(&cosh_zero);
    }
}
