/**
 * @file test_number_bessel_y.c
 * @brief Bessel Y correctness and ownership regressions.
 *
 * Checks scalar values, complex paths and result lifetime across numeric scopes. The documented examples exercise
 * the same public special-function API.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

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
    num_destroy(&negative);
    ASSERT_TRUE(num_get_effective_prec_bits(positive) >= 384u);
    ASSERT_TRUE(num_eq(z, num_create_from_string("2+i")));
    ASSERT_TRUE(num_get_default_prec_bits() == 384u);

    /* Y_0(i) = -2*K_0(1)/pi + i*I_0(1), an independent modified-cylinder identity. */
    number_t imaginary = num_create_from_string("i");
    number_t k0 = num_bessel_k(NUM_ZERO, NUM_ONE);
    number_t expected = num_add(num_neg(num_div(num_mul(NUM_TWO, k0), pi)),
                                num_mul(imaginary, num_bessel_i(NUM_ZERO, NUM_ONE)));
    number_t imaginary_value = num_bessel_y(NUM_ZERO, imaginary);
    assert_bessel_y_close(imaginary_value, expected, "1e-108");
    num_destroy(&imaginary_value);
    num_destroy(&k0);

    /* The exact half-order connection must remain reliable when the ordinary cosine residue dominates. */
    number_t tiny = num_create_from_string("1e-100");
    number_t tiny_reference = num_mul(num_sqrt(num_div(NUM_TWO, num_mul(pi, tiny))), num_sin(tiny));
    number_t tiny_value = num_bessel_y(num_neg(half), tiny);
    assert_bessel_y_close(num_div(tiny_value, tiny_reference), NUM_ONE, "1e-108");
    num_destroy(&tiny_value);

    number_t nu = num_create_from_string("0.25+0.375i");
    number_t value = num_bessel_y(nu, z);
    number_t conjugate_value = num_bessel_y(num_conj(nu), num_conj(z));
    assert_bessel_y_close(conjugate_value, num_conj(value), "1e-108");
    num_destroy(&conjugate_value);
    number_t lower_value = num_bessel_y(num_sub(nu, NUM_ONE), z);
    number_t upper_value = num_bessel_y(num_add(nu, NUM_ONE), z);
    number_t neighbours = num_add(lower_value, upper_value);
    assert_bessel_y_close(neighbours, num_mul(num_div(num_mul(NUM_TWO, nu), z), value), "1e-108");
    num_destroy(&upper_value);
    num_destroy(&lower_value);
    num_destroy(&value);

    /* Exact complex inputs have no stored component precision. They must use the current default,
     * not the generic NUMBER_COMPLEX 106-bit fallback, for every shared cylinder evaluator. */
    number_t exact_order = num_add(num_create_from_frac(1, 4), num_div(imaginary, num_create_from_long(3)));
    number_t exact_z = num_add(NUM_TWO, imaginary);
    ASSERT_TRUE(num_is_exact(num_real_part(exact_order)) && num_is_exact(num_imag_part(exact_order)));
    ASSERT_TRUE(num_is_exact(num_real_part(exact_z)) && num_is_exact(num_imag_part(exact_z)));
    number_t exact_y = num_bessel_y(exact_order, exact_z);
    ASSERT_TRUE(num_get_effective_prec_bits(exact_y) >= 384u);
    number_t exact_lower = num_bessel_y(num_sub(exact_order, NUM_ONE), exact_z);
    number_t exact_upper = num_bessel_y(num_add(exact_order, NUM_ONE), exact_z);
    number_t y_neighbours = num_add(exact_lower, exact_upper);
    number_t recurrence_factor = num_div(num_mul(NUM_TWO, exact_order), exact_z);
    assert_bessel_y_close(y_neighbours, num_mul(recurrence_factor, exact_y), "1e-108");
    num_destroy(&exact_upper);
    num_destroy(&exact_lower);
    num_destroy(&exact_y);
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
        number_t above_value = num_bessel_y(above, z);
        number_t below_value = num_bessel_y(below_order, z);
        number_t off_axis_value = num_bessel_y(off_axis, z);
        assert_bessel_y_close(above_value, integer_value, "1e-108");
        assert_bessel_y_close(below_value, integer_value, "1e-108");
        assert_bessel_y_close(off_axis_value, integer_value, "1e-108");
        num_destroy(&off_axis_value);
        num_destroy(&below_value);
        num_destroy(&above_value);
        num_destroy(&integer_value);
        number_t on_cut = num_bessel_y(order, num_neg(NUM_TWO));
        number_t positive_value = num_bessel_y(order, NUM_TWO);
        number_t cut_reference = num_add(positive_value,
                                         num_mul(num_mul(NUM_TWO, imaginary), num_bessel_j(order, NUM_TWO)));
        if (n % 2 != 0)
            cut_reference = num_neg(cut_reference);
        assert_bessel_y_close(on_cut, cut_reference, "1e-108");
        number_t below = num_sub(num_neg(NUM_TWO), num_create_from_string("1e-120i"));
        number_t below_cut = num_bessel_y(order, below);
        assert_bessel_y_close(below_cut, num_conj(on_cut), "1e-108");
        num_destroy(&below_cut);
        num_destroy(&positive_value);
        num_destroy(&on_cut);
    }
    ASSERT_EQ_INT(num_set_default_prec_bits(384u), 0);

    for (long n = 0; n <= 4; ++n) {
        number_t order = num_neg(num_add(num_create_from_long(n), half));
        number_t zero_value = num_bessel_y(order, NUM_ZERO);
        ASSERT_TRUE(num_is_zero(zero_value));
        num_destroy(&zero_value);
    }
    number_t rejected[] = {
        num_bessel_y(NUM_ZERO, NUM_ZERO),
        num_bessel_y(NUM_ONE, NUM_ZERO),
        num_bessel_y(NUM_NAN, NUM_ONE),
        num_bessel_y(NUM_ZERO, NUM_INF),
        num_bessel_y(num_create_from_long(1001), NUM_ONE),
        num_bessel_y(half, num_create_from_long(1001)),
    };
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        ASSERT_TRUE(num_is_nan(rejected[i]));
        num_destroy(&rejected[i]);
    }
    number_t large_value = num_bessel_y(NUM_ZERO, num_create_from_long(1001));
    ASSERT_TRUE(num_is_finite(large_value));
    num_destroy(&large_value);

    number_t one_reference = num_bessel_y(NUM_ZERO, NUM_ONE);
    number_t qf_value = num_create_from_qfloat(qf_bessel_y(QF_ZERO, QF_ONE));
    assert_bessel_y_close(qf_value, one_reference, "1e-30");
    num_destroy(&one_reference);
    ASSERT_TRUE(qf_isnan(qf_bessel_y(QF_ZERO, qf_neg(QF_ONE))));
    ASSERT_TRUE(qf_isnan(qf_bessel_y(QF_ZERO, QF_ZERO)));
    ASSERT_TRUE(qf_eq(qf_bessel_y(qf_neg(QF_HALF), QF_ZERO), QF_ZERO));

    qcomplex_t qc_value = qc_bessel_y(qc_make(QF_HALF, QF_ZERO), qc_make(QF_TWO, QF_ONE));
    assert_bessel_y_close(num_create_from_qcomplex(qc_value), positive, "1e-29");
    num_destroy(&positive);
    qcomplex_t qc_cut = qc_bessel_y(QC_ZERO, qc_make(qf_neg(QF_ONE), QF_ZERO));
    number_t cut_value = num_bessel_y(NUM_ZERO, num_neg(NUM_ONE));
    assert_bessel_y_close(num_create_from_qcomplex(qc_cut), cut_value, "1e-29");
    num_destroy(&cut_value);
    ASSERT_TRUE(num_is_zero(num_create_from_qcomplex(qc_bessel_y(qc_make(qf_neg(QF_HALF), QF_ZERO), QC_ZERO))));

    /* Fixed inputs retain their own precision, independent of a higher default. */
    number_t fixed = num_bessel_y(num_create_from_qfloat(QF_ZERO), num_create_from_qfloat(QF_ONE));
    ASSERT_TRUE(num_get_effective_prec_bits(fixed) == num_get_effective_prec_bits(num_create_from_qfloat(QF_ONE)));
    num_destroy(&fixed);
    ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
}

/* Keep the fixed-width results beyond both the wrapper's scope and its caller's scope. */
void run_number_bessel_y_qfloat_ownership_tests(void)
{
    qfloat_t positive, zero, negative_argument, singular, nan_order, nan_argument, infinite_argument;

    {
        NUM_SCOPE(scope);
        positive = qf_bessel_y(QF_ZERO, QF_ONE);
        zero = qf_bessel_y(qf_neg(QF_HALF), QF_ZERO);
        negative_argument = qf_bessel_y(QF_ZERO, qf_neg(QF_ONE));
        singular = qf_bessel_y(QF_ZERO, QF_ZERO);
        nan_order = qf_bessel_y(QF_NAN, QF_ONE);
        nan_argument = qf_bessel_y(QF_ZERO, QF_NAN);
        infinite_argument = qf_bessel_y(QF_ZERO, QF_INF);
    }
    {
        NUM_SCOPE(scope);
        number_t expected = num_create_from_string("0.0882569642156769579829267660235151628278");

        assert_bessel_y_close(num_create_from_qfloat(positive), expected, "1e-30");
        ASSERT_TRUE(qf_eq(zero, QF_ZERO));
        ASSERT_TRUE(qf_isnan(negative_argument));
        ASSERT_TRUE(qf_isnan(singular));
        ASSERT_TRUE(qf_isnan(nan_order));
        ASSERT_TRUE(qf_isnan(nan_argument));
        ASSERT_TRUE(qf_isnan(infinite_argument));
    }
}

/* Exercise real, complex, branch-cut and rejected results with explicit wrapper cleanup. */
void run_number_bessel_y_qcomplex_ownership_tests(void)
{
    size_t saved_precision = num_get_default_prec_bits();
    qcomplex_t positive, complex_value, cut, zero, singular, nan_order, nan_argument, infinite_argument;

    ASSERT_EQ_INT(num_set_default_prec_bits(256u), 0);
    {
        NUM_SCOPE(scope);
        positive = qc_bessel_y(QC_ZERO, QC_ONE);
        complex_value = qc_bessel_y(qc_make(QF_HALF, QF_ZERO), qc_make(QF_TWO, QF_ONE));
        cut = qc_bessel_y(QC_ZERO, qc_make(qf_neg(QF_ONE), QF_ZERO));
        zero = qc_bessel_y(qc_make(qf_neg(QF_HALF), QF_ZERO), QC_ZERO);
        singular = qc_bessel_y(QC_ZERO, QC_ZERO);
        nan_order = qc_bessel_y(QC_NAN, QC_ONE);
        nan_argument = qc_bessel_y(QC_ZERO, QC_NAN);
        infinite_argument = qc_bessel_y(QC_ZERO, qc_make(QF_INF, QF_ZERO));
    }
    {
        NUM_SCOPE(scope);
        number_t expected = num_create_from_string("0.0882569642156769579829267660235151628278");
        number_t expected_cut = num_create_from_string(
            "0.0882569642156769579829267660235151628278+1.5303953731159331028994350522053264418i");
        number_t z = num_create_from_string("2+i");
        number_t factor = num_sqrt(num_div(NUM_TWO, num_mul(num_const(NUM_PI), z)));
        number_t expected_complex = num_neg(num_mul(factor, num_cos(z)));

        assert_bessel_y_close(num_create_from_qcomplex(positive), expected, "1e-30");
        ASSERT_TRUE(qf_eq(qc_imag(positive), QF_ZERO));
        assert_bessel_y_close(num_create_from_qcomplex(complex_value), expected_complex, "1e-29");
        assert_bessel_y_close(num_create_from_qcomplex(cut), expected_cut, "1e-29");
        ASSERT_TRUE(qc_eq(zero, QC_ZERO));
        ASSERT_TRUE(qc_isnan(singular));
        ASSERT_TRUE(qc_isnan(nan_order));
        ASSERT_TRUE(qc_isnan(nan_argument));
        ASSERT_TRUE(qc_isnan(infinite_argument));
    }
    ASSERT_EQ_INT(num_set_default_prec_bits(saved_precision), 0);
}

/* Detached Bessel Y results remain owned by the caller after all input temporaries expire. */
void run_number_bessel_y_scope_lifetime_tests(void)
{
    size_t saved_precision = num_get_default_prec_bits();
    number_t positive, complex_value, zero, singular, rejected;

    ASSERT_EQ_INT(num_set_default_prec_bits(256u), 0);
    {
        NUM_SCOPE(scope);
        number_t z = num_create_from_string("2+i");

        positive = num_bessel_y(NUM_HALF, NUM_ONE);
        complex_value = num_bessel_y(NUM_HALF, z);
        zero = num_bessel_y(num_neg(NUM_HALF), NUM_ZERO);
        singular = num_bessel_y(NUM_ZERO, NUM_ZERO);
        rejected = num_bessel_y(NUM_NAN, NUM_ONE);
    }
    {
        NUM_SCOPE(scope);
        number_t pi = num_const(NUM_PI);
        number_t expected = num_neg(num_mul(num_sqrt(num_div(NUM_TWO, pi)), num_cos(NUM_ONE)));
        number_t z = num_create_from_string("2+i");
        number_t factor = num_sqrt(num_div(NUM_TWO, num_mul(pi, z)));
        number_t expected_complex = num_neg(num_mul(factor, num_cos(z)));

        ASSERT_TRUE(num_is_real(positive));
        ASSERT_TRUE(num_get_effective_prec_bits(positive) >= 256u);
        ASSERT_TRUE(num_get_effective_prec_bits(complex_value) >= 256u);
        assert_bessel_y_close(positive, expected, "1e-65");
        assert_bessel_y_close(complex_value, expected_complex, "1e-65");
        ASSERT_TRUE(num_is_zero(zero));
        ASSERT_TRUE(num_is_nan(singular));
        ASSERT_TRUE(num_is_nan(rejected));
    }
    num_destroy(&rejected);
    num_destroy(&singular);
    num_destroy(&zero);
    num_destroy(&complex_value);
    num_destroy(&positive);
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
    num_destroy(&number_value);
    snprintf(output, sizeof(output), "Y_0(1) = %.15f", qf_to_double(real_value));
    ASSERT_TRUE(strcmp(output, "Y_0(1) = 0.088256964215677") == 0);
    printf("README qfloat Bessel Y\n%s\n", output);
    snprintf(output, sizeof(output), "Y_0(1) = %.15f + %.0fi",
             qf_to_double(qc_real(complex_value)), qf_to_double(qc_imag(complex_value)));
    ASSERT_TRUE(strcmp(output, "Y_0(1) = 0.088256964215677 + 0i") == 0);
    printf("README qcomplex Bessel Y\n%s\n", output);
}
