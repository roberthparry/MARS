/**
 * @file expr_fourier_support.c
 * @brief Fourier construction, matching and condition helpers.
 *
 * Owns temporary transform expressions and supplies affine matching and parameter checks. Forward and inverse rule
 * units share this context to keep ownership and domain handling consistent.
 *
 * This is part of the expression.h implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

/* Shared Fourier construction, structural matching and domain-condition helpers. */
#include "expr_fourier_internal.h"

/* Own a temporary expression in the Fourier context. */
expr_t *expr_fourier_keep(fourier_context_t *c, expr_t *expr)
{
    if (!expr)
        return NULL;
    if (c->count == c->capacity) {
        size_t capacity = c->capacity ? c->capacity * 2u : 64u;
        expr_t **nodes = realloc(c->nodes, capacity * sizeof(*nodes));
        if (!nodes) {
            expr_free(expr);
            c->failed = true;
            return NULL;
        }
        c->nodes = nodes;
        c->capacity = capacity;
    }
    c->nodes[c->count++] = expr;
    return expr;
}

/* Construct an exact Euler–Mascheroni constant with its symbolic provenance. */
expr_t *expr_fourier_euler_constant(fourier_context_t *c)
{
    expr_t *out = expr_fourier_keep(c, expr_new_named_const(NUM_EULER_MASCHERONI, "@gamma"));
    if (out)
        out->binding_expr = expr_binding_expr_new_const(EXPR_BINDING_CONST_GAMMA);
    return out;
}

/* Recover literal provenance recursively without substituting bindings. */
static void exact_literals_owned(expr_t **node)
{
    expr_t *f = *node;
    if (!f)
        return;
    if (expr_is_unnamed_const(f) && f->binding_expr && !expr_binding_expr_is_numeric_literal(f->binding_expr)) {
        expr_t *expanded = expr_expand_preserved_for_display(f);
        if (expanded) {
            *node = expanded;
            expr_free(f);
        }
        return;
    }
    exact_literals_owned(&f->a);
    exact_literals_owned(&f->b);
    f->simplified = false;
    f->simplify_epoch = 0u;
}

/* Clone an expression and recover exact literal coefficient provenance. */
expr_t *expr_fourier_exact_literals(fourier_context_t *c, const expr_t *f)
{
    expr_t *out = expr_clone(f);
    exact_literals_owned(&out);
    return expr_fourier_keep(c, out);
}

/* Match a symbolic or fixed-exponent power without changing its base. */
bool expr_fourier_match_power(fourier_context_t *c, const expr_t *f, const expr_t **base, const expr_t **power)
{
    if (expr_match_pow_expr(f, base, power))
        return true;
    if (f && f->ops == &ops_pow_d) {
        *base = f->a;
        *power = constant(c, f->c);
        return true;
    }
    return false;
}

/* Determine whether an expression depends on the source coordinate. */
bool expr_fourier_uses(const expr_t *e, const expr_t *x)
{
    bool used = true;
    expr_t *variable = (expr_t *)x;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

/* Construct an absolute value without substituting variable bindings. */
expr_t *expr_fourier_abs(fourier_context_t *c, const expr_t *e)
{
    while (e && e->ops == &ops_neg)
        e = e->a;
    if (expr_is_const(e) && !e->name) {
        number_t value = num_abs(e->c);
        expr_t *out = constant(c, value);
        num_destroy(&value);
        return out;
    }
    /* Retain exact named constants such as pi while removing a provable real sign.
     * A variable's supplied value must never determine the displayed algebra. */
    number_t value = NUM_NAN;
    bool known_real = expr_fourier_literal_value(e, &value) && num_is_real(value);
    bool negative = known_real && num_lt(value, NUM_ZERO);
    num_destroy(&value);
    if (known_real)
        return negative ? clean(c, ft_neg(c, e)) : expr_fourier_keep(c, expr_clone(e));
    return expr_fourier_keep(c, expr_abs(e));
}

/* Create a temporary coordinate that cannot capture existing bindings. */
expr_t *expr_fourier_fresh_variable(fourier_context_t *c, const expr_t *f, const expr_t *target)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(f);
    expr_bindings_t *targets = expr_bindings_from_expr_internal(target);
    char name[64];
    for (unsigned n = 0u;; ++n) {
        snprintf(name, sizeof(name), "_fourier_%u", n);
        if (!expr_bindings_get(bindings, name) && !expr_bindings_get(targets, name))
            break;
    }
    expr_t *out = expr_fourier_keep(c, expr_new_named_var(NUM_NAN, name));
    expr_bindings_free(targets);
    expr_bindings_free(bindings);
    return out;
}

/* Find an absolute-value node containing the source coordinate. */
const expr_t *expr_fourier_absolute_source(const expr_t *f, const expr_t *x)
{
    if (!f)
        return NULL;
    if (f->ops == &ops_abs && expr_struct_eq(f->a, x))
        return f;
    const expr_t *left = expr_fourier_absolute_source(f->a, x);
    return left ? left : expr_fourier_absolute_source(f->b, x);
}

/* Evaluate only expressions with no parameter or variable bindings. */
bool expr_fourier_literal_value(const expr_t *e, number_t *value)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(e);
    bool literal = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (literal)
        *value = expr_eval(e);
    return literal && num_is_finite(*value);
}

/* Check or retain a strictly positive real-part condition. */
bool expr_fourier_positive(fourier_context_t *c, const expr_t *value)
{
    if (value && value->ops == &ops_real_bound)
        value = value->a;
    expr_t *bound = clean(c, value);
    number_t n = NUM_NAN;
    bool known = expr_fourier_literal_value(bound, &n);
    number_t real = num_real_part(n);
    bool valid = !known || num_gt(real, NUM_ZERO);
    num_destroy(&real);
    num_destroy(&n);
    if (!valid)
        return false;
    if (!known) {
        expr_t *pair = expr_alloc(&ops_argument_list);
        pair->a = expr_clone(bound);
        pair->b = expr_alloc(&ops_argument_list);
        pair->b->a = expr_const_zero();
        pair->b->b = c->conditions;
        c->conditions = pair;
    }
    return true;
}

/* Check or retain a real-valued parameter condition. */
bool expr_fourier_real_parameter(fourier_context_t *c, const expr_t *value)
{
    value = clean(c, value);
    if (value && (value->ops == &ops_real_bound || value->ops == &ops_imag_coordinate))
        return true;
    while (value && value->ops == &ops_neg)
        value = value->a;
    expr_t *test = expr_fourier_keep(c, expr_new_unary_internal(&ops_real_parameter, expr_clone(value)));
    return expr_fourier_positive(c, test);
}

/* Collect affine coefficients structurally without sampling free parameters. */
bool expr_fourier_affine(fourier_context_t *c, const expr_t *f, const expr_t *x, expr_t **a, expr_t **b)
{
    if (!expr_fourier_uses(f, x)) {
        *a = integer(c, 0);
        *b = expr_fourier_keep(c, expr_clone(f));
        return true;
    }
    if (expr_struct_eq(f, x)) {
        *a = integer(c, 1);
        *b = integer(c, 0);
        return true;
    }
    expr_t *left_a = NULL, *left_b = NULL, *right_a = NULL, *right_b = NULL;
    if ((f->ops == &ops_add || f->ops == &ops_sub) &&
        expr_fourier_affine(c, f->a, x, &left_a, &left_b) && expr_fourier_affine(c, f->b, x, &right_a, &right_b)) {
        *a = clean(c, f->ops == &ops_add ? ft_add(c, left_a, right_a) : ft_sub(c, left_a, right_a));
        *b = clean(c, f->ops == &ops_add ? ft_add(c, left_b, right_b) : ft_sub(c, left_b, right_b));
        return true;
    }
    if (f->ops == &ops_neg && expr_fourier_affine(c, f->a, x, &left_a, &left_b)) {
        *a = clean(c, ft_neg(c, left_a));
        *b = clean(c, ft_neg(c, left_b));
        return true;
    }
    if (f->ops == &ops_mul) {
        const expr_t *coefficient = !expr_fourier_uses(f->a, x) ? f->a : f->b;
        const expr_t *dependent = coefficient == f->a ? f->b : f->a;
        if (!expr_fourier_uses(coefficient, x) && expr_fourier_affine(c, dependent, x, &left_a, &left_b)) {
            *a = clean(c, ft_mul(c, coefficient, left_a));
            *b = clean(c, ft_mul(c, coefficient, left_b));
            return true;
        }
    }
    if (f->ops == &ops_div && !expr_fourier_uses(f->b, x) && expr_fourier_affine(c, f->a, x, &left_a, &left_b)) {
        *a = clean(c, ft_div(c, left_a, f->b));
        *b = clean(c, ft_div(c, left_b, f->b));
        return true;
    }
    return false;
}

/* Match exponential notation or a power of Euler's constant. */
const expr_t *expr_fourier_exponent(const expr_t *f)
{
    if (f->ops == &ops_exp)
        return f->a;
    const expr_t *base = NULL, *power = NULL;
    return expr_match_pow_expr(f, &base, &power) && expr_is_const(base) && num_eq(base->c, NUM_E) ? power : NULL;
}
