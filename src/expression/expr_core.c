/* expr_core.c - lazy, vtable-driven, reference-counted differentiable value DAG
 *
 * This file implements:
 *   - Node allocation and the expr_new_X / expr_create_X family of constructors
 *   - Reference counting (expr_retain / expr_free)
 *   - Name handling, including ASCII-to-Unicode normalisation for Greek letter
 *     names (e.g. "alpha" -> "alpha-as-unicode") so names are canonical in output
 *   - Lazy primal evaluation (expr_eval) via vtable dispatch (ops->eval)
 *   - Lazy derivative construction (expr_get_deriv / expr_create_deriv) via
 *     vtable dispatch (ops->deriv), with the result cached in expr_t::dx_cache
 *   - All arithmetic and mathematical operator constructors (expr_add, expr_sin, etc.)
 *   - expr_cmp, expr_print, and other accessors
 *
 * The operator implementations (eval/deriv bodies) live in the same file,
 * grouped by operator family after the core infrastructure.
 *
 * Partial derivatives: tl_wrt is the active variable being
 * differentiated with respect to. NULL means "single-variable / differentiate
 * w.r.t. every variable" (the original behaviour of expr_get_deriv /
 * expr_create_deriv). This is ordinary process-local differentiation context;
 * the DAG itself remains unsynchronised and is not safe for concurrent
 * mutation or evaluation.
 */

#include "expr_bindings.h"
#include "number.h"
#include <limits.h>
#include <stdlib.h>
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expression.h"

/* Pointer to the variable being differentiated with respect to.
 * NULL = single-variable / "all variables" mode (original behaviour of
 * expr_get_deriv / expr_create_deriv). This is differentiation context only,
 * not a general thread-safety mechanism for the DAG. */
static const expr_t *tl_wrt = NULL;

static struct _expr_t _EXPR_NAN_NODE = {.ops = &ops_const,
                                        .a = NULL,
                                        .b = NULL,
                                        .c = {{0, 0, 0, 0, 0}},
                                        .x = {{0, 0, 0, 0, 0}},
                                        .x_valid = 1,
                                        .epoch = 0,
                                        .simplified = true,
                                        .simplify_epoch = 0,
                                        .dx_cache = NULL,
                                        .name = NULL,
                                        .refcount = INT_MAX,
                                        .var_id = 0};

static expr_t *expr_nan_const_shared(void)
{
    static int initialised = 0;

    if (!initialised) {
        _EXPR_NAN_NODE.c = NUM_NAN;
        _EXPR_NAN_NODE.x = NUM_NAN;
        initialised = 1;
    }
    return &_EXPR_NAN_NODE;
}

/* ------------------------------------------------------------------------- */
/* Lazy eval / deriv                                                         */
/* ------------------------------------------------------------------------- */

static number_t expr_eval_cached_num(const expr_t *expr)
{
    if (!expr)
        return NUM_ZERO;

    expr_t *m = (expr_t *)expr;

    /* Binding-expression atoms are lazy constants/initialisers. They only
     * refresh when the global working precision increases. */
    if (m->ops->arity == EXPR_OP_ATOM) {
        if (m->binding_expr) {
            number_t refreshed;

            if (expr_binding_expr_eval_if_precision_increased(m->binding_expr, &refreshed)) {
                expr_store_const_num(m, num_clone(refreshed));
                expr_store_value_num(m, refreshed);
                m->x_valid = 1;
                m->epoch++;
            }
        }
        return m->x;
    }

    /* Recurse into children to bring their epochs current, then check whether
     * this node's cached value is still valid. ops->eval() will call expr_eval_qc
     * on children a second time, but those calls return immediately (x_valid=1). */
    expr_eval_cached_num(m->a);
    if (m->ops->arity == EXPR_OP_BINARY)
        expr_eval_cached_num(m->b);

    uint64_t child_epoch = m->a ? m->a->epoch : 0;
    if (m->b && m->b->epoch > child_epoch)
        child_epoch = m->b->epoch;

    if (!m->x_valid || child_epoch > m->epoch) {
        expr_store_value_num(m, m->ops->eval(m));
        m->x_valid = 1;
        m->epoch = child_epoch;
    }
    return m->x;
}

number_t expr_eval_num_internal(const expr_t *expr)
{
    expr_init_singletons();
    return expr_eval_cached_num(expr);
}

static uint64_t current_wrt_id(void)
{
    return tl_wrt ? tl_wrt->var_id : 0;
}

/* Look up (or compute and cache) the derivative of expr w.r.t. tl_wrt.
 * Returns a borrowed pointer owned by the cache entry. */
static expr_t *expr_build_dx(expr_t *expr)
{
    if (!expr || !expr->ops->deriv)
        return NULL;

    uint64_t wrt_id = current_wrt_id();

    /* Search the cache for a matching wrt entry. */
    for (expr_deriv_cache_t *ce = expr->dx_cache; ce; ce = ce->next) {
        if (ce->wrt_id == wrt_id)
            return ce->dx; /* borrowed */
    }

    /* Not cached: compute and insert at head. */
    expr_t *dx = expr->ops->deriv(expr); /* refcount = 1, tl_wrt still set */
    expr_deriv_cache_t *ce = malloc(sizeof *ce);
    if (!ce)
        abort();
    ce->wrt_id = wrt_id;
    ce->dx = dx;
    ce->next = expr->dx_cache;
    expr->dx_cache = ce;
    return dx; /* borrowed */
}

bool expr_is_differentiable(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr->ops && expr->ops->diff_kind == EXPR_DIFF_NONE)
        return false;
    if (expr_is_integral_transform(expr) || expr->ops == &ops_real_domain)
        return true;
    if (expr->ops == &ops_convolution || expr->ops == &ops_causal_convolution)
        return expr_is_differentiable(expr->a->a) && expr_is_differentiable(expr->a->b);
    if (expr->ops == &ops_summation || expr->ops == &ops_product)
        return expr_is_differentiable(expr->a);
    if (expr->ops == &ops_pow_d)
        return expr_is_differentiable(expr->a);
    if (expr->ops == &ops_polygamma)
        return expr_is_differentiable(expr->b);
    if (expr->ops && expr->ops->arity != EXPR_OP_ATOM && !expr_is_differentiable(expr->a))
        return false;
    if (expr->ops && expr->ops->arity == EXPR_OP_BINARY && !expr_is_differentiable(expr->b))
        return false;
    return true;
}

/* Return an owning reference to the derivative of expr w.r.t. tl_wrt.
 * Falls back to a zero constant when no derivative exists. */
static expr_t *get_dx(const expr_t *expr)
{
    /* tl_wrt is already set by the caller; call expr_build_dx directly so we
     * don't go through the public expr_get_deriv signature which also sets it. */
    const expr_t *d = expr_build_dx((expr_t *)expr);
    if (d) {
        expr_retain((expr_t *)d);
        return (expr_t *)d;
    }
    return expr_new_const(NUM_ZERO);
}

expr_t *expr_get_dx_internal(const expr_t *expr)
{
    return get_dx(expr);
}

const expr_t *expr_current_wrt_internal(void)
{
    return tl_wrt;
}

number_t expr_eval(const expr_t *expr)
{
    return num_clone(expr_eval_num_internal(expr));
}

/* ------------------------------------------------------------------------- */
/* Public derivative (borrowed)                                              */
/* ------------------------------------------------------------------------- */

const expr_t *expr_get_deriv(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt)
        return NULL;
    if (wrt->ops == &ops_const)
        return expr_nan_const_shared();
    const expr_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    const expr_t *result = expr_build_dx((expr_t *)expr);
    tl_wrt = saved_wrt;
    return result; /* borrowed */
}

/* ------------------------------------------------------------------------- */
/* Setters                                                                   */
/* ------------------------------------------------------------------------- */

void expr_set_val(expr_t *expr, number_t value)
{
    bool preserve_binding_expr = false;

    if (!expr)
        abort();
    if (expr->ops != &ops_var && !(expr->ops == &ops_const && expr->name && *expr->name))
        abort();
    if (expr->binding_expr && expr_binding_expr_is_numeric_literal(expr->binding_expr)) {
        number_t binding_value = expr_binding_expr_eval(expr->binding_expr);

        preserve_binding_expr = num_eq(binding_value, value);
        num_destroy(&binding_value);
    }
    if (expr->binding_expr && !preserve_binding_expr) {
        expr_binding_expr_free(expr->binding_expr);
        expr->binding_expr = NULL;
    }
    expr_store_const_num(expr, num_clone(value));
    expr_store_value_num(expr, num_clone(expr->c));
    expr->x_valid = 1;
    expr->epoch++;
    expr->simplified = false;
    expr->simplify_epoch = 0;
}

/* Keep the authored binding alongside its numerical value for native result formatting. */
bool expr_set_binding_value_text(expr_t *expr, const string_t *text)
{
    if (!expr || !text || (expr->ops != &ops_var && !(expr->ops == &ops_const && expr->name && *expr->name)))
        return false;
    expr_binding_expr_t *binding = expr_binding_expr_parse_view(string_view_all(text), NULL);

    if (!binding)
        return false;
    binding = expr_binding_expr_simplify(binding);
    if (!binding)
        return false;
    number_t value = expr_binding_expr_eval(binding);

    expr_set_val(expr, value);
    expr_binding_expr_free(expr->binding_expr);
    expr->binding_expr = binding;
    num_destroy(&value);
    return true;
}

/* Copy exact binding metadata before updating a possibly identical destination node. */
bool expr_copy_binding_value(expr_t *destination, const expr_t *source)
{
    if (!destination || !source ||
        (destination->ops != &ops_var && !(destination->ops == &ops_const && destination->name && *destination->name)))
        return false;
    if (destination == source)
        return true;
    expr_binding_expr_t *binding = source->binding_expr ? expr_binding_expr_clone(source->binding_expr) : NULL;

    if (source->binding_expr && !binding)
        return false;
    number_t value = expr_get_val(source);

    expr_set_val(destination, value);
    expr_binding_expr_free(destination->binding_expr);
    destination->binding_expr = binding;
    num_destroy(&value);
    return true;
}

void expr_set_name(expr_t *expr, const char *name)
{
    if (!expr)
        return;
    if (expr->name)
        free(expr->name);
    expr->name = expr_normalise_name(name);
    expr->epoch++;
    expr->simplified = false;
    expr->simplify_epoch = 0;
}

void expr_set_name_text(expr_t *expr, const string_t *name)
{
    if (!expr)
        return;
    if (expr->name)
        free(expr->name);
    expr->name = name ? expr_take_string_as_c_string(expr_normalise_name_text(name)) : NULL;
    expr->epoch++;
    expr->simplified = false;
    expr->simplify_epoch = 0;
}

number_t expr_get_val(const expr_t *expr)
{
    return expr_eval(expr);
}

/* ------------------------------------------------------------------------- */
/* Core node constructors (no retaining here)                                */
/* ------------------------------------------------------------------------- */

expr_t *expr_new_unary_internal(const expr_ops_t *ops, const expr_t *a)
{
    expr_t *expr = expr_alloc(ops);
    expr->a = (expr_t *)a;
    return expr;
}

expr_t *expr_new_binary_internal(const expr_ops_t *ops, const expr_t *a, const expr_t *b)
{
    expr_t *expr = expr_alloc(ops);
    expr->a = (expr_t *)a;
    expr->b = (expr_t *)b;
    return expr;
}

expr_t *expr_new_pow_const_internal(const expr_t *a, number_t exponent)
{
    expr_t *expr = expr_alloc(&ops_pow_d);

    expr->a = (expr_t *)a;
    expr_store_const_num(expr, num_clone(exponent));
    return expr;
}

int expr_cmp(const expr_t *expr1, const expr_t *expr2)
{
    NUM_SCOPE(scope);
    number_t a = expr_eval_num_internal(expr1);
    number_t b = expr_eval_num_internal(expr2);
    number_t a_real = num_real_part(a);
    number_t b_real = num_real_part(b);
    int cmp = num_cmp(a_real, b_real);

    if (cmp == 0) {
        number_t a_imag = num_imag_part(a);
        number_t b_imag = num_imag_part(b);

        cmp = num_cmp(a_imag, b_imag);
    }
    return cmp;
}

/* ------------------------------------------------------------------------- */
/* Derivative creation (owning)                                              */
/* ------------------------------------------------------------------------- */

static size_t expr_count_log_linear_combination_terms(const expr_t *expr)
{
    if (!expr)
        return 0u;
    if (expr_is_op(expr, &ops_log))
        return 1u;
    if (expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_sub))
        return expr_count_log_linear_combination_terms(expr->a) + expr_count_log_linear_combination_terms(expr->b);
    if (expr_is_op(expr, &ops_neg))
        return expr_count_log_linear_combination_terms(expr->a);
    if (expr_is_op(expr, &ops_mul)) {
        if (expr_is_op(expr->a, &ops_const))
            return expr_count_log_linear_combination_terms(expr->b);
        if (expr_is_op(expr->b, &ops_const))
            return expr_count_log_linear_combination_terms(expr->a);
    }
    return 0u;
}

static expr_t *expr_create_log_linear_combination_deriv(const expr_t *expr, const expr_t *wrt)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;

    if (!expr)
        return NULL;
    if (expr_is_op(expr, &ops_log)) {
        expr_t *argument_deriv = expr_create_deriv(expr->a, wrt);

        out = argument_deriv ? expr_div(argument_deriv, expr->a) : NULL;
        expr_free(argument_deriv);
        return out;
    }
    if (expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_sub)) {
        left = expr_create_log_linear_combination_deriv(expr->a, wrt);
        right = expr_create_log_linear_combination_deriv(expr->b, wrt);
        out = (left && right) ? (expr_is_op(expr, &ops_add) ? expr_add(left, right) : expr_sub(left, right)) : NULL;
        expr_free(right);
        expr_free(left);
        return out;
    }
    if (expr_is_op(expr, &ops_neg)) {
        left = expr_create_log_linear_combination_deriv(expr->a, wrt);
        out = left ? expr_neg(left) : NULL;
        expr_free(left);
        return out;
    }
    if (expr_is_op(expr, &ops_mul)) {
        const expr_t *constant = NULL;
        const expr_t *dependent = NULL;

        if (expr_is_op(expr->a, &ops_const)) {
            constant = expr->a;
            dependent = expr->b;
        } else if (expr_is_op(expr->b, &ops_const)) {
            constant = expr->b;
            dependent = expr->a;
        }
        if (constant && dependent) {
            left = expr_create_log_linear_combination_deriv(dependent, wrt);
            out = left ? expr_mul(constant, left) : NULL;
            expr_free(left);
            return out;
        }
    }
    return NULL;
}

static expr_t *expr_create_deriv_impl(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt)
        return NULL;
    if (wrt->ops == &ops_const)
        return expr_nan_const_shared();
    if (expr_count_log_linear_combination_terms(expr) >= 4u) {
        expr_t *log_sum_deriv = expr_create_log_linear_combination_deriv(expr, wrt);

        if (log_sum_deriv)
            return log_sum_deriv;
    }
    {
        expr_t *source = expr_finite_weighted_sinh_from_lerch_form(expr);
        expr_t *special;

        if (!source)
            source = expr_finite_weighted_cosh_from_lerch_form(expr);
        if (source) {
            expr_t *closed_form;

            special = expr_create_deriv(source, wrt);
            expr_free(source);
            closed_form = special ? expr_finite_progression_closed_form(special) : NULL;
            if (closed_form) {
                expr_free(special);
                return closed_form;
            }
            return special;
        }
        special = expr_deriv_cosine_harmonic_antiderivative(expr, wrt);

        if (special)
            return special;
        special = expr_deriv_rational_over_polynomial_power(expr, wrt);

        if (special)
            return special;
    }
    const expr_t *saved_wrt = tl_wrt;
    tl_wrt = wrt;
    expr_t *raw = expr_build_dx((expr_t *)expr); /* borrowed */
    if (!raw) {
        tl_wrt = saved_wrt;
        return NULL;
    }
    expr_retain(raw); /* now owning */
    expr_t *simp = expr_simplify(raw);
    expr_free(raw);
    tl_wrt = saved_wrt;
    return simp;
}

/* Create an owning derivative whose symbols remain linked to the source bindings. */
expr_t *expr_create_deriv(const expr_t *expr, const expr_t *wrt)
{
    expr_t *result;
    expr_t *linked;

    if (!expr || !wrt)
        return NULL;
    if (expr_is_one_sided_log(expr)) {
        expr_t *variable = (expr_t *)wrt;
        return expr_new_formal_derivative(expr, 1u, &variable);
    }
    result = expr_create_deriv_impl(expr, wrt);
    linked = result ? expr_clone_linked_symbols(result, expr) : NULL;
    expr_free(result);
    return linked;
}

expr_t *expr_create_2nd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2)
{
    expr_t *g = expr_create_deriv(expr, wrt1);
    if (!g)
        return NULL;
    expr_t *h = expr_create_deriv(g, wrt2);
    expr_free(g);
    return h;
}

expr_t *expr_create_3rd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2, const expr_t *wrt3)
{
    expr_t *g = expr_create_deriv(expr, wrt1);
    if (!g)
        return NULL;
    expr_t *h = expr_create_deriv(g, wrt2);
    expr_free(g);
    if (!h)
        return NULL;
    expr_t *k = expr_create_deriv(h, wrt3);
    expr_free(h);
    return k;
}

expr_t *expr_create_nth_deriv(unsigned int n, const expr_t *expr, const expr_t *wrt)
{
    const expr_t *cur = expr;
    while (n--) {
        expr_t *next = expr_create_deriv(cur, wrt);
        if (cur != expr)
            expr_free((expr_t *)cur);
        cur = next;
        if (!cur)
            break;
    }
    return (expr_t *)cur;
}
