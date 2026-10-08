/**
 * @file test_lab_math_cylindrical_bessel_y.c
 * @brief Principal Bessel Y quadrature, precision, branch and calculus regressions.
 *
 * Ports all ten ordinary test_bessel_y.py methods and its separate README case.
 * Independent Schlaefli quadrature preserves the original 8192-panel grids. A
 * local 384-bit MPFR complex series replaces the 105-digit Decimal reference,
 * retaining the same constants, term counts and 80-digit residual threshold.
 * Protocol parsing and text ownership use the shared native fixture helpers.
 */
#include <math.h>
#include <mpfr.h>

#include "test_lab_math_support.h"

typedef struct lab_math_bessel_y_reference {
    double complex order;
    double complex argument;
} lab_math_bessel_y_reference_t;

static const json_t *lab_math_bessel_y_fields(const char *source)
{
    return lab_math_fields(source, "x", "evaluate", 40);
}

static double complex lab_math_bessel_y_value(const char *source)
{
    return lab_math_number(lab_math_bessel_y_fields(source), "value");
}

static double complex lab_math_bessel_y_first_integrand(double t, void *context)
{
    const lab_math_bessel_y_reference_t *reference = context;
    return csin(reference->argument * sin(t) - reference->order * t);
}

static double complex lab_math_bessel_y_second_integrand(double t, void *context)
{
    const lab_math_bessel_y_reference_t *reference = context;
    return (cexp(reference->order * t) + ccos(acos(-1.0) * reference->order) * cexp(-reference->order * t)) *
           cexp(-reference->argument * sinh(t));
}

static double complex lab_math_bessel_y_reference(double complex order, double complex argument)
{
    /* DLMF 10.9.7: independent Schlaefli integrals for Re(argument) > 0. */
    lab_math_bessel_y_reference_t context = {order, argument};
    double pi = acos(-1.0);
    double complex first = lab_math_simpson(lab_math_bessel_y_first_integrand, &context, 0, pi, 8192);
    double complex second = lab_math_simpson(lab_math_bessel_y_second_integrand, &context, 0, 8, 8192);
    return (first - second) / pi;
}

static void lab_math_bessel_y_precision_reference(mpfr_t real, mpfr_t imaginary)
{
    mpfr_t pi, gamma, angle, power, logarithm, term_real, term_imaginary, j_real, j_imaginary;
    mpfr_t weighted_real, weighted_imaginary, harmonic, next_real, next_imaginary, temporary;
    mpfr_inits2(384, pi, gamma, angle, power, logarithm, term_real, term_imaginary, j_real, j_imaginary,
                weighted_real, weighted_imaginary, harmonic, next_real, next_imaginary, temporary, (mpfr_ptr)NULL);
    mpfr_set_str(pi,
        "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148",
        10, MPFR_RNDN);
    mpfr_set_str(gamma,
        "0.577215664901532860606512090082402431042159335939923598805767234884867726777664670936947063291746749514631",
        10, MPFR_RNDN);
    mpfr_set_zero(angle, 1);
    mpfr_set_d(power, 0.5, MPFR_RNDN);
    for (unsigned k = 0; k < 190; ++k) {
        mpfr_div_ui(temporary, power, 2u * k + 1, MPFR_RNDN);
        mpfr_add(angle, angle, temporary, MPFR_RNDN);
        mpfr_div_si(power, power, -4, MPFR_RNDN);
    }
    mpfr_set_d(logarithm, 1.25, MPFR_RNDN);
    mpfr_log(logarithm, logarithm, MPFR_RNDN);
    mpfr_div_ui(logarithm, logarithm, 2, MPFR_RNDN);
    mpfr_add(logarithm, logarithm, gamma, MPFR_RNDN);
    mpfr_set_ui(term_real, 1, MPFR_RNDN);
    mpfr_set_ui(j_real, 1, MPFR_RNDN);
    mpfr_set_zero(term_imaginary, 1);
    mpfr_set_zero(j_imaginary, 1);
    mpfr_set_zero(weighted_real, 1);
    mpfr_set_zero(weighted_imaginary, 1);
    mpfr_set_zero(harmonic, 1);
    for (unsigned k = 1; k < 150; ++k) {
        /* -z*z/4 = -3/4-i; retain both old term components until the next pair is complete. */
        mpfr_mul_si(next_real, term_real, -3, MPFR_RNDN);
        mpfr_div_ui(next_real, next_real, 4, MPFR_RNDN);
        mpfr_add(next_real, next_real, term_imaginary, MPFR_RNDN);
        mpfr_div_ui(next_real, next_real, k * k, MPFR_RNDN);
        mpfr_mul_si(next_imaginary, term_imaginary, -3, MPFR_RNDN);
        mpfr_div_ui(next_imaginary, next_imaginary, 4, MPFR_RNDN);
        mpfr_sub(next_imaginary, next_imaginary, term_real, MPFR_RNDN);
        mpfr_div_ui(next_imaginary, next_imaginary, k * k, MPFR_RNDN);
        mpfr_set(term_real, next_real, MPFR_RNDN);
        mpfr_set(term_imaginary, next_imaginary, MPFR_RNDN);
        mpfr_set_ui(temporary, 1, MPFR_RNDN);
        mpfr_div_ui(temporary, temporary, k, MPFR_RNDN);
        mpfr_add(harmonic, harmonic, temporary, MPFR_RNDN);
        mpfr_add(j_real, j_real, term_real, MPFR_RNDN);
        mpfr_add(j_imaginary, j_imaginary, term_imaginary, MPFR_RNDN);
        mpfr_mul(temporary, harmonic, term_real, MPFR_RNDN);
        mpfr_add(weighted_real, weighted_real, temporary, MPFR_RNDN);
        mpfr_mul(temporary, harmonic, term_imaginary, MPFR_RNDN);
        mpfr_add(weighted_imaginary, weighted_imaginary, temporary, MPFR_RNDN);
    }
    mpfr_mul(real, logarithm, j_real, MPFR_RNDN);
    mpfr_mul(temporary, angle, j_imaginary, MPFR_RNDN);
    mpfr_sub(real, real, temporary, MPFR_RNDN);
    mpfr_sub(real, real, weighted_real, MPFR_RNDN);
    mpfr_mul_ui(real, real, 2, MPFR_RNDN);
    mpfr_div(real, real, pi, MPFR_RNDN);
    mpfr_mul(imaginary, logarithm, j_imaginary, MPFR_RNDN);
    mpfr_mul(temporary, angle, j_real, MPFR_RNDN);
    mpfr_add(imaginary, imaginary, temporary, MPFR_RNDN);
    mpfr_sub(imaginary, imaginary, weighted_imaginary, MPFR_RNDN);
    mpfr_mul_ui(imaginary, imaginary, 2, MPFR_RNDN);
    mpfr_div(imaginary, imaginary, pi, MPFR_RNDN);
    mpfr_clears(pi, gamma, angle, power, logarithm, term_real, term_imaginary, j_real, j_imaginary,
                weighted_real, weighted_imaginary, harmonic, next_real, next_imaginary, temporary, (mpfr_ptr)NULL);
}

static void test_bessel_y_independent_quadrature(void)
{
    const char *orders[] = {"0", "1", "-2", "1/3", "1/4+3*i/8", "-5/4+i/2"};
    const double complex order_values[] = {0, 1, -2, 1.0 / 3, 0.25 + 0.375 * I, -1.25 + 0.5 * I};
    const char *arguments[] = {"1", "2+i", "2-i"};
    const double complex points[] = {1, 2 + I, 2 - I};
    for (size_t n = 0; n < 6; ++n)
        for (size_t z = 0; z < 3; ++z) {
            double complex expected = lab_math_bessel_y_reference(order_values[n], points[z]);
            lab_math_close(lab_math_bessel_y_value(lab_math_format("bessely(%s,%s)", orders[n], arguments[z])),
                           expected, 2e-11 * (1 + cabs(expected)));
        }
}

static void test_bessel_y_y0_complex_precision(void)
{
    mpfr_t real, imaginary;
    mpfr_inits2(384, real, imaginary, (mpfr_ptr)NULL);
    lab_math_bessel_y_precision_reference(real, imaginary);
    char *real_text = NULL, *imaginary_text = NULL;
    int real_size = mpfr_asprintf(&real_text, "%.105Rg", real);
    int imaginary_size = mpfr_asprintf(&imaginary_text, "%.105Rg", imaginary);
    if (lab_math_check(real_size > 0 && imaginary_size > 0, "format independent 105-digit Y0 reference")) {
        const int signs[] = {1, -1};
        for (size_t i = 0; i < 2; ++i) {
            const char *source = lab_math_format("Y0(2+(%d)*i)-((%s)+(%d)*(%s)*i)",
                                                 signs[i], real_text, signs[i], imaginary_text);
            const json_t *residual = lab_math_fields(source, "x", "evaluate", 80);
            lab_math_close(lab_math_number(residual, "value"), 0, 1e-75);
        }
    }
    if (real_text)
        mpfr_free_str(real_text);
    if (imaginary_text)
        mpfr_free_str(imaginary_text);
    mpfr_clears(real, imaginary, (mpfr_ptr)NULL);
}

static void test_bessel_y_half_integer_principal_values(void)
{
    const char *arguments[] = {"1/5", "2+i", "2-i", "-2", "-2+i/5", "-2-i/5"};
    const char *orders[] = {"1/2", "-1/2", "3/2", "-3/2"};
    for (size_t z = 0; z < 6; ++z) {
        const char *argument = arguments[z];
        const char *common = lab_math_format("sqrt(2/@pi)/sqrt(%s)", argument);
        const char *formulas[] = {lab_math_format("-%s*cos(%s)", common, argument),
            lab_math_format("%s*sin(%s)", common, argument),
            lab_math_format("-%s*(cos(%s)/(%s)+sin(%s))", common, argument, argument, argument),
            lab_math_format("%s*(cos(%s)-sin(%s)/(%s))", common, argument, argument, argument)};
        for (size_t n = 0; n < 4; ++n) {
            const char *source = lab_math_format("bessely(%s,%s)-(%s)", orders[n], argument, formulas[n]);
            lab_math_close(lab_math_number(lab_math_fields(source, "x", "evaluate", 80), "value"), 0, 1e-70);
        }
    }
}

static void test_bessel_y_near_integer_orders(void)
{
    /* The exact denominator must fit the expression evaluator's rational precision budget. */
    const int orders[] = {0, 1, -2};
    const char *offsets[] = {"1/10^100", "-1/10^100", "i/10^100"};
    for (size_t n = 0; n < 3; ++n)
        for (size_t offset = 0; offset < 3; ++offset) {
            const char *displacement = lab_math_format("(%d+(%s))-(%d)", orders[n], offsets[offset], orders[n]);
            const json_t *result = lab_math_fields(displacement, "x", "evaluate", 140);
            lab_math_check(lab_math_number(result, "value") != 0, displacement);
            const char *source = lab_math_format("bessely(%d+(%s),2+i)-bessely(%d,2+i)",
                                                 orders[n], offsets[offset], orders[n]);
            lab_math_close(lab_math_number(lab_math_fields(source, "x", "evaluate", 140), "value"), 0, 1e-72);
        }
}

static void test_bessel_y_tiny_negative_half_order(void)
{
    const char *source = "bessely(-1/2,1/10^100)/(sqrt(2/@pi)*sqrt(1/10^100))-1";
    lab_math_close(lab_math_number(lab_math_fields(source, "x", "evaluate", 80), "value"), 0, 1e-72);
}

static void test_bessel_y_integer_cut_and_parity(void)
{
    for (int n = 0; n < 4; ++n) {
        double complex upper = lab_math_bessel_y_value(lab_math_format("bessely(%d,-2)", n));
        double complex near_upper = lab_math_bessel_y_value(lab_math_format("bessely(%d,-2+i/10^30)", n));
        double complex lower = lab_math_bessel_y_value(lab_math_format("bessely(%d,-2-i/10^30)", n));
        lab_math_close(upper, near_upper, 1e-25);
        lab_math_close(conj(upper), lower, 1e-25);
        const char *source = lab_math_format("bessely(-%d,2+i)-(-1)^%d*bessely(%d,2+i)", n, n, n);
        lab_math_close(lab_math_bessel_y_value(source), 0, 1e-30);
    }
}

static void test_bessel_y_origin_limits_and_recurrence(void)
{
    const char *origin_orders[] = {"-1/2", "-3/2", "-5/2"};
    for (size_t i = 0; i < 3; ++i) {
        const char *source = lab_math_format("bessely(%s,0)", origin_orders[i]);
        lab_math_check(lab_math_bessel_y_value(source) == 0, source);
    }
    lab_math_regex(lab_math_text(lab_math_bessel_y_fields("Y0(0)"), "value"), "[Nn][Aa][Nn]", true);
    const char *orders[] = {"0", "2", "1/3", "1/4+i/3"};
    for (size_t i = 0; i < 4; ++i) {
        const char *source = lab_math_format(
            "bessely((%s)-1,2+i)+bessely((%s)+1,2+i)-2*(%s)/(2+i)*bessely(%s,2+i)",
            orders[i], orders[i], orders[i], orders[i]);
        lab_math_close(lab_math_number(lab_math_fields(source, "x", "evaluate", 80), "value"), 0, 1e-70);
    }
}

static void test_bessel_y_complex_derivatives_and_finite_sum(void)
{
    const double orders[] = {0, 1, -0.5, -1.5, -2.5};
    for (size_t i = 0; i < 5; ++i) {
        double complex actual = lab_math_bessel_y_value(lab_math_format("{Dx(bessely(%g,x))|x=2+i}", orders[i]));
        double complex expected = (lab_math_bessel_y_reference(orders[i], 2.00001 + I) -
                                   lab_math_bessel_y_reference(orders[i], 1.99999 + I)) / 0.00002;
        lab_math_close(actual, expected, 3e-9);
    }
    const char *origin_orders[] = {"-3/2", "-5/2"};
    for (size_t i = 0; i < 2; ++i) {
        const char *source = lab_math_format("{Dx(bessely(%s,x))|x=0}", origin_orders[i]);
        lab_math_check(lab_math_bessel_y_value(source) == 0, source);
    }
    double complex expected = 0;
    for (int n = 0; n < 4; ++n)
        expected += lab_math_bessel_y_reference(n, 2 + I);
    lab_math_close(lab_math_bessel_y_value("sum(n,0,3,bessely(n,2+i))"), expected, 2e-10);
}

static void test_bessel_y_integer_primitives_differentiate_back(void)
{
    const int orders[] = {0, 1, 2, -1, -2};
    for (size_t i = 0; i < 5; ++i) {
        const char *source = lab_math_format("bessely(%d,2*x+1)", orders[i]);
        const json_t *result = lab_math_fields(source, "x", "integral", 40);
        lab_math_regex(lab_math_text(result, "integral_function"), "return integral\\(-?bessely\\(", true);
        const char *primitive = lab_math_after(lab_math_text(result, "integral"), " = ");
        lab_math_contains(primitive, "∫", false);
        primitive = lab_math_replace(primitive, "x = NAN", "x = 1+i");
        const json_t *derivative = lab_math_fields(primitive, "x", "derivative", 40);
        const json_t *expected = lab_math_fields(lab_math_format("bessely(%d,3+2*i)", orders[i]), "x", "evaluate", 80);
        lab_math_close(lab_math_number(derivative, "derivative_value"), lab_math_number(expected, "value"), 2e-12);
    }
}

static void test_bessel_y_aliases(void)
{
    const char *aliases[] = {"Y0", "Y_0", "Y₀"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_places(creal(lab_math_bessel_y_value(lab_math_format("%s(1)", aliases[i]))), 0.08825696421567696, 15);
}

static void test_bessel_y_readme(void)
{
    /* README example: docs/expression.md; registered after every ordinary mathematical suite. */
    lab_math_places(creal(lab_math_bessel_y_value("Y0(1)")), 0.088256964215677, 14);
    lab_math_places(creal(lab_math_bessel_y_value("{Dx(Y0(x))|x=1}")), 0.781212821300289, 14);
}

/* Register all ordinary Bessel Y cases before README examples. */
void test_lab_math_cylindrical_bessel_y_cases(void)
{
    TEST_RUN_IN_GROUP(test_bessel_y_independent_quadrature, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_y0_complex_precision, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_half_integer_principal_values, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_near_integer_orders, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_tiny_negative_half_order, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_integer_cut_and_parity, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_origin_limits_and_recurrence, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_complex_derivatives_and_finite_sum, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_integer_primitives_differentiate_back, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_y_aliases, tests, NULL);
    lab_math_reset();
}

/* Register the Bessel Y README example in the final test phase. */
void test_lab_math_cylindrical_bessel_y_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_bessel_y_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
