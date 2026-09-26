#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

typedef number_t (*signal_numeric_fn)(number_t);
static const signal_numeric_fn signal_numeric[EXPR_KIND_COUNT] = {
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
    if (a && expr->ops != &ops_step && expr->ops != &ops_principal_value && expr->ops != &ops_finite_part) {
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
    if (expr->ops == &ops_step) {
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

static expr_t *signal_integrate(const expr_t *expr, const expr_t *wrt)
{
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
