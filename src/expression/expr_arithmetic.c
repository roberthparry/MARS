#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "integrator.h"
#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

/* ------------------------------------------------------------------------- */
/* EVALUATION FUNCTIONS                                                      */
/* ------------------------------------------------------------------------- */

static number_t eval_const(expr_t *dv)
{
    return num_clone(dv->c);
}

static number_t eval_var(expr_t *dv)
{
    if (dv && dv->binding_expr)
        return expr_binding_expr_eval(dv->binding_expr);
    return num_clone(dv->c);
}

static number_t eval_formal_derivative(expr_t *dv)
{
    (void)dv;
    return num_clone(NUM_NAN);
}

static number_t eval_arbitrary_function(expr_t *dv)
{
    (void)dv;
    return num_clone(NUM_NAN);
}

static number_t eval_argument_list(expr_t *dv)
{
    (void)dv;
    return num_clone(NUM_NAN);
}

static number_t eval_add(expr_t *dv)
{
    return num_add(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

static number_t eval_sub(expr_t *dv)
{
    return num_sub(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

static expr_t *lambert_product_inner(expr_t *a, expr_t *b)
{
    expr_t *w;
    expr_t *exp_term;

    if (a && expr_ops_is_lambert(a->ops) && expr_is_exp_expr(b)) {
        w = a;
        exp_term = b;
    } else if (b && expr_ops_is_lambert(b->ops) && expr_is_exp_expr(a)) {
        w = b;
        exp_term = a;
    } else {
        return NULL;
    }

    return expr_struct_eq(w, exp_term->a) ? (expr_t *)expr_lambert_arg(w) : NULL;
}

static number_t eval_mul(expr_t *dv)
{
    expr_t *inner = lambert_product_inner(dv->a, dv->b);

    if (inner)
        return num_clone(expr_eval_num_internal(inner));

    return num_mul(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

static bool expr_same_var_local(const expr_t *left, const expr_t *right)
{
    if (!left || !right || !expr_is_var(left) || !expr_is_var(right))
        return false;
    return left == right ||
           (left->var_id != 0u &&
            right->var_id != 0u &&
            left->var_id == right->var_id);
}

static bool expr_find_single_var_local(const expr_t *expr, const expr_t **var_io)
{
    if (!expr)
        return true;
    if (expr_is_var(expr)) {
        if (!*var_io) {
            *var_io = expr;
            return true;
        }
        return expr_same_var_local(*var_io, expr);
    }
    return expr_find_single_var_local(expr->a, var_io) &&
           expr_find_single_var_local(expr->b, var_io);
}

static number_t eval_div_removable_singularity(expr_t *dv)
{
    const expr_t *wrt = NULL;
    const expr_t *numer = dv->a;
    const expr_t *denom = dv->b;

    if (!expr_find_single_var_local(dv, &wrt) || !wrt)
        return num_div(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));

    for (size_t depth = 0u; depth < 8u; ++depth) {
        number_t numer_value = expr_eval_num_internal(numer);
        number_t denom_value = expr_eval_num_internal(denom);

        if (!num_is_zero(numer_value) || !num_is_zero(denom_value))
            return num_div(numer_value, denom_value);

        const expr_t *dnumer = expr_get_deriv(numer, wrt);
        const expr_t *ddenom = expr_get_deriv(denom, wrt);

        if (!dnumer || !ddenom)
            break;
        numer = dnumer;
        denom = ddenom;
    }

    return num_div(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

static number_t eval_div(expr_t *dv)
{
    number_t numerator = expr_eval_num_internal(dv->a);
    number_t denominator = expr_eval_num_internal(dv->b);

    if (num_is_zero(numerator) && num_is_zero(denominator))
        return eval_div_removable_singularity(dv);
    return num_div(numerator, denominator);
}

static number_t eval_neg(expr_t *dv)
{
    return num_neg(expr_eval_num_internal(dv->a));
}

static number_t eval_pow(expr_t *dv)
{
    return num_pow(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

static number_t eval_pow_d(expr_t *dv)
{
    return num_pow(expr_eval_num_internal(dv->a), dv->c);
}

static number_t eval_integral_bounds(expr_t *dv)
{
    (void)dv;
    return num_clone(NUM_NAN);
}

static number_t eval_integral_meta(expr_t *dv)
{
    (void)dv;
    return num_clone(NUM_NAN);
}

static bool eval_integral_antiderivative_difference(const expr_t *antiderivative,
                                                    const expr_t *local_var,
                                                    const expr_t *upper_expr,
                                                    const expr_t *lower_expr,
                                                    number_t *out)
{
    expr_t *zero_lower = NULL;
    expr_t *upper_eval_expr = NULL;
    expr_t *lower_eval_expr = NULL;
    expr_t *diff_expr = NULL;
    expr_t *simplified_diff = NULL;
    const expr_t *effective_lower = lower_expr;
    const expr_t *value_expr;
    number_t value;
    bool ok;

    if (!antiderivative || !local_var || !upper_expr || !out)
        return false;

    if (!effective_lower) {
        zero_lower = expr_new_const(NUM_ZERO);
        effective_lower = zero_lower;
    }

    upper_eval_expr = expr_substitute(antiderivative, local_var, upper_expr);
    lower_eval_expr = effective_lower
        ? expr_substitute(antiderivative, local_var, effective_lower)
        : NULL;
    diff_expr = (upper_eval_expr && lower_eval_expr)
        ? expr_sub(upper_eval_expr, lower_eval_expr)
        : NULL;
    simplified_diff = diff_expr ? expr_simplify(diff_expr) : NULL;
    value_expr = simplified_diff ? simplified_diff : diff_expr;
    value = value_expr ? expr_eval(value_expr) : num_clone(NUM_NAN);
    ok = num_is_real(value) && num_is_finite(value);
    if (ok)
        *out = value;
    else
        num_destroy(&value);

    expr_free(simplified_diff);
    expr_free(diff_expr);
    expr_free(lower_eval_expr);
    expr_free(upper_eval_expr);
    expr_free(zero_lower);
    return ok;
}

static size_t eval_integral_interval_budget(void)
{
    size_t digits = num_get_default_prec_digits();
    size_t refinements = 0u;
    size_t scale = 1u;
    size_t steps_per_refinement;

    if (digits < 2u)
        digits = 2u;
    while (scale < digits && scale <= SIZE_MAX / 2u) {
        scale *= 2u;
        refinements++;
    }
    refinements += 2u;
    /*
     * At refinement h = 1/scale, tanh-sinh needs a transformed range of
     * roughly eight units to make its endpoint tails negligible at the
     * requested precision.  Calculate that complete final-level width up
     * front instead of discovering an inadequate cap by retrying.
     */
    if (scale > (SIZE_MAX - 1u) / 8u)
        return SIZE_MAX;
    steps_per_refinement = scale * 8u + 1u;
    if (refinements > SIZE_MAX / steps_per_refinement)
        return SIZE_MAX;
    return refinements * steps_per_refinement;
}

static number_t eval_integral(expr_t *dv)
{
    integrator_t *ig;
    const expr_t *lower_expr;
    const expr_t *upper_expr;
    const expr_t *dummy_expr;
    number_t lower;
    number_t upper;
    number_t result;
    expr_t *local_var;
    expr_t *local_integrand;
    expr_t *antiderivative;
    int status;

    if (!dv || !dv->a || !dv->b)
        return num_clone(NUM_NAN);

    lower_expr = expr_integral_lower_bound_expr(dv);
    upper_expr = expr_integral_upper_bound_expr(dv);
    dummy_expr = expr_integral_dummy_expr(dv);
    if (!upper_expr || !dummy_expr)
        return num_clone(NUM_NAN);

    upper = expr_eval(upper_expr);
    if (!num_is_real(upper) || num_is_nan(upper)) {
        num_destroy(&upper);
        return num_clone(NUM_NAN);
    }
    lower = lower_expr ? expr_eval(lower_expr) : num_clone(NUM_ZERO);
    if (!num_is_real(lower) || num_is_nan(lower)) {
        num_destroy(&lower);
        num_destroy(&upper);
        return num_clone(NUM_NAN);
    }

    ig = intg_new();
    if (!ig) {
        num_destroy(&lower);
        num_destroy(&upper);
        return num_clone(NUM_NAN);
    }

    local_var = expr_clone(dummy_expr);
    if (!local_var) {
        intg_free(ig);
        num_destroy(&lower);
        num_destroy(&upper);
        return num_clone(NUM_NAN);
    }
    local_integrand = expr_substitute(dv->a, dummy_expr, local_var);
    if (!local_integrand) {
        expr_free(local_var);
        intg_free(ig);
        num_destroy(&lower);
        num_destroy(&upper);
        return num_clone(NUM_NAN);
    }

    result = num_new();
    antiderivative = expr_integrate(local_integrand, local_var);
    if (antiderivative) {
        number_t symbolic_value;

        if (eval_integral_antiderivative_difference(antiderivative,
                                                    local_var,
                                                    upper_expr,
                                                    lower_expr,
                                                    &symbolic_value)) {
            num_destroy(&result);
            result = symbolic_value;
            expr_free(antiderivative);
            status = 0;
        } else {
            expr_t *upper_const = expr_new_const(upper);
            expr_t *lower_const = expr_new_const(lower);
            expr_t *upper_eval_expr = upper_const
                ? expr_substitute(antiderivative, local_var, upper_const)
                : NULL;
            expr_t *lower_eval_expr = lower_const
                ? expr_substitute(antiderivative, local_var, lower_const)
                : NULL;

            if (!upper_const || !lower_const || !upper_eval_expr || !lower_eval_expr) {
                expr_free(lower_eval_expr);
                expr_free(upper_eval_expr);
                expr_free(lower_const);
                expr_free(upper_const);
                expr_free(antiderivative);
                status = -1;
            } else {
                number_t upper_value = expr_eval(upper_eval_expr);
                number_t lower_value = expr_eval(lower_eval_expr);

                num_destroy(&result);
                result = num_sub(upper_value, lower_value);
                num_destroy(&lower_value);
                num_destroy(&upper_value);
                expr_free(lower_eval_expr);
                expr_free(upper_eval_expr);
                expr_free(lower_const);
                expr_free(upper_const);
                expr_free(antiderivative);
                status = 0;
            }
        }
    } else {
        size_t interval_budget = eval_integral_interval_budget();

        intg_set_interval_count_max(ig, interval_budget);
        status = intg_integral(ig, local_integrand, local_var, lower, upper,
                               &result, NULL);
    }

    expr_free(local_integrand);
    expr_free(local_var);
    intg_free(ig);
    num_destroy(&lower);
    num_destroy(&upper);

    if (status != 0 || !num_is_real(result) || !num_is_finite(result)) {
        num_destroy(&result);
        return num_clone(NUM_NAN);
    }

    return result;
}

/* ------------------------------------------------------------------------- */
/* DERIVATIVE FUNCTIONS — lazy, stored in each node                          */
/* ------------------------------------------------------------------------- */

static expr_t *deriv_const(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_ZERO);
}

static expr_t *deriv_var(expr_t *dv)
{
    const expr_t *wrt = expr_current_wrt_internal();
    bool same_var = wrt == NULL || dv == wrt ||
                    (wrt && expr_is_var(wrt) &&
                     dv->var_id != 0 && dv->var_id == wrt->var_id);

    return expr_new_const(same_var ? NUM_ONE : NUM_ZERO);
}

static expr_t *deriv_formal_derivative(expr_t *dv)
{
    const expr_t *wrt = expr_current_wrt_internal();
    expr_t **wrts;
    expr_t *out;

    if (!wrt)
        return NULL;

    wrts = calloc(dv->formal_wrt_count + 1u, sizeof(*wrts));
    if (!wrts)
        return NULL;
    for (size_t i = 0u; i < dv->formal_wrt_count; ++i)
        wrts[i] = dv->formal_wrts[i];
    wrts[dv->formal_wrt_count] = (expr_t *)wrt;

    out = expr_new_formal_derivative(
        dv->a, dv->formal_wrt_count + 1u, wrts);
    free(wrts);
    return out;
}

static expr_t *deriv_arbitrary_function(expr_t *dv)
{
    expr_t *outer;
    expr_t *inner;
    expr_t *out;
    size_t name_length;
    char *derivative_name;

    if (!dv || !dv->name || !dv->a)
        return NULL;
    name_length = strlen(dv->name);
    derivative_name = malloc(name_length + 2u);
    if (!derivative_name)
        return NULL;
    memcpy(derivative_name, dv->name, name_length);
    derivative_name[name_length] = '\'';
    derivative_name[name_length + 1u] = '\0';
    outer = expr_new_arbitrary_function(derivative_name, dv->a);
    free(derivative_name);
    inner = expr_get_dx_internal(dv->a);
    out = outer && inner ? expr_mul(outer, inner) : NULL;
    expr_free(inner);
    expr_free(outer);
    return out;
}

expr_t *expr_new_arbitrary_function(const char *name, const expr_t *argument)
{
    expr_t *expr;

    if (!name || !*name || !argument)
        return NULL;
    expr = expr_alloc(&ops_arbitrary_function);
    expr->name = strdup(name);
    if (!expr->name) {
        expr_free(expr);
        return NULL;
    }
    expr->a = (expr_t *)argument;
    expr_retain(expr->a);
    return expr;
}

static expr_t *expr_new_argument_list(
    const expr_t *left,
    const expr_t *right)
{
    expr_t *expr;

    if (!left || !right)
        return NULL;
    expr = expr_alloc(&ops_argument_list);
    expr->a = (expr_t *)left;
    expr->b = (expr_t *)right;
    expr_retain(expr->a);
    expr_retain(expr->b);
    return expr;
}

expr_t *expr_new_arbitrary_function_n(
    const char *name,
    size_t argument_count,
    expr_t *const *arguments)
{
    expr_t *list;

    if (!name || !*name || argument_count == 0u || !arguments)
        return NULL;
    list = expr_clone(arguments[0]);
    for (size_t i = 1u; list && i < argument_count; ++i) {
        expr_t *next =
            expr_new_argument_list(list, arguments[i]);

        expr_free(list);
        list = next;
    }
    if (!list)
        return NULL;

    expr_t *function = expr_new_arbitrary_function(name, list);

    expr_free(list);
    return function;
}

bool expr_is_arbitrary_function(const expr_t *expr)
{
    return expr && expr->ops == &ops_arbitrary_function;
}

expr_t *expr_new_formal_derivative(const expr_t *dependent,
                                   size_t wrt_count,
                                   expr_t *const *wrts)
{
    expr_t *expr;

    if (!dependent || wrt_count == 0u || !wrts)
        return NULL;
    if (wrt_count > SIZE_MAX / sizeof(*expr->formal_wrts))
        return NULL;

    expr = expr_alloc(&ops_formal_derivative);
    expr->formal_wrts = calloc(wrt_count, sizeof(*expr->formal_wrts));
    if (!expr->formal_wrts) {
        expr_free(expr);
        return NULL;
    }
    expr->formal_wrt_count = wrt_count;

    expr->a = (expr_t *)dependent;
    expr_retain(expr->a);
    for (size_t i = 0u; i < wrt_count; ++i) {
        if (!wrts[i]) {
            expr_free(expr);
            return NULL;
        }
        expr->formal_wrts[i] = wrts[i];
        expr_retain(expr->formal_wrts[i]);
    }
    return expr;
}

bool expr_is_formal_derivative(const expr_t *expr)
{
    return expr && expr->ops == &ops_formal_derivative;
}

const expr_t *expr_formal_derivative_dependent(const expr_t *expr)
{
    return expr_is_formal_derivative(expr) ? expr->a : NULL;
}

size_t expr_formal_derivative_order(const expr_t *expr)
{
    return expr_is_formal_derivative(expr) ? expr->formal_wrt_count : 0u;
}

const expr_t *expr_formal_derivative_wrt_at(const expr_t *expr, size_t index)
{
    if (!expr_is_formal_derivative(expr) || index >= expr->formal_wrt_count)
        return NULL;
    return expr->formal_wrts[index];
}

static expr_t *deriv_add(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *db  = expr_get_dx_internal(dv->b);
    expr_t *out = expr_add(da, db);
    expr_free(da);
    expr_free(db);
    return out;
}

static expr_t *deriv_sub(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *db  = expr_get_dx_internal(dv->b);
    expr_t *out = expr_sub(da, db);
    expr_free(da);
    expr_free(db);
    return out;
}

static expr_t *deriv_exp_inverse_scaled_sqrt_product(expr_t *dv);
static expr_t *deriv_power_inverse_scaled_sqrt_product(expr_t *dv);
static expr_t *deriv_sqrt_affine_over_power(expr_t *dv);

static expr_t *deriv_mul(expr_t *dv)
{
    expr_t *special = deriv_exp_inverse_scaled_sqrt_product(dv);

    if (special)
        return special;
    special = deriv_power_inverse_scaled_sqrt_product(dv);
    if (special)
        return special;

    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *db  = expr_get_dx_internal(dv->b);
    expr_t *t1  = expr_mul(da, dv->b);
    expr_t *t2  = expr_mul(dv->a, db);
    expr_t *out = expr_add(t1, t2);
    expr_free(da);
    expr_free(db);
    expr_free(t1);
    expr_free(t2);
    return out;
}

static void deriv_zero_number_array_local(number_t *values, size_t n)
{
    for (size_t i = 0u; i < n; ++i)
        values[i] = num_new();
}

static void deriv_reset_number_array_local(number_t *values, size_t n)
{
    for (size_t i = 0u; i < n; ++i) {
        num_destroy(&values[i]);
        values[i] = num_new();
    }
}

static void deriv_clear_number_array_local(number_t *values, size_t n)
{
    for (size_t i = 0u; i < n; ++i)
        num_destroy(&values[i]);
}

static number_t *deriv_alloc_number_array_local(size_t n)
{
    number_t *values;

    if (n == 0u || n > SIZE_MAX / sizeof(*values))
        return NULL;
    values = calloc(n, sizeof(*values));
    if (values)
        deriv_zero_number_array_local(values, n);
    return values;
}

static void deriv_free_number_array_local(number_t *values, size_t n)
{
    if (!values)
        return;
    deriv_clear_number_array_local(values, n);
    free(values);
}

static void deriv_copy_number_array_local(number_t *dst,
                                          const number_t *src,
                                          size_t n)
{
    for (size_t i = 0u; i < n; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static bool deriv_poly_mul_local(const number_t *left,
                                 const number_t *right,
                                 number_t *out,
                                 size_t n)
{
    size_t product_count = 2u * n - 1u;
    number_t *product = NULL;
    bool fits = true;

    if (n == 0u || n > SIZE_MAX / 2u + 1u)
        return false;

    product = deriv_alloc_number_array_local(product_count);
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
        deriv_copy_number_array_local(out, product, n);
    deriv_free_number_array_local(product, product_count);
    return fits;
}

static bool deriv_poly_scale_local(const number_t *src,
                                   number_t scale,
                                   number_t *out,
                                   size_t n)
{
    if (!num_is_real(scale))
        return false;

    for (size_t i = 0u; i < n; ++i) {
        number_t value = num_mul(src[i], scale);

        num_destroy(&out[i]);
        out[i] = value;
    }
    return true;
}

static long deriv_poly_exponent_local(number_t value, size_t n)
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

static bool deriv_poly_expr_degree_local(const expr_t *expr,
                                         const expr_t *wrt,
                                         size_t *degree_out)
{
    expr_t *vars[1];
    size_t var_index = 0u;
    number_t value = num_new();
    number_t scale = num_new();
    const expr_t *base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    size_t left_degree = 0u;
    size_t right_degree = 0u;
    long numerator = 0L;
    long denominator = 0L;
    bool is_sub = false;
    bool ok = false;

    if (!expr || !wrt || !degree_out)
        goto cleanup;

    vars[0] = (expr_t *)wrt;
    if (expr_match_var_expr(expr, 1u, vars, &var_index) && var_index == 0u) {
        *degree_out = 1u;
        ok = true;
    } else if (expr_match_const_value(expr, &value) && num_is_real(value)) {
        *degree_out = 0u;
        ok = true;
    } else if (expr_match_unary_op(expr, EXPR_KIND_NEG, &base)) {
        ok = deriv_poly_expr_degree_local(base, wrt, degree_out);
    } else if (expr_match_scaled_expr(expr, &scale, &base) && base &&
               base != expr) {
        ok = num_is_real(scale) &&
             deriv_poly_expr_degree_local(base, wrt, degree_out);
    } else if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        (void)is_sub;
        ok = deriv_poly_expr_degree_local(left, wrt, &left_degree) &&
             deriv_poly_expr_degree_local(right, wrt, &right_degree);
        if (ok)
            *degree_out = left_degree > right_degree
                              ? left_degree
                              : right_degree;
    } else if (expr_match_mul_expr(expr, &left, &right)) {
        ok = deriv_poly_expr_degree_local(left, wrt, &left_degree) &&
             deriv_poly_expr_degree_local(right, wrt, &right_degree) &&
             left_degree <= SIZE_MAX - right_degree;
        if (ok)
            *degree_out = left_degree + right_degree;
    } else if (expr_match_pow_const(expr, &base, &value) &&
               num_get_small_rational(value, &numerator, &denominator) &&
               denominator == 1L && numerator >= 0L) {
        ok = deriv_poly_expr_degree_local(base, wrt, &left_degree) &&
             (numerator == 0L ||
              left_degree <= SIZE_MAX / (size_t)numerator);
        if (ok)
            *degree_out = left_degree * (size_t)numerator;
    }

cleanup:
    num_destroy(&scale);
    num_destroy(&value);
    return ok;
}

static bool deriv_collect_poly_local(const expr_t *expr,
                                     const expr_t *wrt,
                                     number_t *out,
                                     size_t n)
{
    expr_t *vars[1];
    size_t var_index = 0u;
    number_t value = num_new();
    number_t scale = num_new();
    const expr_t *base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t *left_poly = NULL;
    number_t *right_poly = NULL;
    bool is_sub = false;
    bool ok = false;

    if (!expr || !wrt || !out || n < 2u)
        goto cleanup_value;

    vars[0] = (expr_t *)wrt;
    if (expr_match_var_expr(expr, 1u, vars, &var_index) && var_index == 0u) {
        deriv_reset_number_array_local(out, n);
        num_destroy(&out[1]);
        out[1] = num_clone(NUM_ONE);
        ok = true;
        goto cleanup_value;
    }

    if (expr_match_const_value(expr, &value) && num_is_real(value)) {
        deriv_reset_number_array_local(out, n);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        ok = true;
        goto cleanup_value;
    }

    left_poly = deriv_alloc_number_array_local(n);
    right_poly = deriv_alloc_number_array_local(n);
    if (!left_poly || !right_poly)
        goto cleanup_arrays;

    if (expr_match_unary_op(expr, EXPR_KIND_NEG, &base)) {
        ok = deriv_collect_poly_local(base, wrt, out, n) &&
             deriv_poly_scale_local(out, NUM_NEG_ONE, out, n);
    } else if (expr_match_scaled_expr(expr, &scale, &base) && base &&
               base != expr) {
        ok = deriv_collect_poly_local(base, wrt, left_poly, n) &&
             deriv_poly_scale_local(left_poly, scale, out, n);
    } else if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        ok = deriv_collect_poly_local(left, wrt, left_poly, n) &&
             deriv_collect_poly_local(right, wrt, right_poly, n);
        if (ok) {
            for (size_t i = 0u; i < n; ++i) {
                number_t sum = is_sub ? num_sub(left_poly[i], right_poly[i])
                                      : num_add(left_poly[i], right_poly[i]);

                num_destroy(&out[i]);
                out[i] = sum;
            }
        }
    } else if (expr_match_mul_expr(expr, &left, &right)) {
        ok = deriv_collect_poly_local(left, wrt, left_poly, n) &&
             deriv_collect_poly_local(right, wrt, right_poly, n) &&
             deriv_poly_mul_local(left_poly, right_poly, out, n);
    } else if (expr_match_pow_const(expr, &base, &value)) {
        long exponent = deriv_poly_exponent_local(value, n);
        number_t *result = deriv_alloc_number_array_local(n);

        ok = result && exponent >= 0L &&
             deriv_collect_poly_local(base, wrt, left_poly, n);
        if (result) {
            num_destroy(&result[0]);
            result[0] = num_clone(NUM_ONE);
        }
        for (long i = 0L; ok && i < exponent; ++i) {
            ok = deriv_poly_mul_local(result, left_poly, right_poly, n);
            if (ok)
                deriv_copy_number_array_local(result, right_poly, n);
        }
        if (ok)
            deriv_copy_number_array_local(out, result, n);
        deriv_free_number_array_local(result, n);
    }

cleanup_arrays:
    deriv_free_number_array_local(right_poly, n);
    deriv_free_number_array_local(left_poly, n);

cleanup_value:
    num_destroy(&scale);
    num_destroy(&value);
    return ok;
}

static size_t deriv_poly_degree_local(const number_t *coeffs, size_t n)
{
    size_t degree = n ? n - 1u : 0u;

    while (degree > 0u && num_is_zero(coeffs[degree]))
        --degree;
    return degree;
}

static long deriv_long_gcd_local(long a, long b)
{
    a = labs(a);
    b = labs(b);
    while (b != 0L) {
        long r = a % b;

        a = b;
        b = r;
    }
    return a;
}

static long deriv_poly_integer_content_local(const number_t *coeffs, size_t n)
{
    long content = 0L;

    for (size_t i = 0u; i < n; ++i) {
        long numerator = 0L;
        long denominator = 0L;

        if (num_is_zero(coeffs[i]))
            continue;
        if (!num_get_small_rational(coeffs[i], &numerator, &denominator) ||
            denominator != 1L) {
            return 0L;
        }
        content = content == 0L
                      ? labs(numerator)
                      : deriv_long_gcd_local(content, numerator);
    }
    return content > 1L ? content : 0L;
}

static bool deriv_poly_divide_integer_local(const number_t *src,
                                            long divisor,
                                            number_t *out,
                                            size_t n)
{
    number_t d = num_create_from_long(divisor);

    if (divisor == 0L) {
        num_destroy(&d);
        return false;
    }

    for (size_t i = 0u; i < n; ++i) {
        number_t value = num_div(src[i], d);

        num_destroy(&out[i]);
        out[i] = value;
    }
    num_destroy(&d);
    return true;
}

static expr_t *deriv_build_flat_poly_local(const expr_t *var,
                                           const number_t *coeffs,
                                           size_t n)
{
    expr_t *sum = NULL;

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

    return sum ? sum : expr_new_const(NUM_ZERO);
}

static expr_t *deriv_build_poly_local(const expr_t *var,
                                      const number_t *coeffs,
                                      size_t n)
{
    size_t common_power = 0u;
    expr_t *inner_raw = NULL;
    expr_t *inner = NULL;
    expr_t *factor = NULL;
    expr_t *out = NULL;

    if (!var || !coeffs || n == 0u)
        return NULL;

    while (common_power < n && num_is_zero(coeffs[common_power]))
        ++common_power;
    if (common_power == 0u || common_power == n)
        return deriv_build_flat_poly_local(var, coeffs, n);

    inner_raw = deriv_build_flat_poly_local(
        var, coeffs + common_power, n - common_power);
    inner = inner_raw ? expr_simplify(inner_raw) : NULL;
    if (common_power == 1u) {
        factor = expr_retain_expr(var);
    } else {
        number_t exponent = num_create_from_long((long)common_power);

        factor = expr_pow(var, &exponent);
        num_destroy(&exponent);
    }
    out = (factor && inner) ? expr_mul(factor, inner) : NULL;

    expr_free(factor);
    expr_free(inner);
    expr_free(inner_raw);
    return out;
}

static uint64_t deriv_subtree_epoch_local(const expr_t *expr)
{
    uint64_t epoch;
    uint64_t child_epoch;

    if (!expr)
        return 0u;

    epoch = expr->epoch;
    child_epoch = deriv_subtree_epoch_local(expr->a);
    if (child_epoch > epoch)
        epoch = child_epoch;
    child_epoch = deriv_subtree_epoch_local(expr->b);
    if (child_epoch > epoch)
        epoch = child_epoch;
    return epoch;
}

static expr_t *deriv_mark_simplified_local(expr_t *expr)
{
    if (expr) {
        expr->simplified = true;
        expr->simplify_epoch = deriv_subtree_epoch_local(expr);
    }
    return expr;
}

static bool deriv_match_positive_power_local(const expr_t *expr,
                                             const expr_t **base_out,
                                             unsigned int *exponent_out)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    long numerator = 0L;
    long denominator = 0L;
    bool ok = false;

    if (!expr || !base_out || !exponent_out)
        goto cleanup;

    if (!expr_match_pow_const(expr, &base, &exponent) ||
        !num_get_small_rational(exponent, &numerator, &denominator) ||
        denominator != 1L ||
        numerator <= 0L) {
        goto cleanup;
    }

    *base_out = base;
    *exponent_out = (unsigned int)numerator;
    ok = true;

cleanup:
    num_destroy(&exponent);
    return ok;
}

expr_t *expr_deriv_rational_over_polynomial_power(const expr_t *expr,
                                                  const expr_t *wrt)
{
    enum {
        MAX_RATIONAL_POLYNOMIAL_DEGREE = 16,
        MAX_RATIONAL_DENOMINATOR_POWER = 16
    };
    const expr_t *denom_base = NULL;
    unsigned int denom_power = 1u;
    number_t *p = NULL;
    number_t *q = NULL;
    number_t *dp = NULL;
    number_t *dq = NULL;
    number_t *left = NULL;
    number_t *right = NULL;
    number_t *numer = NULL;
    number_t scale = num_new();
    number_t exponent = num_new();
    expr_t *numer_expr = NULL;
    expr_t *scaled_numer_expr = NULL;
    expr_t *denom_expr = NULL;
    expr_t *out = NULL;
    size_t numerator_degree = 0u;
    size_t denominator_degree = 0u;
    size_t coefficient_count = 0u;
    long content = 0L;

    if (!expr || !expr_is_div(expr) || !expr->a || !expr->b || !wrt)
        goto cleanup;

    if (!deriv_match_positive_power_local(
            expr->b, &denom_base, &denom_power)) {
        denom_base = expr->b;
        denom_power = 1u;
    }
    if (denom_power == 0u ||
        denom_power > MAX_RATIONAL_DENOMINATOR_POWER || !denom_base ||
        !deriv_poly_expr_degree_local(expr->a, wrt, &numerator_degree) ||
        !deriv_poly_expr_degree_local(
            denom_base, wrt, &denominator_degree) ||
        numerator_degree > MAX_RATIONAL_POLYNOMIAL_DEGREE ||
        denominator_degree == 0u ||
        denominator_degree > MAX_RATIONAL_POLYNOMIAL_DEGREE ||
        numerator_degree > SIZE_MAX - denominator_degree - 1u)
        goto cleanup;

    coefficient_count = numerator_degree + denominator_degree + 1u;
    if (coefficient_count < 3u)
        coefficient_count = 3u;

    p = deriv_alloc_number_array_local(coefficient_count);
    q = deriv_alloc_number_array_local(coefficient_count);
    dp = deriv_alloc_number_array_local(coefficient_count);
    dq = deriv_alloc_number_array_local(coefficient_count);
    left = deriv_alloc_number_array_local(coefficient_count);
    right = deriv_alloc_number_array_local(coefficient_count);
    numer = deriv_alloc_number_array_local(coefficient_count);
    if (!p || !q || !dp || !dq || !left || !right || !numer ||
        !deriv_collect_poly_local(expr->a, wrt, p, coefficient_count) ||
        !deriv_collect_poly_local(denom_base, wrt, q, coefficient_count) ||
        deriv_poly_degree_local(q, coefficient_count) != denominator_degree)
        goto cleanup;

    for (size_t i = 1u; i < coefficient_count; ++i) {
        if (i > (size_t)LONG_MAX)
            goto cleanup;
        number_t factor = num_create_from_long((long)i);

        num_destroy(&dp[i - 1u]);
        dp[i - 1u] = num_mul(factor, p[i]);
        num_destroy(&dq[i - 1u]);
        dq[i - 1u] = num_mul(factor, q[i]);
        num_destroy(&factor);
    }

    if (!deriv_poly_mul_local(dp, q, left, coefficient_count) ||
        !deriv_poly_mul_local(p, dq, right, coefficient_count))
        goto cleanup;

    num_destroy(&scale);
    scale = num_create_from_long((long)denom_power);
    if (!deriv_poly_scale_local(right, scale, right, coefficient_count))
        goto cleanup;

    for (size_t i = 0u; i < coefficient_count; ++i) {
        number_t value = num_sub(left[i], right[i]);

        num_destroy(&numer[i]);
        numer[i] = value;
    }

    content = deriv_poly_integer_content_local(numer, coefficient_count);
    if (content != 0L) {
        number_t *reduced = deriv_alloc_number_array_local(coefficient_count);
        number_t content_num = num_create_from_long(content);
        expr_t *content_expr = NULL;

        if (reduced &&
            deriv_poly_divide_integer_local(numer, content, reduced,
                                             coefficient_count)) {
            numer_expr = deriv_build_poly_local(wrt, reduced,
                                                coefficient_count);
            content_expr = expr_new_const(content_num);
            scaled_numer_expr = (content_expr && numer_expr)
                                    ? expr_mul(content_expr, numer_expr)
                                    : NULL;
        }
        expr_free(content_expr);
        deriv_free_number_array_local(reduced, coefficient_count);
        num_destroy(&content_num);
    } else {
        numer_expr = deriv_build_poly_local(wrt, numer, coefficient_count);
    }
    num_destroy(&exponent);
    exponent = num_create_from_long((long)denom_power + 1L);
    denom_expr = expr_pow(denom_base, &exponent);
    out = ((scaled_numer_expr ? scaled_numer_expr : numer_expr) && denom_expr)
              ? expr_div(scaled_numer_expr ? scaled_numer_expr : numer_expr,
                         denom_expr)
              : NULL;
    deriv_mark_simplified_local(out);

cleanup:
    expr_free(denom_expr);
    expr_free(scaled_numer_expr);
    expr_free(numer_expr);
    deriv_free_number_array_local(numer, coefficient_count);
    deriv_free_number_array_local(right, coefficient_count);
    deriv_free_number_array_local(left, coefficient_count);
    deriv_free_number_array_local(dq, coefficient_count);
    deriv_free_number_array_local(dp, coefficient_count);
    deriv_free_number_array_local(q, coefficient_count);
    deriv_free_number_array_local(p, coefficient_count);
    num_destroy(&exponent);
    num_destroy(&scale);
    return out;
}

static expr_t *deriv_rational_over_polynomial_power(expr_t *dv)
{
    return expr_deriv_rational_over_polynomial_power(
        dv, expr_current_wrt_internal());
}

static int expr_has_composite_preserved_binding_expr_node(const expr_t *dv)
{
    return expr_is_const(dv) && dv->binding_expr &&
        dv->binding_expr->kind != EXPR_BINDING_EXPR_NUMBER &&
        dv->binding_expr->kind != EXPR_BINDING_EXPR_CONST;
}

static int expr_binding_aware_search(const expr_t *dv,
                                   const expr_t *needle,
                                   int null_needle_matches_any,
                                   int match_composite_binding)
{
    expr_t *expanded;
    int depends;

    if (!dv)
        return 0;

    if (match_composite_binding &&
        expr_has_composite_preserved_binding_expr_node(dv))
        return 1;

    if (!needle) {
        if (null_needle_matches_any)
            return 1;
    } else if (dv == needle || expr_struct_eq(dv, needle)) {
        return 1;
    }

    if (expr_has_composite_preserved_binding_expr_node(dv)) {
        expanded = expr_binding_expr_eval_expr(dv->binding_expr);
        depends = expanded
            ? expr_binding_aware_search(expanded, needle,
                                      null_needle_matches_any,
                                      match_composite_binding)
            : 0;
        if (expanded)
            expr_free(expanded);
        return depends;
    }

    return expr_binding_aware_search(dv->a, needle,
                                   null_needle_matches_any,
                                   match_composite_binding) ||
           expr_binding_aware_search(dv->b, needle,
                                   null_needle_matches_any,
                                   match_composite_binding);
}

static int expr_depends_on_current_wrt(const expr_t *dv)
{
    return expr_binding_aware_search(dv, expr_current_wrt_internal(), 1, 0);
}

static int expr_depends_on_structural_node(const expr_t *dv, const expr_t *needle)
{
    if (!needle)
        return 0;
    return expr_binding_aware_search(dv, needle, 0, 0);
}

static int expr_has_composite_preserved_binding_expr(const expr_t *dv)
{
    return expr_binding_aware_search(dv, NULL, 0, 1);
}

static int expr_is_deriv_foldable_real_const(const expr_t *dv)
{
    return expr_is_unnamed_const(dv) &&
           (!dv->binding_expr || dv->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER) &&
           num_is_real(dv->c);
}

static int expr_is_sqrt_like_expr(const expr_t *dv)
{
    return expr_is_sqrt_expr(dv) ||
           (expr_is_pow_d_expr(dv) && num_eq(dv->c, NUM_HALF));
}

static const expr_t *expr_sqrt_like_arg(const expr_t *dv)
{
    return dv->a;
}

static int split_scaled_sqrt_denominator(const expr_t *dv,
                                         number_t *scale_out,
                                         const expr_t **sqrt_out)
{
    if (expr_is_sqrt_like_expr(dv)) {
        num_destroy(scale_out);
        *scale_out = num_clone(NUM_ONE);
        *sqrt_out = dv;
        return 1;
    }

    if (!expr_is_op(dv, &ops_mul))
        return 0;

    if (expr_is_deriv_foldable_real_const(dv->a) && expr_is_sqrt_like_expr(dv->b)) {
        num_destroy(scale_out);
        *scale_out = num_clone(dv->a->c);
        *sqrt_out = dv->b;
        return 1;
    }

    if (expr_is_deriv_foldable_real_const(dv->b) && expr_is_sqrt_like_expr(dv->a)) {
        num_destroy(scale_out);
        *scale_out = num_clone(dv->b->c);
        *sqrt_out = dv->a;
        return 1;
    }

    return 0;
}

static int split_exp_numerator(const expr_t *dv,
                               const expr_t **factor_out,
                               const expr_t **exp_out)
{
    if (expr_is_exp_expr(dv)) {
        *factor_out = NULL;
        *exp_out = dv;
        return 1;
    }

    if (!expr_is_op(dv, &ops_mul))
        return 0;

    if (expr_is_exp_expr(dv->a) && !expr_depends_on_current_wrt(dv->b)) {
        *factor_out = dv->b;
        *exp_out = dv->a;
        return 1;
    }

    if (expr_is_exp_expr(dv->b) && !expr_depends_on_current_wrt(dv->a)) {
        *factor_out = dv->a;
        *exp_out = dv->b;
        return 1;
    }

    return 0;
}

static int exp_arg_is_scaled_sqrt(const expr_t *arg, const expr_t *sqrt_node)
{
    const expr_t *sqrt_arg = expr_sqrt_like_arg(sqrt_node);

    if (expr_struct_eq(arg, sqrt_node))
        return 1;
    if (expr_is_sqrt_like_expr(arg) && expr_struct_eq(expr_sqrt_like_arg(arg), sqrt_arg))
        return 1;

    if (!expr_is_op(arg, &ops_mul))
        return 0;

    if (((expr_struct_eq(arg->a, sqrt_node)) ||
         (expr_is_sqrt_like_expr(arg->a) &&
          expr_struct_eq(expr_sqrt_like_arg(arg->a), sqrt_arg))) &&
        !expr_depends_on_current_wrt(arg->b))
        return 1;
    if (((expr_struct_eq(arg->b, sqrt_node)) ||
         (expr_is_sqrt_like_expr(arg->b) &&
          expr_struct_eq(expr_sqrt_like_arg(arg->b), sqrt_arg))) &&
        !expr_depends_on_current_wrt(arg->a))
        return 1;

    return 0;
}

static expr_t *deriv_exp_over_scaled_sqrt(expr_t *dv)
{
    NUM_SCOPE(scope);
    const expr_t *sqrt_den;
    const expr_t *factor;
    const expr_t *exp_node;
    number_t den_scale = num_new();
    expr_t *factor_exp = NULL;
    expr_t *numerator = NULL;
    expr_t *bracket = NULL;
    expr_t *one = NULL;
    expr_t *x_pow = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!split_scaled_sqrt_denominator(dv->b, &den_scale, &sqrt_den))
        return NULL;
    if (!split_exp_numerator(dv->a, &factor, &exp_node)) {
        num_destroy(&den_scale);
        return NULL;
    }
    if (!exp_arg_is_scaled_sqrt(exp_node->a, sqrt_den)) {
        num_destroy(&den_scale);
        return NULL;
    }
    if (factor && expr_depends_on_structural_node(factor, expr_sqrt_like_arg(sqrt_den))) {
        num_destroy(&den_scale);
        return NULL;
    }

    if (factor) {
        expr_retain(factor);
        expr_retain(exp_node);
        factor_exp = expr_mul(factor, exp_node);
        expr_free((expr_t *)factor);
        expr_free((expr_t *)exp_node);
    } else {
        expr_retain(exp_node);
        factor_exp = (expr_t *)exp_node;
    }

    expr_retain(exp_node->a);
    one = expr_new_const(NUM_ONE);
    bracket = expr_sub(exp_node->a, one);
    expr_free(exp_node->a);
    expr_free(one);

    numerator = expr_mul(factor_exp, bracket);
    expr_free(factor_exp);
    expr_free(bracket);

    {
        number_t three = num_create_from_long(3L);
        number_t three_halves = num_div(three, NUM_TWO);
        number_t two_den = num_mul(NUM_TWO, den_scale);
        number_t scalar = num_div(NUM_ONE, two_den);

        x_pow = expr_pow(expr_sqrt_like_arg(sqrt_den), &three_halves);
        quotient = expr_div(numerator, x_pow);
        out = expr_make_scaled(scalar, quotient);
        expr_free(x_pow);
        num_destroy(&three);
        num_destroy(&three_halves);
        num_destroy(&two_den);
        num_destroy(&scalar);
    }

    expr_free(numerator);

    num_destroy(&den_scale);
    return out;
}

static int split_inverse_scaled_sqrt(const expr_t *dv,
                                     number_t *num_scale_out,
                                     number_t *den_scale_out,
                                     const expr_t **sqrt_out)
{
    if (!expr_is_div(dv))
        return 0;
    if (!expr_is_deriv_foldable_real_const(dv->a))
        return 0;
    if (!split_scaled_sqrt_denominator(dv->b, den_scale_out, sqrt_out))
        return 0;

    num_destroy(num_scale_out);
    *num_scale_out = num_clone(dv->a->c);
    return 1;
}

static int split_symbolic_inverse_scaled_sqrt(const expr_t *dv,
                                              number_t *den_scale_out,
                                              const expr_t **factor_out,
                                              const expr_t **sqrt_out)
{
    if (!expr_is_div(dv))
        return 0;
    if (!split_scaled_sqrt_denominator(dv->b, den_scale_out, sqrt_out))
        return 0;
    if (expr_has_composite_preserved_binding_expr(dv->a))
        return 0;
    if (expr_depends_on_structural_node(dv->a, expr_sqrt_like_arg(*sqrt_out)))
        return 0;

    *factor_out = dv->a;
    return 1;
}

static int split_power_like(const expr_t *dv,
                            const expr_t **base_out,
                            number_t *exponent_out)
{
    if (expr_is_sqrt_like_expr(dv)) {
        *base_out = expr_sqrt_like_arg(dv);
        num_destroy(exponent_out);
        *exponent_out = num_clone(NUM_HALF);
        return 1;
    }
    if (expr_is_pow_d_expr(dv)) {
        *base_out = dv->a;
        num_destroy(exponent_out);
        *exponent_out = num_clone(dv->c);
        return 1;
    }
    return 0;
}

static int split_scaled_sqrt_factor_owned(const expr_t *term,
                                          const expr_t *base,
                                          expr_t **factor_out)
{
    if (!term || !base)
        return 0;
    if (expr_is_sqrt_like_expr(term) &&
        expr_struct_eq(expr_sqrt_like_arg(term), base)) {
        *factor_out = expr_new_const(NUM_ONE);
        return 1;
    }
    if (expr_is_neg(term)) {
        expr_t *inner_factor = NULL;

        if (!split_scaled_sqrt_factor_owned(term->a, base, &inner_factor))
            return 0;
        *factor_out = expr_neg(inner_factor);
        expr_free(inner_factor);
        return 1;
    }
    if (expr_is_mul(term)) {
        if (expr_is_sqrt_like_expr(term->a) &&
            expr_struct_eq(expr_sqrt_like_arg(term->a), base) &&
            !expr_depends_on_structural_node(term->b, base)) {
            expr_retain(term->b);
            *factor_out = term->b;
            return 1;
        }
        if (expr_is_sqrt_like_expr(term->b) &&
            expr_struct_eq(expr_sqrt_like_arg(term->b), base) &&
            !expr_depends_on_structural_node(term->a, base)) {
            expr_retain(term->a);
            *factor_out = term->a;
            return 1;
        }
    }
    return 0;
}

static int split_sqrt_affine_numerator_owned(const expr_t *num,
                                             const expr_t *base,
                                             expr_t **factor_out,
                                             expr_t **constant_out)
{
    expr_t *factor = NULL;

    if (split_scaled_sqrt_factor_owned(num, base, &factor)) {
        *factor_out = factor;
        *constant_out = expr_new_const(NUM_ZERO);
        return 1;
    }

    if (!expr_is_addsub(num))
        return 0;

    if (split_scaled_sqrt_factor_owned(num->a, base, &factor) &&
        !expr_depends_on_structural_node(num->b, base)) {
        *factor_out = factor;
        if (expr_is_op(num, &ops_sub)) {
            expr_t *neg_const;

            expr_retain(num->b);
            neg_const = expr_neg(num->b);
            expr_free(num->b);
            *constant_out = neg_const;
        } else {
            expr_retain(num->b);
            *constant_out = num->b;
        }
        return 1;
    }
    expr_free(factor);
    factor = NULL;

    if (expr_is_op(num, &ops_add) &&
        split_scaled_sqrt_factor_owned(num->b, base, &factor) &&
        !expr_depends_on_structural_node(num->a, base)) {
        expr_retain(num->a);
        *factor_out = factor;
        *constant_out = num->a;
        return 1;
    }

    if (expr_is_op(num, &ops_sub) &&
        split_scaled_sqrt_factor_owned(num->b, base, &factor) &&
        !expr_depends_on_structural_node(num->a, base)) {
        expr_t *neg_factor = expr_neg(factor);

        expr_free(factor);
        expr_retain(num->a);
        *factor_out = neg_factor;
        *constant_out = num->a;
        return 1;
    }
    expr_free(factor);
    return 0;
}

static expr_t *deriv_sqrt_affine_over_power(expr_t *dv)
{
    NUM_SCOPE(scope);
    const expr_t *base = NULL;
    number_t exponent = num_new();
    number_t half_minus_exponent = num_new();
    number_t neg_exponent = num_new();
    number_t den_exponent = num_new();
    expr_t *factor = NULL;
    expr_t *constant = NULL;
    expr_t *sqrt_base = NULL;
    expr_t *sqrt_term = NULL;
    expr_t *term1 = NULL;
    expr_t *term2 = NULL;
    expr_t *sum = NULL;
    expr_t *den = NULL;
    expr_t *out = NULL;

    if (!split_power_like(dv->b, &base, &exponent))
        goto cleanup;
    if (!split_sqrt_affine_numerator_owned(dv->a, base, &factor, &constant))
        goto cleanup;

    half_minus_exponent = num_sub(NUM_HALF, exponent);
    neg_exponent = num_neg(exponent);
    den_exponent = num_add(exponent, NUM_ONE);

    sqrt_base = expr_sqrt(base);
    sqrt_term = expr_mul(factor, sqrt_base);
    term1 = expr_make_scaled(half_minus_exponent, sqrt_term);
    sqrt_term = NULL;
    term2 = expr_make_scaled(neg_exponent, constant);
    constant = NULL;
    sum = expr_add(term1, term2);
    den = expr_pow(base, &den_exponent);
    out = expr_div(sum, den);

cleanup:
    expr_free(factor);
    expr_free(constant);
    expr_free(sqrt_base);
    expr_free(sqrt_term);
    expr_free(term1);
    expr_free(term2);
    expr_free(sum);
    expr_free(den);
    num_destroy(&exponent);
    num_destroy(&half_minus_exponent);
    num_destroy(&neg_exponent);
    num_destroy(&den_exponent);
    return out;
}

static expr_t *deriv_power_inverse_scaled_sqrt_product(expr_t *dv)
{
    NUM_SCOPE(scope);
    const expr_t *factor = NULL;
    const expr_t *sqrt_den = NULL;
    const expr_t *base = NULL;
    number_t den_scale = num_new();
    number_t exponent = num_new();
    number_t coeff = num_new();
    number_t out_exponent = num_new();
    number_t three = num_create_from_long(3L);
    number_t three_halves = num_div(three, NUM_TWO);
    expr_t *factor_scaled = NULL;
    expr_t *power_base = NULL;
    expr_t *pow_term = NULL;
    expr_t *base_dx = NULL;
    expr_t *tmp = NULL;
    expr_t *out = NULL;
    int matched;

    matched = (split_power_like(dv->a, &base, &exponent) &&
               split_symbolic_inverse_scaled_sqrt(dv->b, &den_scale,
                                                  &factor, &sqrt_den)) ||
              (split_power_like(dv->b, &base, &exponent) &&
               split_symbolic_inverse_scaled_sqrt(dv->a, &den_scale,
                                                  &factor, &sqrt_den));
    if (!matched)
        goto cleanup;
    if (!expr_struct_eq(base, expr_sqrt_like_arg(sqrt_den)))
        goto cleanup;

    coeff = num_sub(exponent, NUM_HALF);
    if (num_is_zero(coeff)) {
        out = expr_new_const(NUM_ZERO);
        goto cleanup;
    }
    coeff = num_div(coeff, den_scale);
    out_exponent = num_sub(exponent, three_halves);

    expr_retain(factor);
    factor_scaled = expr_make_scaled(coeff, (expr_t *)factor);
    expr_retain(base);
    power_base = (expr_t *)base;
    pow_term = expr_make_pow_like(power_base, out_exponent);
    base_dx = expr_get_dx_internal((expr_t *)base);

    tmp = expr_mul(factor_scaled, pow_term);
    out = expr_mul(tmp, base_dx);

cleanup:
    expr_free(factor_scaled);
    expr_free(pow_term);
    expr_free(base_dx);
    expr_free(tmp);
    num_destroy(&den_scale);
    num_destroy(&exponent);
    num_destroy(&coeff);
    num_destroy(&out_exponent);
    num_destroy(&three);
    num_destroy(&three_halves);
    return out;
}

static expr_t *deriv_exp_inverse_scaled_sqrt_product(expr_t *dv)
{
    NUM_SCOPE(scope);
    const expr_t *factor = NULL;
    const expr_t *exp_node = NULL;
    const expr_t *sqrt_den = NULL;
    number_t num_scale = num_new();
    number_t den_scale = num_new();
    expr_t *factor_exp = NULL;
    expr_t *bracket = NULL;
    expr_t *one = NULL;
    expr_t *numerator = NULL;
    expr_t *x_pow = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    int matched;

    matched = (split_exp_numerator(dv->a, &factor, &exp_node) &&
               split_inverse_scaled_sqrt(dv->b, &num_scale, &den_scale, &sqrt_den)) ||
              (split_exp_numerator(dv->b, &factor, &exp_node) &&
               split_inverse_scaled_sqrt(dv->a, &num_scale, &den_scale, &sqrt_den));
    if (!matched) {
        num_destroy(&num_scale);
        num_destroy(&den_scale);
        return NULL;
    }

    if (!exp_arg_is_scaled_sqrt(exp_node->a, sqrt_den)) {
        num_destroy(&num_scale);
        num_destroy(&den_scale);
        return NULL;
    }
    if (factor && expr_depends_on_structural_node(factor, expr_sqrt_like_arg(sqrt_den))) {
        num_destroy(&num_scale);
        num_destroy(&den_scale);
        return NULL;
    }

    if (factor) {
        expr_retain(factor);
        expr_retain(exp_node);
        factor_exp = expr_mul(factor, exp_node);
        expr_free((expr_t *)factor);
        expr_free((expr_t *)exp_node);
    } else {
        expr_retain(exp_node);
        factor_exp = (expr_t *)exp_node;
    }

    expr_retain(exp_node->a);
    one = expr_new_const(NUM_ONE);
    bracket = expr_sub(exp_node->a, one);
    expr_free(exp_node->a);
    expr_free(one);

    numerator = expr_mul(factor_exp, bracket);
    expr_free(factor_exp);
    expr_free(bracket);

    {
        number_t three = num_create_from_long(3L);
        number_t three_halves = num_div(three, NUM_TWO);
        number_t two_den = num_mul(NUM_TWO, den_scale);
        number_t scalar = num_div(num_scale, two_den);

        x_pow = expr_pow(expr_sqrt_like_arg(sqrt_den), &three_halves);
        quotient = expr_div(numerator, x_pow);
        out = expr_make_scaled(scalar, quotient);
        expr_free(x_pow);
        num_destroy(&three);
        num_destroy(&three_halves);
        num_destroy(&two_den);
        num_destroy(&scalar);
    }

    expr_free(numerator);

    num_destroy(&num_scale);
    num_destroy(&den_scale);
    return out;
}

static int split_scaled_atan(const expr_t *dv,
                             number_t *scale_out,
                             const expr_t **atan_out)
{
    if (expr_is_op(dv, &ops_atan)) {
        num_destroy(scale_out);
        *scale_out = num_clone(NUM_ONE);
        *atan_out = dv;
        return 1;
    }

    if (!expr_is_op(dv, &ops_mul))
        return 0;

    if (expr_is_deriv_foldable_real_const(dv->a) &&
        expr_is_op(dv->b, &ops_atan)) {
        num_destroy(scale_out);
        *scale_out = num_clone(dv->a->c);
        *atan_out = dv->b;
        return 1;
    }

    if (expr_is_deriv_foldable_real_const(dv->b) &&
        expr_is_op(dv->a, &ops_atan)) {
        num_destroy(scale_out);
        *scale_out = num_clone(dv->b->c);
        *atan_out = dv->a;
        return 1;
    }

    return 0;
}

static int expr_has_sqrt_like_factor(const expr_t *dv)
{
    return expr_is_sqrt_like_expr(dv) ||
           (expr_is_op(dv, &ops_mul) &&
            (expr_is_sqrt_like_expr(dv->a) ||
             expr_is_sqrt_like_expr(dv->b)));
}

static void replace_deriv_number(number_t *target, number_t value)
{
    num_destroy(target);
    *target = value;
}

static int split_numeric_affine_in_current_wrt(const expr_t *dv,
                                               number_t *linear_out,
                                               number_t *constant_out)
{
    const expr_t *wrt = expr_current_wrt_internal();

    if (!dv || !wrt)
        return 0;

    if (dv == wrt || expr_struct_eq(dv, wrt)) {
        replace_deriv_number(linear_out, num_clone(NUM_ONE));
        replace_deriv_number(constant_out, num_clone(NUM_ZERO));
        return 1;
    }

    if (expr_is_deriv_foldable_real_const(dv)) {
        replace_deriv_number(linear_out, num_clone(NUM_ZERO));
        replace_deriv_number(constant_out, num_clone(dv->c));
        return 1;
    }

    if (expr_is_neg(dv)) {
        number_t linear = num_new();
        number_t constant = num_new();

        if (!split_numeric_affine_in_current_wrt(dv->a, &linear, &constant)) {
            num_destroy(&constant);
            num_destroy(&linear);
            return 0;
        }
        replace_deriv_number(linear_out, num_neg(linear));
        replace_deriv_number(constant_out, num_neg(constant));
        num_destroy(&constant);
        num_destroy(&linear);
        return 1;
    }

    if (expr_is_addsub(dv)) {
        number_t left_linear = num_new();
        number_t left_constant = num_new();
        number_t right_linear = num_new();
        number_t right_constant = num_new();
        int matched =
            split_numeric_affine_in_current_wrt(dv->a,
                                                &left_linear,
                                                &left_constant) &&
            split_numeric_affine_in_current_wrt(dv->b,
                                                &right_linear,
                                                &right_constant);

        if (matched && expr_is_op(dv, &ops_sub)) {
            replace_deriv_number(linear_out,
                                 num_sub(left_linear, right_linear));
            replace_deriv_number(constant_out,
                                 num_sub(left_constant, right_constant));
        } else if (matched) {
            replace_deriv_number(linear_out,
                                 num_add(left_linear, right_linear));
            replace_deriv_number(constant_out,
                                 num_add(left_constant, right_constant));
        }

        num_destroy(&right_constant);
        num_destroy(&right_linear);
        num_destroy(&left_constant);
        num_destroy(&left_linear);
        return matched;
    }

    if (expr_is_mul(dv)) {
        const expr_t *factor = NULL;
        const expr_t *affine = NULL;
        number_t linear = num_new();
        number_t constant = num_new();
        int matched = 0;

        if (expr_is_deriv_foldable_real_const(dv->a)) {
            factor = dv->a;
            affine = dv->b;
        } else if (expr_is_deriv_foldable_real_const(dv->b)) {
            factor = dv->b;
            affine = dv->a;
        }

        if (factor &&
            split_numeric_affine_in_current_wrt(affine, &linear, &constant)) {
            replace_deriv_number(linear_out, num_mul(factor->c, linear));
            replace_deriv_number(constant_out, num_mul(factor->c, constant));
            matched = 1;
        }

        num_destroy(&constant);
        num_destroy(&linear);
        return matched;
    }

    return 0;
}

static int split_numeric_scaled_sqrt_denominator(const expr_t *dv,
                                                 number_t *scale_out,
                                                 number_t *radicand_out)
{
    if (!dv)
        return 0;

    if (expr_is_sqrt_like_expr(dv) &&
        expr_is_deriv_foldable_real_const(expr_sqrt_like_arg(dv))) {
        replace_deriv_number(scale_out, num_clone(NUM_ONE));
        replace_deriv_number(radicand_out,
                             num_clone(expr_sqrt_like_arg(dv)->c));
        return 1;
    }

    if (expr_is_unnamed_const(dv) && dv->binding_expr &&
        dv->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
        dv->binding_expr->u.unary_op.ops == &ops_sqrt) {
        expr_t *radicand = expr_binding_expr_eval_expr(
            dv->binding_expr->u.unary_op.child);
        int matched = expr_is_deriv_foldable_real_const(radicand);

        if (matched) {
            replace_deriv_number(scale_out, num_clone(NUM_ONE));
            replace_deriv_number(radicand_out, num_clone(radicand->c));
        }
        expr_free(radicand);
        return matched;
    }

    if (expr_is_mul(dv)) {
        const expr_t *factor = NULL;
        const expr_t *root = NULL;
        number_t inner_scale = num_new();
        number_t radicand = num_new();
        int matched = 0;

        if (expr_is_deriv_foldable_real_const(dv->a)) {
            factor = dv->a;
            root = dv->b;
        } else if (expr_is_deriv_foldable_real_const(dv->b)) {
            factor = dv->b;
            root = dv->a;
        }

        if (factor && split_numeric_scaled_sqrt_denominator(root,
                                                            &inner_scale,
                                                            &radicand)) {
            replace_deriv_number(scale_out,
                                 num_mul(factor->c, inner_scale));
            replace_deriv_number(radicand_out, num_clone(radicand));
            matched = 1;
        }
        num_destroy(&radicand);
        num_destroy(&inner_scale);
        return matched;
    }

    return 0;
}

static expr_t *deriv_atan_over_matching_sqrt(expr_t *dv,
                                             number_t atan_scale,
                                             const expr_t *atan_node)
{
    NUM_SCOPE(scope);
    const expr_t *arg = atan_node ? atan_node->a : NULL;
    const expr_t *wrt = expr_current_wrt_internal();
    number_t arg_den_scale = num_new();
    number_t outer_den_scale = num_new();
    number_t arg_radicand = num_new();
    number_t outer_radicand = num_new();
    number_t linear = num_new();
    number_t constant = num_new();
    number_t delta = num_new();
    number_t linear_sq = num_new();
    number_t linear_term_coeff = num_new();
    number_t constant_sq = num_new();
    number_t arg_den_scale_sq = num_new();
    number_t scaled_delta = num_new();
    number_t denominator_constant = num_new();
    number_t numerator_coeff = num_new();
    expr_t *x = NULL;
    expr_t *x_sq = NULL;
    expr_t *quadratic_term = NULL;
    expr_t *linear_term = NULL;
    expr_t *constant_term = NULL;
    expr_t *partial_denominator = NULL;
    expr_t *denominator = NULL;
    expr_t *numerator = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!arg || !expr_is_div(arg) || !wrt)
        goto cleanup;
    if (!split_numeric_scaled_sqrt_denominator(arg->b,
                                               &arg_den_scale,
                                               &arg_radicand) ||
        !split_numeric_scaled_sqrt_denominator(dv->b,
                                               &outer_den_scale,
                                               &outer_radicand))
        goto cleanup;
    if (!num_eq(arg_radicand, outer_radicand))
        goto cleanup;

    if (!num_gt(arg_radicand, NUM_ZERO) ||
        !split_numeric_affine_in_current_wrt(arg->a, &linear, &constant) ||
        num_is_zero(linear))
        goto cleanup;

    replace_deriv_number(&delta, num_clone(arg_radicand));
    replace_deriv_number(&linear_sq, num_mul(linear, linear));
    replace_deriv_number(&linear_term_coeff,
                         num_mul(NUM_TWO, num_mul(linear, constant)));
    replace_deriv_number(&constant_sq, num_mul(constant, constant));
    replace_deriv_number(&arg_den_scale_sq,
                         num_mul(arg_den_scale, arg_den_scale));
    replace_deriv_number(&scaled_delta,
                         num_mul(arg_den_scale_sq, delta));
    replace_deriv_number(&denominator_constant,
                         num_add(constant_sq, scaled_delta));
    replace_deriv_number(&numerator_coeff,
                         num_div(num_mul(num_mul(atan_scale, linear),
                                         arg_den_scale),
                                 outer_den_scale));

    x = expr_retain_expr(wrt);
    x_sq = x ? expr_mul(x, x) : NULL;
    expr_free(x);
    x = NULL;
    quadratic_term = x_sq ? expr_make_scaled(linear_sq, x_sq) : NULL;
    x_sq = NULL;
    x = expr_retain_expr(wrt);
    linear_term = x ? expr_make_scaled(linear_term_coeff, x) : NULL;
    x = NULL;
    constant_term = expr_new_const(denominator_constant);
    partial_denominator = (quadratic_term && linear_term)
        ? expr_add(quadratic_term, linear_term)
        : NULL;
    denominator = (partial_denominator && constant_term)
        ? expr_add(partial_denominator, constant_term)
        : NULL;
    numerator = expr_new_const(numerator_coeff);
    quotient = (numerator && denominator) ? expr_div(numerator, denominator) : NULL;
    out = quotient ? expr_simplify(quotient) : NULL;

cleanup:
    expr_free(quotient);
    expr_free(numerator);
    expr_free(denominator);
    expr_free(partial_denominator);
    expr_free(constant_term);
    expr_free(linear_term);
    expr_free(quadratic_term);
    expr_free(x_sq);
    expr_free(x);
    num_destroy(&numerator_coeff);
    num_destroy(&denominator_constant);
    num_destroy(&scaled_delta);
    num_destroy(&arg_den_scale_sq);
    num_destroy(&constant_sq);
    num_destroy(&linear_term_coeff);
    num_destroy(&linear_sq);
    num_destroy(&delta);
    num_destroy(&constant);
    num_destroy(&linear);
    num_destroy(&outer_radicand);
    num_destroy(&arg_radicand);
    num_destroy(&outer_den_scale);
    num_destroy(&arg_den_scale);
    return out;
}

static expr_t *deriv_atan_over_scaled_sqrt(expr_t *dv)
{
    const expr_t *atan_node = NULL;
    number_t scale = num_new();
    expr_t *arg_dx = NULL;
    expr_t *arg_sq = NULL;
    expr_t *one = NULL;
    expr_t *atan_den = NULL;
    expr_t *full_den = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!split_scaled_atan(dv->a, &scale, &atan_node))
        goto cleanup;

    out = deriv_atan_over_matching_sqrt(dv, scale, atan_node);
    if (out)
        goto cleanup;
    if (!expr_has_sqrt_like_factor(dv->b))
        goto cleanup;

    arg_dx = expr_get_dx_internal(atan_node->a);
    arg_sq = expr_mul(atan_node->a, atan_node->a);
    one = expr_new_const(NUM_ONE);
    atan_den = expr_add(one, arg_sq);
    full_den = expr_mul(dv->b, atan_den);
    quotient = expr_div(arg_dx, full_den);
    out = expr_make_scaled(scale, quotient);

cleanup:
    expr_free(arg_dx);
    expr_free(arg_sq);
    expr_free(one);
    expr_free(atan_den);
    expr_free(full_den);
    num_destroy(&scale);
    return out;
}

static expr_t *deriv_div(expr_t *dv)
{
    NUM_SCOPE(scope);
    expr_t *special = deriv_exp_over_scaled_sqrt(dv);

    if (special)
        return special;
    special = deriv_sqrt_affine_over_power(dv);
    if (special)
        return special;
    special = deriv_atan_over_scaled_sqrt(dv);
    if (special)
        return special;
    special = deriv_rational_over_polynomial_power(dv);
    if (special)
        return special;

    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *db   = expr_get_dx_internal(dv->b);
    expr_t *num1 = expr_mul(da, dv->b);
    expr_t *num2 = expr_mul(dv->a, db);
    expr_t *num  = expr_sub(num1, num2);
    number_t two = num_create_from_long(2);
    expr_t *den  = expr_pow(dv->b, &two);
    expr_t *out  = expr_div(num, den);
    expr_free(da);
    expr_free(db);
    expr_free(num1);
    expr_free(num2);
    expr_free(num);
    expr_free(den);
    return out;
}

static expr_t *deriv_neg(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *out = expr_neg(da);
    expr_free(da);
    return out;
}

static expr_t *expr_unary_constexpr_from_preserved_arg(const expr_ops_t *ops,
                                                     const expr_t *arg)
{
    expr_binding_expr_t *expr;
    number_t value;
    expr_t *node;

    if (!arg || !expr_is_const(arg) || !arg->binding_expr ||
        (arg->name && *arg->name))
        return NULL;

    expr = expr_binding_expr_new_unary_op(ops,
                                        expr_binding_expr_clone(arg->binding_expr));
    expr = expr_binding_expr_simplify(expr);
    value = expr_binding_expr_eval(expr);
    node = expr_new_const(value);
    num_destroy(&value);
    node->binding_expr = expr;
    return node;
}

static expr_t *expr_log_preserving_constexpr(const expr_t *arg)
{
    expr_t *preserved = expr_unary_constexpr_from_preserved_arg(&ops_log, arg);

    return preserved ? preserved : expr_log(arg);
}

static expr_t *deriv_pow(expr_t *dv)
{
    expr_t *a  = dv->a;
    expr_t *b  = dv->b;
    expr_t *da = expr_get_dx_internal(a);
    expr_t *db = expr_get_dx_internal(b);

    expr_t *loga    = expr_log_preserving_constexpr(a);
    expr_t *da_on_a = expr_div(da, a);
    expr_t *term1   = expr_mul(db, loga);
    expr_t *term2   = expr_mul(b, da_on_a);
    expr_t *sum     = expr_add(term1, term2);
    expr_t *powab   = expr_pow_xp(a, b);
    expr_t *out     = expr_mul(sum, powab);

    expr_free(da);
    expr_free(db);
    expr_free(loga);
    expr_free(da_on_a);
    expr_free(term1);
    expr_free(term2);
    expr_free(sum);
    expr_free(powab);

    return out;
}

static expr_t *deriv_pow_d(expr_t *dv)
{
    NUM_SCOPE(scope);
    number_t exponent = num_clone(dv->c);
    number_t exponent_minus_one = num_sub(exponent, NUM_ONE);
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *p    = expr_pow(dv->a, &exponent_minus_one);
    expr_t *coef = expr_new_const(exponent);
    expr_t *cp   = expr_mul(coef, p);
    expr_t *out  = expr_mul(cp, da);
    expr_free(da);
    expr_free(p);
    expr_free(coef);
    expr_free(cp);
    return out;
}

static expr_t *deriv_integral_bounds(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_NAN);
}

static expr_t *deriv_integral_meta(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_NAN);
}

static expr_t *deriv_integral(expr_t *dv)
{
    const expr_t *wrt;
    const expr_t *lower;
    const expr_t *upper;
    const expr_t *dummy;
    expr_t *integrand_deriv = NULL;
    expr_t *integrand_term = NULL;
    expr_t *upper_integrand = NULL;
    expr_t *upper_deriv;
    expr_t *upper_term;
    expr_t *lower_integrand = NULL;
    expr_t *lower_deriv = NULL;
    expr_t *lower_term = NULL;
    expr_t *boundary_term = NULL;
    expr_t *out;

    if (!dv || !dv->a || !dv->b)
        return expr_new_const(NUM_NAN);
    wrt = expr_current_wrt_internal();
    lower = expr_integral_lower_bound_expr(dv);
    upper = expr_integral_upper_bound_expr(dv);
    dummy = expr_integral_dummy_expr(dv);
    if (!upper || !dummy)
        return expr_new_const(NUM_NAN);

    integrand_deriv = wrt ? expr_create_deriv(dv->a, wrt) : expr_get_dx_internal(dv->a);
    if (!integrand_deriv)
        return expr_new_const(NUM_NAN);
    if (!expr_is_exact_zero(integrand_deriv)) {
        integrand_term = lower
            ? expr_integral_with_bounds_internal(integrand_deriv, lower, upper, dummy)
            : expr_integral_with_dummy_internal(integrand_deriv, upper, dummy);
        if (!integrand_term) {
            expr_free(integrand_deriv);
            return expr_new_const(NUM_NAN);
        }
    }
    expr_free(integrand_deriv);

    upper_integrand = expr_substitute(dv->a, dummy, (expr_t *)upper);
    if (!upper_integrand)
        goto fail;

    upper_deriv = expr_get_dx_internal(upper);
    if (!upper_deriv) {
        expr_free(upper_integrand);
        goto fail;
    }
    upper_term = expr_mul(upper_integrand, upper_deriv);
    expr_free(upper_deriv);
    expr_free(upper_integrand);
    if (!upper_term)
        goto fail;

    if (!lower) {
        boundary_term = upper_term;
    } else {
        lower_integrand = expr_substitute(dv->a, dummy, (expr_t *)lower);
        lower_deriv = expr_get_dx_internal(lower);
        if (!lower_integrand || !lower_deriv) {
            expr_free(lower_deriv);
            expr_free(lower_integrand);
            expr_free(upper_term);
            goto fail;
        }

        lower_term = expr_mul(lower_integrand, lower_deriv);
        expr_free(lower_deriv);
        expr_free(lower_integrand);
        if (!lower_term) {
            expr_free(upper_term);
            goto fail;
        }

        boundary_term = expr_sub(upper_term, lower_term);
        expr_free(lower_term);
        expr_free(upper_term);
        if (!boundary_term)
            goto fail;
    }

    if (!integrand_term)
        return boundary_term;

    out = expr_add(boundary_term, integrand_term);
    expr_free(integrand_term);
    expr_free(boundary_term);
    return out ? out : expr_new_const(NUM_NAN);

fail:
    expr_free(integrand_term);
    return expr_new_const(NUM_NAN);
}

static expr_t *expr_integral_meta_apply(const expr_t *domain,
                                        const expr_t *dummy)
{
    if (!domain || !dummy)
        return NULL;
    expr_retain(domain);
    expr_retain(dummy);
    return expr_new_binary_internal(&ops_integral_meta, domain, dummy);
}

static expr_t *expr_integral_bounds_apply(const expr_t *lower,
                                          const expr_t *upper)
{
    if (!lower || !upper)
        return NULL;
    expr_retain(lower);
    expr_retain(upper);
    return expr_new_binary_internal(&ops_integral_bounds, lower, upper);
}

static bool expr_contains_free_var_named_impl(const expr_t *expr,
                                              const char *name,
                                              const expr_t *bound_dummy)
{
    const expr_t *dummy;
    const expr_t *lower;
    const expr_t *upper;

    if (!expr || !name)
        return false;

    if (expr_is_var(expr)) {
        bool shadowed = bound_dummy == expr ||
            (bound_dummy && bound_dummy->var_id != 0 &&
             expr->var_id != 0 &&
             bound_dummy->var_id == expr->var_id);

        return !shadowed && expr->name && strcmp(expr->name, name) == 0;
    }

    if (expr_is_op(expr, &ops_integral)) {
        dummy = expr_integral_dummy_expr(expr);
        lower = expr_integral_lower_bound_expr(expr);
        upper = expr_integral_upper_bound_expr(expr);

        return expr_contains_free_var_named_impl(lower, name, bound_dummy) ||
               expr_contains_free_var_named_impl(upper, name, bound_dummy) ||
               expr_contains_free_var_named_impl(expr->a, name, dummy);
    }

    return expr_contains_free_var_named_impl(expr->a, name, bound_dummy) ||
           expr_contains_free_var_named_impl(expr->b, name, bound_dummy);
}

static bool expr_contains_free_var_named(const expr_t *expr, const char *name)
{
    return expr_contains_free_var_named_impl(expr, name, NULL);
}

static const char *expr_choose_integral_dummy_name(const expr_t *integrand,
                                                   const expr_t *wrt)
{
    static const char *const candidates[] = {
        "t", "u", "v", "w", "s", "r", "q", "p"
    };

    if (!integrand || !expr_is_var(wrt))
        return wrt && wrt->name ? wrt->name : "t";

    for (size_t i = 0u; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        const char *candidate = candidates[i];

        if (wrt->name && strcmp(wrt->name, candidate) == 0)
            continue;
        if (!expr_contains_free_var_named(integrand, candidate))
            return candidate;
    }

    return (wrt->name && *wrt->name) ? wrt->name : "t";
}

/* ------------------------------------------------------------------------- */
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

const expr_ops_t ops_const = {
    .eval = eval_const,
    .deriv = deriv_const,
    .reverse = expr_reverse_atom,
    .kind = EXPR_KIND_CONST,
    .arity = EXPR_OP_ATOM,
    .name = "const",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = expr_simplify_passthrough,
    .fold_const_unary = NULL
};

const expr_ops_t ops_var = {
    .eval = eval_var,
    .deriv = deriv_var,
    .reverse = expr_reverse_atom,
    .kind = EXPR_KIND_VAR,
    .arity = EXPR_OP_ATOM,
    .name = "var",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = expr_simplify_passthrough,
    .fold_const_unary = NULL
};

const expr_ops_t ops_formal_derivative = {
    .eval = eval_formal_derivative,
    .deriv = deriv_formal_derivative,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_FORMAL_DERIVATIVE,
    .arity = EXPR_OP_UNARY,
    .diff_kind = EXPR_DIFF_NONE,
    .name = "D",
    .tex_name = "\\operatorname{D}",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = expr_simplify_passthrough,
    .fold_const_unary = NULL
};

const expr_ops_t ops_arbitrary_function = {
    .eval = eval_arbitrary_function,
    .deriv = deriv_arbitrary_function,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_ARBITRARY_FUNCTION,
    .arity = EXPR_OP_UNARY,
    .diff_kind = EXPR_DIFF_SMOOTH,
    .name = "arbitrary-function",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = expr_simplify_passthrough,
    .fold_const_unary = NULL
};

const expr_ops_t ops_argument_list = {
    .eval = eval_argument_list,
    .deriv = NULL,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_ARGUMENT_LIST,
    .arity = EXPR_OP_BINARY,
    .diff_kind = EXPR_DIFF_NONE,
    .name = "argument-list",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = expr_new_argument_list,
    .simplify = expr_simplify_passthrough,
    .fold_const_unary = NULL
};

const expr_ops_t ops_add = {
    .eval = eval_add,
    .deriv = deriv_add,
    .reverse = expr_reverse_add,
    .kind = EXPR_KIND_ADD,
    .arity = EXPR_OP_BINARY,
    .name = "+",
    .tex_name = "+",
    .apply_unary = NULL,
    .apply_binary = expr_add,
    .simplify = expr_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_sub = {
    .eval = eval_sub,
    .deriv = deriv_sub,
    .reverse = expr_reverse_sub,
    .kind = EXPR_KIND_SUB,
    .arity = EXPR_OP_BINARY,
    .name = "-",
    .tex_name = "-",
    .apply_unary = NULL,
    .apply_binary = expr_sub,
    .simplify = expr_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_mul = {
    .eval = eval_mul,
    .deriv = deriv_mul,
    .reverse = expr_reverse_mul,
    .kind = EXPR_KIND_MUL,
    .arity = EXPR_OP_BINARY,
    .name = "*",
    .tex_name = "\\cdot",
    .apply_unary = NULL,
    .apply_binary = expr_mul,
    .simplify = expr_simplify_mul_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_div = {
    .eval = eval_div,
    .deriv = deriv_div,
    .reverse = expr_reverse_div,
    .kind = EXPR_KIND_DIV,
    .arity = EXPR_OP_BINARY,
    .name = "/",
    .tex_name = "/",
    .apply_unary = NULL,
    .apply_binary = expr_div,
    .simplify = expr_simplify_div_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_pow = {
    .eval = eval_pow,
    .deriv = deriv_pow,
    .reverse = expr_reverse_pow,
    .kind = EXPR_KIND_POW,
    .arity = EXPR_OP_BINARY,
    .name = "^",
    .tex_name = "^",
    .apply_unary = NULL,
    .apply_binary = expr_pow_xp,
    .simplify = expr_simplify_pow_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_pow_d = {
    .eval = eval_pow_d,
    .deriv = deriv_pow_d,
    .reverse = expr_reverse_pow_d,
    .kind = EXPR_KIND_POW_D,
    .arity = EXPR_OP_BINARY,
    .name = "^",
    .tex_name = "^",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = expr_simplify_pow_d_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_integral = {
    .eval = eval_integral,
    .deriv = deriv_integral,
    /* Unevaluated integrals support symbolic d/dx via the fundamental theorem,
     * but they are not part of the numeric reverse-mode AD pipeline. */
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_INTEGRAL,
    .arity = EXPR_OP_BINARY,
    .diff_kind = EXPR_DIFF_SMOOTH,
    .name = "integral",
    .tex_name = "\\int",
    .apply_unary = NULL,
    .apply_binary = expr_integral,
    .simplify = expr_simplify_rebuild_binary_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_integral_meta = {
    .eval = eval_integral_meta,
    .deriv = deriv_integral_meta,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_INTEGRAL_META,
    .arity = EXPR_OP_BINARY,
    .diff_kind = EXPR_DIFF_SMOOTH,
    .name = "integral_meta",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = expr_integral_meta_apply,
    .simplify = expr_simplify_rebuild_binary_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_integral_bounds = {
    .eval = eval_integral_bounds,
    .deriv = deriv_integral_bounds,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_INTEGRAL_BOUNDS,
    .arity = EXPR_OP_BINARY,
    .diff_kind = EXPR_DIFF_SMOOTH,
    .name = "integral_bounds",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = expr_integral_bounds_apply,
    .simplify = expr_simplify_rebuild_binary_operator,
    .fold_const_unary = NULL
};

const expr_ops_t ops_neg = {
    .eval = eval_neg,
    .deriv = deriv_neg,
    .reverse = expr_reverse_neg,
    .kind = EXPR_KIND_NEG,
    .arity = EXPR_OP_UNARY,
    .name = "-",
    .tex_name = "-",
    .apply_unary = expr_neg,
    .apply_binary = NULL,
    .simplify = expr_simplify_neg_operator,
    .fold_const_unary = NULL
};

/* ------------------------------------------------------------------------- */
/* Arithmetic constructors (retain children)                                 */
/* ------------------------------------------------------------------------- */

expr_t *expr_neg(const expr_t *dv)
{
    if (!dv)
        return NULL;
    expr_retain(dv);
    return expr_new_unary_internal(&ops_neg, dv);
}

expr_t *expr_add(const expr_t *expr1, const expr_t *expr2)
{
    if (!expr1 || !expr2)
        return NULL;
    expr_retain(expr1);
    expr_retain(expr2);
    return expr_new_binary_internal(&ops_add, expr1, expr2);
}

expr_t *expr_sub(const expr_t *expr1, const expr_t *expr2)
{
    if (!expr1 || !expr2)
        return NULL;
    expr_retain(expr1);
    expr_retain(expr2);
    return expr_new_binary_internal(&ops_sub, expr1, expr2);
}

expr_t *expr_mul(const expr_t *expr1, const expr_t *expr2)
{
    if (!expr1 || !expr2)
        return NULL;
    expr_retain(expr1);
    expr_retain(expr2);
    return expr_new_binary_internal(&ops_mul, expr1, expr2);
}

expr_t *expr_div(const expr_t *expr1, const expr_t *expr2)
{
    if (!expr1 || !expr2)
        return NULL;
    expr_retain(expr1);
    expr_retain(expr2);
    return expr_new_binary_internal(&ops_div, expr1, expr2);
}

expr_t *expr_pow_xp(const expr_t *a, const expr_t *b)
{
    if (!a || !b)
        return NULL;
    expr_retain(a);
    expr_retain(b);
    return expr_new_binary_internal(&ops_pow, a, b);
}

expr_t *expr_integral(const expr_t *integrand, const expr_t *wrt)
{
    expr_t *dummy;
    expr_t *display_integrand;
    expr_t *integral;
    const char *dummy_name;

    if (!integrand || !wrt)
        return NULL;
    if (!expr_is_var(wrt))
        return expr_integral_with_dummy_internal(integrand, wrt, wrt);

    dummy_name = expr_choose_integral_dummy_name(integrand, wrt);
    dummy = expr_new_named_var(NUM_ZERO, dummy_name);
    if (!dummy)
        return NULL;

    display_integrand = expr_substitute(integrand, wrt, dummy);
    if (!display_integrand) {
        expr_free(dummy);
        return NULL;
    }

    integral = expr_integral_with_dummy_internal(display_integrand, wrt, dummy);
    expr_free(display_integrand);
    expr_free(dummy);
    return integral;
}

expr_t *expr_integral_with_dummy_internal(const expr_t *integrand,
                                          const expr_t *upper,
                                          const expr_t *dummy)
{
    expr_t *meta;
    expr_t *integral;

    if (!integrand || !upper || !dummy)
        return NULL;

    expr_retain(upper);
    expr_retain(dummy);
    meta = expr_new_binary_internal(&ops_integral_meta, upper, dummy);
    if (!meta) {
        expr_free((expr_t *)dummy);
        expr_free((expr_t *)upper);
        return NULL;
    }

    expr_retain(integrand);
    integral = expr_new_binary_internal(&ops_integral, integrand, meta);
    if (!integral) {
        expr_free(meta);
        expr_free((expr_t *)integrand);
    }
    return integral;
}

expr_t *expr_integral_with_bounds_internal(const expr_t *integrand,
                                           const expr_t *lower,
                                           const expr_t *upper,
                                           const expr_t *dummy)
{
    expr_t *bounds;
    expr_t *meta;
    expr_t *integral;

    if (!integrand || !lower || !upper || !dummy)
        return NULL;

    expr_retain(lower);
    expr_retain(upper);
    bounds = expr_new_binary_internal(&ops_integral_bounds, lower, upper);
    if (!bounds) {
        expr_free((expr_t *)upper);
        expr_free((expr_t *)lower);
        return NULL;
    }

    expr_retain(dummy);
    meta = expr_new_binary_internal(&ops_integral_meta, bounds, dummy);
    if (!meta) {
        expr_free(bounds);
        expr_free((expr_t *)dummy);
        return NULL;
    }

    expr_retain(integrand);
    integral = expr_new_binary_internal(&ops_integral, integrand, meta);
    if (!integral) {
        expr_free(meta);
        expr_free((expr_t *)integrand);
    }
    return integral;
}

expr_t *expr_pow(const expr_t *dv, const number_t *exponent)
{
    if (!dv || !exponent)
        return NULL;
    expr_retain(dv);
    return expr_new_pow_const_internal(dv, *exponent);
}

expr_t *expr_add_num(const expr_t *dv, const number_t *value)
{
    expr_t *c;
    expr_t *r;

    if (!value)
        return NULL;
    c = expr_new_const(*value);
    if (!c)
        return NULL;
    r = expr_add(dv, c);
    expr_free(c);
    return r;
}

expr_t *expr_sub_num(const expr_t *dv, const number_t *value)
{
    expr_t *c;
    expr_t *r;

    if (!value)
        return NULL;
    c = expr_new_const(*value);
    if (!c)
        return NULL;
    r = expr_sub(dv, c);
    expr_free(c);
    return r;
}

expr_t *expr_num_sub(const number_t *value, const expr_t *dv)
{
    expr_t *c;
    expr_t *r;

    if (!value)
        return NULL;
    c = expr_new_const(*value);
    if (!c)
        return NULL;
    r = expr_sub(c, dv);
    expr_free(c);
    return r;
}

expr_t *expr_mul_num(const expr_t *dv, const number_t *value)
{
    expr_t *c;
    expr_t *r;

    if (!value)
        return NULL;
    c = expr_new_const(*value);
    if (!c)
        return NULL;
    r = expr_mul(dv, c);
    expr_free(c);
    return r;
}

expr_t *expr_div_num(const expr_t *dv, const number_t *value)
{
    expr_t *c;
    expr_t *r;

    if (!value)
        return NULL;
    c = expr_new_const(*value);
    if (!c)
        return NULL;
    r = expr_div(dv, c);
    expr_free(c);
    return r;
}

expr_t *expr_num_div(const number_t *value, const expr_t *dv)
{
    expr_t *c;
    expr_t *r;

    if (!value)
        return NULL;
    c = expr_new_const(*value);
    if (!c)
        return NULL;
    r = expr_div(c, dv);
    expr_free(c);
    return r;
}
