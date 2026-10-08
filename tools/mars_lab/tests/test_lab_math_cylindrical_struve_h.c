/**
 * @file test_lab_math_cylindrical_struve_h.c
 * @brief Ordinary Struve H reference, calculus and transform regressions.
 *
 * Ports every test_struve_h.py case, retaining independent gamma-series values,
 * half-order identities, reciprocal-gamma zeros, aliases, Hermite distinction,
 * copied Laplace inverses and README examples. A local 384-bit MPFR reference
 * replaces Decimal without changing the worker's requested 80-digit precision.
 * All worker text is managed by the shared string_t fixture arena.
 */
#include <math.h>
#include <mpfr.h>

#include "test_lab_math_support.h"

static const json_t *lab_math_struve_h_fields(const char *source)
{
    return lab_math_fields(source, "x", "evaluate", 40);
}

static double complex lab_math_struve_h_value(const char *source)
{
    return lab_math_number(lab_math_struve_h_fields(source), "value");
}

static double complex lab_math_struve_h_reference(double order, double complex argument)
{
    double complex total = 0;
    for (unsigned k = 0; k < 150; ++k) {
        double parameter = k + order + 1.5;
        if (parameter <= 0 && parameter == floor(parameter))
            continue;
        double complex term = (k % 2 ? -1 : 1) * cpow(argument / 2, 2 * k + order + 1) /
                              (tgamma(k + 1.5) * tgamma(parameter));
        total += term;
        if (cabs(term) < 1e-18 * fmax(1, cabs(total)) && k > 10)
            break;
    }
    return total;
}

static void test_struve_h_real_orders_and_complex_arguments(void)
{
    const double orders[] = {0, 1, 2, -1, -2, -0.5, -1.5, -2.5, 0.3};
    const double complex points[] = {0.4, 2, 1 + 0.5 * I, -1 + 0.5 * I};
    const char *arguments[] = {"2/5", "2", "1+i/2", "-1+i/2"};
    for (size_t n = 0; n < 9; ++n)
        for (size_t z = 0; z < 4; ++z) {
            double complex expected = lab_math_struve_h_reference(orders[n], points[z]);
            lab_math_close(lab_math_struve_h_value(lab_math_format("struve_h(%g,%s)", orders[n], arguments[z])),
                           expected, 3e-12 * (1 + cabs(expected)));
        }
}

static void test_struve_h_half_integer_identities(void)
{
    const char *arguments[] = {"1/5", "2", "1+i"};
    const char *orders[] = {"-1/2", "1/2", "-3/2"};
    for (size_t z = 0; z < 3; ++z) {
        const char *argument = arguments[z];
        const char *scale = lab_math_format("sqrt(2/(@pi*(%s)))", argument);
        const char *formulas[] = {lab_math_format("%s*sin(%s)", scale, argument),
            lab_math_format("%s*(1-cos(%s))", scale, argument),
            lab_math_format("%s*(cos(%s)-sin(%s)/(%s))", scale, argument, argument, argument)};
        for (size_t n = 0; n < 3; ++n) {
            const char *source = lab_math_format("struve_h(%s,%s)-(%s)", orders[n], argument, formulas[n]);
            lab_math_close(lab_math_number(lab_math_fields(source, "x", "evaluate", 80), "value"), 0, 1e-65);
        }
    }
}

static void test_struve_h_precision(void)
{
    mpfr_t pi, term, total, actual, error, tolerance;
    mpfr_inits2(384, pi, term, total, actual, error, tolerance, (mpfr_ptr)NULL);
    mpfr_set_str(pi,
        "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068",
        10, MPFR_RNDN);
    mpfr_ui_div(term, 2, pi, MPFR_RNDN);
    mpfr_set(total, term, MPFR_RNDN);
    for (unsigned k = 1; k < 90; ++k) {
        mpfr_div_ui(term, term, (2u * k + 1) * (2u * k + 1), MPFR_RNDN);
        mpfr_neg(term, term, MPFR_RNDN);
        mpfr_add(total, total, term, MPFR_RNDN);
    }
    const json_t *result = lab_math_fields("H0(1)", "x", "evaluate", 80);
    const char *text = lab_math_replace(lab_math_text(result, "value"), "−", "-");
    bool parsed = mpfr_set_str(actual, text, 10, MPFR_RNDN) == 0;
    mpfr_sub(error, actual, total, MPFR_RNDN);
    mpfr_abs(error, error, MPFR_RNDN);
    mpfr_set_str(tolerance, "1e-76", 10, MPFR_RNDN);
    lab_math_check(parsed && mpfr_number_p(actual) && mpfr_less_p(error, tolerance),
                   "H0(1), 80 digits: independent MPFR reference has error below 1e-76");
    mpfr_clears(pi, term, total, actual, error, tolerance, (mpfr_ptr)NULL);
}

static void test_struve_h_aliases_renderings_and_hermite_distinction(void)
{
    const char *aliases[] = {"struve_h", "struveh", "StruveH", "𝐇"};
    const char *zero_aliases[] = {"H0", "H_0", "H₀", "𝐇₀"};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_close(lab_math_struve_h_value(lab_math_format("%s(0,1)", aliases[i])), lab_math_struve_h_reference(0, 1), 2e-13);
        lab_math_close(lab_math_struve_h_value(lab_math_format("%s(1)", zero_aliases[i])),
                       lab_math_struve_h_reference(0, 1), 2e-13);
    }
    lab_math_check(lab_math_struve_h_value("ℋ₀(1)") == 1, "Hermite H0 remains distinct from Struve H0");
    const char *sources[] = {"𝐇₀(x)", "𝐇₋₂(x)", "H_n(x)", "𝐇_{n+1}(x)", "struve_h(1/2,x)"};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *result = lab_math_struve_h_fields(sources[i]);
        lab_math_contains(lab_math_text(result, "expression"), "𝐇", true);
        lab_math_contains(lab_math_text(result, "tex"), "\\mathbf{H}", true);
        lab_math_contains(lab_math_text(result, "function"), "struveh(", true);
        lab_math_equal(lab_math_text(lab_math_struve_h_fields(lab_math_text(result, "expression")), "unbound"),
                       lab_math_text(result, "unbound"));
    }
}

static void test_struve_h_derivatives_parity_origin_and_sum(void)
{
    const double orders[] = {0, 1, -1, -1.5, 0.25};
    for (size_t i = 0; i < 5; ++i) {
        double complex actual = lab_math_struve_h_value(lab_math_format("{Dx(struve_h(%g,x)) | x=1}", orders[i]));
        double complex expected = (lab_math_struve_h_reference(orders[i], 1.00001) -
                                   lab_math_struve_h_reference(orders[i], 0.99999)) / 0.00002;
        lab_math_close(actual, expected, 2e-9);
    }
    const int integral_orders[] = {0, 1, 2, -1, -2};
    for (size_t i = 0; i < 5; ++i) {
        int order = integral_orders[i];
        lab_math_close(lab_math_struve_h_value(lab_math_format("struve_h(%d,-1)", order)),
                       (order % 2 ? 1 : -1) * lab_math_struve_h_reference(order, 1), 2e-12);
    }
    lab_math_places(creal(lab_math_struve_h_value("{Dx(H0(x)) | x=0}")), 2 / acos(-1.0), 13);
    lab_math_check(lab_math_struve_h_value("H0(0)") == 0, "Struve H0 vanishes at the origin");
    double complex expected = 0;
    for (int n = 0; n < 4; ++n)
        expected += lab_math_struve_h_reference(n, 1);
    lab_math_close(lab_math_struve_h_value("sum(n,0,3,struve_h(n,1))"), expected, 2e-12);
}

static void test_struve_h_primitives_differentiate_back(void)
{
    const double orders[] = {0, 1, -1, -1.5, -2.5, 0.25};
    for (size_t i = 0; i < 6; ++i) {
        const json_t *result = lab_math_fields(lab_math_format("struve_h(%g,2*x+1)", orders[i]), "x", "integral", 40);
        lab_math_contains(lab_math_text(result, "integral_function"), "return integral(struveh(", true);
        const char *primitive = lab_math_after(lab_math_text(result, "integral"), " = ");
        lab_math_contains(primitive, "∫", false);
        primitive = lab_math_replace(primitive, "x = NAN", "x = 1");
        const json_t *derivative = lab_math_fields(primitive, "x", "derivative", 40);
        lab_math_close(lab_math_number(derivative, "derivative_value"), lab_math_struve_h_reference(orders[i], 3), 3e-10);
    }
}

static void test_struve_h_laplace_native_formula_and_copied_inverse(void)
{
    const json_t *result = lab_math_struve_h_fields("@L{asinh(t)}");
    lab_math_contains(lab_math_text(result, "expression"), "𝐇₀(s)", true);
    lab_math_contains(lab_math_text(result, "expression"), "Y₀(s)", true);
    lab_math_contains(lab_math_text(result, "function"), "struveh(0, s)", true);
    lab_math_contains(lab_math_text(result, "function"), "hypergeometric", false);
    lab_math_contains(lab_math_text(result, "tex"), "\\mathbf{H}_{0}", true);
    const char *spectra[] = {lab_math_algebra(result), "@pi*(struve_h(0,s)-bessely(0,s))/(2*s)",
        "@pi*(2*s/@pi*hypergeometricpfq(1,2,1,3/2,3/2,-s^2/4)-bessely(0,s))/(2*s)"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *inverse = lab_math_struve_h_fields(lab_math_format("InverseLaplace(%s,s,t)", spectra[i]));
        lab_math_contains(lab_math_text(inverse, "function"), "inverselaplace(", false);
        lab_math_contains(lab_math_text(inverse, "function"), "asinh(t)", true);
    }
}

static void test_struve_h_readme(void)
{
    /* README examples: docs/expression.md, ordinary Struve H. */
    lab_math_places(creal(lab_math_struve_h_value("H0(1)")), 0.568656627048288, 14);
    lab_math_places(creal(lab_math_struve_h_value("{Dx(H0(x)) | x=0}")), 0.636619772367581, 14);
}

/* Register all ordinary Struve H cases before README examples. */
void test_lab_math_cylindrical_struve_h_cases(void)
{
    TEST_RUN_IN_GROUP(test_struve_h_real_orders_and_complex_arguments, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_h_half_integer_identities, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_h_precision, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_h_aliases_renderings_and_hermite_distinction, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_h_derivatives_parity_origin_and_sum, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_h_primitives_differentiate_back, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_h_laplace_native_formula_and_copied_inverse, tests, NULL);
    lab_math_reset();
}

/* Register Struve H README examples in the final test phase. */
void test_lab_math_cylindrical_struve_h_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_struve_h_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
