/**
 * @file test_lab_math_cylindrical_bessel_i.c
 * @brief Native modified Bessel I numerical, calculus and presentation regressions.
 *
 * Ports all seven ordinary cases and the README case from test_bessel_i.py.
 * Independent Fourier-coefficient quadrature and a 384-bit MPFR power series
 * preserve the original references and error bounds. Worker text is owned by
 * the shared string_t fixture arena; README registration remains separate.
 */
#include <math.h>
#include <mpfr.h>

#include "test_lab_math_support.h"

typedef struct lab_math_bessel_i_reference {
    int order;
    double complex argument;
} lab_math_bessel_i_reference_t;

static const json_t *lab_math_bessel_i_fields(const char *source)
{
    return lab_math_fields(source, "x", "evaluate", 40);
}

static double complex lab_math_bessel_i_value(const char *source)
{
    return lab_math_number(lab_math_bessel_i_fields(source), "value");
}

static double complex lab_math_bessel_i_integrand(double theta, void *context)
{
    const lab_math_bessel_i_reference_t *reference = context;
    return cexp(reference->argument * cos(theta)) * cos(reference->order * theta);
}

static double complex lab_math_bessel_i_reference(int order, double complex argument)
{
    lab_math_bessel_i_reference_t context = {order, argument};
    double pi = acos(-1.0);
    return lab_math_simpson(lab_math_bessel_i_integrand, &context, 0, pi, 4096) / pi;
}

static void test_bessel_i_integer_orders_and_complex_arguments(void)
{
    const int orders[] = {0, 1, 2, -1, -3};
    const char *arguments[] = {"0", "1", "-2", "1+i"};
    const double complex points[] = {0, 1, -2, 1 + I};
    for (size_t n = 0; n < 5; ++n)
        for (size_t z = 0; z < 4; ++z)
            lab_math_close(lab_math_bessel_i_value(lab_math_format("bessel_i(%d,%s)", orders[n], arguments[z])),
                           lab_math_bessel_i_reference(orders[n], points[z]), 2e-12);
}

static void test_bessel_i_half_integer_formulas(void)
{
    const char *arguments[] = {"1/5", "2", "1+i"};
    const char *orders[] = {"-1/2", "1/2", "3/2"};
    for (size_t z = 0; z < 3; ++z) {
        const char *argument = arguments[z];
        const char *common = lab_math_format("sqrt(2/(@pi*(%s)))", argument);
        const char *formulas[] = {lab_math_format("%s*cosh(%s)", common, argument),
            lab_math_format("%s*sinh(%s)", common, argument),
            lab_math_format("%s*(cosh(%s)-sinh(%s)/(%s))", common, argument, argument, argument)};
        for (size_t n = 0; n < 3; ++n) {
            const char *source = lab_math_format("bessel_i(%s,%s)-(%s)", orders[n], argument, formulas[n]);
            lab_math_close(lab_math_number(lab_math_fields(source, "x", "evaluate", 80), "value"), 0, 1e-65);
        }
    }
}

static void test_bessel_i_precision(void)
{
    mpfr_t term, total, actual, error, tolerance;
    mpfr_inits2(384, term, total, actual, error, tolerance, (mpfr_ptr)NULL);
    mpfr_set_ui(term, 1, MPFR_RNDN);
    mpfr_set_ui(total, 1, MPFR_RNDN);
    for (unsigned n = 1; n < 90; ++n) {
        mpfr_div_ui(term, term, 4u * n * n, MPFR_RNDN);
        mpfr_add(total, total, term, MPFR_RNDN);
    }
    const json_t *result = lab_math_fields("I0(1)", "x", "evaluate", 80);
    const char *text = lab_math_replace(lab_math_text(result, "value"), "−", "-");
    bool parsed = mpfr_set_str(actual, text, 10, MPFR_RNDN) == 0;
    mpfr_sub(error, actual, total, MPFR_RNDN);
    mpfr_abs(error, error, MPFR_RNDN);
    mpfr_set_str(tolerance, "1e-76", 10, MPFR_RNDN);
    lab_math_check(parsed && mpfr_number_p(actual) && mpfr_less_p(error, tolerance),
                   "I0(1), 80 digits: independent MPFR reference has error below 1e-76");
    mpfr_clears(term, total, actual, error, tolerance, (mpfr_ptr)NULL);
}

static void test_bessel_i_aliases_and_renderings(void)
{
    const char *aliases[] = {"bessel_i", "besseli", "BesselI"};
    const char *zero_aliases[] = {"I0", "I_0", "I₀"};
    const char *sources[] = {"I_n(x)", "I_{n}(x)", "bessel_i(n,x)"};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_close(lab_math_bessel_i_value(lab_math_format("%s(0,1)", aliases[i])), lab_math_bessel_i_reference(0, 1), 2e-13);
        lab_math_close(lab_math_bessel_i_value(lab_math_format("%s(1)", zero_aliases[i])),
                       lab_math_bessel_i_reference(0, 1), 2e-13);
        const json_t *result = lab_math_bessel_i_fields(sources[i]);
        lab_math_contains(lab_math_text(result, "tex"), "I_{n}", true);
        lab_math_contains(lab_math_text(result, "function"), "besseli(n, x)", true);
        lab_math_equal(lab_math_text(lab_math_bessel_i_fields(lab_math_text(result, "expression")), "unbound"),
                       lab_math_text(result, "unbound"));
    }
}

static void test_bessel_i_cylindrical_unicode_expression_round_trips(void)
{
    const char *names[] = {"besselj", "bessely", "besselk", "besseli", "struvel", "struveh"};
    const char *symbols[] = {"J", "Y", "K", "I", "𝐋", "𝐇"};
    const char *orders[] = {"0", "12", "-2", "1/2", "n+1"};
    const char *indices[] = {"₀", "₁₂", "₋₂", "_{", "_{"};
    for (size_t family = 0; family < 6; ++family) {
        for (size_t order = 0; order < 5; ++order) {
            const json_t *result = lab_math_bessel_i_fields(lab_math_format("%s(%s,x)", names[family], orders[order]));
            const char *index = order == 2 && family >= 1 && family <= 3 ? "₂" : indices[order];
            lab_math_contains(lab_math_text(result, "expression"), lab_math_format("%s%s", symbols[family], index), true);
            const json_t *copied = lab_math_bessel_i_fields(lab_math_text(result, "expression"));
            lab_math_equal(lab_math_text(copied, "unbound"), lab_math_text(result, "unbound"));
            lab_math_equal(lab_math_text(copied, "tex"), lab_math_text(result, "tex"));
            lab_math_contains(lab_math_text(result, "function"), lab_math_format("%s(", names[family]), true);
        }
    }
    lab_math_contains(lab_math_text(lab_math_bessel_i_fields("{bessel_i(n,x) | x=2; n=3}"), "expression"), "I_{n}(x)", true);
}

static void test_bessel_i_derivatives_and_finite_sum(void)
{
    const int orders[] = {0, 1, 2, -1};
    for (size_t i = 0; i < 4; ++i) {
        int n = orders[i];
        double complex actual = lab_math_bessel_i_value(lab_math_format("{Dx(bessel_i(%d,2*x)) | x=1/2}", n));
        lab_math_close(actual, lab_math_bessel_i_reference(n - 1, 1) + lab_math_bessel_i_reference(n + 1, 1), 2e-12);
    }
    lab_math_check(lab_math_bessel_i_value("{Dx(I0(x)) | x=0}") == 0, "Bessel I0 derivative at zero");
    lab_math_places(creal(lab_math_bessel_i_value("{Dx(I_1(x)) | x=0}")), 0.5, 14);
    double complex expected = 0;
    for (int n = 0; n < 4; ++n)
        expected += lab_math_bessel_i_reference(n, 1);
    lab_math_close(lab_math_bessel_i_value("sum(n,0,3,bessel_i(n,1))"), expected, 2e-12);
}

static void test_bessel_i_primitives_differentiate_back(void)
{
    const int orders[] = {0, 1, 2, -1, -2};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *result = lab_math_fields(lab_math_format("bessel_i(%d,2*x+1)", orders[i]), "x", "integral", 40);
        lab_math_contains(lab_math_text(result, "integral_function"), "return integral(besseli(", true);
        const char *primitive = lab_math_after(lab_math_text(result, "integral"), " = ");
        lab_math_contains(primitive, "∫", false);
        primitive = lab_math_replace(primitive, "x = NAN", "x = 1");
        const json_t *derivative = lab_math_fields(primitive, "x", "derivative", 40);
        lab_math_close(lab_math_number(derivative, "derivative_value"), lab_math_bessel_i_reference(orders[i], 3), 2e-10);
    }
}

static void test_bessel_i_readme(void)
{
    /* README example: docs/expression.md, native modified Bessel I. */
    lab_math_places(creal(lab_math_bessel_i_value("I0(1)")), 1.266065877752008, 14);
    lab_math_check(lab_math_bessel_i_value("{Dx(I0(x)) | x=0}") == 0, "README Bessel I0 derivative at zero");
}

/* Register all ordinary Bessel I cases before README examples. */
void test_lab_math_cylindrical_bessel_i_cases(void)
{
    TEST_RUN_IN_GROUP(test_bessel_i_integer_orders_and_complex_arguments, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_i_half_integer_formulas, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_i_precision, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_i_aliases_and_renderings, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_i_cylindrical_unicode_expression_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_i_derivatives_and_finite_sum, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_i_primitives_differentiate_back, tests, NULL);
    lab_math_reset();
}

/* Register the Bessel I README example in the final test phase. */
void test_lab_math_cylindrical_bessel_i_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_bessel_i_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
