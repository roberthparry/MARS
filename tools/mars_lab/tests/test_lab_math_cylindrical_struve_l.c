/**
 * @file test_lab_math_cylindrical_struve_l.c
 * @brief Modified Struve L numerical, calculus and rendering regressions.
 *
 * Ports all nine ordinary cases and the README case from test_struve_l.py.
 * Independent C gamma-series references preserve reciprocal-gamma zeros and
 * complex principal powers; a 384-bit MPFR series preserves the Decimal accuracy
 * check. Worker protocol, strings and numerical parsing use the shared native
 * fixture helpers. No test starts a compiler or an interpreter.
 */
#include <math.h>
#include <mpfr.h>

#include "test_lab_math_support.h"

static const json_t *lab_math_struve_l_fields(const char *source)
{
    return lab_math_fields(source, "x", "evaluate", 40);
}

static double complex lab_math_struve_l_value(const char *source)
{
    return lab_math_number(lab_math_struve_l_fields(source), "value");
}

static double complex lab_math_struve_l_reference(double order, double complex argument)
{
    double complex total = 0;
    for (unsigned k = 0; k < 150; ++k) {
        double parameter = k + order + 1.5;
        if (parameter <= 0 && parameter == floor(parameter))
            continue;
        double complex term = cpow(argument / 2, 2 * k + order + 1) /
                              (tgamma(k + 1.5) * tgamma(parameter));
        total += term;
        if (cabs(term) < 1e-17 * fmax(1, cabs(total)) && k > 10)
            break;
    }
    return total;
}

static void test_struve_l_real_orders_and_complex_arguments(void)
{
    const double orders[] = {0, 1, 2, -1, -2, -0.5, -1.5, -2.5, 0.3};
    const double complex points[] = {0.4, 2, 1 + 0.5 * I};
    const char *arguments[] = {"2/5", "2", "1+i/2"};
    for (size_t n = 0; n < 9; ++n)
        for (size_t z = 0; z < 3; ++z) {
            double complex expected = lab_math_struve_l_reference(orders[n], points[z]);
            lab_math_close(lab_math_struve_l_value(lab_math_format("struve_l(%g,%s)", orders[n], arguments[z])),
                           expected, 3e-12 * (1 + cabs(expected)));
        }
}

static void test_struve_l_integer_parity_and_origin(void)
{
    const int orders[] = {0, 1, 2, -1, -2};
    for (size_t i = 0; i < 5; ++i) {
        double complex positive = lab_math_struve_l_value(lab_math_format("struve_l(%d,1)", orders[i]));
        double complex negative = lab_math_struve_l_value(lab_math_format("struve_l(%d,-1)", orders[i]));
        lab_math_close(negative, (orders[i] % 2 ? 1 : -1) * positive, 2e-12);
    }
    const double origin_orders[] = {0, 1, 2, -0.5, -1.5, -2.5};
    for (size_t i = 0; i < 6; ++i) {
        const char *source = lab_math_format("struve_l(%g,0)", origin_orders[i]);
        lab_math_check(lab_math_struve_l_value(source) == 0, source);
    }
    lab_math_places(creal(lab_math_struve_l_value("struve_l(-1,0)")), 2 / acos(-1.0), 13);
}

static void test_struve_l_half_integer_closed_forms(void)
{
    const char *arguments[] = {"1/5", "2", "1+i"};
    const char *orders[] = {"-1/2", "1/2", "-3/2"};
    for (size_t z = 0; z < 3; ++z) {
        const char *argument = arguments[z];
        const char *common = lab_math_format("sqrt(2/(@pi*(%s)))", argument);
        const char *formulas[] = {lab_math_format("%s*sinh(%s)", common, argument),
            lab_math_format("%s*(cosh(%s)-1)", common, argument),
            lab_math_format("%s*(cosh(%s)-sinh(%s)/(%s))", common, argument, argument, argument)};
        for (size_t n = 0; n < 3; ++n) {
            const char *source = lab_math_format("struve_l(%s,%s)-(%s)", orders[n], argument, formulas[n]);
            lab_math_close(lab_math_number(lab_math_fields(source, "x", "evaluate", 80), "value"), 0, 1e-65);
        }
    }
}

static void test_struve_l_precision(void)
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
        mpfr_add(total, total, term, MPFR_RNDN);
    }
    const json_t *result = lab_math_fields("struve_l(0,1)", "x", "evaluate", 80);
    const char *text = lab_math_replace(lab_math_text(result, "value"), "−", "-");
    bool parsed = mpfr_set_str(actual, text, 10, MPFR_RNDN) == 0;
    mpfr_sub(error, actual, total, MPFR_RNDN);
    mpfr_abs(error, error, MPFR_RNDN);
    mpfr_set_str(tolerance, "1e-76", 10, MPFR_RNDN);
    lab_math_check(parsed && mpfr_number_p(actual) && mpfr_less_p(error, tolerance),
                   "Struve L0(1), 80 digits: independent MPFR reference has error below 1e-76");
    mpfr_clears(pi, term, total, actual, error, tolerance, (mpfr_ptr)NULL);
}

static void test_struve_l_aliases_and_rendering(void)
{
    const char *aliases[] = {"struve_l", "struvel", "StruveL", "𝐋"};
    const char *zero_aliases[] = {"L0", "L_0", "L₀", "𝐋₀"};
    const char *sources[] = {"L_n(x)", "L_{n}(x)", "𝐋_n(x)", "struve_l(n,x)"};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_close(lab_math_struve_l_value(lab_math_format("%s(0,1)", aliases[i])), lab_math_struve_l_reference(0, 1), 2e-13);
        lab_math_close(lab_math_struve_l_value(lab_math_format("%s(1)", zero_aliases[i])),
                       lab_math_struve_l_reference(0, 1), 2e-13);
        const json_t *result = lab_math_struve_l_fields(sources[i]);
        lab_math_contains(lab_math_text(result, "tex"), "\\mathbf{L}_{n}", true);
        lab_math_contains(lab_math_text(result, "function"), "struvel(n, x)", true);
        lab_math_equal(lab_math_text(lab_math_struve_l_fields(lab_math_text(result, "expression")), "unbound"),
                       lab_math_text(result, "unbound"));
    }
}

static void test_struve_l_derivative_and_sum(void)
{
    const double orders[] = {0, 1, 2, -1, -1.5};
    for (size_t i = 0; i < 5; ++i) {
        double complex actual = lab_math_struve_l_value(lab_math_format("{Dx(struve_l(%g,2*x)) | x=1/2}", orders[i]));
        double h = 1e-5;
        double complex expected = (lab_math_struve_l_reference(orders[i], 1 + h) -
                                   lab_math_struve_l_reference(orders[i], 1 - h)) / h;
        lab_math_close(actual, expected, 2e-9);
    }
    lab_math_places(creal(lab_math_struve_l_value("{Dx(L0(x)) | x=0}")), 2 / acos(-1.0), 12);
    double complex expected = 0;
    for (int n = 0; n < 4; ++n)
        expected += lab_math_struve_l_reference(n, 1);
    lab_math_close(lab_math_struve_l_value("sum(n,0,3,struve_l(n,1))"), expected, 2e-12);
}

static void test_struve_l_affine_primitives_differentiate_back(void)
{
    const double orders[] = {0, 1, 2, -0.5, -1.5};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *result = lab_math_fields(lab_math_format("struve_l(%g,2*x+1)", orders[i]), "x", "integral", 40);
        lab_math_contains(lab_math_text(result, "integral_function"), "return integral(struvel(", true);
        const char *primitive = lab_math_after(lab_math_text(result, "integral"), " = ");
        lab_math_contains(primitive, "∫", false);
        primitive = lab_math_replace(primitive, "x = NAN", "x = 1");
        const json_t *derivative = lab_math_fields(primitive, "x", "derivative", 40);
        lab_math_close(lab_math_number(derivative, "derivative_value"), lab_math_struve_l_reference(orders[i], 3), 2e-10);
    }
}

static void test_struve_l_laplace_notation_and_round_trip(void)
{
    const json_t *result = lab_math_struve_l_fields("@L{acosh(t)}");
    lab_math_contains(lab_math_text(result, "tex"), "\\mathbf{L}_{0}", true);
    lab_math_contains(lab_math_text(result, "tex"), "I_{0}", true);
    lab_math_contains(lab_math_text(result, "tex"), "F_{", false);
    lab_math_contains(lab_math_text(result, "function"), "struvel(0,", true);
    const json_t *inverse = lab_math_struve_l_fields(lab_math_format("InverseLaplace(%s,s,t)", lab_math_algebra(result)));
    lab_math_contains(lab_math_text(inverse, "function"), "inverselaplace(", false);
    lab_math_contains(lab_math_text(inverse, "function"), "acosh(t)", true);
}

static void test_struve_l_general_hypergeometric_notation(void)
{
    const json_t *ordinary = lab_math_struve_l_fields("2*x*hypergeometricpfq(1,2,2,3,4,x)");
    lab_math_contains(lab_math_text(ordinary, "tex"), "\\cdot", true);
    lab_math_contains(lab_math_text(ordinary, "tex"), "F_{2}", true);
    lab_math_contains(lab_math_text(lab_math_struve_l_fields("I0(x)"), "tex"), "I_{0}", true);
    lab_math_contains(lab_math_text(lab_math_struve_l_fields("I0(x)"), "function"), "besseli(0, x)", true);
    lab_math_contains(lab_math_text(lab_math_struve_l_fields("hypergeometricpfq(0,1,2,x^2/4)"), "tex"), "F_{1}", true);
}

static void test_struve_l_readme(void)
{
    /* README example: docs/expression.md, modified Struve function. */
    lab_math_places(creal(lab_math_struve_l_value("struve_l(0,1)")), 0.710243185937891, 14);
    lab_math_places(creal(lab_math_struve_l_value("{Dx(L0(x)) | x=0}")), 0.636619772367581, 14);
}

/* Register all ordinary Struve L cases before README examples. */
void test_lab_math_cylindrical_struve_l_cases(void)
{
    TEST_RUN_IN_GROUP(test_struve_l_real_orders_and_complex_arguments, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_integer_parity_and_origin, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_half_integer_closed_forms, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_precision, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_aliases_and_rendering, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_derivative_and_sum, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_affine_primitives_differentiate_back, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_laplace_notation_and_round_trip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_struve_l_general_hypergeometric_notation, tests, NULL);
    lab_math_reset();
}

/* Register Struve L README examples in the final test phase. */
void test_lab_math_cylindrical_struve_l_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_struve_l_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
