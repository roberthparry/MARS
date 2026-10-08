/**
 * @file test_lab_math_fourier_hyperbolic.c
 * @brief Native hyperbolic-power Fourier regression port.
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


typedef struct {
    double complex exponent;
    double frequency, scale, offset;
    bool inverse, sinh;
    double complex phase;
} math_fourier_quadrature_t;

static double complex math_fourier_power_integrand(double r, void *context)
{
    const math_fourier_quadrature_t *p = context;
    double complex kernel = p->inverse ? I : -I;
    if (r == 0)
        return p->exponent == -0.75 && p->sinh ?
            4*(1+p->phase)*cexp(-kernel*p->frequency*p->offset/p->scale)/fabs(p->scale) : 0;
    double u = pow(r, 4);
    double complex magnitude = cexp(p->exponent*log(p->sinh ? sinh(u) : cosh(u)));
    double complex positive = cexp(kernel*p->frequency*(u-p->offset)/p->scale);
    double complex negative = cexp(kernel*p->frequency*(-u-p->offset)/p->scale);
    return 4*r*r*r*magnitude*(positive+p->phase*negative)/fabs(p->scale);
}

static double complex math_fourier_quadrature(double complex exponent, double frequency, double scale, double offset,
                                  bool inverse, bool sinh_base, double complex branch)
{
    math_fourier_quadrature_t p = {exponent, frequency, scale, offset, inverse, sinh_base,
                                  sinh_base ? cexp(I*M_PI*branch) : 1};
    return lab_math_simpson(math_fourier_power_integrand, &p, 0, pow(160, 0.25), 6000)/(inverse ? 2*M_PI : 1);
}

static const char *math_fourier_spectrum(const char *body, bool inverse)
{
    return lab_math_algebra(math_fourier_fields(lab_math_format("%s{%s}", inverse ? "@Finv" : "@F", body), inverse ? "t" : "ω"));
}

static void test_copied_beta_spectrum_recovers_shifted_sinh(void)
{
    const char *s = "2^(-(n+1))*exp(b*ω*i/a)/abs(a)*(exp(i*@pi*n)*B(-(n+ω*i/a)/2,n+1)+B((ω*i/a-n)/2,n+1))";
    const json_t *r = math_fourier_fields(lab_math_format("@Finv{%s}", s), "t");
    const char *prefix = "{ sinh(at + b)^n |";
    lab_math_check(strncmp(math_fourier_text(r, "expression"), prefix, strlen(prefix)) == 0, "copied beta spectrum");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "function"), "beta(", false);
    lab_math_contains(math_fourier_text(r, "expression"), "Re(n + 1) > 0", true);
    lab_math_contains(math_fourier_text(r, "expression"), "Re(-n) > 0", true);
    lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "t"), "tex"),
        math_fourier_text(r, "tex"));
}

static void test_hyperbolic_beta_round_trips(void)
{
    const char *bodies[] = {"sinh(a*t+b)^n", "abs(sinh(a*t+b))^n", "cosech(a*t+b)^n",
                            "cosh(a*t+b)^n", "sech(a*t+b)^n"};
    const double exponents[] = {-0.5, -0.5, 0.5, -0.5, 0.5};
    const double parameters[][3] = {{2, 0.3, 0.7}, {-2, 0.3, 0.7}, {-1, -0.2, -0.6}};
    for (size_t b = 0; b < 5; ++b) {
        const char *s = math_fourier_spectrum(bodies[b], false);
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format("@Finv{%s}", s), "t"), "function"),
            "fourier(", false);
        for (size_t p = 0; p < 3; ++p) {
            const char *bindings = lab_math_format(" | t=%.17g; a=%.17g; b=%.17g; n=%.17g",
                parameters[p][2], parameters[p][0], parameters[p][1], exponents[b]);
            const json_t *restored = math_fourier_fields(lab_math_format("{@Finv{%s}%s}", s, bindings), "t");
            const json_t *expected = math_fourier_fields(lab_math_format("{%s%s}", bodies[b], bindings), "t");
            lab_math_close(math_fourier_number(restored), math_fourier_number(expected), 1e-12);
        }
    }
}

static void test_beta_duality_and_reverse_round_trip(void)
{
    const char *s = math_fourier_spectrum("sinh(a*t+b)^n", false);
    const json_t *r = math_fourier_fields(lab_math_format("{@F{%s,ω,t} | t=0.7; a=-2; b=0.3; n=-1/2}", s), "t");
    lab_math_close(math_fourier_number(r), 2*M_PI*cpow(CMPLX(sinh(1.7), 0), -0.5), 1e-12);
    s = math_fourier_spectrum("sinh(a*ω+b)^n", true);
    r = math_fourier_fields(lab_math_format("@F{%s}", s), "ω");
    const char *prefix = "{ sinh(b + aω)^n |";
    lab_math_check(strncmp(math_fourier_text(r, "expression"), prefix, strlen(prefix)) == 0, "reverse beta round trip");
    lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{@F{%s} | ω=0.7; a=2; b=0.3; n=-1/2}", s), "ω")),
                   cpow(CMPLX(sinh(1.7), 0), -0.5), 1e-12);
}

static void test_negative_imaginary_products_survive_expression_round_trip(void)
{
    const char *bodies[] = {"exp(x*(-i))", "exp(x*(-2*i))", "x*(-i)", "(x+1)*(-3*i)"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{%s | x=0.7}", bodies[i]), "x");
        lab_math_close(math_fourier_number(math_fourier_fields(math_fourier_text(r, "expression"), "x")),
            math_fourier_number(r), 1e-14);
    }
}

static void test_beta_round_trip_with_complex_power_and_swapped_arguments(void)
{
    const char *s = "2^(-(n+1))/abs(a)*(B(n+1,(i*ω/a-n)/2)+exp(i*@pi*n)*B(n+1,(-i*ω/a-n)/2))";
    const double points[] = {-0.4, 0.4};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{@Finv{%s} | t=%.17g; a=-2; n=-1/4+i/5}", s, points[i]), "t");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        lab_math_close(math_fourier_number(r), cpow(CMPLX(sinh(-2*points[i]), 0), -0.25+0.2*I), 1e-12);
    }
}

static void test_beta_pair_does_not_accept_near_misses_or_invalid_domains(void)
{
    const char *b = "B((i*ω/a-n)/2,n+1)", *r = "B((-i*ω/a-n)/2,n+1)";
    const char *bodies[] = {lab_math_format("%s+2*exp(i*@pi*n)*%s", b, r),
        lab_math_format("%s+exp(i*@pi*n)*B((-i*ω/a-n)/2,n+2)", b),
        lab_math_format("%s*%s", b, r), lab_math_format("1/(%s+exp(i*@pi*n)*%s)", b, r)};
    for (size_t i = 0; i < 4; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format("@Finv{%s}", bodies[i]), "t"),
            "function"), "fourier(", true);
    const char *s = math_fourier_spectrum("sinh(a*t+b)^n", false);
    const char *bindings[] = {"a=1; b=0; n=2", "a=1; b=0; n=-2", "a=i; b=0; n=-1/2"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format("{@Finv{%s} | t=?; %s}", s,
            bindings[i]), "t"), "function"),
                          "fourier(", true);
}

static void test_beta_symbol_and_callable_aliases(void)
{
    const char *aliases[] = {"beta", "B", "Β"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("%s(x,y)", aliases[i]), "x");
        lab_math_contains(math_fourier_text(r, "tex"), "\\mathrm{B}", true);
        lab_math_contains(math_fourier_text(r, "tex"), "\\operatorname{beta}", false);
        lab_math_contains(math_fourier_text(r, "expression"), "B(", true);
        lab_math_contains(math_fourier_text(r, "function"), "beta(", true);
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "x"), "tex"),
            math_fourier_text(r, "tex"));
        lab_math_places(creal(math_fourier_number(math_fourier_fields(lab_math_format("{%s(x,y) | x=2; y=3}",
            aliases[i]), "x"))), 1.0/12, 7);
    }
}

static void test_requested_symbolic_power_and_conditions(void)
{
    const json_t *r = math_fourier_fields("@F{sinh^n(at+b)}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "function"), "beta(", true);
    lab_math_contains(math_fourier_text(r, "tex"), "\\mathrm{B}", true);
    lab_math_contains(math_fourier_text(r, "tex"), "n", true);
    lab_math_contains(math_fourier_text(r, "function"), "realpart", true);
    lab_math_contains(math_fourier_text(r, "value_note"), "Principal powers", true);
    lab_math_contains(math_fourier_text(r, "value_note"), "-1 < Re(n) < 0", true);
    lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "ω"), "tex"),
        math_fourier_text(r, "tex"));
    lab_math_equal(math_fourier_text(math_fourier_fields("@F{sinh(at+b)^n}", "ω"), "tex"), math_fourier_text(r, "tex"));
    lab_math_equal(math_fourier_text(math_fourier_fields("@F{sinh(a*t+b)^n}", "ω"), "tex"), math_fourier_text(r, "tex"));
}

static void test_sinh_powers_against_independent_quadrature(void)
{
    const double powers[] = {-0.25, -0.5, -0.75};
    const double parameters[][3] = {{1, 0, 0}, {2, 0.3, 0.7}, {-1, -0.2, -0.6}};
    for (size_t d = 0; d < 2; ++d)
        for (size_t n = 0; n < 3; ++n)
            for (size_t p = 0; p < 3; ++p) {
                const char *op = d ? "@Finv" : "@F", *source = d ? "ω" : "t", *target = d ? "t" : "ω";
                double a = parameters[p][0], b = parameters[p][1], k = parameters[p][2];
                const json_t *r = math_fourier_fields(lab_math_format(
                    "{%s{sinh(a*%s+b)^n} | %s=%.17g; a=%.17g; b=%.17g; n=%.17g}",
                    op, source, target, k, a, b, powers[n]), target);
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                lab_math_close(math_fourier_number(r), math_fourier_quadrature(powers[n], k, a, b, d, true, powers[n]), 2e-9);
            }
}

static void test_complex_exponent_in_convergence_strip(void)
{
    for (size_t d = 0; d < 2; ++d) {
        const json_t *r = math_fourier_fields(lab_math_format("{%s{sinh(%s)^(-1/4+i/5)} | %s=0.6}",
            d ? "@Finv" : "@F", d ? "ω" : "t", d ? "t" : "ω"), d ? "t" : "ω");
        lab_math_close(math_fourier_number(r), math_fourier_quadrature(-0.25+0.2*I, 0.6, 1, 0, d, true, -0.25+0.2*I), 2e-8);
    }
}

static void test_reciprocals_and_absolute_powers_preserve_branches(void)
{
    const char *sources[] = {"1/sqrt(sinh(t))", "sqrt(cosech(t))", "abs(sinh(t))^(-1/2)", "1/sinh(t)^(1/2)"};
    const double branches[] = {-0.5, 0.5, 0, -0.5};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{@F{%s} | ω=0.4}", sources[i]), "ω");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        lab_math_close(math_fourier_number(r), math_fourier_quadrature(-0.5, 0.4, 1, 0, false, true, branches[i]), 2e-9);
    }
}

static void test_cosh_and_sech_powers(void)
{
    for (size_t d = 0; d < 2; ++d) {
        const char *source = d ? "ω" : "t", *target = d ? "t" : "ω", *op = d ? "@Finv" : "@F";
        const char *bodies[] = {lab_math_format("cosh(2*%s+0.3)^(-1/2)", source),
                               lab_math_format("sech(2*%s+0.3)^(1/2)", source)};
        for (size_t i = 0; i < 2; ++i) {
            const json_t *r = math_fourier_fields(lab_math_format("{%s{%s} | %s=-0.7}", op, bodies[i], target), target);
            lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
            lab_math_close(math_fourier_number(r), math_fourier_quadrature(-0.5, -0.7, 2, 0.3, d, false, 0), 2e-9);
        }
    }
}

static const char *math_fourier_optional_text(const json_t *r, const char *name)
{
    string_t *key = string_new();
    string_append_utf8_exact(key, name, strlen(name));
    const string_t *value = json_string_value(json_object_get(r, key));
    string_free(key);
    return value ? string_c_str(value) : "";
}

static void test_growth_and_singularities_are_not_unsupported_formula_notes(void)
{
    const char *growing[] = {"sinh(2*t+1)^(1/2)", "abs(sinh(t))^(1/2)", "sinh(t)^(1+i)"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{%s}", growing[i]), "ω");
        lab_math_contains(math_fourier_text(r, "value_note"), "No ordinary Fourier transform", true);
        lab_math_contains(math_fourier_text(r, "value_note"), "grows exponentially", true);
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        lab_math_equal(math_fourier_text(r, "value"), "NAN");
        lab_math_contains(math_fourier_text(r, "function"), "return NAN.", true);
    }
    const char *singular[] = {"sinh(t)^(-2)", "cosech(t)^2"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{%s}", singular[i]), "ω");
        lab_math_contains(math_fourier_text(r, "value_note"), "non-integrable singularity", true);
        lab_math_contains(math_fourier_text(r, "value_note"), "prescription", true);
    }
    const char *odd[] = {"sinh(t)^(-1)", "1/sinh(t)", "cosech(2*t+1)"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{%s}", odd[i]), "ω");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        lab_math_contains(math_fourier_text(r, "value_note"), "symmetric cancellation", true);
    }
    const json_t *r = math_fourier_fields("@F{sinh(a*t+b)^2}", "ω");
    lab_math_contains(math_fourier_text(r, "value_note"), "Extended Fourier transform", true);
    lab_math_contains(math_fourier_text(r, "function"), "analytic_delta(", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{sinh(t)^i}", "ω"), "value_note"),
        "No ordinary Fourier transform", false);
    lab_math_contains(math_fourier_optional_text(math_fourier_fields("@F{sinh(i*t)^2}", "ω"), "value_note"),
        "grows exponentially", false);
    r = math_fourier_fields("@F{isqrt(sinh(t))}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", true);
    lab_math_contains(math_fourier_text(r, "function"), "beta(", false);
}

static void test_constant_cases_and_binding_specialisation(void)
{
    const char *sources[] = {"{@F{sinh(a*t+b)^n} | ω=?; n=0}", "@F{sinh(0*t+1)^(-1/2)}"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(sources[i], "ω");
        lab_math_contains(math_fourier_text(r, "expression"), "δ(ω)", true);
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    }
    lab_math_contains(math_fourier_text(math_fourier_fields("{@F{sinh(a*t+b)^n} | ω=?; a=1; b=0; n=2}", "ω"), "value_note"),
                      "Extended Fourier transform", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("{@F{sinh(a*t+b)^n} | ω=?; a=1; b=0; n=-2}", "ω"), "value_note"),
                      "non-integrable singularity", true);
    const json_t *r = math_fourier_fields("{@F{sinh(a*t+b)^n} | ω=?; a=-2; b=1; n=-1/2}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "function"), "const n", false);
    lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "ω"), "tex"),
        math_fourier_text(r, "tex"));
}

static void test_proven_growth_is_rejected_in_both_directions(void)
{
    const char *operators[] = {"Fourier", "InverseFourier"};
    const char *bodies[] = {"sinh(x)^(1/2)", "abs(sinh(x))^(1/2)", "sinh(x)^(1+i)"};
    for (size_t d = 0; d < 2; ++d)
        for (size_t b = 0; b < 3; ++b)
            for (size_t binding = 0; binding < 2; ++binding) {
                const char *source = lab_math_format("%s(%s,x,k)", operators[d], bodies[b]);
                const json_t *r = math_fourier_fields(binding ? lab_math_format("{%s | k=1}", source) : source, "k");
                lab_math_equal(math_fourier_text(r, "value"), "NAN");
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                lab_math_contains(math_fourier_text(r, "value_note"), "grows exponentially", true);
            }
    const char *constants[] = {"sinh(0*x)", "sinh(x)^0", "cosh(0*x+1)"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("Fourier(%s,x,k)", constants[i]), "k");
        lab_math_contains(math_fourier_optional_text(r, "value_note"), "grows exponentially", false);
        string_t *function = string_new_with(math_fourier_text(r, "function"));
        string_offset_t end = string_find(function, "else");
        string_t *first = string_substr(function, 0, end < 0 ? string_byte_length(function) : (size_t)end);
        lab_math_contains(string_c_str(first), "return @nan.", false);
        string_free(first);
        string_free(function);
    }
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{c*sinh(x)}", "k"), "function"), "analytic_delta(", true);
    lab_math_check(math_fourier_number(math_fourier_fields("{Fourier(c*sinh(x),x,k) | k=1; c=0}", "k")) == 0,
        "zero multiplier cancels growth");
    lab_math_check(math_fourier_number(math_fourier_fields("{Fourier(sinh(x)-sinh(x),x,k) | k=1}", "k")) == 0,
        "subtraction cancels growth");
}

static void test_readme_hyperbolic_fourier_examples(void)
{
    /* README examples: docs/expression.md, powers and odd reciprocal. */
    lab_math_close(math_fourier_number(math_fourier_fields("{@F{sinh(t)^(-1/2)} | ω=0}", "ω")),
        3.708149354602744-3.708149354602744*I, 1e-14);
    const json_t *r = math_fourier_fields("@F{sinh(t)^2}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_equal(math_fourier_text(r, "value"), "NAN");
    lab_math_contains(math_fourier_text(r, "value_note"), "Extended Fourier transform", true);
    r = math_fourier_fields("@F{sinh(t)^(-1)}", "ω");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "expression"), "tanh", true);
    lab_math_contains(math_fourier_text(r, "value_note"), "symmetric cancellation", true);
    lab_math_close(math_fourier_number(math_fourier_fields("{@F{sinh(t)^(-1)} | ω=0.4}", "ω")), -I*M_PI*tanh(M_PI*0.4/2), 1e-12);
}

static void test_readme_inverse_beta_spectrum(void)
{
    /* README examples: docs/expression.md, copied beta spectrum. */
    const char *source = "@Finv{(B(1/4+i*ω/2,1/2)-i*B(1/4-i*ω/2,1/2))/sqrt(2)}";
    lab_math_contains(math_fourier_text(math_fourier_fields(source, "t"), "function"), "fourier(", false);
    const double points[] = {-0.7, 0.7};
    for (size_t i = 0; i < 2; ++i)
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{%s | t=%.17g}", source, points[i]), "t")),
                       cpow(CMPLX(sinh(points[i]), 0), -0.5), 1e-12);
}

/* Register ordinary regressions. */
void test_lab_math_fourier_hyperbolic_cases(void)
{
    TEST_RUN_IN_GROUP(test_copied_beta_spectrum_recovers_shifted_sinh, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_hyperbolic_beta_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_beta_duality_and_reverse_round_trip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_negative_imaginary_products_survive_expression_round_trip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_beta_round_trip_with_complex_power_and_swapped_arguments, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_beta_pair_does_not_accept_near_misses_or_invalid_domains, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_beta_symbol_and_callable_aliases, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_requested_symbolic_power_and_conditions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_sinh_powers_against_independent_quadrature, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_complex_exponent_in_convergence_strip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_reciprocals_and_absolute_powers_preserve_branches, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_cosh_and_sech_powers, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_growth_and_singularities_are_not_unsupported_formula_notes, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_constant_cases_and_binding_specialisation, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_proven_growth_is_rejected_in_both_directions, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_hyperbolic_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_hyperbolic_fourier_examples, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_inverse_beta_spectrum, readme_examples, "math,readme,output");
    lab_math_reset();
}
