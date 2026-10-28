/**
 * @file diffequ_notation_euler.c
 * @brief Describe the radial Euler PDE reduction as semantic mathematics.
 *
 * The differential-equation solver provides the coordinates and coefficients;
 * TeX renders the radial operator, chart and characteristic equation.
 */
#include <stdlib.h>

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

static const TeX_model_node_t *pair(notation_build_t *build, TeX_node_kind_t kind,
                                   const TeX_model_node_t *left, const TeX_model_node_t *right)
{
    const TeX_model_node_t *children[] = {left, right};
    return notation_build_node(build, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *derivative(notation_build_t *build, const TeX_model_node_t *body,
                                         const TeX_model_node_t *variable, unsigned order)
{
    const TeX_model_node_t *children[] = {body, variable};
    return notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE,
                               .child_count = 2u, .order = order, .partial = true}, children);
}

/* Project the coefficient values, radial chart and characteristic polynomial. */
bool de_pde_euler_steps(const diffequ_t *de, diffequ_solve_result_t *result, const number_t *parameters)
{
    if (!de || !result || !parameters)
        return false;
    expr_t *values[3] = {NULL};
    char *text[3] = {NULL};
    bool valid = true;
    for (size_t i = 0u; i < 3u; ++i) {
        values[i] = expr_new_const(parameters[i]);
        text[i] = values[i] ? expr_to_string(values[i], style_UNBOUND) : NULL;
        valid = valid && text[i];
    }
    string_t *plain = valid ? string_sprintf(
        "Radial Euler equation with A=%s, B=%s, C=%s.\n"
        "In generic coordinates, E=x*partial_x+y*partial_y differentiates along radial scaling.\n"
        "x*x*z_xx+2*x*y*z_xy+y*y*z_yy=(E*E-E)z.\n"
        "On the positive first-coordinate chart, set r=ln(x), eta=y/x; then E=partial_r.\n"
        "The equation becomes A*z_rr+(B-A)*z_r+C*z=0 at each fixed eta.\n"
        "Solve A*m*(m-1)+B*m+C=0 using the native equation solver.\n"
        "Distinct roots give x^m1*F(eta)+x^m2*G(eta); a repeated root gives x^m*(F(eta)+ln(x)*G(eta)).\n"
        "F and G are arbitrary twice-differentiable functions (complex-valued if needed).\n"
        "This local chart requires the first coordinate > 0; no boundary data are imposed.\n",
        text[0], text[1], text[2]) : NULL;
    notation_build_t *build = plain ? notation_build_new() : NULL;
    string_t *display = NULL;
    if (build) {
        const char *x_name = expr_symbol_name(de->independent_vars[0]);
        const char *y_name = expr_symbol_name(de->independent_vars[1]);
        const TeX_model_node_t *x = notation_build_literal(build, TeX_NODE_IDENTIFIER, x_name);
        const TeX_model_node_t *y = notation_build_literal(build, TeX_NODE_IDENTIFIER, y_name);
        const TeX_model_node_t *r = notation_build_literal(build, TeX_NODE_IDENTIFIER, "r");
        const TeX_model_node_t *eta = notation_build_literal(build, TeX_NODE_IDENTIFIER, "η");
        const TeX_model_node_t *E = notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "E");
        const TeX_model_node_t *z = notation_build_literal(build, TeX_NODE_IDENTIFIER, "z");
        const TeX_model_node_t *m = notation_build_literal(build, TeX_NODE_IDENTIFIER, "m");
        const TeX_model_node_t *A = notation_build_literal(build, TeX_NODE_IDENTIFIER, "A");
        const TeX_model_node_t *B = notation_build_literal(build, TeX_NODE_IDENTIFIER, "B");
        const TeX_model_node_t *C = notation_build_literal(build, TeX_NODE_IDENTIFIER, "C");
        const TeX_model_node_t *one = notation_build_literal(build, TeX_NODE_NUMBER, "1");
        const TeX_model_node_t *two = notation_build_literal(build, TeX_NODE_NUMBER, "2");
        const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *coefficients[] = {
            pair(build, TeX_NODE_EQUATION, A, notation_build_expr(build, values[0])),
            pair(build, TeX_NODE_EQUATION, B, notation_build_expr(build, values[1])),
            pair(build, TeX_NODE_EQUATION, C, notation_build_expr(build, values[2]))
        };
        const TeX_model_node_t *radial = pair(build, TeX_NODE_SUM,
            pair(build, TeX_NODE_PRODUCT, x, derivative(build, z, x, 1u)),
            pair(build, TeX_NODE_PRODUCT, y, derivative(build, z, y, 1u)));
        const TeX_model_node_t *mixed_variables[] = {z, x, y};
        const TeX_model_node_t *mixed = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 3u, .order = 2u, .partial = true},
            mixed_variables);
        const TeX_model_node_t *radial_second = pair(build, TeX_NODE_SUM,
            pair(build, TeX_NODE_SUM,
                pair(build, TeX_NODE_PRODUCT, pair(build, TeX_NODE_POWER, x, two), derivative(build, z, x, 2u)),
                pair(build, TeX_NODE_PRODUCT,
                    pair(build, TeX_NODE_PRODUCT, pair(build, TeX_NODE_PRODUCT, two, x), y), mixed)),
            pair(build, TeX_NODE_PRODUCT, pair(build, TeX_NODE_POWER, y, two), derivative(build, z, y, 2u)));
        const TeX_model_node_t *operator = pair(build, TeX_NODE_DIFFERENCE,
            pair(build, TeX_NODE_POWER, E, two), E);
        operator = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_FENCED, .child_count = 1u, .fence = TeX_FENCE_PARENTHESES}, &operator);
        const TeX_model_node_t *ode = pair(build, TeX_NODE_SUM,
            pair(build, TeX_NODE_SUM, pair(build, TeX_NODE_PRODUCT, A, derivative(build, z, r, 2u)),
                 pair(build, TeX_NODE_PRODUCT, pair(build, TeX_NODE_DIFFERENCE, B, A), derivative(build, z, r, 1u))),
            pair(build, TeX_NODE_PRODUCT, C, z));
        const TeX_model_node_t *polynomial = pair(build, TeX_NODE_SUM,
            pair(build, TeX_NODE_SUM,
                 pair(build, TeX_NODE_PRODUCT, pair(build, TeX_NODE_PRODUCT, A, m),
                      pair(build, TeX_NODE_DIFFERENCE, m, one)), pair(build, TeX_NODE_PRODUCT, B, m)), C);
        const TeX_model_node_t *chart_conditions[] = {x, zero};
        const TeX_model_node_t *rows[] = {
            notation_build_literal(build, TeX_NODE_TEXT, "Radial Euler equation"),
            notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 3u}, coefficients),
            pair(build, TeX_NODE_EQUATION, pair(build, TeX_NODE_APPLY, E, z), radial),
            pair(build, TeX_NODE_EQUATION, radial_second, pair(build, TeX_NODE_APPLY, operator, z)),
            pair(build, TeX_NODE_EQUATION, r, pair(build, TeX_NODE_APPLY,
                notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "ln"), x)),
            pair(build, TeX_NODE_EQUATION, eta, pair(build, TeX_NODE_FRACTION, y, x)),
            pair(build, TeX_NODE_EQUATION, pair(build, TeX_NODE_APPLY, E, z), derivative(build, z, r, 1u)),
            pair(build, TeX_NODE_EQUATION, ode, zero),
            pair(build, TeX_NODE_EQUATION, polynomial, zero),
            notation_build_literal(build, TeX_NODE_TEXT, "A repeated root introduces a logarithmic second solution."),
            notation_build_literal(build, TeX_NODE_TEXT, "The two coefficients are arbitrary functions of η."),
            pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Local chart:"),
                notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                                    .relation = TeX_RELATION_GREATER, .child_count = 2u}, chart_conditions)),
            notation_build_literal(build, TeX_NODE_TEXT, "No boundary data imposed.")
        };
        const TeX_model_node_t *root = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
        display = notation_build_render(build, root);
    }
    valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
            de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    string_free(display); notation_build_free(build); string_free(plain);
    for (size_t i = 0u; i < 3u; ++i) {
        free(text[i]); expr_free(values[i]);
    }
    return valid;
}
