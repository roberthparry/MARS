/**
 * @file test_lab_math_imaginary.c
 * @brief End-to-end imaginary-axis formula-card regressions.
 *
 * Exercises the built-in native expression worker, including its repeated display expansion and Protobuf
 * result fields. The browser must receive simplified algebra rather than remove spurious zero terms itself.
 */
#include <stdio.h>

#include "expression-t.h"
#include "test_lab_math_support.h"

static void test_imaginary_axis_formula_cards(void)
{
    const char *sources[] = {"cos(i)", "sin(i)", "tan(i)", "sinh(i)", "cosh(i)", "tanh(i)",
                             "asin(i)", "acos(i)", "atan(i)", "atan(-i)"};
    const char *functions[] = {"cosh", "sinh", "tanh", "sin", "cos", "tan", "asinh", "asinh", "atan", "atan"};
    const char *fields[] = {"expression", "function", "tex"};
    for (size_t k = 0; k < sizeof(sources) / sizeof(*sources); ++k) {
        const json_t *result = lab_math_fields(sources[k], "x", "evaluate", 116);
        for (size_t f = 0; f < sizeof(fields) / sizeof(*fields); ++f) {
            const char *text = lab_math_text(result, fields[f]);
            lab_math_contains(text, "0i", false);
            lab_math_contains(text, "0.i", false);
            lab_math_contains(text, "NAN", false);
            if (f < 2u)
                lab_math_contains(text, functions[k], true);
        }
        if (k == 0u)
            lab_math_equal(lab_math_text(result, "expression"), "cosh(1)");
        lab_math_reset();
    }
}

static void test_symbolic_imaginary_axis_formula_cards(void)
{
    const char *functions[] = {"sin", "cos", "tan", "sinh", "cosh", "tanh",
                               "sec", "cosec", "cot", "sech", "cosech", "coth"};
    const char *arguments[] = {"ix", "x*i", "-i*x", "i*(x+y)", "i*x/2"};
    const char *fields[] = {"expression", "function", "tex"};
    for (size_t k = 0; k < sizeof(functions) / sizeof(*functions); ++k) {
        for (size_t a = 0; a < sizeof(arguments) / sizeof(*arguments); ++a) {
            const json_t *result = lab_math_fields(lab_math_format("%s(%s)", functions[k], arguments[a]),
                                                   "x", "evaluate", 116);
            for (size_t f = 0; f < sizeof(fields) / sizeof(*fields); ++f) {
                const char *text = lab_math_text(result, fields[f]);
                lab_math_contains(text, "0 +", false);
                lab_math_contains(text, "0i", false);
                lab_math_contains(text, "0.i", false);
                lab_math_contains(text, "NAN", false);
            }
            lab_math_contains(lab_math_text(result, "expression"), "x = ?", true);
            lab_math_reset();
        }
    }
    const json_t *result = lab_math_fields("sinh(ix)", "x", "evaluate", 116);
    lab_math_equal(lab_math_text(result, "expression"), "{ sin(x)·i | x = ? }");
    lab_math_reset();
}

static void test_symbolic_imaginary_axis_bound_values(void)
{
    static const struct {
        const char *name;
        double complex (*reference)(double complex);
    } functions[] = {
        {"sin", csin}, {"cos", ccos}, {"tan", ctan},
        {"sinh", csinh}, {"cosh", ccosh}, {"tanh", ctanh},
        {"asin", casin}, {"acos", cacos}, {"atan", catan},
        {"asinh", casinh}, {"acosh", cacosh}, {"atanh", catanh},
        {"asec", NULL}, {"acosec", NULL}, {"acot", NULL},
        {"asech", NULL}, {"acosech", NULL}, {"acoth", NULL},
    };
    const double complex values[] = {0.5, 2, -2, 0.5 + 0.25 * I, -0.5 - 0.25 * I};
    for (size_t k = 0; k < sizeof(functions) / sizeof(*functions); ++k) {
        const json_t *symbolic = lab_math_fields(lab_math_format("%s(i*x)", functions[k].name),
                                                "x", "evaluate", 40);
        const char *algebra = lab_math_algebra(symbolic);
        for (size_t v = 0; v < sizeof(values) / sizeof(*values); ++v) {
            const char *bound = lab_math_format("{%s | x=%.17g+(%.17g)*i}", algebra,
                                                creal(values[v]), cimag(values[v]));
            const json_t *result = lab_math_fields(bound, "x", "evaluate", 40);
            expr_t *original = expr_from_string(lab_math_format("{%s(i*x) | x=%.17g+(%.17g)*i}",
                                                functions[k].name, creal(values[v]), cimag(values[v])), NULL);
            number_t expected = expr_eval(original);
            number_t real = num_real_part(expected);
            number_t imaginary = num_imag_part(expected);
            double complex wanted = num_to_double(real) + I * num_to_double(imaginary);
            lab_math_close(lab_math_number(result, "value"), wanted, 2e-12);
            /* Off the cuts, also compare an independent C complex implementation. */
            if (functions[k].reference && (v == 0 || v >= 3))
                lab_math_close(wanted, functions[k].reference(I * values[v]), 2e-12);
            num_destroy(&imaginary);
            num_destroy(&real);
            num_destroy(&expected);
            expr_free(original);
        }
        lab_math_reset();
    }
}

/* Register native formula-card regressions before README examples. */
void test_lab_math_imaginary_cases(void)
{
    TEST_RUN_IN_GROUP(test_imaginary_axis_formula_cards, tests, NULL);
    TEST_RUN_IN_GROUP(test_symbolic_imaginary_axis_formula_cards, tests, NULL);
    TEST_RUN_IN_GROUP(test_symbolic_imaginary_axis_bound_values, tests, NULL);
    lab_math_reset();
}

/* README example: expression guide, symbolic imaginary-axis presentation. */
static void example_symbolic_imaginary_axis(void)
{
    const json_t *result = lab_math_fields("sinh(ix)", "x", "evaluate", 116);
    const char *text = lab_math_text(result, "expression");
    lab_math_equal(text, "{ sin(x)·i | x = ? }");
    printf("%s\n", text);
}

/* Register the documented output after ordinary Lab cases. */
void test_lab_math_imaginary_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_symbolic_imaginary_axis, readme_examples, "expression,readme,output");
    lab_math_reset();
}
