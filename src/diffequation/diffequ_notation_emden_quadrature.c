/**
 * @file diffequ_notation_emden_quadrature.c
 * @brief Semantic explanations for exact modified-Emden quadratures with independent coefficients.
 *
 * Presents the velocity ratio, reduced linear equation, primitive and linearisation criterion.
 * All formulae are native expression graphs; the TeX module alone supplies typesetting syntax.
 * The two integration constants are distinguished from the freely chosen fixed reference parameter.
 */
#include <stdlib.h>

#include "TeX-t.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

/* The parameter is an argument of both coordinates, without changing their native equation identities. */
bool de_emden_quadrature_display(diffequ_solve_result_t *result)
{
    const expr_t *parameter = de_solve_result_quadrature_parameter(result);
    if (!parameter) return true;
    for (size_t i = 0; i < 2u; ++i) {
        equation_t *equation = (equation_t *)de_solve_result_at(result, i);
        notation_build_t *build = notation_build_new();
        const TeX_model_node_t *parts[] = {
            build && equation ? notation_build_expr(build, equ_lhs(equation)) : NULL,
            build ? notation_build_expr(build, parameter) : NULL
        };
        const TeX_model_node_t *left = build ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 2u}, parts) : NULL;
        string_t *left_TeX = left ? notation_build_render(build, left) : NULL;
        char *right_body = equation ? expr_to_TeX_body(equ_rhs(equation)) : NULL;
        string_t *right_TeX = right_body ? string_new_with(right_body) : NULL;
        free(right_body);
        string_t *name = equation ? expr_to_text(equ_lhs(equation), style_UNBOUND) : NULL;
        string_t *argument = expr_to_text(parameter, style_UNBOUND);
        string_t *left_text = name && argument ? string_sprintf("%S(%S)", name, argument) : NULL;
        string_t *right_text = equation ? expr_to_text(equ_rhs(equation), style_UNBOUND) : NULL;
        bool ok = left_text && right_text && left_TeX && right_TeX &&
            equ_set_display_unbound(equation, left_text, right_text) == 0 &&
            equ_set_display_TeX(equation, left_TeX, right_TeX) == 0;
        string_free(right_text); string_free(left_text); string_free(argument); string_free(name);
        string_free(right_TeX); string_free(left_TeX); notation_build_free(build);
        if (!ok) return false;
    }
    return true;
}

/* Project mathematical identities without exposing native graph layouts to the renderer. */
bool de_emden_quadrature_steps(diffequ_solve_result_t *result, const expr_t *t, const expr_t *y,
    const expr_t *first, const expr_t *a, const expr_t *b, const expr_t *r, const expr_t *reference,
    const expr_t *q, const expr_t *primitive)
{
    expr_t *ratio = expr_mul_simplify_owned(expr_pow_long(y, -2), expr_clone(first));
    expr_t *coordinate[] = {(expr_t *)r};
    expr_t *dy = expr_new_formal_derivative(y, 1u, coordinate);
    expr_t *rhs = expr_div_simplify_owned(de_simplify_unary_owned(
        expr_mul_simplify_owned(expr_clone(r), expr_clone(y)), expr_neg), expr_clone(q));
    expr_t *q_name = expr_new_named_var(NUM_NAN, "Q"), *j_name = expr_new_named_var(NUM_NAN, "J");
    expr_t *a_squared = expr_pow_long(a, 2), *nine_b = expr_mul_long(b, 9);
    const expr_t *left[] = {r, q_name, dy, j_name, a_squared};
    const expr_t *right[] = {ratio, q, rhs, primitive, nine_b};
    const char *captions[] = {
        "Velocity-ratio substitution:", "Quadratic in the curve parameter:",
        "The reduced equation is linear in the dependent coordinate:", "Define the integrating-factor primitive:",
        "Extra point symmetries require the following relation; it is not assumed:"
    };
    string_t *plain = string_new_with("Independent-coefficient modified-Emden reduction.\n"
        "Time translation and simultaneous inverse scaling of the dependent coordinate are symmetries.\n");
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *rows[12] = {NULL};
    size_t used = 0u;
    bool valid = result && t && reference && plain && build;
    for (size_t i = 0; valid && i < 5u; ++i) {
        equation_t *equation = left[i] && right[i] ? equ_new(left[i], right[i]) : NULL;
        string_t *text = equation ? equ_to_text(equation, style_UNBOUND) : NULL;
        valid = text && string_append_format(plain, "%s\n%S\n", captions[i], text) >= 0;
        const TeX_model_node_t *parts[] = {notation_build_expr(build, left[i]), notation_build_expr(build, right[i])};
        const TeX_model_node_t *identity = valid ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_EQUATION, .child_count = 2u}, parts) : NULL;
        const TeX_model_node_t *statement[] = {notation_build_literal(build, TeX_NODE_TEXT, captions[i]), identity};
        rows[used++] = valid ? notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 2u}, statement) : NULL;
        valid = valid && rows[used - 1u];
        string_free(text); equ_free(equation);
    }
    string_t *reference_text = de_notation_symbol_text(reference);
    string_t *scope = reference_text ? string_sprintf(
        "The first two returned equations are one simultaneous parametric family, with two integration constants.\n"
        "%S is a fixed reference in a connected interval where Q does not vanish; changing it only changes those constants.\n"
        "The remaining equations are separate exceptional families: equilibria and constant velocity-ratio branches.\n"
        "Initial-value and boundary constraints are not imposed by this general-family reduction.", reference_text) : NULL;
    valid = valid && scope && string_append_format(plain, "%S", scope) >= 0;
    rows[used++] = valid ? notation_build_literal(build, TeX_NODE_TEXT,
        "The first two equations form one parametric family with two integration constants.") : NULL;
    rows[used++] = valid ? notation_build_literal(build, TeX_NODE_TEXT,
        "The integration reference is fixed within a regular interval, not a third integration constant.") : NULL;
    rows[used++] = valid ? notation_build_literal(build, TeX_NODE_TEXT,
        "The remaining equations are separate equilibrium and constant-ratio branches.") : NULL;
    const TeX_model_node_t *root = valid ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = used}, rows) : NULL;
    string_t *rendered = root ? notation_build_render(build, root) : NULL;
    valid = rendered && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(rendered)) == 0;
    string_free(rendered); string_free(scope); string_free(reference_text); string_free(plain);
    notation_build_free(build); expr_free(nine_b); expr_free(a_squared); expr_free(j_name); expr_free(q_name);
    expr_free(rhs); expr_free(dy); expr_free(ratio);
    return valid;
}
