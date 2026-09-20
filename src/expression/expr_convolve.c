#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <stdio.h>

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

static bool uses(const expr_t *e, const expr_t *x)
{
    expr_t *variable = (expr_t *)x;
    bool used = true;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static expr_t *gaussian_rate(const expr_t *f, const expr_t *x)
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
    bool valid = rate && !uses(rate, x) && expr_const_is_zero(check);
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

static bool contains_formal(const expr_t *e)
{
    return e && (expr_is_arbitrary_function(e) || expr_is_integral_transform(e) ||
                 e->ops == &ops_integral || contains_formal(e->a) || contains_formal(e->b));
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
        expr_t *a = gaussian_rate(f, x), *b = gaussian_rate(g, x), *out = NULL;
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
        if (contains_formal(f) || contains_formal(g))
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
