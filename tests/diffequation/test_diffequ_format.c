#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_format.h"

static void check_power_display(const char *source, const char *expected)
{
    diffequ_t *de = de_from_string(source);
    char *text = de ? de_to_string(de, style_EXPRESSION) : NULL;
    diffequ_t *again = text ? de_from_string(text) : NULL;
    char *round_trip = again ? de_to_string(again, style_EXPRESSION) : NULL;
    char *base = de ? de_to_string(de, style_UNBOUND) : NULL;
    char *round_trip_base = again ? de_to_string(again, style_UNBOUND) : NULL;
    bool matched = text && strstr(text, expected);
    bool stable = text && round_trip && strcmp(text, round_trip) == 0;
    bool equivalent = base && round_trip_base && strcmp(base, round_trip_base) == 0;

    printf("  %s\n  %s\n", source, text ? text : "NULL");
    if (!stable)
        printf("  round trip: %s\n", round_trip ? round_trip : "NULL");
    free(round_trip_base);
    free(base);
    free(round_trip);
    free(text);
    de_free(again);
    de_free(de);
    ASSERT_TRUE(matched);
    ASSERT_TRUE(stable);
    ASSERT_TRUE(equivalent);
}

/* Keep polynomial powers consistent across equations, conditions and bindings. */
void test_diffequ_expression_integer_powers(void)
{
    static const struct { const char *source; const char *expected; } cases[] = {
        {"u_tt - v^2u_xx = f(x,t); u(x,0) = g(x); u_t(x,0) = h(x)", "v²∂²u/∂x²"},
        {"y' = x^12 + x^0 + x^(3)", "x¹² + x⁰ + x³"},
        {"y' = x^2; y(0) = 2^3", "y(0) = 2³"},
        {"{ y' = a*x^2 | x = ?; a = 2^3; y(0) = 1 }", "a = 2³"},
        {"(x^2+y^2)dx + 2xy dy = 0", "x²+y²"},
        {"y' = x^2.5", "x^2.5"},
        {"y' = x^2e1", "x^2e1"},
        {"y' = x^1/2", "x^1/2"},
        {"y' = x^(2/3)", "x^(2/3)"},
        {"y' = x^-2", "x^-2"},
        {"y' = x^n", "x^n"},
        {"y' = x^2^3", "x^2^3"},
        {"y' = x²^3", "x²^3"},
        {"y' = x^2!", "x^2!"},
        {"y' = x^(2+3)", "x^(2+3)"},
        {"y' = x^2147483648", "x^2147483648"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i)
        check_power_display(cases[i].source, cases[i].expected);
}

/* README example from docs/diffequation.md: native Unicode problem-card powers. */
void example_diffequation_unicode_powers(void)
{
    check_power_display("u_tt - v^2u_xx = f(x,t); u(x,0) = g(x); u_t(x,0) = h(x)",
                        "{ ∂²u/∂t² - v²∂²u/∂x² = f(x,t) | t = ?, x = ?; ; u(x,0) = g(x), ∂u/∂t(x,0) = h(x) }");
}
