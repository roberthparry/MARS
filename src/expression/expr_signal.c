#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <stdio.h>

typedef number_t (*signal_numeric_fn)(number_t);
static const signal_numeric_fn signal_numeric[EXPR_KIND_COUNT] = {
    [EXPR_KIND_SGN]  = num_sgn,
    [EXPR_KIND_STEP] = num_step,
    [EXPR_KIND_RECT] = num_rect,
    [EXPR_KIND_TRI]  = num_tri,
    [EXPR_KIND_CIRC] = num_circ,
    [EXPR_KIND_SINC] = num_sinc,
};

/* The regular restriction of a real Dirac distribution vanishes off its support. */
static number_t delta_regular_value(number_t argument)
{
    return num_clone(num_is_real(argument) && num_is_finite(argument) && !num_is_zero(argument) ? NUM_ZERO : NUM_NAN);
}

/* Regularisation changes the singularity, not finite values away from it. */
static number_t distribution_regular_value(number_t argument)
{
    return num_clone(num_is_finite(argument) ? argument : NUM_NAN);
}

/* Evaluation only: do not let numerical bindings erase distribution nodes during simplification. */
static const signal_numeric_fn distribution_numeric[EXPR_KIND_COUNT] = {
    [EXPR_KIND_DELTA]           = delta_regular_value,
    [EXPR_KIND_PRINCIPAL_VALUE] = distribution_regular_value,
    [EXPR_KIND_FINITE_PART]     = distribution_regular_value,
};

static number_t signal_eval(expr_t *expr)
{
    number_t x = expr_eval(expr->a);
    signal_numeric_fn evaluate = signal_numeric[expr->ops->kind];
    if (!evaluate)
        evaluate = distribution_numeric[expr->ops->kind];
    number_t out = evaluate ? evaluate(x) : num_clone(NUM_NAN);
    num_destroy(&x);
    return out;
}

/* Differentiate only the regular restriction, retaining the original distributional derivative tree. */
number_t expr_distribution_derivative_eval(expr_t *expr)
{
    const expr_t *source = expr->a;
    if (!source || !distribution_numeric[source->ops->kind])
        return num_clone(NUM_NAN);
    number_t value = expr_eval(source);
    bool regular = num_is_finite(value);
    num_destroy(&value);
    if (!regular)
        return num_clone(NUM_NAN);
    if (source->ops == &ops_delta)
        return num_clone(NUM_ZERO);
    expr_t *body = expr_clone(source->a);
    for (size_t index = 0u; body && index < expr->formal_wrt_count; ++index) {
        expr_t *derivative = expr_create_deriv(body, expr->formal_wrts[index]);
        expr_free(body);
        body = derivative;
    }
    value = body ? expr_eval(body) : num_clone(NUM_NAN);
    expr_free(body);
    if (num_is_finite(value))
        return value;
    num_destroy(&value);
    return num_clone(NUM_NAN);
}

static expr_t *signal_simplify(const expr_t *expr, expr_t *a, expr_t *b)
{
    if (expr->ops == &ops_analytic_delta)
        return expr_simplify_passthrough(expr, a, b);
    if (a && expr->ops == &ops_sgn) {
        expr_t *positive = expr_simplify_positive_part_if_negative(a);
        if (positive) {
            expr_t *sign = expr_sgn(positive);
            expr_t *out = sign ? expr_neg(sign) : NULL;
            expr_free(sign);
            expr_free(positive);
            if (out) {
                expr_free(a);
                expr_free(b);
                return out;
            }
        }
    }
    if (a && expr->ops != &ops_sgn && expr->ops != &ops_step &&
        expr->ops != &ops_principal_value && expr->ops != &ops_finite_part) {
        expr_t *positive = expr_simplify_positive_part_if_negative(a);
        if (positive) {
            expr_free(a);
            a = positive;
        }
    }
    signal_numeric_fn evaluate = signal_numeric[expr->ops->kind];
    if (evaluate && expr_is_const(a) && !a->name) {
        number_t value = evaluate(a->c);
        if (!num_is_nan(value)) {
            expr_t *out = expr_new_const(value);
            num_destroy(&value);
            expr_free(a);
            expr_free(b);
            return out;
        }
        num_destroy(&value);
    }
    return expr_simplify_passthrough(expr, a, b);
}

/* Entire hypergeometric forms keep sinc derivatives and primitives regular at zero. */
static expr_t *sinc_entire(const expr_t *x, bool primitive)
{
    expr_t *pi = expr_new_const(NUM_PI);
    expr_t *px = expr_mul(pi, x);
    expr_t *square = expr_mul(px, px);
    expr_t *quarter = expr_new_const(NUM_QUARTER);
    expr_t *minus_quarter = expr_neg(quarter);
    expr_t *z = expr_mul(minus_quarter, square);
    expr_t *half = expr_new_const(NUM_HALF);
    expr_t *one = expr_const_one();
    expr_t *three_halves = expr_add(one, half);
    const expr_t *upper[] = {half};
    const expr_t *lower[] = {three_halves, three_halves};
    expr_t *h = expr_hypergeometric_pFq(primitive ? 1u : 0u, upper, primitive ? 2u : 1u, lower, z);
    expr_t *out = primitive ? expr_mul(x, h) : expr_clone(h);
    expr_free(h);
    expr_free(three_halves);
    expr_free(one);
    expr_free(half);
    expr_free(z);
    expr_free(minus_quarter);
    expr_free(quarter);
    expr_free(square);
    expr_free(px);
    expr_free(pi);
    return out;
}

static expr_t *signal_deriv(expr_t *expr)
{
    const expr_t *wrt = expr_current_wrt_internal();
    if (!wrt)
        return NULL;
    if (expr->ops == &ops_delta || expr->ops == &ops_analytic_delta ||
        expr->ops == &ops_principal_value || expr->ops == &ops_finite_part) {
        expr_t *variable = (expr_t *)wrt;
        return expr_new_formal_derivative(expr, 1u, &variable);
    }
    if (expr->ops == &ops_sinc) {
        expr_t *entire = sinc_entire(expr->a, false);
        expr_t *out = entire ? expr_create_deriv(entire, wrt) : NULL;
        expr_free(entire);
        return out;
    }
    expr_t *chain = expr_create_deriv(expr->a, wrt);
    expr_t *local = NULL;
    if (expr->ops == &ops_sgn) {
        expr_t *delta = expr_delta(expr->a);
        local = delta ? expr_mul_long(delta, 2L) : NULL;
        expr_free(delta);
    } else if (expr->ops == &ops_step) {
        local = expr_delta(expr->a);
    } else {
        expr_t *radius = expr_new_const(expr->ops == &ops_rect ? NUM_HALF : NUM_ONE);
        expr_t *left = expr_add(expr->a, radius);
        expr_t *right = expr_sub(expr->a, radius);
        expr_t *a = expr->ops == &ops_tri ? expr_step(left) : expr_delta(left);
        expr_t *b = expr->ops == &ops_tri ? expr_step(right) : expr_delta(right);
        local = expr->ops == &ops_tri ? expr_add(a, b) : expr_sub(a, b);
        if (expr->ops == &ops_tri) {
            expr_t *centre = expr_step(expr->a);
            expr_t *two = expr_const_long(2);
            expr_t *twice = expr_mul(two, centre);
            expr_t *updated = expr_sub(local, twice);
            expr_free(local);
            local = updated;
            expr_free(twice);
            expr_free(two);
            expr_free(centre);
        }
        expr_free(b);
        expr_free(a);
        expr_free(right);
        expr_free(left);
        expr_free(radius);
    }
    expr_t *out = chain && local ? expr_mul(local, chain) : NULL;
    expr_free(local);
    expr_free(chain);
    return out;
}

static expr_t *ramp(const expr_t *x, bool squared)
{
    expr_t *step = expr_step(x);
    expr_t *power = squared ? expr_mul(x, x) : expr_clone(x);
    expr_t *out = expr_mul(power, step);
    expr_free(power);
    expr_free(step);
    return out;
}

/* Match fixed real coefficients, never infer a domain from mutable parameter bindings. */
static bool sgn_real_affine(const expr_t *argument, const expr_t *wrt, number_t *offset, number_t *slope)
{
    number_t coefficients[5] = {NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ZERO, NUM_ZERO};
    bool matched = argument && wrt && expr_collect_poly_deg4(argument, wrt, coefficients) &&
                   num_is_zero(coefficients[2]) && num_is_zero(coefficients[3]) && num_is_zero(coefficients[4]) &&
                   num_is_real(coefficients[0]) && num_is_finite(coefficients[0]) &&
                   num_is_real(coefficients[1]) && num_is_finite(coefficients[1]);

    if (matched) {
        num_destroy(offset);
        num_destroy(slope);
        *offset = num_clone(coefficients[0]);
        *slope = num_clone(coefficients[1]);
    }
    for (size_t i = 0u; i < 5u; ++i)
        num_destroy(&coefficients[i]);
    return matched;
}

/* Count signs on an integer interval without scanning it or specialising live bound variables. */
expr_t *expr_sgn_sum_closed_form(const expr_t *expr)
{
    if (!expr_is_op(expr, &ops_summation) || !expr_is_op(expr->a, &ops_sgn) ||
        !expr_is_op(expr->b, &ops_argument_list))
        return NULL;
    const expr_t *index = expr->b->a;
    const expr_t *upper = expr->b->b;
    const expr_t *lower = EXPR_ZERO;
    if (expr_is_op(upper, &ops_argument_list)) {
        lower = upper->a;
        upper = upper->b;
    }
    if (!expr_is_var(index) || !expr_is_unnamed_const(lower) || !expr_is_unnamed_const(upper) ||
        !num_is_real(lower->c) || !num_is_finite(lower->c) || !num_is_integer(lower->c) ||
        !num_is_real(upper->c) || !num_is_finite(upper->c) || !num_is_integer(upper->c))
        return NULL;

    NUM_SCOPE(scope);
    number_t offset = num_new();
    number_t slope = num_new();
    if (!sgn_real_affine(expr->a->a, index, &offset, &slope))
        return NULL;
    if (num_lt(upper->c, lower->c))
        return expr_new_const(NUM_ZERO);
    if (num_is_zero(slope)) {
        number_t count = num_add(num_sub(upper->c, lower->c), NUM_ONE);
        return expr_new_const(num_mul(count, num_sgn(offset)));
    }

    /* F(k)=sgn(a)*|k-m| has F(k+1)-F(k)=sgn(a*k+b) on the integers.
     * At an integer root r, m=r+1/2 makes the zero term a plateau; otherwise m=ceil(r). */
    number_t root = num_div(num_neg(offset), slope);
    if (!num_is_finite(root))
        return NULL;
    number_t midpoint = num_mul(NUM_HALF, num_add(num_add(num_floor(root), num_ceil(root)), NUM_ONE));
    number_t first = num_abs(num_sub(lower->c, midpoint));
    number_t last = num_abs(num_sub(num_add(upper->c, NUM_ONE), midpoint));
    return expr_new_const(num_mul(num_sgn(slope), num_sub(last, first)));
}

/* Retain the real domain symbolically, regardless of the variable's current binding. */
static expr_t *sgn_integrate(const expr_t *expr, const expr_t *wrt)
{
    number_t offset = num_new();
    number_t slope = num_new();
    expr_t *out = NULL;

    if (sgn_real_affine(expr->a, wrt, &offset, &slope) && !num_is_zero(slope)) {
        expr_t *absolute = expr_abs(expr->a);
        expr_t *rate = expr_new_const(slope);
        expr_t *primitive = absolute && rate ? expr_div(absolute, rate) : NULL;
        expr_t *real = ops_real_parameter.apply_unary(wrt);
        expr_t *zero = expr_const_zero();
        expr_t *args[] = {primitive, real, zero};

        out = primitive && real && zero ? expr_real_domain_from_args(3u, args) : NULL;
        expr_free(zero);
        expr_free(real);
        expr_free(primitive);
        expr_free(rate);
        expr_free(absolute);
    }
    num_destroy(&slope);
    num_destroy(&offset);
    return out;
}

static expr_t *signal_integrate(const expr_t *expr, const expr_t *wrt)
{
    if (expr->ops == &ops_sgn)
        return sgn_integrate(expr, wrt);
    if (expr->ops == &ops_principal_value || expr->ops == &ops_finite_part || expr->ops == &ops_analytic_delta)
        return NULL;
    expr_t *rate = expr_create_deriv(expr->a, wrt);
    bool used = true;
    expr_t *variable = (expr_t *)wrt;
    if (!rate || !expr_collect_var_usage(rate, 1u, &variable, &used) || used || expr_const_is_zero(rate)) {
        expr_free(rate);
        return NULL;
    }
    expr_t *local = NULL;
    if (expr->ops == &ops_delta)
        local = expr_step(expr->a);
    else if (expr->ops == &ops_step)
        local = ramp(expr->a, false);
    else if (expr->ops == &ops_sinc)
        local = sinc_entire(expr->a, true);
    else {
        bool triangular = expr->ops == &ops_tri;
        expr_t *radius = expr_new_const(expr->ops == &ops_rect ? NUM_HALF : NUM_ONE);
        expr_t *left = expr_add(expr->a, radius);
        expr_t *right = expr_sub(expr->a, radius);
        expr_t *a = ramp(left, triangular);
        expr_t *b = ramp(right, triangular);
        local = triangular ? expr_add(a, b) : expr_sub(a, b);
        if (triangular) {
            expr_t *centre = ramp(expr->a, true);
            expr_t *half = expr_new_const(NUM_HALF);
            expr_t *scaled = expr_mul(half, local);
            expr_t *updated = expr_sub(scaled, centre);
            expr_free(local);
            local = updated;
            expr_free(scaled);
            expr_free(half);
            expr_free(centre);
        }
        expr_free(b);
        expr_free(a);
        expr_free(right);
        expr_free(left);
        expr_free(radius);
    }
    expr_t *out = local ? expr_div(local, rate) : NULL;
    expr_free(local);
    expr_free(rate);
    return out;
}

/* The ordinary derivative is zero away from the jump and undefined on its support. */
static void sgn_reverse(const expr_t *expr, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t local = delta_regular_value(expr_eval_num_internal(expr->a));

    *a_bar = num_mul(*out_bar, local);
    *b_bar = num_clone(NUM_ZERO);
    num_destroy(&local);
}

const expr_ops_t ops_sgn = {
    .eval = signal_eval, .deriv = signal_deriv, .reverse = sgn_reverse,
    .kind = EXPR_KIND_SGN, .arity = EXPR_OP_UNARY,
    .expression_name = "sgn", .function_name = "sgn", .TeX_name = "\\operatorname{sgn}",
    .apply_unary = expr_sgn, .simplify = signal_simplify, .integrate = signal_integrate,
};

#define SIGNAL_OP(name, kind_id, expression, display)                                                                    \
    const expr_ops_t ops_##name = {                                                                                     \
        .eval = signal_eval, .deriv = signal_deriv, .reverse = expr_reverse_not_differentiable,                           \
        .kind = kind_id, .arity = EXPR_OP_UNARY, .expression_name = expression, .function_name = #name,                   \
        .TeX_name = display, .apply_unary = expr_##name, .simplify = signal_simplify, .integrate = signal_integrate,      \
    }

SIGNAL_OP(step, EXPR_KIND_STEP, "step", "\\operatorname{step}");
SIGNAL_OP(rect, EXPR_KIND_RECT, "rect", "\\operatorname{rect}");
SIGNAL_OP(tri, EXPR_KIND_TRI, "tri", "\\operatorname{tri}");
SIGNAL_OP(circ, EXPR_KIND_CIRC, "circ", "\\operatorname{circ}");
SIGNAL_OP(sinc, EXPR_KIND_SINC, "sinc", "\\operatorname{sinc}");
SIGNAL_OP(delta, EXPR_KIND_DELTA, "δ", "\\delta");
SIGNAL_OP(analytic_delta, EXPR_KIND_ANALYTIC_DELTA, "analytic_delta", "\\delta");
SIGNAL_OP(principal_value, EXPR_KIND_PRINCIPAL_VALUE, "principal_value", "\\operatorname{PV}");
SIGNAL_OP(finite_part, EXPR_KIND_FINITE_PART, "finite_part", "\\operatorname{Fp}");
#undef SIGNAL_OP

static expr_t *signal_new(const expr_ops_t *ops, const expr_t *argument)
{
    if (!argument)
        return NULL;
    expr_retain(argument);
    return expr_new_unary_internal(ops, argument);
}

/* Construct the real signum, including its zero value at the origin. */
expr_t *expr_sgn(const expr_t *a) { return signal_new(&ops_sgn, a); }
/* Construct the real unit step with symmetric endpoint convention. */
expr_t *expr_step(const expr_t *a) { return signal_new(&ops_step, a); }
/* Construct the unit-width rectangular pulse. */
expr_t *expr_rect(const expr_t *a) { return signal_new(&ops_rect, a); }
/* Construct the unit-height triangular pulse. */
expr_t *expr_tri(const expr_t *a) { return signal_new(&ops_tri, a); }
/* Construct the even unit-radius aperture profile. */
expr_t *expr_circ(const expr_t *a) { return signal_new(&ops_circ, a); }
/* Construct the entire normalised sinc function. */
expr_t *expr_sinc(const expr_t *a) { return signal_new(&ops_sinc, a); }
/* Construct a Dirac distribution, retaining its support while evaluating to zero elsewhere. */
expr_t *expr_delta(const expr_t *a) { return signal_new(&ops_delta, a); }
/* Construct a complex evaluation functional without assigning it pointwise values. */
expr_t *expr_analytic_delta(const expr_t *a) { return signal_new(&ops_analytic_delta, a); }
/* Preserve the principal-value interpretation through expression operations. */
expr_t *expr_principal_value(const expr_t *a)
{
    return signal_new(&ops_principal_value, a);
}

/* Preserve the unit-cutoff finite-part interpretation, evaluating only its regular restriction. */
expr_t *expr_finite_part(const expr_t *a)
{
    return signal_new(&ops_finite_part, a);
}

/* Half-line reciprocals retain regularisation internally but display the evaluated quotient. */
bool expr_is_half_line_finite_part(const expr_t *expr)
{
    return expr && expr->ops == &ops_finite_part && expr->a && expr->a->ops == &ops_div &&
           expr->a->a && expr->a->a->ops == &ops_step;
}

/* Whole-line and causal convolutions. */

static expr_t *convolution_new(const expr_ops_t *ops, const expr_t *left, const expr_t *right, const expr_t *x)
{
    if (!left || !right || !expr_is_var(x))
        return NULL;
    expr_t *out = expr_alloc(ops);
    out->a = expr_alloc(&ops_argument_list);
    expr_retain(left);
    expr_retain(right);
    expr_retain(x);
    out->a->a = (expr_t *)left;
    out->a->b = (expr_t *)right;
    out->b = (expr_t *)x;
    return out;
}

/* Construct a whole-line convolution, leaving convergence to the operands' transform domains. */
expr_t *expr_convolve(const expr_t *f, const expr_t *g, const expr_t *x)
{
    return convolution_new(&ops_convolution, f, g, x);
}

/* Construct the zero-based causal convolution used by the unilateral Laplace theorem. */
expr_t *expr_causal_convolve(const expr_t *f, const expr_t *g, const expr_t *x)
{
    return convolution_new(&ops_causal_convolution, f, g, x);
}

expr_t *expr_convolution_from_args(size_t count, expr_t *const *args)
{
    return count == 3u ? expr_convolve(args[0], args[1], args[2]) : NULL;
}

expr_t *expr_causal_convolution_from_args(size_t count, expr_t *const *args)
{
    return count == 3u ? expr_causal_convolve(args[0], args[1], args[2]) : NULL;
}

/* Introduce a genuinely fresh dummy, avoiding both capture and ambiguous printed names. */
expr_t *expr_convolution_integral(const expr_t *e)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(e);
    char name[64] = "τ";
    for (unsigned index = 1u; expr_bindings_get(bindings, name); ++index)
        snprintf(name, sizeof(name), "tau%u", index);
    expr_t *dummy = expr_new_named_var(NUM_NAN, name);
    expr_bindings_free(bindings);
    expr_t *shift = expr_sub(e->b, dummy);
    expr_t *left = expr_substitute(e->a->a, e->b, dummy);
    expr_t *right = expr_substitute(e->a->b, e->b, shift);
    expr_t *product = expr_mul(left, right);
    bool causal = e->ops == &ops_causal_convolution;
    expr_t *lower = expr_new_const(causal ? NUM_ZERO : NUM_NINF);
    expr_t *upper = causal ? expr_clone(e->b) : expr_new_const(NUM_INF);
    expr_t *out = expr_integral_with_bounds_internal(product, lower, upper, dummy);
    expr_free(dummy);
    expr_free(shift);
    expr_free(left);
    expr_free(right);
    expr_free(product);
    expr_free(lower);
    expr_free(upper);
    return out;
}

static bool convolution_uses(const expr_t *e, const expr_t *x)
{
    expr_t *variable = (expr_t *)x;
    bool used = true;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static expr_t *convolution_gaussian_rate(const expr_t *f, const expr_t *x)
{
    const expr_t *base = NULL, *phase = f->ops == &ops_exp ? f->a : NULL;
    if (!phase && !(expr_match_pow_expr(f, &base, &phase) && expr_is_const(base) && num_eq(base->c, NUM_E)))
        return NULL;
    expr_t *first = expr_create_deriv(phase, x);
    expr_t *second = first ? expr_create_deriv(first, x) : NULL;
    expr_t *minus_two = expr_const_long(-2);
    expr_t *raw = second ? expr_div(second, minus_two) : NULL;
    expr_t *rate = raw ? expr_simplify(raw) : NULL;
    expr_t *square = expr_mul(x, x), *product = rate ? expr_mul(rate, square) : NULL;
    expr_t *difference = product ? expr_add(phase, product) : NULL;
    expr_t *check = difference ? expr_simplify(difference) : NULL;
    bool valid = rate && !convolution_uses(rate, x) && expr_const_is_zero(check);
    expr_free(first);
    expr_free(second);
    expr_free(minus_two);
    expr_free(raw);
    expr_free(square);
    expr_free(product);
    expr_free(difference);
    expr_free(check);
    if (!valid) {
        expr_free(rate);
        return NULL;
    }
    return rate;
}

static bool convolution_contains_formal(const expr_t *e)
{
    return e && (expr_is_arbitrary_function(e) || expr_is_integral_transform(e) ||
                 e->ops == &ops_integral || convolution_contains_formal(e->a) || convolution_contains_formal(e->b));
}

static expr_t *convolution_result(const expr_t *e)
{
    const expr_t *f = e->a->a, *g = e->a->b, *x = e->b;
    if (expr_const_is_zero(f) || expr_const_is_zero(g))
        return expr_const_zero();
    if (e->ops == &ops_convolution) {
        const expr_t *impulses[2] = {f, g}, *others[2] = {g, f};
        for (unsigned j = 0u; j < 2u; ++j) {
            if (impulses[j]->ops == &ops_delta && expr_struct_eq(impulses[j]->a, x))
                return expr_clone(others[j]);
            if (impulses[j]->ops == &ops_delta) {
                expr_t *rate = expr_create_deriv(impulses[j]->a, x);
                expr_t *zero = expr_const_zero();
                expr_t *offset = expr_substitute(impulses[j]->a, x, zero);
                expr_t *clean_offset = offset ? expr_simplify(offset) : NULL;
                expr_t *out = NULL;
                if (rate && expr_is_const(rate) && !rate->name && num_is_real(rate->c) &&
                    !num_is_zero(rate->c) && clean_offset && expr_is_const(clean_offset) &&
                    !clean_offset->name && num_is_real(clean_offset->c)) {
                    expr_t *distance = expr_div(clean_offset, rate);
                    expr_t *argument = expr_add(x, distance);
                    expr_t *shifted = expr_substitute(others[j], x, argument);
                    expr_t *scale = expr_abs(rate);
                    out = expr_div(shifted, scale);
                    expr_free(distance);
                    expr_free(argument);
                    expr_free(shifted);
                    expr_free(scale);
                }
                expr_free(rate);
                expr_free(zero);
                expr_free(offset);
                expr_free(clean_offset);
                if (out)
                    return out;
            }
        }
        if (f->ops == &ops_rect && g->ops == &ops_rect &&
            expr_struct_eq(f->a, x) && expr_struct_eq(g->a, x))
            return expr_tri(x);
        expr_t *a = convolution_gaussian_rate(f, x), *b = convolution_gaussian_rate(g, x), *out = NULL;
        if (a && b) {
            expr_t *sum = expr_add(a, b), *product = expr_mul(a, b);
            expr_t *square = expr_mul(x, x), *scaled = expr_mul(product, square);
            expr_t *quotient = expr_div(scaled, sum), *negative = expr_neg(quotient);
            expr_t *exponential = expr_exp(negative), *pi = expr_new_const(NUM_PI);
            expr_t *variance = expr_div(pi, sum), *factor = expr_sqrt(variance);
            expr_t *body = expr_mul(factor, exponential), *zero = expr_const_zero();
            expr_t *args[] = {body, a, zero, b, zero};
            out = expr_real_domain_from_args(5u, args);
            expr_free(sum);
            expr_free(product);
            expr_free(square);
            expr_free(scaled);
            expr_free(quotient);
            expr_free(negative);
            expr_free(exponential);
            expr_free(pi);
            expr_free(variance);
            expr_free(factor);
            expr_free(body);
            expr_free(zero);
        }
        expr_free(a);
        expr_free(b);
        return out;
    } else {
        if (convolution_contains_formal(f) || convolution_contains_formal(g))
            return NULL;
        expr_t *integral = expr_convolution_integral(e);
        const expr_t *dummy = expr_integral_dummy_expr(integral);
        expr_t *expanded = integral ? expr_expand_products_internal(integral->a) : NULL;
        expr_t *primitive = expanded ? expr_integrate(expanded, dummy) : NULL;
        expr_t *out = NULL;
        if (primitive && primitive->ops != &ops_integral) {
            expr_t *zero = expr_const_zero();
            expr_t *upper = expr_substitute(primitive, dummy, x);
            expr_t *lower = expr_substitute(primitive, dummy, zero);
            out = expr_sub(upper, lower);
            expr_free(zero);
            expr_free(upper);
            expr_free(lower);
        }
        expr_free(primitive);
        expr_free(expanded);
        expr_free(integral);
        return out;
    }
    return NULL;
}

static number_t convolution_eval(expr_t *e)
{
    expr_t *result = convolution_result(e);
    number_t out = result ? expr_eval(result) : num_clone(NUM_NAN);
    expr_free(result);
    return out;
}

static expr_t *convolution_simplify(const expr_t *e, expr_t *a, expr_t *b)
{
    expr_t copy = *e;
    copy.a = a;
    copy.b = b;
    expr_t *out = convolution_result(&copy);
    if (!out)
        return expr_simplify_passthrough(e, a, b);
    expr_free(a);
    expr_free(b);
    expr_t *simplified = expr_simplify(out);
    expr_free(out);
    return simplified;
}

static expr_t *convolution_deriv(expr_t *e)
{
    const expr_t *wrt = expr_current_wrt_internal();
    if (!wrt)
        return NULL;
    if (expr_struct_eq(wrt, e->b)) {
        expr_t *derivative = expr_create_deriv(e->a->b, wrt);
        expr_t *out = derivative ? convolution_new(e->ops, e->a->a, derivative, e->b) : NULL;
        if (out && e->ops == &ops_causal_convolution) {
            expr_t *zero = expr_const_zero();
            expr_t *initial = expr_substitute(e->a->b, e->b, zero);
            expr_t *boundary = expr_mul(e->a->a, initial);
            expr_t *updated = expr_add(out, boundary);
            expr_free(out);
            out = updated;
            expr_free(zero);
            expr_free(initial);
            expr_free(boundary);
        }
        expr_free(derivative);
        return out;
    }
    expr_t *left = expr_create_deriv(e->a->a, wrt), *right = expr_create_deriv(e->a->b, wrt);
    expr_t *a = left ? convolution_new(e->ops, left, e->a->b, e->b) : NULL;
    expr_t *b = right ? convolution_new(e->ops, e->a->a, right, e->b) : NULL;
    expr_t *out = a && b ? expr_add(a, b) : NULL;
    expr_free(left);
    expr_free(right);
    expr_free(a);
    expr_free(b);
    return out;
}

static expr_t *convolution_integrate(const expr_t *e, const expr_t *wrt)
{
    return expr_integral(e, wrt);
}

#define CONVOLUTION_OP(name, id, spelling) \
    const expr_ops_t ops_##name = { .eval = convolution_eval, .deriv = convolution_deriv, \
        .kind = id, .arity = EXPR_OP_BINARY, .expression_name = spelling, .function_name = spelling, \
        .TeX_name = "\\operatorname{" spelling "}", .simplify = convolution_simplify, \
        .integrate = convolution_integrate }
CONVOLUTION_OP(convolution, EXPR_KIND_CONVOLUTION, "convolve");
CONVOLUTION_OP(causal_convolution, EXPR_KIND_CAUSAL_CONVOLUTION, "causal_convolve");
#undef CONVOLUTION_OP
