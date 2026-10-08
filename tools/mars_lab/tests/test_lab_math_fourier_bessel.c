/**
 * @file test_lab_math_fourier_bessel.c
 * @brief Indexed-Bessel subset of the complete general Fourier regression port.
 *
 * Preserves every original case, parameter grid, reference calculation and
 * tolerance. README examples are registered separately after ordinary tests.
 */
#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "test_lab_math_support.h"

static const json_t *math_fourier_fields(const char *source, const char *variable)
{
    return lab_math_fields(source, variable, "evaluate", 40);
}

static const char *math_fourier_text(const json_t *result, const char *key)
{
    return lab_math_text(result, key);
}

static double complex math_fourier_number(const json_t *result)
{
    return lab_math_number(result, "value");
}


static double math_fourier_bessel_series(int order, double point, int terms)
{
    double total = 0;
    for (int k = 0; k < terms; ++k)
        total += (k % 2 ? -1 : 1)*pow(point/2, 2*k+order)/(tgamma(k+1)*tgamma(k+order+1));
    return total;
}

static void test_bessel_indexed_parser_and_bindings(void)
{
    const char *cases[][2] = {{"J_n(x)", "bessel_j(n,x)"}, {"J_{n+1}(x)", "bessel_j(n+1,x)"},
        {"J_3(x)", "bessel_j(3,x)"}, {"J₃(x)", "bessel_j(3,x)"}, {"J₋₃(x)", "bessel_j(-3,x)"},
        {"J_-3(x)", "bessel_j(-3,x)"}, {"J3(x)", "bessel_j(3,x)"}, {"Y_n(x)", "bessel_y(n,x)"},
        {"Y_{n+1}(x)", "bessel_y(n+1,x)"}, {"Y₂(x)", "bessel_y(2,x)"}};
    for (size_t i = 0; i < 10; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(cases[i][0], "x"), "tex"), math_fourier_text(
            math_fourier_fields(cases[i][1], "x"), "tex"));
    const json_t *r = math_fourier_fields("@F{J_n(x)}", "k");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "function"), "const n", true);
    lab_math_contains(math_fourier_text(r, "function"), "const J", false);
    lab_math_contains(math_fourier_text(r, "function"), "x = ?", false);
    lab_math_contains(math_fourier_text(r, "expression"), "k", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F(J_n(x),x,ω)", "ω"), "expression"), "ω", true);
    const char *sources[] = {"@F{J_n(x)}", "@Finv{J_n(ω)}"};
    for (size_t i = 0; i < 2; ++i) {
        r = math_fourier_fields(sources[i], "ω");
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "ω"), "tex"),
            math_fourier_text(r, "tex"));
    }
}

static void test_bessel_fourier_integer_orders_both_directions(void)
{
    const int orders[] = {0, 1, 2, 3, -1, -3};
    const double points[] = {-1.5, -0.7, 0.2, 0.6, 1.5};
    for (size_t n = 0; n < 6; ++n)
        for (size_t d = 0; d < 2; ++d)
            for (size_t p = 0; p < 5; ++p) {
                const char *target = d ? "t" : "k";
                const json_t *r = math_fourier_fields(lab_math_format("{%s{J_n(%s)} | %s=%.17g; n=%d}",
                    d ? "@Finv" : "@F", d ? "ω" : "x", target, points[p], orders[n]), target);
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                double complex factor = d ? cpow(I, orders[n])/M_PI : 2*cpow(-I, orders[n]);
                double x = points[p];
                double complex expected = fabs(x) < 1 ? factor*cos(orders[n]*acos(x))/sqrt(1-x*x) : 0;
                lab_math_close(math_fourier_number(r), expected, 1e-11);
            }
    for (int edge = -1; edge <= 1; edge += 2) {
        const json_t *r = math_fourier_fields(lab_math_format("{@F{J_n(x)} | k=%d; n=3}", edge), "k");
        lab_math_check(isnan(creal(math_fourier_number(r))), "Bessel spectral endpoint is singular");
        lab_math_contains(math_fourier_text(r, "value_note"), "singularities", true);
    }
    const char *orders_bad[] = {"1/2", "i"};
    for (size_t i = 0; i < 2; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format("@F{J_{%s}(x)}", orders_bad[i]), "k"),
            "function"), "fourier(", true);
}

static void test_bessel_specialised_order_simplifies_absolute_value(void)
{
    for (size_t d = 0; d < 2; ++d) {
        const char *op = d ? "@Finv" : "@F", *source = d ? "ω" : "x", *target = d ? "t" : "k";
        for (int order = 3; order >= -3; order -= 6) {
            const json_t *r = math_fourier_fields(lab_math_format("{%s{J_n(%s)} | %s=?; n=%d}", op, source, target,
                order), target);
            lab_math_contains(math_fourier_text(r, "tex"), "T_{3}", true);
            lab_math_contains(math_fourier_text(r, "expression"), "Tn(3,", true);
            lab_math_contains(math_fourier_text(r, "function"), "chebyshev_t(3,", true);
            lab_math_contains(math_fourier_text(r, "transform_identity_TeX"), "T_{\\left|", false);
        }
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format("%s{J_n(%s)}", op, source), target),
            "tex"), "T_{\\left|n\\right|}", true);
    }
    const char *cases[][2] = {{"3", "3"}, {"-3", "3"}, {"0", "0"}, {"-0.75", "3/4"}, {"-3/2", "3/2"}};
    for (size_t i = 0; i < 5; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("abs(%s)", cases[i][0]), "ω"), "tex"),
            math_fourier_text(math_fourier_fields(cases[i][1], "ω"), "tex"));
    lab_math_contains(math_fourier_text(math_fourier_fields("x+abs(a)", "x"), "tex"), "\\left|a\\right|", true);
}

static void test_scaled_chebyshev_spectrum_inverse(void)
{
    const char *forms[] = {"-2i*Tn(5,ω)*rect(ω/2)/sqrt(1-ω^2)", "-2i·Tn(5, ω)·rect(ω/2)/√(1 - ω^2)",
        "rect(ω/2)*(-2i*Tn(5,ω))/sqrt(1-ω^2)", "Tn(5,ω)*rect(ω/2)/(i*sqrt(1-ω^2)/2)"};
    const double points[] = {-0.8, 0, 0.7};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("@Finv{%s}", forms[i]), "t");
        lab_math_contains(math_fourier_text(r, "function"), "inversefourier(", false);
        lab_math_contains(math_fourier_text(r, "function"), "besselj(5, t)", true);
        lab_math_contains(math_fourier_text(r, "tex"), "J_{5}", true);
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "t"), "tex"),
            math_fourier_text(r, "tex"));
        for (size_t p = 0; p < 3; ++p)
            lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{@Finv{%s} | t=%.17g}", forms[i],
                points[p]), "t")),
                           math_fourier_bessel_series(5, points[p], 18), 1e-12);
    }
}

static void test_scalar_factors_in_fourier_products_and_quotients(void)
{
    const double points[] = {-0.7, 0, 0.6};
    for (size_t d = 0; d < 2; ++d) {
        const char *op = d ? "@Finv" : "@F", *source = d ? "ω" : "t", *target = d ? "t" : "ω";
        const char *bodies[] = {lab_math_format("a*Tn(2,%s)*rect(%s/2)/(b*sqrt(1-%s^2))", source, source, source),
            lab_math_format("6/(2*(1+%s^2))", source), lab_math_format("3*exp(-2*%s)*step(%s)", source, source)};
        double norm = d ? 1/(2*M_PI) : 1, direction = d ? -1 : 1;
        for (size_t c = 0; c < 3; ++c)
            for (size_t p = 0; p < 3; ++p) {
                double x = points[p];
                double complex expected[] = {-(3+2*I)*M_PI/4*norm*math_fourier_bessel_series(2, x, 18),
                    3*M_PI*norm*exp(-fabs(x)), 3*norm/(2+direction*I*x)};
                const json_t *r = math_fourier_fields(lab_math_format("{%s{%s} | %s=%.17g%s}", op, bodies[c], target, x,
                                                        c == 0 ? "; a=3+2i; b=4" : ""), target);
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                lab_math_close(math_fourier_number(r), expected[c], 1e-11);
            }
    }
}

static void test_bessel_affine_scaling(void)
{
    const double points[] = {-0.8, 0.4, 3};
    for (size_t d = 0; d < 2; ++d)
        for (int rate = -2; rate <= 2; rate += 4)
            for (size_t p = 0; p < 3; ++p) {
                double q = points[p]/rate;
                double complex factor = d ? I/M_PI : -2*I;
                double complex expected = fabs(q) < 1 ?
                    factor*q/sqrt(1-q*q)/abs(rate)*cexp((d ? -I : I)*q) : 0;
                const char *target = d ? "t" : "k";
                lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{%s{J_1(%d*%s+1)} | %s=%.17g}",
                    d ? "@Finv" : "@F", rate, d ? "ω" : "x", target, points[p]), target)), expected, 1e-11);
            }
}

static void test_bessel_spectrum_against_independent_inverse_quadrature(void)
{
    const int orders[] = {0, 1, 3, -2};
    for (size_t n = 0; n < 4; ++n) {
        double complex total = 0;
        for (int index = 0; index < 32; ++index) {
            double theta = M_PI*(index+0.5)/32, frequency = cos(theta);
            const json_t *r = math_fourier_fields(lab_math_format("{@F{J_n(x)} | k=%.17g; n=%d}", frequency, orders[n]), "k");
            total += math_fourier_number(r)*cexp(I*0.8*frequency)*sin(theta)/64;
        }
        int degree = abs(orders[n]);
        double expected = math_fourier_bessel_series(degree, 0.8, 20);
        if (orders[n] < 0)
            expected *= degree % 2 ? -1 : 1;
        lab_math_close(total, expected, 1e-11);
    }
}

static void test_readme_bessel_fourier_examples(void)
{
    /* README examples: docs/expression.md, indexed Bessel Fourier transforms. */
    const char *sources[] = {"{@F{J_n(x)} | k=0; n=0}", "{@F{J_n(x)} | k=2; n=3}", "{@F(J_n(x),x,ω) | ω=0; n=0}"};
    const char *expected[] = {"2", "0", "2"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(sources[i], "k"), "value"), expected[i]);
}

/* Register ordinary regressions. */
void test_lab_math_fourier_bessel_cases(void)
{
    TEST_RUN_IN_GROUP(test_bessel_indexed_parser_and_bindings, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_fourier_integer_orders_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_specialised_order_simplifies_absolute_value, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_scaled_chebyshev_spectrum_inverse, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_scalar_factors_in_fourier_products_and_quotients, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_affine_scaling, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bessel_spectrum_against_independent_inverse_quadrature, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_bessel_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_bessel_fourier_examples, readme_examples, "math,readme,output");
    lab_math_reset();
}
