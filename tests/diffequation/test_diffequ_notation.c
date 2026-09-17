#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_notation.h"

void test_diffequ_caret_derivative_orders(void)
{
    static const struct { const char *source, *canonical; } cases[] = {
        {"∂^3u/∂x^3 = 0", "Dxxx(u) = 0"},
        {"∂³u/∂x^3 = 0", "Dxxx(u) = 0"},
        {"∂^3u/∂x³ = 0", "Dxxx(u) = 0"},
        {"∂^{3}u/∂x^{3} = 0", "Dxxx(u) = 0"},
        {"∂ ^{ 3 } u / ∂x ^{ 3 } = 0", "Dxxx(u) = 0"},
        {"∂^3u/∂x^2 ∂y = 0", "Dyxx(u) = 0"},
        {"∂^4u/∂x²∂y^2 = 0", "Dyyxx(u) = 0"},
        {"∂^12u/∂x^12 = 0", "Dxxxxxxxxxxxx(u) = 0"},
        {"d^3y/dx^3 = y", "Dxxx(y) = y"},
        {"d³y/dx^3 = y", "Dxxx(y) = y"},
        {"d^3y/dx³ = y", "Dxxx(y) = y"},
        {"d^{4}y/dx^{4} = y", "Dxxxx(y) = y"},
        {"d^12y/dx^12 = y", "Dxxxxxxxxxxxx(y) = y"},
        {"∂u/∂t = 1/2(n + 1)(n + 2)u^n ∂u/∂x - ∂^3u/∂x^3",
         "Dt(u) = (1/2)*(n+1)*(n+2)*u^n*Dx(u) - Dxxx(u)"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *de = de_from_string(cases[i].source), *canonical = de_from_string(cases[i].canonical);
        string_t *actual = de ? equ_to_text(de_equation(de), style_UNBOUND) : NULL;
        string_t *expected = canonical ? equ_to_text(de_equation(canonical), style_UNBOUND) : NULL;
        bool valid = actual && expected && strcmp(string_c_str(actual), string_c_str(expected)) == 0;
        printf("  %s\n  %s\n", cases[i].source, actual ? string_c_str(actual) : "NULL");
        string_free(expected); string_free(actual); de_free(canonical); de_free(de);
        ASSERT_TRUE(valid);
    }
}

void test_diffequ_invalid_derivative_orders(void)
{
    static const char *const sources[] = {
        "∂^2u/∂x^3=0", "∂^0u/∂x^0=0", "∂^-3u/∂x^-3=0", "∂^u/∂x=0",
        "∂^{3u/∂x^3=0", "∂^3u/∂x^{3=0", "∂^3u/∂x^0=0", "∂^129u/∂x^129=0",
        "∂^999999999999999999999999u/∂x=0", "∂⁰u/∂x=0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        bool rejected = !de;
        de_free(de);
        ASSERT_TRUE(rejected);
    }
}

/* README example from docs/diffequation.md: parse a generalised KdV equation without claiming to solve it. */
void example_diffequation_caret_pde(void)
{
    const char *source = "∂u/∂t = 1/2(n + 1)(n + 2)u^n ∂u/∂x - ∂^3u/∂x^3";
    diffequ_t *de = de_from_string(source);
    string_t *text = de ? equ_to_text(de_equation(de), style_UNBOUND) : NULL;
    char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
    const char *expected = "Dt(u) = ½·(n·(n + 3) + 2)·u^n·Dx(u) - Dxxx(u)";
    bool valid = text && TeX && strcmp(string_c_str(text), expected) == 0 &&
                 strstr(TeX, "\\partial^{3} u") && strstr(TeX, "\\partial x^{3}");
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    free(TeX); string_free(text); de_free(de);
    ASSERT_TRUE(valid);
}
