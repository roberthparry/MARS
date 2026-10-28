/**
 * @file diffequ_notation_heat.c
 * @brief Prepare half-line heat-solution conditions and mathematical derivations.
 *
 * The solver owns the domain, data assumptions and limiting interpretation. This
 * adapter prepares semantic relations and formula rows; TeX owns their syntax.
 */
#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

/* Keep domain restrictions and boundary-limit qualifications with the solution. */
bool de_heat_present(equation_t *solution, const expr_t *space, const expr_t *time,
                      const expr_t *diffusivity, const expr_t *reaction)
{
    char *field = expr_to_string(equ_lhs(solution), style_UNBOUND);
    char *body_text = expr_to_string(equ_rhs(solution), style_UNBOUND);
    const char *space_name = expr_symbol_name(space), *time_name = expr_symbol_name(time);
    char *x = space_name ? strdup(space_name) : expr_to_string(space, style_UNBOUND);
    char *t = time_name ? strdup(time_name) : expr_to_string(time, style_UNBOUND);
    string_t *left = field ? string_new_with(field) : NULL;
    string_t *right = body_text && x && t ? string_sprintf(
        "%s; %s > 0, %s > 0; right half-line, decay at infinity; "
        "continuous locally bounded boundary data, zero at the corner; boundary value by the right-hand spatial limit.",
        body_text, x, t) : NULL;
    notation_build_t *build = notation_build_new();
    const TeX_model_node_t *zero = notation_build_literal(build, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *positive_values[] = {
        notation_build_expr(build, space), notation_build_expr(build, time), notation_build_expr(build, diffusivity)
    };
    const TeX_model_node_t *conditions[4];
    for (size_t i = 0u; i < 3u; ++i) {
        const TeX_model_node_t *parts[] = {positive_values[i], zero};
        conditions[i] = notation_build_node(build,
            (TeX_node_view_t){.kind = TeX_NODE_RELATION, .relation = TeX_RELATION_GREATER, .child_count = 2u}, parts);
    }
    const TeX_model_node_t *real[] = {notation_build_expr(build, reaction), notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NUMBER_SET, .number_set = TeX_SET_REALS}, NULL)};
    conditions[3] = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_RELATION, .relation = TeX_RELATION_MEMBER, .child_count = 2u}, real);
    const TeX_model_node_t *rows[] = {
        notation_build_expr(build, equ_rhs(solution)),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 4u}, conditions),
        notation_build_literal(build, TeX_NODE_TEXT, "Right half-line; decay at spatial infinity."),
        notation_build_literal(build, TeX_NODE_TEXT, "Continuous, locally bounded boundary data; zero at the corner."),
        notation_build_literal(build, TeX_NODE_TEXT, "Boundary value is the right-hand spatial limit, not substitution at zero.")
    };
    const TeX_model_node_t *body = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = 5u}, rows);
    bool valid = de_notation_display(solution, left, right, build, body);
    notation_build_free(build);
    string_free(right);
    string_free(left);
    free(t);
    free(x);
    free(body_text);
    free(field);
    return valid;
}

/* Explain the verified kernel and its boundary limit using native mathematical source. */
bool de_heat_set_steps(diffequ_solve_result_t *result)
{
    static const char plain[] =
        "Normalise to u_t=kappa*u_xx-a*u, with constant kappa>0 and real constant a.\n"
        "On the right half-line use zero initial data, prescribed Dirichlet history and decay at infinity.\n"
        "Multiplication by exp(a*t) removes the reaction term.\n"
        "The heat Dirichlet Poisson kernel is x*exp(-x^2/(4*kappa*tau))/(2*sqrt(pi*kappa)*tau^(3/2)).\n"
        "Multiply by exp(-a*tau) and convolve with the boundary history.\n"
        "For x>0 the integrand vanishes rapidly at tau=0; it solves the PDE and has zero initial trace.\n"
        "As x tends to zero from above, the kernel is an approximate identity and recovers continuous boundary data.\n"
        "Do not substitute x=0 inside the integral: that would miss the boundary limit.\n"
        "Assume continuous locally bounded boundary data, zero at t=0 for corner compatibility.\n"
        "The result decays at spatial infinity on finite time intervals; "
        "no left half-line or finite interval is asserted.";
    static const de_notation_step_t rows[] = {
        {"kappa is positive; a is real.", "Dt(u)", "@kappa*Dxx(u)-a*u"},
        {NULL, "w", "exp(a*t)*u"},
        {"The transformed equation is", "Dt(w)", "@kappa*Dxx(w)"},
        {"For x and elapsed time h positive:", "P(x,h)",
            "x/(2*sqrt(@pi*@kappa))*h^(-3/2)*exp(-x^2/(4*@kappa*h)-a*h)"},
        {NULL, "u", "@S_0^t P(x,t-s)*g(s) ds"},
        {"For x > 0 the kernel solves the PDE and has zero initial trace.", NULL, NULL},
        {"As x tends to zero from above, u(x,t) tends to g(t) by the approximate-identity boundary limit.", NULL, NULL},
        {"Assume continuous locally bounded data, g(0) = 0, and decay at infinity.", NULL, NULL},
        {"Here x, t and g denote the recognised space, time and boundary history.", NULL, NULL}
    };
    return de_notation_steps(result, plain, rows, sizeof rows / sizeof *rows);
}
