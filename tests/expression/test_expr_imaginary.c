/**
 * @file test_expr_imaginary.c
 * @brief Imaginary-axis identities and principal-branch presentation regressions.
 *
 * Exercises the native display pipeline shared by Lab cards at double and extended precision. Numerical
 * comparisons retain complex signs and principal branches; poles must remain symbolic rather than introduce
 * undefined Cartesian subexpressions. This is test support, not part of the installed library.
 */

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

#include "test_expr.h"

static expr_t *imaginary_display(const expr_t *input)
{
    expr_t *simple = expr_simplify(input);
    expr_t *expanded = expr_complex_unary_cartesian_for_display(simple);
    expr_t *display = expr_beautify_presimplified(expanded ? expanded : simple);

    expr_free(expanded);
    expr_free(simple);
    return display;
}

static void test_imaginary_axis_identities(void)
{
    static const struct {
        const char *input;
        const char *identity;
        const char *function;
    } cases[] = {
        {"cos(i)", "cosh(1)", "cosh"},
        {"sin(i)", "i*sinh(1)", "sinh"},
        {"tan(i)", "i*tanh(1)", "tanh"},
        {"sinh(i)", "i*sin(1)", "sin"},
        {"cosh(i)", "cos(1)", "cos"},
        {"tanh(i)", "i*tan(1)", "tan"},
        {"asin(i)", "i*asinh(1)", "asinh"},
        {"acos(i)", "@pi/2-i*asinh(1)", "asinh"},
        {"cos(-i)", "cosh(1)", "cosh"},
        {"sin(-i)", "-i*sinh(1)", "sinh"},
        {"tan(-i)", "-i*tanh(1)", "tanh"},
        {"sinh(-i)", "-i*sin(1)", "sin"},
        {"cosh(-i)", "cos(1)", "cos"},
        {"tanh(-i)", "-i*tan(1)", "tan"},
        {"asin(-i)", "-i*asinh(1)", "asinh"},
        {"acos(-i)", "@pi/2+i*asinh(1)", "asinh"},
        {"sec(i)", "sech(1)", "sech"},
        {"cosec(i)", "-i*cosech(1)", "cosech"},
        {"cot(i)", "-i*coth(1)", "coth"},
        {"sech(i)", "sec(1)", "sec"},
        {"cosech(i)", "-i*cosec(1)", "cosec"},
        {"coth(i)", "-i*cot(1)", "cot"},
    };
    const size_t precisions[] = {53u, 384u, 768u};
    size_t saved = num_get_default_prec_bits();

    for (size_t p = 0; p < sizeof(precisions) / sizeof(*precisions); ++p) {
        num_set_default_prec_bits(precisions[p]);
        number_t tolerance = num_ldexp(NUM_ONE, 16 - (int)precisions[p]);
        for (size_t k = 0; k < sizeof(cases) / sizeof(*cases); ++k) {
            expr_t *input = expr_from_string(cases[k].input, NULL);
            expr_t *expected = expr_from_string(cases[k].identity, NULL);
            expr_t *display = imaginary_display(input);
            ASSERT_NOT_NULL(input);
            ASSERT_NOT_NULL(expected);
            ASSERT_NOT_NULL(display);
            number_t raw = expr_eval(input);
            number_t want = expr_eval(expected);
            number_t got = expr_eval(display);
            number_t delta = num_sub(got, want);
            number_t error = num_abs(delta);
            ASSERT_TRUE(num_lt(error, tolerance));
            num_destroy(&error);
            num_destroy(&delta);
            delta = num_sub(got, raw);
            error = num_abs(delta);
            ASSERT_TRUE(num_lt(error, tolerance));
            num_destroy(&error);
            num_destroy(&delta);
            const style_t styles[] = {style_EXPRESSION, style_FUNCTION, style_LATEX};
            for (size_t s = 0; s < sizeof(styles) / sizeof(*styles); ++s) {
                char *text = expr_to_string(display, styles[s]);
                ASSERT_NOT_NULL(text);
                if (styles[s] != style_LATEX) {
                    if (!strstr(text, cases[k].function))
                        fprintf(stderr, "%s at %zu bits rendered as %s\n", cases[k].input, precisions[p], text);
                    ASSERT_TRUE(strstr(text, cases[k].function) != NULL);
                }
                ASSERT_TRUE(strstr(text, "0i") == NULL);
                ASSERT_TRUE(strstr(text, "0.i") == NULL);
                ASSERT_TRUE(strstr(text, "NAN") == NULL);
                free(text);
            }
            num_destroy(&got);
            num_destroy(&want);
            num_destroy(&raw);
            expr_free(display);
            expr_free(expected);
            expr_free(input);
        }
        num_destroy(&tolerance);
    }
    num_set_default_prec_bits(saved);
}

static void test_imaginary_atan_poles_and_branches(void)
{
    const char *cases[] = {"atan(i)", "atan(-i)", "atan(2*i)", "atan(-2*i)",
                           "atan(1+i)", "atan(-1+i)", "asin(1+i)", "acos(-1+i)", "atan(i/2)"};
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(384u);
    number_t tolerance = num_ldexp(NUM_ONE, -360);
    for (size_t k = 0; k < sizeof(cases) / sizeof(*cases); ++k) {
        expr_t *input = expr_from_string(cases[k], NULL);
        expr_t *display = imaginary_display(input);
        ASSERT_NOT_NULL(display);
        number_t raw = expr_eval(input);
        number_t got = expr_eval(display);
        if (k < 2u) {
            ASSERT_TRUE(expr_struct_eq(input, display));
            number_t real = num_real_part(got);
            number_t imaginary = num_imag_part(got);
            ASSERT_TRUE(num_is_zero(real));
            ASSERT_TRUE(num_is_inf(imaginary));
            ASSERT_EQ_INT(num_get_sign(imaginary), k == 0u ? 1 : -1);
            num_destroy(&imaginary);
            num_destroy(&real);
        } else {
            number_t delta = num_sub(got, raw);
            number_t error = num_abs(delta);
            ASSERT_TRUE(num_lt(error, tolerance));
            num_destroy(&error);
            num_destroy(&delta);
        }
        num_destroy(&got);
        num_destroy(&raw);
        expr_free(display);
        expr_free(input);
    }
    num_destroy(&tolerance);
    num_set_default_prec_bits(saved);
}

static void test_symbolic_cartesian_zero_components(void)
{
    const char *cases[] = {"sinh(i*x)", "sin(i*x)", "tan(i*x)", "cos(i*x)", "cosh(i*x)", "tanh(i*x)",
                           "i*x", "-i*x", "(0+i*x)/2", "3*(0+i*x)", "x+0*i"};
    const style_t styles[] = {style_EXPRESSION, style_FUNCTION, style_LATEX};
    for (size_t k = 0; k < sizeof(cases) / sizeof(*cases); ++k) {
        expr_t *input = expr_from_string(cases[k], NULL);
        expr_t *display = imaginary_display(input);
        ASSERT_NOT_NULL(display);
        /* Lab separates Cartesian parts more than once; neither pass may reintroduce zeros. */
        for (unsigned pass = 0; pass < 2; ++pass) {
            expr_t *separated = expr_separate_cartesian_for_display(display);
            if (separated) {
                expr_free(display);
                display = separated;
            }
            for (size_t s = 0; s < sizeof(styles) / sizeof(*styles); ++s) {
                char *text = expr_to_string(display, styles[s]);
                ASSERT_NOT_NULL(text);
                ASSERT_TRUE(strstr(text, "0 +") == NULL);
                ASSERT_TRUE(strstr(text, "0i") == NULL);
                ASSERT_TRUE(strstr(text, "NAN") == NULL);
                free(text);
            }
        }
        expr_free(display);
        expr_free(input);
    }
    expr_t *undefined = expr_new_const(NUM_NAN);
    char *text = expr_to_string(undefined, style_EXPRESSION);
    ASSERT_TRUE(text && strcmp(text, "NAN") == 0);
    free(text);
    expr_free(undefined);
}

/* Register imaginary-axis regressions with the expression suite. */
void test_expr_t_imaginary_axis(void)
{
    TEST_RUN_SUBTEST(test_imaginary_axis_identities, NULL);
    TEST_RUN_SUBTEST(test_imaginary_atan_poles_and_branches, NULL);
    TEST_RUN_SUBTEST(test_symbolic_cartesian_zero_components, NULL);
}
