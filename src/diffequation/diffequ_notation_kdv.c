/**
 * @file diffequ_notation_kdv.c
 * @brief Prepare solitary-wave meaning and derivations for semantic rendering.
 *
 * The solver owns its assumptions, family qualification and coefficient matching.
 * Mathematical rows use native expression syntax; TeX supplies all typesetting
 * through the shared semantic context, without interpreting the solver result.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

/* Attach the family qualifications to the native solution's semantic right side. */
bool de_kdv_present(equation_t *solution, const expr_t *k, const expr_t *position)
{
    char *plain = expr_to_string(equ_rhs(solution), style_UNBOUND);
    string_t *left = string_sprintf("Solitary-wave family (not the general solution): %s",
                                     expr_symbol_name(equ_lhs(solution)));
    string_t *right = plain ? string_sprintf("%s; %s > 0, %s real (arbitrary constants).", plain,
                                             expr_symbol_name(k), expr_symbol_name(position)) : NULL;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *positive[] = {
        notation_build_expr(build, k), notation_build_literal(build, TeX_NODE_NUMBER, "0")
    };
    const TeX_model_node_t *real[] = {
        notation_build_expr(build, position), notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET, .number_set = TeX_SET_REALS}, NULL)
    };
    const TeX_model_node_t *conditions[] = {
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
            .relation = TeX_RELATION_GREATER, .child_count = 2u}, positive),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
            .relation = TeX_RELATION_MEMBER, .child_count = 2u}, real),
        notation_build_literal(build, TeX_NODE_TEXT, "(arbitrary constants)")
    };
    const TeX_model_node_t *rows[] = {
        notation_build_expr(build, equ_rhs(solution)),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 3u}, conditions),
        notation_build_literal(build, TeX_NODE_TEXT, "Solitary-wave family; not the general solution.")
    };
    const TeX_model_node_t *body = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = 3u}, rows);
    bool valid = de_notation_display(solution, left, right, build, body);
    notation_build_free(build);
    string_free(right);
    string_free(left);
    free(plain);
    return valid;
}

/* Describe the coefficient identities already verified by the native solver. */
bool de_kdv_set_steps(diffequ_solve_result_t *result)
{
    static const char plain[] =
        "Recognise A*u_t+B*u*u_x+C*u_xxx=0 with real nonzero constant coefficients.\n"
        "Here x, t, u, k and x0 denote the spatial, temporal, field, inverse-width and position roles.\n"
        "Set u=V(z), z=x-v*t-x0. Integrate once with zero background: C*V''-A*v*V+(B/2)*V^2=0.\n"
        "For V=M*sech(k*z)^2, V''=4*k^2*V-(6*k^2/M)*V^2.\n"
        "Matching coefficients gives v=4*C*k^2/A and M=12*C*k^2/B.\n"
        "k>0 and x0 is real. This is a solitary-wave family, not the general solution.\n"
        "No arbitrary initial or boundary data, multi-soliton or periodic solutions are claimed.";
    static const de_notation_step_t rows[] = {
        {"A, B and C are real nonzero constants.", "A*Dt(u)+B*u*Dx(u)+C*Dxxx(u)", "0"},
        {"Here x, t and u denote the recognised space, time and field.", NULL, NULL},
        {NULL, "u", "V(z)"},
        {NULL, "z", "x-v*t-x0"},
        {"Integrate with zero background:", "C*Dzz(V)-A*v*V+(B/2)*V^2", "0"},
        {NULL, "V", "M*sech(k*z)^2"},
        {NULL, "Dzz(V)", "4*k^2*V-(6*k^2/M)*V^2"},
        {NULL, "v", "4*C*k^2/A"},
        {NULL, "M", "12*C*k^2/B"},
        {"k is positive and x0 is real.", NULL, NULL},
        {"Zero-background solitary-wave family; not the general solution.", NULL, NULL}
    };
    return de_notation_steps(result, plain, rows, sizeof rows / sizeof *rows);
}
