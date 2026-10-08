/**
 * @file test_lab_math_style.c
 * @brief Registered derivative and transform Function-style regression ports.
 *
 * Preserves test_derivative_function_style.py and test_transform_function_style.py,
 * including reparsing canonical calls and separately registered README examples.
 */
#include <string.h>

#include "test_lab_math_support.h"

static const char *const transforms[][3] = {
    {"Fourier(f(t), t, @omega)", "fourier(f(t), t, @omega)", "@omega"},
    {"InverseFourier(F(@omega), @omega, t)", "inversefourier(F(@omega), @omega, t)", "t"},
    {"Laplace(f(t), t, s)", "laplace(f(t), t, s)", "s"},
    {"InverseLaplace(F(s), s, t)", "inverselaplace(F(s), s, t)", "t"},
};

static const char *const derivatives[][2] = {
    {"Dk(step(k)*ln(abs(k)))", "derivative(step(k).ln(abs(k)), k, 1)"},
    {"{Dkk(delta(k)) | k=?}", "derivative(delta(k), k, 2)"},
    {"Dω(delta(ω))", "derivative(delta(@omega), @omega, 1)"},
};

static void test_derivative_function_style(void)
{
    for (size_t i = 0; i < 3; ++i) {
        const json_t *result = lab_math_fields(derivatives[i][0], "k", "evaluate", 40);
        lab_math_contains(lab_math_text(result, "function"), derivatives[i][1], true);
        lab_math_regex(lab_math_text(result, "function"), "(^|[^[:alnum:]_])D(k|x|y|@omega)+\\(", false);
        const json_t *copy = lab_math_fields(lab_math_replace(derivatives[i][1], ".ln", "*ln"), "k", "evaluate", 40);
        lab_math_equal(lab_math_text(copy, "tex"), lab_math_text(result, "tex"));
    }
    const char *spectrum = "2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)"
                           "-besselj(0,k)*derivative(step(k)*ln(abs(k)),k,1))";
    const json_t *result = lab_math_fields(lab_math_format("InverseFourier(%s,k,x)", spectrum), "x", "evaluate", 40);
    lab_math_contains(lab_math_text(result, "expression"), "asin(x)", true);
}

static void test_transform_function_style(void)
{
    const char *keys[] = {"function", "expression", "tex"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *original = lab_math_fields(transforms[i][0], transforms[i][2], "evaluate", 40);
        const json_t *copied = lab_math_fields(transforms[i][1], transforms[i][2], "evaluate", 40);
        lab_math_contains(lab_math_text(original, "function"), lab_math_format("return %s.", transforms[i][1]), true);
        for (size_t k = 0; k < 3; ++k)
            lab_math_equal(lab_math_text(original, keys[k]), lab_math_text(copied, keys[k]));
    }
    const char *resolved[][3] = {
        {"Fourier(exp(-t^2),t,k)", "fourier(exp(-t^2),t,k)", "k"},
        {"InverseFourier(exp(-k^2),k,t)", "inversefourier(exp(-k^2),k,t)", "t"},
        {"Laplace(t,t,s)", "laplace(t,t,s)", "s"},
        {"InverseLaplace(1/s^2,s,t)", "inverselaplace(1/s^2,s,t)", "t"},
    };
    for (size_t i = 0; i < 4; ++i) {
        const json_t *original = lab_math_fields(resolved[i][0], resolved[i][2], "evaluate", 40);
        const json_t *copied = lab_math_fields(resolved[i][1], resolved[i][2], "evaluate", 40);
        lab_math_equal(lab_math_text(original, "tex"), lab_math_text(copied, "tex"));
        string_t *source = string_new_with(resolved[i][1]);
        string_offset_t end = string_find(source, "(");
        lab_math_contains(lab_math_text(copied, "function"),
                          lab_math_format("%.*s", (int)(end + 1), resolved[i][1]), false);
        string_free(source);
    }
}

static void test_style_readme(void)
{
    /* README examples: docs/expression.md, derivative and transform Function bodies. */
    for (size_t i = 0; i < 2; ++i) {
        const json_t *result = lab_math_fields(derivatives[i][0], "k", "evaluate", 40);
        lab_math_contains(lab_math_text(result, "function"), lab_math_format("return %s.", derivatives[i][1]), true);
    }
    for (size_t i = 0; i < 4; ++i) {
        const json_t *result = lab_math_fields(transforms[i][0], transforms[i][2], "evaluate", 40);
        lab_math_contains(lab_math_text(result, "function"), lab_math_format("return %s.", transforms[i][1]), true);
    }
}

/* Register both original ordinary suites before any README examples. */
void test_lab_math_style_cases(void)
{
    TEST_RUN_IN_GROUP(test_derivative_function_style, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_transform_function_style, tests, NULL);
    lab_math_reset();
}

/* Run the documented Function-body cases in the final README phase. */
void test_lab_math_style_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_style_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
