#include "expr_fourier_internal.h"

/* MARS's real-axis asin and atanh values have positive imaginary parts on both tails;
 * acos = pi/2 - asin and acosh = i acos on this same real-axis boundary. */
typedef expr_t *(*branch_function_t)(const expr_t *);
static const branch_function_t branch_functions[EXPR_KIND_COUNT] = {
    [EXPR_KIND_ASIN]  = expr_asin,
    [EXPR_KIND_ACOS]  = expr_acos,
    [EXPR_KIND_ACOSH] = expr_acosh,
    [EXPR_KIND_ATANH] = expr_atanh,
};

/* D(step(u) ln|u|) is a distributional derivative of a locally integrable primitive.
 * Retaining the derivative avoids the undefined product delta(u) ln|u|. */
bool expr_is_one_sided_log(const expr_t *f)
{
    if (!f || f->ops != &ops_mul)
        return false;
    const expr_t *step = f->a->ops == &ops_step ? f->a : f->b;
    const expr_t *log = step == f->a ? f->b : f->a;
    if (step->ops != &ops_step || log->ops != &ops_log || log->a->ops != &ops_abs)
        return false;
    expr_t *difference = expr_sub_simplify_owned(expr_clone(step->a), expr_clone(log->a->a));
    expr_t *sum = expr_add_simplify_owned(expr_clone(step->a), expr_clone(log->a->a));
    bool same = expr_const_is_zero(difference) || expr_const_is_zero(sum);
    expr_free(sum);
    expr_free(difference);
    return same;
}

static expr_t *branch_spectrum(fourier_context_t *c, expr_op_kind_t kind, const expr_t *rate,
                               const expr_t *offset, const expr_t *w)
{
    expr_t *pi = pi_constant(c), *i = constant(c, NUM_I), *two_pi = ft_mul(c, integer(c, 2), pi);
    expr_t *signed_rate = c->inverse ? ft_neg(c, rate) : (expr_t *)rate;
    expr_t *q = clean(c, ft_div(c, w, signed_rate));
    expr_t *phase = ft_exp(c, ft_mul(c, ft_mul(c, i, q), offset));
    expr_t *impulse = ft_delta(c, w);
    expr_t *out;
    if (kind == EXPR_KIND_ATANH) {
        expr_t *kernel = ft_mul(c, ft_step(c, q), ft_sinc(c, ft_div(c, q, pi)));
        out = ft_sub(c, ft_mul(c, ft_mul(c, i, ft_mul(c, pi, pi)), impulse),
                     ft_div(c, ft_mul(c, ft_mul(c, i, two_pi), ft_mul(c, phase, kernel)), ft_abs(c, rate)));
    } else {
        /* Use a unit cutoff in the frequency coordinate. Rescaling its logarithmic primitive
         * contributes ln|rate| to the impulse; omitting it would change the inverse by a constant. */
        expr_t *reciprocal = ft_finite_part(c, ft_div(c, ft_step(c, q), w));
        expr_t *kernel = ft_mul(c, keep(c, expr_bessel_j(integer(c, 0), q)), reciprocal);
        expr_t *gamma = euler_constant(c);
        expr_t *correction = ft_sub(c, ft_ln(c, ft_mul(c, integer(c, 2), ft_abs(c, rate))), gamma);
        expr_t *regularised = ft_sub(c, ft_mul(c, correction, impulse),
                                     ft_mul(c, ft_div(c, signed_rate, ft_abs(c, rate)), ft_mul(c, phase, kernel)));
        expr_t *cosine_impulse = ft_mul(c, ft_mul(c, pi, pi), impulse);
        if (kind == EXPR_KIND_ACOSH) {
            /* acosh = i acos: cancel i*i before constructing the spectral sum. */
            out = ft_add(c, ft_mul(c, i, cosine_impulse), ft_mul(c, two_pi, regularised));
        } else {
            out = ft_mul(c, ft_mul(c, i, two_pi), regularised);
            if (kind == EXPR_KIND_ACOS)
                out = ft_sub(c, cosine_impulse, out);
        }
    }
    return clean(c, out);
}

/* These quotients share the unit-cutoff convention, including independently entered spectra.
 * Unwrap only this known kernel for matching; arbitrary finite parts retain their operators. */
static void canonical_kernels(fourier_context_t *c, expr_t **node, const expr_t *x)
{
    expr_t *f = *node;
    if (!f)
        return;
    if (expr_is_half_line_finite_part(f)) {
        *node = expr_clone(f->a);
        expr_free(f);
        f = *node;
    }
    if (f->ops == &ops_formal_derivative && f->formal_wrt_count == 1u &&
        expr_struct_eq(f->formal_wrts[0], x) && expr_is_one_sided_log(f->a)) {
        const expr_t *step = f->a->a->ops == &ops_step ? f->a->a : f->a->b;
        expr_t *rate = NULL, *offset = NULL;
        if (affine(c, step->a, x, &rate, &offset) && expr_const_is_zero(offset) && !expr_const_is_zero(rate)) {
            expr_t *correction = ft_mul(c, ft_div(c, rate, ft_abs(c, rate)), ft_ln(c, ft_abs(c, rate)));
            expr_t *value = clean(c, ft_add(c, ft_div(c, step, x), ft_mul(c, correction, ft_delta(c, x))));
            *node = expr_clone(value);
            expr_free(f);
            f = *node;
        }
    }
    canonical_kernels(c, &f->a, x);
    canonical_kernels(c, &f->b, x);
    f->simplified = false;
    f->simplify_epoch = 0u;
}

/* Find a family marker in an expression tree, never by its displayed spelling. */
static const expr_t *branch_marker(const expr_t *f)
{
    if (!f)
        return NULL;
    if (f->ops == &ops_sinc || (f->ops == &ops_bessel_j && expr_const_is_zero(f->a)))
        return f;
    const expr_t *left = branch_marker(f->a);
    return left ? left : branch_marker(f->b);
}

static const expr_t *phase_marker(const expr_t *f, const expr_t *x)
{
    if (!f)
        return NULL;
    if (exponent(f) && uses(f, x))
        return f;
    const expr_t *left = phase_marker(f->a, x);
    return left ? left : phase_marker(f->b, x);
}

static const expr_t *step_coordinate(const expr_t *f)
{
    if (!f)
        return NULL;
    if (f->ops == &ops_step)
        return f->a;
    const expr_t *left = step_coordinate(f->a);
    return left ? left : step_coordinate(f->b);
}

/* Canonicalise equivalent affine arguments before comparing signal factors: -x/2 and -(1/2)x agree. */
static void canonical_arguments(fourier_context_t *c, expr_t *f, const expr_t *x)
{
    if (!f)
        return;
    canonical_arguments(c, f->a, x);
    canonical_arguments(c, f->b, x);
    expr_t **argument = f->ops == &ops_bessel_j ? &f->b :
                        (f->ops == &ops_step || f->ops == &ops_sinc || f->ops == &ops_abs || f->ops == &ops_exp)
                            ? &f->a : NULL;
    expr_t *rate = NULL, *offset = NULL;
    if (argument && affine(c, *argument, x, &rate, &offset)) {
        expr_t *replacement = clean(c, ft_add(c, ft_mul(c, rate, x), offset));
        expr_free(*argument);
        *argument = expr_clone(replacement);
    }
    f->simplified = false;
    f->simplify_epoch = 0u;
}

static void replace_impulses(expr_t **node, const expr_t *x, const expr_t *replacement)
{
    expr_t *f = *node;
    if (!f)
        return;
    if (f->ops == &ops_delta && expr_struct_eq(f->a, x)) {
        *node = expr_clone(replacement);
        expr_free(f);
        return;
    }
    replace_impulses(&f->a, x, replacement);
    replace_impulses(&f->b, x, replacement);
    f->simplified = false;
    f->simplify_epoch = 0u;
}

static bool smooth_impulse_coefficient(fourier_context_t *c, const expr_t *f, const expr_t *x)
{
    if (!uses(f, x))
        return true;
    if (f->ops == &ops_add || f->ops == &ops_sub || f->ops == &ops_mul)
        return smooth_impulse_coefficient(c, f->a, x) && smooth_impulse_coefficient(c, f->b, x);
    if (f->ops == &ops_div)
        return !uses(f->b, x) && smooth_impulse_coefficient(c, f->a, x);
    if (f->ops == &ops_neg)
        return smooth_impulse_coefficient(c, f->a, x);
    const expr_t *argument = f->ops == &ops_exp ? f->a :
                            (f->ops == &ops_bessel_j && expr_const_is_zero(f->a) ? f->b : NULL);
    expr_t *rate = NULL, *offset = NULL;
    return argument && affine(c, argument, x, &rate, &offset);
}

/* The rescaled logarithmic primitive introduces an impulse multiplied by entire Bessel/phase
 * factors. Evaluate those factors at the impulse support before extracting the family marker. */
static expr_t *canonical_impulses(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *dummy = fresh_variable(c, f, w);
    expr_t *polynomial = expr_clone(f);
    replace_impulses(&polynomial, x, dummy);
    keep(c, polynomial);
    expr_t *coefficient = NULL, *remainder = NULL;
    if (!affine(c, clean(c, polynomial), dummy, &coefficient, &remainder) || !uses(coefficient, x) ||
        !smooth_impulse_coefficient(c, coefficient, x))
        return (expr_t *)f;
    expr_t *at_zero = clean(c, replace(c, coefficient, x, integer(c, 0)));
    if (uses(at_zero, x) || (expr_is_const(at_zero) && !num_is_finite(at_zero->c)))
        return (expr_t *)f;
    return clean(c, ft_add(c, remainder, ft_mul(c, at_zero, ft_delta(c, x))));
}

/* Match an independently supplied spectrum, not a cached forward-transform provenance tag. */
static expr_t *branch_inverse(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    const expr_t *marker = branch_marker(f);
    if (!marker)
        return NULL;
    expr_t *canonical = expr_clone(f);
    canonical_kernels(c, &canonical, x);
    keep(c, canonical);
    canonical_arguments(c, canonical, x);
    f = canonical_impulses(c, clean(c, canonical), x, w);
    marker = branch_marker(f);
    if (!marker)
        return NULL;
    expr_op_kind_t kind = marker->ops == &ops_sinc ? EXPR_KIND_ATANH : EXPR_KIND_ASIN;
    /* sinc and J0 are even, so only the step factor retains the orientation of a negative scale. */
    const expr_t *coordinate = step_coordinate(f);
    if (!coordinate)
        return NULL;
    expr_t *slope = NULL, *shift = NULL;
    if (!affine(c, clean(c, coordinate), x, &slope, &shift) || !expr_const_is_zero(shift) ||
        expr_const_is_zero(slope))
        return NULL;
    expr_t *rate = clean(c, ft_div(c, integer(c, 1), slope));
    expr_t *offset = integer(c, 0);
    const expr_t *phase = phase_marker(f, x);
    if (phase) {
        expr_t *phase_slope = NULL, *phase_shift = NULL;
        if (!affine(c, exponent(phase), x, &phase_slope, &phase_shift) || !expr_const_is_zero(phase_shift))
            return NULL;
        offset = clean(c, ft_neg(c, ft_mul(c, constant(c, NUM_I), ft_mul(c, phase_slope, rate))));
    }
    bool inverse = c->inverse;
    c->inverse = false;
    expr_t *expected = branch_spectrum(c, kind, rate, offset, x);
    c->inverse = inverse;
    expr_t *canonical_expected = expr_clone(expected);
    canonical_kernels(c, &canonical_expected, x);
    keep(c, canonical_expected);
    canonical_arguments(c, canonical_expected, x);
    expected = clean(c, canonical_expected);
    const expr_t *expected_marker = branch_marker(expected);
    if (!expected_marker)
        return NULL;
    const expr_t *actual_argument = kind == EXPR_KIND_ATANH ? marker->a : marker->b;
    const expr_t *expected_argument = kind == EXPR_KIND_ATANH ? expected_marker->a : expected_marker->b;
    if (!expr_const_is_zero(clean(c, ft_sub(c, actual_argument, expected_argument))) &&
        !expr_const_is_zero(clean(c, ft_add(c, actual_argument, expected_argument))))
        return NULL;
    expr_t *dummy = fresh_variable(c, f, w);
    expr_t *coefficient = NULL, *remainder = NULL, *expected_coefficient = NULL, *expected_remainder = NULL;
    if (!affine(c, replace(c, f, marker, dummy), dummy, &coefficient, &remainder) ||
        !affine(c, replace(c, expected, expected_marker, dummy), dummy, &expected_coefficient, &expected_remainder))
        return NULL;
    /* Every branch spectrum has an imaginary prefactor. Cancel it before forming the quotient,
     * rather than leaving i/(2 i pi) for later additive cancellation to discover. */
    expr_t *minus_i = constant(c, NUM_NEG_I);
    expr_t *scale = clean(c, ft_div(c, clean(c, ft_mul(c, minus_i, coefficient)),
                                   clean(c, ft_mul(c, minus_i, expected_coefficient))));
    if (uses(scale, x))
        return NULL;
    expr_t *residual = clean(c, ft_sub(c, remainder, ft_mul(c, scale, expected_remainder)));
    expr_t *constant_part = NULL, *other = NULL;
    expr_t *delta_polynomial = expr_clone(residual);
    replace_impulses(&delta_polynomial, x, dummy);
    keep(c, delta_polynomial);
    if (!affine(c, clean(c, delta_polynomial), dummy, &constant_part, &other) ||
        uses(constant_part, x) || !expr_const_is_zero(other) ||
        !real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, ft_abs(c, rate)))
        return NULL;
    expr_t *argument = clean(c, ft_add(c, ft_mul(c, rate, inverse ? w : ft_neg(c, w)), offset));
    expr_t *cosine_constant = clean(c, exact_literals(c, clean(c, ft_add(c, constant_part,
                                     ft_mul(c, scale, ft_mul(c, pi_constant(c), pi_constant(c)))))));
    if (kind == EXPR_KIND_ASIN && expr_const_is_zero(cosine_constant)) {
        kind = EXPR_KIND_ACOS;
        scale = ft_neg(c, scale);
        constant_part = integer(c, 0);
        /* Prefer the real multiple of acosh to an imaginary multiple of acos. This also
         * recognises inverse-first spectra, whose scalar includes the Fourier normalisation. */
        expr_t *hyperbolic_scale = clean(c, ft_mul(c, minus_i, scale));
        number_t value = NUM_ZERO;
        if (literal_value(hyperbolic_scale, &value) && num_is_real(value) && num_is_finite(value)) {
            kind = EXPR_KIND_ACOSH;
            scale = hyperbolic_scale;
        }
    }
    expr_t *body = keep(c, branch_functions[kind](argument));
    if (kind == EXPR_KIND_ATANH &&
        !positive(c, ft_abs(c, ft_sub(c, integer(c, 1), ft_mul(c, argument, argument)))))
        return NULL;
    return ft_add(c, ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, scale, body)), constant_part);
}

/* The caller supplies inverse normalisation once, after the shared kernel has been reflected. */
expr_t *expr_fourier_branch_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    branch_function_t function = branch_functions[f->ops->kind];
    if (!function)
        return branch_inverse(c, f, x, w);
    expr_t *rate = NULL, *offset = NULL;
    if (!affine(c, f->a, x, &rate, &offset))
        return NULL;
    if (expr_const_is_zero(rate)) {
        expr_t *body = keep(c, function(offset));
        return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, body, ft_delta(c, w)));
    }
    if (!real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, ft_abs(c, rate)))
        return NULL;
    return branch_spectrum(c, f->ops->kind, rate, offset, w);
}

/* Endpoint singularities are locally integrable; unrelated source restrictions must remain attached. */
bool expr_fourier_branch_pole_condition(fourier_context_t *c, const expr_t *f, const expr_t *condition)
{
    if (f->ops != &ops_atanh || condition->ops != &ops_abs)
        return false;
    expr_t *endpoints = ft_sub(c, integer(c, 1), ft_mul(c, f->a, f->a));
    return expr_const_is_zero(clean(c, ft_sub(c, condition->a, endpoints))) ||
           expr_const_is_zero(clean(c, ft_add(c, condition->a, endpoints)));
}
