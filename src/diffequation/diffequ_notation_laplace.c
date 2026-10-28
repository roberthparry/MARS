/**
 * @file diffequ_notation_laplace.c
 * @brief Describe the verified Cartesian harmonic family through semantic nodes.
 *
 * The differential-equation module computes and checks the derivatives; TeX
 * presents the resulting mathematical structure without accessing solver internals.
 */
#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *pair(notation_build_t *b, TeX_node_kind_t kind,
                                   const TeX_model_node_t *a, const TeX_model_node_t *c)
{
    const TeX_model_node_t *children[] = {a, c};
    return notation_build_node(b, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static expr_t *second_derivative(const expr_t *body, const expr_t *coordinate)
{
    expr_t *first_raw = expr_create_deriv(body, coordinate);
    expr_t *first = first_raw ? expr_simplify(first_raw) : NULL;
    expr_t *second_raw = first ? expr_create_deriv(first, coordinate) : NULL;
    expr_t *second = second_raw ? expr_simplify(second_raw) : NULL;
    expr_free(second_raw); expr_free(first); expr_free(first_raw);
    return second;
}

static const TeX_model_node_t *partial_second(notation_build_t *b, const TeX_model_node_t *body,
                                              const TeX_model_node_t *coordinate)
{
    const TeX_model_node_t *children[] = {body, coordinate};
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE,
                               .child_count = 2u, .order = 2u, .partial = true}, children);
}

/* Verify the computed derivatives cancel and retain the complete proof. */
bool de_pde_cartesian_laplace_steps(const diffequ_t *de, const equation_t *solution, char **steps_out,
                                    char **steps_TeX_out)
{
    if (!steps_out || !steps_TeX_out)
        return false;
    *steps_out = NULL; *steps_TeX_out = NULL;
    const expr_t *dependent = solution ? equ_lhs(solution) : NULL;
    const expr_t *right = solution ? equ_rhs(solution) : NULL;
    if (!de || de->independent_count != 2u || !dependent || !right)
        return false;
    const expr_t *x = de->independent_vars[0], *y = de->independent_vars[1];
    const char *xname = expr_symbol_name(x), *yname = expr_symbol_name(y);
    if (!xname || !yname)
        return false;
    expr_t *imaginary = expr_new_named_const(NUM_I, "i");
    expr_t *iy = imaginary ? expr_mul(imaginary, y) : NULL;
    expr_t *positive = iy ? expr_add(x, iy) : NULL;
    expr_t *negative = iy ? expr_sub(x, iy) : NULL;
    expr_t *dxx = second_derivative(right, x), *dyy = second_derivative(right, y);
    expr_t *sum = dxx && dyy ? expr_add(dxx, dyy) : NULL;
    expr_t *simplified = sum ? expr_simplify(sum) : NULL;
    bool valid = positive && negative && dxx && dyy && simplified && expr_is_exact_zero(simplified);
    char *u_text = valid ? expr_to_string(dependent, style_UNBOUND) : NULL;
    char *positive_text = valid ? expr_to_string(positive, style_UNBOUND) : NULL;
    char *negative_text = valid ? expr_to_string(negative, style_UNBOUND) : NULL;
    char *right_text = valid ? expr_to_string(right, style_UNBOUND) : NULL;
    char *dxx_text = valid ? expr_to_string(dxx, style_UNBOUND) : NULL;
    char *dyy_text = valid ? expr_to_string(dyy, style_UNBOUND) : NULL;
    string_t *plain = u_text && positive_text && negative_text && right_text && dxx_text && dyy_text ? string_sprintf(
        "Use the parsed coordinates to form:\n"
        "      z = %s,    w = %s\n"
        "Construct the harmonic family:\n"
        "      %s = %s\n"
        "Compute the required second partials with MARSlib:\n"
        "      ∂²%s/∂%s² = %s\n"
        "      ∂²%s/∂%s² = %s\n"
        "Substitute the computed derivatives:\n"
        "      Δ%s = ∂²%s/∂%s² + ∂²%s/∂%s²\n"
        "           = (%s) + (%s)\n"
        "           = 0", positive_text, negative_text, u_text, right_text, u_text, xname, dxx_text,
        u_text, yname, dyy_text, u_text, u_text, xname, u_text, yname, dxx_text, dyy_text) : NULL;
    notation_build_t *b = plain ? notation_build_new() : NULL;
    string_t *display = NULL;
    if (b) {
        const TeX_model_node_t *u = notation_build_expr(b, dependent);
        const TeX_model_node_t *xn = notation_build_literal(b, TeX_NODE_IDENTIFIER, xname);
        const TeX_model_node_t *yn = notation_build_literal(b, TeX_NODE_IDENTIFIER, yname);
        const TeX_model_node_t *dx = partial_second(b, u, xn), *dy = partial_second(b, u, yn);
        const TeX_model_node_t *delta = pair(b, TeX_NODE_PRODUCT,
            notation_build_literal(b, TeX_NODE_IDENTIFIER, "Δ"), u);
        const TeX_model_node_t *computed_x = notation_build_expr(b, dxx), *computed_y = notation_build_expr(b, dyy);
        const TeX_model_node_t *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *rows[] = {
            pair(b, TeX_NODE_EQUATION, notation_build_literal(b, TeX_NODE_IDENTIFIER, "z"), notation_build_expr(b, positive)),
            pair(b, TeX_NODE_EQUATION, notation_build_literal(b, TeX_NODE_IDENTIFIER, "w"), notation_build_expr(b, negative)),
            pair(b, TeX_NODE_EQUATION, u, notation_build_expr(b, right)),
            pair(b, TeX_NODE_EQUATION, dx, computed_x), pair(b, TeX_NODE_EQUATION, dy, computed_y),
            pair(b, TeX_NODE_EQUATION, delta, pair(b, TeX_NODE_SUM, dx, dy)),
            pair(b, TeX_NODE_EQUATION, delta, pair(b, TeX_NODE_SUM, computed_x, computed_y)),
            pair(b, TeX_NODE_EQUATION, delta, zero)
        };
        const TeX_model_node_t *root = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
        display = notation_build_render(b, root);
    }
    valid = plain && display && (*steps_out = strdup(string_c_str(plain))) &&
            (*steps_TeX_out = strdup(string_c_str(display)));
    if (!valid) {
        free(*steps_out); free(*steps_TeX_out); *steps_out = NULL; *steps_TeX_out = NULL;
    }
    string_free(display); notation_build_free(b); string_free(plain);
    free(dyy_text); free(dxx_text); free(right_text); free(negative_text); free(positive_text); free(u_text);
    expr_free(simplified); expr_free(sum); expr_free(dyy); expr_free(dxx);
    expr_free(negative); expr_free(positive); expr_free(iy); expr_free(imaginary);
    return valid;
}
