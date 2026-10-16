/**
 * @file expr_fourier_folded.c
 * @brief Exact impulse spectra of periodic inverse-circular compositions.
 *
 * Expands acos(cos(a*x+b)) into its absolutely convergent odd-harmonic
 * Fourier series. Related sine and asin folds share this representation.
 * The forward transform is a tempered distribution, not a pointwise function.
 */
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_fourier_internal.h"

/* Retain every intermediate in the owning Fourier context. */
expr_t *expr_fourier_folded_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *a = NULL, *b = NULL;
    if (c->inverse || !expr_fourier_affine(c, f->a->a, x, &a, &b) || expr_const_is_zero(a) ||
        !expr_fourier_real_parameter(c, a) || !expr_fourier_real_parameter(c, b) ||
        !expr_fourier_positive(c, expr_fourier_abs(c, a)))
        return NULL;
    expr_t *pi = pi_constant(c), *two = integer(c, 2);
    if (f->a->ops == &ops_sin)
        b = clean(c, ft_sub(c, b, ft_div(c, pi, two)));
    number_t rate = NUM_NAN;
    if (expr_fourier_literal_value(a, &rate) && num_lt(rate, NUM_ZERO)) {
        a = clean(c, ft_neg(c, a));
        b = clean(c, ft_neg(c, b));
    }
    num_destroy(&rate);
    expr_bindings_t *source = expr_bindings_from_expr_internal(f);
    expr_bindings_t *target = expr_bindings_from_expr_internal(w);
    bool available = !expr_bindings_get(source, "n") && !expr_bindings_get(target, "n");
    expr_bindings_free(source);
    expr_bindings_free(target);
    expr_t *n = available ? expr_fourier_keep(c, expr_new_named_var(NUM_NAN, "n")) :
                           expr_fourier_fresh_variable(c, f, w);
    expr_t *k = ft_add(c, ft_mul(c, two, n), integer(c, 1));
    expr_t *frequency = clean(c, ft_mul(c, k, a));
    expr_t *phase = ft_mul(c, constant(c, NUM_I), ft_mul(c, k, b));
    expr_t *positive = ft_mul(c, ft_exp(c, phase), ft_delta(c, ft_sub(c, w, frequency)));
    expr_t *negative = ft_mul(c, ft_exp(c, ft_neg(c, phase)), ft_delta(c, ft_add(c, w, frequency)));
    expr_t *numerator = ft_add(c, positive, negative);
    expr_t *phase_factor = integer(c, 1);
    expr_t *quarter_turns = clean(c, ft_div(c, ft_mul(c, two, b), pi));
    if (expr_is_const(quarter_turns) && num_is_real(quarter_turns->c) && num_is_integer(quarter_turns->c)) {
        number_t four = num_create_from_long(4), turn = num_mod(quarter_turns->c, four);
        if (num_lt(turn, NUM_ZERO)) {
            number_t adjusted = num_add(turn, four);
            num_destroy(&turn);
            turn = adjusted;
        }
        expr_t *left = ft_delta(c, ft_sub(c, w, frequency)), *right = ft_delta(c, ft_add(c, w, frequency));
        if (num_is_zero(turn) || num_eq(turn, NUM_TWO)) {
            numerator = ft_add(c, left, right);
            phase_factor = integer(c, num_is_zero(turn) ? 1 : -1);
        } else {
            numerator = ft_mul(c, ft_pow_xp(c, integer(c, -1), n), ft_sub(c, left, right));
            phase_factor = num_eq(turn, NUM_ONE) ? constant(c, NUM_I) : ft_neg(c, constant(c, NUM_I));
        }
        num_destroy(&turn);
        num_destroy(&four);
    }
    expr_t *term = ft_div(c, numerator, ft_pow_xp(c, k, two));
    expr_t *sum = expr_fourier_keep(c, expr_new_finite_summation_range(term, n, integer(c, 0), constant(c, NUM_INF)));
    expr_t *harmonics = ft_mul(c, ft_mul(c, integer(c, 4), phase_factor), sum);
    return f->ops == &ops_asin ? harmonics :
        ft_sub(c, ft_mul(c, ft_mul(c, pi, pi), ft_delta(c, w)), harmonics);
}
