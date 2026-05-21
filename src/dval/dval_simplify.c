/* dval_simplify.c - algebraic simplification of differentiable value nodes
 *
 * dv_simplify() rewrites a DAG node into a canonical form using a small set
 * of structural rules applied bottom-up.  It is called automatically by
 * dv_create_deriv() so that derivative expressions stay readable.
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
 * dv_simplify() returns a new owning node (refcount = 1).  The input node is
 * borrowed; its refcount is not changed.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval_bindings_internal.h"
#include "dval_internal.h"
#include "internal/number_internal.h"
#include "number/number_internal.h"

/* forward declaration — helpers below call dv_simplify recursively */
dval_t *dv_simplify(const dval_t *dv);

static void *dv_xrealloc(void *ptr, size_t size)
{
    void *grown = realloc(ptr, size);

    if (grown)
        return grown;

    fprintf(stderr, "dval_simplify: out of memory\n");
    abort();
}

static inline dval_t *dv_new_const_owned_local(number_t value)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    dval_t *out = dv_new_const(value);

    return out;
}

static inline dval_t *dv_make_scaled_owned_local(number_t coeff, dval_t *base)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    dval_t *out = dv_make_scaled(coeff, base);

    return out;
}

static inline dval_t *dv_make_pow_like_owned_local(dval_t *base, number_t exponent)
{
    NUM_SCOPE_SUSPEND(saved_scope);
    dval_t *out = dv_make_pow_like(base, exponent);

    return out;
}

typedef struct {
    dval_t *base;
    number_t exponent;
} dv_factor_t;

static void dv_free_factors_local(dv_factor_t *factors, size_t n)
{
    size_t i;

    for (i = 0u; i < n; ++i) {
        dv_free(factors[i].base);
        num_destroy(&factors[i].exponent);
    }
    free(factors);
}

static void dv_append_factor_local(dv_factor_t **factors, size_t *n,
                                   size_t *cap, const dval_t *base,
                                   number_t exponent)
{
    size_t i;

    for (i = 0u; i < *n; ++i) {
        if (dv_struct_eq((*factors)[i].base, base)) {
            number_t sum = num_add((*factors)[i].exponent, exponent);

            num_destroy(&(*factors)[i].exponent);
            (*factors)[i].exponent = num_scope_detach(sum);
            return;
        }
    }

    if (*n == *cap) {
        *cap = *cap ? *cap * 2u : 8u;
        *factors = dv_xrealloc(*factors, *cap * sizeof(**factors));
    }

    dv_retain(base);
    (*factors)[*n].base = (dval_t *)base;
    (*factors)[*n].exponent = num_scope_detach(num_clone(exponent));
    ++(*n);
}

static void dv_split_product_factors_scaled_local(const dval_t *dv,
                                                  number_t sign,
                                                  dv_factor_t **factors,
                                                  size_t *n, size_t *cap)
{
    if (!dv)
        return;
    if (dv_is_op(dv, &ops_mul)) {
        dv_split_product_factors_scaled_local(dv->a, sign, factors, n, cap);
        dv_split_product_factors_scaled_local(dv->b, sign, factors, n, cap);
        return;
    }
    if (dv_is_div(dv)) {
        number_t neg_sign = num_neg(sign);

        dv_split_product_factors_scaled_local(dv->a, sign, factors, n, cap);
        dv_split_product_factors_scaled_local(dv->b, neg_sign, factors, n, cap);
        num_destroy(&neg_sign);
        return;
    }
    if (dv_is_pow_d_expr(dv)) {
        number_t exponent = num_mul(dv->c, sign);

        dv_append_factor_local(factors, n, cap, dv->a, exponent);
        num_destroy(&exponent);
        return;
    }
    dv_append_factor_local(factors, n, cap, dv, sign);
}

static void dv_split_product_factors_local(const dval_t *dv,
                                           dv_factor_t **factors,
                                           size_t *n, size_t *cap)
{
    dv_split_product_factors_scaled_local(dv, NUM_ONE, factors, n, cap);
}

static dval_t *dv_rebuild_factors_local(dv_factor_t *factors, size_t n)
{
    dval_t **num_terms = NULL;
    dval_t **den_terms = NULL;
    size_t nnum = 0u;
    size_t nden = 0u;
    size_t num_cap = 0u;
    size_t den_cap = 0u;
    dval_t *numerator;
    dval_t *denominator;
    dval_t *out;
    size_t i;

    for (i = 0u; i < n; ++i) {
        dval_t *factor;

        if (num_is_zero(factors[i].exponent)) {
            dv_free(factors[i].base);
            num_destroy(&factors[i].exponent);
            continue;
        }

        if (num_lt(factors[i].exponent, NUM_ZERO)) {
            number_t den_exponent = num_neg(factors[i].exponent);

            factor = dv_make_pow_like_owned_local(factors[i].base,
                                                  den_exponent);
            dv_append_node(&den_terms, &nden, &den_cap, factor);
        } else {
            factor = dv_make_pow_like_owned_local(factors[i].base,
                                                  factors[i].exponent);
            dv_append_node(&num_terms, &nnum, &num_cap, factor);
        }
        num_destroy(&factors[i].exponent);
    }

    free(factors);
    numerator = dv_rebuild_product_chain(NUM_ONE, num_terms, nnum);
    if (nden == 0u) {
        free(den_terms);
        return numerator;
    }

    denominator = dv_rebuild_division_chain(den_terms, nden);
    out = dv_div(numerator, denominator);
    dv_free(numerator);
    dv_free(denominator);
    return out;
}

static void dv_keep_common_factors_local(dv_factor_t *common, size_t *ncommon,
                                         const dv_factor_t *factors,
                                         size_t n)
{
    size_t i;

    for (i = 0u; i < *ncommon; ++i) {
        size_t j;
        int found = 0;

        for (j = 0u; j < n; ++j) {
            if (!dv_struct_eq(common[i].base, factors[j].base))
                continue;

            if (num_lt(factors[j].exponent, common[i].exponent)) {
                num_destroy(&common[i].exponent);
                common[i].exponent = num_clone(factors[j].exponent);
            }
            found = 1;
            break;
        }

        if (!found)
            num_destroy(&common[i].exponent),
            common[i].exponent = num_clone(NUM_ZERO);
    }
}

static int dv_common_factors_nonempty_local(const dv_factor_t *factors,
                                            size_t n)
{
    size_t i;

    for (i = 0u; i < n; ++i) {
        if (!num_is_zero(factors[i].exponent))
            return 1;
    }
    return 0;
}

static int dv_common_factors_useful_local(const dv_factor_t *factors, size_t n)
{
    size_t i;

    for (i = 0u; i < n; ++i) {
        if (num_is_zero(factors[i].exponent))
            continue;
        if (!dv_is_op(factors[i].base, &ops_var))
            return 1;
    }
    return 0;
}

static dval_t *dv_reduce_by_common_factors_local(const dval_t *base,
                                                 const dv_factor_t *common,
                                                 size_t ncommon)
{
    dv_factor_t *factors = NULL;
    size_t n = 0u;
    size_t cap = 0u;
    size_t i;

    dv_split_product_factors_local(base, &factors, &n, &cap);

    for (i = 0u; i < ncommon; ++i) {
        size_t j;

        if (num_is_zero(common[i].exponent))
            continue;

        for (j = 0u; j < n; ++j) {
            number_t diff;

            if (!dv_struct_eq(factors[j].base, common[i].base))
                continue;

            diff = num_sub(factors[j].exponent, common[i].exponent);
            num_destroy(&factors[j].exponent);
            factors[j].exponent = diff;
            break;
        }
    }

    return dv_rebuild_factors_local(factors, n);
}

static dval_t *dv_try_factor_common_symbolic_product_local(dval_t *sum)
{
    number_t c_const = num_const(NUM_ZERO);
    addend_t *terms = NULL;
    size_t n = 0u;
    size_t cap = 0u;
    dv_factor_t *common = NULL;
    size_t ncommon = 0u;
    size_t common_cap = 0u;
    dval_t *inner = NULL;
    dval_t *common_factor;
    dval_t *factored;
    size_t i;
    size_t nonzero = 0u;

    dv_collect_addends(sum, NUM_ONE, &c_const, &terms, &n, &cap);
    if (!num_is_zero(c_const))
        goto no_factor;

    for (i = 0u; i < n; ++i) {
        if (terms[i].base && !num_is_zero(terms[i].coeff))
            ++nonzero;
    }
    if (nonzero < 2u)
        goto no_factor;

    for (i = 0u; i < n; ++i) {
        dv_factor_t *factors = NULL;
        size_t nfactors = 0u;
        size_t fcap = 0u;

        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;

        dv_split_product_factors_local(terms[i].base, &factors, &nfactors, &fcap);
        if (!common) {
            common = factors;
            ncommon = nfactors;
            common_cap = fcap;
            continue;
        }

        dv_keep_common_factors_local(common, &ncommon, factors, nfactors);
        dv_free_factors_local(factors, nfactors);
    }

    (void)common_cap;
    if (!dv_common_factors_nonempty_local(common, ncommon) ||
        !dv_common_factors_useful_local(common, ncommon))
        goto no_factor;

    for (i = 0u; i < n; ++i) {
        dval_t *reduced;
        dval_t *term;

        if (!terms[i].base || num_is_zero(terms[i].coeff)) {
            if (terms[i].base)
                dv_free(terms[i].base);
            num_destroy(&terms[i].coeff);
            continue;
        }

        reduced = dv_reduce_by_common_factors_local(terms[i].base, common, ncommon);
        if (reduced)
            term = dv_make_scaled_owned_local(terms[i].coeff, reduced);
        else
            term = dv_new_const_owned_local(terms[i].coeff);

        dv_free(terms[i].base);
        num_destroy(&terms[i].coeff);

        if (!inner) {
            inner = term;
        } else {
            dval_t *tmp = dv_add(inner, term);

            dv_free(inner);
            dv_free(term);
            inner = tmp;
        }
    }

    free(terms);
    num_destroy(&c_const);

    if (!inner) {
        dv_free_factors_local(common, ncommon);
        return NULL;
    }

    {
        dval_t *simplified_inner = dv_simplify(inner);

        dv_free(inner);
        inner = simplified_inner;
    }

    common_factor = dv_rebuild_factors_local(common, ncommon);
    if (!common_factor) {
        dv_free(inner);
        return NULL;
    }

    factored = dv_mul(common_factor, inner);
    dv_free(common_factor);
    dv_free(inner);
    return factored;

no_factor:
    if (common)
        dv_free_factors_local(common, ncommon);
    for (i = 0u; i < n; ++i) {
        dv_free(terms[i].base);
        num_destroy(&terms[i].coeff);
    }
    free(terms);
    num_destroy(&c_const);
    return NULL;
}

static long dv_gcd_long_local(long a, long b)
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

static long dv_lcm_long_local(long a, long b)
{
    long g;

    if (a < 0L)
        a = -a;
    if (b < 0L)
        b = -b;
    if (a == 0L || b == 0L)
        return 0L;
    g = dv_gcd_long_local(a, b);
    return g ? (a / g) * b : 0L;
}

static int dv_number_small_rational_local(number_t value, long *numerator, long *denominator)
{
    const number_private_t *impl = number_impl_const(&value);
    const mint_t *num_mint;
    const mint_t *den_mint;
    long n;
    long d;

    if (!num_is_real(value))
        return 0;

    if (impl->kind == NUMBER_MINT) {
        if (!mi_get_long(impl->value.mi, &n))
            return 0;
        *numerator = n;
        *denominator = 1L;
        return 1;
    }

    if (impl->kind != NUMBER_MRATIONAL)
        return 0;

    num_mint = mr_numerator(impl->value.mr);
    den_mint = mr_denominator(impl->value.mr);
    if (!mi_get_long(num_mint, &n) || !mi_get_long(den_mint, &d) || d == 0L)
        return 0;
    if (d < 0L) {
        n = -n;
        d = -d;
    }
    *numerator = n;
    *denominator = d;
    return 1;
}

static int dv_update_abs_coeff_rational_gcd_local(number_t abs_coeff,
                                                  long *gcd_numer,
                                                  long *lcm_denom)
{
    long numer;
    long denom;

    if (!dv_number_small_rational_local(abs_coeff, &numer, &denom))
        return 0;
    if (numer < 0L)
        numer = -numer;
    if (numer == 0L)
        return 1;
    *gcd_numer = *gcd_numer ? dv_gcd_long_local(*gcd_numer, numer) : numer;
    *lcm_denom = *lcm_denom ? dv_lcm_long_local(*lcm_denom, denom) : denom;
    return *lcm_denom != 0L;
}

static number_t dv_number_from_small_rational_local(long numerator, long denominator)
{
    if (denominator == 1L)
        return num_create_from_long(numerator);
    {
        mrational_t *rational = mr_create_frac_long(numerator, denominator);
        number_t value = rational ? num_create_from_mrational(rational)
                                  : num_create_from_long(numerator);

        mr_free(rational);
        return value;
    }
}

static int dv_update_abs_coeff_gcd_local(number_t abs_coeff, long *gcd_value)
{
    if (!num_is_integer(abs_coeff))
        return 0;
    double d = num_to_double(abs_coeff);

    if (d < 1.0 || d > 1000000000.0)
        return 0;
    long value = (long)(d + 0.5);

    if ((double)value != d)
        return 0;
    *gcd_value = *gcd_value ? dv_gcd_long_local(*gcd_value, value) : value;
    return 1;
}

static int dv_try_common_abs_coeff_local(const addend_t *terms, size_t n,
                                         number_t c_const, number_t *out)
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
        if (!dv_update_abs_coeff_gcd_local(abs_coeff, &gcd_value))
            gcd_common = 0;
        if (!dv_update_abs_coeff_rational_gcd_local(abs_coeff,
                                                   &rational_gcd_numer,
                                                   &rational_lcm_denom))
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

        if (gcd_common && !dv_update_abs_coeff_gcd_local(abs_coeff, &gcd_value))
            gcd_common = 0;
        if (rational_common &&
            !dv_update_abs_coeff_rational_gcd_local(abs_coeff,
                                                   &rational_gcd_numer,
                                                   &rational_lcm_denom))
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
        common = dv_number_from_small_rational_local(rational_gcd_numer,
                                                     rational_lcm_denom);
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

static bool dv_contains_var_local(const dval_t *dv)
{
    if (!dv)
        return false;
    if (dv_is_var(dv))
        return true;
    return dv_contains_var_local(dv->a) || dv_contains_var_local(dv->b);
}

static bool dv_is_lambert_expr_local(const dval_t *dv)
{
    return dv_is_op(dv, &ops_lambert_w0) || dv_is_op(dv, &ops_lambert_wm1);
}

static bool dv_is_simplifiable_const_local(const dval_t *dv)
{
    return dv_is_op(dv, &ops_const) &&
           (!dv->name || !*dv->name || !dv->binding_expr);
}

static dval_t *dv_try_simplify_lambert_product(dval_t *a, dval_t *b)
{
    dval_t *w;
    dval_t *exp_term;
    dval_t *inner;

    if (dv_is_lambert_expr_local(a) && dv_is_exp_expr(b)) {
        w = a;
        exp_term = b;
    } else if (dv_is_lambert_expr_local(b) && dv_is_exp_expr(a)) {
        w = b;
        exp_term = a;
    } else {
        return NULL;
    }

    if (!dv_struct_eq(w, exp_term->a))
        return NULL;

    inner = w->a;

    if (!dv_current_wrt_internal() && dv_is_var(inner) && inner->binding_expr)
        return dv_binding_expr_eval_dval(inner->binding_expr);

    dv_retain(inner);
    return inner;
}

static dval_t *dv_try_simplify_exp_quarter_turn(const dval_t *arg)
{
    NUM_SCOPE(scope);
    number_t arg_value;
    number_t exp_value;
    number_t neg_i;

    if (!arg || dv_contains_var_local(arg))
        return NULL;

    arg_value = dv_eval(arg);
    exp_value = num_exp(arg_value);
    neg_i = num_neg(NUM_I);

    if (num_eq(exp_value, NUM_I)) {
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return dv_new_named_const(NUM_I, "i");
    }
    if (num_eq(exp_value, NUM_NEG_ONE)) {
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return dv_new_const(NUM_NEG_ONE);
    }
    if (num_eq(exp_value, neg_i)) {
        dval_t *i = dv_new_named_const(NUM_I, "i");
        dval_t *out = dv_neg(i);

        dv_free(i);
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return out;
    }
    if (num_eq(exp_value, NUM_ONE)) {
        num_destroy(&neg_i);
        num_destroy(&exp_value);
        num_destroy(&arg_value);
        return dv_new_const(NUM_ONE);
    }

    num_destroy(&neg_i);
    num_destroy(&exp_value);
    num_destroy(&arg_value);
    return NULL;
}

int dv_fold_zero_to_zero(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ZERO;
    return 1;
}

int dv_fold_cos_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ONE;
    return 1;
}

int dv_fold_exp_const(const number_t *in, number_t *out)
{
    if (!in || !out || !num_eq(*in, NUM_ZERO))
        return 0;
    *out = NUM_ONE;
    return 1;
}

int dv_fold_log_const(const number_t *in, number_t *out)
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

int dv_fold_sqrt_const(const number_t *in, number_t *out)
{
    if (!in || !out)
        return 0;
    if (num_eq(*in, NUM_ZERO)) {
        *out = NUM_ZERO;
        return 1;
    }
    if (num_eq(*in, NUM_ONE)) {
        *out = NUM_ONE;
        return 1;
    }
    return 0;
}

/* ========================================================================= */
/* Multiplication flattening                                                  */
/* ========================================================================= */

static void collect_mul_flat(
    dval_t *dv,
    number_t *c_acc, int *is_zero,
    dval_t ***terms, size_t *nterms, size_t *cap)
{
    NUM_SCOPE(scope);
    if (*is_zero) {
        num_scope_leave(&(scope));
        return;
    }

    if (dv_is_unnamed_const(dv) && num_is_real(dv->c) &&
        (!dv->binding_expr || dv->binding_expr->kind == DV_BINDING_EXPR_NUMBER)) {
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
    if (dv_is_unnamed_const(dv) && num_is_real(dv->c) && dv->binding_expr) {
        number_t coeff;
        dv_binding_expr_t *rest_expr = NULL;

        if (dv_binding_expr_split_leading_number(dv->binding_expr,
                                                &coeff,
                                                &rest_expr)) {
            if (num_is_zero(coeff)) {
                num_destroy(&coeff);
                dv_binding_expr_free(rest_expr);
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
                dval_t *rest = dv_binding_expr_eval_dval(rest_expr);

                dv_binding_expr_free(rest_expr);
                if (*nterms == *cap) {
                    *cap   = (*cap == 0 ? 4 : *cap * 2);
                    *terms = dv_xrealloc(*terms, *cap * sizeof(dval_t *));
                }
                (*terms)[(*nterms)++] = rest;
            }
            num_scope_leave(&(scope));
            return;
        }
    }
    if (dv_is_op(dv, &ops_neg)) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t negated = num_neg(*c_acc);

        num_destroy(c_acc);
        *c_acc = negated;
        num_scope_leave(&(scope));
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (dv_is_op(dv, &ops_mul)) {
        num_scope_leave(&(scope));
        collect_mul_flat(dv->a, c_acc, is_zero, terms, nterms, cap);
        collect_mul_flat(dv->b, c_acc, is_zero, terms, nterms, cap);
        return;
    }
    if (*nterms == *cap) {
        *cap   = (*cap == 0 ? 4 : *cap * 2);
        *terms = dv_xrealloc(*terms, *cap * sizeof(dval_t *));
    }
    dv_retain(dv);
    (*terms)[(*nterms)++] = dv;
    num_scope_leave(&(scope));
}

static int dv_is_foldable_real_const_local(const dval_t *dv)
{
    return dv_is_unnamed_const(dv) && num_is_real(dv->c) &&
           (!dv->binding_expr || dv->binding_expr->kind == DV_BINDING_EXPR_NUMBER);
}

static void collect_quotient_flat(
    dval_t *dv, int in_denominator,
    number_t *c_acc, int *is_zero,
    dval_t ***terms, size_t *nterms, size_t *term_cap,
    dval_t ***den_terms, size_t *nden_terms, size_t *den_cap)
{
    NUM_SCOPE(scope);

    if (*is_zero) {
        num_scope_leave(&(scope));
        return;
    }

    if (dv_is_foldable_real_const_local(dv)) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t next;

        if (num_is_zero(dv->c) && !in_denominator) {
            *is_zero = 1;
            num_scope_leave(&(scope));
            return;
        }

        next = in_denominator ? num_div(*c_acc, dv->c)
                              : num_mul(*c_acc, dv->c);
        num_destroy(c_acc);
        *c_acc = next;
        num_scope_leave(&(scope));
        return;
    }

    if (dv_is_op(dv, &ops_neg)) {
        NUM_SCOPE_SUSPEND(saved_scope);
        number_t negated = num_neg(*c_acc);

        num_destroy(c_acc);
        *c_acc = negated;
        num_scope_leave(&(scope));
        collect_quotient_flat(dv->a, in_denominator, c_acc, is_zero,
                              terms, nterms, term_cap,
                              den_terms, nden_terms, den_cap);
        return;
    }

    if (dv_is_op(dv, &ops_mul)) {
        num_scope_leave(&(scope));
        collect_quotient_flat(dv->a, in_denominator, c_acc, is_zero,
                              terms, nterms, term_cap,
                              den_terms, nden_terms, den_cap);
        collect_quotient_flat(dv->b, in_denominator, c_acc, is_zero,
                              terms, nterms, term_cap,
                              den_terms, nden_terms, den_cap);
        return;
    }

    if (dv_is_div(dv)) {
        num_scope_leave(&(scope));
        collect_quotient_flat(dv->a, in_denominator, c_acc, is_zero,
                              terms, nterms, term_cap,
                              den_terms, nden_terms, den_cap);
        collect_quotient_flat(dv->b, !in_denominator, c_acc, is_zero,
                              terms, nterms, term_cap,
                              den_terms, nden_terms, den_cap);
        return;
    }

    if (in_denominator) {
        dv_retain(dv);
        dv_append_node(den_terms, nden_terms, den_cap, dv);
    } else {
        dv_retain(dv);
        dv_append_node(terms, nterms, term_cap, dv);
    }

    num_scope_leave(&(scope));
}

static dval_t *dv_simplify_flat_quotient_local(dval_t *a, dval_t *b)
{
    number_t c_acc = num_const(NUM_ONE);
    dval_t **terms = NULL;
    dval_t **den_terms = NULL;
    size_t nterms = 0u;
    size_t nden_terms = 0u;
    size_t term_cap = 0u;
    size_t den_cap = 0u;
    int is_zero = 0;
    dval_t *numerator;
    dval_t *denominator;
    dval_t *out;

    collect_quotient_flat(a, 0, &c_acc, &is_zero,
                          &terms, &nterms, &term_cap,
                          &den_terms, &nden_terms, &den_cap);
    collect_quotient_flat(b, 1, &c_acc, &is_zero,
                          &terms, &nterms, &term_cap,
                          &den_terms, &nden_terms, &den_cap);
    dv_free(a);
    dv_free(b);

    if (is_zero) {
        dv_free_node_array(terms, nterms);
        dv_free_node_array(den_terms, nden_terms);
        num_destroy(&c_acc);
        return dv_new_const(NUM_ZERO);
    }

    dv_combine_like_powers(terms, nterms);
    dv_combine_like_powers(den_terms, nden_terms);
    dv_cancel_common_powers(terms, nterms, den_terms, nden_terms);
    dv_combine_exp_terms(terms, nterms);
    dv_merge_sqrt_terms(terms, nterms);
    dv_merge_sqrt_terms(den_terms, nden_terms);

    {
        number_t four = num_create_from_long(4L);
        number_t quarter = num_div(NUM_ONE, four);

        if (num_eq(c_acc, quarter)) {
            dval_t *four_node = dv_new_const(four);

            num_destroy(&c_acc);
            c_acc = num_clone(NUM_ONE);
            dv_append_node(&den_terms, &nden_terms, &den_cap, four_node);
        }

        num_destroy(&quarter);
        num_destroy(&four);
    }

    numerator = dv_rebuild_product_chain(c_acc, terms, nterms);
    num_destroy(&c_acc);

    if (nden_terms == 0u) {
        free(den_terms);
        return numerator;
    }

    denominator = dv_rebuild_division_chain(den_terms, nden_terms);
    out = dv_div(numerator, denominator);
    dv_free(numerator);
    dv_free(denominator);
    return out;
}

static int dv_contains_addsub_local(const dval_t *dv)
{
    if (!dv)
        return 0;
    if (dv_is_addsub(dv))
        return 1;
    return dv_contains_addsub_local(dv->a) ||
           dv_contains_addsub_local(dv->b);
}

/* ========================================================================= */
/* Unary function simplification                                             */
/* ========================================================================= */

dval_t *dv_simplify_passthrough(const dval_t *dv, dval_t *a, dval_t *b)
{
    if (a)
        dv_free(a);
    if (b)
        dv_free(b);
    dv_retain((dval_t *)dv);
    return (dval_t *)dv;
}

dval_t *dv_simplify_unary_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    (void)b;
    if (dv_is_exp_expr(dv)) {
        dval_t *quarter_turn = dv_try_simplify_exp_quarter_turn(a);

        if (quarter_turn) {
            dv_free(a);
            return quarter_turn;
        }
    }

    /* exp(log(x)) -> x, log(exp(x)) -> x */
    if ((dv_is_exp_expr(dv) && dv_is_op(a, &ops_log)) ||
        (dv_is_op(dv, &ops_log) && dv_is_exp_expr(a))) {
        dval_t *inner = a->a;
        dv_retain(inner);
        dv_free(a);
        return inner;
    }

    /* log10(10^x) -> x */
    if (dv_is_op(dv, &ops_log10) &&
        dv_is_op(a, &ops_pow) &&
        dv_is_op(a->a, &ops_const) &&
        num_eq(a->a->c, NUM_TEN)) {
        dval_t *inner = a->b;
        dv_retain(inner);
        dv_free(a);
        return inner;
    }

    if (dv_is_op(a, &ops_const)) {
        number_t folded = num_new();

        if (dv->ops->fold_const_unary && dv->ops->fold_const_unary(&a->c, &folded)) {
            dval_t *out = dv_new_const_owned_local(folded);

            dv_free(a);
            return out;
        }

        if ((!a->name || !*a->name) && dv->ops->apply_unary) {
            dval_t *tmp = dv->ops->apply_unary(a);
            number_t value = tmp->ops->eval(tmp);
            dval_t *out = dv_new_const_owned_local(value);

            dv_free(tmp);
            dv_free(a);
            return out;
        }
    }

    if (dv_is_op(dv, &ops_sqrt) &&
        dv_is_op(a, &ops_mul) &&
        dv_is_unnamed_const(a->a)) {
        if (num_is_real(a->a->c) && num_gt(a->a->c, NUM_ZERO)) {
            number_t coeff_root = num_sqrt(a->a->c);
            number_t coeff_square = num_mul(coeff_root, coeff_root);

            if (num_eq(coeff_square, a->a->c)) {
                dval_t *raw;
                dval_t *simp;

                dv_retain(a->b);
                raw = dv_sqrt(a->b);
                dv_free(a->b);
                simp = dv_simplify(raw);
                dv_free(raw);
                dv_free(a);
                dval_t *out = dv_make_scaled_owned_local(coeff_root, simp);

                return out;
            }
        }
    }

    if (dv->ops->apply_unary) {
        dval_t *out = dv->ops->apply_unary(a);
        dv_free(a);
        return out;
    }

    dv_free(a);
    dv_retain((dval_t *)dv);
    return (dval_t *)dv;
}

dval_t *dv_simplify_binary_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    if ((!a || !b) || !dv->ops->apply_binary)
    {
        return dv_simplify_passthrough(dv, a, b);
    }

    if (dv_is_unnamed_const(a) && !a->binding_expr &&
        dv_is_unnamed_const(b) && !b->binding_expr) {
        dval_t *tmp = dv->ops->apply_binary(a, b);
        number_t value = tmp->ops->eval(tmp);
        dval_t *out = dv_new_const_owned_local(value);

        dv_free(tmp);
        dv_free(a);
        dv_free(b);
        return out;
    }

    dval_t *out = dv->ops->apply_binary(a, b);
    dv_free(a);
    dv_free(b);
    return out;
}

/* ========================================================================= */
/* Per-operation simplifiers                                                 */
/* ========================================================================= */

dval_t *dv_simplify_neg_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    (void)dv;
    (void)b;
    /* --x → x */
    if (dv_is_op(a, &ops_neg)) {
        dval_t *inner = a->a; dv_retain(inner); dv_free(a); return inner;
    }
    /* neg(c) → -c */
    if (dv_is_op(a, &ops_const)) {
        number_t c = num_neg(a->c);
        dval_t *out = dv_new_const_owned_local(c);

        dv_free(a);
        return out;
    }
    /* neg(c·x) where c < 0 → |c|·x  (eliminates spurious double-negative) */
    if (dv_is_op(a, &ops_mul) &&
        dv_is_op(a->a, &ops_const) &&
        (!a->a->name || !*a->a->name)) {
        if (num_is_real(a->a->c) && num_lt(a->a->c, NUM_ZERO)) {
            number_t pos_c = num_neg(a->a->c);

            dv_retain(a->b);
            dval_t *rest = a->b;
            dv_free(a);
            dval_t *out = dv_make_scaled_owned_local(pos_c, rest);

            return out;
        }
    }
    dval_t *r = dv_neg(a); dv_free(a); return r;
}

/* --- */

dval_t *dv_simplify_add_sub_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    number_t c_const = num_const(NUM_ZERO);
    number_t common_coeff = num_const(NUM_ONE);
    addend_t *terms   = NULL;
    size_t    n = 0, cap = 0;

    dv_collect_addends(a, NUM_ONE, &c_const, &terms, &n, &cap);
    dv_free(a);
    dv_collect_addends(b, dv_is_op(dv, &ops_sub) ? NUM_NEG_ONE : NUM_ONE, &c_const, &terms, &n, &cap);
    dv_free(b);

    dv_combine_common_denominator_addends(terms, n);
    dv_sort_addends(terms, n);
    dv_extract_common_addend_coeff(terms, n, c_const, &common_coeff);
    if (num_is_one(common_coeff))
        dv_try_common_abs_coeff_local(terms, n, c_const, &common_coeff);

    dval_t *identity = dv_try_trig_pythagorean_identity(terms, n, c_const, common_coeff);

    if (identity) {
        for (size_t i = 0; i < n; ++i) {
            dv_free(terms[i].base);
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

    dval_t *cur = NULL;

    /* emit a positive constant before any leading negative symbolic term so
     * the expression reads "1 - tanh²(x)" rather than "-tanh²(x) + 1" */
    int const_emitted = 0;
    if (!num_is_zero(scaled_const) && num_gt(scaled_const, NUM_ZERO) && leading_neg) {
        cur = dv_new_const(scaled_const);
        const_emitted = 1;
    }

    for (size_t i = 0; i < n; ++i) {
        if (num_is_zero(terms[i].coeff)) {
            dv_free(terms[i].base);
            num_destroy(&terms[i].coeff);
            continue;
        }
        number_t scaled_coeff = num_div(terms[i].coeff, common_coeff);
        dval_t *term = dv_make_scaled(scaled_coeff, terms[i].base);

        num_destroy(&terms[i].coeff);
        if (!cur) cur = term;
        else {
            dval_t *tmp = dv_add(cur, term);
            dv_free(cur); dv_free(term); cur = tmp;
        }
    }
    free(terms);

    if (!const_emitted && !num_is_zero(scaled_const)) {
        dval_t *cterm = dv_new_const(scaled_const);
        if (!cur) cur = cterm;
        else {
            dval_t *tmp = dv_add(cur, cterm);
            dv_free(cur); dv_free(cterm); cur = tmp;
        }
    }

    if (!cur)
        cur = dv_new_const(NUM_ZERO);

    if (!const_emitted && num_is_zero(scaled_const)) {
        dval_t *factored = dv_try_factor_common_symbolic_product_local(cur);

        if (factored) {
            dv_free(cur);
            cur = factored;
        }
    }

    if (!num_is_one(common_coeff)) {
        dval_t *scaled = dv_make_scaled(common_coeff, cur);

        cur = scaled;
    }

    num_destroy(&scaled_const);
    num_destroy(&c_const);
    num_destroy(&common_coeff);

    return cur;
}

/* --- */

dval_t *dv_simplify_mul_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    dval_t **terms = NULL;
    dval_t **den_terms = NULL;
    size_t nterms = 0, term_cap = 0;
    size_t nden_terms = 0, den_cap = 0;
    number_t c_acc = num_const(NUM_ONE);
    int is_zero = 0;
    dval_t *expanded;
    dval_t *numerator;
    dval_t *denominator;
    dval_t *division;

    (void)dv;

    if ((dv_is_simplifiable_const_local(a) && dv_const_is_zero(a)) ||
        (dv_is_simplifiable_const_local(b) && dv_const_is_zero(b))) {
        dv_free(a);
        dv_free(b);
        return dv_new_const(NUM_ZERO);
    }
    if (dv_is_simplifiable_const_local(a) && dv_const_is_one(a)) {
        dv_free(a);
        return b;
    }
    if (dv_is_simplifiable_const_local(b) && dv_const_is_one(b)) {
        dv_free(b);
        return a;
    }

    {
        dval_t *lambert_identity = dv_try_simplify_lambert_product(a, b);

        if (lambert_identity) {
            dv_free(a);
            dv_free(b);
            return lambert_identity;
        }
    }

    if (dv_is_div(a) && dv_struct_eq(a->b, b)) {
        dval_t *out;

        dv_retain(a->a);
        out = a->a;
        dv_free(a);
        dv_free(b);
        return out;
    }

    if (dv_is_div(b) && dv_struct_eq(b->b, a)) {
        dval_t *out;

        dv_retain(b->a);
        out = b->a;
        dv_free(a);
        dv_free(b);
        return out;
    }

    collect_mul_flat(a, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    collect_mul_flat(b, &c_acc, &is_zero, &terms, &nterms, &term_cap);
    dv_free(a);
    dv_free(b);

    if (is_zero) {
        dv_free_node_array(terms, nterms);
        num_destroy(&c_acc);
        return dv_new_const(NUM_ZERO);
    }

    dv_split_division_terms(&c_acc, &is_zero, terms, nterms,
                         &den_terms, &nden_terms, &den_cap);

    if (is_zero) {
        dv_free_node_array(terms, nterms);
        dv_free_node_array(den_terms, nden_terms);
        num_destroy(&c_acc);
        return dv_new_const(NUM_ZERO);
    }

    dv_combine_like_powers(den_terms, nden_terms);
    dv_combine_like_powers(terms, nterms);
    dv_cancel_common_powers(terms, nterms, den_terms, nden_terms);
    dv_combine_exp_terms(terms, nterms);
    dv_merge_sqrt_terms(terms, nterms);
    dv_merge_sqrt_terms(den_terms, nden_terms);

    expanded = dv_try_expand_shallow_product(c_acc, terms, nterms,
                                             den_terms, nden_terms);
    if (expanded) {
        num_destroy(&c_acc);
        return expanded;
    }

    numerator = dv_rebuild_product_chain(c_acc, terms, nterms);
    num_destroy(&c_acc);
    if (nden_terms == 0) {
        free(den_terms);
        return numerator;
    }

    denominator = dv_rebuild_division_chain(den_terms, nden_terms);
    division = dv_div(numerator, denominator);
    dv_free(numerator);
    dv_free(denominator);
    numerator = dv_simplify(division);
    dv_free(division);
    return numerator;
}

/* --- */

dval_t *dv_simplify_div_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    (void)dv;

    if (dv_is_op(b, &ops_const) && dv_const_is_one(b)) { dv_free(b); return a; }
    if (dv_is_pow_d_expr(b) && dv_struct_eq(a, b->a)) {
        number_t exponent = num_sub(b->c, NUM_ONE);
        dval_t *base;
        dval_t *denom;
        dval_t *one;
        dval_t *out;

        dv_retain(b->a);
        base = b->a;
        dv_free(a);
        dv_free(b);

        if (num_eq(exponent, NUM_ZERO)) {
            dv_free(base);
            return dv_new_const(NUM_ONE);
        }

        denom = dv_make_pow_like_owned_local(base, exponent);
        one = dv_new_const(NUM_ONE);
        out = dv_div(one, denom);
        dv_free(one);
        dv_free(denom);
        return out;
    }
    if (dv_is_pow_d_expr(a) && dv_struct_eq(a->a, b)) {
        number_t exponent = num_sub(a->c, NUM_ONE);
        dval_t *base;

        dv_retain(a->a);
        base = a->a;
        dv_free(a);
        dv_free(b);
        return dv_make_pow_like_owned_local(base, exponent);
    }
    if (dv_is_unnamed_const(b) &&
        dv_is_op(a, &ops_mul) &&
        dv_is_unnamed_const(a->a) &&
        num_is_real(a->a->c) &&
        num_is_real(b->c)) {
        dval_t *rest;
        number_t folded = num_div(a->a->c, b->c);

        dv_retain(a->b);
        rest = a->b;
        dv_free(a);
        dv_free(b);
        dval_t *out = dv_make_scaled_owned_local(folded, rest);

        return out;
    }
    if (dv_is_unnamed_const(b) && dv_is_op(a, &ops_neg)) {
        dval_t *inner;
        dval_t *quot;
        dval_t *simp;

        dv_retain(a->a);
        dv_retain(b);
        inner = a->a;
        quot = dv_div(inner, b);
        dv_free(inner);
        dv_free(b);
        simp = dv_simplify(quot);
        dv_free(quot);
        dv_free(a);
        return dv_simplify_neg_operator(dv, simp, NULL);
    }
    if (dv_is_unnamed_const(b) && dv_is_addsub(a)) {
        number_t c_const = num_const(NUM_ZERO);
        number_t denom = NUM_ZERO;
        addend_t *terms = NULL;
        size_t n = 0, cap = 0;
        dval_t *cur = NULL;

        if (!num_is_real(b->c)) {
            num_destroy(&c_const);
            goto div_fallback_2;
        }
        denom = num_clone(b->c);

        dv_collect_addends(a, NUM_ONE, &c_const, &terms, &n, &cap);
        dv_free(a);
        dv_free(b);

        for (size_t i = 0; i < n; ++i) {
            dval_t *term;

            if (!terms[i].base || num_is_zero(terms[i].coeff)) {
                if (terms[i].base)
                    dv_free(terms[i].base);
                num_destroy(&terms[i].coeff);
                continue;
            }

            number_t scaled_coeff = num_div(terms[i].coeff, denom);

            term = dv_make_scaled_owned_local(scaled_coeff, terms[i].base);
            num_destroy(&terms[i].coeff);
            if (!cur) cur = term;
            else {
                dval_t *tmp = dv_add(cur, term);
                dv_free(cur);
                dv_free(term);
                cur = tmp;
            }
        }
        free(terms);

        if (!num_is_zero(c_const)) {
            number_t scaled_const = num_div(c_const, denom);
            dval_t *cterm = dv_new_const_owned_local(scaled_const);

            if (!cur)
                cur = cterm;
            else {
                dval_t *tmp = dv_add(cur, cterm);
                dv_free(cur);
                dv_free(cterm);
                cur = tmp;
            }
        }

        {
            dval_t *out = cur ? cur : dv_new_const(NUM_ZERO);

            num_destroy(&c_const);
            num_destroy(&denom);
            return out;
        }
    }
	div_fallback_2:
    if (!dv_is_addsub(a) &&
        (dv_is_op(a, &ops_mul) || dv_is_div(a) ||
         dv_is_op(b, &ops_mul) || dv_is_div(b)) &&
        (dv_contains_addsub_local(a) || dv_contains_addsub_local(b)))
        return dv_simplify_flat_quotient_local(a, b);

    if (dv_is_op(a, &ops_mul) && dv_struct_eq(a->a, b)) {
        dval_t *rest;

        dv_retain(a->b);
        rest = a->b;
        dv_free(a);
        dv_free(b);
        return rest;
    }
    if (dv_is_op(a, &ops_mul) && dv_struct_eq(a->b, b)) {
        dval_t *rest;

        dv_retain(a->a);
        rest = a->a;
        dv_free(a);
        dv_free(b);
        return rest;
    }
    if (dv_struct_eq(a, b)) {
        dv_free(a);
        dv_free(b);
        return dv_new_const(NUM_ONE);
    }
    if (dv_is_op(a, &ops_const) && dv_const_is_zero(a)) {
        dv_free(a); dv_free(b); return dv_new_const(NUM_ZERO);
    }
    if (dv_is_unnamed_const(a) && !a->binding_expr &&
        dv_is_unnamed_const(b) && !b->binding_expr) {
        number_t q = num_div(a->c, b->c);

        dv_free(a);
        dv_free(b);
        dval_t *out = dv_new_const_owned_local(q);

        return out;
    }

    /* sinh(x)/cosh(x) → tanh(x) */
    if (dv_is_op(a, &ops_sinh) && dv_is_op(b, &ops_cosh) &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tanh(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* sin(x)/cos(x) → tan(x) */
    if (dv_is_op(a, &ops_sin) && dv_is_op(b, &ops_cos) &&
        dv_struct_eq(a->a, b->a)) {
        dv_retain(a->a);
        dval_t *r = dv_tan(a->a); dv_free(a->a); dv_free(a); dv_free(b);
        return r;
    }
    /* x/abs(x) → abs(x)/x  (canonical sign-function form) */
    if (dv_is_op(b, &ops_abs) && dv_struct_eq(a, b->a)) {
        dval_t *r = dv_div(b, a); dv_free(a); dv_free(b); return r;
    }

    dval_t *r = dv_div(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

dval_t *dv_simplify_pow_d_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    NUM_SCOPE(scope);
    (void)b;
    number_t exponent = dv->c;

    if (num_eq(exponent, NUM_ONE)) {
        return a;
    }
    if (num_eq(exponent, NUM_ZERO)) {
        dv_free(a);
        return dv_new_const(NUM_ONE);
    }

    if (dv_is_pow_d_expr(a)) {
        number_t folded_exponent = num_mul(a->c, exponent);
        dval_t *base;

        dv_retain(a->a);
        base = a->a;
        dv_free(a);
        return dv_make_pow_like_owned_local(base, folded_exponent);
    }

    if (dv_is_unnamed_const(a)) {
        number_t v = num_pow(a->c, exponent);

        dv_free(a);
        dval_t *out = dv_new_const_owned_local(v);

        return out;
    }

    /* sqrt(x)^n → x^(n/2) */
    if (dv_is_sqrt_expr(a)) {
        number_t half = num_div(exponent, NUM_TWO);
        dv_retain(a->a);
        dval_t *inner = a->a;
        dv_free(a);
        dval_t *out = dv_make_pow_like_owned_local(inner, half);

        return out;
    }

    /* (ab)^2 -> a^2 b^2. Keep this deliberately narrow: it exposes factors
     * like (2√x)^2 to the product simplifier without expanding sums. */
    if (num_eq(exponent, NUM_TWO) && dv_is_op(a, &ops_mul)) {
        dval_t *left_pow = dv_pow(a->a, &exponent);
        dval_t *right_pow = dv_pow(a->b, &exponent);
        dval_t *raw = dv_mul(left_pow, right_pow);
        dval_t *out;

        dv_free(left_pow);
        dv_free(right_pow);
        dv_free(a);
        out = dv_simplify(raw);
        dv_free(raw);
        return out;
    }

    return dv_make_pow_like_owned_local(a, exponent);
}

/* --- */

dval_t *dv_simplify_pow_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;
    if (dv_is_op(b, &ops_const) && dv_const_is_one(b)) { dv_free(b); return a; }
    if (dv_is_op(b, &ops_const) && dv_const_is_zero(b)) {
        dv_free(a); dv_free(b); return dv_new_const(NUM_ONE);
    }
    if (dv_is_op(a, &ops_const) && num_eq(a->c, NUM_E)) {
        dval_t *raw;
        dval_t *out;

        raw = dv_exp(b);
        out = dv_simplify(raw);
        dv_free(raw);
        dv_free(a);
        dv_free(b);
        return out;
    }
    if (dv_is_op(a, &ops_const) && num_eq(a->c, NUM_TEN) && dv_is_op(b, &ops_log10)) {
        dval_t *inner = b->a;
        dv_retain(inner);
        dv_free(a);
        dv_free(b);
        return inner;
    }
    dval_t *r = dv_pow_dv(a, b); dv_free(a); dv_free(b); return r;
}

/* --- */

dval_t *dv_simplify_hypot_operator(const dval_t *dv, dval_t *a, dval_t *b)
{
    (void)dv;

    if (dv_is_op(a, &ops_const) && dv_const_is_zero(a)) {
        dv_free(a);
        dval_t *r = dv_abs(b);
        dv_free(b);
        return r;
    }
    if (dv_is_op(b, &ops_const) && dv_const_is_zero(b)) {
        dv_free(b);
        dval_t *r = dv_abs(a);
        dv_free(a);
        return r;
    }

    return dv_simplify_binary_operator(dv, a, b);
}

/* ========================================================================= */
/* Main dispatcher                                                            */
/* ========================================================================= */

dval_t *dv_simplify(const dval_t *dv)
{
    if (!dv) return NULL;

    if (dv->ops->arity == DV_OP_ATOM) { dv_retain(dv); return (dval_t *)dv; }

    dval_t *a = dv->a ? dv_simplify(dv->a) : NULL;
    dval_t *b = dv->b ? dv_simplify(dv->b) : NULL;

    if (dv->ops->simplify)
        return dv->ops->simplify(dv, a, b);

    return dv_simplify_passthrough(dv, a, b);
}
