/**
 * @file diffequ_notation_fourier.c
 * @brief Expose Fourier evolution identities and assumptions as semantic rows.
 *
 * This native adapter supplies the derived real and imaginary symbols, elapsed
 * time and initial-data assumptions. TeX receives formula and prose nodes only.
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

/* Describe the exact mode evolution and the interpretation of its initial-data limit. */
bool de_fourier_steps(diffequ_solve_result_t *result, const expr_t *dependent, const expr_t *real, const expr_t *imaginary,
                       const expr_t *frequency, const expr_t *time, const expr_t *initial_time)
{
    static const char plain[] =
        "Whole-line Fourier evolution with real constant coefficients.\n"
        "Distinct symbols remain distinct: a coefficient such as U is not the dependent variable u.\n"
        "Replace each spatial derivative of order n by (i*k)^n, obtaining P(i*k)=R(k)+i*S(k).\n"
        "Each Fourier mode evolves by exp(-(t-t0)*P(i*k)).\n"
        "Pair positive and negative frequencies to obtain the real cosine kernel, then convolve with the initial data.\n"
        "The highest even power in R has a known positive coefficient, ensuring high-frequency decay for t>t0.\n"
        "Assumptions: whole real spatial line, real finite coefficient parameters, t>t0, Schwartz initial data.\n"
        "Schwartz means smooth with the function and all derivatives decaying faster than every inverse power.\n"
        "The initial condition holds as t tends to t0 from above by Fourier inversion, not by an ordinary kernel at t0.\n"
        "This is an exact integral representation; no numerical integral or boundary-value problem is claimed.\n";
    notation_build_t *build = notation_build_new();
    notation_build_dependent_variables(build, true);
    const TeX_model_node_t *h = notation_build_literal(build, TeX_NODE_IDENTIFIER, "h");
    const TeX_model_node_t *t = notation_build_literal(build, TeX_NODE_IDENTIFIER, expr_symbol_name(time));
    const TeX_model_node_t *k = notation_build_expr(build, frequency);
    const TeX_model_node_t *r = pair(build, TeX_NODE_APPLY,
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "R"), k);
    const TeX_model_node_t *s = pair(build, TeX_NODE_APPLY,
        notation_build_literal(build, TeX_NODE_FUNCTION_NAME, "S"), k);
    const TeX_model_node_t *positive[] = {h, notation_build_literal(build, TeX_NODE_NUMBER, "0")};
    const TeX_model_node_t *definitions[] = {
        pair(build, TeX_NODE_EQUATION, h,
            pair(build, TeX_NODE_DIFFERENCE, t, notation_build_expr(build, initial_time))),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
            .relation = TeX_RELATION_GREATER, .child_count = 2u}, positive),
        pair(build, TeX_NODE_EQUATION, r, notation_build_expr(build, real)),
        pair(build, TeX_NODE_EQUATION, s, notation_build_expr(build, imaginary))
    };
    const TeX_model_node_t *argument = pair(build, TeX_NODE_PRODUCT,
        pair(build, TeX_NODE_PRODUCT, notation_build_literal(build, TeX_NODE_IDENTIFIER, "i"), k),
        notation_build_literal(build, TeX_NODE_TEXT, "spatial coordinate"));
    argument = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &argument);
    const TeX_model_node_t *u = notation_build_expr(build, dependent);
    const TeX_model_node_t *hat = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_HAT, .child_count = 1u}, &u);
    const TeX_model_node_t *derivative_parts[] = {hat, h};
    const TeX_model_node_t *derivative = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE, .child_count = 2u,
                          .order = 1u, .partial = true}, derivative_parts);
    const TeX_model_node_t *symbol = pair(build, TeX_NODE_SUM, r,
        pair(build, TeX_NODE_PRODUCT, notation_build_literal(build, TeX_NODE_IDENTIFIER, "i"), s));
    const TeX_model_node_t *negative_symbol = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &symbol);
    const TeX_model_node_t *exponent = pair(build, TeX_NODE_PRODUCT, h, negative_symbol);
    const TeX_model_node_t *initial = pair(build, TeX_NODE_APPLY, hat,
        notation_build_literal(build, TeX_NODE_NUMBER, "0"));
    const TeX_model_node_t *rows[] = {
        notation_build_literal(build, TeX_NODE_TEXT, "Whole-line Fourier evolution; real constant parameters."),
        notation_build_node(build, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = 4u}, definitions),
        pair(build, TeX_NODE_STATEMENTS,
            notation_build_literal(build, TeX_NODE_TEXT, "The Fourier transform uses the factor"),
            pair(build, TeX_NODE_POWER, notation_build_literal(build, TeX_NODE_IDENTIFIER, "e"), argument)),
        pair(build, TeX_NODE_STATEMENTS, hat,
            notation_build_literal(build, TeX_NODE_TEXT, "denotes the spatial Fourier transform; h is elapsed time.")),
        pair(build, TeX_NODE_EQUATION, derivative, pair(build, TeX_NODE_PRODUCT, negative_symbol, hat)),
        pair(build, TeX_NODE_EQUATION, pair(build, TeX_NODE_APPLY, hat, h),
            pair(build, TeX_NODE_PRODUCT,
                pair(build, TeX_NODE_POWER, notation_build_literal(build, TeX_NODE_IDENTIFIER, "e"), exponent),
                initial)),
        notation_build_literal(build, TeX_NODE_TEXT,
            "Pair opposite frequencies and convolve the resulting kernel with the supplied initial data."),
        notation_build_literal(build, TeX_NODE_TEXT, "Assume smooth, rapidly decreasing initial data on the whole real line."),
        notation_build_literal(build, TeX_NODE_TEXT,
            "All derivatives of the data must also decrease faster than every inverse power."),
        notation_build_literal(build, TeX_NODE_TEXT, "Initial data are recovered as h tends to zero from above by Fourier inversion."),
        notation_build_literal(build, TeX_NODE_TEXT, "Exact integral representation; no finite-interval boundary conditions.")
    };
    const TeX_model_node_t *root = notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
    string_t *rendered = notation_build_render(build, root);
    bool valid = rendered && de_solve_result_set_steps(result, plain) == 0 &&
                            de_solve_result_set_steps_TeX(result, string_c_str(rendered)) == 0;
    string_free(rendered);
    notation_build_free(build);
    return valid;
}
