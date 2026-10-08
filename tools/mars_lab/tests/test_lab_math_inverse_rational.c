/**
 * @file test_lab_math_inverse_rational.c
 * @brief Exact rational inverse Laplace and equation-copy regression port.
 *
 * Preserves test_inverse_laplace_rational.py, including all repeated factors,
 * independent convergent series, initial conditions and unsupported spectra.
 * The documented equation result runs separately in the README phase.
 */
#include <math.h>

#include "ustring.h"
#include "test_lab_math_support.h"

static const json_t *lab_math_inverse_rational_fields(const char *source)
{
    return lab_math_fields(source, "t", "evaluate", 40);
}

/* Preserve float(...)'s rejection of complex syntax, including zero or underflowed imaginary parts. */
static double lab_math_inverse_rational_real(const json_t *fields, const char *key)
{
    const char *raw = lab_math_text(fields, key);
    string_t *text = string_new_with(raw);
    if (!lab_math_check(text != NULL, "allocate rational real-only numerical field"))
        return NAN;
    string_trim(text);
    bool real_only = !string_ends_with(text, "i");
    string_free(text);
    if (!lab_math_check(real_only, lab_math_format("expected real-only %s field: %s", key, raw)))
        return NAN;
    return creal(lab_math_parse_number(raw));
}

static double lab_math_inverse_rational_r0(double t) { return 1 - cos(t); }
static double lab_math_inverse_rational_r1(double t) { return (exp(-t) - cos(t) + sin(t)) / 2; }
static double lab_math_inverse_rational_r2(double t) { return (sin(t) - sin(2 * t) / 2) / 3; }
static double lab_math_inverse_rational_r3(double t) { return (sin(t) - t * cos(t)) / 2; }
static double lab_math_inverse_rational_r4(double t) { return ((3 - t * t) * sin(t) - 3 * t * cos(t)) / 8; }
static double lab_math_inverse_rational_r5(double t) { return (sin(2 * t) - 2 * t * cos(2 * t)) / 16; }
static double lab_math_inverse_rational_r6(double t) { return exp(-2 * t) * lab_math_inverse_rational_r5(t); }
static double lab_math_inverse_rational_r7(double t) { return t * sin(t) / 2; }
static double lab_math_inverse_rational_r8(double t) { return (t * cosh(t) - sinh(t)) / 2; }
static double lab_math_inverse_rational_r9(double t) { return t * t * t * exp(-t) / 6; }
static double lab_math_inverse_rational_r10(double t) { return t * t * exp(-t) / 2; }
static double lab_math_inverse_rational_r11(double t) { return t - 1 + exp(-t); }
static double lab_math_inverse_rational_r12(double t) { return lab_math_inverse_rational_r1(t) / 6; }

static const char *rational_expected = "10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8";

static void test_inverse_rational_equation(void)
{
    const char *operands[] = {
        "-5*(-s-10/s^2-3)/(s*(s+4)+5)", "(5*s+15+50/s^2)/(s^2+4*s+5)",
        "(5*s^3+15*s^2+50)/(s^2*(s^2+4*s+5))", "(5*s^3+15*s^2+50)/(s^4+4*s^3+5*s^2)",
        "10/s^2-8/s+(13*(s+2)+11)/((s+2)^2+1)",
    };
    for (size_t i = 0; i < 5; ++i) {
        const json_t *result = lab_math_inverse_rational_fields(lab_math_format("@Linv{%s}", operands[i]));
        lab_math_equal(lab_math_text(result, "unbound"), rational_expected);
        lab_math_contains(lab_math_text(result, "function"), "inverselaplace(", false);
        lab_math_contains(lab_math_text(result, "transform_identity_TeX"), " = ", true);
        lab_math_equal(lab_math_text(lab_math_inverse_rational_fields(lab_math_text(result, "unbound")), "tex"),
            lab_math_text(result, "tex"));
    }
    int status = -1;
    const char *raw = "";
    const json_t *solved = lab_math_worker("equation_lab", "{(s^2+4*s+5)*Y=5*s+15+50/s^2 | Y=?; s=?}",
                                          "t", "evaluate", 40, &status, &raw);
    lab_math_check(status == 0, raw);
    const char *operand = lab_math_after(lab_math_text(solved, "solutions"), " = ");
    lab_math_equal(operand, "5/(s·(s + 4) + 5)·(s + 10/s² + 3)");
    lab_math_contains(lab_math_text(solved, "solutions_TeX"), "\\times 1", false);
    lab_math_contains(lab_math_text(solved, "solutions_TeX"), "&= -", false);
    const json_t *simplified = lab_math_inverse_rational_fields(operand);
    lab_math_equal(lab_math_text(simplified, "unbound"), operand);
    lab_math_contains(lab_math_text(solved, "solutions_TeX"), lab_math_text(simplified, "tex"), true);
    lab_math_equal(lab_math_text(lab_math_inverse_rational_fields(lab_math_format("@Linv{%s}", operand)), "unbound"),
        rational_expected);
}

static void test_inverse_rational_factors(void)
{
    const struct { const char *operand; double (*reference)(double); } cases[] = {
        {"1/(s*(s^2+1))", lab_math_inverse_rational_r0}, {"1/((s+1)*(s^2+1))", lab_math_inverse_rational_r1},
            {"1/((s^2+1)*(s^2+4))", lab_math_inverse_rational_r2},
        {"1/(s^2+1)^2", lab_math_inverse_rational_r3}, {"1/(s^2+1)^3", lab_math_inverse_rational_r4}, {"1/(s^2+4)^2",
            lab_math_inverse_rational_r5},
        {"1/((s+2)^2+4)^2", lab_math_inverse_rational_r6}, {"s/(s^2+1)^2", lab_math_inverse_rational_r7},
            {"1/(s^2-1)^2", lab_math_inverse_rational_r8},
        {"1/(s^2+2*s+1)^2", lab_math_inverse_rational_r9}, {"1/((s+1)*(s^2+2*s+1))", lab_math_inverse_rational_r10},
            {"1/(s*(s^2+s))", lab_math_inverse_rational_r11},
        {"1/((2*s+2)*(3*s^2+3))", lab_math_inverse_rational_r12}, {"(1+1/s)^2/(s+1)^3", lab_math_inverse_rational_r11},
        {"1/(s/(s^2+1))/((s^2+1)^2)", lab_math_inverse_rational_r0},
    };
    const double times[] = {0, 0.5, 1.25};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        const json_t *symbolic = lab_math_inverse_rational_fields(lab_math_format("@Linv{%s}", cases[i].operand));
        lab_math_contains(lab_math_text(symbolic, "function"), "inverselaplace(", false);
        for (size_t j = 0; j < 3; ++j) {
            const json_t *actual = lab_math_inverse_rational_fields(lab_math_format("{@Linv{%s} | t=%.17g}",
                cases[i].operand, times[j]));
            lab_math_places(lab_math_inverse_rational_real(actual, "value"), cases[i].reference(times[j]), 12);
        }
    }
}

static void test_inverse_rational_series(void)
{
    const double times[] = {0.5, 1.25};
    for (int power = 4; power <= 8; power += 4) {
        const json_t *inverse = lab_math_inverse_rational_fields(lab_math_format("@Linv{1/(s^2+1)^%d}", power));
        lab_math_contains(lab_math_text(inverse, "function"), "inverselaplace(", false);
        for (size_t j = 0; j < 2; ++j) {
            double term = pow(times[j], 2 * power - 1) / tgamma(2 * power);
            double expected = term;
            for (int k = 1; k < 30; ++k) {
                term *= -(power + k - 1.0) / k * times[j] * times[j] /
                         ((2 * power + 2 * k - 2.0) * (2 * power + 2 * k - 1.0));
                expected += term;
            }
            const json_t *actual = lab_math_inverse_rational_fields(lab_math_format("{@Linv{1/(s^2+1)^%d} | t=%.17g}",
                power, times[j]));
            lab_math_places(lab_math_inverse_rational_real(actual, "value") / expected, 1, 11);
        }
        if (power == 4) {
            const json_t *forward = lab_math_inverse_rational_fields(lab_math_format("{@L{%s} | s=3}",
                lab_math_text(inverse, "unbound")));
            lab_math_places(lab_math_inverse_rational_real(forward, "value") * pow(10, power), 1, 11);
        }
    }
}

static void test_inverse_rational_parameters(void)
{
    const char *source = "@Linv((a*p+b)/(p*(p^2+1)),p,x)";
    lab_math_contains(lab_math_text(lab_math_inverse_rational_fields(source), "function"), "inverselaplace(", false);
    const int parameters[][2] = {{2, 3}, {-3, 1}};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *bound = lab_math_inverse_rational_fields(lab_math_format("{%s | x=0.75; a=%d,b=%d}", source,
            parameters[i][0], parameters[i][1]));
        double expected = parameters[i][0] * sin(0.75) + parameters[i][1] * (1 - cos(0.75));
        lab_math_places(lab_math_inverse_rational_real(bound, "value"), expected, 12);
    }
    source = "{@Linv{-5*(-s-10/s^2-3)/(s*(s+4)+5)} | t=0}";
    lab_math_places(lab_math_inverse_rational_real(lab_math_inverse_rational_fields(source), "value"), 5, 7);
    const json_t *derivative = lab_math_fields(source, "t", "derivative", 40);
    lab_math_places(lab_math_inverse_rational_real(derivative, "derivative_value"), -5, 7);
    const char *unsupported[] = {"@Linv{1/((s-a)*(s-b)*(s^2+1))}", "@Linv{1/(s^2+a)^2}",
                                 "@Linv{(s+1)/s}", "@Linv{1/(s^2+1)^9}"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_contains(lab_math_text(lab_math_inverse_rational_fields(unsupported[i]), "function"), "inverselaplace(", true);
}

static void test_inverse_rational_readme(void)
{
    /* README example: docs/expression.md, copied Equation-mode rational inverse. */
    lab_math_equal(lab_math_text(lab_math_inverse_rational_fields("@Linv{-5*(-s-10/s^2-3)/(s*(s+4)+5)}"), "unbound"),
        rational_expected);
}

/* Register complete rational inverse regression groups. */
void test_lab_math_inverse_rational_cases(void)
{
    TEST_RUN_IN_GROUP(test_inverse_rational_equation, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_inverse_rational_factors, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_inverse_rational_series, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_inverse_rational_parameters, tests, NULL);
    lab_math_reset();
}

/* Register the rational inverse README output after ordinary regressions. */
void test_lab_math_inverse_rational_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_inverse_rational_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
