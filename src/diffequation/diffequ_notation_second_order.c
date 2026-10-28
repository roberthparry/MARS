/**
 * @file diffequ_notation_second_order.c
 * @brief Describe second-order PDE factorisation and particular solutions semantically.
 *
 * The differential-equation solver owns characteristic roots, forcing and the
 * verified solution. TeX receives only mathematical nodes and explanatory prose.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                   const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *label(notation_build_t *build, const char *text, const TeX_model_node_t *math)
{
    return pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, text), math);
}

static const TeX_model_node_t *apply(notation_build_t *build, const char *name, const TeX_model_node_t *argument)
{
    return pair(build, TeX_NODE_APPLY, notation_build_literal(build, TeX_NODE_FUNCTION_NAME, name), argument);
}

/* Explain characteristic roots and retain the verified native particular solution. */
bool de_pde_second_order_steps(const diffequ_t *de, diffequ_solve_result_t *result, const expr_t *particular)
{
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *problem = de ? de_to_string(de, style_UNBOUND) : NULL;
    char *particular_text = particular ? expr_to_string(particular, style_UNBOUND) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    string_t *plain = problem && particular_text && solution_text ? string_sprintf(
        "Factor the homogeneous second-order constant-coefficient operator.\n"
        "Parsed equation: %s\n"
        "For A*u_xx + B*u_xy + C*u_yy = 0, solve A*m^2 + B*m + C = 0.\n"
        "Distinct roots give u_h = F(m1*x+y) + G(m2*x+y).\n"
        "A repeated root gives u_h = F(m*x+y) + x*G(m*x+y).\n"
        "Exchange coordinates if A = 0; for u_xy = 0 use F(x) + G(y).\n"
        "The roots are obtained by the native equation solver.\n"
        "For forcing f, seek u_p = q*f/k with q in {1,x,y,x*x,x*y,y*y} and L(q*f) = k*f.\n"
        "Otherwise, for f = f(s), s = a*x+b*y+c, reduce to (A*a*a+B*a*b+C*b*b)*H'' = f.\n"
        "At resonance use a coordinate multiplier and one integration, or a quadratic multiplier.\n"
        "Verify antiderivatives and retain exact integral nodes when no closed form is available.\n"
        "Apply superposition to sums and verify L(u_p) = f symbolically.\n"
        "Particular solution: u_p = %s\nSolution: %s", problem, particular_text, string_c_str(solution_text)) : NULL;
    notation_build_t *build = plain ? notation_build_new() : NULL;
    string_t *display = NULL;
    if (build) {
        const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
        const TeX_model_node_t *two = notation_build_literal(build, TeX_NODE_NUMBER, "2");
        const TeX_model_node_t *x = notation_build_literal(build, TeX_NODE_IDENTIFIER, "x");
        const TeX_model_node_t *y = notation_build_literal(build, TeX_NODE_IDENTIFIER, "y");
        const TeX_model_node_t *m = notation_build_literal(build, TeX_NODE_IDENTIFIER, "m");
        const TeX_model_node_t *A = notation_build_literal(build, TeX_NODE_IDENTIFIER, "A");
        const TeX_model_node_t *C = notation_build_literal(build, TeX_NODE_IDENTIFIER, "C");
        const TeX_model_node_t *u = notation_build_literal(build, TeX_NODE_IDENTIFIER, "u");
        const TeX_model_node_t *uh = pair(build, TeX_NODE_SUBSCRIPT, u,
            notation_build_literal(build, TeX_NODE_IDENTIFIER, "h"));
        const TeX_model_node_t *up = pair(build, TeX_NODE_SUBSCRIPT, u,
            notation_build_literal(build, TeX_NODE_IDENTIFIER, "p"));
        const TeX_model_node_t *m1 = pair(build, TeX_NODE_SUBSCRIPT, m, one);
        const TeX_model_node_t *m2 = pair(build, TeX_NODE_SUBSCRIPT, m, two);
        const TeX_model_node_t *arg1 = pair(build, TeX_NODE_SUM, pair(build, TeX_NODE_PRODUCT, m1, x), y);
        const TeX_model_node_t *arg2 = pair(build, TeX_NODE_SUM, pair(build, TeX_NODE_PRODUCT, m2, x), y);
        const TeX_model_node_t *arg = pair(build, TeX_NODE_SUM, pair(build, TeX_NODE_PRODUCT, m, x), y);
        const TeX_model_node_t *distinct = pair(build, TeX_NODE_SUM, apply(build, "F", arg1), apply(build, "G", arg2));
        const TeX_model_node_t *repeated = pair(build, TeX_NODE_SUM, apply(build, "F", arg),
            pair(build, TeX_NODE_PRODUCT, x, apply(build, "G", arg)));
        const TeX_model_node_t *unmixed = pair(build, TeX_NODE_SUM, apply(build, "F", x), apply(build, "G", y));
        const TeX_model_node_t *s = notation_build_literal(build, TeX_NODE_IDENTIFIER, "s");
        const TeX_model_node_t *kappa = notation_build_literal(build, TeX_NODE_IDENTIFIER, "κ");
        const TeX_model_node_t *H = notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "H");
        const TeX_model_node_t *Hprime = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_PRIME, .child_count = 1u, .order = 2u}, &H);
        const TeX_model_node_t *kappa_children[] = {kappa, zero};
        const TeX_model_node_t *kap_nonzero = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_RELATION, .child_count = 2u, .relation = TeX_RELATION_NOT_EQUAL},
            kappa_children);
        const TeX_model_node_t *rows[] = {
            label(build, "Equation:", de_notation_problem(build, de)),
            label(build, "Characteristic polynomial:", pair(build, TeX_NODE_EQUATION,
                notation_build_formula(build, "A*m^2+B*m+C"), zero)),
            label(build, "Distinct roots:", pair(build, TeX_NODE_EQUATION, uh, distinct)),
            label(build, "Repeated root:", pair(build, TeX_NODE_EQUATION, uh, repeated)),
            pair(build, TeX_NODE_STATEMENTS, label(build, "If", pair(build, TeX_NODE_EQUATION, A, zero)),
                 notation_build_literal(build, TeX_NODE_TEXT, "exchange coordinates.")),
            pair(build, TeX_NODE_STATEMENTS,
                 label(build, "If", pair(build, TeX_NODE_EQUATION, pair(build, TeX_NODE_EQUATION, A, C), zero)),
                 pair(build, TeX_NODE_EQUATION, uh, unmixed)),
            label(build, "Single-phase reduction:", pair(build, TeX_NODE_EQUATION, s,
                notation_build_formula(build, "a*x+b*y+c"))),
            pair(build, TeX_NODE_STATEMENTS, kap_nonzero, pair(build, TeX_NODE_EQUATION,
                pair(build, TeX_NODE_PRODUCT, kappa, pair(build, TeX_NODE_APPLY, Hprime, s)), apply(build, "f", s))),
            pair(build, TeX_NODE_EQUATION, kappa, notation_build_formula(build, "A*a^2+B*a*b+C*b^2")),
            label(build, "Particular solution:", pair(build, TeX_NODE_EQUATION, up, notation_build_expr(build, particular))),
            label(build, "Verified:", pair(build, TeX_NODE_EQUATION, apply(build, "L", up),
                notation_build_literal(build, TeX_NODE_IDENTIFIER, "f"))),
            label(build, "Solution:", pair(build, TeX_NODE_EQUATION,
                notation_build_expr(build, equ_lhs(solution)), notation_build_expr(build, equ_rhs(solution))))
        };
        const TeX_model_node_t *root = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
        display = notation_build_render(build, root);
    }
    bool valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                 de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    string_free(display); notation_build_free(build); string_free(plain); string_free(solution_text);
    free(particular_text); free(problem);
    return valid;
}
