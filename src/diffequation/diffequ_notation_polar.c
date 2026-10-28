/**
 * @file diffequ_notation_polar.c
 * @brief Describe the verified polar-coordinate harmonic family semantically.
 *
 * The native solver supplies the chain-rule identities and cancellation; TeX
 * renders their mathematical structure without knowing Laplace's equation.
 */
#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static const TeX_model_node_t *pair(notation_build_t *b, TeX_node_kind_t k,
                                   const TeX_model_node_t *a, const TeX_model_node_t *c)
{
    const TeX_model_node_t *children[] = {a, c};
    return notation_build_node(b, (TeX_node_view_t){.kind = k, .child_count = 2u}, children);
}

static const TeX_model_node_t *deriv(notation_build_t *b, const TeX_model_node_t *f,
                                     const TeX_model_node_t *x, unsigned order)
{
    const TeX_model_node_t *children[] = {f, x};
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE,
                               .child_count = 2u, .order = order, .partial = true}, children);
}

static const TeX_model_node_t *prime_at(notation_build_t *b, const char *name, unsigned order,
                                        const TeX_model_node_t *argument)
{
    const TeX_model_node_t *f = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, name);
    const TeX_model_node_t *prime = notation_build_node(b,
        (TeX_node_view_t){.kind = TeX_NODE_PRIME, .child_count = 1u, .order = order}, &f);
    return pair(b, TeX_NODE_APPLY, prime, argument);
}

static const TeX_model_node_t *negate(notation_build_t *b, const TeX_model_node_t *v)
{
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &v);
}

/* Present the chain-rule proof and its cancellation without embedding typesetting syntax. */
bool de_polar_laplace_set_steps(diffequ_solve_result_t *result)
{
    static const char plain[] =
        "Set the complex polar coordinates:\n"
        "      z = r·exp(iθ),    w = r·exp(−iθ)\n"
        "Their derivatives are:\n"
        "      z_r = exp(iθ),    z_rr = 0,    z_θ = iz,    z_θθ = −z\n"
        "      w_r = exp(−iθ),   w_rr = 0,    w_θ = −iw,   w_θθ = −w\n"
        "For u = F(z) + G(w), the chain rule gives:\n"
        "      u_r = exp(iθ)F'(z) + exp(−iθ)G'(w)\n"
        "      u_rr = exp(2iθ)F''(z) + exp(−2iθ)G''(w)\n"
        "      u_θθ = −zF'(z) − z²F''(z) − wG'(w) − w²G''(w)\n"
        "Substitute these into the polar Laplacian:\n"
        "      Δu = u_rr + u_r/r + u_θθ/r²\n"
        "The F' and G' terms cancel because z/r = exp(iθ) and w/r = exp(−iθ).\n"
        "The F'' and G'' terms cancel because z²/r² = exp(2iθ) and w²/r² = exp(−2iθ).\n"
        "Therefore Δu = 0, and the general harmonic family is:\n"
        "      u(r, θ) = F(r·exp(iθ)) + G(r·exp(−iθ))";
    notation_build_t *b = result ? notation_build_new() : NULL;
    if (!b)
        return false;
    const TeX_model_node_t *r = notation_build_literal(b, TeX_NODE_IDENTIFIER, "r");
    const TeX_model_node_t *theta = notation_build_literal(b, TeX_NODE_IDENTIFIER, "θ");
    const TeX_model_node_t *z = notation_build_literal(b, TeX_NODE_IDENTIFIER, "z");
    const TeX_model_node_t *w = notation_build_literal(b, TeX_NODE_IDENTIFIER, "w");
    const TeX_model_node_t *u = notation_build_literal(b, TeX_NODE_IDENTIFIER, "u");
    const TeX_model_node_t *i = notation_build_literal(b, TeX_NODE_IDENTIFIER, "i");
    const TeX_model_node_t *e = notation_build_literal(b, TeX_NODE_IDENTIFIER, "e");
    const TeX_model_node_t *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *two = notation_build_literal(b, TeX_NODE_NUMBER, "2");
    const TeX_model_node_t *it = pair(b, TeX_NODE_PRODUCT, i, theta);
    const TeX_model_node_t *ep = pair(b, TeX_NODE_POWER, e, it);
    const TeX_model_node_t *em = pair(b, TeX_NODE_POWER, e, negate(b, it));
    const TeX_model_node_t *e2p = pair(b, TeX_NODE_POWER, e, pair(b, TeX_NODE_PRODUCT, two, it));
    const TeX_model_node_t *e2m = pair(b, TeX_NODE_POWER, e, negate(b, pair(b, TeX_NODE_PRODUCT, two, it)));
    const TeX_model_node_t *fp = prime_at(b, "F", 1u, z), *gp = prime_at(b, "G", 1u, w);
    const TeX_model_node_t *fpp = prime_at(b, "F", 2u, z), *gpp = prime_at(b, "G", 2u, w);
    const TeX_model_node_t *z2 = pair(b, TeX_NODE_POWER, z, two), *w2 = pair(b, TeX_NODE_POWER, w, two);
    const TeX_model_node_t *r2 = pair(b, TeX_NODE_POWER, r, two);
    const TeX_model_node_t *F = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "F");
    const TeX_model_node_t *G = notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "G");
    const TeX_model_node_t *family = pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_APPLY, F, z), pair(b, TeX_NODE_APPLY, G, w));
    const TeX_model_node_t *theta_second = pair(b, TeX_NODE_DIFFERENCE,
        pair(b, TeX_NODE_DIFFERENCE,
            pair(b, TeX_NODE_DIFFERENCE, negate(b, pair(b, TeX_NODE_PRODUCT, z, fp)),
                 pair(b, TeX_NODE_PRODUCT, z2, fpp)), pair(b, TeX_NODE_PRODUCT, w, gp)),
        pair(b, TeX_NODE_PRODUCT, w2, gpp));
    const TeX_model_node_t *laplacian = pair(b, TeX_NODE_SUM,
        pair(b, TeX_NODE_SUM, deriv(b, u, r, 2u), pair(b, TeX_NODE_PRODUCT,
            pair(b, TeX_NODE_FRACTION, notation_build_literal(b, TeX_NODE_NUMBER, "1"), r), deriv(b, u, r, 1u))),
        pair(b, TeX_NODE_PRODUCT, pair(b, TeX_NODE_FRACTION, notation_build_literal(b, TeX_NODE_NUMBER, "1"), r2),
             deriv(b, u, theta, 2u)));
    const TeX_model_node_t *delta_u = pair(b, TeX_NODE_PRODUCT,
        notation_build_literal(b, TeX_NODE_IDENTIFIER, "Δ"), u);
    const TeX_model_node_t *dependent_args[] = {u, r, theta};
    const TeX_model_node_t *u_at = notation_build_node(b,
        (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 3u}, dependent_args);
    const TeX_model_node_t *rows[] = {
        pair(b, TeX_NODE_EQUATION, z, pair(b, TeX_NODE_PRODUCT, r, ep)),
        pair(b, TeX_NODE_EQUATION, w, pair(b, TeX_NODE_PRODUCT, r, em)),
        pair(b, TeX_NODE_EQUATION, u, family),
        pair(b, TeX_NODE_EQUATION, deriv(b, z, r, 1u), ep),
        pair(b, TeX_NODE_EQUATION, deriv(b, z, r, 2u), zero),
        pair(b, TeX_NODE_EQUATION, deriv(b, z, theta, 1u), pair(b, TeX_NODE_PRODUCT, i, z)),
        pair(b, TeX_NODE_EQUATION, deriv(b, z, theta, 2u), negate(b, z)),
        pair(b, TeX_NODE_EQUATION, deriv(b, w, r, 1u), em),
        pair(b, TeX_NODE_EQUATION, deriv(b, w, r, 2u), zero),
        pair(b, TeX_NODE_EQUATION, deriv(b, w, theta, 1u), negate(b, pair(b, TeX_NODE_PRODUCT, i, w))),
        pair(b, TeX_NODE_EQUATION, deriv(b, w, theta, 2u), negate(b, w)),
        pair(b, TeX_NODE_EQUATION, deriv(b, u, r, 1u),
            pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_PRODUCT, ep, fp), pair(b, TeX_NODE_PRODUCT, em, gp))),
        pair(b, TeX_NODE_EQUATION, deriv(b, u, r, 2u),
            pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_PRODUCT, e2p, fpp), pair(b, TeX_NODE_PRODUCT, e2m, gpp))),
        pair(b, TeX_NODE_EQUATION, deriv(b, u, theta, 2u), theta_second),
        pair(b, TeX_NODE_EQUATION, delta_u, laplacian),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_FRACTION, z, r), ep),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_FRACTION, w, r), em),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_FRACTION, z2, r2), e2p),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_FRACTION, w2, r2), e2m),
        pair(b, TeX_NODE_EQUATION, delta_u, zero),
        pair(b, TeX_NODE_EQUATION, u_at, pair(b, TeX_NODE_SUM,
            pair(b, TeX_NODE_APPLY, F, pair(b, TeX_NODE_PRODUCT, r, ep)),
            pair(b, TeX_NODE_APPLY, G, pair(b, TeX_NODE_PRODUCT, r, em))))
    };
    const TeX_model_node_t *root = notation_build_node(b,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = sizeof rows / sizeof *rows}, rows);
    string_t *display = notation_build_render(b, root);
    bool ok = display && de_solve_result_set_steps(result, plain) == 0 &&
              de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0;
    string_free(display); notation_build_free(b);
    return ok;
}
