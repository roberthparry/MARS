/**
 * @file test_lab_math_inverse_special.c
 * @brief Special-function inverse Laplace regressions with independent references.
 *
 * Ports every special inverse case, including copied spectra, all Bessel orders,
 * incomplete-gamma families and tiny tails. Adaptive Simpson quadrature and
 * convergent defining series reproduce the original independent references.
 * Source-function comparisons for tiny tails are retained and supplemented by
 * stable C references. This fixture is used only by the sequential native tests.
 */
#include <math.h>

#include "test_lab_math_support.h"

typedef double (*reference_t)(double);
static const double euler_gamma = 0.5772156649015328606;
static double rate;
static double bessel_order;
static double shape;
static bool upper;
static bool regularised;
static reference_t exponential_reference;

static const json_t *math_inverse_fields(const char *source)
{
    return lab_math_fields(source, "t", "evaluate", 40);
}

static double complex math_inverse_value(const char *source, const char *time)
{
    return lab_math_number(math_inverse_fields(lab_math_format("{%s | t=%s}", source, time)), "value");
}

static const json_t *math_inverse_resolve(const char *spectrum)
{
    const json_t *result = math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", spectrum));
    lab_math_contains(lab_math_text(result, "function"), "inverselaplace(", false);
    return result;
}

static void math_inverse_check(const char *spectrum, reference_t reference)
{
    const char *restored = lab_math_algebra(math_inverse_resolve(spectrum));
    const double points[] = {0.2, 0.7, 1.3};
    for (size_t i = 0; i < 3; ++i) {
        double expected = reference(points[i]);
        double complex actual = math_inverse_value(restored, lab_math_format("%.17g", points[i]));
        lab_math_close(actual, expected, 3e-9 * fmax(1, fabs(expected)));
    }
}

static void math_inverse_round_trip(const char *source, reference_t reference)
{
    const json_t *forward = lab_math_fields(lab_math_format("Laplace(%s,t,s)", source), "s", "evaluate", 40);
    lab_math_contains(lab_math_text(forward, "function"), "return laplace(", false);
    math_inverse_check(lab_math_algebra(forward), reference);
}

static double math_inverse_e1_integrand(double u, double x)
{
    return exp(-u) / (x + u);
}

static double math_inverse_refine(double x, double a, double b, double fa, double fm, double fb,
                                  double estimate, double error, unsigned depth)
{
    double mid = (a + b) / 2;
    double fl = math_inverse_e1_integrand((a + mid) / 2, x);
    double fr = math_inverse_e1_integrand((mid + b) / 2, x);
    double lo = (mid - a) * (fa + 4 * fl + fm) / 6;
    double hi = (b - mid) * (fm + 4 * fr + fb) / 6;
    double delta = lo + hi - estimate;
    if (fabs(delta) <= 15 * error)
        return lo + hi + delta / 15;
    if (!lab_math_check(depth != 0, "Independent E1 quadrature did not converge"))
        return NAN;
    return math_inverse_refine(x, a, mid, fa, fl, fm, lo, error / 2, depth - 1)
           + math_inverse_refine(x, mid, b, fm, fr, fb, hi, error / 2, depth - 1);
}

static double math_inverse_e1(double x)
{
    /* E1(x) = exp(-x) integral_0^infinity exp(-u)/(x+u) du; same 48-unit cutoff. */
    double first = math_inverse_e1_integrand(0, x);
    double centre = math_inverse_e1_integrand(24, x);
    double last = math_inverse_e1_integrand(48, x);
    double whole = 48 * (first + 4 * centre + last) / 6;
    return exp(-x) * math_inverse_refine(x, 0, 48, first, centre, last, whole, 2e-12, 24);
}

static double math_inverse_negative_e1(double x)
{
    return -math_inverse_e1(x);
}

static double math_inverse_positive_ei(double x)
{
    double term = x;
    double total = term;
    for (int k = 2; k < 1000; ++k) {
        term *= x / k;
        double contribution = term / k;
        total += contribution;
        if (fabs(contribution) < 2e-16 * fmax(1, fabs(total)))
            return euler_gamma + log(x) + total;
    }
    lab_math_check(false, "Ei reference series did not converge");
    return NAN;
}

static double math_inverse_bessel_j(double order, double x)
{
    if (order < 0 && order == floor(order))
        return pow(-1, -order) * math_inverse_bessel_j(-order, x);
    double term = pow(x / 2, order) / tgamma(order + 1);
    double total = term;
    for (int k = 1; k < 1000; ++k) {
        term *= -(x * x / 4) / (k * (k + order));
        total += term;
        if (fabs(term) < 2e-16 * fmax(1, fabs(total)))
            return total;
    }
    lab_math_check(false, "Bessel J reference series did not converge");
    return NAN;
}

static double math_inverse_bessel_y_zero(double x)
{
    double term = 1, harmonic = 0, correction = 0;
    for (int k = 1; k < 1000; ++k) {
        term *= -(x * x / 4) / ((double)k * k);
        harmonic += 1.0 / k;
        double contribution = -harmonic * term;
        correction += contribution;
        if (fabs(contribution) < 2e-16 * fmax(1, fabs(correction)))
            return 2 / acos(-1) * ((log(x / 2) + euler_gamma) * math_inverse_bessel_j(0, x) + correction);
    }
    lab_math_check(false, "Bessel Y reference series did not converge");
    return NAN;
}

static double math_inverse_incomplete_gamma(double a, double x, bool is_upper)
{
    double value, current;
    if (a == floor(a)) {
        value = is_upper ? exp(-x) : -expm1(-x);
        current = 1;
    } else {
        if (!lab_math_check(2 * a == floor(2 * a), "Gamma reference requires integer or half-integer shape"))
            return NAN;
        value = sqrt(acos(-1)) * (is_upper ? erfc(sqrt(x)) : erf(sqrt(x)));
        current = 0.5;
    }
    while (current < a) {
        double tail = pow(x, current) * exp(-x);
        value = current * value + (is_upper ? tail : -tail);
        current += 1;
    }
    return value;
}

static double math_inverse_exponential(double t) { return exponential_reference(rate * t); }
static double math_inverse_log_one(double t) { return log(2 * t + 1); }
static double math_inverse_log_three(double t) { return log(2 * t + 3); }
static double math_inverse_log10_three(double t) { return log10(2 * t + 3); }
static double math_inverse_affine_e1(double t) { return -3 * math_inverse_e1(2 * t) + 4; }
static double math_inverse_j_reference(double t) { return math_inverse_bessel_j(bessel_order, 2 * t); }
static double math_inverse_y_reference(double t) { return math_inverse_bessel_y_zero(2 * t); }

static double math_inverse_gamma_reference(double t)
{
    return math_inverse_incomplete_gamma(shape, 2 * t, upper) / (regularised ? tgamma(shape) : 1);
}

static void test_exponential_integrals(void)
{
    const double rates[] = {0.5, 2};
    const char *names[] = {"E1", "Ei", "Ei"};
    const int signs[] = {1, -1, 1};
    const reference_t references[] = {math_inverse_e1, math_inverse_negative_e1, math_inverse_positive_ei};
    for (size_t i = 0; i < 2; ++i)
        for (size_t n = 0; n < 3; ++n) {
            rate = rates[i];
            exponential_reference = references[n];
            math_inverse_round_trip(lab_math_format("%s(%.17g*t)", names[n], signs[n] * rate),
                                    math_inverse_exponential);
        }
}

static void test_copied_logarithmic_spectra(void)
{
    const char *spectra[] = {"ln(1+s/2)/s", "-ln(s/2-1)/s", "-ln(1+s/2)/s"};
    const reference_t references[] = {math_inverse_e1, math_inverse_positive_ei, math_inverse_negative_e1};
    rate = 2;
    for (size_t i = 0; i < 3; ++i) {
        exponential_reference = references[i];
        math_inverse_check(spectra[i], math_inverse_exponential);
    }
    math_inverse_check("exp(3*s/2)*E1(3*s/2)/s+ln(3)/s", math_inverse_log_three);
}

static void test_affine_logarithms_and_scalar_multiples(void)
{
    const char *sources[] = {"ln(2*t+1)", "ln(2*t+3)", "log10(2*t+3)", "-3*E1(2*t)+4"};
    const reference_t references[] = {math_inverse_log_one, math_inverse_log_three,
                                     math_inverse_log10_three, math_inverse_affine_e1};
    for (size_t i = 0; i < 4; ++i)
        math_inverse_round_trip(sources[i], references[i]);
}

static void test_bessel_orders_and_scales(void)
{
    const double orders[] = {-3, -0.75, 0, 0.5, 1, 3};
    for (size_t i = 0; i < 6; ++i) {
        bessel_order = orders[i];
        math_inverse_round_trip(lab_math_format("bessel_j(%.17g,2*t)", bessel_order), math_inverse_j_reference);
    }
    math_inverse_round_trip("bessel_y(0,2*t)", math_inverse_y_reference);
}

static void test_copied_bessel_spectra(void)
{
    const char *spectra[] = {"1/sqrt(s^2+4)", "(2/(s+sqrt(s^2+4)))^3/sqrt(s^2+4)",
                             "(2/(s+sqrt(s^2+4)))^(1/2)/sqrt(s^2+4)"};
    const double orders[] = {0, 3, 0.5};
    for (size_t i = 0; i < 3; ++i) {
        bessel_order = orders[i];
        math_inverse_check(spectra[i], math_inverse_j_reference);
    }
    math_inverse_check("-2*ln((s+sqrt(s^2+4))/2)/(@pi*sqrt(s^2+4))", math_inverse_y_reference);
}

static void test_incomplete_gamma_families(void)
{
    const double shapes[] = {0.5, 1.5, 3};
    const char *names[2][2] = {{"gammainc_lower", "gammainc_P"}, {"gammainc_upper", "gammainc_Q"}};
    for (size_t i = 0; i < 3; ++i)
        for (size_t u = 0; u < 2; ++u)
            for (size_t r = 0; r < 2; ++r) {
                shape = shapes[i];
                upper = u != 0;
                regularised = r != 0;
                math_inverse_round_trip(lab_math_format("%s(%.17g,2*t)", names[u][r], shape),
                                        math_inverse_gamma_reference);
            }
}

static void test_copied_gamma_spectrum_and_symbolic_conditions(void)
{
    shape = 1.5;
    upper = false;
    regularised = true;
    math_inverse_check("(2/(s+2))^(3/2)/s", math_inverse_gamma_reference);
    const json_t *result = math_inverse_resolve("(a/(s+a))^v/s");
    lab_math_contains(lab_math_text(result, "expression"), "Re(v) > 0", true);
    lab_math_contains(lab_math_text(result, "expression"), "Re(a) > 0", true);
    lab_math_contains(lab_math_text(result, "expression"), "Re(s)", false);
}

static void test_copied_upper_gamma_spectra_preserve_small_tails(void)
{
    const char *spectra[] = {"(1-(2/(s+2))^(3/2))/s", "gamma(3/2)*(1-(2/(s+2))^(3/2))/s"};
    const char *sources[] = {"gammainc_Q(3/2,2*t)", "gammainc_upper(3/2,2*t)"};
    const char *times[] = {"50", "100"};
    const double numerical_times[] = {50, 100};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *result = math_inverse_resolve(spectra[i]);
        lab_math_contains(lab_math_text(result, "function"), "gammaincq(", true);
        lab_math_contains(lab_math_text(result, "function"), "gammaincp(", false);
        for (size_t j = 0; j < 2; ++j) {
            double complex expected = math_inverse_value(sources[i], times[j]);
            double complex actual = math_inverse_value(lab_math_algebra(result), times[j]);
            lab_math_check(cabs(expected) > 0, "Upper gamma source tail must remain non-zero");
            lab_math_close((actual - expected) / expected, 0, 2e-12);
            double independent = math_inverse_incomplete_gamma(1.5, 2 * numerical_times[j], true);
            if (i == 0)
                independent /= tgamma(1.5);
            lab_math_close((actual - independent) / independent, 0, 2e-12);
        }
    }
}

static double math_inverse_lower_tail(double x)
{
    /* Lower gamma's positive series avoids subtraction of almost equal erf terms. */
    double term = 1 / 1.5;
    double total = term;
    for (int k = 1; k < 1000; ++k) {
        term *= x / (1.5 + k);
        total += term;
        if (fabs(term) < 2e-16 * fabs(total))
            return pow(x, 1.5) * exp(-x) * total;
    }
    lab_math_check(false, "Lower gamma tail reference series did not converge");
    return NAN;
}

static void test_copied_lower_gamma_spectra_preserve_small_tails(void)
{
    const char *spectra[] = {"(2/(s+2))^(3/2)/s", "gamma(3/2)*(2/(s+2))^(3/2)/s"};
    const char *sources[] = {"gammainc_P(3/2,2*t)", "gammainc_lower(3/2,2*t)"};
    const char *times[] = {"1e-20", "1e-30"};
    const double numerical_times[] = {1e-20, 1e-30};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *result = math_inverse_resolve(spectra[i]);
        lab_math_contains(lab_math_text(result, "function"), "gammaincp(", true);
        lab_math_contains(lab_math_text(result, "function"), "gammaincq(", false);
        for (size_t j = 0; j < 2; ++j) {
            double complex expected = math_inverse_value(sources[i], times[j]);
            double complex actual = math_inverse_value(lab_math_algebra(result), times[j]);
            lab_math_check(cabs(expected) > 0, "Lower gamma source tail must remain non-zero");
            lab_math_close((actual - expected) / expected, 0, 2e-12);
            double independent = math_inverse_lower_tail(2 * numerical_times[j]);
            if (i == 0)
                independent /= tgamma(1.5);
            lab_math_close((actual - independent) / independent, 0, 2e-12);
        }
    }
}

static void test_free_parameter_bindings_do_not_remove_guards(void)
{
    const json_t *result = math_inverse_fields("{InverseLaplace((a/(s+a))^v/s,s,t) | t=?,a=2,v=3/2}");
    lab_math_contains(lab_math_text(result, "expression"), "Re(v) > 0", true);
    lab_math_contains(lab_math_text(result, "expression"), "Re(a) > 0", true);
}

static void test_constant_parameters_are_specialised(void)
{
    const json_t *result = math_inverse_fields("{InverseLaplace((a/(s+a))^v/s,s,t) | t=?; a=2; v=3/2}");
    lab_math_contains(lab_math_text(result, "function"), "inverselaplace(", false);
    lab_math_contains(lab_math_text(result, "expression"), "Re(v)", false);
    lab_math_contains(lab_math_text(result, "expression"), "Re(a)", false);
}

/* Register all 11 original special-function inverse regression methods. */
void test_lab_math_inverse_special_cases(void)
{
    TEST_RUN_IN_GROUP(test_exponential_integrals, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_logarithmic_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_affine_logarithms_and_scalar_multiples, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_orders_and_scales, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_bessel_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_incomplete_gamma_families, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_gamma_spectrum_and_symbolic_conditions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_upper_gamma_spectra_preserve_small_tails, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_lower_gamma_spectra_preserve_small_tails, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_free_parameter_bindings_do_not_remove_guards, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_constant_parameters_are_specialised, tests, NULL);
    lab_math_reset();
}
