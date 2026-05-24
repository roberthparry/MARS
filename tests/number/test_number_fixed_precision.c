#include <complex.h>
#include <stdio.h>

#include "test_number.h"

void run_number_fixed_precision_tests(void)
{
    printf(C_CYAN "Testing fixed-precision real and complex backends...\n" C_RESET);

    {
        number_t d = num_create_from_double(1.25);
        number_t d_neg = num_neg(d);
        number_t d_exp = num_exp(d);
        number_t cd_z = num_create_from_cdouble(1.0 + 2.0 * I);
        number_t cd_conj = num_create_from_cdouble(1.0 - 2.0 * I);
        number_t cd_prod = num_mul(cd_z, cd_conj);
        number_t cd_prod_real = num_real_part(cd_prod);
        number_t cd_prod_imag = num_imag_part(cd_prod);
        number_t d_neg_one = num_create_from_double(-1.0);
        number_t cd_sqrt_neg = num_sqrt(d_neg_one);
        number_t cd_sqrt_neg_real = num_real_part(cd_sqrt_neg);
        number_t cd_sqrt_neg_imag = num_imag_part(cd_sqrt_neg);
        number_t cd_set = num_create_from_double(0.0);
        number_t qf_a = num_create_from_qfloat(QF_TWO);
        number_t qf_b = num_create_from_qfloat(QF_HALF);
        number_t qf_sum = num_add(qf_a, qf_b);
        number_t qf_sqrt = num_sqrt(qf_a);
        number_t qc_z = num_create_from_qcomplex(qc_from_string("1 + 2i"));
        number_t qc_conj = num_create_from_qcomplex(qc_from_string("1 - 2i"));
        number_t qc_prod = num_mul(qc_z, qc_conj);
        number_t qc_sqrt = num_sqrt(qc_z);
        number_t qc_i = num_create_from_qcomplex(qc_from_string("0 + 1i"));
        number_t sin_i = num_sin(qc_i);
        number_t cos_i = num_cos(qc_i);
        number_t tan_i = num_tan(qc_i);
        number_t sinh_i = num_sinh(qc_i);
        number_t cosh_i = num_cosh(qc_i);
        number_t tanh_i = num_tanh(qc_i);
        number_t qc_w0_arg = num_create_from_qcomplex(qc_from_string("0.2 + 0.1i"));
        number_t qc_w0 = num_lambert_w0(qc_w0_arg);
        number_t expected_w0 = num_create_from_qcomplex(qc_productlog(qc_from_string("0.2 + 0.1i")));
        number_t expected_sin_i = num_create_from_qcomplex(qc_sin(qc_from_string("0 + 1i")));
        number_t expected_cos_i = num_create_from_qcomplex(qc_cos(qc_from_string("0 + 1i")));
        number_t expected_tan_i = num_create_from_qcomplex(qc_tan(qc_from_string("0 + 1i")));
        number_t expected_sinh_i = num_create_from_qcomplex(qc_sinh(qc_from_string("0 + 1i")));
        number_t expected_cosh_i = num_create_from_qcomplex(qc_cosh(qc_from_string("0 + 1i")));
        number_t expected_tanh_i = num_create_from_qcomplex(qc_tanh(qc_from_string("0 + 1i")));
        number_t zero = num_create_from_string("0");
        number_t sin_zero = NUM_ZERO;
        number_t cos_zero = NUM_ZERO;
        number_t sinh_zero = NUM_ZERO;
        number_t cosh_zero = NUM_ZERO;

        assert_number_string("num_neg(double 1.25)", d_neg, "-1.25");
        ASSERT_TRUE(num_is_real(d));
        ASSERT_TRUE(num_is_real(d_exp));
        ASSERT_TRUE(!num_is_exact(d));
        ASSERT_EQ_INT((int)num_get_prec_bits(d), 53);
        ASSERT_EQ_INT(num_set_cdouble(&cd_set, 3.0 + 4.0 * I), 0);
        ASSERT_TRUE(!num_is_real(cd_z));
        ASSERT_TRUE(num_is_real(cd_prod));
        ASSERT_TRUE(!num_is_real(cd_sqrt_neg));
        ASSERT_EQ_INT((int)num_get_prec_bits(cd_z), 53);
        ASSERT_EQ_INT((int)num_get_prec_bits(cd_sqrt_neg), 53);
        assert_number_string("num_mul(cdouble 1+2i, 1-2i).real", cd_prod_real, "5");
        assert_number_string("num_mul(cdouble 1+2i, 1-2i).imag", cd_prod_imag, "0");
        assert_number_string("num_sqrt(double -1).real", cd_sqrt_neg_real, "0");
        assert_number_string("num_sqrt(double -1).imag", cd_sqrt_neg_imag, "1");

        assert_number_string_prefix("num_add(qfloat 2, qfloat 1/2)", qf_sum, "2.5");
        ASSERT_TRUE(num_is_real(qf_sum));
        ASSERT_TRUE(num_is_real(qf_sqrt));
        ASSERT_EQ_INT((int)num_get_prec_bits(qf_a), 106);
        ASSERT_EQ_INT((int)num_get_prec_bits(qf_sum), 106);

        ASSERT_TRUE(num_is_real(qc_prod));
        ASSERT_TRUE(!num_is_real(qc_sqrt));
        ASSERT_EQ_INT((int)num_get_prec_bits(qc_z), 106);
        ASSERT_EQ_INT(num_get_sign(d), 1);
        ASSERT_EQ_INT(num_get_exponent2(d), 0);

        ASSERT_NUMBER_EQ(sin_i, expected_sin_i);
        ASSERT_NUMBER_EQ(cos_i, expected_cos_i);
        ASSERT_NUMBER_EQ(tan_i, expected_tan_i);
        ASSERT_NUMBER_EQ(sinh_i, expected_sinh_i);
        ASSERT_NUMBER_EQ(cosh_i, expected_cosh_i);
        ASSERT_NUMBER_EQ(tanh_i, expected_tanh_i);
        ASSERT_NUMBER_EQ(qc_w0, expected_w0);

        ASSERT_EQ_INT(num_sincos(zero, &sin_zero, &cos_zero), 0);
        assert_number_string("num_sincos(0).sin", sin_zero, "0");
        assert_number_string("num_sincos(0).cos", cos_zero, "1");

        ASSERT_EQ_INT(num_sinhcosh(zero, &sinh_zero, &cosh_zero), 0);
        assert_number_string("num_sinhcosh(0).sinh", sinh_zero, "0");
        assert_number_string("num_sinhcosh(0).cosh", cosh_zero, "1");

        num_destroy(&d);
        num_destroy(&d_neg);
        num_destroy(&d_exp);
        num_destroy(&cd_z);
        num_destroy(&cd_conj);
        num_destroy(&cd_prod);
        num_destroy(&cd_prod_real);
        num_destroy(&cd_prod_imag);
        num_destroy(&d_neg_one);
        num_destroy(&cd_sqrt_neg);
        num_destroy(&cd_sqrt_neg_real);
        num_destroy(&cd_sqrt_neg_imag);
        num_destroy(&cd_set);
        num_destroy(&qf_a);
        num_destroy(&qf_b);
        num_destroy(&qf_sum);
        num_destroy(&qf_sqrt);
        num_destroy(&qc_z);
        num_destroy(&qc_conj);
        num_destroy(&qc_prod);
        num_destroy(&qc_sqrt);
        num_destroy(&qc_i);
        num_destroy(&sin_i);
        num_destroy(&cos_i);
        num_destroy(&tan_i);
        num_destroy(&sinh_i);
        num_destroy(&cosh_i);
        num_destroy(&tanh_i);
        num_destroy(&qc_w0_arg);
        num_destroy(&qc_w0);
        num_destroy(&expected_w0);
        num_destroy(&expected_sin_i);
        num_destroy(&expected_cos_i);
        num_destroy(&expected_tan_i);
        num_destroy(&expected_sinh_i);
        num_destroy(&expected_cosh_i);
        num_destroy(&expected_tanh_i);
        num_destroy(&zero);
        num_destroy(&sin_zero);
        num_destroy(&cos_zero);
        num_destroy(&sinh_zero);
        num_destroy(&cosh_zero);
    }
}
