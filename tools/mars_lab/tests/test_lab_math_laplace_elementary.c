/**
 * @file test_lab_math_laplace_elementary.c
 * @brief Elementary Laplace regression port with independent numerical integrals.
 *
 * Preserves test_laplace_elementary.py, including complex targets, symbolic
 * guards, staircase sums, periodic logarithms and smoothed branch-cut integrals.
 * Quadrature interval counts and decimal-place thresholds match the originals.
 */
#include <math.h>

#include "test_lab_math_support.h"

typedef struct {
    double complex target;
    double complex rate;
    double offset;
    double sign;
    double divisor;
    unsigned order;
    double complex (*function)(double complex);
} lab_math_elementary_context_t;

static double complex lab_math_elementary_sech(double complex x) { return 1 / ccosh(x); }
static double complex lab_math_elementary_acot(double complex x) { return acos(-1) / 2 - catan(x); }

static const json_t *lab_math_elementary_fields(const char *source)
{
    return lab_math_fields(source, "s", "evaluate", 40);
}

static void lab_math_elementary_components(double complex actual, double complex expected, unsigned places)
{
    lab_math_places(creal(actual), creal(expected), places);
    lab_math_places(cimag(actual), cimag(expected), places);
}

static const json_t *lab_math_elementary_transform(const char *operand, const char *target, double complex expected,
                                                  const char *constants, unsigned places)
{
    const char *source = lab_math_format("{Laplace(%s,t,s) | s=%s%s%s}", operand, target,
                                         constants && *constants ? "; " : "", constants ? constants : "");
    const json_t *result = lab_math_elementary_fields(source);
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    lab_math_elementary_components(lab_math_number(result, "value"), expected, places);
    return result;
}

static double complex lab_math_elementary_integrand(double t, void *opaque)
{
    lab_math_elementary_context_t *context = opaque;
    double complex f = context->function(context->rate * t + context->offset);
    /* divisor==0 selects the ordinary function; otherwise use (1+sign*f)/divisor. */
    double complex source = context->divisor ? (1 + context->sign * f) / context->divisor : cpow(f, context->order);
    return cexp(-context->target * t) * source;
}

static double complex lab_math_elementary_integral(double complex (*function)(double complex), double complex rate,
                                                  double offset, unsigned order, double complex target, double end)
{
    lab_math_elementary_context_t context = {target, rate, offset, 1, 0, order, function};
    return lab_math_simpson(lab_math_elementary_integrand, &context, 0, end, 20000);
}

static void test_laplace_elementary_circular(void)
{
    const char *names[] = {"versin", "vercos", "coversin", "covercos", "haversin", "havercos", "hacoversin", "hacovercos"};
    for (size_t i = 0; i < 8; ++i) {
        lab_math_elementary_context_t context = {2, 2, 0.3, i % 2 ? 1 : -1, i < 4 ? 1 : 2, 1,
                                                 i % 4 < 2 ? ccos : csin};
        double complex expected = lab_math_simpson(lab_math_elementary_integrand, &context, 0, 32, 20000);
        lab_math_elementary_transform(lab_math_format("%s(2*t+3/10)", names[i]), "2", expected, "", 9);
    }
    const json_t *result = lab_math_elementary_fields("Laplace(versin(c*t),t,s)");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    lab_math_contains(lab_math_text(result, "expression"), "Re(s)", true);
    lab_math_elementary_context_t context = {3, 1 + I, 0, -1, 1, 1, ccos};
    double complex expected = lab_math_simpson(lab_math_elementary_integrand, &context, 0, 32, 20000);
    const json_t *copy = lab_math_elementary_fields(lab_math_format("{%s | s=3; c=1+i}", lab_math_text(result, "unbound")));
    lab_math_elementary_components(lab_math_number(copy, "value"), expected, 9);
    copy = lab_math_elementary_fields(lab_math_format("{%s | s=1/2; c=1+i}", lab_math_text(result, "unbound")));
    lab_math_equal(lab_math_text(copy, "value"), "NAN");
}

static void test_laplace_elementary_sech(void)
{
    const json_t *result = lab_math_elementary_fields("Laplace(sech(t),t,s)");
    lab_math_contains(lab_math_text(result, "function"), "abs(1)", false);
    const int rates[] = {1, -1, 2, -2};
    for (size_t i = 0; i < 4; ++i)
        lab_math_elementary_transform("sech(c*t)", "2", lab_math_elementary_integral(lab_math_elementary_sech, rates[i],
            0, 1, 2, 32),
                                       lab_math_format("c=%d", rates[i]), 9);
    lab_math_elementary_transform("sech(t)", "0", acos(-1) / 2, "", 9);
    lab_math_elementary_transform("sech(t)", "-1/2", lab_math_elementary_integral(lab_math_elementary_sech, 1, 0, 1,
        -0.5, 80), "", 9);
    lab_math_elementary_transform("sech(0*t)", "2", 0.5, "", 9);
    lab_math_equal(lab_math_text(lab_math_elementary_fields("{Laplace(sech(t),t,s) | s=-1}"), "value"), "NAN");
    result = lab_math_elementary_fields("Laplace(sech(c*t),t,s)");
    lab_math_contains(lab_math_text(result, "function"), "digamma(", true);
    lab_math_contains(lab_math_text(result, "function"), "sqrt(", true);
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    lab_math_equal(lab_math_text(lab_math_elementary_fields(lab_math_text(result, "expression")), "unbound"),
        lab_math_text(result, "unbound"));
    const char *complex_rates[] = {"1+i", "-1-i"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *copy = lab_math_elementary_fields(lab_math_format("{%s | s=2; c=%s}", lab_math_text(result,
            "unbound"), complex_rates[i]));
        double complex expected = lab_math_elementary_integral(lab_math_elementary_sech, i ? -1 - I : 1 + I, 0, 1, 2, 32);
        lab_math_elementary_components(lab_math_number(copy, "value"), expected, 9);
    }
    const char *bindings[] = {"s=2; c=i", "s=2; c=0", "s=-2; c=1"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(lab_math_text(lab_math_elementary_fields(lab_math_format("{%s | %s}", lab_math_text(result,
            "unbound"), bindings[i])), "value"), "NAN");
}

static void test_laplace_elementary_affine(void)
{
    const char *operands[] = {"abs((3+4i)*t)", "abs(-2*t-3)", "abs(2*t+3)", "abs(2*t-3)", "abs(3-2*t)"};
    const double expected[] = {1.25, 2, 2, 1 + exp(-3), 1 + exp(-3)};
    for (size_t i = 0; i < 5; ++i)
        lab_math_elementary_transform(operands[i], "2", expected[i], "", 9);
    lab_math_elementary_transform("abs(a*t+b)", "2", 0.5, "a=-2, b=0", 9);
    const json_t *result = lab_math_elementary_fields("Laplace(abs(c*t),t,s)");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    lab_math_contains(lab_math_text(result, "function"), "c", true);
    double complex target = 2 + I, wanted = (1 - 2 * I) / (target * target) + (3 + I) / target;
    lab_math_elementary_transform("conj((1+2i)*t+3-i)", "2+i", wanted, "", 9);
    result = lab_math_elementary_fields("Laplace(conj(a*t+b),t,s)");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    const json_t *copy = lab_math_elementary_fields(lab_math_format("{%s | s=2+i; a=1+2i, b=3-i}", lab_math_text(result,
        "unbound")));
    lab_math_elementary_components(lab_math_number(copy, "value"), wanted, 12);
}

static void test_laplace_elementary_staircases(void)
{
    const int rates[] = {1, -1, 2, -2};
    const char *names[] = {"floor", "ceil"};
    for (size_t r = 0; r < 4; ++r) {
        for (size_t n = 0; n < 2; ++n) {
            double width = 1.0 / fabs((double)rates[r]), expected = 0;
            for (unsigned k = 0; k < 120; ++k) {
                double lower = (double)k * width, upper = ((double)k + 1) * width;
                double x = rates[r] * (k + 0.5) * width;
                expected += (n ? ceil(x) : floor(x)) * (exp(-2 * lower) - exp(-2 * upper)) / 2;
            }
            lab_math_elementary_transform(lab_math_format("%s(c*t)", names[n]), "2", expected, lab_math_format("c=%d",
                rates[r]), 12);
        }
    }
    for (size_t n = 0; n < 2; ++n) {
        lab_math_elementary_transform(lab_math_format("%s(0*t)", names[n]), "-2", 0, "", 9);
        const json_t *result = lab_math_elementary_fields(lab_math_format("{Laplace(%s(t),t,s) | s=0}", names[n]));
        lab_math_equal(lab_math_text(result, "value"), "NAN");
    }
}

static void test_laplace_elementary_inverse_functions(void)
{
    const int rates[] = {1, -1, 2, -2};
    const char *targets[] = {"2", "2+i", "2-i"}, *names[] = {"atan", "acot", "asinh"};
    const double complex values[] = {2, 2 + I, 2 - I};
    double complex (*functions[])(double complex) = {catan, lab_math_elementary_acot, casinh};
    for (size_t r = 0; r < 4; ++r) {
        for (size_t s = 0; s < 3; ++s) {
            for (size_t f = 0; f < 3; ++f) {
                double complex expected = lab_math_elementary_integral(functions[f], rates[r], 0, 1, values[s], 32);
                const json_t *result = lab_math_elementary_transform(lab_math_format("%s(c*t)", names[f]), targets[s], expected,
                                                                    lab_math_format("c=%d", rates[r]), 9);
                if (f == 2 && s) {
                    lab_math_contains(lab_math_text(result, "function"), "bessely(", true);
                    lab_math_contains(lab_math_text(result, "function"), "struveh(", true);
                    lab_math_close(lab_math_number(result, "value"), expected, 2e-10);
                }
            }
        }
    }
    lab_math_elementary_transform("atan(0*t)", "-2", 0, "", 9);
    lab_math_elementary_transform("acot(0*t)", "2", acos(-1) / 4, "", 9);
    lab_math_elementary_transform("asinh(0*t)", "-2", 0, "", 9);
    lab_math_equal(lab_math_text(lab_math_elementary_fields("{Laplace(asinh(t),t,s) | s=0}"), "value"), "NAN");
}

static void test_laplace_elementary_logs_and_powers(void)
{
    const char *names[] = {"ln", "log", "lg", "log10"}, *targets[] = {"2", "2+i"};
    const double complex values[] = {2, 2 + I};
    for (size_t n = 0; n < 4; ++n) {
        double divisor = n ? log(10) : 1;
        for (size_t s = 0; s < 2; ++s)
            lab_math_elementary_transform(lab_math_format("%s(a*t+b)", names[n]), targets[s],
                                           lab_math_elementary_integral(clog, 2, 3, 1, values[s], 32) / divisor, "a=2, b=3", 9);
        lab_math_elementary_transform(lab_math_format("%s(2*t)", names[n]), "3",
                                       (log(2) - 0.5772156649015328606 - log(3)) / (3 * divisor), "", 9);
        lab_math_elementary_transform(lab_math_format("%s(0*t+3)", names[n]), "2", log(3) / (2 * divisor), "", 9);
    }
    lab_math_equal(lab_math_text(lab_math_elementary_fields("{Laplace(ln(2*t+3),t,s) | s=0}"), "value"), "NAN");
    const char *hyperbolic[] = {"sinh", "cosh"};
    double complex (*functions[])(double complex) = {csinh, ccosh};
    for (size_t f = 0; f < 2; ++f) {
        for (unsigned order = 2; order <= 4; ++order)
            lab_math_elementary_transform(lab_math_format("%s(t+1/4)^%u", hyperbolic[f], order), lab_math_format("%u", order + 2),
                                           lab_math_elementary_integral(functions[f], 1, 0.25, order, order + 2, 24), "", 9);
    }
    const unsigned orders[] = {5, 8, 16};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *result = lab_math_elementary_transform(lab_math_format("sinh(t)^%u", orders[i]),
            lab_math_format("%u", orders[i] + 2),
                                                             1 / ((orders[i] + 1) * pow(2, orders[i] + 1)), "", 12);
        lab_math_contains(lab_math_text(result, "function"), "sum(", true);
    }
    lab_math_elementary_transform("cosh(t+1/4)^5", "7", lab_math_elementary_integral(ccosh, 1, 0.25, 5, 7, 24), "", 9);
    const json_t *result = lab_math_elementary_fields("Laplace(cosh(c*t)^2,t,s)");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    const json_t *copy = lab_math_elementary_fields(lab_math_format("{%s | s=3; c=-1}", lab_math_text(result, "unbound")));
    lab_math_places(creal(lab_math_number(copy, "value")), 7.0 / 15, 12);
    copy = lab_math_elementary_fields(lab_math_format("{%s | s=1; c=1}", lab_math_text(result, "unbound")));
    lab_math_equal(lab_math_text(copy, "value"), "NAN");
    const char *unsupported[] = {"floor(c*t)", "ceil(c*t)", "floor(i*t)", "ceil(i*t)", "atan(c*t)", "acot(c*t)",
                                 "atan(i*t)", "acot(i*t)", "asinh(c*t)", "asinh(i*t)", "asinh(t+1)", "abs(c*t+d)",
                                 "abs(t+i)", "sech(t+1)", "ln(c*t+1)", "ln(1-t)", "ln(t+i)", "sinh(t)^(1/2)",
                                 "cosh(t)^n", "sinh(t^2)^2"};
    for (size_t i = 0; i < 20; ++i)
        lab_math_contains(lab_math_text(lab_math_elementary_fields(lab_math_format("Laplace(%s,t,s)", unsupported[i])),
            "function"), "laplace(", true);
}

typedef struct {
    double rate;
    double magnitude;
    double complex target;
    unsigned kind;
    bool exterior;
    bool derivative;
} lab_math_cut_context_t;

static double complex lab_math_cut_integrand(double u, void *opaque)
{
    lab_math_cut_context_t *c = opaque;
    if (!u)
        return 0;
    double pi = acos(-1), x = c->exterior ? 1 + u * u : 1 - u * u;
    double q = c->kind == 1 ? c->magnitude / c->rate : c->kind == 2 ? 1 / fabs(c->rate) : 1;
    double t = q * x;
    double complex source;
    if (c->kind == 1)
        source = log(c->magnitude * u * u) + (c->exterior ? 0 : I * pi);
    else if (c->kind == 2)
        source = copysign(1, c->rate) * log((1 + x) / (u * u)) / 2 + (c->exterior ? I * pi / 2 : 0);
    else
        source = (log(1 + t) - 2 * log(u)) / 2 - (c->exterior ? 0 : I * pi / 2);
    return 2 * q * u * cexp(-c->target * t) * source * (c->derivative ? -t : 1);
}

static double complex lab_math_cut_integral(lab_math_cut_context_t *context, double end)
{
    context->exterior = false;
    double complex result = lab_math_simpson(lab_math_cut_integrand, context, 0, 1, 40000);
    context->exterior = true;
    return result + lab_math_simpson(lab_math_cut_integrand, context, 0, end, 40000);
}

static void test_laplace_elementary_cuts(void)
{
    const char *targets[] = {"1", "1+i", "1-i"};
    const double complex values[] = {1, 1 + I, 1 - I};
    for (size_t s = 0; s < 3; ++s) {
        lab_math_cut_context_t context = {1, 1, values[s], 0, false, true};
        double complex expected = lab_math_cut_integral(&context, sqrt(40));
        const json_t *result = lab_math_elementary_fields(lab_math_format("{ Ds(@L{@S^t 1/(1-x^2) dx}) | s=%s }", targets[s]));
        lab_math_contains(lab_math_text(result, "function"), "derivative(", false);
        lab_math_places(lab_math_number(result, "value"), expected, 6);
    }
    const char *log_names[] = {"ln", "log", "lg", "log10"}, *cut_targets[] = {"1", "2+i", "1-2i"};
    const double rates[] = {1, 2, 3}, magnitudes[] = {1, 3, 2};
    const double complex cut_values[] = {1, 2 + I, 1 - 2 * I};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_cut_context_t context = {rates[i], magnitudes[i], cut_values[i], 1, false, false};
        double complex expected = lab_math_cut_integral(&context, sqrt(40 * rates[i] / magnitudes[i]));
        for (size_t n = 0; n < 4; ++n) {
            const json_t *result = lab_math_elementary_transform(lab_math_format("%s(%g*t-%g)", log_names[n], rates[i],
                magnitudes[i]),
                                                                 cut_targets[i], expected / (n ? log(10) : 1), "", 6);
            lab_math_contains(lab_math_text(result, "expression"), "Re(s) > 0", true);
        }
    }
    const char *invalid[] = {"0", "-1", "i"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(lab_math_text(lab_math_elementary_fields(lab_math_format("{Laplace(ln(t-1),t,s) | s=%s}",
            invalid[i])), "value"), "NAN");
    lab_math_elementary_transform("ln(-2)", "1+i", (log(2) + I * acos(-1)) / (1 + I), "", 9);
    const double atanh_rates[] = {1, -1, 2, -2};
    const double complex atanh_values[] = {1, 1, 2, 1 + I};
    const char *atanh_targets[] = {"1", "1", "2", "1+i"};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_cut_context_t context = {atanh_rates[i], 1, atanh_values[i], 2, false, false};
        double complex expected = lab_math_cut_integral(&context, sqrt(40 * fabs(atanh_rates[i])));
        lab_math_elementary_transform(lab_math_format("atanh(%g*t)", atanh_rates[i]), atanh_targets[i], expected, "", 6);
        context.derivative = true;
        expected = lab_math_cut_integral(&context, sqrt(40 * fabs(atanh_rates[i])));
        const json_t *result = lab_math_fields(lab_math_format("{@L(atanh(%g*t)) | s=%s}", atanh_rates[i],
            atanh_targets[i]), "s", "derivative", 40);
        lab_math_elementary_components(lab_math_number(result, "derivative_value"), expected, 6);
    }
    const json_t *result = lab_math_elementary_fields("@L{atanh(ct)}");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    lab_math_contains(lab_math_text(result, "function"), "realpart(c) == c", true);
    lab_math_contains(lab_math_text(result, "tex"), "\\in\\mathbb{R}", true);
    lab_math_contains(lab_math_text(result, "tex"), "\\frac{1}{2\\mkern-2mu s}\\,\\left[", true);
    lab_math_contains(lab_math_text(result, "tex"), "\\right]\\quad", true);
    for (int rate = -1; rate <= 1; rate += 2) {
        const json_t *copy = lab_math_elementary_fields(lab_math_format("{%s | s=1; c=%d}", lab_math_text(result,
            "unbound"), rate));
        const json_t *direct = lab_math_elementary_fields(lab_math_format("{@L(atanh(%d*t)) | s=1}", rate));
        lab_math_places(lab_math_number(copy, "value"), lab_math_number(direct, "value"), 12);
    }
    lab_math_equal(lab_math_text(lab_math_elementary_fields(lab_math_format("{%s | s=1; c=i}", lab_math_text(result,
        "unbound"))), "value"), "NAN");
    lab_math_elementary_transform("atanh(0*t)", "-1", 0, "", 9);
    lab_math_contains(lab_math_text(lab_math_elementary_fields("@L(atanh(i*t))"), "function"), "laplace(", true);
}

typedef struct {
    double left;
    double right;
    double rate;
    double complex target;
    bool sine;
    bool absolute;
} lab_math_periodic_context_t;

static double complex lab_math_periodic_integrand(double u, void *opaque)
{
    if (u == 0 || u == 1)
        return 0;
    lab_math_periodic_context_t *c = opaque;
    double pi = acos(-1), t = c->left + (c->right - c->left) * (1 - cos(pi * u)) / 2;
    double jacobian = (c->right - c->left) * pi * sin(pi * u) / 2;
    double wave = c->sine ? sin(c->rate * t) : cos(c->rate * t);
    double complex value = log(fabs(wave)) + (!c->absolute && wave < 0 ? I * pi : 0);
    return jacobian * cexp(-c->target * t) * value;
}

static void test_laplace_elementary_periodic_logs(void)
{
    const double rates[] = {1, -2, 3};
    const double complex values[] = {acos(-1), 1 + 2 * I, 0.5 - I};
    const char *targets[] = {"pi", "1+2i", "0.5-i"}, *names[] = {"ln", "log", "lg", "log10"};
    for (unsigned sine = 0; sine < 2; ++sine) {
        const double cuts_cos[] = {0, 0.5, 1.5, 2}, cuts_sin[] = {0, 1, 2};
        const double *cuts = sine ? cuts_sin : cuts_cos;
        for (size_t r = 0; r < 3; ++r) {
            for (unsigned absolute = 0; absolute < 2; ++absolute) {
                double complex expected = 0;
                for (size_t interval = 0; interval < (sine ? 2u : 3u); ++interval) {
                    lab_math_periodic_context_t context = {cuts[interval] * acos(-1) / fabs(rates[r]),
                                                           cuts[interval + 1] * acos(-1) / fabs(rates[r]),
                                                           rates[r], values[r], sine != 0, absolute != 0};
                    expected += lab_math_simpson(lab_math_periodic_integrand, &context, 0, 1, 20000);
                }
                expected /= 1 - cexp(-values[r] * 2 * acos(-1) / fabs(rates[r]));
                const char *argument = lab_math_format("%s(%g*t)", sine ? "sin" : "cos", rates[r]);
                if (absolute)
                    argument = lab_math_format("abs(%s)", argument);
                for (size_t n = 0; n < 4; ++n) {
                    const json_t *result = lab_math_elementary_transform(lab_math_format("%s(%s)", names[n], argument),
                        targets[r],
                                                                         expected / (n ? log(10) : 1), "", 6);
                    lab_math_contains(lab_math_text(result, "expression"), "Re(s) > 0", true);
                }
            }
        }
    }
    const char *operands[] = {"ln(cos(t))", "ln(sin(t))", "ln(abs(cos(t)))", "ln(abs(sin(t)))"};
    const char *invalid[] = {"0", "-1", "i"};
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 3; ++j)
            lab_math_equal(lab_math_text(lab_math_elementary_fields(lab_math_format("{Laplace(%s,t,s) | s=%s}",
                operands[i], invalid[j])), "value"), "NAN");
    }
    const char *unsupported[] = {"ln(cos(c*t))", "ln(cos(i*t))", "ln(cos(t+1))"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(lab_math_text(lab_math_elementary_fields(lab_math_format("Laplace(%s,t,s)", unsupported[i])),
            "function"), "laplace(", true);
}

/* Register every elementary forward transform regression and independent oracle. */
void test_lab_math_laplace_elementary_cases(void)
{
    TEST_RUN_IN_GROUP(test_laplace_elementary_circular, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_elementary_sech, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_elementary_affine, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_elementary_staircases, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_elementary_inverse_functions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_elementary_logs_and_powers, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_elementary_cuts, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_elementary_periodic_logs, tests, NULL);
    lab_math_reset();
}
