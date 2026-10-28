/**
 * @file diffequ_notation_linear.c
 * @brief Describe parameter-linear PDE solutions as semantic mathematical nodes.
 *
 * The native solver supplies the coefficient, primitives and verified solution.
 * TeX formats the integrating-factor derivation without knowing solver internals.
 */
#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static const TeX_model_node_t *pair(notation_build_t *b, TeX_node_kind_t kind,
                                   const TeX_model_node_t *a, const TeX_model_node_t *c)
{
    const TeX_model_node_t *children[] = {a, c};
    return notation_build_node(b, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *partial(notation_build_t *b, const TeX_model_node_t *body,
                                        const TeX_model_node_t *variable)
{
    const TeX_model_node_t *children[] = {body, variable};
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE,
                               .child_count = 2u, .order = 1u, .partial = true}, children);
}

/* Present the integrating factor, product rule, primitives and verified result. */
bool de_pde_parameter_linear_steps(const expr_t *independent, const expr_t *dependent, expr_t *const *parameters,
                                   size_t parameter_count, const expr_t *coefficient, const expr_t *forcing,
                                   const expr_t *coefficient_integral, const expr_t *integrating_factor,
                                   const expr_t *weighted_forcing, const expr_t *forcing_integral,
                                   const expr_t *arbitrary, const expr_t *solved_right, char **steps_out,
                                   char **steps_TeX_out)
{
    if (!steps_out || !steps_TeX_out)
        return false;
    *steps_out = NULL; *steps_TeX_out = NULL;
    const expr_t *values[] = {dependent, coefficient, forcing, coefficient_integral, integrating_factor,
                              weighted_forcing, forcing_integral, arbitrary, solved_right};
    const char *name = independent ? expr_symbol_name(independent) : NULL;
    char *text[9] = {NULL};
    bool valid = name && parameters && parameter_count > 0u && parameter_count <= 65536u;
    for (size_t j = 0u; valid && j < 9u; ++j) {
        text[j] = values[j] ? expr_to_string(values[j], style_UNBOUND) : NULL;
        valid = text[j] != NULL;
    }
    string_t *parameter_text = valid ? string_new() : NULL;
    for (size_t j = 0u; parameter_text && j < parameter_count; ++j) {
        char *value = parameters[j] ? expr_to_string(parameters[j], style_UNBOUND) : NULL;
        bool appended = value && (!j || string_append_cstr(parameter_text, ", ") == 0) &&
                        string_append_cstr(parameter_text, value) == 0;
        free(value);
        if (!appended) {
            string_free(parameter_text);
            parameter_text = NULL;
        }
    }
    string_t *plain = parameter_text ? string_sprintf(
        "Treat %S as parameter%s and solve in %s.\n"
        "Write the linear equation in standard form:\n"
        "      ∂%s/∂%s + (%s)%s = %s\n"
        "Use the general rule ∂v/∂s + Pv = Q. Its integrating factor\n"
        "      μ = exp(∫P ds)\n"
        "satisfies ∂μ/∂s = Pμ. Here P = %s and Q = %s.\n"
        "Integrate the coefficient and form the integrating factor:\n"
        "      ∫(%s)d%s = %s\n"
        "      μ = exp(∫(%s)d%s) = %s\n"
        "Thus ∂μ/∂%s = (%s)μ. Multiply by μ and use the product rule:\n"
        "      μ∂%s/∂%s + (∂μ/∂%s)%s = μ%s\n"
        "      ∂(μ%s)/∂%s = μ%s = %s\n"
        "Integrate with respect to %s:\n"
        "      μ%s = %s + %s\n"
        "Therefore:\n"
        "      %s = %s", parameter_text, parameter_count == 1u ? "" : "s", name,
        text[0], name, text[1], text[0], text[2], text[1], text[2], text[1], name, text[3], text[1], name,
        text[4], name, text[1], text[0], name, name, text[0], text[2], text[0], name, text[2], text[5],
        name, text[0], text[6], text[7], text[0], text[8]) : NULL;
    notation_build_t *b = plain ? notation_build_new() : NULL;
    const TeX_model_node_t **params = b ? calloc(parameter_count, sizeof *params) : NULL;
    string_t *display = NULL;
    if (params) {
        const TeX_model_node_t *nodes[9];
        for (size_t j = 0u; j < 9u; ++j)
            nodes[j] = notation_build_expr(b, values[j]);
        for (size_t j = 0u; j < parameter_count; ++j)
            params[j] = notation_build_expr(b, parameters[j]);
        const TeX_model_node_t *s = notation_build_literal(b, TeX_NODE_IDENTIFIER, name);
        const TeX_model_node_t *u = nodes[0], *P = nodes[1], *Q = nodes[2];
        const TeX_model_node_t *mu = notation_build_literal(b, TeX_NODE_IDENTIFIER, "μ");
        const TeX_model_node_t *e = notation_build_literal(b, TeX_NODE_IDENTIFIER, "e");
        const TeX_model_node_t *integral = pair(b, TeX_NODE_INTEGRAL, P, s);
        const TeX_model_node_t *mu_u = pair(b, TeX_NODE_PRODUCT, mu, u);
        const TeX_model_node_t *mu_Q = pair(b, TeX_NODE_PRODUCT, mu, Q);
        const TeX_model_node_t *param_list = notation_build_node(b,
            (TeX_node_view_t){.kind = TeX_NODE_LIST, .child_count = parameter_count}, params);
        const TeX_model_node_t *rows[] = {
            pair(b, TeX_NODE_STATEMENTS, notation_build_literal(b, TeX_NODE_TEXT, "Treat as parameters:"), param_list),
            pair(b, TeX_NODE_STATEMENTS, notation_build_literal(b, TeX_NODE_TEXT, "Solve in"), s),
            pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_SUM, partial(b, u, s), pair(b, TeX_NODE_PRODUCT, P, u)), Q),
            pair(b, TeX_NODE_EQUATION, integral, nodes[3]),
            pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_EQUATION, mu, pair(b, TeX_NODE_POWER, e, integral)), nodes[4]),
            pair(b, TeX_NODE_EQUATION, partial(b, mu, s), pair(b, TeX_NODE_PRODUCT, P, mu)),
            pair(b, TeX_NODE_EQUATION,
                pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_PRODUCT, mu, partial(b, u, s)),
                     pair(b, TeX_NODE_PRODUCT, partial(b, mu, s), u)), mu_Q),
            pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_EQUATION, partial(b, mu_u, s), mu_Q), nodes[5]),
            pair(b, TeX_NODE_EQUATION, mu_u, pair(b, TeX_NODE_SUM, nodes[6], nodes[7])),
            pair(b, TeX_NODE_EQUATION, u, nodes[8])
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
    free(params); string_free(display); notation_build_free(b); string_free(plain); string_free(parameter_text);
    for (size_t j = 0u; j < 9u; ++j)
        free(text[j]);
    return valid;
}
