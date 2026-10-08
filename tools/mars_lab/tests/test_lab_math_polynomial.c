/**
 * @file test_lab_math_polynomial.c
 * @brief Polynomial conventions and independently normalised convolution regressions.
 *
 * Ports every case of test_orthopoly_convolution.py. Independent recurrences,
 * series and Fourier coefficients retain the original sample grids and tolerances.
 * README examples are exposed separately for final-phase execution.
 */
#include <math.h>

#include "test_lab_math_support.h"

static const json_t *lab_math_polynomial_fields(const char *source, const char *variable)
{
    return lab_math_fields(source, variable, "evaluate", 40);
}

static double complex lab_math_polynomial_value(const char *source, const char *variable)
{
    return lab_math_number(lab_math_polynomial_fields(source, variable), "value");
}

static double lab_math_polynomial_hermite(unsigned n, double x)
{
    double previous = 1, current = 2 * x;
    if (!n)
        return previous;
    for (unsigned k = 1; k < n; ++k) {
        double next = 2 * x * current - 2 * k * previous;
        previous = current;
        current = next;
    }
    return current;
}

static void test_polynomial_names(void)
{
    const char *chebyshev[] = {"Tn", "chebyshev_t", "ChebyshevT"};
    const char *hermites[] = {"ℋ", "hermite_h", "HermiteH"};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_check(lab_math_polynomial_value(lab_math_format("%s(3,2)", chebyshev[i]), "x") == 26, chebyshev[i]);
        lab_math_check(lab_math_polynomial_value(lab_math_format("%s(3,2)", hermites[i]), "x") == 40, hermites[i]);
        lab_math_check(lab_math_polynomial_value(lab_math_format("{x | x=%s(3,2)}", hermites[i]), "x") == 40, hermites[i]);
    }
    lab_math_check(lab_math_polynomial_value("{x | x=Tn(3,2)}", "x") == 26, "polynomial binding");
    const char *indexed[] = {"T_3(2)", "T₃(2)", "ℋ_3(2)", "ℋ₃(2)"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_check(lab_math_polynomial_value(indexed[i], "x") == (i < 2 ? 26 : 40), indexed[i]);
    lab_math_places(creal(lab_math_polynomial_value("Hn(3,2)", "x")), 20.0 / 3, 7);
    lab_math_contains(lab_math_text(lab_math_polynomial_fields("ℋ(n,x)", "x"), "tex"), "\\mathcal{H}", true);
    const char *sources[] = {"Tn(n,x)", "Un(n,x)", "ℋ(n,x)"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *result = lab_math_polynomial_fields(sources[i], "x");
        lab_math_equal(lab_math_text(lab_math_polynomial_fields(lab_math_text(result, "expression"), "x"), "tex"),
            lab_math_text(result, "tex"));
    }
}

static void test_polynomial_calculus(void)
{
    const double points[] = {-1, -0.3, 0, 1, 2};
    for (unsigned n = 0; n < 7; ++n) {
        for (size_t i = 0; i < 5; ++i) {
            double x = points[i];
            lab_math_places(creal(lab_math_polynomial_value(lab_math_format("ℋ(%u,%.17g)", n, x), "x")),
                lab_math_polynomial_hermite(n, x), 10);
            double expected = fabs(x) <= 1 ? cos(n * acos(x)) : cosh(n * acosh(x));
            lab_math_places(creal(lab_math_polynomial_value(lab_math_format("Tn(%u,%.17g)", n, x), "x")), expected, 9);
        }
    }
    lab_math_check(lab_math_polynomial_value("ℋ(3,i)", "x") == -20 * I, "complex Hermite polynomial");
    const char *sources[] = {"{Tn(3,x)|x=2}", "{Un(3,x)|x=1}", "{ℋ(3,x)|x=2}", "{ℋ(0,x)|x=0}",
                             "{x+Un(n,x+j)|x=1; n=3; j=0}"};
    const double expected[] = {45, 20, 84, 0, 21};
    for (size_t i = 0; i < 5; ++i)
        lab_math_places(lab_math_number(lab_math_fields(sources[i], "x", "derivative", 40), "derivative_value"), expected[i], 7);
    const char *primitives[] = {"Tn(3,x)", "Un(3,x)", "ℋ(3,x)"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(lab_math_text(lab_math_fields(primitives[i], "x", "integral", 40), "integral"), "No integral", false);
    lab_math_check(lab_math_polynomial_value("sum(k,0,3,Tn(k,2))", "x") == 36, "Chebyshev finite sum");
    lab_math_check(lab_math_polynomial_value("sum(k,0,3,ℋ(k,2))", "x") == 59, "Hermite finite sum");
}

static void test_polynomial_fourier(void)
{
    const double pi = acos(-1);
    const unsigned orders[] = {0, 1, 3, 6};
    const int rates[] = {1, 2, -2};
    const double points[] = {-0.6, 0, 1.1};
    for (unsigned inverse = 0; inverse < 2; ++inverse) {
        const char *operator = inverse ? "@Finv" : "@F", *source = inverse ? "ω" : "t", *target = inverse ? "t" : "ω";
        for (size_t n = 0; n < 4; ++n) {
            for (size_t a = 0; a < 3; ++a) {
                for (size_t p = 0; p < 3; ++p) {
                    const char *body = lab_math_format("ℋ(n,(%d)*%s)*exp(-((%d)*%s)^2/2)", rates[a], source, rates[a], source);
                    const char *input = lab_math_format("{%s{%s} | n=%u; %s=%.17g}", operator, body, orders[n], target,
                        points[p]);
                    double complex coefficient = inverse ? cpow(I, orders[n]) / sqrt(2 * pi)
                                                          : sqrt(2 * pi) * cpow(-I, orders[n]);
                    double x = points[p] / rates[a];
                    double complex expected = coefficient / fabs((double)rates[a]) *
                        lab_math_polynomial_hermite(orders[n], x) * exp(-x * x / 2);
                    lab_math_close(lab_math_polynomial_value(input, target), expected, 1e-9);
                }
            }
        }
        for (size_t n = 0; n < 3; ++n) {
            const char *input = lab_math_format("%s{Tn(%u,%s)*rect(%s/2)/sqrt(1-%s^2)}", operator, orders[n], source,
                source, source);
            lab_math_contains(lab_math_text(lab_math_polynomial_fields(input, target), "function"), "fourier(", false);
            double bessel = 0;
            for (unsigned k = 0; k < 18; ++k)
                bessel += (k % 2 ? -1 : 1) * pow(0.35, 2 * k + orders[n]) / (tgamma(k + 1) * tgamma(k + orders[n] + 1));
            double complex expected = (inverse ? cpow(I, orders[n]) / 2 : pi * cpow(-I, orders[n])) * bessel;
            lab_math_close(lab_math_polynomial_value(lab_math_format("{%s | %s=0.7}", input, target), target), expected, 1e-10);
        }
    }
    const json_t *result = lab_math_polynomial_fields("@F{ℋ(n,t)*exp(-t^2/2)}", "ω");
    lab_math_contains(lab_math_text(result, "function"), "�", false);
    lab_math_equal(lab_math_text(lab_math_polynomial_fields(lab_math_text(result, "expression"), "ω"), "tex"),
        lab_math_text(result, "tex"));
    result = lab_math_polynomial_fields("@F{J_n(t)}", "ω");
    lab_math_contains(lab_math_text(result, "expression"), "Tn(", true);
    lab_math_contains(lab_math_text(result, "expression"), "acos", false);
}

static void test_polynomial_convolution(void)
{
    const double pi = acos(-1), points[] = {-1, 0, 0.5};
    for (unsigned inverse = 0; inverse < 2; ++inverse) {
        const char *operator = inverse ? "@Finv" : "@F", *source = inverse ? "ω" : "t", *target = inverse ? "t" : "ω";
        const char *input = lab_math_format("%s{convolve(exp(-%s^2),exp(-%s^2),%s)}", operator, source, source, source);
        for (size_t p = 0; p < 3; ++p)
            lab_math_places(creal(lab_math_polynomial_value(lab_math_format("{%s | %s=%.17g}", input, target,
                points[p]), target)),
                            (inverse ? 0.5 : pi) * exp(-points[p] * points[p] / 2), 11);
    }
    const char *copies[] = {"@F{convolve(f(t),g(t),t)}", "@F{f(t)*g(t)}", "@Finv{f(ω)*g(ω)}",
                            "@L{causal_convolve(f(t),g(t),t)}", "@Linv{F(s)*G(s)}"};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *result = lab_math_polynomial_fields(copies[i], "x");
        lab_math_equal(lab_math_text(lab_math_polynomial_fields(lab_math_text(result, "expression"), "x"), "tex"),
            lab_math_text(result, "tex"));
    }
    const json_t *result = lab_math_polynomial_fields("@Finv{f(ω)*g(ω)}", "x");
    lab_math_contains(lab_math_text(result, "function"), "convolve(", true);
    lab_math_contains(lab_math_text(result, "function"), "@pi", false);
    const double frequencies[] = {0.5, 1, 3}, times[] = {-1.2, 0, 0.2, 1};
    for (size_t i = 0; i < 3; ++i)
        lab_math_places(creal(lab_math_polynomial_value(lab_math_format("{@L{causal_convolve(t,t,t)} | s=%.17g}",
            frequencies[i]), "s")),
                        1 / pow(frequencies[i], 4), 10);
    for (size_t i = 0; i < 4; ++i) {
        lab_math_places(creal(lab_math_polynomial_value(lab_math_format("{convolve(rect(t),rect(t),t) | t=%.17g}",
            times[i]), "t")),
                        fmax(0, 1 - fabs(times[i])), 12);
        lab_math_places(creal(lab_math_polynomial_value(lab_math_format("{convolve(delta(t-1),sin(t),t) | t=%.17g}",
            times[i]), "t")),
                        sin(times[i] - 1), 12);
    }
    lab_math_places(creal(lab_math_polynomial_value("{causal_convolve(t,t,t)|t=2}", "t")), 4.0 / 3, 7);
    lab_math_places(creal(lab_math_polynomial_value("{causal_convolve(t+τ,t,t)|t=2; τ=3}", "t")), 22.0 / 3, 7);
    lab_math_places(creal(lab_math_polynomial_value("{convolve(exp(-t^2),exp(-t^2),t)|t=1}", "t")), sqrt(pi / 2) * exp(-0.5), 12);
    lab_math_text(lab_math_fields("convolve(f(t),g(t),t)", "t", "derivative", 40), "derivative");
}

static void test_polynomial_readme(void)
{
    /* README examples: docs/expression.md, polynomial and convolution examples. */
    const char *sources[] = {"Tn(3,2)", "Un(3,2)", "ℋ(3,2)", "sum(k,0,3,Tn(k,2))", "sum(k,0,3,ℋ(k,2))",
                             "{convolve(rect(t),rect(t),t)|t=1/4}", "{causal_convolve(t,t,t)|t=2}",
                             "{@L{causal_convolve(t,t,t)}|s=2}"};
    const double expected[] = {26, 56, 40, 36, 59, 0.75, 4.0 / 3, 1.0 / 16};
    for (size_t i = 0; i < 8; ++i)
        lab_math_places(lab_math_polynomial_value(sources[i], "t"), expected[i], 12);
}

/* Register polynomial and convolution regressions with independent references. */
void test_lab_math_polynomial_cases(void)
{
    TEST_RUN_IN_GROUP(test_polynomial_names, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_polynomial_calculus, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_polynomial_fourier, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_polynomial_convolution, tests, NULL);
    lab_math_reset();
}

/* Run polynomial/convolution documentation examples after ordinary tests. */
void test_lab_math_polynomial_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_polynomial_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
