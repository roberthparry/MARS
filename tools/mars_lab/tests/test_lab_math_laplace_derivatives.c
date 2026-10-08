/**
 * @file test_lab_math_laplace_derivatives.c
 * @brief Function-derivative notation and unilateral Laplace theorem regressions.
 *
 * Ports test_laplace_function_derivatives.py, retaining prime shorthand scope,
 * symbolic orders, initial-value ordering, copied expressions and README output.
 */
#include <string.h>

#include "test_lab_math_support.h"

static const json_t *lab_math_laplace_derivatives_fields(const char *source)
{
    return lab_math_fields(source, "s", "evaluate", 40);
}

static void lab_math_laplace_derivatives_contains(const json_t *result, const char *key, const char *fragment, bool present)
{
    lab_math_contains(lab_math_text(result, key), fragment, present);
}

static void lab_math_laplace_derivatives_same(const char *left, const char *right, const char *key)
{
    lab_math_equal(lab_math_text(lab_math_laplace_derivatives_fields(left), key),
        lab_math_text(lab_math_laplace_derivatives_fields(right), key));
}

static void test_laplace_derivative_primes(void)
{
    lab_math_laplace_derivatives_same("@L(Dt(f(t)))", "@L{f'(t)}", "tex");
    for (unsigned order = 1; order <= 3; ++order) {
        const json_t *prime = lab_math_laplace_derivatives_fields(lab_math_format("@L{f%.*s(t)}", order, "'''"));
        const json_t *indexed = lab_math_laplace_derivatives_fields(lab_math_format("@L{f^(%u)(t)}", order));
        lab_math_equal(lab_math_text(prime, "tex"), lab_math_text(indexed, "tex"));
        lab_math_laplace_derivatives_contains(prime, "function", "f(0)", true);
        lab_math_laplace_derivatives_contains(prime, "function", "realpart(s)", false);
        lab_math_laplace_derivatives_contains(prime, "transform_identity_TeX", " = ", true);
    }
    const json_t *result = lab_math_laplace_derivatives_fields("@L{y''(t)+4*y'(t)+5*y(t)-50*t}");
    lab_math_laplace_derivatives_contains(result, "unbound", "ℒ(y(t), t, s)", true);
    lab_math_laplace_derivatives_contains(result, "function", "laplace(y''", false);
    lab_math_laplace_derivatives_contains(result, "function", "y'(0)", true);
    lab_math_laplace_derivatives_contains(result, "function", "y(0)", true);
    lab_math_laplace_derivatives_contains(result, "transform_identity_TeX", " = ", true);
    const char *special = lab_math_replace(lab_math_text(result, "unbound"), "ℒ(y(t), t, s)", "(2/s^3)");
    special = lab_math_replace(lab_math_replace(special, "y'(0)", "0"), "y(0)", "0");
    lab_math_places(lab_math_number(lab_math_laplace_derivatives_fields(lab_math_format("{%s | s=2}", special)),
        "value"), -8.25, 13);
    const char *linear[] = {"@L{2*u(t)+3*v(t)}", "@L{-u(t)}", "@L{u(t)/2}"};
    for (size_t i = 0; i < 3; ++i) {
        result = lab_math_laplace_derivatives_fields(linear[i]);
        lab_math_laplace_derivatives_contains(result, "function", "laplace(u(t), t, s)", true);
        lab_math_laplace_derivatives_contains(result, "function", "realpart(s)", false);
    }
}

static void test_laplace_derivative_shorthand(void)
{
    const char *pairs[][2] = {
        {"@L{y''+4y'+5y-50t}", "@L{y''(t)+4*y'(t)+5*y(t)-50*t}"},
        {"@L{5y+4y'+y''-50t}", "@L{5*y(t)+4*y'(t)+y''(t)-50*t}"},
        {"@L{y''+a*y'+b*y}", "@L{y''(t)+a*y'(t)+b*y(t)}"},
        {"@L(y''+4y'+5y-50x,x,p)", "@L(y''(x)+4*y'(x)+5*y(x)-50*x,x,p)"},
        {"@L{y+z+y'+z''}", "@L{y(t)+z(t)+y'(t)+z''(t)}"},
        {"@L{y+y'(t)}", "@L{y(t)+y'(t)}"}, {"@L{y'''}", "@L{y'''(t)}"},
        {"Laplace(y'+a*y,t,s)", "Laplace(y'(t)+a*y(t),t,s)"},
    };
    const char *keys[] = {"unbound", "tex", "function", "transform_identity_TeX"};
    for (size_t i = 0; i < 8; ++i) {
        const json_t *actual = lab_math_laplace_derivatives_fields(pairs[i][0]), *expected =
            lab_math_laplace_derivatives_fields(pairs[i][1]);
        for (size_t j = 0; j < 4; ++j)
            lab_math_equal(lab_math_text(actual, keys[j]), lab_math_text(expected, keys[j]));
    }
    const char *scope[][2] = {
        {"y+@L{y'+y}", "y+@L{y'(t)+y(t)}"}, {"y(0)+@L{y'}", "y(0)+@L{y'(t)}"},
        {"@L{y'}+y(0)", "@L{y'(t)}+y(0)"}, {"@L{y'+y}+@L{y*t}", "@L{y'(t)+y(t)}+@L{y*t}"},
        {"@L{y'+y+[_laplace_prime_0_0]*t}", "@L{y'(t)+y(t)+[_laplace_prime_0_0]*t}"},
        {"@L(y'+@L(z',x,p),t,s)", "@L(y'(t)+@L(z'(x),x,p),t,s)"},
    };
    for (size_t i = 0; i < 6; ++i)
        lab_math_laplace_derivatives_same(scope[i][0], scope[i][1], "unbound");
    lab_math_laplace_derivatives_contains(lab_math_laplace_derivatives_fields("@L{a*t}"), "unbound", "a(t)", false);
}

static string_offset_t lab_math_laplace_derivatives_position(const char *text, const char *fragment, bool last)
{
    /* Presentation-order checking requires examining the bounded output string. */
    string_t *input = string_new_with(text);
    string_cursor_t *cursor = string_cursor_new(input);
    string_offset_t position = -1;
    while (cursor && !string_cursor_done(cursor)) {
        if (string_cursor_match(cursor, fragment)) {
            position = (string_offset_t)string_cursor_position(cursor);
            if (!last)
                break;
        }
        string_cursor_next(cursor);
    }
    lab_math_check(position >= 0, fragment);
    string_cursor_free(cursor);
    string_free(input);
    return position;
}

static void test_laplace_derivative_order(void)
{
    const json_t *result = lab_math_laplace_derivatives_fields("@L{f^(n)(t)}");
    lab_math_laplace_derivatives_contains(result, "tex", "\\sum_{k=0}^{n - 1}", true);
    lab_math_laplace_derivatives_contains(result, "tex", "n\\in\\mathbb{Z}_{\\ge0}", true);
    lab_math_laplace_derivatives_contains(result, "function", "f^(k)(0)", true);
    lab_math_laplace_derivatives_contains(result, "function", "floor(n) == n", true);
    const char *round_trips[] = {"@L{f^(n)(t)}", "@L{f'(t)}", "@L{f''(t)}", "f(0)-s*Laplace(f(t),t,s)"};
    for (size_t i = 0; i < 4; ++i) {
        result = lab_math_laplace_derivatives_fields(round_trips[i]);
        if (i == 3)
            lab_math_laplace_derivatives_contains(result, "unbound", "f(0)", true);
        lab_math_equal(lab_math_text(lab_math_laplace_derivatives_fields(lab_math_text(result, "unbound")), "tex"),
            lab_math_text(result, "tex"));
    }
    const unsigned orders[] = {3, 4, 8};
    const char *keys[] = {"unbound", "function", "tex"};
    for (size_t i = 0; i < 3; ++i) {
        unsigned order = orders[i];
        for (unsigned coordinate = 0; coordinate < 2; ++coordinate) {
            const char *variable = coordinate ? "p" : "s";
            const char *source = coordinate ? lab_math_format("@L(f^(%u)(x),x,p)", order)
                                             : lab_math_format("@L{f^(%u)(t)}", order);
            result = lab_math_laplace_derivatives_fields(source);
            for (size_t key = 0; key < 3; ++key) {
                if (order > 4 && key == 1)
                    continue;
                const char *initial = order <= 4 ? lab_math_format("f%.*s", order - 1, "'''")
                                                 : key == 2 ? "f^{(7)}" : "f^(7)";
                const char *text = lab_math_text(result, keys[key]);
                string_offset_t previous = lab_math_laplace_derivatives_position(text, initial, false);
                for (unsigned power = 2; power < order; ++power) {
                    const char *suffix = key == 2 ? lab_math_format("%s^{%u}", variable, power)
                                                  : lab_math_format("%s^%u", variable, power);
                    string_offset_t position = lab_math_laplace_derivatives_position(text, suffix, true);
                    lab_math_check(previous >= 0 && position >= previous, "initial values in ascending powers");
                    previous = position;
                }
            }
            const json_t *copied = lab_math_laplace_derivatives_fields(lab_math_text(result, "unbound"));
            const char *text = lab_math_text(copied, "tex");
            const char *initial = order <= 4 ? lab_math_format("f%.*s", order - 1, "'''") : "f^{(7)}";
            string_offset_t first = lab_math_laplace_derivatives_position(text, initial, false);
            string_offset_t last = lab_math_laplace_derivatives_position(text,
                lab_math_format("%s^{2}", variable), true);
            lab_math_check(first >= 0 && last > first, "copied initial value precedes quadratic term");
        }
    }
    lab_math_equal(lab_math_text(lab_math_laplace_derivatives_fields("@L{f'''(t)}"), "unbound"),
                   "s^3·ℒ(f(t), t, s) - f''(0) - s·f'(0) - f(0)·s^2");
    lab_math_laplace_derivatives_same("@L{f^(0)(t)}", "@L{f(t)}", "tex");
    lab_math_laplace_derivatives_same("{@L{f^(n)(t)} | n=3}", "@L{f^(n)(t)}", "tex");
    lab_math_laplace_derivatives_contains(lab_math_laplace_derivatives_fields("@L{f^(30)(t)}"), "tex", "\\sum_", true);
    result = lab_math_laplace_derivatives_fields("@L(f^(k)(x),x,p)");
    lab_math_laplace_derivatives_contains(result, "function", "laplace(f(x), x, p)", true);
    lab_math_laplace_derivatives_contains(result, "function", "sum(j,", true);
    lab_math_laplace_derivatives_contains(result, "function", "f^(j)(0)", true);
    lab_math_laplace_derivatives_contains(lab_math_fields("f'(t)", "t", "derivative", 40), "derivative", "f''", true);
    lab_math_laplace_derivatives_contains(lab_math_fields("f^(n)(t)", "t", "derivative", 40), "derivative", "n + 1", true);
    lab_math_laplace_derivatives_same("f^(n)", "f^n", "tex");
    lab_math_laplace_derivatives_same("sin(t)^2", "sin^2(t)", "tex");
    lab_math_laplace_derivatives_same("a(t+1)", "a*(t+1)", "tex");
    const char *unsupported[] = {"@L{f^(-1)(t)}", "@L{f^(1/2)(t)}", "@L{f^(t)(t)}", "@L{f'(2*t)}"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_laplace_derivatives_contains(lab_math_laplace_derivatives_fields(unsupported[i]),
            "transform_identity_TeX", " = ", false);
}

static void test_laplace_derivative_readme(void)
{
    /* README examples: docs/expression.md and the integral-transform design note. */
    const char *cases[][2] = {
        {"@L{f'(t)}", "s·ℒ(f(t), t, s) - f(0)"}, {"@L{f'}", "s·ℒ(f(t), t, s) - f(0)"},
        {"@L{f''(t)}", "s^2·ℒ(f(t), t, s) - f'(0) - s·f(0)"},
        {"@L{f^(n)(t)}", "s^n·ℒ(f(t), t, s) - Σ_(k=0)^(n - 1) f^(k)(0)·s^(n - 1 - k) where (n ∈ ℤ≥0)"},
    };
    for (size_t i = 0; i < 4; ++i)
        lab_math_equal(lab_math_text(lab_math_laplace_derivatives_fields(cases[i][0]), "unbound"), cases[i][1]);
}

/* Register the complete function-derivative regression groups. */
void test_lab_math_laplace_derivatives_cases(void)
{
    TEST_RUN_IN_GROUP(test_laplace_derivative_primes, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_derivative_shorthand, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_derivative_order, tests, NULL);
    lab_math_reset();
}

/* Run exact derivative README renderings in the final phase. */
void test_lab_math_laplace_derivatives_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_laplace_derivative_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
