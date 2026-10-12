/**
 * @file expr_bound_display.c
 * @brief Context sign evidence, structural solver proofs and native calculus presentation.
 *
 * Uses expr_context_t to build a disposable display snapshot: real parameter signs can remove
 * principal square roots of their squares, while satisfied parameter conditions
 * can be omitted. Parameter names remain symbolic and the active calculus variable
 * remains free unless an explicit assumption permits a rewrite. Rebuild after context changes; this is
 * not an unconditional algebraic identity or a replacement for the generic result.
 * Binder bodies and unresolved calculus requests are not specialised here.
 * Solver queries use only structural arithmetic facts and context-owned atom values,
 * never numerical sampling of compound expressions to authorise a pivot division.
 */
#include <errno.h>

#include "expr_context-t.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static bool expr_bound_display_scoped(const expr_t *expr)
{
    return expr_is_op(expr, &ops_integral) || expr_is_op(expr, &ops_summation) ||
           expr_is_op(expr, &ops_product) || expr_is_formal_derivative(expr) ||
           expr_is_integral_transform(expr) || expr_is_arbitrary_function(expr);
}

/* Only complete parameter expressions may supply sign or condition evidence. */
static bool expr_bound_display_parameter(const expr_t *expr, const expr_context_t *context)
{
    if (!expr)
        return false;
    if (expr_is_const(expr) && expr_simplify_allows_const_identity_fold(expr))
        return num_is_finite(expr->c);
    if (expr_is_var(expr) || expr_is_const(expr)) {
        expr_context_role_t role;
        return expr_context_get(context, expr, &role, NULL, NULL) && role == EXPR_CONTEXT_PARAMETER &&
               num_is_finite(expr_is_var(expr) ? expr->x : expr->c);
    }
    if (expr_bound_display_scoped(expr) || expr_is_op(expr, &ops_real_domain))
        return false;
    return expr_bound_display_parameter(expr->a, context) &&
           (expr_is_pow_d_expr(expr) || expr->ops->arity != EXPR_OP_BINARY ||
            expr_bound_display_parameter(expr->b, context));
}

static unsigned expr_context_number_facts(number_t value)
{
    if (!num_is_finite(value))
        return 0;
    unsigned facts = num_is_zero(value) ? 0 : EXPR_CONTEXT_NONZERO;
    if (num_is_real(value)) {
        facts |= EXPR_CONTEXT_REAL;
        if (num_gt(value, NUM_ZERO))
            facts |= EXPR_CONTEXT_POSITIVE;
        else if (num_lt(value, NUM_ZERO))
            facts |= EXPR_CONTEXT_NEGATIVE;
    }
    return facts;
}

/* Infer only conservative real/sign facts, never substitute a sample for an assumption. */
static unsigned expr_context_facts(const expr_t *expr, const expr_context_t *context, bool structural)
{
    if (!expr || expr_bound_display_scoped(expr))
        return 0;
    if (structural && expr->ops->arity == EXPR_OP_ATOM) {
        if (expr_is_const(expr) && expr_simplify_allows_const_identity_fold(expr))
            return expr_context_number_facts(expr->c);
        unsigned facts = 0;
        expr_context_role_t role;
        number_t value;
        if (expr_context_get(context, expr, &role, &facts, &value)) {
            if (role == EXPR_CONTEXT_PARAMETER)
                facts |= expr_context_number_facts(value);
            num_destroy(&value);
        }
        return facts;
    }
    if (!structural && expr_bound_display_parameter(expr, context)) {
        number_t value = expr_eval(expr);
        unsigned facts = expr_context_number_facts(value);
        num_destroy(&value);
        return facts;
    }
    if (expr_is_var(expr) || expr_is_const(expr)) {
        unsigned facts = 0;
        expr_context_get(context, expr, NULL, &facts, NULL);
        return facts;
    }
    unsigned a = expr_context_facts(expr->a, context, structural);
    if (expr_is_neg(expr)) {
        unsigned facts = a & (EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO);
        if (a & EXPR_CONTEXT_POSITIVE)
            facts |= EXPR_CONTEXT_NEGATIVE;
        if (a & EXPR_CONTEXT_NEGATIVE)
            facts |= EXPR_CONTEXT_POSITIVE;
        return facts;
    }
    const expr_t *base = NULL;
    number_t exponent = num_new();
    bool square = expr_match_pow_const(expr, &base, &exponent) && num_eq(exponent, NUM_TWO);
    num_destroy(&exponent);
    if (square)
        return (a & (EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO)) |
               ((a & EXPR_CONTEXT_REAL) && (a & EXPR_CONTEXT_NONZERO) ? EXPR_CONTEXT_POSITIVE : 0);
    if (expr_is_sqrt_expr(expr) && (a & EXPR_CONTEXT_POSITIVE))
        return a;
    bool addition = expr_is_op(expr, &ops_add);
    bool subtraction = expr_is_op(expr, &ops_sub);
    if (!addition && !subtraction && !expr_is_op(expr, &ops_mul) && !expr_is_op(expr, &ops_div))
        return 0;
    unsigned b = expr_context_facts(expr->b, context, structural);
    if (addition || subtraction) {
        unsigned facts = (a & b) & EXPR_CONTEXT_REAL;
        unsigned positive = subtraction ? EXPR_CONTEXT_NEGATIVE : EXPR_CONTEXT_POSITIVE;
        unsigned negative = subtraction ? EXPR_CONTEXT_POSITIVE : EXPR_CONTEXT_NEGATIVE;
        if ((a & EXPR_CONTEXT_POSITIVE) && (b & positive))
            facts |= EXPR_CONTEXT_NONZERO | EXPR_CONTEXT_POSITIVE;
        if ((a & EXPR_CONTEXT_NEGATIVE) && (b & negative))
            facts |= EXPR_CONTEXT_NONZERO | EXPR_CONTEXT_NEGATIVE;
        return facts;
    }
    if (expr_is_op(expr, &ops_div) && !(b & EXPR_CONTEXT_NONZERO))
        return 0;
    unsigned facts = (a & b) & (EXPR_CONTEXT_REAL | EXPR_CONTEXT_NONZERO);
    if (((a & EXPR_CONTEXT_POSITIVE) && (b & EXPR_CONTEXT_POSITIVE)) ||
        ((a & EXPR_CONTEXT_NEGATIVE) && (b & EXPR_CONTEXT_NEGATIVE)))
        facts |= EXPR_CONTEXT_POSITIVE;
    if (((a & EXPR_CONTEXT_POSITIVE) && (b & EXPR_CONTEXT_NEGATIVE)) ||
        ((a & EXPR_CONTEXT_NEGATIVE) && (b & EXPR_CONTEXT_POSITIVE)))
        facts |= EXPR_CONTEXT_NEGATIVE;
    return facts;
}

/* Inspect context-owned atom values directly; compound expressions are proved structurally. */
unsigned expr_context_proven_facts(const expr_context_t *context, const expr_t *source)
{
    return expr_context_facts(source, context, true);
}

/* Extract one real, non-zero parameter square from a product under a principal root. */
static bool expr_bound_display_square(const expr_t *expr, const expr_context_t *context,
                                      expr_t **outside, expr_t **remaining)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    bool square = expr_match_pow_const(expr, &base, &exponent) && num_eq(exponent, NUM_TWO);
    num_destroy(&exponent);
    if (square) {
        unsigned facts = expr_context_facts(base, context, false);
        bool signed_real = (facts & (EXPR_CONTEXT_POSITIVE | EXPR_CONTEXT_NEGATIVE)) != 0;
        if (signed_real) {
            *outside = (facts & EXPR_CONTEXT_NEGATIVE) ? expr_neg(base) : expr_clone(base);
            *remaining = expr_new_const(NUM_ONE);
        }
        if (signed_real)
            return true;
    }
    if (!expr_is_op(expr, &ops_mul))
        return false;
    expr_t *rest = NULL;
    if (expr_bound_display_square(expr->a, context, outside, &rest))
        *remaining = expr_mul(rest, expr->b);
    else if (expr_bound_display_square(expr->b, context, outside, &rest))
        *remaining = expr_mul(expr->a, rest);
    else
        return false;
    expr_free(rest);
    return true;
}

/* Mutate only the fresh snapshot; never detach symbols or restrictions in its source. */
static void expr_bound_display_rewrite(expr_t **slot, const expr_context_t *context, bool *changed)
{
    expr_t *expr = *slot;
    if (!expr || expr->ops->arity == EXPR_OP_ATOM || expr_bound_display_scoped(expr))
        return;
    if (expr_is_op(expr, &ops_real_domain)) {
        expr_t **link = &expr->b;
        while (*link) {
            expr_t *pair = *link;
            bool numerical = expr_bound_display_parameter(pair->a, context);
            if (!expr_bound_display_parameter(pair->b->a, context)) {
                link = &pair->b->b;
                continue;
            }
            number_t limit = expr_eval(pair->b->a);
            unsigned facts = numerical ? 0 : expr_context_facts(pair->a, context, false);
            bool signed_zero = num_eq(limit, NUM_ZERO) &&
                               (facts & (EXPR_CONTEXT_POSITIVE | EXPR_CONTEXT_NEGATIVE));
            if (!numerical && !signed_zero) {
                num_destroy(&limit);
                link = &pair->b->b;
                continue;
            }
            number_t value = numerical ? expr_eval(pair->a) : num_clone(NUM_NAN);
            number_t real = num_real_part(value);
            bool valid = numerical ? num_is_finite(value) && num_is_finite(limit) && num_is_real(limit) &&
                                     num_gt(real, limit) : (facts & EXPR_CONTEXT_POSITIVE) != 0;
            num_destroy(&real);
            num_destroy(&limit);
            num_destroy(&value);
            *changed = true;
            if (!valid) {
                *slot = expr_new_const(NUM_NAN);
                expr_free(expr);
                return;
            }
            *link = pair->b->b;
            pair->b->b = NULL;
            expr_free(pair);
        }
        if (!expr->b) {
            *slot = expr->a;
            expr->a = NULL;
            expr_free(expr);
            expr_bound_display_rewrite(slot, context, changed);
            return;
        }
        expr_bound_display_rewrite(&expr->a, context, changed);
        expr->simplified = false;
        return;
    }
    expr_bound_display_rewrite(&expr->a, context, changed);
    expr_bound_display_rewrite(&expr->b, context, changed);
    expr->simplified = false;
    if (expr_is_sqrt_expr(expr)) {
        expr_t *outside = NULL, *remaining = NULL;
        if (expr_bound_display_square(expr->a, context, &outside, &remaining)) {
            *changed = true;
            expr_t *root = expr_sqrt(remaining);
            expr_bound_display_rewrite(&root, context, changed);
            *slot = expr_mul(outside, root);
            expr_free(root);
            expr_free(remaining);
            expr_free(outside);
            expr_free(expr);
        }
    }
}

/* Apply owned binding values only to an independent, non-scoped graph. */
static void expr_context_bind_snapshot(expr_t *expr, const expr_context_t *context)
{
    if (!expr || expr_bound_display_scoped(expr))
        return;
    if (expr_is_var(expr) || expr_is_const(expr)) {
        number_t value;
        if (expr_context_get(context, expr, NULL, NULL, &value)) {
            number_t old = expr_is_var(expr) ? expr->x : expr->c;
            if (!num_eq(old, value) && !(num_is_nan(old) && num_is_nan(value)))
                expr_set_val(expr, value);
            num_destroy(&value);
        }
        return;
    }
    expr_context_bind_snapshot(expr->a, context);
    expr_context_bind_snapshot(expr->b, context);
}

/* Generic beautification may enter binders, so do not run it over a scoped snapshot. */
static bool expr_context_contains_scope(const expr_t *expr)
{
    if (expr_is_integral_transform(expr)) {
        expr_t *result = expr_transform_result(expr);
        const expr_t *body = result;
        while (body && expr_is_op(body, &ops_real_domain))
            body = body->a;
        bool unresolved = !body || expr_struct_eq(body, expr) || expr_context_contains_scope(result);
        expr_free(result);
        return unresolved;
    }
    return expr && (expr_bound_display_scoped(expr) || expr_context_contains_scope(expr->a) ||
                    expr_context_contains_scope(expr->b));
}

/* Build the expression module's native implementation of a context snapshot. */
expr_t *expr_context_snapshot(const expr_context_t *context, const expr_t *expr, bool beautify_unchanged)
{
    bool changed = false;
    expr_t *copy = expr_clone(expr);
    expr_context_bind_snapshot(copy, context);
    expr_bound_display_rewrite(&copy, context, &changed);
    if ((!beautify_unchanged && !changed) || expr_context_contains_scope(copy))
        return copy;
    expr_t *out = copy ? expr_beautify(copy) : NULL;
    expr_free(copy);
    return out;
}

/* Keep private node identity out of the public context module. */
bool expr_context_symbol_identity(const expr_t *symbol, uint64_t *identity, const char **name)
{
    if (expr_is_var(symbol)) {
        *identity = symbol->var_id;
        *name = NULL;
        return true;
    }
    if (expr_is_const(symbol) && symbol->name && *symbol->name &&
        !expr_simplify_allows_const_identity_fold(symbol)) {
        *identity = 0;
        *name = symbol->name;
        return true;
    }
    return false;
}

/* Pointer identity avoids revisiting shared subgraphs while discovering symbols. */
static size_t expr_context_node_hash(const void *key)
{
    uintptr_t pointer = (uintptr_t)*(const expr_t *const *)key;
    return (size_t)((pointer >> 4) ^ (pointer >> 19));
}

static int expr_context_node_compare(const void *left, const void *right)
{
    uintptr_t a = (uintptr_t)*(const expr_t *const *)left;
    uintptr_t b = (uintptr_t)*(const expr_t *const *)right;
    return a == b ? 0 : (a < b ? -1 : 1);
}

/* Traverse each distinct graph node once; look up each discovered symbol by hash. */
static bool expr_context_capture_visit(expr_context_t *context, const expr_t *expr, const expr_t *wrt,
                                       dictionary_t *visited)
{
    if (!expr || expr_bound_display_scoped(expr))
        return true;
    bool seen = true;
    if (dictionary_get(visited, &expr, &seen))
        return true;
    if (!dictionary_set(visited, &expr, &seen))
        return false;
    uint64_t identity;
    const char *name;
    if (expr_context_symbol_identity(expr, &identity, &name)) {
        number_t value = expr_is_var(expr) ? expr->x : expr->c;
        number_t captured;
        if (expr_context_get(context, expr, NULL, NULL, &captured)) {
            bool equal = num_eq(captured, value) || (num_is_nan(captured) && num_is_nan(value));
            num_destroy(&captured);
            if (!equal)
                errno = EDOM;
            return equal;
        }
        if (!num_is_finite(value) && !num_is_nan(value))
            return true;
        expr_context_role_t role = expr_is_var(expr) && (!wrt || identity == wrt->var_id) ?
                                   EXPR_CONTEXT_FREE : EXPR_CONTEXT_PARAMETER;
        return expr_context_set(context, expr, role, 0, value);
    }
    return expr_context_capture_visit(context, expr->a, wrt, visited) &&
           expr_context_capture_visit(context, expr->b, wrt, visited);
}

/* Capture a context using a temporary visited-node index, never retaining source nodes. */
bool expr_context_capture_symbols(expr_context_t *context, const expr_t *expr, const expr_t *wrt)
{
    dictionary_t *visited = dictionary_create(sizeof(expr_t *), sizeof(bool), expr_context_node_hash,
                                             expr_context_node_compare, NULL, NULL, NULL, NULL, NULL);
    if (!visited)
        return false;
    bool ok = expr_context_capture_visit(context, expr, wrt, visited);
    dictionary_destroy(visited);
    return ok;
}
