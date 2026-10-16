/**
 * @file test_lab_math_laplace_circular.c
 * @brief Principal acosh, asin and acos Laplace regression ports.
 *
 * Preserves test_laplace_acosh.py and test_laplace_invcircular.py. Independent
 * quadrature smooths each branch point; inverse checks reparse copied spectra
 * at every original time. README examples have a separate final-phase entry.
 */
#include <math.h>
#include <string.h>

#define MARS_SHARED_EXPR_DISTRIBUTION_INTERNAL_ACCESS
#include "internal/expr_distribution_internal.h"
#include "test_lab_math_support.h"

typedef struct {
    unsigned family;
    double rate;
    double complex target;
    bool exterior;
} circular_context_t;

static const char *names[] = {"acosh", "asin", "acos", "asinh"};

static const json_t *lab_math_laplace_circular_fields(const char *source, const char *variable)
{
    return lab_math_fields(source, variable, "evaluate", 40);
}

static double complex lab_math_laplace_circular_real_cut(unsigned family, double x)
{
    if (!family)
        return cacosh(x + 0.0 * I);
    if (family == 3)
        return asinh(x);
    double complex sine = fabs(x) <= 1 ? asin(x) : copysign(acos(-1) / 2, x) + I * acosh(fabs(x));
    return family == 1 ? sine : acos(-1) / 2 - sine;
}

static double complex lab_math_laplace_circular_circular_integrand(double u, void *opaque)
{
    circular_context_t *context = opaque;
    double pi = acos(-1), q = fabs(context->rate), sign = context->rate > 0 ? 1 : -1;
    double t = (context->exterior ? cosh(u) : cos(u)) / q;
    double jacobian = (context->exterior ? sinh(u) : sin(u)) / q;
    double complex source;
    if (!context->family) {
        source = context->exterior ? u + (sign < 0 ? I * pi : 0) : I * (sign > 0 ? u : pi - u);
    } else {
        double complex sine = context->exterior ? sign * pi / 2 + I * u : sign * (pi / 2 - u);
        source = context->family == 1 ? sine : pi / 2 - sine;
    }
    return source * cexp(-context->target * t) * jacobian;
}

static double complex lab_math_laplace_circular_reference(unsigned family, double rate, double complex target)
{
    circular_context_t context = {family, rate, target, false};
    double complex integral = lab_math_simpson(lab_math_laplace_circular_circular_integrand, &context,
                                               0, acos(-1) / 2, 8000);
    context.exterior = true;
    return integral + lab_math_simpson(lab_math_laplace_circular_circular_integrand, &context, 0, 7, 8000);
}

static const json_t *lab_math_laplace_circular_check_inverse(const char *spectrum, unsigned family, double rate, double scale,
                                  double offset, bool shift)
{
    const json_t *inverse = lab_math_laplace_circular_fields(lab_math_format("InverseLaplace(%s,s,t)", spectrum), "t");
    lab_math_contains(lab_math_text(inverse, "function"), "inverselaplace(", false);
    const double points[] = {0.19, 0.5, 1, 1.31, 3};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *actual = lab_math_laplace_circular_fields(lab_math_format("{%s | t=%.17g}",
            lab_math_algebra(inverse), points[i]), "t");
        double complex expected = scale * lab_math_laplace_circular_real_cut(family, rate * points[i]) + offset;
        if (shift)
            expected *= exp(-points[i]);
        lab_math_close(lab_math_number(actual, "value"), expected, 2e-11 * (1 + cabs(expected)));
    }
    return inverse;
}

static void test_laplace_circular_quadrature(void)
{
    const double rates[] = {1, -1, 2, -0.5};
    const char *targets[][3] = {{"2", "1+i", NULL}, {"1", "2+i", "2-i"}, {"1", "2+i", "2-i"}};
    const double complex values[][3] = {{2, 1 + I, 0}, {1, 2 + I, 2 - I}, {1, 2 + I, 2 - I}};
    for (unsigned family = 0; family < 3; ++family) {
        for (size_t r = 0; r < 4; ++r) {
            const char *source = lab_math_format("Laplace(%s(%.17g*t),t,s)", names[family], rates[r]);
            const json_t *unbound = lab_math_laplace_circular_fields(source, "s");
            lab_math_contains(lab_math_text(unbound, "function"), "laplace(", false);
            for (size_t s = 0; s < (family ? 3u : 2u); ++s) {
                const json_t *bound = lab_math_laplace_circular_fields(lab_math_format("{%s | s=%s}", source,
                    targets[family][s]), "s");
                lab_math_contains(lab_math_text(bound, "function"), "laplace(", false);
                lab_math_equal(lab_math_text(bound, "tex"), lab_math_text(unbound, "tex"));
                lab_math_close(lab_math_number(bound, "value"), lab_math_laplace_circular_reference(family, rates[r],
                    values[family][s]),
                               family ? 3e-10 : 2e-10);
            }
            const json_t *inverse = lab_math_laplace_circular_check_inverse(lab_math_algebra(unbound), family, rates[r],
                1, 0, false);
            if (family) {
                lab_math_contains(lab_math_text(inverse, "function"), lab_math_format("%s(", names[family]), true);
                lab_math_contains(lab_math_text(inverse, "function"), "acosh(", false);
            }
        }
    }
}

static void test_laplace_circular_domains(void)
{
    const char *invalid[] = {"0", "-1", "i"}, *arguments[] = {"c*t", "i*t", "t+1"};
    for (unsigned family = 0; family < 3; ++family) {
        const json_t *bound = lab_math_laplace_circular_fields(lab_math_format("{Laplace(%s(c*t),t,s) | s=2; c=-2}",
            names[family]), "s");
        lab_math_contains(lab_math_text(bound, "function"), "laplace(", false);
        lab_math_close(lab_math_number(bound, "value"), lab_math_laplace_circular_reference(family, -2, 2), family ?
            3e-10 : 2e-10);
        for (size_t i = 0; i < 3; ++i) {
            const json_t *result = lab_math_laplace_circular_fields(lab_math_format("{Laplace(%s(t),t,s) | s=%s}",
                names[family], invalid[i]), "s");
            lab_math_equal(lab_math_text(result, "value"), "NAN");
            result = lab_math_laplace_circular_fields(lab_math_format("Laplace(%s(%s),t,s)", names[family], arguments[i]), "s");
            lab_math_contains(lab_math_text(result, "function"), "laplace(", true);
        }
        const json_t *zero = lab_math_laplace_circular_fields(lab_math_format("{Laplace(%s(0*t),t,s) | s=2}",
            names[family]), "s");
        double complex expected = family == 0 ? I * acos(-1) / 4 : family == 1 ? 0 : acos(-1) / 4;
        lab_math_close(lab_math_number(zero, "value"), expected, family ? 1e-14 : 2e-12);
        if (family) {
            const json_t *free = lab_math_laplace_circular_fields(lab_math_format("{Laplace(%s(c*t),t,s) | c=-2, s=2}",
                names[family]), "s");
            lab_math_contains(lab_math_text(free, "function"), "laplace(", true);
        }
    }
}

static void test_laplace_circular_rendering(void)
{
    const char *symbols[] = {"K₀(s)", "I₀(s)", "𝐋₀(s)"};
    const char *calls[] = {"besselk(0, s)", "besseli(0, s)", "struvel(0, s)"};
    const char *old_names[] = {"BesselK(", "BesselI(", "StruveL("};
    for (unsigned family = 0; family < 3; ++family) {
        const json_t *forward = lab_math_laplace_circular_fields(lab_math_format("@L{%s(t)}", names[family]), "s");
        for (size_t i = 0; i < 3; ++i) {
            lab_math_contains(lab_math_text(forward, "expression"), symbols[i], true);
            if (family)
                lab_math_contains(lab_math_text(forward, "function"), calls[i], true);
            else
                lab_math_contains(lab_math_text(forward, "expression"), old_names[i], false);
        }
        if (!family)
            lab_math_equal(lab_math_text(lab_math_laplace_circular_fields(lab_math_text(forward, "expression"), "s"), "unbound"),
                           lab_math_text(forward, "unbound"));
        else {
            for (int x = -2; x <= 2; ++x)
                lab_math_close(lab_math_number(lab_math_laplace_circular_fields(lab_math_format("%s(%d)", names[family],
                    x), "s"), "value"),
                               lab_math_laplace_circular_real_cut(family, x), 1e-14);
        }
    }
}

static void test_laplace_circular_independent_inverses(void)
{
    for (int sign = -1; sign <= 1; sign += 2) {
        const char *spectrum = lab_math_format("besselk(0,s/2)/s+i*@pi/(2*s)*(1-(%d)*"
                                               "(hypergeometricpfq(0,1,1,s^2/16)-s/@pi*"
                                               "hypergeometricpfq(1,2,1,3/2,3/2,s^2/16)))", sign);
        lab_math_laplace_circular_check_inverse(spectrum, 0, sign * 2, 1, 0, false);
        lab_math_laplace_circular_check_inverse(lab_math_format("3*(%s)+2/s", spectrum), 0, sign * 2, 3, 2, false);
    }
    const char *spectra[] = {"@pi/(2*s)*(bessel_i(0,s)-struve_l(0,s))+i*besselk(0,s)/s",
                             "@pi/(2*s)*(1-bessel_i(0,s)+struve_l(0,s))-i*besselk(0,s)/s"};
    for (unsigned family = 1; family < 3; ++family) {
        lab_math_laplace_circular_check_inverse(spectra[family - 1], family, 1, 1, 0, false);
        lab_math_laplace_circular_check_inverse(lab_math_format("3*(%s)+2/s", spectra[family - 1]), family, 1, 3, 2, false);
    }
    const json_t *incomplete = lab_math_laplace_circular_fields("InverseLaplace(besselk(0,s)/s,s,t)", "t");
    if (!strstr(lab_math_text(incomplete, "function"), "inverselaplace(")) {
        const json_t *result = lab_math_laplace_circular_fields(lab_math_format("{%s | t=1/2}",
            lab_math_algebra(incomplete)), "t");
        lab_math_check(cabs(lab_math_number(result, "value") - cacosh(0.5 + 0.0 * I)) > 0.1,
                       "K0 alone must not recover the full principal acosh branch");
    }
    for (unsigned family = 0; family < 4; ++family) {
        const json_t *forward = lab_math_laplace_circular_fields(lab_math_format("Laplace(exp(-t)*%s(t),t,s)",
            names[family]), "s");
        const json_t *inverse = lab_math_laplace_circular_check_inverse(lab_math_algebra(forward), family, 1, 1, 0, true);
        if (family == 1 || family == 2)
            lab_math_contains(lab_math_text(inverse, "function"), lab_math_format("%s(", names[family]), true);
    }
}

static void test_laplace_circular_readme(void)
{
    /* README examples: docs/expression.md, principal acosh and inverse circular Laplace pairs. */
    const json_t *forward = lab_math_laplace_circular_fields("@L{acosh(t)}", "s");
    lab_math_equal(lab_math_text(forward, "unbound"),
                   "(K₀(s) + 0.5iπ·(1 - I₀(s) + 𝐋₀(s)))/s where (Re(s) > 0)");
    const double complex documented[] = {0.421024438240708 + 0.697712084144029 * I,
                                         0.873084242650868 + 0.421024438240708 * I,
                                         0.697712084144029 - 0.421024438240708 * I};
    for (unsigned family = 0; family < 3; ++family) {
        const json_t *result = lab_math_laplace_circular_fields(lab_math_format("{@L{%s(t)} | s=1}", names[family]), "s");
        lab_math_close(lab_math_number(result, "value"), documented[family], 1e-15);
        lab_math_close(lab_math_number(result, "value"), lab_math_laplace_circular_reference(family, 1, 1), family ?
            3e-10 : 2e-10);
    }
    const json_t *inverse = lab_math_laplace_circular_check_inverse(lab_math_algebra(forward), 0, 1, 1, 0, false);
    lab_math_contains(lab_math_text(inverse, "function"), "acosh(t)", true);
}

typedef struct {
    double rate, phase;
    bool outer_sine, inner_sine;
    double complex target;
} folded_context_t;

static double complex folded_integrand(double t, void *opaque)
{
    folded_context_t *c = opaque;
    double phase = c->rate * t + c->phase;
    double inner = c->inner_sine ? sin(phase) : cos(phase);
    return (c->outer_sine ? asin(inner) : acos(inner)) * cexp(-c->target * t);
}

static void test_laplace_folded_circular(void)
{
    const double rates[] = {1, -1, 2, -0.5};
    const double phases[] = {0, 2, -20, 20};
    for (unsigned outer = 0; outer < 2; ++outer) {
        for (unsigned inner = 0; inner < 2; ++inner) {
            for (unsigned i = 0; i < 4; ++i) {
                folded_context_t context = {rates[i], phases[i], outer != 0, inner != 0, 1 + I};
                double period = 2 * acos(-1) / fabs(rates[i]);
                double complex expected = lab_math_simpson(folded_integrand, &context, 0, period, 20000) /
                                          (1 - cexp(-context.target * period));
                const json_t *fields = lab_math_laplace_circular_fields(lab_math_format(
                    "{Laplace(%s(%s(%.17g*t+%.17g)),t,s) | s=1+i}",
                    outer ? "asin" : "acos", inner ? "sin" : "cos", rates[i], phases[i]), "s");
                lab_math_contains(lab_math_text(fields, "function"), "laplace(", false);
                lab_math_close(lab_math_number(fields, "value"), expected, 2e-7);
            }
        }
    }
    const char *corners[] = {"0", "@pi", "-@pi", "2*@pi", "-2*@pi", "@pi/2"};
    for (unsigned i = 0; i < 6; ++i) {
        const json_t *fields = lab_math_laplace_circular_fields(lab_math_format(
            "{Laplace(acos(cos(t+(%s))),t,s) | s=1}", corners[i]), "s");
        lab_math_contains(lab_math_text(fields, "function"), "laplace(", false);
        double expected = i == 1 || i == 2 ? acos(-1) - tanh(acos(-1) / 2) :
                          i == 5 ? acos(-1) / 2 + 1 - 1 / cosh(acos(-1) / 2) : tanh(acos(-1) / 2);
        lab_math_close(lab_math_number(fields, "value"), expected, 1e-13);
    }
    const char *outside[] = {"0", "-1", "i"};
    for (unsigned i = 0; i < 3; ++i) {
        const json_t *fields = lab_math_laplace_circular_fields(lab_math_format(
            "{@L{acos(sin(2-x))} | s=%s}", outside[i]), "s");
        lab_math_equal(lab_math_text(fields, "value"), "NAN");
    }
    const json_t *constant = lab_math_laplace_circular_fields(
        "{Laplace(acos(sin(a*t+b)),t,s) | s=1; a=-1, b=2}", "s");
    double d = 2 - acos(-1) / 2;
    lab_math_contains(lab_math_text(constant, "function"), "laplace(", false);
    lab_math_close(lab_math_number(constant, "value"), d - 1 + 2 * exp(-d) / (1 + exp(-acos(-1))), 1e-13);
    const json_t *zero = lab_math_laplace_circular_fields(
        "{Laplace(acos(sin(0*t+2)),t,s) | s=2}", "s");
    lab_math_close(lab_math_number(zero, "value"), acos(sin(2)) / 2, 1e-13);
    const json_t *free = lab_math_laplace_circular_fields(
        "{Laplace(acos(sin(a*t+2)),t,s) | a=-1, s=1}", "s");
    lab_math_contains(lab_math_text(free, "function"), "laplace(", true);
    const char *unsupported[] = {"a*t+2", "i*t+2", "t^2+2"};
    for (unsigned i = 0; i < 3; ++i) {
        const json_t *fields = lab_math_laplace_circular_fields(lab_math_format(
            "Laplace(acos(sin(%s)),t,s)", unsupported[i]), "s");
        lab_math_contains(lab_math_text(fields, "function"), "laplace(", true);
    }
}

typedef struct {
    double rate, phase, sign;
    size_t count;
} spectrum_fixture_t;

static bool check_spectral_weight(number_t coordinate, number_t weight, void *opaque)
{
    spectrum_fixture_t *fixture = opaque;
    double k = num_to_double(coordinate) / fixture->rate;
    number_t real = num_real_part(weight), imag = num_imag_part(weight);
    double complex actual = num_to_double(real) + I * num_to_double(imag);
    double complex expected = k == 0 ? acos(-1) * acos(-1) :
        fixture->sign * 4 * cexp(I * k * fixture->phase) / (k * k);
    lab_math_close(actual, expected, 1e-12);
    num_destroy(&imag);
    num_destroy(&real);
    ++fixture->count;
    return true;
}

static void check_folded_spectrum(const char *source, spectrum_fixture_t *fixture)
{
    expr_bindings_t *bindings = NULL;
    const json_t *fields = lab_math_laplace_circular_fields(source, "k");
    lab_math_contains(lab_math_text(fields, "function"), "fourier(", false);
    expr_t *parsed = expr_from_string(lab_math_text(fields, "expression"), &bindings), *result = expr_simplify(parsed);
    bool preview = false;
    expr_t *regular = expr_distribution_decompose(result, expr_bindings_get(bindings, "k"),
                                                   check_spectral_weight, fixture, &preview);
    lab_math_check(regular && preview && fixture->count == (fixture->sign < 0 ? 65u : 64u),
                   "folded Fourier spectra retain every signed odd harmonic in the bounded preview");
    expr_free(regular);
    expr_free(result);
    expr_free(parsed);
    expr_bindings_free(bindings);
}

static void test_fourier_folded_spectrum(void)
{
    for (unsigned outer = 0; outer < 2; ++outer) {
        for (unsigned inner = 0; inner < 2; ++inner) {
            for (int sign = -1; sign <= 1; sign += 2) {
                spectrum_fixture_t fixture = {2 * sign, 0.37 - (inner ? acos(-1) / 2 : 0), outer ? 1 : -1, 0};
                check_folded_spectrum(lab_math_format("@F{%s(%s(%d*x+37/100))}",
                    outer ? "asin" : "acos", inner ? "sin" : "cos", 2 * sign), &fixture);
            }
        }
    }
}

static void test_fourier_folded_readme(void)
{
    /* README example: docs/expression.md, Fourier spectrum of a shifted triangular wave. */
    spectrum_fixture_t fixture = {-1, acos(-1) / 2, -1, 0};
    check_folded_spectrum("@F{acos(sin(pi-x))}", &fixture);
}

static void test_laplace_folded_readme(void)
{
    /* README example: docs/expression.md, shifted periodic triangular wave. */
    const json_t *fields = lab_math_laplace_circular_fields("@L{acos(sin(2-x))}", "s");
    lab_math_contains(lab_math_text(fields, "function"), "laplace(", false);
    lab_math_contains(lab_math_text(fields, "unbound"), "Re(s) > 0", true);
    fields = lab_math_laplace_circular_fields("{@L{acos(sin(2-x))} | s=1}", "s");
    double d = 2 - acos(-1) / 2;
    lab_math_close(lab_math_number(fields, "value"), d - 1 + 2 * exp(-d) / (1 + exp(-acos(-1))), 1e-13);
}

/* Register both complete branch-sensitive forward Laplace suites. */
void test_lab_math_laplace_circular_cases(void)
{
    TEST_RUN_IN_GROUP(test_fourier_folded_spectrum, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_folded_circular, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_circular_quadrature, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_circular_domains, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_circular_rendering, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_circular_independent_inverses, tests, NULL);
    lab_math_reset();
}

/* Run the documented branch-sensitive pairs in the final README phase. */
void test_lab_math_laplace_circular_readme_cases(void)
{
    TEST_RUN_IN_GROUP(test_fourier_folded_readme, readme_examples, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_folded_readme, readme_examples, NULL);
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_laplace_circular_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
