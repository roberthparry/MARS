#include "test_number.h"
#include <string.h>

static void assert_bessel_y_close(number_t actual, number_t expected, const char *tolerance_text)
{
    NUM_SCOPE(scope);
    number_t tolerance = num_create_from_string(tolerance_text);
    ASSERT_TRUE(num_is_finite(actual));
    ASSERT_TRUE(num_lt(num_abs(num_sub(actual, expected)), tolerance));
}

void run_number_bessel_y_tests(void)
{
    NUM_SCOPE(scope);
    size_t saved_precision = num_get_default_prec_bits();
    ASSERT_EQ_INT(num_set_default_prec_bits(384u), 0);
    number_t z = num_create_from_string("2+i");
    number_t half = num_create_from_frac(1, 2);
    number_t pi = num_const_prec(NUM_PI, 384u);
    number_t factor = num_sqrt(num_div(NUM_TWO, num_mul(pi, z)));
    number_t positive = num_bessel_y(half, z);
    number_t negative = num_bessel_y(num_neg(half), z);
    assert_bessel_y_close(positive, num_neg(num_mul(factor, num_cos(z))), "1e-108");
    assert_bessel_y_close(negative, num_mul(factor, num_sin(z)), "1e-108");
    ASSERT_TRUE(num_get_effective_prec_bits(positive) >= 384u);
    ASSERT_TRUE(num_eq(z, num_create_from_string("2+i")));
    ASSERT_TRUE(num_get_default_prec_bits() == 384u);

    /* Y_0(i) = -2*K_0(1)/pi + i*I_0(1), an independent modified-cylinder identity. */
    number_t imaginary = num_create_from_string("i");
    number_t expected = num_add(num_neg(num_div(num_mul(NUM_TWO, num_bessel_k(NUM_ZERO, NUM_ONE)), pi)),
                                num_mul(imaginary, num_bessel_i(NUM_ZERO, NUM_ONE)));
    assert_bessel_y_close(num_bessel_y(NUM_ZERO, imaginary), expected, "1e-108");

    /* The exact half-order connection must remain reliable when the ordinary cosine residue dominates. */
    number_t tiny = num_create_from_string("1e-100");
    number_t tiny_reference = num_mul(num_sqrt(num_div(NUM_TWO, num_mul(pi, tiny))), num_sin(tiny));
    assert_bessel_y_close(num_div(num_bessel_y(num_neg(half), tiny), tiny_reference), NUM_ONE, "1e-108");

    number_t nu = num_create_from_string("0.25+0.375i");
    number_t value = num_bessel_y(nu, z);
    assert_bessel_y_close(num_bessel_y(num_conj(nu), num_conj(z)), num_conj(value), "1e-108");
    number_t neighbours = num_add(num_bessel_y(num_sub(nu, NUM_ONE), z), num_bessel_y(num_add(nu, NUM_ONE), z));
    assert_bessel_y_close(neighbours, num_mul(num_div(num_mul(NUM_TWO, nu), z), value), "1e-108");

    /* Exact complex inputs have no stored component precision. They must use the current default,
     * not the generic NUMBER_COMPLEX 106-bit fallback, for every shared cylinder evaluator. */
    number_t exact_order = num_add(num_create_from_frac(1, 4), num_div(imaginary, num_create_from_long(3)));
    number_t exact_z = num_add(NUM_TWO, imaginary);
    ASSERT_TRUE(num_is_exact(num_real_part(exact_order)) && num_is_exact(num_imag_part(exact_order)));
    ASSERT_TRUE(num_is_exact(num_real_part(exact_z)) && num_is_exact(num_imag_part(exact_z)));
    number_t exact_y = num_bessel_y(exact_order, exact_z);
    ASSERT_TRUE(num_get_effective_prec_bits(exact_y) >= 384u);
    number_t y_neighbours = num_add(num_bessel_y(num_sub(exact_order, NUM_ONE), exact_z),
                                    num_bessel_y(num_add(exact_order, NUM_ONE), exact_z));
    number_t recurrence_factor = num_div(num_mul(NUM_TWO, exact_order), exact_z);
    assert_bessel_y_close(y_neighbours, num_mul(recurrence_factor, exact_y), "1e-108");
    number_t exact_i = num_bessel_i(exact_order, exact_z);
    ASSERT_TRUE(num_get_effective_prec_bits(exact_i) >= 384u);
    number_t i_neighbours = num_sub(num_bessel_i(num_sub(exact_order, NUM_ONE), exact_z),
                                    num_bessel_i(num_add(exact_order, NUM_ONE), exact_z));
    assert_bessel_y_close(i_neighbours, num_mul(recurrence_factor, exact_i), "1e-108");
    ASSERT_TRUE(num_get_effective_prec_bits(num_struve_h(exact_order, exact_z)) >= 384u);
    ASSERT_TRUE(num_get_effective_prec_bits(num_struve_l(exact_order, exact_z)) >= 384u);

    /* The 10^140 denominator needs a larger rational budget than the surrounding 384-bit checks. */
    ASSERT_EQ_INT(num_set_default_prec_digits(180u), 0);
    number_t perturbation = num_div(NUM_ONE, num_pow_int(num_create_from_long(10), 140));
    ASSERT_TRUE(num_is_exact(perturbation));
    for (long n = -2; n <= 2; ++n) {
        number_t order = num_create_from_long(n);
        number_t above = num_add(order, perturbation);
        number_t below_order = num_sub(order, perturbation);
        number_t off_axis = num_add(order, num_mul(imaginary, perturbation));
        ASSERT_TRUE(num_is_exact(above) && num_is_exact(below_order));
        ASSERT_TRUE(num_eq(num_sub(above, order), perturbation));
        ASSERT_TRUE(num_eq(num_sub(order, below_order), perturbation));
        ASSERT_TRUE(!num_is_zero(num_sub(off_axis, order)));
        number_t integer_value = num_bessel_y(order, z);
        assert_bessel_y_close(num_bessel_y(above, z), integer_value, "1e-108");
        assert_bessel_y_close(num_bessel_y(below_order, z), integer_value, "1e-108");
        assert_bessel_y_close(num_bessel_y(off_axis, z), integer_value, "1e-108");
        number_t on_cut = num_bessel_y(order, num_neg(NUM_TWO));
        number_t cut_reference = num_add(num_bessel_y(order, NUM_TWO),
                                         num_mul(num_mul(NUM_TWO, imaginary), num_bessel_j(order, NUM_TWO)));
        if (n % 2 != 0)
            cut_reference = num_neg(cut_reference);
        assert_bessel_y_close(on_cut, cut_reference, "1e-108");
        number_t below = num_sub(num_neg(NUM_TWO), num_create_from_string("1e-120i"));
        assert_bessel_y_close(num_bessel_y(order, below), num_conj(on_cut), "1e-108");
    }
    ASSERT_EQ_INT(num_set_default_prec_bits(384u), 0);

    for (long n = 0; n <= 4; ++n) {
        number_t order = num_neg(num_add(num_create_from_long(n), half));
        ASSERT_TRUE(num_is_zero(num_bessel_y(order, NUM_ZERO)));
    }
    ASSERT_TRUE(num_is_nan(num_bessel_y(NUM_ZERO, NUM_ZERO)));
    ASSERT_TRUE(num_is_nan(num_bessel_y(NUM_ONE, NUM_ZERO)));
    ASSERT_TRUE(num_is_nan(num_bessel_y(NUM_NAN, NUM_ONE)));
    ASSERT_TRUE(num_is_nan(num_bessel_y(NUM_ZERO, NUM_INF)));
    ASSERT_TRUE(num_is_nan(num_bessel_y(num_create_from_long(1001), NUM_ONE)));
    ASSERT_TRUE(num_is_nan(num_bessel_y(half, num_create_from_long(1001))));
    ASSERT_TRUE(num_is_finite(num_bessel_y(NUM_ZERO, num_create_from_long(1001))));

    number_t one_reference = num_bessel_y(NUM_ZERO, NUM_ONE);
    number_t qf_value = num_create_from_qfloat(qf_bessel_y(QF_ZERO, QF_ONE));
    assert_bessel_y_close(qf_value, one_reference, "1e-30");
    ASSERT_TRUE(qf_isnan(qf_bessel_y(QF_ZERO, qf_neg(QF_ONE))));
    ASSERT_TRUE(qf_isnan(qf_bessel_y(QF_ZERO, QF_ZERO)));
    ASSERT_TRUE(qf_eq(qf_bessel_y(qf_neg(QF_HALF), QF_ZERO), QF_ZERO));

    qcomplex_t qc_value = qc_bessel_y(qc_make(QF_HALF, QF_ZERO), qc_make(QF_TWO, QF_ONE));
    assert_bessel_y_close(num_create_from_qcomplex(qc_value), positive, "1e-29");
    qcomplex_t qc_cut = qc_bessel_y(QC_ZERO, qc_make(qf_neg(QF_ONE), QF_ZERO));
    assert_bessel_y_close(num_create_from_qcomplex(qc_cut), num_bessel_y(NUM_ZERO, num_neg(NUM_ONE)), "1e-29");
    ASSERT_TRUE(num_is_zero(num_create_from_qcomplex(qc_bessel_y(qc_make(qf_neg(QF_HALF), QF_ZERO), QC_ZERO))));

    /* Fixed inputs retain their own precision, independent of a higher default. */
    number_t fixed = num_bessel_y(num_create_from_qfloat(QF_ZERO), num_create_from_qfloat(QF_ONE));
    ASSERT_TRUE(num_get_effective_prec_bits(fixed) == num_get_effective_prec_bits(num_create_from_qfloat(QF_ONE)));
    ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
}

void run_number_bessel_y_readme_tests(void)
{
    NUM_SCOPE(scope);
    /* README examples: docs/number.md, docs/qfloat.md and docs/qcomplex.md. */
    number_t expected = num_create_from_string("0.0882569642156769579829267660235151628278");
    number_t number_value = num_bessel_y(NUM_ZERO, NUM_ONE);
    qfloat_t real_value = qf_bessel_y(QF_ZERO, QF_ONE);
    qcomplex_t complex_value = qc_bessel_y(QC_ZERO, QC_ONE);
    assert_bessel_y_close(number_value, expected, "1e-30");
    assert_bessel_y_close(num_create_from_qfloat(real_value), expected, "1e-30");
    assert_bessel_y_close(num_create_from_qcomplex(complex_value), expected, "1e-30");
    ASSERT_TRUE(qf_eq(qc_imag(complex_value), QF_ZERO));
    char output[96];
    snprintf(output, sizeof(output), "Y_0(1) = %.15f", num_to_double(number_value));
    ASSERT_TRUE(strcmp(output, "Y_0(1) = 0.088256964215677") == 0);
    printf("README number Bessel Y\n%s\n", output);
    snprintf(output, sizeof(output), "Y_0(1) = %.15f", qf_to_double(real_value));
    ASSERT_TRUE(strcmp(output, "Y_0(1) = 0.088256964215677") == 0);
    printf("README qfloat Bessel Y\n%s\n", output);
    snprintf(output, sizeof(output), "Y_0(1) = %.15f + %.0fi",
             qf_to_double(qc_real(complex_value)), qf_to_double(qc_imag(complex_value)));
    ASSERT_TRUE(strcmp(output, "Y_0(1) = 0.088256964215677 + 0i") == 0);
    printf("README qcomplex Bessel Y\n%s\n", output);
}
