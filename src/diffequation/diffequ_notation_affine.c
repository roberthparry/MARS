/**
 * @file diffequ_notation_affine.c
 * @brief Prepare affine characteristic-flow explanations as mathematical nodes.
 *
 * The native solver supplies the parameter, clock and verified family. This
 * adapter owns the characteristic identities and qualifications; the TeX module
 * receives only generic semantic operations and literal explanatory text.
 */
#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                   const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *parts[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, parts);
}

static const TeX_model_node_t *caption(notation_build_t *build, const char *text, const TeX_model_node_t *math)
{
    const TeX_model_node_t *literal = notation_build_literal(build, TeX_NODE_TEXT, text);
    return math ? pair(build, TeX_NODE_STATEMENTS, literal, math) : literal;
}

/* Explain the integrating factor and invariants without embedding typesetting commands. */
bool de_affine_steps(diffequ_solve_result_t *result, const expr_t *time, const expr_t *clock,
                      const expr_t *arbitrary, const expr_t *particular, bool coupled)
{
    const char *name = expr_symbol_name(time);
    string_t *plain = name ? string_sprintf(
        "Affine transport by characteristics, using %s as parameter.\n"
        "Divide the original transport coefficients by the constant coefficient of its derivative.\n"
        "%s"
        "Each remaining coordinate obeys dz/d%s = A(%s)*z+B(%s).\n"
        "Integrate A in that parameter and negate its primitive to form mu = exp(-primitive).\n"
        "The invariant is I = mu*z minus a primitive of mu*B in the same parameter.\n"
        "Constant rates retain their equivalent compact shifted-exponential forms.\n"
        "F is an arbitrary differentiable function of all these invariants.\n"
        "Integrate the normalised forcing in %s for the particular solution.\n"
        "Every invariant and the complete solution are verified against the original PDE.\n"
        "The solution is local to regions where the coefficient functions and primitives are defined.\n"
        "No initial or boundary data are imposed.\n", name,
        coupled ? "Resolve upstream coordinates first and substitute their flows into each remaining drift.\n"
                  "Eliminate the integration parameters using the earlier invariants.\n" : "",
        name, name, name, name) : NULL;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *parameter = notation_build_literal(build, TeX_NODE_IDENTIFIER, name);
    const TeX_model_node_t *z = notation_build_literal(build, TeX_NODE_IDENTIFIER, "z");
    const TeX_model_node_t *mu = notation_build_literal(build, TeX_NODE_IDENTIFIER, "μ");
    const TeX_model_node_t *a = pair(build, TeX_NODE_APPLY,
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "A"), parameter);
    const TeX_model_node_t *b = pair(build, TeX_NODE_APPLY,
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "B"), parameter);
    const TeX_model_node_t *variables[] = {z, parameter};
    const TeX_model_node_t *derivative = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u, .order = 1u}, variables);
    const TeX_model_node_t *integral = pair(build, TeX_NODE_INTEGRAL, a, parameter);
    const TeX_model_node_t *negative = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &integral);
    const TeX_model_node_t *exponential = pair(build, TeX_NODE_POWER,
        notation_build_literal(build, TeX_NODE_IDENTIFIER, "e"), negative);
    const TeX_model_node_t *invariant = pair(build, TeX_NODE_DIFFERENCE,
        pair(build, TeX_NODE_PRODUCT, mu, z),
        pair(build, TeX_NODE_INTEGRAL, pair(build, TeX_NODE_PRODUCT, mu, b), parameter));
    const TeX_model_node_t *rows[12];
    size_t count = 0u;
    rows[count++] = caption(build, "Affine transport by characteristics; parameter", parameter);
    rows[count++] = caption(build, "Normalise the transport operator by", notation_build_expr(build, clock));
    if (coupled) {
        rows[count++] = caption(build, "Resolve upstream flows first; evaluate each drift along those flows.", NULL);
        rows[count++] = caption(build, "Eliminate their integration parameters using the earlier invariants.", NULL);
    }
    rows[count++] = pair(build, TeX_NODE_EQUATION, derivative,
        pair(build, TeX_NODE_SUM, pair(build, TeX_NODE_PRODUCT, a, z), b));
    rows[count++] = pair(build, TeX_NODE_STATEMENTS,
        pair(build, TeX_NODE_EQUATION, mu, exponential), pair(build, TeX_NODE_EQUATION,
            notation_build_literal(build, TeX_NODE_IDENTIFIER, "I"), invariant));
    rows[count++] = caption(build, "Homogeneous family:", notation_build_expr(build, arbitrary));
    rows[count++] = caption(build, "Particular solution:", notation_build_expr(build, particular));
    rows[count++] = caption(build, "F is arbitrary and differentiable.", NULL);
    rows[count++] = caption(build, "All invariants and the complete solution satisfy the original operator.", NULL);
    rows[count++] = caption(build, "Local where coefficients and primitives are defined; no initial or boundary data.", NULL);
    const TeX_model_node_t *root = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = count}, rows);
    string_t *rendered = notation_build_render(build, root);
    bool valid = plain && rendered && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                                     de_solve_result_set_steps_TeX(result, string_c_str(rendered)) == 0;
    string_free(rendered);
    notation_build_free(build);
    string_free(plain);
    return valid;
}
