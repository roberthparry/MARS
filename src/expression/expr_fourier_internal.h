/**
 * @file expr_fourier_internal.h
 * @brief Private Fourier rule context and construction helpers.
 *
 * Defines temporary-node ownership, accumulated conditions and forward or inverse transform context. Fourier rule
 * units share these helpers to build formulas and retain domain restrictions without leaking intermediate
 * expressions.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef MARS_EXPR_FOURIER_INTERNAL_H
#define MARS_EXPR_FOURIER_INTERNAL_H

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    expr_t **nodes;
    size_t count, capacity;
    expr_t *conditions;
    bool failed;
    bool inverse;
} fourier_context_t;

/** Own a temporary expression in the Fourier context. */
expr_t *expr_fourier_keep(fourier_context_t *c, expr_t *expr);

/* Retain each constructed expression in the Fourier context's temporary-node arena. */
static inline expr_t *ft_neg(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_neg(a)); }

static inline expr_t *ft_exp(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_exp(a)); }

static inline expr_t *ft_sqrt(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_sqrt(a)); }

static inline expr_t *ft_sech(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_sech(a)); }

static inline expr_t *ft_sinh(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_sinh(a)); }

static inline expr_t *ft_cosh(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_cosh(a)); }

static inline expr_t *ft_conj(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_conj(a)); }

static inline expr_t *ft_rect(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_rect(a)); }

static inline expr_t *ft_tri(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_tri(a)); }

static inline expr_t *ft_sinc(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_sinc(a)); }

static inline expr_t *ft_delta(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_delta(a)); }

static inline expr_t *ft_analytic_delta(fourier_context_t *c, const expr_t *a) {
    return expr_fourier_keep(c, expr_analytic_delta(a));
}

static inline expr_t *ft_step(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_step(a)); }

static inline expr_t *ft_principal_value(fourier_context_t *c, const expr_t *a) {
    return expr_fourier_keep(c, expr_principal_value(a));
}

static inline expr_t *ft_finite_part(fourier_context_t *c, const expr_t *a) {
    return expr_fourier_keep(c, expr_finite_part(a));
}

static inline expr_t *ft_ln(fourier_context_t *c, const expr_t *a) { return expr_fourier_keep(c, expr_ln(a)); }

static inline expr_t *ft_add(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_add(a, b));
}

static inline expr_t *ft_sub(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_sub(a, b));
}

static inline expr_t *ft_mul(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_mul(a, b));
}

static inline expr_t *ft_div(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_div(a, b));
}

static inline expr_t *ft_pow_xp(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_pow_xp(a, b));
}

static inline expr_t *ft_chebyshev_t(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_chebyshev_t(a, b));
}

static inline expr_t *ft_hermite_h(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_hermite_h(a, b));
}

static inline expr_t *ft_beta(fourier_context_t *c, const expr_t *a, const expr_t *b) {
    return expr_fourier_keep(c, expr_beta(a, b));
}

static inline expr_t *integer(fourier_context_t *c, long n) { return expr_fourier_keep(c, expr_const_long(n)); }
static inline expr_t *constant(fourier_context_t *c, number_t n) { return expr_fourier_keep(c, expr_new_const(n)); }
static inline expr_t *pi_constant(fourier_context_t *c) {
    return expr_fourier_keep(c, expr_new_named_const(NUM_PI, "@pi"));
}

/** Construct an exact Euler–Mascheroni constant with its symbolic provenance. */
expr_t *expr_fourier_euler_constant(fourier_context_t *c);
static inline expr_t *clean(fourier_context_t *c, const expr_t *e) { return expr_fourier_keep(c, expr_simplify(e)); }

/** Clone an expression and recover exact literal coefficient provenance. */
expr_t *expr_fourier_exact_literals(fourier_context_t *c, const expr_t *f);

/** Match a symbolic or fixed-exponent power without changing its base. */
bool expr_fourier_match_power(fourier_context_t *c, const expr_t *f, const expr_t **base, const expr_t **power);

/** Determine whether an expression depends on the source coordinate. */
bool expr_fourier_uses(const expr_t *e, const expr_t *x);

/** Construct an absolute value without substituting variable bindings. */
expr_t *expr_fourier_abs(fourier_context_t *c, const expr_t *e);

/** Create a temporary coordinate that cannot capture existing bindings. */
expr_t *expr_fourier_fresh_variable(fourier_context_t *c, const expr_t *f, const expr_t *target);

/** Find an absolute-value node containing the source coordinate. */
const expr_t *expr_fourier_absolute_source(const expr_t *f, const expr_t *x);

/** Evaluate only expressions with no parameter or variable bindings. */
bool expr_fourier_literal_value(const expr_t *e, number_t *value);

/** Check or retain a strictly positive real-part condition. */
bool expr_fourier_positive(fourier_context_t *c, const expr_t *value);

/** Check or retain a real-valued parameter condition. */
bool expr_fourier_real_parameter(fourier_context_t *c, const expr_t *value);

static inline expr_t *replace(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *value) {
    return clean(c, expr_fourier_keep(c, expr_substitute(f, x, value)));
}

/** Collect affine coefficients structurally without sampling free parameters. */
bool expr_fourier_affine(fourier_context_t *c, const expr_t *f, const expr_t *x, expr_t **a, expr_t **b);

/** Match exponential notation or a power of Euler's constant. */
const expr_t *expr_fourier_exponent(const expr_t *f);

/** Match finite exponential spectra and complex evaluation functionals before normalisation. */
expr_t *expr_fourier_analytic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Match a complete hyperbolic beta spectrum, returning an arena-owned unnormalised Fourier result. */
expr_t *expr_fourier_beta_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise a condition excluding exactly the ordinary representative's periodic poles. */
bool expr_fourier_periodic_pole_condition(const expr_t *body, const expr_t *condition);

/** Match periodic functions and their impulse series, before inverse normalisation. */
expr_t *expr_fourier_periodic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Match real affine odd hyperbolic Fourier pairs, before inverse normalisation. */
expr_t *expr_fourier_odd_hyperbolic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise the affine pole exclusion of a cosech or coth spectrum. */
bool expr_fourier_odd_hyperbolic_pole_condition(fourier_context_t *c, const expr_t *f,
                                              const expr_t *x, const expr_t *condition);

/** Describe the singular-integral interpretation of a recognised odd hyperbolic pair. */
const char *expr_fourier_odd_hyperbolic_note(const expr_t *transform);

/** Match locally integrable fractional absolute powers and their dual powers. */
expr_t *expr_fourier_absolute_power_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise only the matched absolute power's own excluded singular point. */
bool expr_fourier_absolute_power_pole_condition(fourier_context_t *c, const expr_t *f,
                                                const expr_t *x, const expr_t *condition);

/** Match real affine sign functions and their modulated reciprocal spectra. */
expr_t *expr_fourier_sgn_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise only the zero-frequency exclusion of a sign-function reciprocal spectrum. */
bool expr_fourier_sgn_pole_condition(fourier_context_t *c, const expr_t *f,
                                    const expr_t *x, const expr_t *condition);

/** Describe the symmetric-cancellation convention for the sign-function Fourier pair. */
const char *expr_fourier_sgn_note(const expr_t *transform);

/** Match real affine arctangents and their exponentially damped reciprocal spectra. */
expr_t *expr_fourier_atan_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Match real affine inverse hyperbolic sines and their modified-Bessel reciprocal spectra. */
expr_t *expr_fourier_asinh_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise precisely the zero-frequency exclusion of an inverse-hyperbolic-sine spectrum. */
bool expr_fourier_asinh_pole_condition(fourier_context_t *c, const expr_t *f,
                                      const expr_t *x, const expr_t *condition);

/** Recognise the pole exclusion of a matched arctangent spectrum. */
bool expr_fourier_atan_pole_condition(fourier_context_t *c, const expr_t *f,
                                     const expr_t *x, const expr_t *condition);

/** Describe the distributional interpretation of a recognised atan or asinh transform. */
const char *expr_fourier_atan_note(const expr_t *transform);

/** Match inverse trigonometric and hyperbolic boundary-value pairs, before inverse normalisation. */
expr_t *expr_fourier_branch_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise precisely the real endpoint exclusions of an atanh argument. */
bool expr_fourier_branch_pole_condition(fourier_context_t *c, const expr_t *f, const expr_t *condition);

/** Match vertical-line gamma transforms and their exponential spectra. */
expr_t *expr_fourier_gamma_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recombine Cartesian components in recovered gamma arguments after a vertical-line inverse. */
expr_t *expr_fourier_gamma_cartesian_result(fourier_context_t *c, const expr_t *result);

/** Explain non-existence for a real affine gamma argument. */
const char *expr_fourier_gamma_note(const expr_t *transform);

#endif
