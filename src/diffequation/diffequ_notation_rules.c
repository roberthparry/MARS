/**
 * @file diffequ_notation_rules.c
 * @brief Describe recognised differential-equation solver rules semantically.
 *
 * The native module chooses rule names, mathematical reductions and solution
 * branches. TeX renders those generic nodes without knowledge of solver families.
 */
#include <stdint.h>
#include <stdlib.h>

#include "diffequation-t.h"
#include "TeX-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

typedef const TeX_model_node_t *(*rule_factory_t)(notation_build_t *build);
typedef struct {
    const char *name;
    const char *plain;
    rule_factory_t formula;
} rule_description_t;

static const TeX_model_node_t *pair(notation_build_t *b, TeX_node_kind_t kind,
                                   const TeX_model_node_t *a, const TeX_model_node_t *c)
{
    const TeX_model_node_t *children[] = {a, c};
    return notation_build_node(b, (TeX_node_view_t){.kind = kind, .child_count = 2u}, children);
}

static const TeX_model_node_t *symbol(notation_build_t *b, const char *name)
{
    return notation_build_literal(b, TeX_NODE_IDENTIFIER, name);
}

static const TeX_model_node_t *call(notation_build_t *b, const char *name, const TeX_model_node_t *arg)
{
    return pair(b, TeX_NODE_APPLY, notation_build_literal(b, TeX_NODE_FUNCTION_NAME, name), arg);
}

static const TeX_model_node_t *prime(notation_build_t *b, const TeX_model_node_t *value, unsigned order)
{
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_PRIME, .child_count = 1u, .order = order}, &value);
}

static const TeX_model_node_t *partial(notation_build_t *b, const TeX_model_node_t *body,
                                       const TeX_model_node_t *variable)
{
    const TeX_model_node_t *children[] = {body, variable};
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_VARIABLE_DERIVATIVE,
                               .child_count = 2u, .order = 1u, .partial = true}, children);
}

static const TeX_model_node_t *differential(notation_build_t *b, const TeX_model_node_t *variable)
{
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_DIFFERENTIAL, .child_count = 1u}, &variable);
}

static const TeX_model_node_t *neq(notation_build_t *b, const TeX_model_node_t *a, const TeX_model_node_t *c)
{
    const TeX_model_node_t *children[] = {a, c};
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_RELATION,
                               .relation = TeX_RELATION_NOT_EQUAL, .child_count = 2u}, children);
}

static const TeX_model_node_t *statements(notation_build_t *b, const TeX_model_node_t *const *nodes, size_t count)
{
    return notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_STATEMENTS, .child_count = count}, nodes);
}

static const TeX_model_node_t *math(notation_build_t *b, const char *source)
{
    return notation_build_formula(b, source);
}

static const TeX_model_node_t *rule_bernoulli(notation_build_t *b)
{
    const TeX_model_node_t *y = symbol(b, "y"), *x = symbol(b, "x"), *n = symbol(b, "n"), *v = symbol(b, "v");
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_SUM, prime(b, y, 1u),
            pair(b, TeX_NODE_PRODUCT, call(b, "P", x), y)),
            pair(b, TeX_NODE_PRODUCT, call(b, "Q", x), pair(b, TeX_NODE_POWER, y, n))),
        pair(b, TeX_NODE_EQUATION, v, pair(b, TeX_NODE_POWER, y,
            pair(b, TeX_NODE_DIFFERENCE, notation_build_literal(b, TeX_NODE_NUMBER, "1"), n)))
    };
    return statements(b, nodes, 2u);
}

static const TeX_model_node_t *rule_characteristics(notation_build_t *b)
{
    const TeX_model_node_t *x = symbol(b, "x"), *y = symbol(b, "y"), *u = symbol(b, "u");
    const TeX_model_node_t *dx = pair(b, TeX_NODE_FRACTION, differential(b, x), symbol(b, "a"));
    const TeX_model_node_t *dy = pair(b, TeX_NODE_FRACTION, differential(b, y), symbol(b, "b"));
    const TeX_model_node_t *du = pair(b, TeX_NODE_FRACTION, differential(b, u), math(b, "f-c*u"));
    return pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_EQUATION, dx, dy), du);
}

static const TeX_model_node_t *rule_constant_linear(notation_build_t *b)
{
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_PRODUCT, call(b, "P", symbol(b, "D")), symbol(b, "y")),
             call(b, "f", symbol(b, "x"))),
        pair(b, TeX_NODE_EQUATION, call(b, "P", symbol(b, "m")), notation_build_literal(b, TeX_NODE_NUMBER, "0"))
    };
    return statements(b, nodes, 2u);
}

static const TeX_model_node_t *rule_transport(notation_build_t *b)
{
    const TeX_model_node_t *j = symbol(b, "j"), *s = symbol(b, "s");
    const TeX_model_node_t *xj = pair(b, TeX_NODE_SUBSCRIPT, symbol(b, "x"), j);
    const TeX_model_node_t *aj = pair(b, TeX_NODE_SUBSCRIPT, symbol(b, "a"), j);
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_FRACTION, differential(b, xj), differential(b, s)), aj),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_FRACTION, differential(b, symbol(b, "u")), differential(b, s)),
             math(b, "f-c*u"))
    };
    return statements(b, nodes, 2u);
}

static const TeX_model_node_t *rule_quadratic(notation_build_t *b)
{
    const TeX_model_node_t *x = symbol(b, "x"), *y = symbol(b, "y"), *p = symbol(b, "p");
    const TeX_model_node_t *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *two = notation_build_literal(b, TeX_NODE_NUMBER, "2");
    const TeX_model_node_t *quadratic = pair(b, TeX_NODE_SUM,
        pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_PRODUCT, call(b, "A", y), pair(b, TeX_NODE_POWER, p, two)),
             pair(b, TeX_NODE_PRODUCT, call(b, "B", y), p)), call(b, "D", y));
    const TeX_model_node_t *nodes[] = {pair(b, TeX_NODE_EQUATION, p, prime(b, y, 1u)),
        pair(b, TeX_NODE_EQUATION, quadratic, zero),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_FRACTION, differential(b, x), differential(b, y)),
             pair(b, TeX_NODE_FRACTION, notation_build_literal(b, TeX_NODE_NUMBER, "1"), p))};
    return statements(b, nodes, 3u);
}

static const TeX_model_node_t *rule_exact_derivative(notation_build_t *b)
{
    const TeX_model_node_t *y = symbol(b, "y"), *x = symbol(b, "x");
    const TeX_model_node_t *children[] = {notation_build_literal(b, TeX_NODE_FUNCTION_NAME, "G"),
                                          y, prime(b, y, 1u), prime(b, y, 2u)};
    const TeX_model_node_t *G = notation_build_node(b,
        (TeX_node_view_t){.kind = TeX_NODE_APPLY, .child_count = 4u}, children);
    const TeX_model_node_t *args[] = {G, x};
    return pair(b, TeX_NODE_EQUATION, notation_build_node(b,
        (TeX_node_view_t){.kind = TeX_NODE_DERIVATIVE, .child_count = 2u, .order = 1u}, args), call(b, "f", x));
}

static const TeX_model_node_t *rule_exact_first(notation_build_t *b)
{
    const TeX_model_node_t *M = symbol(b, "M"), *N = symbol(b, "N"), *x = symbol(b, "x"), *y = symbol(b, "y");
    const TeX_model_node_t *phi = symbol(b, "Φ"), *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_SUM,
            pair(b, TeX_NODE_PRODUCT, M, differential(b, x)), pair(b, TeX_NODE_PRODUCT, N, differential(b, y))), zero),
        pair(b, TeX_NODE_EQUATION, partial(b, M, y), partial(b, N, x)),
        pair(b, TeX_NODE_EQUATION, partial(b, phi, x), M),
        pair(b, TeX_NODE_EQUATION, partial(b, phi, y), N)
    };
    return statements(b, nodes, 4u);
}

static const TeX_model_node_t *rule_homogeneous(notation_build_t *b)
{
    const TeX_model_node_t *x = symbol(b, "x"), *y = symbol(b, "y"), *v = symbol(b, "v");
    const TeX_model_node_t *nodes[] = {pair(b, TeX_NODE_EQUATION, v, pair(b, TeX_NODE_FRACTION, y, x)),
        pair(b, TeX_NODE_EQUATION, prime(b, y, 1u),
             pair(b, TeX_NODE_SUM, v, pair(b, TeX_NODE_PRODUCT, x, prime(b, v, 1u))))};
    return statements(b, nodes, 2u);
}

static const TeX_model_node_t *rule_laplace(notation_build_t *b)
{
    return pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_PRODUCT, symbol(b, "Δ"), symbol(b, "u")),
                notation_build_literal(b, TeX_NODE_NUMBER, "0"));
}

static const TeX_model_node_t *rule_linear(notation_build_t *b)
{
    const TeX_model_node_t *u = symbol(b, "u"), *s = symbol(b, "s"), *mu = symbol(b, "μ");
    const TeX_model_node_t *P = call(b, "P", s), *Q = call(b, "Q", s);
    const TeX_model_node_t *integral = pair(b, TeX_NODE_INTEGRAL, P, s);
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_SUM, partial(b, u, s), pair(b, TeX_NODE_PRODUCT, P, u)), Q),
        pair(b, TeX_NODE_EQUATION, mu, pair(b, TeX_NODE_POWER, symbol(b, "e"), integral)),
        pair(b, TeX_NODE_EQUATION, partial(b, pair(b, TeX_NODE_PRODUCT, mu, u), s), pair(b, TeX_NODE_PRODUCT, mu, Q))
    };
    return statements(b, nodes, 3u);
}

static const TeX_model_node_t *rule_substitution(notation_build_t *b)
{
    const TeX_model_node_t *y = symbol(b, "y"), *u = symbol(b, "u");
    const TeX_model_node_t *nodes[] = {pair(b, TeX_NODE_EQUATION, u, math(b, "a*x+b*y+c")),
        pair(b, TeX_NODE_EQUATION, prime(b, u, 1u),
             pair(b, TeX_NODE_SUM, symbol(b, "a"), pair(b, TeX_NODE_PRODUCT, symbol(b, "b"), prime(b, y, 1u))))};
    return statements(b, nodes, 2u);
}

static const TeX_model_node_t *rule_transformation(notation_build_t *b)
{
    const TeX_model_node_t *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *nodes[] = {pair(b, TeX_NODE_EQUATION, symbol(b, "X"), math(b, "a*x+b*y")),
        pair(b, TeX_NODE_EQUATION, symbol(b, "Y"), math(b, "c*x+d*y")), neq(b, math(b, "a*d-b*c"), zero)};
    return statements(b, nodes, 3u);
}

static const TeX_model_node_t *rule_none(notation_build_t *b)
{
    const TeX_model_node_t *R = symbol(b, "R");
    R = notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_CALLIGRAPHIC, .child_count = 1u}, &R);
    return pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_APPLY, R, symbol(b, "E")),
                notation_build_literal(b, TeX_NODE_NUMBER, "0"));
}

static const TeX_model_node_t *rule_bessel(notation_build_t *b)
{
    const TeX_model_node_t *x = symbol(b, "x"), *y = symbol(b, "y"), *u = symbol(b, "u"), *z = symbol(b, "z");
    const TeX_model_node_t *m = symbol(b, "m"), *n = symbol(b, "n"), *a = symbol(b, "a"), *p = symbol(b, "p");
    const TeX_model_node_t *nu = symbol(b, "ν"), *mu = symbol(b, "μ"), *K = symbol(b, "K");
    const TeX_model_node_t *one = notation_build_literal(b, TeX_NODE_NUMBER, "1");
    const TeX_model_node_t *two = notation_build_literal(b, TeX_NODE_NUMBER, "2");
    const TeX_model_node_t *sqrt_a = notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_ROOT, .child_count = 1u}, &a);
    const TeX_model_node_t *sqrt_x = notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_ROOT, .child_count = 1u}, &x);
    const TeX_model_node_t *m2 = pair(b, TeX_NODE_SUM, m, two);
    const TeX_model_node_t *coefficient = pair(b, TeX_NODE_DIFFERENCE, pair(b, TeX_NODE_POWER, z, two),
                                                pair(b, TeX_NODE_POWER, nu, two));
    const TeX_model_node_t *transformed = pair(b, TeX_NODE_SUM,
        pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_PRODUCT, pair(b, TeX_NODE_POWER, z, two), prime(b, u, 2u)),
             pair(b, TeX_NODE_PRODUCT, z, prime(b, u, 1u))), pair(b, TeX_NODE_PRODUCT, coefficient, u));
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_SUM, prime(b, y, 2u),
            pair(b, TeX_NODE_PRODUCT, pair(b, TeX_NODE_PRODUCT, a, pair(b, TeX_NODE_POWER, x, m)), y)),
            pair(b, TeX_NODE_PRODUCT, symbol(b, "b"), pair(b, TeX_NODE_POWER, x, n))),
        pair(b, TeX_NODE_EQUATION, nu, pair(b, TeX_NODE_FRACTION, one, m2)),
        pair(b, TeX_NODE_EQUATION, p, pair(b, TeX_NODE_FRACTION, m2, two)),
        pair(b, TeX_NODE_EQUATION, z, pair(b, TeX_NODE_PRODUCT, pair(b, TeX_NODE_FRACTION, sqrt_a, p),
                                           pair(b, TeX_NODE_POWER, x, p))),
        pair(b, TeX_NODE_EQUATION, y, pair(b, TeX_NODE_PRODUCT, sqrt_x, pair(b, TeX_NODE_APPLY, u, z))),
        pair(b, TeX_NODE_EQUATION, mu, pair(b, TeX_NODE_FRACTION,
            pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_DIFFERENCE, pair(b, TeX_NODE_PRODUCT, two, n), m), one), m2)),
        pair(b, TeX_NODE_EQUATION, K, pair(b, TeX_NODE_PRODUCT,
            pair(b, TeX_NODE_FRACTION, symbol(b, "b"), pair(b, TeX_NODE_POWER, p, two)),
            pair(b, TeX_NODE_POWER, pair(b, TeX_NODE_FRACTION, p, sqrt_a), pair(b, TeX_NODE_SUM, mu, one)))),
        pair(b, TeX_NODE_EQUATION, transformed,
             pair(b, TeX_NODE_PRODUCT, K, pair(b, TeX_NODE_POWER, z, pair(b, TeX_NODE_SUM, mu, one))))
    };
    return statements(b, nodes, sizeof nodes / sizeof *nodes);
}

static const TeX_model_node_t *rule_separable(notation_build_t *b)
{
    const TeX_model_node_t *x = symbol(b, "x"), *y = symbol(b, "y"), *one = notation_build_literal(b, TeX_NODE_NUMBER, "1");
    const TeX_model_node_t *X = call(b, "X", x), *Y = call(b, "Y", y);
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, prime(b, y, 1u), pair(b, TeX_NODE_PRODUCT, X, Y)),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_INTEGRAL, pair(b, TeX_NODE_FRACTION, one, Y), y),
             pair(b, TeX_NODE_SUM, pair(b, TeX_NODE_INTEGRAL, X, x), symbol(b, "C")))
    };
    return statements(b, nodes, 2u);
}

static const TeX_model_node_t *rule_stationary(notation_build_t *b)
{
    const TeX_model_node_t *psi = symbol(b, "ψ"), *zero = notation_build_literal(b, TeX_NODE_NUMBER, "0");
    const TeX_model_node_t *t = symbol(b, "t"), *t0 = pair(b, TeX_NODE_SUBSCRIPT, t, zero);
    const TeX_model_node_t *psi0 = pair(b, TeX_NODE_SUBSCRIPT, psi, zero), *E = symbol(b, "E");
    const TeX_model_node_t *exponent = pair(b, TeX_NODE_PRODUCT,
        pair(b, TeX_NODE_PRODUCT, symbol(b, "i"), E), pair(b, TeX_NODE_DIFFERENCE, t, t0));
    exponent = notation_build_node(b, (TeX_node_view_t){.kind = TeX_NODE_NEGATIVE, .child_count = 1u}, &exponent);
    const TeX_model_node_t *nodes[] = {
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_PRODUCT, symbol(b, "H"), psi0),
             pair(b, TeX_NODE_PRODUCT, E, psi0)),
        pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_APPLY, psi, t),
             pair(b, TeX_NODE_PRODUCT, psi0, pair(b, TeX_NODE_POWER, symbol(b, "e"), exponent)))
    };
    return statements(b, nodes, 2u);
}

static const TeX_model_node_t *rule_sturm(notation_build_t *b)
{
    const TeX_model_node_t *x = symbol(b, "x"), *y = symbol(b, "y");
    const TeX_model_node_t *product = pair(b, TeX_NODE_PRODUCT, call(b, "p", x), prime(b, y, 1u));
    return pair(b, TeX_NODE_EQUATION, pair(b, TeX_NODE_SUM, prime(b, product, 1u),
        pair(b, TeX_NODE_PRODUCT, call(b, "q", x), y)),
        pair(b, TeX_NODE_PRODUCT, call(b, "w", x), call(b, "f", x)));
}

static const char plain_none[] = "Apply the selected symbolic rule to the parsed expression tree.";
static const char plain_separable[] = "Isolate y′ = X(x)Y(y), then integrate ∫dy/Y(y) = ∫X(x)dx + C.";
static const char plain_linear[] = "Write u_s + P(s)u = Q(s). Form μ = exp(∫P(s)ds), "
                                   "integrate (μu)_s = μQ, then divide by μ.";
static const char plain_bernoulli[] = "Write y′ + P(x)y = Q(x)y^n and set v = y^(1−n), "
                                      "reducing the equation to a linear one in v.";
static const char plain_homogeneous[] = "Write y′ = F(y/x), set v = y/x, and use y′ = v + xv′ before separating v and x.";
static const char plain_substitution[] = "Identify the affine combination u = ax + by + c, replace u′ by a + by′, "
                                        "and solve the reduced separable equation.";
static const char plain_transformation[] = "Construct the nonsingular linearising coordinates from the parsed coefficients, "
                                           "transform the derivatives, and solve the reduced equation.";
static const char plain_sturm[] = "Normalise the second-order linear equation, construct its self-adjoint or factorised basis, "
                                  "and determine constants from the supplied conditions.";
static const char plain_constant_linear[] = "Form the characteristic polynomial P(m), generate one basis term for each root "
                                            "and multiplicity, then add a verified particular solution for the forcing.";
static const char plain_quadratic[] = "Set p = dy/dx, solve the quadratic relation for p(y), then integrate dx/dy = 1/p(y), "
                                      "retaining every branch.";
static const char plain_exact_derivative[] = "Recognise and integrate the total derivative once, apply the derived "
                                             "logarithmic substitution, then solve the resulting linear recurrence.";
static const char plain_transport[] = "Integrate the characteristic system dx_j/ds = a_j, carry the boundary data "
                                      "along those curves, and integrate the forcing.";
static const char plain_characteristics[] = "Build dx/a = dy/b = du/(f−cu), derive first integrals of that characteristic "
                                            "system, and express the arbitrary data as a function of the invariants.";
static const char plain_stationary[] = "Apply the parsed spatial operator H to the supplied initial state ψ₀. "
                                       "When Hψ₀ = Eψ₀, evolve it by the derived exponential time factor.";
static const char plain_exact_first[] = "Write M(x,y)dx + N(x,y)dy = 0, verify M_y = N_x, "
                                        "construct a potential Φ with Φ_x=M and Φ_y=N, then set Φ=C.";
static const char plain_laplace[] = "Identify the Laplacian in the parsed coordinates and build the harmonic family "
                                    "from conjugate characteristic coordinates.";
static const char plain_bessel[] =
    "For y″ + a*x^m*y = b*x^n, set ν = 1/(m+2), p = (m+2)/2, z = sqrt(a)*x^p/p, and y = sqrt(x)*u(z). "
    "For monomial forcing b != 0, set μ = (2n-m+1)/(m+2) and K = b*(p/sqrt(a))^(μ+1)/p^2. "
    "The transformed equation z^2*u″ + z*u′ + (z^2-ν^2)*u = K*z^(μ+1) has the Lommel particular K*s_(μ,ν)(z).";

static const char title_none[] = "symbolic differential-equation rule";
static const char title_separable[] = "first-order separable rule";
static const char title_linear[] = "first-order linear integrating-factor rule";
static const char title_bernoulli[] = "Bernoulli substitution rule";
static const char title_homogeneous[] = "first-order homogeneous substitution rule";
static const char title_substitution[] = "affine-combination substitution rule";
static const char title_transformation[] = "linear change-of-variables rule";
static const char title_sturm[] = "Sturm–Liouville reduction rule";
static const char title_constant_linear[] = "constant-coefficient characteristic-polynomial rule";
static const char title_quadratic[] = "autonomous derivative-quadratic rule";
static const char title_exact_derivative[] = "exact-derivative linearisation rule";
static const char title_transport[] = "constant-coefficient transport characteristic rule";
static const char title_characteristics[] = "method of characteristics";
static const char title_parameter_linear[] = "parameter-dependent linear integrating-factor rule";
static const char title_stationary[] = "stationary-eigenfunction evolution rule";
static const char title_exact_first[] = "exact first-order differential rule";
static const char title_laplace[] = "Laplace harmonic-function rule";
static const char title_bessel[] = "power-law Bessel reduction rule";

static const rule_description_t rules[] = {
    [DE_SOLVER_NONE]                           = {title_none,             plain_none,             rule_none},
    [DE_SOLVER_SEPARABLE]                      = {title_separable,        plain_separable,        rule_separable},
    [DE_SOLVER_LINEAR]                         = {title_linear,           plain_linear,           rule_linear},
    [DE_SOLVER_BERNOULLI]                      = {title_bernoulli,        plain_bernoulli,        rule_bernoulli},
    [DE_SOLVER_HOMOGENEOUS]                    = {title_homogeneous,      plain_homogeneous,      rule_homogeneous},
    [DE_SOLVER_LINEAR_SUBSTITUTION]            = {title_substitution,     plain_substitution,     rule_substitution},
    [DE_SOLVER_LINEAR_TRANSFORMATION]          = {title_transformation,   plain_transformation,   rule_transformation},
    [DE_SOLVER_STURM_LIOUVILLE]                = {title_sturm,            plain_sturm,            rule_sturm},
    [DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR]    = {title_constant_linear,  plain_constant_linear,  rule_constant_linear},
    [DE_SOLVER_DERIVATIVE_QUADRATIC]           = {title_quadratic,        plain_quadratic,        rule_quadratic},
    [DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION] = {title_exact_derivative, plain_exact_derivative, rule_exact_derivative},
    [DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT] = {title_transport,        plain_transport,        rule_transport},
    [DE_SOLVER_CHARACTERISTICS]                = {title_characteristics,  plain_characteristics,  rule_characteristics},
    [DE_SOLVER_PARAMETER_LINEAR_PDE]           = {title_parameter_linear, plain_linear,           rule_linear},
    [DE_SOLVER_STATIONARY_EIGENFUNCTION]       = {title_stationary,       plain_stationary,       rule_stationary},
    [DE_SOLVER_EXACT_FIRST_ORDER]              = {title_exact_first,      plain_exact_first,      rule_exact_first},
    [DE_SOLVER_LAPLACE]                        = {title_laplace,          plain_laplace,          rule_laplace},
    [DE_SOLVER_POWER_LAW_BESSEL]               = {title_bessel,           plain_bessel,           rule_bessel},
};

static const rule_description_t *description(de_solver_t solver)
{
    size_t index = (unsigned)solver;
    return index < sizeof rules / sizeof *rules && rules[index].formula ? &rules[index] : &rules[DE_SOLVER_NONE];
}

/* Fill absent explanations with the recognised rule and every verified solution branch. */
int de_solve_result_ensure_rule_steps(const diffequ_t *de, diffequ_solve_result_t *result)
{
    if (!result || result->status != DE_SOLVE_STATUS_SOLVED || result->solution_count == 0u ||
        (result->steps && result->steps_TeX))
        return 0;
    if (result->solution_count >= 65533u)
        return -1;
    const rule_description_t *rule = description(result->solver);
    char *problem = de_to_string(de, style_EXPRESSION);
    string_t *plain = problem ? string_sprintf(
        "Recognise the %s.\nParsed equation:\n      %s\nRule:\n      %s\nDerived solution%s:\n",
        rule->name, problem, rule->plain, result->solution_count == 1u ? "" : "s") : NULL;
    notation_build_t *build = plain ? notation_build_new() : NULL;
    size_t count = result->solution_count + 3u;
    const TeX_model_node_t **rows = build ? calloc(count, sizeof *rows) : NULL;
    bool valid = rows != NULL;
    if (valid) {
        rows[0] = pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Rule:"),
                       notation_build_literal(build, TeX_NODE_TEXT, rule->name));
        rows[1] = pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Parsed equation:"),
                       de_notation_problem(build, de));
        rows[2] = pair(build, TeX_NODE_STATEMENTS, notation_build_literal(build, TeX_NODE_TEXT, "Reduction:"),
                       rule->formula(build));
    }
    for (size_t i = 0u; valid && i < result->solution_count; ++i) {
        const equation_t *solution = result->solutions[i];
        string_t *solution_text = equ_to_text(solution, style_UNBOUND);
        valid = solution_text && string_append_format(plain, "      %S%s", solution_text,
                          i + 1u < result->solution_count ? "\n" : "") >= 0;
        string_free(solution_text);
        const TeX_model_node_t *equation = pair(build, TeX_NODE_EQUATION,
            notation_build_expr(build, equ_lhs(solution)), notation_build_expr(build, equ_rhs(solution)));
        rows[i + 3u] = pair(build, TeX_NODE_STATEMENTS,
            notation_build_literal(build, TeX_NODE_TEXT, result->solution_count == 1u ? "Solution:" : "Solution branch:"),
            equation);
    }
    const TeX_model_node_t *root = valid ? notation_build_node(build,
        (TeX_node_view_t){.kind = TeX_NODE_LINES, .child_count = count}, rows) : NULL;
    string_t *display = root ? notation_build_render(build, root) : NULL;
    valid = plain && display && (result->steps || de_solve_result_set_steps(result, string_c_str(plain)) == 0) &&
            (result->steps_TeX || de_solve_result_set_steps_TeX(result, string_c_str(display)) == 0);
    string_free(display); free(rows); notation_build_free(build); string_free(plain); free(problem);
    return valid ? 0 : -1;
}
