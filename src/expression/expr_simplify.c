/* expr_simplify.c - algebraic simplification of differentiable value nodes
 *
 * expr_simplify() rewrites a DAG node into a canonical form using a small set
 * of structural rules applied bottom-up.  It is called automatically by
 * expr_create_deriv() so that derivative expressions stay readable.
 *
 * Rules applied (in order):
 *   1. Constant folding — a sub-tree with no variable leaves is replaced by
 *      a single const node holding the evaluated value.
 *   2. Identity removal — x+0, x*1, x^1, neg(neg(x)), etc. are collapsed.
 *   3. Multiplication flattening — a chain of mul/neg nodes is collected into
 *      (scalar_coefficient * term₁ * term₂ * ...) with the coefficient folded
 *      into a single leading const node.
 *   4. Like-term collection in additions — terms with the same symbolic
 *      structure have their coefficients summed, e.g. 2x + 3x → 5x.
 *
 * expr_simplify() returns a new owning node (refcount = 1).  The input node is
 * borrowed; its refcount is not changed.
 */

#include "ustring.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

/* forward declaration — helpers below call expr_simplify recursively */
expr_t *expr_simplify(const expr_t *dv);
static expr_t *expr_simplify_mark_current(expr_t *dv);
static expr_t *expr_simplify_try_const_over_e_local(expr_t *a, expr_t *b);
static number_t *expr_simplify_number_array_alloc_local(size_t n);
static void expr_simplify_number_array_free_local(number_t *values, size_t n);
static bool expr_simplify_collect_poly_local(const expr_t *expr, const expr_t *var, number_t *out, size_t n);
static expr_t *expr_simplify_build_flat_poly_local(const expr_t *var, const number_t *coeffs, size_t n);
static int expr_simplify_exact_sqrt_radicand_local(const expr_t *expr, number_t *radicand_out);
static expr_t *expr_simplify_try_rational_atan_denominator_local(const expr_t *numerator, const expr_t *denominator);

static void *expr_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    fprintf(stderr, "expr_simplify: out of memory\n");
    abort();
}

static inline expr_t *expr_new_const_owned_local(number_t value)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    expr_t *out = expr_new_const(value);

    return out;
}

static inline expr_t *expr_make_scaled_owned_local(number_t coeff, expr_t *base)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    expr_t *out = expr_make_scaled(coeff, base);

    return out;
}

static inline expr_t *expr_make_pow_like_owned_local(expr_t *base, number_t exponent)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    expr_t *out = expr_make_pow_like(base, exponent);

    return out;
}

static bool expr_simplify_is_literal_euler_const_local(const expr_t *dv)
{
    const char *canon;

    if (!expr_is_op(dv, &ops_const) || !num_eq(dv->c, NUM_E))
        return false;

    if (!dv->binding_expr)
        return true;

    if (!dv->name || !*dv->name)
        return false;

    canon = expr_default_constant_canonical_name(dv->name);
    return canon && strcmp(canon, "e") == 0;
}

static bool expr_simplify_is_literal_ten_const_local(const expr_t *dv)
{
    if (!expr_is_op(dv, &ops_const) || !num_eq(dv->c, NUM_TEN))
        return false;
    if (dv->name && *dv->name)
        return false;
    return !dv->binding_expr || dv->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER;
}

static bool expr_simplify_is_i_const_local(const expr_t *dv)
{
    return dv && expr_is_op(dv, &ops_const) && num_eq(dv->c, NUM_I);
}

static bool expr_simplify_is_i_times_unary_local(const expr_t *dv, const expr_ops_t *ops, const expr_t **arg_out)
{
    const expr_t *candidate;

    if (!dv || !arg_out || !expr_is_op(dv, &ops_mul))
        return false;

    if (expr_simplify_is_i_const_local(dv->a)) {
        candidate = dv->b;
    } else if (expr_simplify_is_i_const_local(dv->b)) {
        candidate = dv->a;
    } else {
        return false;
    }

    if (!expr_is_op(candidate, ops))
        return false;

    *arg_out = candidate->a;
    return true;
}

static bool expr_simplify_is_euler_sum_local(const expr_t *dv, const expr_t **arg_out)
{
    const expr_t *cos_arg = NULL;
    const expr_t *sin_arg = NULL;

    if (!dv || !arg_out || !expr_is_op(dv, &ops_add))
        return false;

    if (expr_is_op(dv->a, &ops_cos) && expr_simplify_is_i_times_unary_local(dv->b, &ops_sin, &sin_arg)) {
        cos_arg = dv->a->a;
    } else if (expr_is_op(dv->b, &ops_cos) && expr_simplify_is_i_times_unary_local(dv->a, &ops_sin, &sin_arg)) {
        cos_arg = dv->b->a;
    } else {
        return false;
    }

    if (!expr_struct_eq(cos_arg, sin_arg))
        return false;

    *arg_out = cos_arg;
    return true;
}

static expr_t *expr_simplify_try_euler_square_local(expr_t *base)
{
    const expr_t *arg = NULL;
    expr_t *two;
    expr_t *raw_double_arg;
    expr_t *double_arg;
    expr_t *cos_term;
    expr_t *sin_term;
    expr_t *i_term;
    expr_t *imag_term;
    expr_t *raw;
    expr_t *out;

    if (!expr_simplify_is_euler_sum_local(base, &arg))
        return NULL;

    two = expr_new_const(NUM_TWO);
    raw_double_arg = expr_mul(two, arg);
    double_arg = expr_simplify(raw_double_arg);
    cos_term = expr_cos(double_arg);
    sin_term = expr_sin(double_arg);
    i_term = expr_new_named_const(NUM_I, "i");
    imag_term = expr_mul(i_term, sin_term);
    raw = expr_add(cos_term, imag_term);
    out = expr_simplify(raw);

    expr_free(raw);
    expr_free(imag_term);
    expr_free(i_term);
    expr_free(sin_term);
    expr_free(cos_term);
    expr_free(double_arg);
    expr_free(raw_double_arg);
    expr_free(two);
    expr_free(base);
    return out;
}

static int binding_expr_power_base_local(const expr_binding_expr_t *expr, expr_binding_expr_t **base_out,
                                         number_t *exponent_out);
static expr_t *expr_from_preserved_binding_expr_local(expr_binding_expr_t *expr);

typedef struct {
    expr_t *base;
    number_t exponent;
} expr_factor_t;

static void expr_free_factors_local(expr_factor_t *factors, size_t n)
{
    size_t i;

    for (i = 0u; i < n; ++i) {
        expr_free(factors[i].base);
        num_destroy(&factors[i].exponent);
    }
    free(factors);
}

static void expr_append_factor_local(expr_factor_t **factors, size_t *n, size_t *cap, const expr_t *base,
                                     number_t exponent)
{
    size_t i;

    for (i = 0u; i < *n; ++i) {
        if (expr_struct_eq((*factors)[i].base, base)) {
            number_t sum = num_add((*factors)[i].exponent, exponent);

            num_destroy(&(*factors)[i].exponent);
            (*factors)[i].exponent = num_scope_detach(sum);
            return;
        }
    }

    if (*n == *cap) {
        *cap = *cap ? *cap * 2u : 8u;
        *factors = expr_xrealloc(*factors, *cap * sizeof(**factors));
    }

    expr_retain(base);
    (*factors)[*n].base = (expr_t *)base;
    (*factors)[*n].exponent = num_scope_detach(num_clone(exponent));
    ++(*n);
}

static void expr_split_product_factors_scaled_local(const expr_t *dv, number_t sign, bool split_symbolic_scalar_power,
                                                    expr_factor_t **factors, size_t *n, size_t *cap)
{
    if (!dv)
        return;
    if (expr_is_op(dv, &ops_mul)) {
        expr_split_product_factors_scaled_local(dv->a, sign, split_symbolic_scalar_power, factors, n, cap);
        expr_split_product_factors_scaled_local(dv->b, sign, split_symbolic_scalar_power, factors, n, cap);
        return;
    }
    if (expr_is_div(dv)) {
        number_t neg_sign = num_neg(sign);

        expr_split_product_factors_scaled_local(dv->a, sign, split_symbolic_scalar_power, factors, n, cap);
        expr_split_product_factors_scaled_local(dv->b, neg_sign, split_symbolic_scalar_power, factors, n, cap);
        num_destroy(&neg_sign);
        return;
    }
    if (split_symbolic_scalar_power && expr_is_op(dv, &ops_pow) && dv->a && dv->b && expr_is_op(dv->a, &ops_mul)) {
        const expr_t *scalar = NULL;
        const expr_t *remainder = NULL;

        if (expr_simplify_is_plain_real_const(dv->a->a) && num_gt(dv->a->a->c, NUM_ZERO)) {
            scalar = dv->a->a;
            remainder = dv->a->b;
        } else if (expr_simplify_is_plain_real_const(dv->a->b) && num_gt(dv->a->b->c, NUM_ZERO)) {
            scalar = dv->a->b;
            remainder = dv->a->a;
        }
        if (scalar && remainder) {
            long numerator = 0L;
            long denominator = 0L;
            bool reciprocal_integer = num_get_small_rational(scalar->c, &numerator, &denominator) &&
                                      numerator == 1L && denominator > 1L;
            expr_t *scalar_base = reciprocal_integer ? expr_const_long(denominator) : (expr_t *)scalar;
            expr_t *scalar_power = expr_pow_xp(scalar_base, dv->b);
            expr_t *remainder_power = expr_pow_xp(remainder, dv->b);
            number_t scalar_sign = reciprocal_integer ? num_neg(sign) : num_clone(sign);

            expr_append_factor_local(factors, n, cap, scalar_power, scalar_sign);
            expr_append_factor_local(factors, n, cap, remainder_power, sign);
            num_destroy(&scalar_sign);
            expr_free(remainder_power);
            expr_free(scalar_power);
            if (reciprocal_integer)
                expr_free(scalar_base);
            return;
        }
    }
    if (expr_is_pow_d_expr(dv)) {
        number_t exponent = num_mul(dv->c, sign);

        expr_append_factor_local(factors, n, cap, dv->a, exponent);
        num_destroy(&exponent);
        return;
    }
    expr_append_factor_local(factors, n, cap, dv, sign);
}

static expr_t *expr_simplify_atan_tan_sawtooth(expr_t *inner)
{
    expr_t *arg;
    expr_t *pi_divisor;
    expr_t *arg_over_pi;
    expr_t *half;
    expr_t *shifted;
    expr_t *floored;
    expr_t *pi_multiplier;
    expr_t *periods;
    expr_t *out;

    if (!inner || !expr_is_op(inner, &ops_tan))
        return NULL;

    expr_retain(inner->a);
    arg = inner->a;

    pi_divisor = expr_new_named_const(NUM_PI, "@pi");
    arg_over_pi = expr_div(arg, pi_divisor);
    half = expr_new_const(NUM_HALF);
    shifted = expr_add(arg_over_pi, half);
    floored = expr_floor(shifted);
    pi_multiplier = expr_new_named_const(NUM_PI, "@pi");
    periods = expr_mul(pi_multiplier, floored);
    out = expr_sub(arg, periods);

    expr_free(periods);
    expr_free(pi_multiplier);
    expr_free(floored);
    expr_free(shifted);
    expr_free(half);
    expr_free(arg_over_pi);
    expr_free(pi_divisor);
    expr_free(arg);
    expr_free(inner);

    return out;
}

static int expr_is_pi_const_local(const expr_t *dv)
{
    return dv && expr_is_op(dv, &ops_const) && num_eq(dv->c, NUM_PI);
}

static int expr_is_pi_times_floor_local(const expr_t *dv)
{
    const expr_t *left;
    const expr_t *right;

    if (!dv || !expr_is_op(dv, &ops_mul))
        return 0;

    left = dv->a;
    right = dv->b;

    return (expr_is_pi_const_local(left) && expr_is_op(right, &ops_floor)) ||
           (expr_is_pi_const_local(right) && expr_is_op(left, &ops_floor));
}

static int expr_collect_pi_floor_product_local(const expr_t *dv, number_t *coeff, int *has_pi, int *has_floor)
{
    if (!dv)
        return 0;

    if (expr_is_op(dv, &ops_neg)) {
        number_t negated;

        if (!expr_collect_pi_floor_product_local(dv->a, coeff, has_pi, has_floor))
            return 0;
        negated = num_neg(*coeff);
        num_destroy(coeff);
        *coeff = negated;
        return 1;
    }

    if (expr_is_op(dv, &ops_mul))
        return expr_collect_pi_floor_product_local(dv->a, coeff, has_pi, has_floor) &&
               expr_collect_pi_floor_product_local(dv->b, coeff, has_pi, has_floor);

    if (expr_is_op(dv, &ops_const) && !dv->binding_expr && (!dv->name || !*dv->name) && num_is_real(dv->c)) {
        number_t product = num_mul(*coeff, dv->c);

        num_destroy(coeff);
        *coeff = product;
        return 1;
    }

    if (expr_is_pi_const_local(dv) && !*has_pi) {
        *has_pi = 1;
        return 1;
    }

    if (expr_is_op(dv, &ops_floor) && !*has_floor) {
        *has_floor = 1;
        return 1;
    }

    return 0;
}

static int expr_is_pos_pi_times_floor_local(const expr_t *dv)
{
    NUM_SCOPE(scope);
    number_t coeff = num_const(NUM_ONE);
    int has_pi = 0;
    int has_floor = 0;
    int ok;

    if (expr_is_pi_times_floor_local(dv))
        return 1;

    ok = expr_collect_pi_floor_product_local(dv, &coeff, &has_pi, &has_floor) && has_pi && has_floor &&
         num_eq(coeff, NUM_ONE);
    num_destroy(&coeff);
    return ok;
}

static int expr_is_period_pi_base_local(const expr_t *dv)
{
    return expr_is_pi_const_local(dv) || expr_is_pos_pi_times_floor_local(dv);
}

static void expr_free_addends_local(addend_t *terms, size_t n)
{
    size_t i;

    for (i = 0u; i < n; ++i) {
        expr_free(terms[i].base);
        num_destroy(&terms[i].coeff);
    }
    free(terms);
}

static expr_t *expr_tan_periodic_base_local(expr_t *arg)
{
    number_t c_const = num_const(NUM_ZERO);
    addend_t *terms = NULL;
    size_t n = 0u;
    size_t cap = 0u;
    expr_t *base = NULL;
    int have_base = 0;
    int have_period = 0;
    size_t i;

    expr_collect_addends(arg, NUM_ONE, &c_const, &terms, &n, &cap);

    if (!num_is_zero(c_const))
        goto fail;

    for (i = 0u; i < n; ++i) {
        if (num_is_zero(terms[i].coeff))
            continue;

        if (expr_is_period_pi_base_local(terms[i].base) && num_is_integer(terms[i].coeff)) {
            have_period = 1;
            continue;
        }

        if (!num_eq(terms[i].coeff, NUM_ONE) || have_base)
            goto fail;

        expr_retain(terms[i].base);
        base = terms[i].base;
        have_base = 1;
    }

    if (!have_period)
        goto fail;

    expr_free_addends_local(terms, n);
    num_destroy(&c_const);
    return base ? base : expr_new_const(NUM_ZERO);

fail:
    if (base)
        expr_free(base);
    expr_free_addends_local(terms, n);
    num_destroy(&c_const);
    return NULL;
}

static expr_t *expr_try_simplify_tan_period_floor(expr_t *arg)
{
    expr_t *base;
    expr_t *out;

    base = expr_tan_periodic_base_local(arg);
    if (!base)
        return NULL;

    out = expr_tan(base);
    expr_free(base);
    expr_free(arg);
    return out;
}

static expr_t *expr_rebuild_factors_local(expr_factor_t *factors, size_t n)
{
    expr_t **num_terms = NULL;
    expr_t **den_terms = NULL;
    size_t nnum = 0u;
    size_t nden = 0u;
    size_t num_cap = 0u;
    size_t den_cap = 0u;
    expr_t *numerator;
    expr_t *denominator;
    expr_t *out;
    size_t i;

    for (i = 0u; i < n; ++i) {
        expr_t *factor;

        if (num_is_zero(factors[i].exponent)) {
            expr_free(factors[i].base);
            num_destroy(&factors[i].exponent);
            continue;
        }

        if (num_lt(factors[i].exponent, NUM_ZERO)) {
            number_t den_exponent = num_neg(factors[i].exponent);

            factor = expr_make_pow_like_owned_local(factors[i].base, den_exponent);
            expr_append_node(&den_terms, &nden, &den_cap, factor);
        } else {
            factor = expr_make_pow_like_owned_local(factors[i].base, factors[i].exponent);
            expr_append_node(&num_terms, &nnum, &num_cap, factor);
        }
        num_destroy(&factors[i].exponent);
    }

    free(factors);
    numerator = expr_rebuild_product_chain(NUM_ONE, num_terms, nnum);
    if (nden == 0u) {
        free(den_terms);
        return numerator;
    }

    denominator = expr_rebuild_division_chain(den_terms, nden);
    out = expr_div(numerator, denominator);
    expr_free(numerator);
    expr_free(denominator);
    return out;
}

static void expr_keep_common_factors_local(expr_factor_t *common, size_t *ncommon, const expr_factor_t *factors,
                                           size_t n)
{
    size_t i;

    for (i = 0u; i < *ncommon; ++i) {
        size_t j;
        int found = 0;

        for (j = 0u; j < n; ++j) {
            if (!expr_struct_eq(common[i].base, factors[j].base))
                continue;

            if (num_lt(factors[j].exponent, common[i].exponent)) {
                num_destroy(&common[i].exponent);
                common[i].exponent = num_clone(factors[j].exponent);
            }
            found = 1;
            break;
        }

        if (!found) {
            num_destroy(&common[i].exponent);
            common[i].exponent = num_clone(NUM_ZERO);
        }
    }
}

static int expr_common_factors_nonempty_local(const expr_factor_t *factors, size_t n)
{
    size_t i;

    for (i = 0u; i < n; ++i) {
        if (!num_is_zero(factors[i].exponent))
            return 1;
    }
    return 0;
}

static int expr_common_factors_useful_local(const expr_factor_t *factors, size_t n)
{
    size_t i;
    int useful = 0;

    for (i = 0u; i < n; ++i) {
        if (num_is_zero(factors[i].exponent))
            continue;
        if (num_lt(factors[i].exponent, NUM_ZERO)) {
            if ((!expr_is_op(factors[i].base, &ops_pow) || !factors[i].base->a ||
                 !expr_simplify_is_plain_real_const(factors[i].base->a) ||
                 !num_eq(factors[i].base->a->c, NUM_TWO)) &&
                !expr_simplify_is_plain_real_const(factors[i].base)) {
                return 0;
            }
            useful = 1;
        } else if (!expr_is_op(factors[i].base, &ops_var)) {
            useful = 1;
        }
    }
    return useful;
}

static expr_t *expr_reduce_by_common_factors_local(const expr_t *base, const expr_factor_t *common, size_t ncommon,
                                                   bool split_symbolic_scalar_power)
{
    expr_factor_t *factors = NULL;
    size_t n = 0u;
    size_t cap = 0u;
    size_t i;

    expr_split_product_factors_scaled_local(base, NUM_ONE, split_symbolic_scalar_power, &factors, &n, &cap);

    for (i = 0u; i < ncommon; ++i) {
        size_t j;

        if (num_is_zero(common[i].exponent))
            continue;

        for (j = 0u; j < n; ++j) {
            number_t diff;

            if (!expr_struct_eq(factors[j].base, common[i].base))
                continue;

            diff = num_sub(factors[j].exponent, common[i].exponent);
            num_destroy(&factors[j].exponent);
            factors[j].exponent = diff;
            break;
        }
    }

    return expr_rebuild_factors_local(factors, n);
}

static int expr_factor_values_close_local(number_t before, number_t after)
{
    number_t delta;
    number_t abs_delta;
    number_t abs_before;
    number_t abs_after;
    number_t scale;
    number_t tolerance;
    number_t scaled_tolerance;
    int ok;

    if (num_is_nan(before) && num_is_nan(after))
        return 1;
    if (num_is_nan(before) || num_is_nan(after))
        return 0;
    if (num_eq(before, after))
        return 1;
    if (!num_is_finite(before) || !num_is_finite(after))
        return 0;

    delta = num_sub(before, after);
    abs_delta = num_abs(delta);
    abs_before = num_abs(before);
    abs_after = num_abs(after);
    scale = num_clone(NUM_ONE);
    if (num_gt(abs_before, scale)) {
        num_destroy(&scale);
        scale = num_clone(abs_before);
    }
    if (num_gt(abs_after, scale)) {
        num_destroy(&scale);
        scale = num_clone(abs_after);
    }
    tolerance = num_create_from_string("1e-24");
    scaled_tolerance = num_mul(tolerance, scale);
    ok = num_le(abs_delta, scaled_tolerance);

    num_destroy(&scaled_tolerance);
    num_destroy(&tolerance);
    num_destroy(&scale);
    num_destroy(&abs_after);
    num_destroy(&abs_before);
    num_destroy(&abs_delta);
    num_destroy(&delta);
    return ok;
}

static int expr_factor_candidate_preserves_value_local(const expr_t *before, const expr_t *after)
{
    number_t before_value;
    number_t after_value;
    int ok;

    if (!before || !after)
        return 0;

    before_value = expr_eval_num_internal(before);
    after_value = expr_eval_num_internal(after);
    ok = expr_factor_values_close_local(before_value, after_value);
    return ok;
}

static expr_t *expr_try_factor_common_symbolic_product_mode_local(expr_t *sum, bool split_symbolic_scalar_power)
{
    number_t c_const = num_const(NUM_ZERO);
    addend_t *terms = NULL;
    size_t n = 0u;
    size_t cap = 0u;
    expr_factor_t *common = NULL;
    size_t ncommon = 0u;
    size_t common_cap = 0u;
    expr_t *inner = NULL;
    expr_t *common_factor;
    expr_t *factored;
    size_t i;
    size_t nonzero = 0u;

    expr_collect_addends(sum, NUM_ONE, &c_const, &terms, &n, &cap);
    if (!num_is_zero(c_const))
        goto no_factor;

    for (i = 0u; i < n; ++i) {
        if (terms[i].base && !num_is_zero(terms[i].coeff))
            ++nonzero;
    }
    if (nonzero < 2u)
        goto no_factor;

    for (i = 0u; i < n; ++i) {
        expr_factor_t *factors = NULL;
        size_t nfactors = 0u;
        size_t fcap = 0u;

        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;

        expr_split_product_factors_scaled_local(terms[i].base, NUM_ONE, split_symbolic_scalar_power, &factors,
                                                &nfactors, &fcap);
        if (!common) {
            common = factors;
            ncommon = nfactors;
            common_cap = fcap;
            continue;
        }

        expr_keep_common_factors_local(common, &ncommon, factors, nfactors);
        expr_free_factors_local(factors, nfactors);
    }

    (void)common_cap;
    if (!expr_common_factors_nonempty_local(common, ncommon) || !expr_common_factors_useful_local(common, ncommon))
        goto no_factor;

    for (i = 0u; i < n; ++i) {
        expr_t *reduced;
        expr_t *term;

        if (!terms[i].base || num_is_zero(terms[i].coeff)) {
            if (terms[i].base)
                expr_free(terms[i].base);
            num_destroy(&terms[i].coeff);
            continue;
        }

        reduced = expr_reduce_by_common_factors_local(terms[i].base, common, ncommon, split_symbolic_scalar_power);
        if (reduced) {
            term = expr_make_scaled_owned_local(terms[i].coeff, reduced);
        } else {
            term = expr_new_const_owned_local(terms[i].coeff);
        }

        expr_free(terms[i].base);
        num_destroy(&terms[i].coeff);

        if (!inner) {
            inner = term;
        } else {
            expr_t *tmp = expr_add(inner, term);

            expr_free(inner);
            expr_free(term);
            inner = tmp;
        }
    }

    free(terms);
    num_destroy(&c_const);

    if (!inner) {
        expr_free_factors_local(common, ncommon);
        return NULL;
    }

    common_factor = expr_rebuild_factors_local(common, ncommon);
    if (!common_factor) {
        expr_free(inner);
        return NULL;
    }

    factored = expr_mul(common_factor, inner);
    expr_free(common_factor);
    expr_free(inner);
    if (!expr_factor_candidate_preserves_value_local(sum, factored)) {
        expr_free(factored);
        return NULL;
    }
    return factored;

no_factor:
    if (common)
        expr_free_factors_local(common, ncommon);
    for (i = 0u; i < n; ++i) {
        expr_free(terms[i].base);
        num_destroy(&terms[i].coeff);
    }
    free(terms);
    num_destroy(&c_const);
    return NULL;
}

static expr_t *expr_try_factor_common_symbolic_product_local(expr_t *sum)
{
    return expr_try_factor_common_symbolic_product_mode_local(sum, false);
}

static bool expr_product_has_half_factor_local(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_const(expr))
        return num_eq(expr->c, NUM_HALF);
    if (!expr_is_mul(expr))
        return false;
    return expr_product_has_half_factor_local(expr->a) || expr_product_has_half_factor_local(expr->b);
}

bool expr_contains_half_scaled_symbolic_power(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_op(expr, &ops_pow) && expr_product_has_half_factor_local(expr->a))
        return true;
    return expr_contains_half_scaled_symbolic_power(expr->a) || expr_contains_half_scaled_symbolic_power(expr->b);
}

expr_t *expr_factor_common_post_calculus(const expr_t *expr)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;
    expr_t *factored;

    if (!expr)
        return NULL;
    if (!expr_is_addsub(expr) && !expr_is_op(expr, &ops_mul) && !expr_is_div(expr)) {
        expr_retain((expr_t *)expr);
        return (expr_t *)expr;
    }

    left = expr_factor_common_post_calculus(expr->a);
    right = expr_factor_common_post_calculus(expr->b);
    if (!left || !right) {
        expr_free(right);
        expr_free(left);
        return NULL;
    }

    if (expr_is_op(expr, &ops_add))
        out = expr_add_simplify_owned(left, right);
    else if (expr_is_op(expr, &ops_sub))
        out = expr_sub_simplify_owned(left, right);
    else if (expr_is_op(expr, &ops_mul))
        out = expr_mul_simplify_owned(left, right);
    else
        out = expr_div_simplify_owned(left, right);

    if (!out || !expr_is_addsub(out))
        return out;
    factored = expr_try_factor_common_symbolic_product_mode_local(out, true);
    if (!factored)
        return out;
    expr_free(out);
    return factored;
}

static long expr_gcd_long_local(long a, long b)
{
    if (a < 0L)
        a = -a;
    if (b < 0L)
        b = -b;
    while (b != 0L) {
        long t = a % b;

        a = b;
        b = t;
    }
    return a;
}

static long expr_lcm_long_local(long a, long b)
{
    long g;

    if (a < 0L)
        a = -a;
    if (b < 0L)
        b = -b;
    if (a == 0L || b == 0L)
        return 0L;
    g = expr_gcd_long_local(a, b);
    return g ? (a / g) * b : 0L;
}

static int expr_number_small_rational_local(number_t value, long *numerator, long *denominator)
{
    return num_get_small_rational(value, numerator, denominator) ? 1 : 0;
}

static int expr_update_abs_coeff_rational_gcd_local(number_t abs_coeff, long *gcd_numer, long *lcm_denom)
{
    long numer;
    long denom;

    if (!expr_number_small_rational_local(abs_coeff, &numer, &denom))
        return 0;
    if (numer < 0L)
        numer = -numer;
    if (numer == 0L)
        return 1;
    *gcd_numer = *gcd_numer ? expr_gcd_long_local(*gcd_numer, numer) : numer;
    *lcm_denom = *lcm_denom ? expr_lcm_long_local(*lcm_denom, denom) : denom;
    return *lcm_denom != 0L;
}

static number_t expr_number_from_small_rational_local(long numerator, long denominator)
{
    if (denominator == 1L)
        return num_create_from_long(numerator);
    return num_create_from_frac(numerator, denominator);
}

static int expr_update_abs_coeff_gcd_local(number_t abs_coeff, long *gcd_value)
{
    if (!num_is_integer(abs_coeff))
        return 0;
    double d = num_to_double(abs_coeff);

    if (d < 1.0 || d > 1000000000.0)
        return 0;
    long value = (long)(d + 0.5);

    if ((double)value != d)
        return 0;
    *gcd_value = *gcd_value ? expr_gcd_long_local(*gcd_value, value) : value;
    return 1;
}

static int expr_try_common_abs_coeff_local(const addend_t *terms, size_t n, number_t c_const, number_t *out)
{
    number_t common = num_new();
    int have_common = 0;
    int equal_common = 1;
    int gcd_common = 1;
    int rational_common = 1;
    long gcd_value = 0L;
    long rational_gcd_numer = 0L;
    long rational_lcm_denom = 1L;
    size_t nonzero = 0u;
    size_t i;

    if (!num_is_zero(c_const)) {
        number_t abs_coeff = num_abs(c_const);

        num_destroy(&common);
        common = num_clone(abs_coeff);
        have_common = 1;
        if (!expr_update_abs_coeff_gcd_local(abs_coeff, &gcd_value))
            gcd_common = 0;
        if (!expr_update_abs_coeff_rational_gcd_local(abs_coeff, &rational_gcd_numer, &rational_lcm_denom))
            rational_common = 0;
        num_destroy(&abs_coeff);
        ++nonzero;
    }

    for (i = 0u; i < n; ++i) {
        number_t abs_coeff;

        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;

        abs_coeff = num_abs(terms[i].coeff);
        if (!have_common) {
            num_destroy(&common);
            common = num_clone(abs_coeff);
            have_common = 1;
        } else if (!num_eq(common, abs_coeff)) {
            equal_common = 0;
        }

        if (gcd_common && !expr_update_abs_coeff_gcd_local(abs_coeff, &gcd_value))
            gcd_common = 0;
        if (rational_common &&
            !expr_update_abs_coeff_rational_gcd_local(abs_coeff, &rational_gcd_numer, &rational_lcm_denom))
            rational_common = 0;
        num_destroy(&abs_coeff);
        ++nonzero;
    }

    if (!have_common || nonzero < 2u) {
        num_destroy(&common);
        return 0;
    }

    if (equal_common && !num_is_one(common)) {
        num_destroy(out);
        *out = common;
        return 1;
    }

    num_destroy(&common);
    if (rational_common && rational_gcd_numer > 0L && rational_lcm_denom > 0L) {
        common = expr_number_from_small_rational_local(rational_gcd_numer, rational_lcm_denom);
        if (!num_is_one(common)) {
            num_destroy(out);
            *out = common;
            return 1;
        }
        num_destroy(&common);
    }

    if (!gcd_common || gcd_value <= 1L)
        return 0;
    common = num_create_from_long(gcd_value);
    num_destroy(out);
    *out = common;
    return 1;
}

static bool expr_contains_var_local(const expr_t *dv)
{
    if (!dv)
        return false;
    if (expr_is_var(dv))
        return true;
    return expr_contains_var_local(dv->a) || expr_contains_var_local(dv->b);
}

static bool expr_is_numeric_arithmetic_const_local(const expr_t *dv)
{
    if (!expr_is_op(dv, &ops_const) || !num_is_finite(dv->c))
        return false;

    if (num_is_real(dv->c) && !num_is_exact(dv->c) && num_constant_name(dv->c) && !num_eq(dv->c, NUM_I) &&
        !num_eq(dv->c, NUM_NEG_I))
        return false;

    if (dv->name && *dv->name && !num_eq(dv->c, NUM_I) && !num_eq(dv->c, NUM_NEG_I))
        return false;

    if (!dv->binding_expr)
        return true;

    return !num_is_real(dv->c) || num_eq(dv->c, NUM_I) || num_eq(dv->c, NUM_NEG_I);
}

static bool expr_is_pure_numeric_arithmetic_local(const expr_t *dv)
{
    if (!dv)
        return false;

    if (expr_is_numeric_arithmetic_const_local(dv))
        return true;

    if (expr_is_op(dv, &ops_neg))
        return expr_is_pure_numeric_arithmetic_local(dv->a);

    if (expr_is_op(dv, &ops_add) || expr_is_op(dv, &ops_sub) || expr_is_op(dv, &ops_mul) || expr_is_op(dv, &ops_div))
        return expr_is_pure_numeric_arithmetic_local(dv->a) && expr_is_pure_numeric_arithmetic_local(dv->b);

    return false;
}

static expr_t *expr_try_fold_numeric_arithmetic_local(const expr_t *dv, expr_t *a, expr_t *b)
{
    expr_t *raw;
    number_t value;
    expr_t *out;

    if (!dv || !dv->ops->apply_binary || !expr_is_pure_numeric_arithmetic_local(a) ||
        !expr_is_pure_numeric_arithmetic_local(b))
        return NULL;

    raw = dv->ops->apply_binary(a, b);
    value = expr_eval(raw);
    expr_free(raw);

    if (!num_is_finite(value)) {
        num_destroy(&value);
        return NULL;
    }

    out = expr_new_const_owned_local(value);
    num_destroy(&value);
    return out;
}

static expr_t *expr_try_simplify_exp_quarter_turn(const expr_t *arg)
{
    NUM_SCOPE(scope);
    number_t arg_value;
    number_t exp_value;

    if (!arg || expr_contains_var_local(arg))
        return NULL;

    arg_value = expr_eval(arg);
    exp_value = num_exp(arg_value);

    if (num_eq(exp_value, NUM_I)) {
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return expr_new_named_const(NUM_I, "i");
    }
    if (num_eq(exp_value, NUM_NEG_ONE)) {
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return expr_new_const(NUM_NEG_ONE);
    }
    if (num_eq(exp_value, NUM_NEG_I)) {
        expr_t *i = expr_new_named_const(NUM_I, "i");
        expr_t *out = expr_neg(i);

        expr_free(i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return out;
    }
    if (num_eq(exp_value, NUM_ONE)) {
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return expr_new_const(NUM_ONE);
    }

    num_destroy(&exp_value);
    num_destroy(&arg_value);
    return NULL;
}

int expr_fold_zero_to_zero(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ZERO;
    return 1;
}

int expr_fold_cos_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ONE;
    return 1;
}

int expr_fold_asin_const(const number_t *in, number_t *out)
{
    number_t neg_half;
    number_t neg_sqrt2_over_two;
    number_t neg_sqrt3_over_two;

    if (!in || !out)
        return 0;
    neg_half = num_neg(NUM_HALF);
    neg_sqrt2_over_two = num_neg(NUM_SQRT2_OVER_TWO);
    neg_sqrt3_over_two = num_neg(NUM_SQRT3_OVER_TWO);
    if (num_eq(*in, NUM_NEG_ONE)) {
        *out = NUM_NEG_PI_2;
        goto matched;
    }
    if (num_eq(*in, neg_half)) {
        *out = num_neg(NUM_PI_6);
        goto matched;
    }
    if (num_eq(*in, neg_sqrt2_over_two)) {
        *out = num_neg(NUM_PI_4);
        goto matched;
    }
    if (num_eq(*in, neg_sqrt3_over_two)) {
        *out = num_neg(NUM_PI_3);
        goto matched;
    }
    if (num_eq(*in, NUM_ZERO)) {
        *out = NUM_ZERO;
        goto matched;
    }
    if (num_eq(*in, NUM_HALF)) {
        *out = NUM_PI_6;
        goto matched;
    }
    if (num_eq(*in, NUM_SQRT2_OVER_TWO)) {
        *out = NUM_PI_4;
        goto matched;
    }
    if (num_eq(*in, NUM_SQRT3_OVER_TWO)) {
        *out = NUM_PI_3;
        goto matched;
    }
    if (num_eq(*in, NUM_ONE)) {
        *out = NUM_PI_2;
        goto matched;
    }
    num_destroy(&neg_sqrt3_over_two);
    num_destroy(&neg_sqrt2_over_two);
    num_destroy(&neg_half);
    return 0;

matched:
    num_destroy(&neg_sqrt3_over_two);
    num_destroy(&neg_sqrt2_over_two);
    num_destroy(&neg_half);
    return 1;
}

int expr_inverse_trig_exact_pi_ratio(const expr_ops_t *ops, const number_t *in, long *numer_out,
                                     unsigned long *denom_out)
{
    number_t three;
    number_t neg_half;
    number_t neg_sqrt2_over_two;
    number_t neg_sqrt3_over_two;
    number_t sqrt3_over_three;
    number_t neg_sqrt3_over_three;
    number_t two_sqrt3;
    number_t two_sqrt3_over_three;
    number_t neg_two_sqrt3_over_three;
    number_t neg_sqrt3;
    number_t neg_sqrt2;
    number_t neg_two;
    int matched = 0;
    long numer = 0L;
    unsigned long denom = 1u;

    if (!in || !ops || !numer_out || !denom_out)
        return 0;

    three = num_create_from_long(3L);
    neg_half = num_neg(NUM_HALF);
    neg_sqrt2_over_two = num_neg(NUM_SQRT2_OVER_TWO);
    neg_sqrt3_over_two = num_neg(NUM_SQRT3_OVER_TWO);
    sqrt3_over_three = num_div(NUM_SQRT3, three);
    neg_sqrt3_over_three = num_neg(sqrt3_over_three);
    two_sqrt3 = num_mul(NUM_TWO, NUM_SQRT3);
    two_sqrt3_over_three = num_div(two_sqrt3, three);
    neg_two_sqrt3_over_three = num_neg(two_sqrt3_over_three);
    neg_sqrt3 = num_neg(NUM_SQRT3);
    neg_sqrt2 = num_neg(NUM_SQRT2);
    neg_two = num_neg(NUM_TWO);

    if (ops == &ops_asin) {
        if (num_eq(*in, NUM_NEG_ONE)) {
            numer = -1L;
            denom = 2u;
            matched = 1;
        } else if (num_eq(*in, neg_half)) {
            numer = -1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt2_over_two)) {
            numer = -1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt3_over_two)) {
            numer = -1L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, NUM_ZERO)) {
            numer = 0L;
            denom = 1u;
            matched = 1;
        } else if (num_eq(*in, NUM_HALF)) {
            numer = 1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT2_OVER_TWO)) {
            numer = 1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT3_OVER_TWO)) {
            numer = 1L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, NUM_ONE)) {
            numer = 1L;
            denom = 2u;
            matched = 1;
        }
    } else if (ops == &ops_acos) {
        if (num_eq(*in, NUM_ONE)) {
            numer = 0L;
            denom = 1u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT3_OVER_TWO)) {
            numer = 1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT2_OVER_TWO)) {
            numer = 1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, NUM_HALF)) {
            numer = 1L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, NUM_ZERO)) {
            numer = 1L;
            denom = 2u;
            matched = 1;
        } else if (num_eq(*in, neg_half)) {
            numer = 2L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt2_over_two)) {
            numer = 3L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt3_over_two)) {
            numer = 5L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_NEG_ONE)) {
            numer = 1L;
            denom = 1u;
            matched = 1;
        }
    } else if (ops == &ops_atan) {
        if (num_eq(*in, neg_sqrt3)) {
            numer = -1L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, NUM_NEG_ONE)) {
            numer = -1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt3_over_three)) {
            numer = -1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_ZERO)) {
            numer = 0L;
            denom = 1u;
            matched = 1;
        } else if (num_eq(*in, sqrt3_over_three)) {
            numer = 1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_ONE)) {
            numer = 1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT3)) {
            numer = 1L;
            denom = 3u;
            matched = 1;
        }
    } else if (ops == &ops_asec) {
        if (num_eq(*in, NUM_ONE)) {
            numer = 0L;
            denom = 1u;
            matched = 1;
        } else if (num_eq(*in, two_sqrt3_over_three)) {
            numer = 1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT2)) {
            numer = 1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, NUM_TWO)) {
            numer = 1L;
            denom = 3u;
            matched = 1;
        } else if (num_is_inf(*in) && num_get_sign(*in) > 0) {
            numer = 1L;
            denom = 2u;
            matched = 1;
        } else if (num_eq(*in, NUM_NEG_ONE)) {
            numer = 1L;
            denom = 1u;
            matched = 1;
        } else if (num_eq(*in, neg_two_sqrt3_over_three)) {
            numer = 5L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt2)) {
            numer = 3L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, neg_two)) {
            numer = 2L;
            denom = 3u;
            matched = 1;
        } else if (num_is_inf(*in) && num_get_sign(*in) < 0) {
            numer = 1L;
            denom = 2u;
            matched = 1;
        }
    } else if (ops == &ops_acosec) {
        if (num_eq(*in, NUM_NEG_ONE)) {
            numer = -1L;
            denom = 2u;
            matched = 1;
        } else if (num_eq(*in, neg_two_sqrt3_over_three)) {
            numer = -1L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt2)) {
            numer = -1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, neg_two)) {
            numer = -1L;
            denom = 6u;
            matched = 1;
        } else if (num_is_inf(*in) && num_get_sign(*in) < 0) {
            numer = 0L;
            denom = 1u;
            matched = 1;
        } else if (num_is_inf(*in) && num_get_sign(*in) > 0) {
            numer = 0L;
            denom = 1u;
            matched = 1;
        } else if (num_eq(*in, NUM_TWO)) {
            numer = 1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT2)) {
            numer = 1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, two_sqrt3_over_three)) {
            numer = 1L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, NUM_ONE)) {
            numer = 1L;
            denom = 2u;
            matched = 1;
        }
    } else if (ops == &ops_acot) {
        if (num_is_inf(*in) && num_get_sign(*in) > 0) {
            numer = 0L;
            denom = 1u;
            matched = 1;
        } else if (num_eq(*in, NUM_SQRT3)) {
            numer = 1L;
            denom = 6u;
            matched = 1;
        } else if (num_eq(*in, NUM_ONE)) {
            numer = 1L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, sqrt3_over_three)) {
            numer = 1L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, NUM_ZERO)) {
            numer = 1L;
            denom = 2u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt3_over_three)) {
            numer = 2L;
            denom = 3u;
            matched = 1;
        } else if (num_eq(*in, NUM_NEG_ONE)) {
            numer = 3L;
            denom = 4u;
            matched = 1;
        } else if (num_eq(*in, neg_sqrt3)) {
            numer = 5L;
            denom = 6u;
            matched = 1;
        } else if (num_is_inf(*in) && num_get_sign(*in) < 0) {
            numer = 1L;
            denom = 1u;
            matched = 1;
        }
    }

    num_destroy(&neg_two);
    num_destroy(&neg_sqrt2);
    num_destroy(&neg_sqrt3);
    num_destroy(&neg_two_sqrt3_over_three);
    num_destroy(&two_sqrt3_over_three);
    num_destroy(&two_sqrt3);
    num_destroy(&neg_sqrt3_over_three);
    num_destroy(&sqrt3_over_three);
    num_destroy(&neg_sqrt3_over_two);
    num_destroy(&neg_sqrt2_over_two);
    num_destroy(&neg_half);
    num_destroy(&three);

    if (!matched)
        return 0;

    *numer_out = numer;
    *denom_out = denom;
    return 1;
}

static int expr_fold_to_pi_ratio(long numer, unsigned long denom, number_t *out)
{
    number_t n;
    number_t d;
    number_t scaled;

    if (!out || denom == 0u)
        return 0;
    if (numer == 0L) {
        *out = NUM_ZERO;
        return 1;
    }

    n = num_create_from_long(numer);
    d = num_create_from_long((long)denom);
    scaled = num_mul(NUM_PI, n);
    *out = num_div(scaled, d);
    num_destroy(&scaled);
    num_destroy(&d);
    num_destroy(&n);
    return 1;
}

int expr_fold_acos_const(const number_t *in, number_t *out)
{
    long numer;
    unsigned long denom;

    return expr_inverse_trig_exact_pi_ratio(&ops_acos, in, &numer, &denom) && expr_fold_to_pi_ratio(numer, denom, out);
}

int expr_fold_atan_const(const number_t *in, number_t *out)
{
    long numer;
    unsigned long denom;

    return expr_inverse_trig_exact_pi_ratio(&ops_atan, in, &numer, &denom) && expr_fold_to_pi_ratio(numer, denom, out);
}

int expr_fold_asec_const(const number_t *in, number_t *out)
{
    long numer;
    unsigned long denom;

    return expr_inverse_trig_exact_pi_ratio(&ops_asec, in, &numer, &denom) && expr_fold_to_pi_ratio(numer, denom, out);
}

int expr_fold_acosec_const(const number_t *in, number_t *out)
{
    long numer;
    unsigned long denom;

    return expr_inverse_trig_exact_pi_ratio(&ops_acosec, in, &numer, &denom) &&
           expr_fold_to_pi_ratio(numer, denom, out);
}

int expr_fold_acot_const(const number_t *in, number_t *out)
{
    long numer;
    unsigned long denom;

    return expr_inverse_trig_exact_pi_ratio(&ops_acot, in, &numer, &denom) && expr_fold_to_pi_ratio(numer, denom, out);
}

int expr_fold_exp_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ONE;
    return 1;
}

int expr_fold_log_const(const number_t *in, number_t *out)
{
    if (!in || !out)
        return 0;
    if (num_eq(*in, NUM_ONE)) {
        *out = NUM_ZERO;
        return 1;
    }
    if (num_eq(*in, NUM_E)) {
        *out = NUM_ONE;
        return 1;
    }
    return 0;
}

static bool expr_long_perfect_square_root_local(long value, long *root_out)
{
    long lo = 0L;
    long hi = value;

    if (value < 0L || !root_out)
        return false;

    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2L;
        long square;

        if (mid != 0L && mid > value / mid) {
            hi = mid - 1L;
            continue;
        }

        square = mid * mid;
        if (square == value) {
            *root_out = mid;
            return true;
        }
        if (square < value)
            lo = mid + 1L;
        else
            hi = mid - 1L;
    }

    return false;
}

int expr_fold_sqrt_const(const number_t *in, number_t *out)
{
    long numerator;
    long denominator;
    long root_numerator;
    long root_denominator;
    number_t numerator_value;
    number_t denominator_value;
    number_t root;

    if (!in || !out)
        return 0;
    if (!num_get_small_rational(*in, &numerator, &denominator) || numerator < 0L || denominator <= 0L ||
        !expr_long_perfect_square_root_local(numerator, &root_numerator) ||
        !expr_long_perfect_square_root_local(denominator, &root_denominator) || root_denominator == 0L)
        return 0;

    numerator_value = num_create_from_long(root_numerator);
    denominator_value = num_create_from_long(root_denominator);
    root = num_div(numerator_value, denominator_value);
    num_destroy(&denominator_value);
    num_destroy(&numerator_value);
    num_destroy(out);
    *out = root;
    return 1;
}

int expr_fold_floor_const(const number_t *in, number_t *out)
{
    if (!in || !out)
        return 0;
    *out = num_floor(*in);
    return 1;
}

int expr_fold_erf_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_is_inf(*in))
        return 0;
    *out = num_get_sign(*in) < 0 ? NUM_NEG_ONE : NUM_ONE;
    return 1;
}

int expr_fold_erfc_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_is_inf(*in))
        return 0;
    *out = num_get_sign(*in) < 0 ? NUM_TWO : NUM_ZERO;
    return 1;
}

static expr_t *expr_try_simplify_preserved_i_power_local(const expr_binding_expr_t *base_expr, number_t exponent);

static bool expr_is_negative_real_power_local(const expr_t *dv)
{
    number_t exponent = num_new();
    bool out = false;

    if (expr_is_pow_d_expr(dv) && dv->a) {
        num_destroy(&exponent);
        exponent = num_clone(dv->c);
    } else if (expr_is_op(dv, &ops_pow) && dv->a && dv->b && expr_simplify_is_plain_real_const(dv->b)) {
        num_destroy(&exponent);
        exponent = num_clone(dv->b->c);
    } else {
        num_destroy(&exponent);
        return false;
    }

    out = num_is_real(exponent) && num_lt(exponent, NUM_ZERO);
    num_destroy(&exponent);
    return out;
}

static bool expr_collect_negative_power_as_reciprocal_local(expr_t *dv, int in_denominator, expr_t ***terms,
                                                            size_t *nterms, size_t *term_cap, expr_t ***den_terms,
                                                            size_t *nden_terms, size_t *den_cap)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    number_t positive_exponent;
    expr_t *factor;

    if (expr_is_pow_d_expr(dv) && dv->a) {
        base = dv->a;
        num_destroy(&exponent);
        exponent = num_clone(dv->c);
    } else if (expr_is_op(dv, &ops_pow) && dv->a && dv->b && expr_simplify_is_plain_real_const(dv->b)) {
        base = dv->a;
        num_destroy(&exponent);
        exponent = num_clone(dv->b->c);
    } else {
        num_destroy(&exponent);
        return false;
    }

    if (!num_is_real(exponent) || !num_lt(exponent, NUM_ZERO)) {
        num_destroy(&exponent);
        return false;
    }

    positive_exponent = num_neg(exponent);
    num_destroy(&exponent);

    expr_retain((expr_t *)base);
    factor = expr_make_pow_like_owned_local((expr_t *)base, positive_exponent);
    num_destroy(&positive_exponent);
    if (in_denominator)
        expr_append_node(terms, nterms, term_cap, factor);
    else
        expr_append_node(den_terms, nden_terms, den_cap, factor);
    return true;
}

/* ========================================================================= */
/* Multiplication flattening                                                  */
/* ========================================================================= */

static void collect_mul_flat(expr_t *dv, number_t *c_acc, int *is_zero, expr_t ***terms, size_t *nterms, size_t *cap)
{
    NUM_SCOPE(scope);
    if (*is_zero) {
        num_scope_leave(&(scope));
        return;
    }

    if (expr_is_op(dv, &ops_const) && (num_eq(dv->c, NUM_I) || num_eq(dv->c, NUM_NEG_I)) &&
        (!dv->binding_expr || dv->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER ||
         dv->binding_expr->kind == EXPR_BINDING_EXPR_CONST)) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t product = num_mul(*c_acc, dv->c);

        num_destroy(c_acc);
        *c_acc = product;
        num_scope_leave(&(scope));
        return;
    }

    if (expr_is_unnamed_const(dv) && (num_is_exact(dv->c) || !num_constant_name(dv->c)) &&
        (!dv->binding_expr || dv->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER)) {
        if (num_is_zero(dv->c)) {
            *is_zero = 1;
            num_scope_leave(&(scope));
            return;
        }
        {
            NUM_SCOPE_SUSPEND(saved_scope);
            number_t product = num_mul(*c_acc, dv->c);

            num_destroy(c_acc);
            *c_acc = product;
        }
        num_scope_leave(&(scope));
        return;
    }
    if (expr_is_unnamed_const(dv) && num_is_real(dv->c) && dv->binding_expr) {
        number_t coeff;
        expr_binding_expr_t *rest_expr = NULL;

        if (expr_binding_expr_split_leading_number(dv->binding_expr, &coeff, &rest_expr)) {
            if (num_is_zero(coeff)) {
                num_destroy(&coeff);
                expr_binding_expr_free(rest_expr);
                *is_zero = 1;
                num_scope_leave(&(scope));
                return;
            }
            {
                NUM_SCOPE_SUSPEND(saved_scope);
                number_t product = num_mul(*c_acc, coeff);

                num_destroy(c_acc);
                *c_acc = product;
            }
            num_destroy(&coeff);
            if (rest_expr) {
                expr_t *rest = expr_binding_expr_eval_expr(rest_expr);

                expr_binding_expr_free(rest_expr);
                if (*nterms == *cap) {
                    *cap = (*cap == 0 ? 4 : *cap * 2);
                    *terms = expr_xrealloc(*terms, *cap * sizeof(expr_t *));
                }
                (*terms)[(*nterms)++] = rest;
            }
            num_scope_leave(&(scope));
            return;
        }
    }
    if (expr_is_unnamed_const(dv) && dv->binding_expr && dv->binding_expr->kind == EXPR_BINDING_EXPR_DIV) {
        expr_t *expanded = expr_binding_expr_eval_expr(dv->binding_expr);

        if (expanded) {
            collect_mul_flat(expanded, c_acc, is_zero, terms, nterms, cap);
            expr_free(expanded);
            num_scope_leave(&(scope));
            return;
        }
    }
    if (expr_is_unnamed_const(dv) && dv->binding_expr) {
        expr_binding_expr_t *base_expr = NULL;
        number_t exponent;

        if (binding_expr_power_base_local(dv->binding_expr, &base_expr, &exponent)) {
            expr_t *i_power = expr_try_simplify_preserved_i_power_local(base_expr, exponent);

            if (i_power) {
                expr_binding_expr_free(base_expr);
                num_destroy(&exponent);
                collect_mul_flat(i_power, c_acc, is_zero, terms, nterms, cap);
                expr_free(i_power);
                num_scope_leave(&(scope));
                return;
            }

            expr_t *base = expr_from_preserved_binding_expr_local(base_expr);
            expr_t *powered = base ? expr_pow(base, &exponent) : NULL;

            num_destroy(&exponent);
            if (base)
                expr_free(base);
            if (powered) {
                collect_mul_flat(powered, c_acc, is_zero, terms, nterms, cap);
                expr_free(powered);
                num_scope_leave(&(scope));
                return;
            }
        }
    }
    {
        expr_t *positive = expr_simplify_positive_part_if_negative(dv);

        if (positive) {
            NUM_SCOPE_SUSPEND(saved_scope);
            number_t negated = num_neg(*c_acc);

            num_destroy(c_acc);
            *c_acc = negated;
            num_scope_leave(&(scope));
            collect_mul_flat(positive, c_acc, is_zero, terms, nterms, cap);
            expr_free(positive);
            return;
        }
    }
    if (expr_is_op(dv, &ops_neg)) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t negated = num_neg(*c_acc);

        num_destroy(c_acc);
        *c_acc = negated;
        num_scope_leave(&(scope));
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (expr_is_op(dv, &ops_mul)) {
        num_scope_leave(&(scope));
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        collect_mul_flat(dv->b, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (*nterms == *cap) {
        *cap = (*cap == 0 ? 4 : *cap * 2);
        *terms = expr_xrealloc(*terms, *cap * sizeof(expr_t *));
    }
    expr_retain(dv);
    (*terms)[(*nterms)++] = dv;
    num_scope_leave(&(scope));
}

static bool expr_is_exact_zero_node_local(const expr_t *dv)
{
    return dv == EXPR_ZERO || (expr_simplify_is_simplifiable_const(dv) && expr_const_is_zero(dv));
}

static expr_t *expr_try_simplify_i_power_local(expr_t *base, number_t exponent)
{
    long numerator;
    long denominator;

    if (!base || !expr_is_op(base, &ops_const))
        return NULL;
    if (base->binding_expr && base->binding_expr->kind != EXPR_BINDING_EXPR_NUMBER &&
        base->binding_expr->kind != EXPR_BINDING_EXPR_CONST)
        return NULL;
    if (!num_eq(base->c, NUM_I) && !num_eq(base->c, NUM_NEG_I))
        return NULL;
    if (!num_get_small_rational(exponent, &numerator, &denominator) || denominator != 1L)
        return NULL;

    {
        number_t folded = num_pow(base->c, exponent);
        expr_t *out = expr_new_const_owned_local(folded);

        expr_free(base);
        return out;
    }
}

static int binding_expr_power_base_local(const expr_binding_expr_t *expr, expr_binding_expr_t **base_out,
                                         number_t *exponent_out)
{
    number_t exponent_value;
    if (!expr || !base_out || !exponent_out)
        return 0;

    if (expr->kind == EXPR_BINDING_EXPR_POWI) {
        *base_out = expr_binding_expr_clone(expr->u.powi.base);
        *exponent_out = num_create_from_long(expr->u.powi.exponent);
        return 1;
    }

    if (expr->kind == EXPR_BINDING_EXPR_BINARY_OP && expr->u.binary_op.ops == &ops_pow &&
        expr_binding_expr_number_value(expr->u.binary_op.right, &exponent_value)) {
        if (!num_is_real(exponent_value) || !num_is_finite(exponent_value)) {
            num_destroy(&exponent_value);
            return 0;
        }

        *base_out = expr_binding_expr_clone(expr->u.binary_op.left);
        *exponent_out = num_clone(exponent_value);
        num_destroy(&exponent_value);
        return 1;
    }

    return 0;
}

static expr_t *expr_from_preserved_binding_expr_local(expr_binding_expr_t *expr)
{
    number_t value;
    expr_t *node;

    if (!expr)
        return NULL;

    expr = expr_binding_expr_simplify(expr);
    value = expr_binding_expr_eval(expr);
    node = expr_new_const(value);
    num_destroy(&value);
    node->binding_expr = expr;
    return node;
}

static expr_t *expr_try_simplify_preserved_integer_power_local(expr_t *base, number_t exponent)
{
    long numerator;
    long denominator;
    expr_binding_expr_t *pow_expr;
    expr_t *out;

    if (!expr_is_unnamed_const(base) || !base->binding_expr || base->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER ||
        !num_get_small_rational(exponent, &numerator, &denominator) || denominator != 1L)
        return NULL;

    pow_expr = expr_binding_expr_new_powi(expr_binding_expr_clone(base->binding_expr), numerator);
    out = expr_from_preserved_binding_expr_local(pow_expr);
    expr_free(base);
    return out;
}

static expr_t *expr_try_simplify_preserved_rational_const_power_local(expr_t *base, number_t exponent)
{
    long numerator;
    long denominator;
    expr_binding_expr_t *base_expr = NULL;
    expr_binding_expr_t *exponent_expr = NULL;
    expr_binding_expr_t *pow_expr = NULL;
    expr_t *out = NULL;
    string_t *base_text = NULL;
    string_t *exponent_text = NULL;

    if (!expr_simplify_is_plain_real_const(base) || !num_is_finite(base->c) || !num_gt(base->c, NUM_ZERO) ||
        !num_get_small_rational(exponent, &numerator, &denominator) || denominator == 1L) {
        return NULL;
    }

    if (base->binding_expr) {
        base_expr = expr_binding_expr_clone(base->binding_expr);
    } else {
        base_text = num_to_string(base->c);
        base_expr = expr_binding_expr_new_number_text(base_text ? string_c_str(base_text) : "NAN");
    }

    exponent_text = num_to_string(exponent);
    exponent_expr = expr_binding_expr_new_number_text(exponent_text ? string_c_str(exponent_text) : "NAN");
    pow_expr = base_expr && exponent_expr ? expr_binding_expr_new_binary_op(&ops_pow, base_expr, exponent_expr) : NULL;
    if (pow_expr) {
        base_expr = NULL;
        exponent_expr = NULL;
        out = expr_from_preserved_binding_expr_local(pow_expr);
    }

    expr_binding_expr_free(exponent_expr);
    expr_binding_expr_free(base_expr);
    string_free(exponent_text);
    string_free(base_text);
    if (out)
        expr_free(base);
    return out;
}

static expr_t *expr_try_simplify_preserved_i_power_local(const expr_binding_expr_t *base_expr, number_t exponent)
{
    expr_t *base;
    expr_t *out;

    if (!base_expr || base_expr->kind != EXPR_BINDING_EXPR_CONST || base_expr->u.const_id != EXPR_BINDING_CONST_I)
        return NULL;

    base = expr_new_const(NUM_I);
    out = expr_try_simplify_i_power_local(base, exponent);
    if (!out)
        expr_free(base);

    return out;
}

static void collect_quotient_flat(expr_t *dv, int in_denominator, number_t *c_acc, int *is_zero, expr_t ***terms,
                                  size_t *nterms, size_t *term_cap, expr_t ***den_terms, size_t *nden_terms,
                                  size_t *den_cap)
{
    NUM_SCOPE(scope);

    if (*is_zero) {
        num_scope_leave(&(scope));
        return;
    }

    if (expr_simplify_is_plain_real_const(dv) && (num_is_exact(dv->c) || !num_constant_name(dv->c))) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t next;

        if (num_is_zero(dv->c) && !in_denominator) {
            *is_zero = 1;
            num_scope_leave(&(scope));
            return;
        }

        next = in_denominator ? num_div(*c_acc, dv->c) : num_mul(*c_acc, dv->c);
        num_destroy(c_acc);
        *c_acc = next;
        num_scope_leave(&(scope));
        return;
    }

    if (expr_is_unnamed_const(dv) && num_is_real(dv->c) && dv->binding_expr) {
        expr_binding_expr_t *base_expr = NULL;
        number_t exponent;

        if (binding_expr_power_base_local(dv->binding_expr, &base_expr, &exponent)) {
            expr_t *base = expr_from_preserved_binding_expr_local(base_expr);
            expr_t *powered = base ? expr_pow(base, &exponent) : NULL;

            num_destroy(&exponent);
            if (base)
                expr_free(base);
            if (powered) {
                collect_quotient_flat(powered, in_denominator, c_acc, is_zero, terms, nterms, term_cap, den_terms,
                                      nden_terms, den_cap);
                expr_free(powered);
                num_scope_leave(&(scope));
                return;
            }
        }
    }

    if (expr_is_op(dv, &ops_neg)) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t negated = num_neg(*c_acc);

        num_destroy(c_acc);
        *c_acc = negated;
        num_scope_leave(&(scope));
        collect_quotient_flat(dv->a, in_denominator, c_acc, is_zero, terms, nterms, term_cap, den_terms, nden_terms,
                              den_cap);
        return;
    }

    if (expr_is_op(dv, &ops_mul)) {
        num_scope_leave(&(scope));
        collect_quotient_flat(dv->a, in_denominator, c_acc, is_zero, terms, nterms, term_cap, den_terms, nden_terms,
                              den_cap);
        collect_quotient_flat(dv->b, in_denominator, c_acc, is_zero, terms, nterms, term_cap, den_terms, nden_terms,
                              den_cap);
        return;
    }

    if (expr_is_div(dv)) {
        num_scope_leave(&(scope));
        collect_quotient_flat(dv->a, in_denominator, c_acc, is_zero, terms, nterms, term_cap, den_terms, nden_terms,
                              den_cap);
        collect_quotient_flat(dv->b, !in_denominator, c_acc, is_zero, terms, nterms, term_cap, den_terms, nden_terms,
                              den_cap);
        return;
    }

    if (expr_collect_negative_power_as_reciprocal_local(dv, in_denominator, terms, nterms, term_cap, den_terms,
                                                        nden_terms, den_cap)) {
        num_scope_leave(&(scope));
        return;
    }

    if (in_denominator) {
        expr_retain(dv);
        expr_append_node(den_terms, nden_terms, den_cap, dv);
    } else {
        expr_retain(dv);
        expr_append_node(terms, nterms, term_cap, dv);
    }

    num_scope_leave(&(scope));
}

static void expr_simplify_split_small_rational_quotient_coeff_local(number_t *c_acc, size_t nterms, expr_t ***den_terms,
                                                                    size_t *nden_terms, size_t *den_cap)
{
    long numerator;
    long denominator;
    bool should_split;
    number_t numerator_value;
    number_t denominator_value;
    expr_t *denominator_node;

    if (!c_acc || !den_terms || !nden_terms || !den_cap || *nden_terms == 0u) {
        return;
    }

    if (!num_get_small_rational(*c_acc, &numerator, &denominator) || denominator <= 1L) {
        return;
    }
    should_split = nterms == 0u;
    for (size_t i = 0u; !should_split && i < *nden_terms; ++i) {
        should_split = (numerator == 1L || numerator == -1L) && expr_is_sqrt_expr((*den_terms)[i]);
    }
    if (!should_split)
        return;

    numerator_value = num_create_from_long(numerator);
    denominator_value = num_create_from_long(denominator);
    denominator_node = expr_new_const(denominator_value);

    num_destroy(c_acc);
    *c_acc = numerator_value;
    expr_append_node(den_terms, nden_terms, den_cap, denominator_node);
    num_destroy(&denominator_value);
}

static expr_t *expr_simplify_radical_ratio_factor_local(const expr_t *numerator_factor,
                                                        const expr_t *denominator_factor)
{
    number_t square;
    number_t common_factor;
    number_t radicand;
    expr_t *radicand_expr;
    expr_t *raw_root;
    expr_t *ratio;
    expr_t *one;
    expr_t *inside;
    expr_t *out;
    bool add_ratio;

    if (!expr_is_addsub(numerator_factor) || !expr_is_sqrt_expr(numerator_factor->a) ||
        !expr_struct_eq(numerator_factor->a, denominator_factor) ||
        !expr_simplify_is_plain_real_const(numerator_factor->b) || !denominator_factor->a ||
        !expr_simplify_is_plain_real_const(denominator_factor->a))
        return NULL;

    square = num_mul(numerator_factor->b->c, numerator_factor->b->c);
    if (!num_is_integer(square) || !num_is_integer(denominator_factor->a->c)) {
        num_destroy(&square);
        return NULL;
    }
    common_factor = num_gcd(square, denominator_factor->a->c);
    if (!num_gt(common_factor, NUM_ONE)) {
        num_destroy(&common_factor);
        num_destroy(&square);
        return NULL;
    }
    num_destroy(&common_factor);

    radicand = num_div(square, denominator_factor->a->c);
    radicand_expr = expr_new_const(radicand);
    raw_root = radicand_expr ? expr_sqrt(radicand_expr) : NULL;
    ratio = raw_root ? expr_simplify(raw_root) : NULL;
    num_destroy(&radicand);
    num_destroy(&square);
    expr_free(raw_root);
    expr_free(radicand_expr);
    if (!ratio)
        return NULL;

    add_ratio = (expr_is_op(numerator_factor, &ops_add) && num_gt(numerator_factor->b->c, NUM_ZERO)) ||
                (expr_is_op(numerator_factor, &ops_sub) && num_lt(numerator_factor->b->c, NUM_ZERO));
    one = expr_new_const(NUM_ONE);
    inside = add_ratio ? expr_add(one, ratio) : expr_sub(one, ratio);
    expr_free(one);
    expr_free(ratio);
    out = inside ? expr_simplify(inside) : NULL;
    expr_free(inside);
    return out;
}

static expr_t *expr_simplify_flat_quotient_local(expr_t *a, expr_t *b)
{
    number_t c_acc = num_const(NUM_ONE);
    expr_t **terms = NULL;
    expr_t **den_terms = NULL;
    size_t nterms = 0u;
    size_t nden_terms = 0u;
    size_t term_cap = 0u;
    size_t den_cap = 0u;
    int is_zero = 0;
    expr_t *numerator;
    expr_t *denominator;
    expr_t *out;

    collect_quotient_flat(a, 0, &c_acc, &is_zero, &terms, &nterms, &term_cap, &den_terms, &nden_terms, &den_cap);
    collect_quotient_flat(b, 1, &c_acc, &is_zero, &terms, &nterms, &term_cap, &den_terms, &nden_terms, &den_cap);
    expr_free(a);
    expr_free(b);

    if (is_zero) {
        expr_free_node_array(terms, nterms);
        expr_free_node_array(den_terms, nden_terms);
        num_destroy(&c_acc);
        return expr_new_const(NUM_ZERO);
    }

    expr_combine_like_powers(terms, nterms);
    expr_combine_like_powers(den_terms, nden_terms);
    expr_cancel_common_powers(terms, nterms, den_terms, nden_terms);
    for (size_t i = 0u; i < nterms; ++i) {
        for (size_t j = 0u; j < nden_terms; ++j) {
            expr_t *replacement = expr_simplify_radical_ratio_factor_local(terms[i], den_terms[j]);

            if (!replacement)
                continue;
            expr_free(terms[i]);
            expr_free(den_terms[j]);
            terms[i] = replacement;
            den_terms[j] = NULL;
            break;
        }
    }
    for (size_t i = 0u; i < nterms; ++i) {
        expr_t *term = terms[i];

        if (!term || !expr_is_op(term, &ops_pow) || !term->a || !term->b)
            continue;

        for (size_t j = 0u; j < nden_terms; ++j) {
            expr_t *den = den_terms[j];
            expr_t *base;
            expr_t *exponent;
            expr_t *one;
            expr_t *shifted;
            expr_t *replacement;

            if (!den || !expr_struct_eq(term->a, den))
                continue;

            expr_retain(term->a);
            expr_retain(term->b);
            base = term->a;
            exponent = term->b;
            expr_free(term);
            expr_free(den);

            one = expr_new_const(NUM_ONE);
            shifted = expr_sub(exponent, one);
            expr_free(exponent);
            expr_free(one);

            replacement = shifted ? expr_pow_xp(base, shifted) : NULL;
            expr_free(base);
            expr_free(shifted);

            terms[i] = replacement;
            den_terms[j] = NULL;
            break;
        }
    }
    expr_combine_exp_terms(terms, nterms);
    expr_merge_sqrt_terms(terms, nterms);
    expr_merge_sqrt_terms(den_terms, nden_terms);
    expr_merge_coefficient_sqrt_terms(&c_acc, terms, nterms);
    expr_merge_sqrt_quotient_terms(terms, nterms, den_terms, nden_terms);

    expr_simplify_split_small_rational_quotient_coeff_local(&c_acc, nterms, &den_terms, &nden_terms, &den_cap);

    numerator = expr_rebuild_product_chain(c_acc, terms, nterms);
    num_destroy(&c_acc);

    if (nden_terms == 0u) {
        free(den_terms);
        return numerator;
    }

    denominator = expr_rebuild_division_chain(den_terms, nden_terms);
    if (!denominator)
        return numerator;
    out = expr_simplify_try_rational_atan_denominator_local(numerator, denominator);
    if (out) {
        expr_free(numerator);
        expr_free(denominator);
        return out;
    }
    out = expr_div(numerator, denominator);
    expr_free(numerator);
    expr_free(denominator);
    return out;
}

/* ========================================================================= */
/* Unary function simplification                                             */
/* ========================================================================= */

expr_t *expr_simplify_passthrough(const expr_t *dv, expr_t *a, expr_t *b)
{
    if (a)
        expr_free(a);
    if (b)
        expr_free(b);
    expr_retain((expr_t *)dv);
    return (expr_t *)dv;
}

expr_t *expr_simplify_rebuild_binary_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    if (!dv || !a || !b)
        return expr_simplify_passthrough(dv, a, b);
    return expr_new_binary_internal(dv->ops, a, b);
}

expr_t *expr_simplify_unary_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    NUM_SCOPE(scope);
    expr_t *branch_inverse;
    expr_t *direct_inverse;
    expr_t *imag_bridge;
    expr_t *lambert_argument;

    (void)b;
    if (expr_is_exp_expr(dv)) {
        expr_t *quarter_turn = expr_try_simplify_exp_quarter_turn(a);
        expr_t *lambert_exp;

        if (quarter_turn) {
            expr_free(a);
            return quarter_turn;
        }

        lambert_exp = expr_simplify_try_lambert_exp(a);
        if (lambert_exp) {
            expr_free(a);
            return lambert_exp;
        }
    }

    if (expr_is_op(dv, &ops_lambert_w) || expr_is_op(dv, &ops_lambert_w0)) {
        lambert_argument = expr_simplify_try_lambert_argument(a);
        if (lambert_argument) {
            expr_free(a);
            return lambert_argument;
        }
    }

    direct_inverse = expr_simplify_direct_inverse_pair_from_raw(dv, dv->a, a);
    if (direct_inverse)
        return direct_inverse;

    if (expr_is_op(dv, &ops_atan)) {
        branch_inverse = expr_simplify_atan_tan_sawtooth(a);
        if (branch_inverse)
            return branch_inverse;
    }

    direct_inverse = expr_simplify_direct_inverse_pair(dv, a);
    if (direct_inverse)
        return direct_inverse;

    direct_inverse = expr_simplify_try_vtable_inverse_argument(dv, a);
    if (direct_inverse) {
        expr_free(a);
        return direct_inverse;
    }

    if (expr_is_op(dv, &ops_tan)) {
        expr_t *periodic = expr_try_simplify_tan_period_floor(a);

        if (periodic)
            return periodic;
    }

    if (expr_is_op(dv, &ops_tanh)) {
        const expr_t *positive_argument = NULL;

        if (expr_is_neg(a) && a->a) {
            positive_argument = a->a;
            expr_t *positive_tanh = expr_tanh(positive_argument);
            expr_t *out = positive_tanh ? expr_negate_owned(positive_tanh) : NULL;

            if (out) {
                expr_free(a);
                return out;
            }
        }
    }

    imag_bridge = expr_simplify_try_imag_trig_bridge(dv, a);
    if (imag_bridge)
        return imag_bridge;

    if (expr_is_op(dv, &ops_log10)) {
        expr_t *log10_power = expr_simplify_try_log10_power_of_ten(a);

        if (log10_power)
            return log10_power;
    }

    {
        expr_t *floor_ceil_const = expr_simplify_try_floor_ceil_const(dv, a);

        if (floor_ceil_const)
            return floor_ceil_const;
    }

    {
        expr_t *const_fold = expr_simplify_try_unary_const_fold(dv, a);

        if (const_fold)
            return const_fold;
    }

    {
        expr_t *value_fold = expr_simplify_try_unary_const_value_fold(dv, a);

        if (value_fold)
            return value_fold;
    }

    if (expr_is_op(dv, &ops_sqrt)) {
        expr_t *sqrt_scaled = expr_simplify_try_sqrt_scaled_square_const(a);

        if (sqrt_scaled)
            return sqrt_scaled;
    }

    if (dv->ops->apply_unary && a != dv->a) {
        expr_t *out = dv->ops->apply_unary(a);
        expr_free(a);
        return out;
    }

    expr_free(a);
    expr_retain((expr_t *)dv);
    return (expr_t *)dv;
}

expr_t *expr_simplify_binary_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    if ((!a || !b) || !dv->ops->apply_binary) {
        return expr_simplify_passthrough(dv, a, b);
    }

    expr_t *out = dv->ops->apply_binary(a, b);
    expr_free(a);
    expr_free(b);
    return out;
}

/* ========================================================================= */
/* Per-operation simplifiers                                                 */
/* ========================================================================= */

expr_t *expr_simplify_neg_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    NUM_SCOPE(scope);
    (void)dv;
    (void)b;
    {
        expr_t *positive = expr_simplify_positive_part_if_negative(a);

        if (positive) {
            expr_free(a);
            return positive;
        }
    }
    /* neg(c) -> -c, but only for plain numeric literals. Named constants
     * and numeric-expression constants must keep their symbolic identity. */
    if (expr_is_unnamed_const(a) && !a->binding_expr) {
        number_t c = num_neg(a->c);
        expr_t *out = expr_new_const_owned_local(c);

        expr_free(a);
        return out;
    }
    expr_t *r = expr_neg(a);
    expr_free(a);
    return r;
}

/* --- */

static expr_t *expr_try_simplify_log_const_difference_local(expr_t *a, expr_t *b)
{
    number_t left = NUM_ZERO;
    number_t right = NUM_ZERO;
    number_t quotient = NUM_ZERO;
    expr_t *quotient_expr;
    expr_t *raw_log;
    expr_t *out;
    string_t *left_text = NULL;
    string_t *right_text = NULL;

    if (!expr_is_op(a, &ops_log) || !expr_is_op(b, &ops_log))
        return NULL;

    if (!expr_simplify_try_get_plain_real_const(a->a, &left))
        return NULL;
    if (!num_gt(left, NUM_ZERO)) {
        num_destroy(&left);
        return NULL;
    }

    if (!expr_simplify_try_get_plain_real_const(b->a, &right)) {
        num_destroy(&left);
        return NULL;
    }
    if (!num_gt(right, NUM_ZERO)) {
        num_destroy(&right);
        num_destroy(&left);
        return NULL;
    }

    quotient = num_div(left, right);
    if (!num_is_finite(quotient) || !num_gt(quotient, NUM_ZERO)) {
        num_destroy(&right);
        num_destroy(&left);
        num_destroy(&quotient);
        return NULL;
    }

    quotient_expr = expr_new_const_owned_local(quotient);
    if (quotient_expr && !num_is_integer(quotient) && num_is_integer(left) && num_is_integer(right)) {
        left_text = num_to_string(left);
        right_text = num_to_string(right);
        if (left_text && right_text) {
            quotient_expr->binding_expr =
                expr_binding_expr_new_div(expr_binding_expr_new_number_text(string_c_str(left_text)),
                                          expr_binding_expr_new_number_text(string_c_str(right_text)));
        }
    }
    string_free(right_text);
    string_free(left_text);
    num_destroy(&right);
    num_destroy(&left);
    num_destroy(&quotient);
    raw_log = expr_log(quotient_expr);
    expr_free(quotient_expr);
    out = expr_simplify(raw_log);
    expr_free(raw_log);
    return out;
}

static int expr_simplify_exact_sqrt_radicand_local(const expr_t *expr, number_t *radicand_out)
{
    if (expr_is_sqrt_expr(expr) && expr->a && expr_simplify_is_plain_real_const(expr->a)) {
        num_destroy(radicand_out);
        *radicand_out = num_clone(expr->a->c);
        return 1;
    }

    if (expr_is_unnamed_const(expr) && expr->binding_expr && expr->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
        expr->binding_expr->u.unary_op.ops == &ops_sqrt) {
        expr_t *radicand = expr_binding_expr_eval_expr(expr->binding_expr->u.unary_op.child);
        int matched = radicand && expr_simplify_is_plain_real_const(radicand);

        if (matched) {
            num_destroy(radicand_out);
            *radicand_out = num_clone(radicand->c);
        }
        expr_free(radicand);
        return matched;
    }

    return 0;
}

static int expr_simplify_atan_const_over_sqrt_local(const expr_t *expr, number_t *numerator_out, number_t *radicand_out)
{
    const expr_t *arg;
    expr_t *numerator = NULL;
    expr_t *root = NULL;
    int matched = 0;

    if (!expr_is_op(expr, &ops_atan) || !expr->a)
        return 0;
    arg = expr->a;

    if (expr_simplify_is_plain_real_const(arg)) {
        num_destroy(numerator_out);
        *numerator_out = num_clone(arg->c);
        num_destroy(radicand_out);
        *radicand_out = num_const(NUM_ONE);
        return 1;
    } else if (expr_is_div(arg)) {
        numerator = expr_retain_expr(arg->a);
        root = expr_retain_expr(arg->b);
    } else if (expr_is_unnamed_const(arg) && arg->binding_expr && arg->binding_expr->kind == EXPR_BINDING_EXPR_DIV) {
        numerator = expr_binding_expr_eval_expr(arg->binding_expr->u.binary.left);
        root = expr_binding_expr_eval_expr(arg->binding_expr->u.binary.right);
    }

    if (numerator && root && expr_simplify_is_plain_real_const(numerator) &&
        expr_simplify_exact_sqrt_radicand_local(root, radicand_out)) {
        num_destroy(numerator_out);
        *numerator_out = num_clone(numerator->c);
        matched = 1;
    }

    expr_free(root);
    expr_free(numerator);
    return matched;
}

static expr_t *expr_try_simplify_atan_const_difference_local(expr_t *a, expr_t *b)
{
    NUM_SCOPE(scope);
    number_t left_numerator = num_new();
    number_t right_numerator = num_new();
    number_t left_radicand = num_new();
    number_t right_radicand = num_new();
    number_t product = num_new();
    number_t denominator = num_new();
    number_t difference = num_new();
    number_t abs_difference = num_new();
    number_t coefficient = num_new();
    expr_t *root = NULL;
    expr_t *radicand = NULL;
    expr_t *coefficient_denominator = NULL;
    expr_t *argument = NULL;
    expr_t *atan_argument = NULL;
    expr_t *out = NULL;

    if (!expr_simplify_atan_const_over_sqrt_local(a, &left_numerator, &left_radicand) ||
        !expr_simplify_atan_const_over_sqrt_local(b, &right_numerator, &right_radicand) ||
        !num_eq(left_radicand, right_radicand))
        goto cleanup;

    num_destroy(&product);
    product = num_mul(left_numerator, right_numerator);
    num_destroy(&denominator);
    denominator = num_add(left_radicand, product);
    if (!num_gt(denominator, NUM_ZERO))
        goto cleanup;

    num_destroy(&difference);
    difference = num_sub(left_numerator, right_numerator);
    if (num_is_zero(difference)) {
        out = expr_new_const(NUM_ZERO);
        goto cleanup;
    }

    num_destroy(&abs_difference);
    abs_difference = num_abs(difference);
    num_destroy(&coefficient);
    coefficient = num_div(abs_difference, denominator);
    if (num_is_one(left_radicand)) {
        argument = expr_new_const_owned_local(coefficient);
    } else {
        radicand = expr_new_const(left_radicand);
        root = radicand ? expr_sqrt(radicand) : NULL;
    }
    if (root && !argument) {
        long numerator_value;
        long denominator_value;

        if (expr_number_small_rational_local(coefficient, &numerator_value, &denominator_value) &&
            numerator_value == 1L && denominator_value > 1L) {
            coefficient_denominator = expr_new_const(num_create_from_long(denominator_value));
            argument = coefficient_denominator ? expr_div(root, coefficient_denominator) : NULL;
        } else {
            argument = expr_make_scaled(coefficient, root);
            root = NULL;
        }
    }
    atan_argument = argument ? expr_atan(argument) : NULL;
    out = atan_argument && num_lt(difference, NUM_ZERO) ? expr_neg(atan_argument) : expr_retain_expr(atan_argument);

cleanup:
    expr_free(atan_argument);
    expr_free(argument);
    expr_free(coefficient_denominator);
    expr_free(root);
    expr_free(radicand);
    num_destroy(&coefficient);
    num_destroy(&abs_difference);
    num_destroy(&difference);
    num_destroy(&denominator);
    num_destroy(&product);
    num_destroy(&right_radicand);
    num_destroy(&left_radicand);
    num_destroy(&right_numerator);
    num_destroy(&left_numerator);
    return out;
}

static int expr_combine_atan_difference_addends_local(addend_t *terms, size_t n)
{
    int combined_any = 0;

    for (size_t i = 0; i < n; ++i) {
        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;
        for (size_t j = i + 1; j < n; ++j) {
            number_t neg_j;
            expr_t *combined;
            number_t combined_coeff;

            if (!terms[j].base || num_is_zero(terms[j].coeff))
                continue;
            neg_j = num_neg(terms[j].coeff);
            if (!num_eq(terms[i].coeff, neg_j)) {
                num_destroy(&neg_j);
                continue;
            }
            num_destroy(&neg_j);

            if (num_gt(terms[i].coeff, NUM_ZERO)) {
                combined = expr_try_simplify_atan_const_difference_local(terms[i].base, terms[j].base);
                combined_coeff = num_clone(terms[i].coeff);
            } else {
                combined = expr_try_simplify_atan_const_difference_local(terms[j].base, terms[i].base);
                combined_coeff = num_clone(terms[j].coeff);
            }
            if (!combined) {
                num_destroy(&combined_coeff);
                continue;
            }

            if (expr_is_op(combined, &ops_neg)) {
                expr_t *positive = expr_retain_expr(combined->a);
                number_t negative_coeff = num_neg(combined_coeff);

                expr_free(combined);
                num_destroy(&combined_coeff);
                combined = positive;
                combined_coeff = negative_coeff;
            }

            expr_free(terms[i].base);
            terms[i].base = combined;
            num_destroy(&terms[i].coeff);
            terms[i].coeff = combined_coeff;
            num_destroy(&terms[j].coeff);
            terms[j].coeff = num_const(NUM_ZERO);
            combined_any = 1;
            break;
        }
    }
    return combined_any;
}

static int expr_combine_log_difference_addends_local(addend_t *terms, size_t n)
{
    int combined_any = 0;

    for (size_t i = 0; i < n; ++i) {
        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;
        for (size_t j = i + 1; j < n; ++j) {
            number_t neg_j;
            expr_t *combined;
            number_t combined_coeff;

            if (!terms[j].base || num_is_zero(terms[j].coeff))
                continue;
            neg_j = num_neg(terms[j].coeff);
            if (!num_eq(terms[i].coeff, neg_j)) {
                num_destroy(&neg_j);
                continue;
            }
            num_destroy(&neg_j);

            if (num_gt(terms[i].coeff, NUM_ZERO)) {
                combined = expr_try_simplify_log_const_difference_local(terms[i].base, terms[j].base);
                combined_coeff = num_clone(terms[i].coeff);
            } else {
                combined = expr_try_simplify_log_const_difference_local(terms[j].base, terms[i].base);
                combined_coeff = num_clone(terms[j].coeff);
            }
            if (!combined) {
                num_destroy(&combined_coeff);
                continue;
            }

            expr_free(terms[i].base);
            terms[i].base = combined;
            num_destroy(&terms[i].coeff);
            terms[i].coeff = combined_coeff;
            num_destroy(&terms[j].coeff);
            terms[j].coeff = num_const(NUM_ZERO);
            combined_any = 1;
            break;
        }
    }
    return combined_any;
}

static int expr_contains_atan_local(const expr_t *expr)
{
    return expr &&
           (expr_is_op(expr, &ops_atan) || expr_contains_atan_local(expr->a) || expr_contains_atan_local(expr->b));
}

static int expr_addends_contain_constant_atan_local(const addend_t *terms, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (terms[i].base && !expr_contains_var_local(terms[i].base) && expr_contains_atan_local(terms[i].base))
            return 1;
    }
    return 0;
}

expr_t *expr_simplify_add_sub_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    NUM_SCOPE(scope);
    number_t c_const = num_const(NUM_ZERO);
    number_t common_coeff = num_const(NUM_ONE);
    addend_t *terms = NULL;
    size_t n = 0, cap = 0;
    int combined_atan_difference = 0;
    expr_t *folded_numeric = expr_try_fold_numeric_arithmetic_local(dv, a, b);

    if (folded_numeric) {
        expr_free(a);
        expr_free(b);
        num_destroy(&c_const);
        num_destroy(&common_coeff);
        return folded_numeric;
    }

    if (expr_polynomial_is_zero_deg4(dv)) {
        expr_free(a);
        expr_free(b);
        num_destroy(&c_const);
        num_destroy(&common_coeff);
        return expr_new_const(NUM_ZERO);
    }

    if (expr_is_op(dv, &ops_add)) {
        expr_t *trig_weighted_sum = expr_simplify_try_trig_weighted_sum(a, b);

        if (trig_weighted_sum) {
            expr_free(a);
            expr_free(b);
            num_destroy(&c_const);
            num_destroy(&common_coeff);
            return trig_weighted_sum;
        }
        expr_t *basic_sum = expr_simplify_try_basic_sum(a, b);

        if (basic_sum) {
            expr_free(a);
            expr_free(b);
            num_destroy(&c_const);
            num_destroy(&common_coeff);
            return basic_sum;
        }
    }

    if (expr_is_op(dv, &ops_sub)) {
        expr_t *atan_difference = expr_try_simplify_atan_const_difference_local(a, b);

        if (atan_difference) {
            expr_free(a);
            expr_free(b);
            num_destroy(&c_const);
            num_destroy(&common_coeff);
            return atan_difference;
        }
    }

    if (expr_is_op(dv, &ops_sub)) {
        expr_t *log_difference = expr_try_simplify_log_const_difference_local(a, b);

        if (log_difference) {
            expr_free(a);
            expr_free(b);
            num_destroy(&c_const);
            num_destroy(&common_coeff);
            return log_difference;
        }
    }
    if (expr_is_op(dv, &ops_add)) {
        expr_t *log_difference = NULL;

        if (expr_is_op(a, &ops_neg))
            log_difference = expr_try_simplify_log_const_difference_local(b, a->a);
        else if (expr_is_op(b, &ops_neg))
            log_difference = expr_try_simplify_log_const_difference_local(a, b->a);

        if (log_difference) {
            expr_free(a);
            expr_free(b);
            num_destroy(&c_const);
            num_destroy(&common_coeff);
            return log_difference;
        }
    }

    expr_collect_addends(a, NUM_ONE, &c_const, &terms, &n, &cap);
    expr_free(a);
    expr_collect_addends(b, expr_is_op(dv, &ops_sub) ? NUM_NEG_ONE : NUM_ONE, &c_const, &terms, &n, &cap);
    expr_free(b);

    combined_atan_difference = expr_combine_atan_difference_addends_local(terms, n);
    expr_combine_log_difference_addends_local(terms, n);
    expr_combine_common_denominator_addends(terms, n);
    expr_combine_trig_pythagorean_addends(terms, n);
    expr_sort_addends(terms, n);
    expr_extract_common_addend_coeff(terms, n, c_const, &common_coeff);
    if (num_is_one(common_coeff) && !combined_atan_difference && !expr_addends_contain_constant_atan_local(terms, n))
        expr_try_common_abs_coeff_local(terms, n, c_const, &common_coeff);

    expr_t *identity = expr_try_trig_pythagorean_identity(terms, n, c_const, common_coeff);

    if (identity) {
        for (size_t i = 0; i < n; ++i) {
            expr_free(terms[i].base);
            num_destroy(&terms[i].coeff);
        }
        free(terms);
        num_destroy(&c_const);
        num_destroy(&common_coeff);
        return identity;
    }
    number_t scaled_const = num_div(c_const, common_coeff);

    /* find the leading non-cancelled symbolic term */
    int leading_neg = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!num_is_zero(terms[i].coeff)) {
            leading_neg = num_lt(terms[i].coeff, NUM_ZERO);
            break;
        }
    }

    expr_t *cur = NULL;

    /* emit a positive constant before any leading negative symbolic term so
     * the expression reads "1 - tanh²(x)" rather than "-tanh²(x) + 1" */
    int const_emitted = 0;
    if (!num_is_zero(scaled_const) && num_gt(scaled_const, NUM_ZERO) && leading_neg) {
        cur = expr_new_const(scaled_const);
        const_emitted = 1;
    }

    size_t preferred_positive = n;
    if (!const_emitted && leading_neg) {
        for (size_t i = 0; i < n; ++i) {
            if (!num_is_zero(terms[i].coeff) && num_gt(terms[i].coeff, NUM_ZERO)) {
                preferred_positive = i;
                break;
            }
        }
    }

    if (preferred_positive < n) {
        number_t scaled_coeff = num_div(terms[preferred_positive].coeff, common_coeff);
        cur = expr_make_scaled(scaled_coeff, terms[preferred_positive].base);
        num_destroy(&terms[preferred_positive].coeff);
    }

    for (size_t i = 0; i < n; ++i) {
        if (i == preferred_positive)
            continue;
        if (num_is_zero(terms[i].coeff)) {
            expr_free(terms[i].base);
            num_destroy(&terms[i].coeff);
            continue;
        }
        number_t scaled_coeff = num_div(terms[i].coeff, common_coeff);
        expr_t *term = expr_make_scaled(scaled_coeff, terms[i].base);

        num_destroy(&terms[i].coeff);
        if (!cur)
            cur = term;
        else {
            expr_t *tmp = expr_add(cur, term);
            expr_free(cur);
            expr_free(term);
            cur = tmp;
        }
    }
    free(terms);

    if (!const_emitted && !num_is_zero(scaled_const)) {
        expr_t *cterm = expr_new_const(scaled_const);
        if (!cur)
            cur = cterm;
        else {
            expr_t *tmp = expr_add(cur, cterm);
            expr_free(cur);
            expr_free(cterm);
            cur = tmp;
        }
    }

    if (!cur)
        cur = expr_new_const(NUM_ZERO);

    if (!const_emitted && num_is_zero(scaled_const)) {
        expr_t *factored = expr_try_factor_common_symbolic_product_local(cur);

        if (factored) {
            expr_free(cur);
            cur = factored;
        }
    }

    if (!num_is_one(common_coeff)) {
        expr_t *scaled = expr_make_scaled(common_coeff, cur);

        cur = scaled;
    }

    num_destroy(&scaled_const);
    num_destroy(&c_const);
    num_destroy(&common_coeff);

    return cur;
}

/* --- */

static bool expr_simplify_match_square_local(const expr_t *expr, const expr_t **base_out)
{
    if (!expr || !base_out)
        return false;

    if (expr_is_pow_d_expr(expr) && expr->a && num_eq(expr->c, NUM_TWO)) {
        *base_out = expr->a;
        return true;
    }

    if (expr_is_op(expr, &ops_pow) && expr->a && expr->b && expr_simplify_is_plain_real_const(expr->b) &&
        num_eq(expr->b->c, NUM_TWO)) {
        *base_out = expr->a;
        return true;
    }

    return false;
}

/* v²((u/v)² + 1) = u² + v².  This form occurs naturally when
 * differentiating atan(u/v); expanding the right-hand side also exposes
 * the polynomial simplifications needed for a compact final derivative. */
static expr_t *expr_simplify_try_squared_quotient_sum_product_local(const expr_t *square_factor,
                                                                    const expr_t *sum_factor)
{
    const expr_t *denominator = NULL;
    const expr_t *quotient_square = NULL;
    const expr_t *quotient = NULL;
    const expr_t *var = NULL;
    number_t *polynomial = NULL;
    expr_t *numerator_squared = NULL;
    expr_t *denominator_squared = NULL;
    expr_t *sum = NULL;
    expr_t *expanded = NULL;

    if (!expr_simplify_match_square_local(square_factor, &denominator) || !expr_is_op(sum_factor, &ops_add))
        return NULL;

    if (expr_simplify_is_simplifiable_const(sum_factor->a) && expr_const_is_one(sum_factor->a)) {
        quotient_square = sum_factor->b;
    } else if (expr_simplify_is_simplifiable_const(sum_factor->b) && expr_const_is_one(sum_factor->b)) {
        quotient_square = sum_factor->a;
    } else {
        return NULL;
    }

    if (!expr_simplify_match_square_local(quotient_square, &quotient) || !expr_is_div(quotient) ||
        !expr_struct_eq(quotient->b, denominator))
        return NULL;

    numerator_squared = expr_mul(quotient->a, quotient->a);
    denominator_squared = expr_mul(denominator, denominator);
    sum = (numerator_squared && denominator_squared) ? expr_add(numerator_squared, denominator_squared) : NULL;
    if (sum && expr_collect_single_var(sum, &var) && var) {
        polynomial = expr_simplify_number_array_alloc_local(9u);
        if (polynomial && expr_simplify_collect_poly_local(sum, var, polynomial, 9u))
            expanded = expr_simplify_build_flat_poly_local(var, polynomial, 9u);
    }

    expr_simplify_number_array_free_local(polynomial, 9u);
    expr_free(sum);
    expr_free(denominator_squared);
    expr_free(numerator_squared);
    return expanded;
}

expr_t *expr_simplify_mul_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    NUM_SCOPE(scope);
    expr_t **terms = NULL;
    expr_t **den_terms = NULL;
    size_t nterms = 0, term_cap = 0;
    size_t nden_terms = 0, den_cap = 0;
    number_t c_acc = num_const(NUM_ONE);
    int is_zero = 0;
    expr_t *expanded;
    expr_t *numerator;
    expr_t *denominator;
    expr_t *division;

    {
        expr_t *folded_numeric = expr_try_fold_numeric_arithmetic_local(dv, a, b);

        if (folded_numeric) {
            expr_free(a);
            expr_free(b);
            num_destroy(&c_acc);
            return folded_numeric;
        }
    }

    if (expr_is_exact_zero_node_local(a) || expr_is_exact_zero_node_local(b)) {
        expr_free(a);
        expr_free(b);
        return expr_new_const(NUM_ZERO);
    }
    if (expr_simplify_is_simplifiable_const(a) && expr_const_is_one(a)) {
        expr_free(a);
        return b;
    }
    if (expr_simplify_is_simplifiable_const(b) && expr_const_is_one(b)) {
        expr_free(b);
        return a;
    }
    if (expr_simplify_is_simplifiable_const(a) && expr_const_is_minus_one(a)) {
        expr_t *out = expr_neg(b);

        expr_free(a);
        expr_free(b);
        return out;
    }
    if (expr_simplify_is_simplifiable_const(b) && expr_const_is_minus_one(b)) {
        expr_t *out = expr_neg(a);

        expr_free(a);
        expr_free(b);
        return out;
    }

    {
        expr_t *quotient_sum = expr_simplify_try_squared_quotient_sum_product_local(a, b);

        if (!quotient_sum)
            quotient_sum = expr_simplify_try_squared_quotient_sum_product_local(b, a);
        if (quotient_sum) {
            expr_free(a);
            expr_free(b);
            return quotient_sum;
        }
    }

    {
        expr_t *basic_product = expr_simplify_try_basic_product(a, b);

        if (basic_product) {
            expr_free(a);
            expr_free(b);
            return basic_product;
        }
    }

    {
        expr_t *const_over_e = expr_simplify_try_const_over_e_local(a, b);

        if (const_over_e)
            return const_over_e;
    }

    {
        expr_t *i_unit_product = expr_simplify_try_i_unit_product(a, b);

        if (i_unit_product) {
            expr_free(a);
            expr_free(b);
            return i_unit_product;
        }
    }

    {
        expr_t *lambert_identity = expr_simplify_try_lambert_product(a, b);

        if (lambert_identity) {
            expr_free(a);
            expr_free(b);
            return lambert_identity;
        }
    }

    {
        expr_t *trig_identity = expr_simplify_try_trig_product(a, b);

        if (trig_identity) {
            expr_free(a);
            expr_free(b);
            return trig_identity;
        }
    }

    if (expr_is_div(a) && expr_struct_eq(a->b, b)) {
        expr_t *out;

        expr_retain(a->a);
        out = a->a;
        expr_free(a);
        expr_free(b);
        return out;
    }

    if (expr_is_div(b) && expr_struct_eq(b->b, a)) {
        expr_t *out;

        expr_retain(b->a);
        out = b->a;
        expr_free(a);
        expr_free(b);
        return out;
    }

    collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    expr_free(a);
    expr_free(b);

    if (is_zero) {
        expr_free_node_array(terms, nterms);
        num_destroy(&c_acc);
        return expr_new_const(NUM_ZERO);
    }

    expr_split_division_terms(&c_acc, &is_zero, terms, nterms, &den_terms, &nden_terms, &den_cap);

    if (is_zero) {
        expr_free_node_array(terms, nterms);
        expr_free_node_array(den_terms, nden_terms);
        num_destroy(&c_acc);
        return expr_new_const(NUM_ZERO);
    }

    expr_combine_like_powers(den_terms, nden_terms);
    expr_combine_like_powers(terms, nterms);
    expr_cancel_common_powers(terms, nterms, den_terms, nden_terms);
    expr_combine_exp_terms(terms, nterms);
    expr_merge_sqrt_terms(terms, nterms);
    expr_merge_sqrt_terms(den_terms, nden_terms);
    expr_merge_coefficient_sqrt_terms(&c_acc, terms, nterms);
    expr_merge_sqrt_quotient_terms(terms, nterms, den_terms, nden_terms);

    expanded = expr_try_expand_shallow_product(c_acc, terms, nterms, den_terms, nden_terms);
    if (expanded) {
        num_destroy(&c_acc);
        return expanded;
    }

    numerator = expr_rebuild_product_chain(c_acc, terms, nterms);
    num_destroy(&c_acc);
    if (nden_terms == 0) {
        free(den_terms);
        return numerator;
    }

    denominator = expr_rebuild_division_chain(den_terms, nden_terms);
    if (!denominator)
        return numerator;
    division = expr_div(numerator, denominator);
    expr_free(numerator);
    expr_free(denominator);
    numerator = expr_simplify(division);
    expr_free(division);
    return numerator;
}

/* --- */

static expr_t *expr_simplify_try_reciprocal_unary(expr_t *numerator, expr_t *denominator)
{
    const expr_ops_t *replacement_ops;

    if (!expr_simplify_is_simplifiable_const(numerator) || !expr_const_is_one(numerator) || !denominator ||
        !denominator->ops || !denominator->a)
        return NULL;

    replacement_ops = expr_ops_reciprocal_unary(denominator->ops);
    if (replacement_ops && replacement_ops->apply_unary) {
        expr_t *arg = denominator->a;
        expr_t *out;

        expr_retain(arg);
        out = replacement_ops->apply_unary(arg);
        expr_free(arg);
        expr_free(numerator);
        expr_free(denominator);
        return out;
    }

    return NULL;
}

static expr_t *expr_simplify_try_reciprocal_unary_power(expr_t *numerator, expr_t *denominator)
{
    const expr_t *base = NULL;
    const expr_ops_t *replacement_ops = NULL;
    number_t exponent = num_new();
    bool matched_power = false;
    expr_t *arg = NULL;
    expr_t *replacement = NULL;
    expr_t *out = NULL;

    if (!expr_simplify_is_simplifiable_const(numerator) || !expr_const_is_one(numerator) || !denominator ||
        !denominator->ops) {
        goto cleanup;
    }

    if (expr_is_pow_d_expr(denominator) && denominator->a) {
        base = denominator->a;
        num_destroy(&exponent);
        exponent = num_clone(denominator->c);
        matched_power = true;
    } else if (expr_is_op(denominator, &ops_pow) && denominator->a && denominator->b &&
               expr_simplify_is_plain_real_const(denominator->b)) {
        base = denominator->a;
        num_destroy(&exponent);
        exponent = num_clone(denominator->b->c);
        matched_power = true;
    }

    if (!matched_power || !num_is_real(exponent) || !num_is_integer(exponent) || !num_gt(exponent, NUM_ZERO) || !base ||
        !base->ops || !base->a) {
        goto cleanup;
    }

    replacement_ops = expr_ops_reciprocal_unary(base->ops);
    if (!replacement_ops || !replacement_ops->apply_unary)
        goto cleanup;

    expr_retain(base->a);
    arg = base->a;
    replacement = replacement_ops->apply_unary(arg);
    out = replacement ? expr_make_pow_like_owned_local(replacement, exponent) : NULL;
    replacement = NULL;
    if (out) {
        expr_free(numerator);
        expr_free(denominator);
    }

cleanup:
    expr_free(replacement);
    expr_free(arg);
    num_destroy(&exponent);
    return out;
}

static bool expr_simplify_is_exp_minus_one_local(const expr_t *dv)
{
    return dv && expr_is_exp_expr(dv) && dv->a && expr_simplify_is_simplifiable_const(dv->a) &&
           expr_const_is_minus_one(dv->a);
}

static expr_t *expr_simplify_try_const_over_e_local(expr_t *a, expr_t *b)
{
    expr_t *coeff = NULL;
    expr_t *e_const = NULL;
    expr_t *out = NULL;

    if (expr_simplify_is_plain_real_const(a) && expr_simplify_is_exp_minus_one_local(b)) {
        expr_retain(a);
        coeff = a;
    } else if (expr_simplify_is_plain_real_const(b) && expr_simplify_is_exp_minus_one_local(a)) {
        expr_retain(b);
        coeff = b;
    } else {
        return NULL;
    }

    e_const = expr_new_named_const(NUM_E, "e");
    out = e_const ? expr_div(coeff, e_const) : NULL;
    expr_free(e_const);
    expr_free(coeff);
    if (!out)
        return NULL;

    expr_free(a);
    expr_free(b);
    return out;
}

static void expr_simplify_zero_number_array_local(number_t *values, size_t n)
{
    for (size_t i = 0u; i < n; ++i)
        values[i] = num_new();
}

static void expr_simplify_reset_number_array_local(number_t *values, size_t n)
{
    for (size_t i = 0u; i < n; ++i) {
        num_destroy(&values[i]);
        values[i] = num_new();
    }
}

static void expr_simplify_clear_number_array_local(number_t *values, size_t n)
{
    for (size_t i = 0u; i < n; ++i)
        num_destroy(&values[i]);
}

static void expr_simplify_copy_number_array_local(number_t *dst, const number_t *src, size_t n)
{
    for (size_t i = 0u; i < n; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static number_t *expr_simplify_number_array_alloc_local(size_t n)
{
    number_t *values = calloc(n, sizeof(*values));

    if (!values)
        return NULL;
    expr_simplify_zero_number_array_local(values, n);
    return values;
}

static void expr_simplify_number_array_free_local(number_t *values, size_t n)
{
    if (!values)
        return;
    expr_simplify_clear_number_array_local(values, n);
    free(values);
}

static bool expr_simplify_poly_mul_local(const number_t *left, const number_t *right, number_t *out, size_t n)
{
    size_t product_count = 2u * n - 1u;
    number_t *product = NULL;
    bool fits = true;

    if (n == 0u || n > 65u)
        return false;

    product = expr_simplify_number_array_alloc_local(product_count);
    if (!product)
        return false;
    for (size_t i = 0u; i < n; ++i) {
        for (size_t j = 0u; j < n; ++j) {
            number_t term = num_mul(left[i], right[j]);
            number_t sum = num_add(product[i + j], term);

            num_destroy(&product[i + j]);
            product[i + j] = sum;
            num_destroy(&term);
        }
    }

    for (size_t i = n; i < product_count; ++i) {
        if (!num_is_zero(product[i])) {
            fits = false;
            break;
        }
    }
    if (fits)
        expr_simplify_copy_number_array_local(out, product, n);
    expr_simplify_number_array_free_local(product, product_count);
    return fits;
}

static long expr_simplify_poly_exponent_local(number_t value, size_t n)
{
    if (!num_is_real(value) || !num_is_integer(value))
        return -1L;

    for (long exponent = 0L; exponent < (long)n; ++exponent) {
        number_t candidate = num_create_from_long(exponent);
        bool equal = num_eq(value, candidate);

        num_destroy(&candidate);
        if (equal)
            return exponent;
    }
    return -1L;
}

static bool expr_simplify_poly_degree_local(const expr_t *expr, const expr_t *var, size_t max_degree,
                                            size_t *degree_out)
{
    const expr_t *base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t value = num_new();
    size_t left_degree = 0u;
    size_t right_degree = 0u;
    long exponent = 0L;
    bool is_sub = false;
    bool ok = false;

    if (!expr || !var || !degree_out)
        goto cleanup;

    if (expr_struct_eq(expr, var)) {
        *degree_out = 1u;
        ok = max_degree >= 1u;
        goto cleanup;
    }

    if (expr_match_const_value(expr, &value)) {
        *degree_out = 0u;
        ok = true;
        goto cleanup;
    }

    if (expr_match_unary_op(expr, EXPR_KIND_NEG, &base)) {
        ok = expr_simplify_poly_degree_local(base, var, max_degree, degree_out);
        goto cleanup;
    }

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        (void)is_sub;
        ok = expr_simplify_poly_degree_local(left, var, max_degree, &left_degree) &&
             expr_simplify_poly_degree_local(right, var, max_degree, &right_degree);
        if (ok) {
            *degree_out = left_degree > right_degree ? left_degree : right_degree;
            ok = *degree_out <= max_degree;
        }
        goto cleanup;
    }

    if (expr_match_mul_expr(expr, &left, &right)) {
        ok = expr_simplify_poly_degree_local(left, var, max_degree, &left_degree) &&
             expr_simplify_poly_degree_local(right, var, max_degree, &right_degree);
        if (ok) {
            ok = left_degree <= max_degree && right_degree <= max_degree && left_degree + right_degree <= max_degree;
            if (ok)
                *degree_out = left_degree + right_degree;
        }
        goto cleanup;
    }

    if (expr_match_pow_const(expr, &base, &value)) {
        exponent = expr_simplify_poly_exponent_local(value, max_degree + 1u);
        ok = exponent >= 0L && expr_simplify_poly_degree_local(base, var, max_degree, &left_degree) &&
             left_degree <= max_degree && (size_t)exponent <= max_degree &&
             left_degree * (size_t)exponent <= max_degree;
        if (ok)
            *degree_out = left_degree * (size_t)exponent;
        goto cleanup;
    }

cleanup:
    num_destroy(&value);
    return ok;
}

static bool expr_simplify_collect_poly_local(const expr_t *expr, const expr_t *var, number_t *out, size_t n)
{
    number_t value = num_new();
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    if (!expr || !var || !out || n < 2u || n > 65u)
        goto no_match;

    if (expr_struct_eq(expr, var)) {
        expr_simplify_reset_number_array_local(out, n);
        num_destroy(&out[1]);
        out[1] = num_clone(NUM_ONE);
        num_destroy(&value);
        return true;
    }

    if (expr_match_const_value(expr, &value)) {
        expr_simplify_reset_number_array_local(out, n);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        num_destroy(&value);
        return true;
    }

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        number_t *left_poly = NULL;
        number_t *right_poly = NULL;
        bool ok;

        left_poly = expr_simplify_number_array_alloc_local(n);
        right_poly = expr_simplify_number_array_alloc_local(n);
        if (!left_poly || !right_poly) {
            expr_simplify_number_array_free_local(right_poly, n);
            expr_simplify_number_array_free_local(left_poly, n);
            num_destroy(&value);
            return false;
        }
        ok = expr_simplify_collect_poly_local(left, var, left_poly, n) &&
             expr_simplify_collect_poly_local(right, var, right_poly, n);
        if (ok) {
            for (size_t i = 0u; i < n; ++i) {
                number_t sum = is_sub ? num_sub(left_poly[i], right_poly[i]) : num_add(left_poly[i], right_poly[i]);

                num_destroy(&out[i]);
                out[i] = sum;
            }
        }
        expr_simplify_number_array_free_local(right_poly, n);
        expr_simplify_number_array_free_local(left_poly, n);
        num_destroy(&value);
        return ok;
    }

    if (expr_match_unary_op(expr, EXPR_KIND_NEG, &left)) {
        number_t *inner_poly = NULL;
        bool ok;

        inner_poly = expr_simplify_number_array_alloc_local(n);
        if (!inner_poly) {
            num_destroy(&value);
            return false;
        }
        ok = expr_simplify_collect_poly_local(left, var, inner_poly, n);
        if (ok) {
            for (size_t i = 0u; i < n; ++i) {
                number_t negated = num_neg(inner_poly[i]);

                num_destroy(&out[i]);
                out[i] = negated;
            }
        }
        expr_simplify_number_array_free_local(inner_poly, n);
        num_destroy(&value);
        return ok;
    }

    if (expr_match_mul_expr(expr, &left, &right)) {
        number_t *left_poly = NULL;
        number_t *right_poly = NULL;
        bool ok;

        left_poly = expr_simplify_number_array_alloc_local(n);
        right_poly = expr_simplify_number_array_alloc_local(n);
        if (!left_poly || !right_poly) {
            expr_simplify_number_array_free_local(right_poly, n);
            expr_simplify_number_array_free_local(left_poly, n);
            num_destroy(&value);
            return false;
        }
        ok = expr_simplify_collect_poly_local(left, var, left_poly, n) &&
             expr_simplify_collect_poly_local(right, var, right_poly, n) &&
             expr_simplify_poly_mul_local(left_poly, right_poly, out, n);
        expr_simplify_number_array_free_local(right_poly, n);
        expr_simplify_number_array_free_local(left_poly, n);
        num_destroy(&value);
        return ok;
    }

    if (expr_match_pow_const(expr, &left, &value)) {
        long exponent = expr_simplify_poly_exponent_local(value, n);
        number_t *base_poly = NULL;
        number_t *result = NULL;
        bool ok = exponent >= 0L;

        base_poly = expr_simplify_number_array_alloc_local(n);
        result = expr_simplify_number_array_alloc_local(n);
        if (!base_poly || !result) {
            expr_simplify_number_array_free_local(result, n);
            expr_simplify_number_array_free_local(base_poly, n);
            num_destroy(&value);
            return false;
        }
        num_destroy(&result[0]);
        result[0] = num_clone(NUM_ONE);

        if (ok)
            ok = expr_simplify_collect_poly_local(left, var, base_poly, n);
        for (long i = 0L; ok && i < exponent; ++i) {
            number_t *next = expr_simplify_number_array_alloc_local(n);

            if (!next) {
                ok = false;
                break;
            }
            ok = expr_simplify_poly_mul_local(result, base_poly, next, n);
            if (ok)
                expr_simplify_copy_number_array_local(result, next, n);
            expr_simplify_number_array_free_local(next, n);
        }
        if (ok)
            expr_simplify_copy_number_array_local(out, result, n);
        expr_simplify_number_array_free_local(result, n);
        expr_simplify_number_array_free_local(base_poly, n);
        num_destroy(&value);
        return ok;
    }

no_match:
    num_destroy(&value);
    return false;
}

static expr_t *expr_simplify_build_flat_poly_local(const expr_t *var, const number_t *coeffs, size_t n)
{
    expr_t *sum = NULL;

    if (!var || !coeffs)
        return NULL;

    for (size_t i = n; i-- > 0u;) {
        expr_t *base = NULL;
        expr_t *term = NULL;

        if (num_is_zero(coeffs[i]))
            continue;

        if (i == 0u) {
            term = expr_new_const(coeffs[i]);
        } else {
            if (i == 1u) {
                base = expr_retain_expr(var);
            } else {
                number_t exponent = num_create_from_long((long)i);

                base = expr_pow(var, &exponent);
                num_destroy(&exponent);
            }
            term = base ? expr_mul_num(base, &coeffs[i]) : NULL;
            expr_free(base);
        }

        if (!term) {
            expr_free(sum);
            return NULL;
        }
        if (sum) {
            expr_t *next = expr_add(sum, term);

            expr_free(sum);
            expr_free(term);
            sum = next;
        } else {
            sum = term;
        }
    }

    return sum ? expr_simplify_owned(sum) : expr_new_const(NUM_ZERO);
}

static bool expr_simplify_denominator_is_quadratic_poly_local(const expr_t *expr, const expr_t *var)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    number_t *poly = NULL;
    size_t count = 3u;
    bool ok = false;

    poly = expr_simplify_number_array_alloc_local(count);
    if (!poly)
        goto cleanup_exponent;

    if (expr_match_pow_const(expr, &base, &exponent)) {
        if (!num_is_integer(exponent) || !num_gt(exponent, NUM_ZERO))
            goto cleanup;
    } else {
        base = expr;
    }

    if (!expr_simplify_collect_poly_local(base, var, poly, count) || num_is_zero(poly[2])) {
        goto cleanup;
    }

    ok = true;

cleanup:
    expr_simplify_number_array_free_local(poly, count);
cleanup_exponent:
    num_destroy(&exponent);
    return ok;
}

static expr_t *expr_simplify_try_exact_poly_quotient_local(const expr_t *a, const expr_t *b)
{
    enum { MAX_EXACT_QUOTIENT_DEGREE = 12 };
    const expr_t *var = NULL;
    number_t *numerator = NULL;
    number_t *denominator = NULL;
    number_t *remainder = NULL;
    number_t *quotient = NULL;
    size_t numerator_degree = 0u;
    size_t denominator_degree = 0u;
    size_t count = 0u;
    expr_t *out = NULL;

    if (!a || !b || !expr_collect_single_var(a, &var) || !var ||
        !expr_simplify_poly_degree_local(a, var, MAX_EXACT_QUOTIENT_DEGREE, &numerator_degree) ||
        !expr_simplify_poly_degree_local(b, var, MAX_EXACT_QUOTIENT_DEGREE, &denominator_degree) ||
        denominator_degree == 0u || numerator_degree < denominator_degree) {
        return NULL;
    }

    count = numerator_degree + 1u;
    numerator = expr_simplify_number_array_alloc_local(count);
    denominator = expr_simplify_number_array_alloc_local(count);
    remainder = expr_simplify_number_array_alloc_local(count);
    quotient = expr_simplify_number_array_alloc_local(count);
    if (!numerator || !denominator || !remainder || !quotient ||
        !expr_simplify_collect_poly_local(a, var, numerator, count) ||
        !expr_simplify_collect_poly_local(b, var, denominator, count) || num_is_zero(denominator[denominator_degree])) {
        goto cleanup;
    }

    expr_simplify_copy_number_array_local(remainder, numerator, count);
    for (size_t degree = numerator_degree + 1u; degree-- > denominator_degree;) {
        size_t quotient_degree = degree - denominator_degree;
        number_t factor = num_div(remainder[degree], denominator[denominator_degree]);

        num_destroy(&quotient[quotient_degree]);
        quotient[quotient_degree] = num_clone(factor);
        for (size_t i = 0u; i <= denominator_degree; ++i) {
            size_t index = quotient_degree + i;
            number_t product = num_mul(factor, denominator[i]);
            number_t difference = num_sub(remainder[index], product);

            num_destroy(&remainder[index]);
            remainder[index] = difference;
            num_destroy(&product);
        }
        num_destroy(&factor);
    }

    for (size_t i = 0u; i < denominator_degree; ++i) {
        if (!num_is_zero(remainder[i]))
            goto cleanup;
    }
    out = expr_simplify_build_flat_poly_local(var, quotient, numerator_degree - denominator_degree + 1u);

cleanup:
    expr_simplify_number_array_free_local(quotient, count);
    expr_simplify_number_array_free_local(remainder, count);
    expr_simplify_number_array_free_local(denominator, count);
    expr_simplify_number_array_free_local(numerator, count);
    return out;
}

static expr_t *expr_simplify_try_poly_quotient_numerator_local(expr_t *a, expr_t *b)
{
    enum { MAX_EAGER_QUOTIENT_NUMERATOR_DEGREE = 7 };
    const expr_t *var = NULL;
    number_t *poly = NULL;
    size_t numerator_degree = 0u;
    size_t count = 0u;
    expr_t *rewritten_a = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!a || !b || !expr_collect_single_var(a, &var) || !var ||
        !expr_simplify_poly_degree_local(a, var, MAX_EAGER_QUOTIENT_NUMERATOR_DEGREE, &numerator_degree) ||
        !expr_simplify_denominator_is_quadratic_poly_local(b, var))
        goto cleanup;

    count = numerator_degree + 1u;
    if (count < 2u)
        count = 2u;
    poly = expr_simplify_number_array_alloc_local(count);
    if (!poly || !expr_simplify_collect_poly_local(a, var, poly, count))
        goto cleanup;

    rewritten_a = expr_simplify_build_flat_poly_local(var, poly, count);
    if (!rewritten_a || expr_struct_eq(a, rewritten_a))
        goto cleanup;

    quotient = expr_div(rewritten_a, b);
    out = expr_simplify_mark_current(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(rewritten_a);
    expr_simplify_number_array_free_local(poly, count);
    return out;
}

static bool expr_simplify_contains_sqrt_local(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_sqrt_expr(expr))
        return true;
    if (expr_is_unnamed_const(expr) && expr->binding_expr && expr->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
        expr->binding_expr->u.unary_op.ops == &ops_sqrt) {
        return true;
    }
    return expr_simplify_contains_sqrt_local(expr->a) || expr_simplify_contains_sqrt_local(expr->b);
}

/* Put N/(1 + (u/v)^2) over the polynomial denominator u^2 + v^2 after
 * quotient flattening has cancelled any radical factors in N. */
static expr_t *expr_simplify_try_rational_atan_denominator_local(const expr_t *numerator, const expr_t *denominator)
{
    enum { MAX_ATAN_POLYNOMIAL_DEGREE = 8 };
    const expr_t *quotient_square = NULL;
    const expr_t *quotient = NULL;
    const expr_t *var = NULL;
    number_t *numerator_poly = NULL;
    number_t *denominator_poly = NULL;
    size_t numerator_degree = 0u;
    size_t denominator_degree = 0u;
    size_t numerator_count = 0u;
    size_t denominator_count = 0u;
    expr_t *v_squared = NULL;
    expr_t *scaled_raw = NULL;
    expr_t *scaled = NULL;
    expr_t *normal_denominator = NULL;
    expr_t *normal_numerator = NULL;
    expr_t *monic_denominator = NULL;
    expr_t *raw = NULL;
    expr_t *out = NULL;

    if (!numerator || !denominator || expr_simplify_contains_sqrt_local(numerator) ||
        !expr_is_op(denominator, &ops_add)) {
        return NULL;
    }
    if (expr_simplify_is_simplifiable_const(denominator->a) && expr_const_is_one(denominator->a)) {
        quotient_square = denominator->b;
    } else if (expr_simplify_is_simplifiable_const(denominator->b) && expr_const_is_one(denominator->b)) {
        quotient_square = denominator->a;
    } else {
        return NULL;
    }
    if (!expr_simplify_match_square_local(quotient_square, &quotient) || !expr_is_div(quotient) || !quotient->a ||
        !quotient->b) {
        return NULL;
    }

    {
        number_t two = num_clone(NUM_TWO);

        v_squared = expr_pow(quotient->b, &two);
        num_destroy(&two);
    }
    normal_denominator =
        v_squared ? expr_simplify_try_squared_quotient_sum_product_local(v_squared, denominator) : NULL;
    scaled_raw = v_squared ? expr_mul(numerator, v_squared) : NULL;
    scaled = scaled_raw ? expr_simplify(scaled_raw) : NULL;
    if (!scaled || !normal_denominator ||
        (!expr_collect_single_var(scaled, &var) && !expr_collect_single_var(normal_denominator, &var)) || !var ||
        !expr_simplify_poly_degree_local(scaled, var, MAX_ATAN_POLYNOMIAL_DEGREE, &numerator_degree) ||
        !expr_simplify_poly_degree_local(normal_denominator, var, MAX_ATAN_POLYNOMIAL_DEGREE, &denominator_degree)) {
        goto cleanup;
    }

    numerator_count = numerator_degree + 1u;
    denominator_count = denominator_degree + 1u;
    numerator_poly = expr_simplify_number_array_alloc_local(numerator_count);
    denominator_poly = expr_simplify_number_array_alloc_local(denominator_count);
    if (!numerator_poly || !denominator_poly ||
        !expr_simplify_collect_poly_local(scaled, var, numerator_poly, numerator_count) ||
        !expr_simplify_collect_poly_local(normal_denominator, var, denominator_poly, denominator_count) ||
        num_is_zero(denominator_poly[denominator_degree])) {
        goto cleanup;
    }

    {
        number_t leading = num_clone(denominator_poly[denominator_degree]);

        for (size_t i = 0u; i < numerator_count; ++i) {
            number_t normalised = num_div(numerator_poly[i], leading);

            num_destroy(&numerator_poly[i]);
            numerator_poly[i] = normalised;
        }
        for (size_t i = 0u; i < denominator_count; ++i) {
            number_t normalised = num_div(denominator_poly[i], leading);

            num_destroy(&denominator_poly[i]);
            denominator_poly[i] = normalised;
        }
        num_destroy(&leading);
    }

    normal_numerator = expr_simplify_build_flat_poly_local(var, numerator_poly, numerator_count);
    monic_denominator = expr_simplify_build_flat_poly_local(var, denominator_poly, denominator_count);
    raw = (normal_numerator && monic_denominator) ? expr_div(normal_numerator, monic_denominator) : NULL;
    out = raw ? expr_simplify(raw) : NULL;

cleanup:
    expr_free(raw);
    expr_free(monic_denominator);
    expr_free(normal_numerator);
    expr_free(scaled);
    expr_free(scaled_raw);
    expr_free(normal_denominator);
    expr_free(v_squared);
    expr_simplify_number_array_free_local(denominator_poly, denominator_count);
    expr_simplify_number_array_free_local(numerator_poly, numerator_count);
    return out;
}

static expr_t *expr_simplify_try_extract_half_base_power_over_log_local(const expr_t *numerator,
                                                                        const expr_t *denominator)
{
    number_t numerator_coefficient = num_clone(NUM_ONE);
    expr_t **numerator_terms = NULL;
    size_t numerator_count = 0u;
    size_t numerator_capacity = 0u;
    int numerator_is_zero = 0;
    size_t power_index = SIZE_MAX;
    expr_t *unscaled_base = NULL;
    expr_t *new_numerator = NULL;
    expr_t *shifted_exponent = NULL;
    expr_t *two_power = NULL;
    expr_t *new_denominator = NULL;
    expr_t *division = NULL;
    expr_t *out = NULL;

    if (!numerator || !denominator || !expr_is_op(denominator, &ops_log))
        goto cleanup;

    collect_mul_flat((expr_t *)numerator, &numerator_coefficient, &numerator_is_zero, &numerator_terms,
                     &numerator_count, &numerator_capacity);
    if (numerator_is_zero || (num_eq(numerator_coefficient, NUM_ONE) && numerator_count == 1u))
        goto cleanup;

    for (size_t i = 0u; i < numerator_count; ++i) {
        number_t base_coefficient = num_clone(NUM_ONE);
        expr_t **base_terms = NULL;
        size_t base_count = 0u;
        size_t base_capacity = 0u;
        int base_is_zero = 0;

        if (!expr_is_op(numerator_terms[i], &ops_pow) || !numerator_terms[i]->a || !numerator_terms[i]->b)
            continue;

        collect_mul_flat(numerator_terms[i]->a, &base_coefficient, &base_is_zero, &base_terms, &base_count,
                         &base_capacity);
        if (!base_is_zero && base_count != 0u && num_eq(base_coefficient, NUM_HALF)) {
            unscaled_base = expr_rebuild_product_chain(NUM_ONE, base_terms, base_count);
            base_terms = NULL;
            power_index = i;
        }
        expr_free_node_array(base_terms, base_count);
        num_destroy(&base_coefficient);
        if (power_index != SIZE_MAX)
            break;
    }
    if (power_index == SIZE_MAX || !unscaled_base)
        goto cleanup;
    if (num_eq(numerator_coefficient, NUM_ONE)) {
        int has_sqrt_factor = 0;

        for (size_t i = 0u; i < numerator_count; ++i) {
            if (i != power_index && expr_is_sqrt_expr(numerator_terms[i])) {
                has_sqrt_factor = 1;
                break;
            }
        }
        if (!has_sqrt_factor)
            goto cleanup;
    }

    {
        expr_t *replacement_power = expr_pow_xp(unscaled_base, numerator_terms[power_index]->b);
        expr_t *exponent = expr_clone(numerator_terms[power_index]->b);

        if (num_eq(numerator_coefficient, NUM_HALF)) {
            shifted_exponent = expr_add_simplify_owned(exponent, expr_new_const(NUM_ONE));
            num_destroy(&numerator_coefficient);
            numerator_coefficient = num_clone(NUM_ONE);
        } else {
            shifted_exponent = exponent;
        }
        expr_free(numerator_terms[power_index]);
        numerator_terms[power_index] = replacement_power;
    }
    new_numerator = expr_rebuild_product_chain(numerator_coefficient, numerator_terms, numerator_count);
    numerator_terms = NULL;
    if (shifted_exponent) {
        expr_t *two = expr_new_const(NUM_TWO);

        two_power = expr_pow_xp(two, shifted_exponent);
        expr_free(two);
    }
    new_denominator = two_power ? expr_mul(two_power, (expr_t *)denominator) : NULL;
    division = (new_numerator && new_denominator) ? expr_div(new_numerator, new_denominator) : NULL;
    out = division ? expr_simplify(division) : NULL;

cleanup:
    expr_free(division);
    expr_free(new_denominator);
    expr_free(two_power);
    expr_free(shifted_exponent);
    expr_free(new_numerator);
    expr_free(unscaled_base);
    expr_free_node_array(numerator_terms, numerator_count);
    num_destroy(&numerator_coefficient);
    return out;
}

static expr_t *expr_simplify_absorb_numerator_into_denominator_power_local(const expr_t *numerator,
                                                                           const expr_t *denominator)
{
    const expr_t *power_factor;
    expr_t *remainder = NULL;
    expr_t *power_base = NULL;
    expr_t *power_exponent = NULL;
    expr_t *shifted_exponent;
    expr_t *shifted_power;
    expr_t *new_denominator;

    if (!expr_simplify_is_plain_real_const(numerator))
        return NULL;

    power_factor = denominator;
    if (expr_is_op(denominator, &ops_mul)) {
        power_factor = denominator->a;
        remainder = expr_clone(denominator->b);
        if (!expr_is_op(power_factor, &ops_pow) &&
            !(expr_is_unnamed_const(power_factor) && power_factor->binding_expr &&
              power_factor->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
              power_factor->binding_expr->u.binary_op.ops == &ops_pow)) {
            power_factor = denominator->b;
            expr_free(remainder);
            remainder = expr_clone(denominator->a);
        }
    } else if (expr_is_unnamed_const(denominator) && denominator->binding_expr &&
               denominator->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
               denominator->binding_expr->u.binary_op.ops == &ops_mul) {
        const expr_binding_expr_t *left = denominator->binding_expr->u.binary_op.left;
        const expr_binding_expr_t *right = denominator->binding_expr->u.binary_op.right;
        bool left_is_power = left && left->kind == EXPR_BINDING_EXPR_BINARY_OP && left->u.binary_op.ops == &ops_pow;
        bool right_is_power = right && right->kind == EXPR_BINDING_EXPR_BINARY_OP && right->u.binary_op.ops == &ops_pow;

        if (left_is_power || right_is_power) {
            const expr_binding_expr_t *power_expr = left_is_power ? left : right;

            power_base = expr_from_preserved_binding_expr_local(expr_binding_expr_clone(power_expr->u.binary_op.left));
            power_exponent =
                expr_from_preserved_binding_expr_local(expr_binding_expr_clone(power_expr->u.binary_op.right));
            remainder = expr_from_preserved_binding_expr_local(expr_binding_expr_clone(left_is_power ? right : left));
        }
    }

    if (!power_base && expr_is_op(power_factor, &ops_pow)) {
        power_base = expr_clone(power_factor->a);
        power_exponent = expr_clone(power_factor->b);
    } else if (!power_base && expr_is_unnamed_const(power_factor) && power_factor->binding_expr &&
               power_factor->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
               power_factor->binding_expr->u.binary_op.ops == &ops_pow) {
        power_base = expr_from_preserved_binding_expr_local(
            expr_binding_expr_clone(power_factor->binding_expr->u.binary_op.left));
        power_exponent = expr_from_preserved_binding_expr_local(
            expr_binding_expr_clone(power_factor->binding_expr->u.binary_op.right));
    }
    if (!power_base || !power_exponent || !expr_struct_eq(numerator, power_base)) {
        expr_free(remainder);
        expr_free(power_exponent);
        expr_free(power_base);
        return NULL;
    }

    shifted_exponent = expr_sub_simplify_owned(power_exponent, expr_new_const(NUM_ONE));
    shifted_power = shifted_exponent ? expr_pow_xp(power_base, shifted_exponent) : NULL;
    expr_free(power_base);
    expr_free(shifted_exponent);
    new_denominator = shifted_power && remainder ? expr_mul_simplify_owned(shifted_power, remainder) : shifted_power;
    return new_denominator ? expr_div_simplify_owned(expr_new_const(NUM_ONE), new_denominator) : NULL;
}

static bool expr_clone_effective_power_local(const expr_t *expr, expr_t **base_out, expr_t **exponent_out)
{
    if (!expr || !base_out || !exponent_out)
        return false;

    *base_out = NULL;
    *exponent_out = NULL;
    if (expr_is_op(expr, &ops_pow)) {
        *base_out = expr_clone(expr->a);
        *exponent_out = expr_clone(expr->b);
    } else if (expr_is_unnamed_const(expr) && expr->binding_expr &&
               expr->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
               expr->binding_expr->u.binary_op.ops == &ops_pow) {
        *base_out = expr_from_preserved_binding_expr_local(
            expr_binding_expr_clone(expr->binding_expr->u.binary_op.left));
        *exponent_out = expr_from_preserved_binding_expr_local(
            expr_binding_expr_clone(expr->binding_expr->u.binary_op.right));
    }

    if (*base_out && *exponent_out)
        return true;
    expr_free(*exponent_out);
    expr_free(*base_out);
    *base_out = NULL;
    *exponent_out = NULL;
    return false;
}

expr_t *expr_simplify_div_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    NUM_SCOPE(scope);

    {
        expr_t *folded_numeric = expr_try_fold_numeric_arithmetic_local(dv, a, b);

        if (folded_numeric) {
            expr_free(a);
            expr_free(b);
            return folded_numeric;
        }
    }

    if (expr_is_op(b, &ops_const) && expr_const_is_one(b)) {
        expr_free(b);
        return a;
    }
    {
        expr_t *a_base = NULL, *a_exponent = NULL, *b_base = NULL, *b_exponent = NULL;

        if (expr_clone_effective_power_local(a, &a_base, &a_exponent) &&
            expr_clone_effective_power_local(b, &b_base, &b_exponent) &&
            expr_simplify_same_factor(a_base, b_base)) {
            number_t base_value = expr_eval(a_base);
            bool nonzero_base = num_is_finite(base_value) && !num_is_zero(base_value);

            num_destroy(&base_value);
            if (nonzero_base) {
                expr_t *difference = expr_sub_simplify_owned(a_exponent, b_exponent);
                expr_t *out = difference ? expr_pow_xp(a_base, difference) : NULL;

                expr_free(difference);
                expr_free(a_base);
                expr_free(b_base);
                expr_free(a);
                expr_free(b);
                return expr_simplify_owned(out);
            }
        }
        expr_free(b_exponent);
        expr_free(b_base);
        expr_free(a_exponent);
        expr_free(a_base);
    }
    if (expr_is_op(a, &ops_pow) && expr_is_op(b, &ops_mul)) {
        const expr_t *power = expr_is_op(b->a, &ops_pow) ? b->a : expr_is_op(b->b, &ops_pow) ? b->b : NULL;
        const expr_t *remainder = power == b->a ? b->b : power == b->b ? b->a : NULL;

        if (power && remainder && power->a && expr_simplify_same_factor(a->a, power->a)) {
            number_t base_value = expr_eval(a->a);
            bool nonzero_base = num_is_finite(base_value) && !num_is_zero(base_value);

            num_destroy(&base_value);
            if (nonzero_base) {
                expr_t *ratio = expr_div_simplify_owned(expr_clone(a), expr_clone(power));
                expr_t *out = ratio ? expr_div_simplify_owned(ratio, expr_clone(remainder)) : NULL;

                expr_free(a);
                expr_free(b);
                return out;
            }
        }
    }
    if (expr_is_op(b, &ops_mul)) {
        const expr_t *power = NULL;
        const expr_t *remainder = NULL;
        const expr_t *power_base = NULL;
        number_t power_exponent = num_new();

        if (expr_match_pow_const(b->a, &power_base, &power_exponent) && num_eq(power_exponent, NUM_NEG_ONE)) {
            power = b->a;
            remainder = b->b;
        } else {
            num_destroy(&power_exponent);
            power_exponent = num_new();
            if (expr_match_pow_const(b->b, &power_base, &power_exponent) && num_eq(power_exponent, NUM_NEG_ONE)) {
                power = b->b;
                remainder = b->a;
            }
        }
        num_destroy(&power_exponent);
        if (power && remainder && power_base) {
            number_t base_value = expr_eval(power_base);
            bool nonzero_base = num_is_finite(base_value) && !num_is_zero(base_value);

            num_destroy(&base_value);
            if (nonzero_base) {
                expr_t *new_numerator = expr_mul_simplify_owned(a, expr_clone(power_base));
                expr_t *out = new_numerator ? expr_div_simplify_owned(new_numerator, expr_clone(remainder)) : NULL;

                expr_free(b);
                return out;
            }
        }
    }
    {
        expr_t *absorbed_power = expr_simplify_absorb_numerator_into_denominator_power_local(a, b);

        if (absorbed_power) {
            expr_free(a);
            expr_free(b);
            return absorbed_power;
        }
    }
    if (expr_simplify_is_plain_real_const(a) && expr_is_op(b, &ops_pow) && b->a && b->b &&
        expr_simplify_is_plain_real_const(b->a)) {
        long numerator = 0L;
        long denominator = 0L;
        long power_base = 0L;
        long power_base_denominator = 0L;

        if (num_get_small_rational(a->c, &numerator, &denominator) && numerator == 1L && denominator > 1L &&
            num_get_small_rational(b->a->c, &power_base, &power_base_denominator) &&
            power_base_denominator == 1L && power_base == denominator) {
            expr_t *shifted_exponent = expr_add_simplify_owned(expr_clone(b->b), expr_new_const(NUM_ONE));
            expr_t *shifted_power = expr_pow_xp(expr_clone(b->a), shifted_exponent);
            expr_t *one = expr_new_const(NUM_ONE);
            expr_t *out = expr_div_simplify_owned(one, shifted_power);

            expr_free(a);
            expr_free(b);
            return out;
        }
    }
    if (expr_is_op(a, &ops_mul) && expr_is_op(b, &ops_pow) && b->a && b->b &&
        expr_simplify_is_plain_real_const(b->a)) {
        const expr_t *coefficient = expr_simplify_is_plain_real_const(a->a) ? a->a :
                                    expr_simplify_is_plain_real_const(a->b) ? a->b : NULL;
        const expr_t *remainder = coefficient == a->a ? a->b : coefficient == a->b ? a->a : NULL;
        long numerator = 0L;
        long denominator = 0L;
        long power_base = 0L;
        long power_base_denominator = 0L;

        if (coefficient && remainder && num_get_small_rational(coefficient->c, &numerator, &denominator) &&
            numerator == 1L && denominator > 1L &&
            num_get_small_rational(b->a->c, &power_base, &power_base_denominator) &&
            power_base_denominator == 1L && power_base == denominator) {
            expr_t *shifted_exponent = expr_add_simplify_owned(expr_clone(b->b), expr_new_const(NUM_ONE));
            expr_t *shifted_power = expr_pow_xp(expr_clone(b->a), shifted_exponent);
            expr_t *out = expr_div_simplify_owned(expr_clone(remainder), shifted_power);

            expr_free(a);
            expr_free(b);
            return out;
        }
    }
    {
        expr_t *combined_half_power = expr_simplify_try_extract_half_base_power_over_log_local(a, b);

        if (combined_half_power) {
            expr_free(a);
            expr_free(b);
            return combined_half_power;
        }
    }
    if (expr_is_op(a, &ops_exp) && expr_is_op(b, &ops_exp)) {
        expr_t *difference = expr_sub_simplify_owned(expr_clone(a->a), expr_clone(b->a));
        expr_t *out = difference ? expr_exp(difference) : NULL;

        expr_free(difference);
        expr_free(a);
        expr_free(b);
        return out;
    }
    {
        expr_t *exact_poly_quotient = expr_simplify_try_exact_poly_quotient_local(a, b);

        if (exact_poly_quotient) {
            expr_free(a);
            expr_free(b);
            return exact_poly_quotient;
        }
    }
    {
        expr_t *poly_quotient = expr_simplify_try_poly_quotient_numerator_local(a, b);

        if (poly_quotient) {
            expr_free(a);
            expr_free(b);
            return poly_quotient;
        }
    }
    {
        expr_t *sqrt_quotient = expr_simplify_try_sqrt_quotient(a, b);

        if (sqrt_quotient)
            return sqrt_quotient;
    }
    {
        expr_t *reciprocal = expr_simplify_try_reciprocal_unary(a, b);

        if (reciprocal)
            return reciprocal;
    }
    {
        expr_t *reciprocal_power = expr_simplify_try_reciprocal_unary_power(a, b);

        if (reciprocal_power)
            return reciprocal_power;
    }
    if (expr_is_pow_d_expr(b) && expr_struct_eq(a, b->a)) {
        number_t exponent = num_sub(b->c, NUM_ONE);
        expr_t *base;
        expr_t *denom;
        expr_t *one;
        expr_t *out;

        expr_retain(b->a);
        base = b->a;
        expr_free(a);
        expr_free(b);

        if (num_eq(exponent, NUM_ZERO)) {
            expr_free(base);
            return expr_new_const(NUM_ONE);
        }

        denom = expr_make_pow_like_owned_local(base, exponent);
        one = expr_new_const(NUM_ONE);
        out = expr_div(one, denom);
        expr_free(one);
        expr_free(denom);
        return out;
    }
    if (expr_is_pow_d_expr(a) && expr_struct_eq(a->a, b)) {
        number_t exponent = num_sub(a->c, NUM_ONE);
        expr_t *base;

        expr_retain(a->a);
        base = a->a;
        expr_free(a);
        expr_free(b);
        return expr_make_pow_like_owned_local(base, exponent);
    }
    if (expr_is_op(a, &ops_pow) && expr_struct_eq(a->a, b)) {
        expr_t *base;
        expr_t *exponent;
        expr_t *one;
        expr_t *shifted;
        expr_t *out;

        expr_retain(a->a);
        expr_retain(a->b);
        base = a->a;
        exponent = a->b;
        expr_free(a);
        expr_free(b);

        one = expr_new_const(NUM_ONE);
        shifted = expr_sub(exponent, one);
        expr_free(exponent);
        expr_free(one);
        if (!shifted) {
            expr_free(base);
            return NULL;
        }

        out = expr_pow_xp(base, shifted);
        expr_free(base);
        expr_free(shifted);
        return out;
    }
    if (expr_is_op(a, &ops_mul) && expr_struct_eq(a->a, b)) {
        expr_t *rest;

        expr_retain(a->b);
        rest = a->b;
        expr_free(a);
        expr_free(b);
        return rest;
    }
    if (expr_is_op(a, &ops_mul) && expr_struct_eq(a->b, b)) {
        expr_t *rest;

        expr_retain(a->a);
        rest = a->a;
        expr_free(a);
        expr_free(b);
        return rest;
    }
    if (expr_simplify_is_plain_real_const(b) && expr_is_op(a, &ops_mul) && expr_simplify_is_plain_real_const(a->a)) {
        expr_t *rest;
        number_t folded = num_div(a->a->c, b->c);

        expr_retain(a->b);
        rest = a->b;
        expr_free(a);
        expr_free(b);
        expr_t *out = expr_make_scaled_owned_local(folded, rest);

        return out;
    }
    if (expr_simplify_is_plain_real_const(b) && expr_is_op(a, &ops_neg)) {
        expr_t *inner;
        expr_t *quot;
        expr_t *simp;

        expr_retain(a->a);
        inner = a->a;
        quot = expr_div(inner, b);
        expr_free(inner);
        expr_free(b);
        simp = expr_simplify(quot);
        expr_free(quot);
        expr_free(a);
        return expr_simplify_neg_operator(dv, simp, NULL);
    }
    if (expr_simplify_is_plain_real_const(b) && expr_is_addsub(a)) {
        number_t c_const = num_const(NUM_ZERO);
        number_t denom = NUM_ZERO;
        addend_t *terms = NULL;
        size_t n = 0, cap = 0;
        expr_t *cur = NULL;

        if (!num_is_real(b->c)) {
            num_destroy(&c_const);
            goto div_fallback_2;
        }
        denom = num_clone(b->c);

        expr_collect_addends(a, NUM_ONE, &c_const, &terms, &n, &cap);
        expr_free(a);
        expr_free(b);

        for (size_t i = 0; i < n; ++i) {
            expr_t *term;

            if (!terms[i].base || num_is_zero(terms[i].coeff)) {
                if (terms[i].base)
                    expr_free(terms[i].base);
                num_destroy(&terms[i].coeff);
                continue;
            }

            number_t scaled_coeff = num_div(terms[i].coeff, denom);

            term = expr_make_scaled_owned_local(scaled_coeff, terms[i].base);
            num_destroy(&terms[i].coeff);
            if (!cur)
                cur = term;
            else {
                expr_t *tmp = expr_add(cur, term);
                expr_free(cur);
                expr_free(term);
                cur = tmp;
            }
        }
        free(terms);

        if (!num_is_zero(c_const)) {
            number_t scaled_const = num_div(c_const, denom);
            expr_t *cterm = expr_new_const_owned_local(scaled_const);

            if (!cur)
                cur = cterm;
            else {
                expr_t *tmp = expr_add(cur, cterm);
                expr_free(cur);
                expr_free(cterm);
                cur = tmp;
            }
        }

        {
            expr_t *out = cur ? cur : expr_new_const(NUM_ZERO);

            num_destroy(&c_const);
            num_destroy(&denom);
            return out;
        }
    }
div_fallback_2:
    if (!expr_is_addsub(a) && (expr_is_op(a, &ops_mul) || expr_is_div(a) || expr_is_negative_real_power_local(a) ||
                               expr_is_op(b, &ops_mul) || expr_is_div(b) || expr_is_negative_real_power_local(b)))
        return expr_simplify_flat_quotient_local(a, b);

    if (expr_struct_eq(a, b) || expr_polynomials_equal_deg4(a, b)) {
        expr_free(a);
        expr_free(b);
        return expr_new_const(NUM_ONE);
    }
    if (expr_is_op(a, &ops_const) && expr_const_is_zero(a)) {
        expr_free(a);
        expr_free(b);
        return expr_new_const(NUM_ZERO);
    }
    if (expr_is_unnamed_const(a) && !a->binding_expr && expr_is_unnamed_const(b) && !b->binding_expr) {
        number_t q = num_div(a->c, b->c);

        expr_free(a);
        expr_free(b);
        expr_t *out = expr_new_const_owned_local(q);

        return out;
    }

    /* sinh(x)/cosh(x) → tanh(x) */
    if (expr_is_op(a, &ops_sinh) && expr_is_op(b, &ops_cosh) && expr_struct_eq(a->a, b->a)) {
        expr_retain(a->a);
        expr_t *r = expr_tanh(a->a);
        expr_free(a->a);
        expr_free(a);
        expr_free(b);
        return r;
    }
    /* sin(x)/cos(x) → tan(x) */
    if (expr_is_op(a, &ops_sin) && expr_is_op(b, &ops_cos) && expr_struct_eq(a->a, b->a)) {
        expr_retain(a->a);
        expr_t *r = expr_tan(a->a);
        expr_free(a->a);
        expr_free(a);
        expr_free(b);
        return r;
    }
    /* x/abs(x) → abs(x)/x  (canonical sign-function form) */
    if (expr_is_op(b, &ops_abs) && expr_struct_eq(a, b->a)) {
        expr_t *r = expr_div(b, a);
        expr_free(a);
        expr_free(b);
        return r;
    }

    expr_t *r = expr_div(a, b);
    expr_free(a);
    expr_free(b);
    return r;
}

/* --- */

static long expr_simplify_gcd_long_local(long a, long b)
{
    a = a < 0L ? -a : a;
    b = b < 0L ? -b : b;
    while (b != 0L) {
        long remainder = a % b;

        a = b;
        b = remainder;
    }
    return a;
}

static long expr_simplify_square_divisor_root_local(long value)
{
    long remaining = value;
    long root = 1L;

    for (long factor = 2L; factor <= remaining / factor; ++factor) {
        unsigned int exponent = 0u;

        while (remaining % factor == 0L) {
            remaining /= factor;
            exponent++;
        }
        for (unsigned int pair = 0u; pair < exponent / 2u; ++pair)
            root *= factor;
    }
    return root;
}

static expr_t *expr_simplify_binomial_three_halves_local(const expr_t *base)
{
    const expr_t *constant = NULL;
    const expr_t *radical = NULL;
    number_t constant_value = number_invalid();
    number_t radicand_value = number_invalid();
    long a, radicand, denominator, a_squared, a_cubed, three_a, plain_term, radical_term;
    long square_root;
    bool subtract_radical = false;
    expr_t *radicand_constant = NULL;
    expr_t *radical_expr = NULL;
    expr_t *scaled_radical = NULL;
    expr_t *plain_expr = NULL;
    expr_t *expanded = NULL;
    expr_t *root = NULL;
    expr_t *out = NULL;

    if (expr_is_op(base, &ops_add)) {
        if (expr_is_const(base->a) && expr_is_sqrt_expr(base->b)) {
            constant = base->a;
            radical = base->b;
        } else if (expr_is_const(base->a) && expr_is_neg(base->b) && expr_is_sqrt_expr(base->b->a)) {
            constant = base->a;
            radical = base->b->a;
            subtract_radical = true;
        } else if (expr_is_sqrt_expr(base->a) && expr_is_const(base->b)) {
            constant = base->b;
            radical = base->a;
        } else if (expr_is_neg(base->a) && expr_is_sqrt_expr(base->a->a) && expr_is_const(base->b)) {
            constant = base->b;
            radical = base->a->a;
            subtract_radical = true;
        }
    } else if (expr_is_op(base, &ops_sub) && expr_is_const(base->a) && expr_is_sqrt_expr(base->b)) {
        constant = base->a;
        radical = base->b;
        subtract_radical = true;
    }
    if (!constant || !radical || !expr_match_const_value(constant, &constant_value) ||
        !expr_match_const_value(radical->a, &radicand_value) ||
        !num_get_small_rational(constant_value, &a, &denominator) || denominator != 1L ||
        !num_get_small_rational(radicand_value, &radicand, &denominator) || denominator != 1L || a <= 0L ||
        radicand <= 0L || __builtin_mul_overflow(a, a, &a_squared) ||
        __builtin_mul_overflow(a_squared, a, &a_cubed) ||
        __builtin_mul_overflow(3L, a, &three_a) || __builtin_mul_overflow(three_a, radicand, &plain_term) ||
        __builtin_add_overflow(a_cubed, plain_term, &plain_term) ||
        __builtin_mul_overflow(3L, a_squared, &radical_term) ||
        __builtin_add_overflow(radical_term, radicand, &radical_term))
        goto cleanup;

    square_root = expr_simplify_square_divisor_root_local(expr_simplify_gcd_long_local(plain_term, radical_term));
    plain_term /= square_root * square_root;
    radical_term /= square_root * square_root;
    radicand_constant = expr_const_long(radicand);
    radical_expr = radicand_constant ? expr_sqrt(radicand_constant) : NULL;
    scaled_radical = radical_expr ? expr_mul_long(radical_expr, radical_term) : NULL;
    plain_expr = expr_const_long(plain_term);
    expanded = plain_expr && scaled_radical ? (subtract_radical ? expr_sub(plain_expr, scaled_radical)
                                                                : expr_add(plain_expr, scaled_radical))
                               : NULL;
    root = expanded ? expr_sqrt(expanded) : NULL;
    out = root ? expr_mul_long(root, square_root) : NULL;
    out = expr_simplify_owned(out);

cleanup:
    expr_free(plain_expr);
    expr_free(root);
    expr_free(expanded);
    expr_free(scaled_radical);
    expr_free(radical_expr);
    expr_free(radicand_constant);
    num_destroy(&radicand_value);
    num_destroy(&constant_value);
    return out;
}

expr_t *expr_simplify_pow_d_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    NUM_SCOPE(scope);
    (void)b;
    number_t exponent = dv->c;

    if (num_eq(exponent, NUM_ONE)) {
        return a;
    }
    if (num_eq(exponent, NUM_ZERO)) {
        expr_free(a);
        return expr_new_const(NUM_ONE);
    }

    {
        long numerator;
        long denominator;

        if (num_get_small_rational(exponent, &numerator, &denominator) && numerator == 3L && denominator == 2L) {
            expr_t *expanded_root = expr_simplify_binomial_three_halves_local(a);

            if (expanded_root) {
                expr_free(a);
                return expanded_root;
            }
        }
    }

    if (expr_is_pow_d_expr(a)) {
        number_t folded_exponent = num_mul(a->c, exponent);
        expr_t *base;

        expr_retain(a->a);
        base = a->a;
        expr_free(a);
        return expr_make_pow_like_owned_local(base, folded_exponent);
    }

    /* (a^b)^n -> a^(bn), limited to integer n to avoid changing
     * principal-branch behaviour for arbitrary complex powers. */
    if (expr_is_op(a, &ops_pow) && num_is_real(exponent) && num_is_integer(exponent)) {
        expr_t *coefficient = expr_new_const(exponent);
        expr_t *base;
        expr_t *inner_exponent;
        expr_t *scaled_raw;
        expr_t *scaled_exponent;
        expr_t *raw_out;
        expr_t *out;

        expr_retain(a->a);
        expr_retain(a->b);
        base = a->a;
        inner_exponent = a->b;

        scaled_raw = expr_mul(coefficient, inner_exponent);
        scaled_exponent = expr_simplify(scaled_raw);
        raw_out = expr_pow_xp(base, scaled_exponent);
        out = expr_simplify(raw_out);

        expr_free(raw_out);
        expr_free(scaled_exponent);
        expr_free(scaled_raw);
        expr_free(inner_exponent);
        expr_free(base);
        expr_free(coefficient);
        expr_free(a);
        return out;
    }

    {
        expr_t *i_power = expr_try_simplify_i_power_local(a, exponent);

        if (i_power)
            return i_power;
    }

    {
        expr_t *preserved_power = expr_try_simplify_preserved_integer_power_local(a, exponent);

        if (preserved_power)
            return preserved_power;
    }

    {
        expr_t *preserved_power = expr_try_simplify_preserved_rational_const_power_local(a, exponent);

        if (preserved_power)
            return preserved_power;
    }

    if (expr_simplify_is_plain_real_const(a)) {
        number_t v = num_pow(a->c, exponent);

        expr_free(a);
        expr_t *out = expr_new_const_owned_local(v);

        return out;
    }

    /* sqrt(x)^n → x^(n/2) */
    if (expr_is_sqrt_expr(a)) {
        number_t half = num_div(exponent, NUM_TWO);
        expr_retain(a->a);
        expr_t *inner = a->a;
        expr_free(a);
        expr_t *out = expr_make_pow_like_owned_local(inner, half);

        return out;
    }

    /* exp(x)^n -> exp(nx). Keep this to integer powers: fractional and
     * arbitrary complex powers have principal-branch subtleties. */
    if (expr_is_exp_expr(a) && num_is_real(exponent) && num_is_integer(exponent)) {
        expr_t *coefficient = expr_new_const(exponent);
        expr_t *inner;
        expr_t *raw_arg;
        expr_t *raw_out;
        expr_t *out;

        expr_retain(a->a);
        inner = a->a;
        raw_arg = expr_mul(coefficient, inner);
        raw_out = expr_exp(raw_arg);
        out = expr_simplify(raw_out);
        expr_free(raw_out);
        expr_free(raw_arg);
        expr_free(inner);
        expr_free(coefficient);
        expr_free(a);
        return out;
    }

    if (num_eq(exponent, NUM_TWO)) {
        expr_t *euler_square = expr_simplify_try_euler_square_local(a);

        if (euler_square)
            return euler_square;
    }

    /* (ab)^2 -> a^2 b^2. Keep this deliberately narrow: it exposes factors
     * like (2√x)^2 to the product simplifier without expanding sums. */
    if (num_eq(exponent, NUM_TWO) && expr_is_op(a, &ops_mul)) {
        expr_t *left_pow = expr_pow(a->a, &exponent);
        expr_t *right_pow = expr_pow(a->b, &exponent);
        expr_t *raw = expr_mul(left_pow, right_pow);
        expr_t *out;

        expr_free(left_pow);
        expr_free(right_pow);
        expr_free(a);
        out = expr_simplify(raw);
        expr_free(raw);
        return out;
    }

    return expr_make_pow_like_owned_local(a, exponent);
}

/* --- */

expr_t *expr_simplify_pow_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    (void)dv;
    if (expr_is_op(b, &ops_const) && expr_const_is_one(b)) {
        expr_free(b);
        return a;
    }
    if (expr_is_op(b, &ops_const) && expr_const_is_zero(b)) {
        expr_free(a);
        expr_free(b);
        return expr_new_const(NUM_ONE);
    }
    {
        number_t exponent = number_invalid();
        long numerator;
        long denominator;

        if (expr_match_const_value(b, &exponent) && num_get_small_rational(exponent, &numerator, &denominator) &&
            numerator == 3L && denominator == 2L) {
            expr_t *expanded_root = expr_simplify_binomial_three_halves_local(a);

            num_destroy(&exponent);
            if (expanded_root) {
                expr_free(a);
                expr_free(b);
                return expanded_root;
            }
        } else {
            num_destroy(&exponent);
        }
    }
    if (expr_simplify_is_simplifiable_const(a) && expr_const_is_one(a)) {
        expr_free(a);
        expr_free(b);
        return expr_new_const(NUM_ONE);
    }

    if (expr_is_op(b, &ops_const)) {
        expr_t *i_power = expr_try_simplify_i_power_local(a, b->c);

        if (i_power) {
            expr_free(b);
            return i_power;
        }
    }

    if (expr_simplify_is_literal_euler_const_local(a)) {
        expr_t *raw;
        expr_t *out;

        raw = expr_exp(b);
        out = expr_simplify(raw);
        expr_free(raw);
        expr_free(a);
        expr_free(b);
        return out;
    }
    if (expr_simplify_is_literal_ten_const_local(a) && expr_is_op(b, &ops_log10)) {
        expr_t *inner = b->a;
        expr_retain(inner);
        expr_free(a);
        expr_free(b);
        return inner;
    }
    expr_t *r = expr_pow_xp(a, b);
    expr_free(a);
    expr_free(b);
    return r;
}

/* --- */

expr_t *expr_simplify_hypot_operator(const expr_t *dv, expr_t *a, expr_t *b)
{
    (void)dv;

    if (expr_is_op(a, &ops_const) && expr_const_is_zero(a)) {
        expr_free(a);
        expr_t *r = expr_abs(b);
        expr_free(b);
        return r;
    }
    if (expr_is_op(b, &ops_const) && expr_const_is_zero(b)) {
        expr_free(b);
        expr_t *r = expr_abs(a);
        expr_free(a);
        return r;
    }

    return expr_simplify_binary_operator(dv, a, b);
}

/* ========================================================================= */
/* Main dispatcher                                                            */
/* ========================================================================= */

static uint64_t expr_simplify_subtree_epoch(const expr_t *dv)
{
    uint64_t epoch;
    uint64_t child_epoch;

    if (!dv)
        return 0;

    epoch = dv->epoch;

    child_epoch = expr_simplify_subtree_epoch(dv->a);
    if (child_epoch > epoch)
        epoch = child_epoch;

    child_epoch = expr_simplify_subtree_epoch(dv->b);
    if (child_epoch > epoch)
        epoch = child_epoch;

    return epoch;
}

static bool expr_simplify_is_current(const expr_t *dv)
{
    return dv && dv->simplified && expr_simplify_subtree_epoch(dv) <= dv->simplify_epoch;
}

static expr_t *expr_simplify_mark_current(expr_t *dv)
{
    if (dv) {
        dv->simplified = true;
        dv->simplify_epoch = expr_simplify_subtree_epoch(dv);
    }
    return dv;
}

static expr_t *expr_simplify_once(const expr_t *dv)
{
    expr_t *out;

    if (!dv)
        return NULL;

    if (expr_simplify_is_current(dv)) {
        expr_retain(dv);
        return (expr_t *)dv;
    }

    if (dv->ops->arity == EXPR_OP_ATOM) {
        expr_retain(dv);
        return expr_simplify_mark_current((expr_t *)dv);
    }

    expr_t *a = dv->a ? expr_simplify(dv->a) : NULL;
    expr_t *b = dv->b ? expr_simplify(dv->b) : NULL;

    if (dv->ops->simplify)
        out = dv->ops->simplify(dv, a, b);
    else
        out = expr_simplify_passthrough(dv, a, b);

    if (out == dv)
        return expr_simplify_mark_current(out);

    return out;
}

expr_t *expr_simplify(const expr_t *dv)
{
    enum { MAX_SIMPLIFY_PASSES = 64 };
    expr_t *cur;
    unsigned pass;

    if (!dv)
        return NULL;

    if (expr_simplify_is_current(dv)) {
        expr_retain(dv);
        return (expr_t *)dv;
    }

    cur = expr_simplify_once(dv);
    if (!cur)
        return NULL;

    for (pass = 0u; pass < MAX_SIMPLIFY_PASSES; ++pass) {
        expr_t *next;

        if (expr_simplify_is_current(cur))
            return cur;

        next = expr_simplify_once(cur);
        if (!next) {
            expr_free(cur);
            return NULL;
        }

        if (next == cur) {
            expr_free(cur);
            return expr_simplify_mark_current(next);
        }

        if (expr_struct_eq(next, cur)) {
            expr_free(cur);
            return expr_simplify_mark_current(next);
        }

        expr_free(cur);
        cur = next;
    }

    return cur;
}
