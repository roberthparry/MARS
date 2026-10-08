/**
 * @file test_lab_math_fourier.c
 * @brief General native angular-frequency Fourier regression port.
 *
 * Preserves every original case, parameter grid, reference calculation and
 * tolerance. README examples are registered separately after ordinary tests.
 */
#include <complex.h>
#include <math.h>
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


static void math_fourier_formula(const char *source, const char *variable, const double *points, const double *expected,
    size_t count)
{
    lab_math_contains(math_fourier_text(math_fourier_fields(source, variable), "function"), "fourier(", false);
    for (size_t i = 0; i < count; ++i)
        lab_math_places(math_fourier_number(math_fourier_fields(lab_math_format("{%s | %s=%.17g}", source, variable,
            points[i]), variable)),
                        expected[i], 12);
}

static void math_fourier_symbolic_formula(const char *source, const char *expected, const char *variable)
{
    lab_math_contains(math_fourier_text(math_fourier_fields(source, variable), "function"), "fourier(", false);
    const double points[] = {-1.25, 0, 0.75};
    for (size_t i = 0; i < 3; ++i)
        lab_math_check(creal(math_fourier_number(math_fourier_fields(lab_math_format("{abs((%s)-(%s)) | %s=%.17g}",
            source, expected, variable, points[i]), variable))) < 1e-12, source);
}

static void test_gaussians_both_directions(void)
{
    const double points[] = {0, 0.3, 1.25};
    double forward[3], inverse[3], scaled[3];
    for (size_t i = 0; i < 3; ++i) {
        double x = points[i];
        forward[i] = sqrt(M_PI)*exp(-x*x/4);
        inverse[i] = exp(-x*x/4)/(2*sqrt(M_PI));
        scaled[i] = sqrt(M_PI/2)*exp(3-x*x/8);
    }
    math_fourier_formula("@F{exp(-t^2)}", "ω", points, forward, 3);
    math_fourier_formula("@Finv{exp(-ω^2)}", "t", points, inverse, 3);
    math_fourier_formula("@F{exp(-2*t^2+3)}", "ω", points, scaled, 3);
}

static void test_absolute_half_power_spellings_and_run(void)
{
    const char *expected = math_fourier_text(math_fourier_fields("@F{1/sqrt(|t|)}", "ω"), "unbound");
    lab_math_equal(expected, "√(2π/|ω|) where (ω ∈ ℝ; ω ≠ 0)");
    const char *bodies[] = {"1/sqrt(|t|)", "1/sqrt(abs(t))", "abs(t)^(-1/2)", "1/abs(t)^(1/2)"};
    const double points[] = {-4, -1, 1, 4, 77};
    double values[5];
    for (size_t i = 0; i < 5; ++i)
        values[i] = sqrt(2*M_PI/fabs(points[i]));
    for (size_t b = 0; b < 4; ++b) {
        const char *source = lab_math_format("@F{%s}", bodies[b]);
        lab_math_equal(math_fourier_text(math_fourier_fields(source, "ω"), "unbound"), expected);
        math_fourier_formula(source, "ω", points, values, 5);
    }
    const char *invalid[] = {"0", "i"};
    for (size_t i = 0; i < 2; ++i)
        lab_math_check(isnan(creal(math_fourier_number(math_fourier_fields(lab_math_format("{@F{1/sqrt(|t|)} | ω=%s}",
            invalid[i]), "ω")))),
                       "half-power frequency domain");
    const char *frequencies[] = {"?", "4"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{@F{1/sqrt(|t|)} | ω=%s}", frequencies[i]), "ω");
        const char *output = lab_math_programme(math_fourier_text(r, "operation_function"), 40);
        if (!i) {
            lab_math_contains(output, "ω = NAN", true);
            lab_math_contains(output, "ℱ", false);
        } else {
            lab_math_places(math_fourier_number(math_fourier_fields(output, "ω")), sqrt(M_PI/2), 12);
        }
    }
}

static void test_absolute_power_inverse_and_copied_spectrum(void)
{
    const double points[] = {-3, -1, 1, 3};
    double direct[4], restored[4];
    for (size_t i = 0; i < 4; ++i) {
        direct[i] = 1/sqrt(2*M_PI*fabs(points[i]));
        restored[i] = 1/sqrt(fabs(points[i]));
    }
    math_fourier_formula("@Finv{1/sqrt(|ω|)}", "t", points, direct, 4);
    const char *inverse = lab_math_format("InverseFourier(%s,ω,t)", math_fourier_text(math_fourier_fields(
        "@F{1/sqrt(|t|)}", "ω"), "unbound"));
    lab_math_equal(math_fourier_text(math_fourier_fields(inverse, "t"), "unbound"), "1/√(|t|) where (t ∈ ℝ; t ≠ 0)");
    math_fourier_formula(inverse, "t", points, restored, 4);
}

static void test_radical_quotient_simplification_preserves_branches(void)
{
    const char *cases[][2] = {{"sqrt(2*pi)/sqrt(abs(x))", "√(2π/|x|)"}, {"sqrt(2*x)", "√(2x)"},
        {"sqrt(4*x)", "2·√(x)"}, {"sqrt(x)/sqrt(abs(y))", "√(x/|y|)"}, {"sqrt(x)/sqrt(y)", "√(x)/√(y)"}};
    for (size_t i = 0; i < 5; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(cases[i][0], "x"), "unbound"), cases[i][1]);
    const int points[][2] = {{-1, -1}, {1, -1}, {-1, 1}};
    for (size_t i = 0; i < 3; ++i)
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{sqrt(x)/sqrt(y) | x=%d; y=%d}",
            points[i][0], points[i][1]), "x")),
                       csqrt(CMPLX(points[i][0], 0))/csqrt(CMPLX(points[i][1], 0)), 1e-12);
    const json_t *r = math_fourier_fields("{sqrt(x)/sqrt(abs(y)) | x=-1; y=4}", "x");
    lab_math_equal(math_fourier_text(r, "unbound"), "√(x)/2");
    lab_math_check(math_fourier_number(r) == 0.5*I, "principal radical quotient");
}

static void test_absolute_power_strip_affine_scaling_and_scalar_factors(void)
{
    const double powers[] = {-0.25, -0.5, -0.75}, points[] = {-1.3, 0.7};
    for (size_t n = 0; n < 3; ++n) {
        double power = powers[n], coefficient = 2*tgamma(power+1)*cos(M_PI*(power+1)/2);
        for (size_t d = 0; d < 2; ++d)
            for (int rate = -2; rate <= 2; rate += 4)
                for (size_t p = 0; p < 2; ++p) {
                    const char *target = d ? "t" : "ω";
                    const json_t *r = math_fourier_fields(lab_math_format("{%s{-3*abs(%d*%s+3)^(%.17g)/2} | %s=%.17g}",
                        d ? "@Finv" : "@F", rate, d ? "ω" : "t", power, target, points[p]), target);
                    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                    double complex expected = -1.5*coefficient*pow(fabs((double)rate), power)*pow(fabs(points[p]), -power-1)*
                                              cexp((d ? -I : I)*points[p]*3/rate)/(d ? 2*M_PI : 1);
                    lab_math_close(math_fourier_number(r), expected, 1e-11);
                }
    }
    const json_t *r = math_fourier_fields("@F{|t|^p}", "ω");
    lab_math_contains(math_fourier_text(r, "unbound"), "Re(-p) > 0", true);
    lab_math_contains(math_fourier_text(r, "unbound"), "Re(p + 1) > 0", true);
    const char *fractions[] = {"-1/4", "-3/4"};
    for (size_t i = 0; i < 2; ++i)
        lab_math_places(math_fourier_number(math_fourier_fields(lab_math_format("{@F{|t|^p} | p=%s; ω=2}", fractions[i]), "ω")),
                        math_fourier_number(math_fourier_fields(lab_math_format("{@F{|t|^(%s)} | ω=2}", fractions[i]), "ω")), 12);
}

static void test_absolute_power_preserves_unrelated_source_domains(void)
{
    const char *conditions[] = {"ω - 1 ≠ 0", "a*ω ≠ 0", "Re(ω) > 0"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format(
            "InverseFourier(1/sqrt(abs(ω)) where (%s),ω,t)", conditions[i]), "t"),
                               "function"), "inversefourier(", true);
    const char *powers[] = {"-2", "-1", "1/4"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(abs(ω)^(%s) where (ω ≠ 0),ω,t)", powers[i]), "t");
        lab_math_contains(math_fourier_text(r, "function"), "inversefourier(", true);
        lab_math_contains(math_fourier_text(r, "unbound"), "ω ≠ 0", true);
    }
}

static void test_implicit_modulus_products(void)
{
    const char *bodies[] = {"2|x|", "2 |x|", "|x||x+1|", "|x| |x+1|", "||x||", "|2*(3|x|)|",
        "|2*|x||", "|hypot(2|x|,8)|", "|floor(2|x|)|", "|sin(2|x|)|"};
    const double expected[] = {6, 6, 6, 6, 3, 18, 6, 10, 6, fabs(sin(6))};
    for (size_t i = 0; i < 10; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{%s | x=-3}", bodies[i]), "x");
        lab_math_places(math_fourier_number(r), expected[i], 12);
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "x"), "value"),
            math_fourier_text(r, "value"));
    }
}

static void test_implicit_modulus_exponential_transform(void)
{
    const json_t *r = math_fourier_fields("@F{exp(a|t|)}", "ω");
    lab_math_equal(math_fourier_text(r, "unbound"), math_fourier_text(math_fourier_fields("@F{exp(a*abs(t))}", "ω"), "unbound"));
    lab_math_contains(math_fourier_text(r, "unbound"), "(-a)²", false);
    lab_math_contains(math_fourier_text(r, "unbound"), "Re(-a) > 0", true);
    const int frequencies[] = {0, 1, 3};
    for (int decay = -1; decay >= -2; --decay)
        for (size_t i = 0; i < 3; ++i) {
            r = math_fourier_fields(lab_math_format("{@F{exp(a|t|)} | ω=%d; a=%d}", frequencies[i], decay), "ω");
            lab_math_places(math_fourier_number(r), -2.0*decay/(decay*decay+frequencies[i]*frequencies[i]), 7);
            const char *output = lab_math_programme(math_fourier_text(r, "operation_function"), 40);
            lab_math_places(math_fourier_number(math_fourier_fields(output, "ω")), math_fourier_number(r), 7);
        }
}

static void test_negated_integer_powers(void)
{
    const int exponents[] = {2, 3, 4, -2, -3};
    const char *points[] = {"2", "-2", "1+i"};
    const double complex values[] = {2, -2, 1+I};
    for (size_t n = 0; n < 5; ++n) {
        const char *source = lab_math_format("(-x)^(%d)", exponents[n]);
        lab_math_contains(math_fourier_text(math_fourier_fields(source, "x"), "unbound"), "(-x)", false);
        for (size_t p = 0; p < 3; ++p)
            lab_math_places(math_fourier_number(math_fourier_fields(lab_math_format("{%s | x=%s}", source, points[p]), "x")),
                            cpow(-values[p], exponents[n]), 7);
    }
    lab_math_equal(math_fourier_text(math_fourier_fields("(-x)*(-x)", "x"), "unbound"), "x²");
    lab_math_contains(math_fourier_text(math_fourier_fields("(-x)^(1/2)", "x"), "unbound"), "-x", true);
}

static double math_fourier_sinc(double x)
{
    return x == 0 ? 1 : sin(M_PI*x)/(M_PI*x);
}

static void test_pulses_and_sinc(void)
{
    const double points[] = {0, 0.3, 1.25};
    double rectangle[3], triangle[3], circle[3], inverse[3], scaled[3];
    for (size_t p = 0; p < 3; ++p) {
        double x = points[p];
        rectangle[p] = math_fourier_sinc(x/(2*M_PI));
        triangle[p] = pow(rectangle[p], 2);
        circle[p] = 2*math_fourier_sinc(x/M_PI);
        inverse[p] = rectangle[p]/(2*M_PI);
        scaled[p] = fmax(1-fabs(x)/(4*M_PI), 0)/2;
    }
    math_fourier_formula("@F{rect(t)}", "ω", points, rectangle, 3);
    math_fourier_formula("@F{tri(t)}", "ω", points, triangle, 3);
    math_fourier_formula("@F{circ(t)}", "ω", points, circle, 3);
    math_fourier_formula("@Finv{rect(ω)}", "t", points, inverse, 3);
    math_fourier_formula("@F{sinc(2*t)^2}", "ω", points, scaled, 3);
    const double pulse_points[] = {0, 1, 4}, pulse_values[] = {1, 1, 0};
    const double square_points[] = {0, 1, 8}, square_values[] = {1, 1-1/(2*M_PI), 0};
    const double inverse_points[] = {0, 0.5, 2}, inverse_values[] = {1, 0.5, 0};
    math_fourier_formula("@F{sinc(t)}", "ω", pulse_points, pulse_values, 3);
    math_fourier_formula("@F{sinc(t)^2}", "ω", square_points, square_values, 3);
    math_fourier_formula("@Finv{sinc(ω/(2*pi))^2}", "t", inverse_points, inverse_values, 3);
}

static void test_two_sided_exponential_and_rational_pair(void)
{
    const double points[] = {0, 0.3, 1.25};
    double forward[3], rational[3], inverse[3];
    for (size_t p = 0; p < 3; ++p) {
        forward[p] = 2/(1+points[p]*points[p]);
        rational[p] = M_PI*exp(-fabs(points[p]));
        inverse[p] = exp(-fabs(points[p]))/2;
    }
    math_fourier_formula("@F{exp(-abs(t))}", "ω", points, forward, 3);
    math_fourier_formula("@F{1/(t^2+1)}", "ω", points, rational, 3);
    math_fourier_formula("@Finv{1/(ω^2+1)}", "t", points, inverse, 3);
}

static void test_hyperbolic_secant(void)
{
    const double points[] = {0, 0.3, 1.25};
    double forward[3], inverse[3];
    for (size_t p = 0; p < 3; ++p) {
        forward[p] = M_PI/cosh(M_PI*points[p]/2);
        inverse[p] = 1/(2*cosh(M_PI*points[p]/2));
    }
    math_fourier_formula("@F{sech(t)}", "ω", points, forward, 3);
    math_fourier_formula("@Finv{sech(ω)}", "t", points, inverse, 3);
}

static void test_impulses_and_step_distribution(void)
{
    lab_math_equal(math_fourier_text(math_fourier_fields("@F{delta(t)}", "ω"), "unbound"), "1");
    const double points[] = {0, 0.3, 1.25}, expected[] = {1/(2*M_PI), 1/(2*M_PI), 1/(2*M_PI)};
    math_fourier_formula("@Finv{delta(ω)}", "t", points, expected, 3);
    const json_t *r = math_fourier_fields("@F{step(t)}", "ω");
    lab_math_contains(math_fourier_text(r, "unbound"), "principal value", true);
    lab_math_contains(math_fourier_text(r, "unbound"), "δ(ω)", true);
    lab_math_contains(math_fourier_text(r, "tex"), "\\text{principal value}", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{1}", "ω"), "unbound"), "δ(ω)", true);
    lab_math_check(isnan(creal(math_fourier_number(math_fourier_fields("delta(0)", "ω")))), "impulse at support is NAN");
}

static void test_arbitrary_functions_and_scope(void)
{
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{f(t)}", "ω"), "function"), "fourier(", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{u(t)}", "ω"), "function"), "fourier(", true);
    const json_t *r = math_fourier_fields("@F{f(t-2)}", "ω");
    lab_math_contains(math_fourier_text(r, "unbound"), "exp(-2iω)", true);
    lab_math_contains(math_fourier_text(r, "function"), "t = ?", false);
    r = math_fourier_fields("@F{f'(t)}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", true);
    lab_math_contains(math_fourier_text(r, "unbound"), "f(0)", false);
    r = math_fourier_fields("@F(@F(f(t),t,ω),t,x)", "ω");
    lab_math_contains(math_fourier_text(r, "unbound"), "δ(x)", true);
    lab_math_contains(math_fourier_text(r, "function"), "t = ?", false);
}

static void test_parser_aliases_and_explicit_mapping(void)
{
    const char *expected = math_fourier_text(math_fourier_fields("@F{exp(-t^2)}", "ω"), "tex");
    const char *aliases[] = {"@F", "ℱ", "Fourier"};
    const char *open[] = {"(", "{"}, *close[] = {")", "}"};
    for (size_t a = 0; a < 3; ++a)
        for (size_t b = 0; b < 2; ++b)
            lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("%s%sexp(-t^2)%s", aliases[a], open[b],
                close[b]), "ω"), "tex"), expected);
    const double points[] = {0, 0.3, 1.25};
    double forward[3], inverse[3];
    for (size_t i = 0; i < 3; ++i) {
        forward[i] = sqrt(M_PI)*exp(-points[i]*points[i]/4);
        inverse[i] = exp(-points[i]*points[i]/4)/(2*sqrt(M_PI));
    }
    math_fourier_formula("@F(exp(-q^2),q,p)", "p", points, forward, 3);
    const char *inverse_aliases[] = {"@Finv", "ℱ⁻¹", "InverseFourier"};
    for (size_t i = 0; i < 3; ++i)
        math_fourier_formula(lab_math_format("%s(exp(-ω^2))", inverse_aliases[i]), "t", points, inverse, 3);
    const char *delta_aliases[] = {"delta", "δ", "@delta", "DiracDelta"};
    const char *step_aliases[] = {"step", "heaviside", "Heaviside", "θ"};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("@F{%s(t)}", delta_aliases[i]), "ω"),
            "unbound"), "1");
        lab_math_check(math_fourier_number(math_fourier_fields(lab_math_format("%s(0)", step_aliases[i]), "ω")) == 0.5,
            "step midpoint");
    }
    const char *invalid[] = {"@F(exp(-t^2),t,t)", "@F{ω*exp(-t^2)}", "Derivative(-t,n)", "Derivative(t+1,n)"};
    for (size_t i = 0; i < 4; ++i) {
        int status = 0;
        const char *raw = NULL;
        lab_math_worker("mars_lab", invalid[i], "ω", "evaluate", 40, &status, &raw);
        lab_math_check(status != 0, raw);
    }
}

static void test_custom_coordinate_defaults_both_directions(void)
{
    const char *aliases[][3] = {{"@F", "ℱ", "Fourier"}, {"@Finv", "ℱ⁻¹", "InverseFourier"}};
    const char *coordinates[] = {"[time]", "[frequency]", "[radius]", "q", "τ"};
    for (size_t d = 0; d < 2; ++d) {
        const char *target = d ? "t" : "ω", *source = d ? "ω" : "t";
        const char *expected = math_fourier_text(math_fourier_fields(lab_math_format("%s{exp(-%s^2)}", aliases[d][0],
            source), target), "unbound");
        for (size_t a = 0; a < 3; ++a)
            for (size_t c = 0; c < 5; ++c) {
                const char *inputs[] = {lab_math_format("%s{exp(-%s^2)}", aliases[d][a], coordinates[c]),
                    lab_math_format("%s(exp(-%s^2),%s)", aliases[d][a], coordinates[c], coordinates[c])};
                for (size_t i = 0; i < 2; ++i) {
                    const json_t *r = math_fourier_fields(inputs[i], target);
                    lab_math_equal(math_fourier_text(r, "unbound"), expected);
                    lab_math_contains(math_fourier_text(r, "operation_function"), lab_math_format("%s = ?.",
                        coordinates[c]), false);
                }
            }
    }
}

static void test_bracketed_gamma_coordinate_and_run(void)
{
    const json_t *r = math_fourier_fields("@F{gamma(a+i[time])}", "ω");
    lab_math_equal(math_fourier_text(r, "unbound"), math_fourier_text(math_fourier_fields(
        "@F(gamma(a+i[time]),[time],ω)", "ω"), "unbound"));
    lab_math_contains(math_fourier_text(r, "operation_function"), "fourier(gamma(a + i.time), time, @omega)", true);
    lab_math_contains(math_fourier_text(r, "expression"), "[time] =", false);
    r = math_fourier_fields("{@F{gamma(a+i[time])} | ω=0; a=1}", "ω");
    lab_math_places(lab_math_parse_number(lab_math_programme(math_fourier_text(r, "operation_function"), 40)), 2*M_PI/exp(1), 12);
}

static void test_custom_coordinate_targets_do_not_capture_or_hide_ambiguity(void)
{
    const char *invalid[] = {"@F{[time]+[position]}", "@Finv{[frequency]+[wave]}",
        "@F(exp(-[time]^2)+ω,[time])", "@Finv(exp(-[frequency]^2)+t,[frequency])",
        "@F(exp(-[time]^2),[time],[time])"};
    for (size_t i = 0; i < 5; ++i) {
        int status = 0;
        const char *raw = NULL;
        lab_math_worker("mars_lab", invalid[i], "ω", "evaluate", 40, &status, &raw);
        lab_math_check(status != 0, raw);
    }
    const json_t *r = math_fourier_fields("@F(exp(-[time]^2),[time],[frequency])", "ω");
    lab_math_contains(math_fourier_text(r, "unbound"), "[frequency] ∈ ℝ", true);
    lab_math_contains(math_fourier_text(r, "operation_function"), "time, frequency)", true);
    lab_math_contains(math_fourier_text(r, "operation_function"), "@omega", false);
}

static void test_conventional_coordinate_pairs_keep_their_defaults(void)
{
    const char *pairs[][2] = {{"t", "ω"}, {"x", "k"}, {"y", "m"}, {"z", "n"}};
    for (size_t i = 0; i < 4; ++i)
        for (size_t d = 0; d < 2; ++d) {
            const char *source = pairs[i][d], *target = pairs[i][1-d], *op = d ? "@Finv" : "@F";
            const json_t *shorthand = math_fourier_fields(lab_math_format("%s{exp(-%s^2)}", op, source), target);
            const json_t *explicit = math_fourier_fields(lab_math_format("%s(exp(-%s^2),%s,%s)", op, source, source,
                target), target);
            lab_math_equal(lab_math_replace(math_fourier_text(shorthand, "unbound"), "¼·", "¼"),
                           lab_math_replace(math_fourier_text(explicit, "unbound"), "¼·", "¼"));
        }
}

static void test_parameters_are_not_free_variable_samples(void)
{
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{exp(-a*t^2)}", "ω"), "unbound"), "a", true);
    const json_t *r = math_fourier_fields("{@F{exp(-a*t^2)} | ω=1; a=2}", "ω");
    lab_math_places(math_fourier_number(r), sqrt(M_PI/2)*exp(-1.0/8), 12);
    lab_math_contains(math_fourier_text(r, "function"), "a = ?", false);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{exp(t^2)}", "ω"), "function"), "fourier(", true);
}

static void test_affine_scaling_shifts_and_modulation(void)
{
    math_fourier_symbolic_formula("@F{rect(-2*t+4)}", "exp(-2*i*ω)*sinc(ω/(4*pi))/2", "ω");
    math_fourier_symbolic_formula("@F{exp(2*i*t)*rect(t)}", "sinc((ω-2)/(2*pi))", "ω");
    math_fourier_symbolic_formula("@Finv{delta(2*ω-4)}", "exp(2*i*t)/(4*pi)", "t");
    math_fourier_symbolic_formula("@F{delta(2*t-4)}", "exp(-2*i*ω)/2", "ω");
}

static void test_one_sided_exponentials(void)
{
    math_fourier_symbolic_formula("@F{exp(-t)*step(t)}", "1/(1+i*ω)", "ω");
    math_fourier_symbolic_formula("@F{exp(t)*step(-t)}", "1/(1-i*ω)", "ω");
    math_fourier_symbolic_formula("@F{exp(-t)*step(t-2)}", "exp(-2*(1+i*ω))/(1+i*ω)", "ω");
    math_fourier_symbolic_formula("@Finv{exp(-ω)*step(ω)}", "1/(2*pi*(1-i*t))", "t");
    const double points[] = {-1, 0, 1};
    const double right[] = {0, 0.5, exp(-1)}, left[] = {exp(-1), 0.5, 0};
    const double forward[] = {2*M_PI*exp(-1), M_PI, 0}, negative[] = {-exp(-1), -0.5, 0};
    math_fourier_formula("@Finv{1/(1+i*ω)}", "t", points, right, 3);
    math_fourier_formula("@Finv{1/(1-i*ω)}", "t", points, left, 3);
    math_fourier_formula("@F{1/(1+i*t)}", "ω", points, forward, 3);
    math_fourier_formula("@Finv{1/(-1+i*ω)}", "t", points, negative, 3);
}

static void test_frequency_derivatives_and_product_association(void)
{
    const double points[] = {0, 0.3, 1.25};
    double expected[3];
    for (size_t i = 0; i < 3; ++i)
        expected[i] = sqrt(M_PI)*(2-points[i]*points[i])*exp(-points[i]*points[i]/4)/4;
    math_fourier_formula("@F{t^2*exp(-t^2)}", "ω", points, expected, 3);
    math_fourier_symbolic_formula("@F{exp(2*i*t)*t*exp(-t^2)}", "-i*sqrt(pi)*(ω-2)*exp(-(ω-2)^2/4)/2", "ω");
    const char *sources[] = {"@F{t*f(t)}", "@Finv{ω*f(ω)}", "@F{t^3}", "@Finv{ω^3}", "@F{exp(2*i*t)*t*f(t)}"};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *r = math_fourier_fields(sources[i], "ω");
        lab_math_contains(math_fourier_text(r, "function"), "_fourier_", false);
        lab_math_contains(math_fourier_text(r, "tex"), "_fourier_", false);
        lab_math_contains(math_fourier_text(r, "unbound"), "D", true);
    }
}

static void test_round_trips_and_step_distribution_inverse(void)
{
    const double points[] = {0, 0.3, 1.25};
    double expected[3];
    for (size_t i = 0; i < 3; ++i)
        expected[i] = exp(-points[i]*points[i]);
    math_fourier_formula("@Finv{@F{exp(-t^2)}}", "t", points, expected, 3);
    math_fourier_formula("@F{@Finv{exp(-ω^2)}}", "ω", points, expected, 3);
    const double step_points[] = {-1, 0, 1}, values[] = {0, 0.5, 1};
    math_fourier_formula("@Finv{pi*delta(ω)+PV(1/(i*ω))}", "t", step_points, values, 3);
}

static void test_conditioned_output_round_trips(void)
{
    const char *sources[] = {"@F{exp(-a*t^2)}", "@F{rect(t)}", "@Finv{exp(-ω^2)}", "@F{1+t^n+delta(t)}", "@Finv{ω^n}"};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *r = math_fourier_fields(sources[i], "ω");
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "ω"), "tex"),
            math_fourier_text(r, "tex"));
    }
    const char *inverses[] = {"@Finv{sinc(ω/(2*pi))^2}", "@Finv{sinc(2*ω)^2}"};
    const double points[] = {0, 0.5, 2};
    for (size_t i = 0; i < 2; ++i)
        for (size_t p = 0; p < 3; ++p) {
            const json_t *r = math_fourier_fields(lab_math_format("{%s | t=%.17g}", inverses[i], points[p]), "ω");
            lab_math_places(math_fourier_number(math_fourier_fields(math_fourier_text(r, "expression"), "ω")),
                math_fourier_number(r), 12);
        }
}

static void test_symbolic_polynomial_distributions(void)
{
    const json_t *r = math_fourier_fields("@F{1+t^n+delta(t)}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "expression"), "Derivative(δ(ω), n)", true);
    lab_math_contains(math_fourier_text(r, "expression"), "n ∈ ℤ≥0", true);
    lab_math_contains(math_fourier_text(r, "function"), "delta(@omega)", true);
    lab_math_contains(math_fourier_text(r, "function"), "derivative(delta(@omega), n)", true);
    lab_math_contains(math_fourier_text(r, "tex"), "\\delta(\\omega)", true);
    lab_math_contains(math_fourier_text(r, "tex"), "\\delta^{(n)}\\left(\\omega\\right)", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@Finv{ω^n}", "t"), "unbound"), "(-i)^n·Derivative(δ(t), n)", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{t^33}", "ω"), "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(math_fourier_fields("@Finv{ω^33}", "ω"), "function"), "fourier(", false);
    r = math_fourier_fields("@F{t^(-1)}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "unbound"), "sgn(ω)", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{t^(-2)}", "ω"), "function"), "fourier(", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{t^(1/2)}", "ω"), "function"), "fourier(", true);
    const int orders[] = {0, 1, 2, 3, 33};
    for (size_t n = 0; n < 5; ++n)
        for (size_t d = 0; d < 2; ++d) {
            const char *target = d ? "t" : "ω";
            math_fourier_symbolic_formula(lab_math_format("%s{Derivative(delta(%s),%d)}",
                d ? "@Finv" : "@F", d ? "ω" : "t", orders[n]),
                lab_math_format("%s*(%s*%s)^%d", d ? "1/(2*pi)" : "1", d ? "-i" : "i", target, orders[n]), target);
        }
    for (int n = 0; n < 4; ++n)
        lab_math_check(math_fourier_number(math_fourier_fields(lab_math_format(
            "{@F{ordered_derivative(delta(t),n)} | n=%d; ω=0}", n), "ω")) ==
                       (n == 0 ? 1 : 0), "ordered impulse derivative at zero frequency");
}

static void test_signal_greek_arguments_remain_function_calls(void)
{
    const char *names[] = {"delta", "step", "rect", "tri", "circ", "sinc", "PV"};
    for (size_t i = 0; i < 7; ++i)
        for (size_t square = 0; square < 2; ++square) {
            const json_t *r = math_fourier_fields(lab_math_format("%s(ω)%s", names[i], square ? "^2" : ""), "ω");
            if (i == 6) {
                lab_math_contains(math_fourier_text(r, "tex"), "\\text{principal value}", true);
                lab_math_contains(math_fourier_text(r, "tex"), "\\operatorname{PV}", false);
            } else {
                lab_math_contains(math_fourier_text(r, "tex"), "(\\omega)", true);
            }
        }
}

static double complex math_fourier_gaussian_integrand(double x, void *context)
{
    const double *parameters = context;
    return cexp(-x*x+2*x+I*parameters[0]*parameters[1]*x);
}

static void test_gaussian_against_independent_quadrature(void)
{
    const double points[] = {-1, 0.5, 2};
    for (size_t d = 0; d < 2; ++d)
        for (size_t p = 0; p < 3; ++p) {
            const char *source = d ? "@Finv{exp(-ω^2+2*ω)}" : "@F{exp(-t^2+2*t)}", *variable = d ? "t" : "ω";
            double parameters[] = {d ? 1 : -1, points[p]};
            double complex expected = lab_math_simpson(math_fourier_gaussian_integrand, parameters, -8, 8, 2048)/(d ? 2*M_PI : 1);
            const char *literal = lab_math_format("(%.17g)+i*(%.17g)", creal(expected), cimag(expected));
            const json_t *r = math_fourier_fields(lab_math_format("{abs((%s)-(%s)) | %s=%.17g}", source, literal,
                variable, points[p]), variable);
            lab_math_check(creal(math_fourier_number(r)) < 1e-11, "independent Gaussian Fourier kernel quadrature");
        }
}

static void test_readme_bracketed_fourier_coordinate(void)
{
    /* README example: docs/expression.md, bracketed Fourier coordinate. */
    lab_math_equal(math_fourier_text(math_fourier_fields("@F{gamma(a+i[time])}", "ω"), "unbound"),
                   "2π·exp(aω - exp(ω)) where (ω ∈ ℝ; Re(a) > 0)");
}

static void test_readme_absolute_half_power(void)
{
    /* README examples: docs/expression.md, absolute half powers. */
    lab_math_equal(math_fourier_text(math_fourier_fields("@F{1/sqrt(|t|)}", "ω"), "unbound"), "√(2π/|ω|) where (ω ∈ ℝ; ω ≠ 0)");
    lab_math_equal(math_fourier_text(math_fourier_fields("@Finv{sqrt(2*pi)/sqrt(|ω|)}", "t"), "unbound"),
        "1/√(|t|) where (t ∈ ℝ; t ≠ 0)");
}

static void test_readme_implicit_modulus_products(void)
{
    /* README examples: docs/expression.md, implicit modulus products. */
    const char *sources[] = {"{2|t| | t=-3}", "{|t||t+1| | t=-3}", "{@F{exp(a|t|)} | ω=1; a=-2}"};
    const char *expected[] = {"6", "6", "0.8"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(sources[i], "ω"), "value"), expected[i]);
}

static void test_readme_fourier_examples(void)
{
    /* README examples: docs/expression.md, angular-frequency Fourier table. */
    const char *cases[][2] = {
        {"@F{exp(-t^2)}", "√(π)·exp(-¼ω²) where (ω ∈ ℝ)"},
        {"@Finv{exp(-ω^2)}", "½·exp(-¼t²)/√(π) where (t ∈ ℝ)"},
        {"@F{rect(t)}", "sinc(ω/(2π)) where (ω ∈ ℝ)"},
        {"@F{delta(t)}", "1"},
        {"@F{step(t)}", "(1/(iω) : principal value) + π·δ(ω) where (ω ∈ ℝ)"},
        {"@F{1+t^n+delta(t)}", "2π·(δ(ω) + i^n·Derivative(δ(ω), n)) + 1 where (ω ∈ ℝ; n ∈ ℤ≥0)"},
        {"@F{f(t)}", "ℱ(f(t))"}};
    for (size_t i = 0; i < 7; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(cases[i][0], "ω"), "unbound"), cases[i][1]);
}

/* Register ordinary regressions. */
void test_lab_math_fourier_cases(void)
{
    TEST_RUN_IN_GROUP(test_gaussians_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_absolute_half_power_spellings_and_run, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_absolute_power_inverse_and_copied_spectrum, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_radical_quotient_simplification_preserves_branches, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_absolute_power_strip_affine_scaling_and_scalar_factors, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_absolute_power_preserves_unrelated_source_domains, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_implicit_modulus_products, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_implicit_modulus_exponential_transform, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_negated_integer_powers, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_pulses_and_sinc, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_two_sided_exponential_and_rational_pair, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_hyperbolic_secant, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_impulses_and_step_distribution, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_arbitrary_functions_and_scope, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_parser_aliases_and_explicit_mapping, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_custom_coordinate_defaults_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bracketed_gamma_coordinate_and_run, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_custom_coordinate_targets_do_not_capture_or_hide_ambiguity, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_conventional_coordinate_pairs_keep_their_defaults, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_parameters_are_not_free_variable_samples, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_affine_scaling_shifts_and_modulation, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_one_sided_exponentials, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_frequency_derivatives_and_product_association, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_round_trips_and_step_distribution_inverse, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_conditioned_output_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_polynomial_distributions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_signal_greek_arguments_remain_function_calls, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_gaussian_against_independent_quadrature, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_bracketed_fourier_coordinate, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_absolute_half_power, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_implicit_modulus_products, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_fourier_examples, readme_examples, "math,readme,output");
    lab_math_reset();
}
