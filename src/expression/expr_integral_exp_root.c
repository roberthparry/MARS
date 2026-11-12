/**
 * @file expr_integral_exp_root.c
 * @brief Stable real definite integrals of reciprocal square roots of affine exponentials.
 *
 * Symbolic integration constructs a local elementary primitive with explicit non-zero/domain conditions.
 * Numerical evaluation recognises k/sqrt(a*exp(b*x+d)+c) structurally, including positive constant-base
 * powers and independent parameter bindings. The substitution u = sqrt(a*exp(b*x+d)+c) avoids repeated
 * symbolic integration and quadrature during sampling. Endpoint positivity establishes the real domain
 * throughout this monotone radicand; an integrable zero endpoint is allowed for non-zero offsets.
 * Unsupported cases retain the ordinary integrator.
 */
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

struct exponential_root {
    number_t offset;
    number_t amplitude;
    number_t rate;
    number_t phase;
};

static bool exponential_base(const expr_t *expr, const expr_t *wrt, struct exponential_root *form, unsigned depth);

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static const expr_t *root_base(const expr_t *expr, const expr_t *wrt, const expr_t **numerator)
{
    *numerator = NULL;
    const expr_t *base = NULL;
    number_t exponent = NUM_NAN;
    if (expr_is_op(expr, &ops_div) && !depends_on_wrt(expr->a, wrt)) {
        *numerator = expr->a;
        if (expr_is_sqrt_expr(expr->b))
            return expr->b->a;
        if (!expr_match_pow_const(expr->b, &base, &exponent) || !num_eq(exponent, NUM_HALF))
            base = NULL;
    } else if (!expr_match_pow_const(expr, &base, &exponent) || !num_eq(exponent, num_neg(NUM_HALF)))
        base = NULL;
    num_destroy(&exponent);
    return base;
}

/* R''/R' = b and R-R'/b = c recognise an affine exponential independently of its spelling. */
expr_t *integrate_exp_root_rule(const expr_t *expr, const expr_t *wrt)
{
    NUM_SCOPE(numbers);
    const expr_t *numerator = NULL;
    const expr_t *base = root_base(expr, wrt, &numerator);
    if (!base)
        return NULL;
    /* A symbolic radicand coefficient can enter a different real case or make the rate/amplitude vanish. */
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(base);
    bool fixed = bindings && expr_bindings_count(bindings) == 1u &&
                 is_wrt(expr_bindings_expr_at(bindings, 0u), wrt);
    expr_bindings_free(bindings);
    if (!fixed)
        return NULL;
    struct exponential_root form = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    if (!exponential_base(base, wrt, &form, 0u) || !finite_real(form.offset) || !finite_real(form.amplitude) ||
        !finite_real(form.rate) || !finite_real(form.phase) || num_is_zero(form.rate) || num_is_zero(form.amplitude))
        return NULL;
    expr_t *first = expr_create_deriv(base, wrt);
    expr_t *second = first ? expr_create_deriv(first, wrt) : NULL;
    expr_t *rate = first && second ? simplify_owned(expr_div(second, first)) : NULL;
    expr_t *exponential = rate ? simplify_owned(expr_div(first, rate)) : NULL;
    expr_t *offset = exponential ? simplify_owned(expr_sub(base, exponential)) : NULL;
    expr_t *result = NULL;
    expr_t *owned[16] = {NULL};
    if (!rate || !offset || depends_on_wrt(rate, wrt) || depends_on_wrt(offset, wrt) || expr_const_is_zero(rate))
        goto done;
    /* A free offset can cross c=0. Keep its integral intact until a real case is known instead of losing that case. */
    if (!expr_is_const(offset) || expr_is_named_const(offset) || offset->binding_expr || !finite_real(offset->c))
        goto done;
    owned[0] = expr_sqrt(base);
    owned[1] = expr_const_is_zero(offset) ? expr_new_const(NUM_ONE) : expr_sqrt(offset);
    owned[2] = owned[1] ? expr_mul(rate, owned[1]) : NULL;
    bool negative = expr_is_const(offset) && num_is_real(offset->c) && num_lt(offset->c, NUM_ZERO);
    bool hyperbolic = !negative && !expr_const_is_zero(offset) && num_gt(form.amplitude, NUM_ZERO);
    if (negative) {
        expr_free(owned[1]);
        owned[3] = expr_neg(offset);
        owned[1] = owned[3] ? expr_sqrt(owned[3]) : NULL;
        expr_free(owned[2]);
        owned[2] = owned[1] ? expr_mul(rate, owned[1]) : NULL;
        owned[4] = owned[0] && owned[1] ? expr_div(owned[0], owned[1]) : NULL;
        owned[5] = owned[4] ? expr_atan(owned[4]) : NULL;
    } else if (expr_const_is_zero(offset)) {
        expr_free(owned[2]);
        owned[2] = owned[0] ? expr_mul(rate, owned[0]) : NULL;
        owned[5] = expr_new_const(NUM_NEG_ONE);
    } else if (hyperbolic) {
        owned[4] = owned[0] && owned[1] ? expr_div(owned[1], owned[0]) : NULL;
        owned[3] = owned[4] ? expr_atanh(owned[4]) : NULL;
        owned[5] = owned[3] ? expr_neg(owned[3]) : NULL;
    } else {
        owned[3] = owned[0] && owned[1] ? expr_sub(owned[0], owned[1]) : NULL;
        owned[4] = owned[0] && owned[1] ? expr_add(owned[0], owned[1]) : NULL;
        owned[12] = owned[3] && owned[4] ? expr_div(owned[3], owned[4]) : NULL;
        owned[13] = owned[12] ? expr_abs(owned[12]) : NULL;
        owned[5] = owned[13] ? expr_log(owned[13]) : NULL;
    }
    owned[6] = owned[5] ? expr_mul_long(owned[5], negative || hyperbolic || expr_const_is_zero(offset) ? 2L : 1L) : NULL;
    owned[7] = owned[6] && owned[2] ? expr_div(owned[6], owned[2]) : NULL;
    owned[8] = owned[7] ? (numerator ? expr_mul(numerator, owned[7]) : expr_clone(owned[7])) : NULL;
    owned[9] = expr_new_const(NUM_ZERO);
    owned[10] = expr_abs(rate);
    owned[11] = expr_abs(offset);
    /* Positive exponential amplitudes use their principal elementary branches, including infinite endpoint
     * limits. The logarithm of an absolute value is restricted to the real-root interval for negative amplitudes. */
    owned[14] = num_gt(form.amplitude, NUM_ZERO) ? expr_new_const(NUM_ONE) :
                owned[9] ? expr_real_comparison(base, owned[9], !expr_const_is_zero(offset)) : NULL;
    /* Keep the generic non-zero-offset branch local; c=0 has its own elementary limit. */
    expr_t *args[] = {owned[8], owned[10], owned[9], owned[14], owned[9], owned[11], owned[9]};
    if (owned[8] && owned[9] && owned[10] && owned[11] && owned[14])
        result = expr_real_domain_from_args(expr_const_is_zero(offset) ? 5u : 7u, args);
done:
    for (size_t i = 0; i < sizeof owned / sizeof *owned; ++i)
        expr_free(owned[i]);
    expr_free(offset);
    expr_free(exponential);
    expr_free(rate);
    expr_free(second);
    expr_free(first);
    return result;
}

/* Follow one dependent exponential through affine arithmetic, with a bounded recognition depth. */
static bool exponential_base(const expr_t *expr, const expr_t *wrt, struct exponential_root *form, unsigned depth)
{
    if (!expr || depth > 32u)
        return false;
    const expr_t *argument = NULL, *independent = NULL, *dependent = NULL;
    bool subtract = expr_is_op(expr, &ops_sub), divide = expr_is_op(expr, &ops_div);
    bool additive = expr_is_op(expr, &ops_add) || subtract;
    bool product = expr_is_op(expr, &ops_mul) || divide;
    if (additive || product) {
        bool left = depends_on_wrt(expr->a, wrt), right = depends_on_wrt(expr->b, wrt);
        if (left == right || (divide && right))
            return false;
        independent = left ? expr->b : expr->a;
        dependent = left ? expr->a : expr->b;
        number_t value = expr_eval(independent);
        if (!finite_real(value) || !exponential_base(dependent, wrt, form, depth + 1u))
            return false;
        if (additive) {
            form->offset = subtract ? (left ? num_sub(form->offset, value) : num_sub(value, form->offset))
                                    : num_add(form->offset, value);
            if (subtract && right)
                form->amplitude = num_neg(form->amplitude);
        } else {
            if (divide && num_is_zero(value))
                return false;
            number_t scale = divide ? num_div(NUM_ONE, value) : num_clone(value);
            form->offset = num_mul(form->offset, scale);
            form->amplitude = num_mul(form->amplitude, scale);
        }
        return true;
    }
    if (expr_is_op(expr, &ops_neg)) {
        if (!exponential_base(expr->a, wrt, form, depth + 1u))
            return false;
        form->offset = num_neg(form->offset);
        form->amplitude = num_neg(form->amplitude);
        return true;
    }
    number_t logarithm = NUM_ONE;
    if (expr->ops && expr->ops->kind == EXPR_KIND_EXP)
        argument = expr->a;
    else if (expr_is_op(expr, &ops_pow) && !depends_on_wrt(expr->a, wrt)) {
        number_t base = expr_eval(expr->a);
        if (!finite_real(base) || !num_gt(base, NUM_ZERO))
            return false;
        logarithm = num_log(base);
        argument = expr->b;
    }
    expr_t *constant = NULL, *coefficient = NULL;
    bool matched = argument && match_symbolic_affine_constant_and_coeff(argument, wrt, &constant, &coefficient);
    if (matched) {
        form->offset = NUM_ZERO;
        form->amplitude = NUM_ONE;
        form->rate = num_mul(expr_eval(coefficient), logarithm);
        form->phase = num_mul(expr_eval(constant), logarithm);
    }
    expr_free(coefficient);
    expr_free(constant);
    return matched;
}

/* Evaluate all three offset signs without specialising the caller's symbolic expression. */
number_t expr_integral_exp_root_definite(const expr_t *integrand, const expr_t *wrt, number_t lower, number_t upper,
                                        bool *matched_out)
{
    NUM_SCOPE(numbers);
    if (!matched_out)
        return num_clone(NUM_NAN);
    *matched_out = false;
    const expr_t *numerator = NULL;
    number_t scale = NUM_ONE;
    if (!integrand || !finite_real(lower) || !finite_real(upper))
        return num_clone(NUM_NAN);
    const expr_t *base = root_base(integrand, wrt, &numerator);
    if (numerator)
        scale = expr_eval(numerator);
    struct exponential_root form = {NUM_NAN, NUM_NAN, NUM_NAN, NUM_NAN};
    if (!base || !finite_real(scale) || !exponential_base(base, wrt, &form, 0u) ||
        !finite_real(form.offset) || !finite_real(form.amplitude) || !finite_real(form.rate) ||
        !finite_real(form.phase) || num_is_zero(form.rate) || num_is_zero(form.amplitude))
        return num_clone(NUM_NAN);
    *matched_out = true;
    number_t radicands[2], roots[2], endpoints[] = {lower, upper};
    for (unsigned i = 0; i < 2u; ++i) {
        number_t power = num_add(num_mul(form.rate, endpoints[i]), form.phase);
        radicands[i] = num_add(form.offset, num_mul(form.amplitude, num_exp(power)));
        if (!finite_real(radicands[i]) || num_lt(radicands[i], NUM_ZERO) ||
            (num_is_zero(radicands[i]) && num_is_zero(form.offset)))
            return num_clone(NUM_NAN);
        roots[i] = num_sqrt(radicands[i]);
    }
    number_t result;
    if (num_is_zero(form.offset))
        result = num_mul(num_div(NUM_TWO, form.rate),
                         num_sub(num_div(NUM_ONE, roots[0]), num_div(NUM_ONE, roots[1])));
    else if (num_gt(form.offset, NUM_ZERO)) {
        number_t root = num_sqrt(form.offset);
        number_t threshold = num_mul(NUM_TWO, root);
        if (num_ge(roots[0], threshold) && num_ge(roots[1], threshold))
            result = num_mul(num_div(NUM_TWO, num_mul(form.rate, root)),
                             num_sub(num_atanh(num_div(root, roots[0])), num_atanh(num_div(root, roots[1]))));
        else {
            number_t ratio = num_div(num_add(roots[1], root), num_add(roots[0], root));
            /* Rationalise u-sqrt(c) near the asymptote; use small atanh arguments in the opposite limit. */
            result = num_div(num_sub(num_mul(form.rate, num_sub(upper, lower)), num_mul(NUM_TWO, num_log(ratio))),
                             num_mul(form.rate, root));
        }
    } else {
        number_t root = num_sqrt(num_neg(form.offset));
        /* Reciprocal arguments avoid subtracting two rounded copies of pi/2 for large exponential values. */
        number_t difference = num_ge(roots[0], root) && num_ge(roots[1], root) ?
            num_sub(num_atan(num_div(root, roots[0])), num_atan(num_div(root, roots[1]))) :
            num_sub(num_atan(num_div(roots[1], root)), num_atan(num_div(roots[0], root)));
        result = num_mul(num_div(NUM_TWO, num_mul(form.rate, root)),
                         difference);
    }
    return num_scope_detach(num_mul(scale, result));
}
