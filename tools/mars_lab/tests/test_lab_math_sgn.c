/**
 * @file test_lab_math_sgn.c
 * @brief Native sign-function regression port, including executable Function cards.
 *
 * Preserves all cases in test_sgn.py: aliases, domains, Fourier and Laplace
 * transforms, copied inverses, calculus, finite sums and README renderings.
 */
#include <math.h>

#include "test_lab_math_support.h"

static const json_t *lab_math_sgn_fields(const char *source, const char *variable)
{
    return lab_math_fields(source, variable, "evaluate", 40);
}

static void test_sgn_aliases_and_real_domain(void)
{
    const char *aliases[] = {"sgn", "sign", "signum"};
    const char *points[][2] = {{"-3", "-1"}, {"0", "0"}, {"3", "1"}, {"1e-80", "1"}};
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 4; ++j)
            lab_math_equal(lab_math_text(lab_math_sgn_fields(lab_math_format("%s(%s)", aliases[i], points[j][0]), "x"), "value"),
                           points[j][1]);
        const json_t *result = lab_math_sgn_fields(lab_math_format("%s(x)", aliases[i]), "x");
        lab_math_contains(lab_math_text(result, "unbound"), "sgn(x)", true);
        lab_math_contains(lab_math_text(result, "function"), "sgn(x)", true);
        lab_math_contains(lab_math_text(result, "tex"), "\\operatorname{sgn}", true);
        lab_math_equal(lab_math_text(lab_math_sgn_fields(lab_math_format("{c+x | c=%s(-3); x=2}", aliases[i]), "x"),
            "value"), "1");
    }
    lab_math_equal(lab_math_text(lab_math_sgn_fields("sgn(i)", "x"), "value"), "NAN");
}

static void test_sgn_fourier(void)
{
    const json_t *result = lab_math_sgn_fields("@F{sgn(t)}", "ω");
    lab_math_contains(lab_math_text(result, "function"), "fourier(", false);
    lab_math_contains(lab_math_text(result, "expression"), "principal value", false);
    lab_math_contains(lab_math_text(result, "expression"), "ω ≠ 0", true);
    lab_math_contains(lab_math_text(result, "expression"), "ω ∈ ℝ", true);
    const int frequencies[] = {-3, -1, 1, 77};
    for (size_t i = 0; i < 4; ++i) {
        double complex value = lab_math_number(lab_math_sgn_fields(lab_math_format("{@F{sgn(t)} | ω=%d}",
            frequencies[i]), "ω"), "value");
        lab_math_places(cimag(value), -2.0 / frequencies[i], 13);
    }
    lab_math_equal(lab_math_text(lab_math_sgn_fields("{@F{sgn(t)} | ω=0}", "ω"), "value"), "NAN");
    const json_t *inverse = lab_math_sgn_fields(lab_math_format("InverseFourier(%s,ω,t)", lab_math_text(result, "unbound")), "t");
    lab_math_contains(lab_math_text(inverse, "unbound"), "sgn(t)", true);
    for (int point = -2; point <= 2; point += 2)
        lab_math_close(lab_math_number(lab_math_sgn_fields(lab_math_format("{InverseFourier(-2i/ω,ω,t) | t=%d}", point),
            "t"), "value"),
                       point < 0 ? -1 : point > 0 ? 1 : 0, 1e-300);
    const char *arguments[] = {"t", "-t", "2t+3", "-2t+3"};
    for (size_t i = 0; i < 4; ++i) {
        result = lab_math_sgn_fields(lab_math_format("@F{sgn(%s)}", arguments[i]), "ω");
        for (int point = -3; point <= 3; point += 3) {
            inverse = lab_math_sgn_fields(lab_math_format("{InverseFourier(%s,ω,t) | t=%d}", lab_math_text(result,
                "unbound"), point), "t");
            const json_t *original = lab_math_sgn_fields(lab_math_format("{sgn(%s) | t=%d}", arguments[i], point), "t");
            lab_math_equal(lab_math_text(inverse, "value"), lab_math_text(original, "value"));
        }
    }
    const char *conditions[] = {"Re(ω)>0", "ω-1 != 0", "a.ω != 0"};
    for (size_t i = 0; i < 3; ++i) {
        result = lab_math_sgn_fields(lab_math_format("InverseFourier(-2i/ω where (%s),ω,t)", conditions[i]), "t");
        lab_math_contains(lab_math_text(result, "function"), "inversefourier(", true);
    }
}

static void test_sgn_calculus(void)
{
    const char *arguments[] = {"t", "-t", "t-1", "1-t"};
    const double expected[] = {0.5, -0.5, -0.5 + exp(-2), 0.5 - exp(-2)};
    for (size_t i = 0; i < 4; ++i)
        lab_math_places(lab_math_number(lab_math_sgn_fields(lab_math_format("{@L{sgn(%s)} | s=2}", arguments[i]), "s"), "value"),
                        expected[i], 13);
    lab_math_contains(lab_math_text(lab_math_sgn_fields("Dx(sgn(x))", "x"), "unbound"), "2·δ(x)", true);
    for (int point = -2; point <= 2; point += 2)
        lab_math_equal(lab_math_text(lab_math_sgn_fields(lab_math_format("{Dx(sgn(x)) | x=%d}", point), "x"), "value"),
                       point ? "0" : "NAN");
    const char *sums[][2] = {{"sum(k,-2,2,sgn(k))", "0"}, {"sum(k,-2,3,sgn(k))", "1"},
                            {"sum(k,0,4,sgn(2k-3))", "1"}, {"sum(k,-1000000,1000001,sgn(k))", "1"}};
    for (size_t i = 0; i < 4; ++i)
        lab_math_equal(lab_math_text(lab_math_sgn_fields(sums[i][0], "x"), "value"), sums[i][1]);
    const json_t *primitive = lab_math_sgn_fields("@S sgn(2x-1) dx", "x");
    const char *fragments[] = {"|2x - 1|", "C", "x ∈ ℝ"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(lab_math_text(primitive, "unbound"), fragments[i], true);
    const double values[] = {5.5, 3.5, 4.5};
    for (int i = 0; i < 3; ++i)
        lab_math_close(lab_math_number(lab_math_sgn_fields(lab_math_format("{@S sgn(2x-1) dx | x=%d; C=3}", 2 * i - 2),
            "x"), "value"),
                       values[i], 1e-300);
    const char *bounds[][2] = {{"-2", "2"}, {"0", "0"}, {"2", "2"}, {"i", "NAN"}};
    for (size_t i = 0; i < 4; ++i)
        lab_math_equal(lab_math_text(lab_math_sgn_fields(lab_math_format("{@S^x_0 sgn(t) dt | x=%s}", bounds[i][0]),
            "x"), "value"),
                       bounds[i][1]);
}

static void test_sgn_function_run(void)
{
    const json_t *result = lab_math_sgn_fields("{sgn(x) | x=-2}", "x");
    lab_math_equal(lab_math_programme(lab_math_text(result, "function"), 40), "-1");
    result = lab_math_sgn_fields("{@F{sgn(t)} | ω=77}", "ω");
    double complex value = lab_math_parse_number(lab_math_programme(lab_math_text(result, "operation_function"), 40));
    lab_math_places(cimag(value), -2.0 / 77, 13);
}

static void test_sgn_readme(void)
{
    /* README examples: docs/expression.md, sign function and transforms. */
    const char *cases[][3] = {{"sgn(-2)", "value", "-1"}, {"sgn(0)", "value", "0"},
                              {"@F{sgn(t)}", "unbound", "-2i/ω where (ω ∈ ℝ; ω ≠ 0)"},
                              {"InverseFourier(-2i/ω,ω,t)", "unbound", "sgn(t) where (t ∈ ℝ)"},
                              {"sum(k,-2,3,sgn(k))", "value", "1"}};
    for (size_t i = 0; i < 5; ++i)
        lab_math_equal(lab_math_text(lab_math_sgn_fields(cases[i][0], "x"), cases[i][1]), cases[i][2]);
}

/* Register sign-function regressions, releasing each case's fixture arena. */
void test_lab_math_sgn_cases(void)
{
    TEST_RUN_IN_GROUP(test_sgn_aliases_and_real_domain, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_sgn_fourier, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_sgn_calculus, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_sgn_function_run, tests, NULL);
    lab_math_reset();
}

/* Run sign-function README examples after all ordinary suites. */
void test_lab_math_sgn_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_sgn_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
