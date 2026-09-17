#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_affine.h"
#include "test_diffequ_flow.h"

void test_diffequ_coupled_flow(void)
{
    static const char *const sources[] = {
        "f_t + xf_x + (x+t) f_y = t^3",
        "(x+t)*f_y + f_t + x*f_x = t^3",
        "2*f_t + 2*x*f_x + (2*x+2*t)*f_y = 2*t^3",
        "w_s + 2*r*w_r + (3*r+s)*w_z = s^2",
        "f_t + x*f_x + (x+t)*f_y + (y+t)*f_z = 0",
        "f_t + x*f_x + (x^2+t)*f_y = t^3",
        "f_t + x*f_x + (y+x)*f_y = 0",
        "K2_t + x*K2_x + (x+t)*K2_y = t^3",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        printf("  solving: %s\n", sources[i]);
        fflush(stdout);
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
        bool verified = solution && test_diffequ_affine_solution_verified(de, solution);
        const char *steps = de_solve_result_steps(result), *TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && strstr(steps, "upstream") && TeX && !strstr(TeX, "NAN");
        de_free(de);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        bool retained = text && strstr(string_c_str(text), "F(") && !strstr(string_c_str(text), "NAN");
        string_free(text);
        de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(verified);
        ASSERT_TRUE(explained);
        ASSERT_TRUE(retained);
    }
}

void test_diffequ_coupled_flow_scope(void)
{
    static const char *const sources[] = {
        "f_t + y*f_x + x*f_y = 0",
        "f_t + x*y*f_x + (x+t)*f_y = 0",
        "f_t + x*f_x + (x+t)*f_y = f",
        "f_t + x*f_x + (x+t)*f_y = sin(x+t)",
        "f_t + x*f_x + (x+t)*f_y = 0; f(x,y,0) = x",
        "f_t + x*f_x + (x+t)*f_yy = 0",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const char *diagnostic = de_solve_result_diagnostic(result);
        bool declined = diagnostic && !strstr(diagnostic, "triangular characteristic flow");
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(declined);
    }
}

void test_diffequ_nested_source_order_TeX(void)
{
    static const struct { const char *source; const char *coefficient; } cases[] = {
        {"f_t + xf_x + (x+t) f_y = t^3", "\\left(x + t\\right)"},
        {"f_t + xf_x + (t+x) f_y = t^3", "\\left(t + x\\right)"},
        {"f_t + xf_x - (x-t) f_y = t^3", "\\left(x - t\\right)"},
        {"f_t + xf_x + (x+t)^2 f_y = t^3", "\\left(x + t\\right)^{2}"},
        {"f_t + xf_x + (x+t) f_yy = t^3", "\\left(x + t\\right)"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
        const char *coefficient = TeX ? strstr(TeX, cases[i].coefficient) : NULL;
        const char *derivative = coefficient ? strstr(coefficient, "\\frac{\\partial") : NULL;
        bool correct = coefficient && derivative && !strstr(TeX, "NAN");
        printf("  %s\n", TeX ? TeX : "NULL");
        free(TeX);
        de_free(de);
        ASSERT_TRUE(correct);
    }
}

/* README example from docs/diffequation.md: an upstream coordinate drives a later characteristic. */
void example_diffequation_coupled_flow(void)
{
    const char *source = "f_t + xf_x + (x+t) f_y = t^3";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    printf("  %s\n  %s\n", source, text ? string_c_str(text) : "NULL");
    bool valid = text && strcmp(string_c_str(text), "f = F(x·exp(-t), ½·(2y - 2x - t²)) + ¼t⁴") == 0 &&
                 solution && test_diffequ_affine_solution_verified(de, solution);
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}
