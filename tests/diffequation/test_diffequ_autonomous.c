#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"
#include "test_diffequ_autonomous.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

/* Substitute the implicit derivatives into the original PDE after multiplying by the common H_u factor. */
static bool implicit_transport_residual(const diffequ_t *de, const equation_t *solution)
{
    const expr_t *dependent = equ_lhs(solution);
    expr_t *relation = equ_residual(solution);
    expr_t *residual = equ_residual(de_equation(de));
    expr_t *hu = relation ? expr_create_deriv(relation, dependent) : NULL;
    bool valid = hu && !expr_is_exact_zero(hu);
    for (size_t i = 0u; valid && residual && i < de_independent_count(de); ++i) {
        expr_t *coordinate = (expr_t *)de_independent_at(de, i);
        expr_t *formal = expr_new_formal_derivative(dependent, 1u, &coordinate);
        expr_t *partial = expr_create_deriv(relation, coordinate);
        expr_t *numerator = partial ? expr_neg(partial) : NULL;
        expr_t *next = formal && numerator ? expr_substitute(residual, formal, numerator) : NULL;
        expr_free(numerator);
        expr_free(partial);
        expr_free(formal);
        expr_free(residual);
        residual = next;
    }
    expr_t *zero = residual ? expr_simplify(residual) : NULL;
    valid = valid && zero && expr_is_exact_zero(zero);
    expr_free(zero);
    expr_free(hu);
    expr_free(residual);
    expr_free(relation);
    return valid;
}

/* Check the whole family, not just a hand-picked solution or a renamed version of the same input. */
void test_diffequ_autonomous_transport(void)
{
    static const char *const sources[] = {
        "u_y + u*u_x = 0",
        "u*u_x + u_y = 0",
        "3*u_y + 3*u*u_x = 0",
        "2*w_t + (w^2+1)*w_r = 0",
        "z_t + sin(z)*z_s = 0",
        "u_x - u*u_y = 0",
        "{ u_y + a*u*u_x = 0 | y = ?, x = ?; a = 2; }",
        "{ a*u_y + u*u_x = 0 | y = ?, x = ?; a = ?; }",
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        const equation_t *solution = de_solve_result_at(result, 0u);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_solver(result) == DE_SOLVER_CHARACTERISTICS &&
                      de_solve_result_count(result) == 1u;
        bool implicit = solution && expr_is_arbitrary_function(equ_rhs(solution));
        bool verified = solution && implicit_transport_residual(de, solution);
        const char *steps = de_solve_result_steps(result);
        const char *TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && strstr(steps, "implicit") && strstr(steps, "nonzero") &&
                         strstr(steps, "no initial or boundary data");
        bool rendered = TeX && strstr(TeX, "\\partial H") && !strstr(TeX, "NAN");
        if (i == 7u)
            explained = explained && strstr(steps, "Normalisation requires a != 0");
        de_free(de);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        printf("  %s\n  %s\n", sources[i], text ? string_c_str(text) : "NULL");
        bool retained = text && strstr(string_c_str(text), "F(");
        string_free(text);
        de_solve_result_free(result);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(implicit);
        ASSERT_TRUE(verified);
        ASSERT_TRUE(explained);
        ASSERT_TRUE(rendered);
        ASSERT_TRUE(retained);
    }
}

/* The implicit family must not silently absorb forcing, variable coefficients or prescribed data. */
void test_diffequ_autonomous_transport_scope(void)
{
    static const char *const excluded[] = {
        "u_y + u*u_x = 1",
        "u_y + x*u*u_x = 0",
        "u_y + u*u_x + u = 0",
        "u_y + u*u_x = 0; u(x,0) = -x",
        "u_y + u*u_x + u_xx = 0",
        "u_y + u*u_x + u_z = 0",
        "u_x*u_y + u*u_x = 0",
    };
    for (size_t i = 0u; i < sizeof(excluded) / sizeof(*excluded); ++i) {
        diffequ_t *de = de_from_string(excluded[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const char *diagnostic = de_solve_result_diagnostic(result);
        bool skipped = diagnostic && !strstr(diagnostic, "implicit local characteristic family");
        printf("  excluded: %s\n", excluded[i]);
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(skipped);
    }
    diffequ_t *de = de_from_string("u_y + u*u_x = 0");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
    const char *steps = de_solve_result_steps(result), *TeX = de_solve_result_steps_TeX(result);
    bool quiet = (!steps || !*steps) && (!TeX || !*TeX);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(solved);
    ASSERT_TRUE(quiet);
}

/* README example from docs/diffequation.md: implicit autonomous transport without initial data. */
void example_diffequation_autonomous_transport(void)
{
    const char *source = "u_y + u*u_x = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    printf("  %s\n  %s\n  %s\n", source, text ? string_c_str(text) : "NULL", TeX ? TeX : "NULL");
    bool matched = text && strcmp(string_c_str(text), "u = F(x - uy)") == 0;
    bool verified = solution && implicit_transport_residual(de, solution);
    free(TeX);
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(matched);
    ASSERT_TRUE(verified);
}
