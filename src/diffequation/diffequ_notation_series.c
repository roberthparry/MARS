/**
 * @file diffequ_notation_series.c
 * @brief Explain Taylor-series ODE solutions through semantic mathematical nodes.
 *
 * The differential-equation solver owns the recurrence and convergence claims;
 * TeX presents the derivative identities, coefficients and remainder.
 */
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static const TeX_model_node_t *pair(notation_build_t *b, TeX_node_kind_t kind,
                                   const TeX_model_node_t *a, const TeX_model_node_t *c)
{
    const TeX_model_node_t *children[] = {a, c};
    return notation_build_node(b, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *prime(notation_build_t *b, const TeX_model_node_t *body, unsigned order)
{
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_PRIME, .child_count = 1u, .order = order}, &body);
}

static const TeX_model_node_t *derivative(notation_build_t *b, const TeX_model_node_t *body,
                                          const TeX_model_node_t *var, bool partial)
{
    const TeX_model_node_t *children[] = {body, var};
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE,
                               .child_count = 2u, .order = 1u, .partial = partial}, children);
}

static const TeX_model_node_t *apply3(notation_build_t *b, const TeX_model_node_t *fn,
                                     const TeX_model_node_t *a, const TeX_model_node_t *c, const TeX_model_node_t *d)
{
    const TeX_model_node_t *children[] = {fn, a, c, d};
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 4u}, children);
}

/* Describe the total-derivative recurrence and the local truncated series. */
bool de_series_steps(const diffequ_t *de, diffequ_solve_result_t *result)
{
    if (!de || !result)
        return false;
    char *centre = expr_to_string(result->series_centre, style_UNBOUND);
    string_t *plain = centre ? string_sprintf(
        "Local Taylor series about %s, through degree %zu.\n"
        "Here x0 is the expansion point and N is the highest retained power.\n"
        "Using generic coordinates, write y'' = f(x,y,p), with p = y' (the slope).\n"
        "Along a solution, dy/dx = p and dp/dx = f.\n"
        "D is total differentiation along that solution: it applies the chain rule, not a new unknown.\n"
        "For any g(x,y,p), Dg = partial_x g + p*partial_y g + f*partial_p g\n"
        "                   = d/dx g(x,y(x),p(x)).\n"
        "The partial derivatives hold the other coordinates fixed.\n"
        "D^k means applying D k times: y'' = f, y''' = Df, y'''' = D^2 f.\n"
        "Taylor coefficients are derivatives at x0 divided by factorials:\n"
        "c0 = y(x0), c1 = y'(x0), cn = y^(n)(x0)/n! = (D^(n-2) f)(x0,c0,c1)/n! for n >= 2.\n"
        "The implementation computes the same coefficients by matching powers in the ODE,\n"
        "using truncated polynomial convolution.\n"
        "Missing initial values remain arbitrary constants.\n"
        "Polynomial f guarantees local convergence for finite initial data; no radius or numerical error bound is supplied.\n"
        "O((x-x0)^(%zu)) denotes omitted higher powers; this is a local truncated series, not a closed form.\n",
        centre, result->series_degree, result->series_degree + 1u) : NULL;
    notation_build_t *b = plain ? notation_build_new() : NULL;
    string_t *display = NULL;
    if (b) {
        const TeX_model_node_t *x = notation_build_literal(b, TeX_NODE_IDENTIFIER, "x");
        const TeX_model_node_t *y = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "y");
        const TeX_model_node_t *p = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "p");
        const TeX_model_node_t *f = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "f");
        const TeX_model_node_t *g = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "g");
        const TeX_model_node_t *D = notation_build_literal(b, TeX_NODE_IDENTIFIER, "D");
        const TeX_model_node_t *c = notation_build_literal(b, TeX_NODE_IDENTIFIER, "c");
        const TeX_model_node_t *n = notation_build_literal(b, TeX_NODE_IDENTIFIER, "n");
        const TeX_model_node_t *N = notation_build_literal(b, TeX_NODE_IDENTIFIER, "N");
        const TeX_model_node_t *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
        const TeX_model_node_t *one = notation_build_literal(b, TeX_NODE_NUMBER, "1");
        const TeX_model_node_t *two = notation_build_literal(b, TeX_NODE_NUMBER, "2");
        const TeX_model_node_t *minus_one = notation_build_literal(b, TeX_NODE_NUMBER, "-1");
        const TeX_model_node_t *x0 = pair(b, TeX_NODE_SUBSCRIPT, x, zero);
        const TeX_model_node_t *c0 = pair(b, TeX_NODE_SUBSCRIPT, c, zero);
        const TeX_model_node_t *c1 = pair(b, TeX_NODE_SUBSCRIPT, c, one);
        const TeX_model_node_t *cn = pair(b, TeX_NODE_SUBSCRIPT, c, n);
        string_t *degree_text = string_sprintf("%zu", result->series_degree);
        const TeX_model_node_t *degree = degree_text ? notation_build_literal(b, TeX_NODE_NUMBER, string_c_str(degree_text)) : NULL;
        string_free(degree_text);
        const TeX_model_node_t *fxyz = apply3(b, f, x, y, p), *gxyz = apply3(b, g, x, y, p);
        const TeX_model_node_t *chain = pair(b, TeX_NODE_SUM,
            pair(b, TeX_NODE_SUM, derivative(b, gxyz, x, true),
                 pair(b, TeX_NODE_PRODUCT, p, derivative(b, gxyz, y, true))),
            pair(b, TeX_NODE_PRODUCT, fxyz, derivative(b, gxyz, p, true)));
        const TeX_model_node_t *y_at_x = pair(b, TeX_NODE_APPLY, y, x);
        const TeX_model_node_t *p_at_x = pair(b, TeX_NODE_APPLY, p, x);
        const TeX_model_node_t *along = apply3(b, g, x, y_at_x, p_at_x);
        const TeX_model_node_t *factorial = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_FACTORIAL, .child_count = 1u}, &n);
        const TeX_model_node_t *inverse_factorial = pair(b, TeX_NODE_POWER, factorial, minus_one);
        const TeX_model_node_t *yn_children[] = {y, n, x0};
        const TeX_model_node_t *yn = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_ORDERED_DERIVATIVE, .child_count = 3u}, yn_children);
        const TeX_model_node_t *operator = pair(b, TeX_NODE_PRODUCT,
            pair(b, TeX_NODE_POWER, D, pair(b, TeX_NODE_DIFFERENCE, n, two)), f);
        const TeX_model_node_t *op_at = apply3(b, operator, x0, c0, c1);
        const TeX_model_node_t *series_term = pair(b, TeX_NODE_PRODUCT, cn,
            pair(b, TeX_NODE_POWER, pair(b, TeX_NODE_DIFFERENCE, x, x0), n));
        const TeX_model_node_t *sum_children[] = {series_term, n, zero, N};
        const TeX_model_node_t *sum = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_SUMMATION, .child_count = 4u}, sum_children);
        const TeX_model_node_t *remainder = pair(b, TeX_NODE_APPLY,
            notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "O"),
            pair(b, TeX_NODE_POWER, pair(b, TeX_NODE_DIFFERENCE, x, x0), pair(b, TeX_NODE_SUM, N, one)));
        const TeX_model_node_t *rows[] = {
            notation_build_literal(b, TeX_NODE_TEXT, "Local Taylor series"),
            pair(b, TeX_NODE_EQUATION, x0, notation_build_expr(b, result->series_centre)),
            pair(b, TeX_NODE_EQUATION, N, degree),
            pair(b, TeX_NODE_EQUATION, prime(b, y, 2u), fxyz),
            pair(b, TeX_NODE_EQUATION, p, prime(b, y, 1u)),
            pair(b, TeX_NODE_EQUATION, derivative(b, y, x, false), p),
            pair(b, TeX_NODE_EQUATION, derivative(b, p, x, false), fxyz),
            notation_build_literal(b, TeX_NODE_TEXT, "D is total differentiation along a solution and applies the chain rule."),
            pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_PRODUCT, D, gxyz), chain),
            pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_PRODUCT, D, gxyz), derivative(b, along, x, false)),
            notation_build_literal(b, TeX_NODE_TEXT, "Partial derivatives hold the other coordinates fixed."),
            pair(b, TeX_NODE_EQUATION, prime(b, y, 3u), pair(b, TeX_NODE_PRODUCT, D, f)),
            pair(b, TeX_NODE_EQUATION, prime(b, y, 4u), pair(b, TeX_NODE_PRODUCT, pair(b, TeX_NODE_POWER, D, two), f)),
            pair(b, TeX_NODE_EQUATION, c0, pair(b, TeX_NODE_APPLY, y, x0)),
            pair(b, TeX_NODE_EQUATION, c1, pair(b, TeX_NODE_APPLY, prime(b, y, 1u), x0)),
            notation_build_literal(b, TeX_NODE_TEXT, "Taylor coefficients are derivatives divided by factorials."),
            pair(b, TeX_NODE_EQUATION, cn, pair(b, TeX_NODE_PRODUCT, inverse_factorial, yn)),
            pair(b, TeX_NODE_EQUATION, cn, pair(b, TeX_NODE_PRODUCT, inverse_factorial, op_at)),
            notation_build_literal(b, TeX_NODE_TEXT, "The last two identities hold for n at least 2."),
            pair(b, TeX_NODE_EQUATION, y, pair(b, TeX_NODE_SUM, sum, remainder)),
            notation_build_literal(b, TeX_NODE_TEXT, "Coefficients are computed by matching powers; missing initial values remain arbitrary."),
            notation_build_literal(b, TeX_NODE_TEXT, "The remainder denotes omitted higher powers; this local truncated series is not a closed form."),
            notation_build_literal(b, TeX_NODE_TEXT, "Polynomial f gives local convergence for finite initial data; no radius or numerical error bound is supplied.")
        };
        const TeX_model_node_t *root = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
        display = notation_build_render(b, root);
    }
    diffequ_solve_result_t *analysis = plain ? de_lie_unsolved_analysis(de, true) : NULL;
    if (analysis && de_solve_result_steps(analysis))
        string_append_format(plain, "\n%s", de_solve_result_steps(analysis));
    bool valid = plain && display && de_solve_result_set_steps(result, string_c_str(plain)) == 0 &&
                 de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    de_solve_result_free(analysis); string_free(display); notation_build_free(b);
    string_free(plain); free(centre);
    return valid;
}
