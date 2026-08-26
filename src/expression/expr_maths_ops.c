#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_maths.h"
#include "ustring.h"

static inline expr_t *expr_math_wrap_unary(const expr_ops_t *ops, const expr_t *a)
{
    if (!a)
        return NULL;
    expr_retain(a);
    return expr_new_unary_internal(ops, a);
}

static inline expr_t *expr_math_wrap_binary(const expr_ops_t *ops, const expr_t *a, const expr_t *b)
{
    if (!a || !b)
        return NULL;
    expr_retain(a);
    expr_retain(b);
    return expr_new_binary_internal(ops, a, b);
}

static expr_t *expr_appell_f1_pack(const expr_t *left, const expr_t *right);
static expr_t *expr_appell_f1_from_packs(const expr_t *params, const expr_t *vars);
static expr_t *expr_lauricella_f_from_packs(const expr_t *params, const expr_t *vars);
static expr_t *expr_hypergeometric_pFq_pack(const expr_t *left, const expr_t *right);
static expr_t *expr_hypergeometric_pFq_from_pack(const expr_t *parameters, const expr_t *argument);
static expr_t *expr_lommel_s_pack(const expr_t *mu, const expr_t *nu);
static expr_t *expr_lommel_s_from_pack(const expr_t *parameters, const expr_t *argument);
expr_t *expr_finite_progression_closed_form(const expr_t *expr);

static number_t eval_formal_series_component(expr_t *dv)
{
    (void)dv;
    return num_clone(NUM_NAN);
}

typedef enum {
    EXPR_FINITE_WEIGHTED_HYPERBOLIC_NONE,
    EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH,
    EXPR_FINITE_WEIGHTED_HYPERBOLIC_COSH,
} expr_finite_weighted_hyperbolic_kind_t;

typedef enum {
    EXPR_FINITE_WEIGHTED_CIRCULAR_NONE,
    EXPR_FINITE_WEIGHTED_CIRCULAR_SIN,
    EXPR_FINITE_WEIGHTED_CIRCULAR_COS,
} expr_finite_weighted_circular_kind_t;

typedef expr_t *(*expr_finite_progression_step_from_q_fn)(const expr_t *q);

typedef enum {
    EXPR_QDIGAMMA_SECANT_NONE,
    EXPR_QDIGAMMA_SECANT_CIRCULAR,
    EXPR_QDIGAMMA_SECANT_HYPERBOLIC,
} expr_qdigamma_secant_signature_t;

typedef struct {
    expr_apply_unary_fn apply;
    expr_finite_progression_fn closed_form;
    expr_finite_progression_step_from_q_fn step_from_q;
    expr_qdigamma_secant_signature_t secant_signature;
} expr_qdigamma_progression_t;

typedef struct {
    const expr_t *first;
    const expr_t *second;
    const expr_t *alternate_first;
    const expr_t *alternate_second;
    size_t count;
    bool single_base;
} expr_qdigamma_scan_t;

static expr_t *expr_finite_sec_progression_closed_form(const expr_t *upper, const expr_t *step);
static expr_t *expr_finite_cosec_progression_closed_form(const expr_t *upper, const expr_t *step);
static expr_t *expr_finite_sech_progression_closed_form(const expr_t *upper, const expr_t *step);
static expr_t *expr_finite_cosech_progression_closed_form(const expr_t *upper, const expr_t *step);

static bool expr_finite_weighted_hyperbolic_parts(const expr_t *expr, const expr_t **upper_out,
                                                   const expr_t **step_out,
                                                   expr_finite_weighted_hyperbolic_kind_t *kind_out);
static number_t eval_finite_weighted_hyperbolic(expr_t *dv, long upper_value);
static number_t eval_finite_qdigamma_progression(const expr_t *expr, long upper_value);
static number_t eval_finite_exponential_progression(const expr_t *expr, long upper_value);
static number_t eval_finite_zero_atan_progression(const expr_t *expr);
static number_t eval_finite_inverse_progression_pole(const expr_t *expr, long upper_value);
static number_t eval_finite_inverse_progression(const expr_t *expr, long upper_value);
static number_t eval_finite_atan_progression(const expr_t *expr, long upper_value);
static bool expr_finite_weighted_circular_parts(const expr_t *expr, const expr_t **upper_out,
                                                const expr_t **step_out,
                                                expr_finite_weighted_circular_kind_t *kind_out);
static expr_t *expr_finite_weighted_circular_from_lerch_form(
    const expr_t *expr, expr_finite_weighted_circular_kind_t kind);

static const expr_t *expr_find_lerch_phi(const expr_t *expr)
{
    const expr_t *found;

    if (!expr)
        return NULL;
    if (expr_is_op(expr, &ops_lerch_phi))
        return expr;
    found = expr_find_lerch_phi(expr->a);
    return found ? found : expr_find_lerch_phi(expr->b);
}

static const expr_t *expr_find_qdigamma_with_symbolic_argument(const expr_t *expr)
{
    const expr_t *found;
    number_t argument_value;

    if (!expr)
        return NULL;
    if (expr_is_op(expr, &ops_qdigamma) && expr->b) {
        argument_value = expr_eval(expr->b);
        if (!num_is_exact(argument_value) || !num_is_one(argument_value)) {
            num_destroy(&argument_value);
            return expr;
        }
        num_destroy(&argument_value);
    }
    found = expr_find_qdigamma_with_symbolic_argument(expr->a);
    return found ? found : expr_find_qdigamma_with_symbolic_argument(expr->b);
}

static bool expr_summation_bound_to_long(const expr_t *bound, long *out)
{
    number_t value;
    string_t *text;
    char *end = NULL;
    long parsed;

    if (!bound || !out)
        return false;
    value = expr_eval(bound);
    if (!num_is_real(value) || !num_is_finite(value) || !num_is_integer(value)) {
        num_destroy(&value);
        return false;
    }
    text = num_to_string(value);
    num_destroy(&value);
    if (!text)
        return false;
    parsed = strtol(string_c_str(text), &end, 10);
    if (!end || *end != '\0') {
        string_free(text);
        return false;
    }
    string_free(text);
    *out = parsed;
    return true;
}

static number_t eval_finite_summation(expr_t *dv)
{
    const long maximum_terms = 10000L;
    expr_t *index;
    const expr_t *lower = NULL;
    const expr_t *upper = NULL;
    number_t sum;
    long lower_value;
    long upper_value;

    if (!dv || !dv->a || !expr_is_op(dv->b, &ops_argument_list))
        return num_clone(NUM_NAN);
    index = dv->b->a;
    upper = dv->b->b;
    if (expr_is_op(upper, &ops_argument_list)) {
        lower = upper->a;
        upper = upper->b;
    }
    lower_value = 0L;
    if (!expr_is_var(index) || !upper || (lower && !expr_summation_bound_to_long(lower, &lower_value)) ||
        !expr_summation_bound_to_long(upper, &upper_value))
        return num_clone(NUM_NAN);
    if (upper_value >= lower_value && upper_value - lower_value >= maximum_terms) {
        expr_t *closed_form = lower_value == 1L ? expr_finite_progression_closed_form(dv) : NULL;

        if (lower_value == 1L) {
            number_t inverse_pole_value = eval_finite_inverse_progression_pole(dv, upper_value);

            if (num_is_inf(inverse_pole_value)) {
                expr_free(closed_form);
                return inverse_pole_value;
            }
            num_destroy(&inverse_pole_value);
        }
        if (lower_value == 1L) {
            number_t zero_atan_value = eval_finite_zero_atan_progression(dv);

            if (num_is_finite(zero_atan_value)) {
                expr_free(closed_form);
                return zero_atan_value;
            }
            num_destroy(&zero_atan_value);
        }
        if (lower_value == 1L) {
            number_t atan_value = eval_finite_atan_progression(dv, upper_value);

            if (num_is_finite(atan_value)) {
                expr_free(closed_form);
                return atan_value;
            }
            num_destroy(&atan_value);
        }
        if (lower_value == 1L) {
            number_t exponential_value = eval_finite_exponential_progression(dv, upper_value);

            if (num_is_finite(exponential_value)) {
                expr_free(closed_form);
                return exponential_value;
            }
            num_destroy(&exponential_value);
        }
        if (lower_value == 1L) {
            number_t progression_value = eval_finite_qdigamma_progression(dv, upper_value);

            if (num_is_finite(progression_value)) {
                expr_free(closed_form);
                return progression_value;
            }
            num_destroy(&progression_value);
        }
        if (closed_form) {
            number_t closed_value = expr_eval(closed_form);

            expr_free(closed_form);
            if (num_is_finite(closed_value)) {
                number_t imaginary_part = num_imag_part(closed_value);

                if (num_is_zero(imaginary_part)) {
                    number_t real_value = num_real_part(closed_value);

                    num_destroy(&imaginary_part);
                    num_destroy(&closed_value);
                    return real_value;
                }
                num_destroy(&imaginary_part);
                return closed_value;
            }
            num_destroy(&closed_value);
        }
        if (lower_value == 1L) {
            number_t inverse_value = eval_finite_inverse_progression(dv, upper_value);

            if (num_is_finite(inverse_value))
                return inverse_value;
            num_destroy(&inverse_value);
        }
        if (lower_value == 1L)
            return eval_finite_weighted_hyperbolic(dv, upper_value);
        return num_clone(NUM_NAN);
    }

    sum = num_clone(NUM_ZERO);
    for (long value = lower_value; value <= upper_value; ++value) {
        number_t index_value = num_create_from_long(value);
        expr_t *index_expression = expr_new_const(index_value);
        expr_t *term_expression = index_expression ? expr_substitute(dv->a, index, index_expression) : NULL;
        number_t term;
        number_t updated;

        num_destroy(&index_value);
        term = term_expression ? expr_eval(term_expression) : num_clone(NUM_NAN);
        expr_free(term_expression);
        expr_free(index_expression);
        if (!num_is_finite(term)) {
            num_destroy(&term);
            num_destroy(&sum);
            sum = num_clone(NUM_NAN);
            break;
        }
        updated = num_add(sum, term);
        num_destroy(&term);
        num_destroy(&sum);
        sum = updated;
        if (value == LONG_MAX)
            break;
    }
    return sum;
}

static number_t eval_finite_product(expr_t *dv)
{
    const long maximum_terms = 1000000L;
    expr_t *index;
    const expr_t *lower = NULL;
    const expr_t *upper = NULL;
    number_t product;
    long lower_value;
    long upper_value;

    if (!dv || !dv->a || !expr_is_op(dv->b, &ops_argument_list))
        return num_clone(NUM_NAN);
    index = dv->b->a;
    upper = dv->b->b;
    if (expr_is_op(upper, &ops_argument_list)) {
        lower = upper->a;
        upper = upper->b;
    }
    lower_value = 0L;
    if (!expr_is_var(index) || !upper || (lower && !expr_summation_bound_to_long(lower, &lower_value)) ||
        !expr_summation_bound_to_long(upper, &upper_value))
        return num_clone(NUM_NAN);
    if (upper_value >= lower_value && upper_value - lower_value >= maximum_terms)
        return num_clone(NUM_NAN);

    product = num_clone(NUM_ONE);
    for (long value = lower_value; value <= upper_value; ++value) {
        number_t index_value = num_create_from_long(value);
        expr_t *index_expression = expr_new_const(index_value);
        expr_t *factor_expression = index_expression ? expr_substitute(dv->a, index, index_expression) : NULL;
        number_t factor;
        number_t updated;

        num_destroy(&index_value);
        factor = factor_expression ? expr_eval(factor_expression) : num_clone(NUM_NAN);
        expr_free(factor_expression);
        expr_free(index_expression);
        if (!num_is_finite(factor)) {
            num_destroy(&factor);
            num_destroy(&product);
            product = num_clone(NUM_NAN);
            break;
        }
        updated = num_mul(product, factor);
        num_destroy(&factor);
        num_destroy(&product);
        product = updated;
        if (value == LONG_MAX)
            break;
    }
    return product;
}

static expr_t *deriv_indexed_symbol(expr_t *dv)
{
    expr_t *index_derivative = expr_get_dx_internal(dv->b);
    expr_t *out = index_derivative && expr_is_exact_zero(index_derivative) ? expr_new_const(NUM_ZERO) : NULL;

    expr_free(index_derivative);
    return out;
}

static expr_t *deriv_summation(expr_t *dv)
{
    expr_t *raw_derivative = expr_get_dx_internal(dv->a);
    expr_t *term_derivative = raw_derivative ? expr_simplify(raw_derivative) : NULL;
    expr_t *out = term_derivative ? expr_new_summation(term_derivative, dv->b) : NULL;

    expr_free(raw_derivative);
    expr_free(term_derivative);
    return out;
}

static expr_t *deriv_product(expr_t *dv)
{
    expr_t *raw_derivative = expr_get_dx_internal(dv->a);
    expr_t *factor_derivative = raw_derivative ? expr_simplify(raw_derivative) : NULL;
    expr_t *log_derivative = factor_derivative ? expr_div(factor_derivative, dv->a) : NULL;
    expr_t *sum = log_derivative ? expr_new_summation(log_derivative, dv->b) : NULL;
    expr_t *product = sum ? expr_new_product(dv->a, dv->b) : NULL;
    expr_t *out = product ? expr_mul(product, sum) : NULL;

    expr_free(product);
    expr_free(sum);
    expr_free(log_derivative);
    expr_free(factor_derivative);
    expr_free(raw_derivative);
    return out;
}

static bool expr_same_progression_index(const expr_t *candidate, const expr_t *index)
{
    return candidate == index ||
           (expr_is_var(candidate) && expr_is_var(index) && candidate->var_id != 0u &&
            candidate->var_id == index->var_id);
}

static bool expr_contains_progression_index(const expr_t *expr, const expr_t *index)
{
    if (!expr)
        return false;
    if (expr_same_progression_index(expr, index))
        return true;
    return expr_contains_progression_index(expr->a, index) || expr_contains_progression_index(expr->b, index);
}

static bool expr_finite_progression_parts(const expr_t *expr, const expr_t **upper_out, expr_t **step_out,
                                          const expr_ops_t **function_ops_out)
{
    const expr_t *index;
    const expr_t *bounds;
    const expr_t *lower;
    const expr_t *upper;
    const expr_t *argument;
    const expr_t *left;
    const expr_t *right;
    expr_t *step = NULL;
    number_t lower_value = NUM_NAN;
    bool matched = false;

    if (!expr || !upper_out || !step_out || !function_ops_out || !expr_is_op(expr, &ops_summation) ||
        !expr_is_op(expr->b, &ops_argument_list))
        return false;
    index = expr->b->a;
    bounds = expr->b->b;
    if (!index || !expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list))
        return false;
    lower = bounds->a;
    upper = bounds->b;
    lower_value = expr_eval(lower);
    if (!num_is_exact(lower_value) || !num_is_one(lower_value))
        goto cleanup;
    if (!expr->a || !expr->a->ops || expr->a->ops->arity != EXPR_OP_UNARY)
        goto cleanup;
    argument = expr->a->a;
    if (!expr_match_mul_expr(argument, &left, &right))
        goto cleanup;
    (void)left;
    (void)right;
    step = expr_simplify_extract_common_factor_quotient(argument, index);
    if (!step || expr_contains_progression_index(step, index))
        goto cleanup;

    *upper_out = upper;
    *step_out = step;
    *function_ops_out = expr->a->ops;
    step = NULL;
    matched = true;

cleanup:
    expr_free(step);
    num_destroy(&lower_value);
    return matched;
}

/* Return whether a finite progression reduction relies on the supplied step value. */
bool expr_finite_progression_requires_bound_step(const expr_t *expr)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    bool required = false;

    if (expr_finite_progression_parts(expr, &upper, &step, &function_ops))
        required = function_ops == &ops_floor || function_ops == &ops_ceil || function_ops == &ops_bit_not;
    expr_free(step);
    return required;
}

static number_t eval_finite_exponential_progression(const expr_t *expr, long upper_value)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    number_t step_value;
    number_t q;
    number_t upper_step;
    number_t q_to_upper;
    number_t numerator_factor;
    number_t numerator;
    number_t denominator;
    number_t out;

    if (upper_value < 1L || !expr_finite_progression_parts(expr, &upper, &step, &function_ops) ||
        function_ops != &ops_exp) {
        expr_free(step);
        return num_clone(NUM_NAN);
    }
    (void)upper;
    step_value = expr_eval((expr_t *)step);
    q = num_exp(step_value);
    if (num_eq(q, NUM_ONE)) {
        out = num_create_from_long(upper_value);
        num_destroy(&q);
        num_destroy(&step_value);
        expr_free(step);
        return out;
    }
    upper_step = num_mul_long(step_value, upper_value);
    q_to_upper = num_exp(upper_step);
    numerator_factor = num_sub(q_to_upper, NUM_ONE);
    numerator = num_mul(q, numerator_factor);
    denominator = num_sub(q, NUM_ONE);
    out = num_div(numerator, denominator);
    num_destroy(&denominator);
    num_destroy(&numerator);
    num_destroy(&numerator_factor);
    num_destroy(&q_to_upper);
    num_destroy(&upper_step);
    num_destroy(&q);
    num_destroy(&step_value);
    expr_free(step);
    return out;
}

static number_t eval_finite_zero_atan_progression(const expr_t *expr)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    number_t step_value;
    number_t out;

    if (!expr_finite_progression_parts(expr, &upper, &step, &function_ops) || function_ops != &ops_atan) {
        expr_free(step);
        return num_clone(NUM_NAN);
    }
    (void)upper;
    step_value = expr_eval(step);
    out = num_is_zero(step_value) ? num_clone(NUM_ZERO) : num_clone(NUM_NAN);
    num_destroy(&step_value);
    expr_free(step);
    return out;
}

/* Return the signed infinity contributed by an exact inverse-function pole in a finite progression. */
static number_t eval_finite_inverse_progression_pole(const expr_t *expr, long upper_value)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    number_t step_value;
    number_t step_magnitude;
    number_t pole_index;
    number_t upper_number;
    bool has_pole;
    int step_sign;

    if (upper_value < 1L || !expr_finite_progression_parts(expr, &upper, &step, &function_ops) ||
        (function_ops != &ops_atanh && function_ops != &ops_acoth)) {
        expr_free(step);
        return num_clone(NUM_NAN);
    }
    (void)upper;
    step_value = expr_eval(step);
    expr_free(step);
    if (!num_is_exact(step_value) || !num_is_real(step_value) || !num_is_finite(step_value) ||
        num_is_zero(step_value)) {
        num_destroy(&step_value);
        return num_clone(NUM_NAN);
    }

    step_sign = num_get_sign(step_value);
    step_magnitude = num_abs(step_value);
    pole_index = num_inv(step_magnitude);
    upper_number = num_create_from_long(upper_value);
    has_pole = num_is_exact(pole_index) && num_is_integer(pole_index) && num_gt(pole_index, NUM_ZERO) &&
               num_le(pole_index, upper_number);

    num_destroy(&upper_number);
    num_destroy(&pole_index);
    num_destroy(&step_magnitude);
    num_destroy(&step_value);
    return num_clone(has_pole ? (step_sign < 0 ? NUM_NINF : NUM_INF) : NUM_NAN);
}

/* Evaluate a recognised inverse progression directly when its finite bound is supplied. */
static number_t eval_finite_inverse_progression(const expr_t *expr, long upper_value)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    expr_number_unary_fn evaluate;
    number_t step_value;
    number_t sum = num_clone(NUM_ZERO);

    if (upper_value < 1L || upper_value > 1000000L ||
        !expr_finite_progression_parts(expr, &upper, &step, &function_ops) ||
        !(evaluate = function_ops->finite_progression_term_eval)) {
        expr_free(step);
        num_destroy(&sum);
        return num_clone(NUM_NAN);
    }
    (void)upper;
    step_value = expr_eval(step);
    expr_free(step);
    if (!num_is_finite(step_value)) {
        num_destroy(&step_value);
        num_destroy(&sum);
        return num_clone(NUM_NAN);
    }

    for (long k = 1L; k <= upper_value; ++k) {
        number_t argument = num_mul_long(step_value, k);
        number_t term = evaluate(argument);
        number_t updated = num_add(sum, term);

        num_destroy(&argument);
        num_destroy(&term);
        num_destroy(&sum);
        sum = updated;
    }
    num_destroy(&step_value);
    return sum;
}

/* Evaluate a finite arctangent progression through its real terms to retain the active precision. */
static number_t eval_finite_atan_progression(const expr_t *expr, long upper_value)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;

    if (!expr_finite_progression_parts(expr, &upper, &step, &function_ops) || function_ops != &ops_atan) {
        expr_free(step);
        return num_clone(NUM_NAN);
    }
    (void)upper;
    expr_free(step);
    return eval_finite_inverse_progression(expr, upper_value);
}

static number_t eval_finite_qdigamma_progression(const expr_t *expr, long upper_value)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    number_t step_value;
    number_t sum = num_clone(NUM_ZERO);

    if (upper_value < 1L || upper_value > 1000000L ||
        !expr_finite_progression_parts(expr, &upper, &step, &function_ops) ||
        (function_ops != &ops_tan && function_ops != &ops_cot && function_ops != &ops_sec &&
         function_ops != &ops_cosec && function_ops != &ops_sech)) {
        expr_free(step);
        num_destroy(&sum);
        return num_clone(NUM_NAN);
    }
    (void)upper;
    step_value = expr_eval((expr_t *)step);
    if (!num_is_finite(step_value)) {
        expr_free(step);
        num_destroy(&step_value);
        num_destroy(&sum);
        return num_clone(NUM_NAN);
    }
    for (long k = 1L; k <= upper_value; ++k) {
        number_t angle = num_mul_long(step_value, k);
        number_t term;
        number_t next;

        if (function_ops == &ops_tan)
            term = num_tan(angle);
        else if (function_ops == &ops_cot)
            term = num_cot(angle);
        else if (function_ops == &ops_sec)
            term = num_sec(angle);
        else if (function_ops == &ops_cosec)
            term = num_cosec(angle);
        else if (function_ops == &ops_tanh)
            term = num_tanh(angle);
        else if (function_ops == &ops_coth)
            term = num_coth(angle);
        else if (function_ops == &ops_sech)
            term = num_sech(angle);
        else if (function_ops == &ops_cosech)
            term = num_cosech(angle);
        else
            term = num_clone(NUM_NAN);
        next = num_add(sum, term);

        num_destroy(&angle);
        num_destroy(&term);
        num_destroy(&sum);
        sum = next;
        if (!num_is_finite(sum))
            break;
    }
    expr_free(step);
    num_destroy(&step_value);
    return sum;
}

static bool expr_finite_weighted_hyperbolic_parts(const expr_t *expr, const expr_t **upper_out,
                                                   const expr_t **step_out,
                                                   expr_finite_weighted_hyperbolic_kind_t *kind_out)
{
    const expr_t *index;
    const expr_t *bounds;
    const expr_t *lower;
    const expr_t *upper;
    const expr_t *numerator;
    const expr_t *denominator;
    const expr_t *left;
    const expr_t *right;
    number_t lower_value = NUM_NAN;
    expr_finite_weighted_hyperbolic_kind_t kind = EXPR_FINITE_WEIGHTED_HYPERBOLIC_NONE;
    bool matched = false;

    if (!expr || !upper_out || !step_out || !kind_out || !expr_is_op(expr, &ops_summation) ||
        !expr_is_op(expr->b, &ops_argument_list))
        return false;
    index = expr->b->a;
    bounds = expr->b->b;
    if (!index || !expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list))
        return false;
    lower = bounds->a;
    upper = bounds->b;
    lower_value = expr_eval(lower);
    if (!num_is_exact(lower_value) || !num_is_one(lower_value) ||
        !expr_is_op(expr->a, &ops_div) || !(numerator = expr->a->a) || !(denominator = expr->a->b) ||
        !(denominator == index || (expr_is_var(denominator) && denominator->var_id == index->var_id)))
        goto cleanup;
    if (expr_is_op(numerator, &ops_sinh))
        kind = EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH;
    else if (expr_is_op(numerator, &ops_cosh))
        kind = EXPR_FINITE_WEIGHTED_HYPERBOLIC_COSH;
    else
        goto cleanup;
    if (!expr_match_mul_expr(numerator->a, &left, &right))
        goto cleanup;
    if (left == index)
        *step_out = right;
    else if (right == index)
        *step_out = left;
    else
        goto cleanup;
    *upper_out = upper;
    *kind_out = kind;
    matched = true;

cleanup:
    num_destroy(&lower_value);
    return matched;
}

static bool expr_finite_weighted_circular_parts(const expr_t *expr, const expr_t **upper_out,
                                                const expr_t **step_out,
                                                expr_finite_weighted_circular_kind_t *kind_out)
{
    const expr_t *index;
    const expr_t *bounds;
    const expr_t *lower;
    const expr_t *upper;
    const expr_t *numerator;
    const expr_t *denominator;
    const expr_t *left;
    const expr_t *right;
    number_t lower_value = NUM_NAN;
    expr_finite_weighted_circular_kind_t kind = EXPR_FINITE_WEIGHTED_CIRCULAR_NONE;
    bool matched = false;

    if (!expr || !upper_out || !step_out || !kind_out || !expr_is_op(expr, &ops_summation) ||
        !expr_is_op(expr->b, &ops_argument_list))
        return false;
    index = expr->b->a;
    bounds = expr->b->b;
    if (!index || !expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list))
        return false;
    lower = bounds->a;
    upper = bounds->b;
    lower_value = expr_eval(lower);
    if (!num_is_exact(lower_value) || !num_is_one(lower_value) || !expr_is_op(expr->a, &ops_div) ||
        !(numerator = expr->a->a) || !(denominator = expr->a->b) ||
        !(denominator == index || (expr_is_var(denominator) && denominator->var_id == index->var_id)))
        goto cleanup;
    if (expr_is_op(numerator, &ops_sin))
        kind = EXPR_FINITE_WEIGHTED_CIRCULAR_SIN;
    else if (expr_is_op(numerator, &ops_cos))
        kind = EXPR_FINITE_WEIGHTED_CIRCULAR_COS;
    else
        goto cleanup;
    if (!expr_match_mul_expr(numerator->a, &left, &right))
        goto cleanup;
    if (left == index || (expr_is_var(left) && left->var_id == index->var_id))
        *step_out = right;
    else if (right == index || (expr_is_var(right) && right->var_id == index->var_id))
        *step_out = left;
    else
        goto cleanup;
    *upper_out = upper;
    *kind_out = kind;
    matched = true;

cleanup:
    num_destroy(&lower_value);
    return matched;
}

static number_t eval_finite_weighted_hyperbolic(expr_t *dv, long upper_value)
{
    const expr_t *upper;
    const expr_t *step_expr;
    number_t step;
    number_t magnitude;
    number_t q;
    number_t q_power;
    number_t dominant_scaled;
    number_t decaying;
    number_t dominant;
    number_t result;
    double step_double;
    expr_finite_weighted_hyperbolic_kind_t kind;
    const long maximum_tail_terms = 10000L;

    if (!expr_finite_weighted_hyperbolic_parts(dv, &upper, &step_expr, &kind) || upper_value < 1L)
        return num_clone(NUM_NAN);
    (void)upper;
    step = expr_eval(step_expr);
    if (!num_is_real(step) || !num_is_finite(step) || num_is_zero(step)) {
        bool zero = num_is_zero(step);

        num_destroy(&step);
        if (!zero)
            return num_clone(NUM_NAN);
        if (kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH)
            return num_clone(NUM_ZERO);
        {
            number_t upper_number = num_create_from_long(upper_value);
            number_t shifted_upper = num_add(upper_number, NUM_ONE);
            number_t digamma = num_digamma(shifted_upper);
            number_t harmonic = num_add(digamma, NUM_EULER_MASCHERONI);

            num_destroy(&digamma);
            num_destroy(&shifted_upper);
            num_destroy(&upper_number);
            return harmonic;
        }
    }
    step_double = num_to_double(step);
    magnitude = step_double < 0.0 ? num_neg(step) : num_clone(step);
    {
        number_t negative_magnitude = num_neg(magnitude);

        q = num_exp(negative_magnitude);
        num_destroy(&negative_magnitude);
    }
    q_power = num_clone(NUM_ONE);
    dominant_scaled = num_clone(NUM_ZERO);
    decaying = num_clone(NUM_ZERO);
    for (long offset = 0L; offset < upper_value && offset < maximum_tail_terms; ++offset) {
        number_t denominator = num_create_from_long(upper_value - offset);
        number_t term = num_div(q_power, denominator);
        number_t updated = num_add(dominant_scaled, term);
        number_t next_power = num_mul(q_power, q);
        number_t term_abs = num_abs(term);
        number_t updated_abs = num_abs(updated);
        double term_size = num_to_double(term_abs);
        double total_size = num_to_double(updated_abs);

        num_destroy(&updated_abs);
        num_destroy(&term_abs);
        num_destroy(&dominant_scaled);
        num_destroy(&q_power);
        num_destroy(&term);
        num_destroy(&denominator);
        dominant_scaled = updated;
        q_power = next_power;
        if (offset > 8L && term_size < 1e-30 * (1.0 + total_size))
            break;
    }
    num_destroy(&q_power);
    q_power = num_clone(q);
    for (long k = 1L; k <= upper_value && k <= maximum_tail_terms; ++k) {
        number_t denominator = num_create_from_long(k);
        number_t term = num_div(q_power, denominator);
        number_t updated = num_add(decaying, term);
        number_t next_power = num_mul(q_power, q);
        number_t term_abs = num_abs(term);
        number_t updated_abs = num_abs(updated);
        double term_size = num_to_double(term_abs);
        double total_size = num_to_double(updated_abs);

        num_destroy(&updated_abs);
        num_destroy(&term_abs);
        num_destroy(&decaying);
        num_destroy(&q_power);
        num_destroy(&term);
        num_destroy(&denominator);
        decaying = updated;
        q_power = next_power;
        if (k > 8L && term_size < 1e-30 * (1.0 + total_size))
            break;
    }
    {
        number_t scaled_exponent = num_mul_long(magnitude, upper_value);
        number_t dominant_exponential = num_exp(scaled_exponent);
        number_t combined;

        dominant = num_mul(dominant_exponential, dominant_scaled);
        combined = kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH ? num_sub(dominant, decaying)
                                                                : num_add(dominant, decaying);
        result = num_div(combined, NUM_TWO);
        num_destroy(&combined);
        num_destroy(&dominant_exponential);
        num_destroy(&scaled_exponent);
    }
    if (kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH && step_double < 0.0) {
        number_t negative = num_neg(result);

        num_destroy(&result);
        result = negative;
    }
    num_destroy(&dominant);
    num_destroy(&decaying);
    num_destroy(&dominant_scaled);
    num_destroy(&q_power);
    num_destroy(&q);
    num_destroy(&magnitude);
    num_destroy(&step);
    return result;
}

static expr_t *expr_finite_weighted_hyperbolic_lerch_form(
    const expr_t *expr, expr_finite_weighted_hyperbolic_kind_t required_kind)
{
    const expr_t *n;
    const expr_t *x;
    expr_t *one = NULL;
    expr_t *n_plus_one = NULL;
    expr_t *negative_x = NULL;
    expr_t *positive_z = NULL;
    expr_t *negative_z = NULL;
    expr_t *positive_polylog = NULL;
    expr_t *negative_polylog = NULL;
    expr_t *positive_power = NULL;
    expr_t *negative_power = NULL;
    expr_t *positive_phi = NULL;
    expr_t *negative_phi = NULL;
    expr_t *positive_tail = NULL;
    expr_t *negative_tail = NULL;
    expr_t *positive_polylog_half = NULL;
    expr_t *negative_polylog_half = NULL;
    expr_t *positive_tail_half = NULL;
    expr_t *negative_tail_half = NULL;
    expr_t *left = NULL;
    expr_t *left_without_negative_tail = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;
    expr_finite_weighted_hyperbolic_kind_t kind;

    if (!expr_finite_weighted_hyperbolic_parts(expr, &n, &x, &kind) || kind != required_kind)
        return NULL;
    one = expr_new_const(NUM_ONE);
    n_plus_one = expr_add_long(n, 1L);
    negative_x = expr_neg(x);
    positive_z = expr_exp(x);
    negative_z = expr_exp(negative_x);
    positive_polylog = expr_polylog1(positive_z);
    negative_polylog = expr_polylog1(negative_z);
    positive_power = expr_pow_xp(positive_z, n_plus_one);
    negative_power = expr_pow_xp(negative_z, n_plus_one);
    positive_phi = expr_lerch_phi(positive_z, one, n_plus_one);
    negative_phi = expr_lerch_phi(negative_z, one, n_plus_one);
    positive_tail = expr_mul(positive_power, positive_phi);
    negative_tail = expr_mul(negative_power, negative_phi);
    positive_polylog_half = expr_mul_num(positive_polylog, &NUM_HALF);
    negative_polylog_half = expr_mul_num(negative_polylog, &NUM_HALF);
    positive_tail_half = expr_mul_num(positive_tail, &NUM_HALF);
    negative_tail_half = expr_mul_num(negative_tail, &NUM_HALF);
    left = kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH
               ? expr_sub(positive_polylog_half, negative_polylog_half)
               : expr_add(positive_polylog_half, negative_polylog_half);
    right = kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH ? expr_sub(negative_tail_half, positive_tail_half) : NULL;
    left_without_negative_tail =
        kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_COSH ? expr_sub(left, negative_tail_half) : NULL;
    out = kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH ? expr_add(left, right)
                                                        : expr_sub(left_without_negative_tail, positive_tail_half);
    expr_free(right);
    expr_free(left_without_negative_tail);
    expr_free(left);
    expr_free(negative_tail_half);
    expr_free(positive_tail_half);
    expr_free(negative_polylog_half);
    expr_free(positive_polylog_half);
    expr_free(negative_tail);
    expr_free(positive_tail);
    expr_free(negative_phi);
    expr_free(positive_phi);
    expr_free(negative_power);
    expr_free(positive_power);
    expr_free(negative_polylog);
    expr_free(positive_polylog);
    expr_free(negative_z);
    expr_free(positive_z);
    expr_free(negative_x);
    expr_free(n_plus_one);
    expr_free(one);
    return out;
}

/* Return the Li1/Lerch-Phi form of a recognised finite weighted sinh sum. */
expr_t *expr_finite_weighted_sinh_lerch_form(const expr_t *expr)
{
    return expr_finite_weighted_hyperbolic_lerch_form(expr, EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH);
}

/* Return the Li1/Lerch-Phi form of a recognised finite weighted cosh sum. */
expr_t *expr_finite_weighted_cosh_lerch_form(const expr_t *expr)
{
    return expr_finite_weighted_hyperbolic_lerch_form(expr, EXPR_FINITE_WEIGHTED_HYPERBOLIC_COSH);
}

static expr_t *expr_finite_weighted_circular_lerch_form(
    const expr_t *expr, expr_finite_weighted_circular_kind_t required_kind)
{
    const expr_t *n;
    const expr_t *x;
    expr_t *one = NULL;
    expr_t *n_plus_one = NULL;
    expr_t *imaginary = NULL;
    expr_t *imaginary_x = NULL;
    expr_t *negative_imaginary_x = NULL;
    expr_t *positive_z = NULL;
    expr_t *negative_z = NULL;
    expr_t *positive_polylog = NULL;
    expr_t *negative_polylog = NULL;
    expr_t *positive_power = NULL;
    expr_t *negative_power = NULL;
    expr_t *positive_phi = NULL;
    expr_t *negative_phi = NULL;
    expr_t *positive_tail = NULL;
    expr_t *negative_tail = NULL;
    expr_t *positive_series = NULL;
    expr_t *negative_series = NULL;
    expr_t *numerator = NULL;
    expr_t *denominator = NULL;
    expr_t *out = NULL;
    expr_finite_weighted_circular_kind_t kind;

    if (!expr_finite_weighted_circular_parts(expr, &n, &x, &kind) || kind != required_kind)
        return NULL;
    one = expr_new_const(NUM_ONE);
    n_plus_one = expr_add_long(n, 1L);
    imaginary = expr_new_const(NUM_I);
    imaginary_x = imaginary ? expr_mul(imaginary, x) : NULL;
    negative_imaginary_x = imaginary_x ? expr_neg(imaginary_x) : NULL;
    positive_z = imaginary_x ? expr_exp(imaginary_x) : NULL;
    negative_z = negative_imaginary_x ? expr_exp(negative_imaginary_x) : NULL;
    positive_polylog = positive_z ? expr_polylog1(positive_z) : NULL;
    negative_polylog = negative_z ? expr_polylog1(negative_z) : NULL;
    positive_power = positive_z && n_plus_one ? expr_pow_xp(positive_z, n_plus_one) : NULL;
    negative_power = negative_z && n_plus_one ? expr_pow_xp(negative_z, n_plus_one) : NULL;
    positive_phi = positive_z && one && n_plus_one ? expr_lerch_phi(positive_z, one, n_plus_one) : NULL;
    negative_phi = negative_z && one && n_plus_one ? expr_lerch_phi(negative_z, one, n_plus_one) : NULL;
    positive_tail = positive_power && positive_phi ? expr_mul(positive_power, positive_phi) : NULL;
    negative_tail = negative_power && negative_phi ? expr_mul(negative_power, negative_phi) : NULL;
    positive_series = positive_polylog && positive_tail ? expr_sub(positive_polylog, positive_tail) : NULL;
    negative_series = negative_polylog && negative_tail ? expr_sub(negative_polylog, negative_tail) : NULL;
    numerator = positive_series && negative_series
                    ? (kind == EXPR_FINITE_WEIGHTED_CIRCULAR_SIN ? expr_sub(positive_series, negative_series)
                                                                 : expr_add(positive_series, negative_series))
                    : NULL;
    denominator = kind == EXPR_FINITE_WEIGHTED_CIRCULAR_SIN && imaginary ? expr_mul_num(imaginary, &NUM_TWO) : NULL;
    out = numerator ? (kind == EXPR_FINITE_WEIGHTED_CIRCULAR_SIN ? expr_div(numerator, denominator)
                                                                  : expr_div_long(numerator, 2L))
                    : NULL;
    expr_free(denominator);
    expr_free(numerator);
    expr_free(negative_series);
    expr_free(positive_series);
    expr_free(negative_tail);
    expr_free(positive_tail);
    expr_free(negative_phi);
    expr_free(positive_phi);
    expr_free(negative_power);
    expr_free(positive_power);
    expr_free(negative_polylog);
    expr_free(positive_polylog);
    expr_free(negative_z);
    expr_free(positive_z);
    expr_free(negative_imaginary_x);
    expr_free(imaginary_x);
    expr_free(imaginary);
    expr_free(n_plus_one);
    expr_free(one);
    return out;
}

static expr_t *expr_finite_weighted_signed_circular_lerch_form(
    const expr_t *expr, expr_finite_weighted_circular_kind_t kind)
{
    const expr_t *sum = expr;
    expr_t *form;
    expr_t *out;
    bool negative = expr_is_op(expr, &ops_neg) && expr->a;

    if (negative)
        sum = expr->a;

    form = expr_finite_weighted_circular_lerch_form(sum, kind);
    if (!form || !negative)
        return form;
    if (kind == EXPR_FINITE_WEIGHTED_CIRCULAR_COS && expr_is_op(form, &ops_div) && expr_is_const(form->b) &&
        num_eq(form->b->c, NUM_TWO)) {
        number_t negative_half = num_neg(NUM_HALF);

        out = expr_mul_num(form->a, &negative_half);
        num_destroy(&negative_half);
    } else {
        out = expr_neg(form);
    }
    expr_free(form);
    return out;
}

/* Return the Li1/Lerch-Phi form of a recognised finite weighted sine sum. */
expr_t *expr_finite_weighted_sin_lerch_form(const expr_t *expr)
{
    expr_t *form = expr_finite_weighted_signed_circular_lerch_form(expr, EXPR_FINITE_WEIGHTED_CIRCULAR_SIN);
    expr_t *sum;

    if (form)
        return form;
    sum = expr_finite_weighted_circular_from_lerch_form(expr, EXPR_FINITE_WEIGHTED_CIRCULAR_SIN);
    form = sum ? expr_finite_weighted_signed_circular_lerch_form(sum, EXPR_FINITE_WEIGHTED_CIRCULAR_SIN) : NULL;
    expr_free(sum);
    return form;
}

/* Return the Li1/Lerch-Phi form of a recognised finite weighted cosine sum. */
expr_t *expr_finite_weighted_cos_lerch_form(const expr_t *expr)
{
    expr_t *form = expr_finite_weighted_signed_circular_lerch_form(expr, EXPR_FINITE_WEIGHTED_CIRCULAR_COS);
    expr_t *sum;

    if (form)
        return form;
    sum = expr_finite_weighted_circular_from_lerch_form(expr, EXPR_FINITE_WEIGHTED_CIRCULAR_COS);
    form = sum ? expr_finite_weighted_signed_circular_lerch_form(sum, EXPR_FINITE_WEIGHTED_CIRCULAR_COS) : NULL;
    expr_free(sum);
    return form;
}

static expr_t *expr_finite_weighted_hyperbolic_from_lerch_form(
    const expr_t *expr, expr_finite_weighted_hyperbolic_kind_t kind)
{
    const expr_t *phi = expr_find_lerch_phi(expr);
    const expr_t *z;
    const expr_t *s;
    const expr_t *a;
    const expr_t *x;
    expr_t *n = NULL;
    expr_t *candidate = NULL;
    expr_t *recognised_sum = NULL;
    expr_t *difference = NULL;
    expr_t *simplified = NULL;
    number_t s_value = NUM_NAN;
    expr_t *out = NULL;

    if (!expr || !phi || !expr_lerch_phi_unpack(phi, &z, &s, &a) ||
        !expr_is_op(z, &ops_exp) || !(x = z->a))
        return NULL;
    if (expr_is_op(x, &ops_neg) && x->a)
        x = x->a;
    s_value = expr_eval(s);
    if (!num_is_exact(s_value) || !num_is_one(s_value))
        goto cleanup;
    {
        expr_t *one = expr_new_const(NUM_ONE);
        expr_t *raw_n = one ? expr_sub(a, one) : NULL;

        n = raw_n ? expr_simplify(raw_n) : NULL;
        expr_free(raw_n);
        expr_free(one);
    }
    if (!n)
        goto cleanup;
    {
        expr_t *index = expr_new_var(NUM_NAN);
        expr_t *argument;
        expr_t *numerator;
        expr_t *term;
        expr_t *lower;
        expr_t *sum;

        if (index)
            expr_set_name(index, "k");
        argument = index ? expr_mul(index, x) : NULL;
        numerator = argument ? (kind == EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH ? expr_sinh(argument)
                                                                                 : expr_cosh(argument))
                             : NULL;
        term = numerator ? expr_div(numerator, index) : NULL;
        lower = expr_new_const(NUM_ONE);
        sum = term && lower ? expr_new_finite_summation_range(term, index, lower, n) : NULL;

        candidate = sum ? expr_finite_weighted_hyperbolic_lerch_form(sum, kind) : NULL;
        recognised_sum = sum;
        expr_free(lower);
        expr_free(term);
        expr_free(numerator);
        expr_free(argument);
        expr_free(index);
    }
    difference = candidate ? expr_sub(expr, candidate) : NULL;
    simplified = difference ? expr_simplify(difference) : NULL;
    if (!simplified || !expr_is_exact_zero(simplified))
        goto cleanup;
    out = recognised_sum;
    recognised_sum = NULL;

cleanup:
    num_destroy(&s_value);
    expr_free(simplified);
    expr_free(difference);
    expr_free(candidate);
    expr_free(recognised_sum);
    expr_free(n);
    return out;
}

/* Recover the finite weighted sinh sum represented by a matching Lerch-Phi expression. */
expr_t *expr_finite_weighted_sinh_from_lerch_form(const expr_t *expr)
{
    return expr_finite_weighted_hyperbolic_from_lerch_form(expr, EXPR_FINITE_WEIGHTED_HYPERBOLIC_SINH);
}

/* Recover the finite weighted cosh sum represented by a matching Lerch-Phi expression. */
expr_t *expr_finite_weighted_cosh_from_lerch_form(const expr_t *expr)
{
    return expr_finite_weighted_hyperbolic_from_lerch_form(expr, EXPR_FINITE_WEIGHTED_HYPERBOLIC_COSH);
}

/* Return whether an expression is the recognised finite weighted hyperbolic sum in Lerch-Phi form. */
bool expr_is_finite_weighted_sinh_lerch_form(const expr_t *expr)
{
    expr_t *sum = expr_finite_weighted_sinh_from_lerch_form(expr);
    bool matched = sum != NULL;

    expr_free(sum);
    return matched;
}

/* Return whether an expression is the recognised finite weighted cosh sum in Li1/Lerch-Phi form. */
bool expr_is_finite_weighted_cosh_lerch_form(const expr_t *expr)
{
    expr_t *sum = expr_finite_weighted_cosh_from_lerch_form(expr);
    bool matched = sum != NULL;

    expr_free(sum);
    return matched;
}

/* Evaluate a recognised finite weighted hyperbolic sum in Lerch-Phi form. */
bool expr_finite_weighted_sinh_lerch_value(const expr_t *expr, number_t *value_out)
{
    const expr_t *upper;
    const expr_t *step;
    expr_t *sum;
    long upper_value;
    expr_finite_weighted_hyperbolic_kind_t kind;

    if (!value_out || !(sum = expr_finite_weighted_sinh_from_lerch_form(expr)))
        return false;

    if (!expr_finite_weighted_hyperbolic_parts(sum, &upper, &step, &kind) ||
        !expr_summation_bound_to_long(upper, &upper_value) || upper_value < 1L) {
        expr_free(sum);
        return false;
    }
    (void)step;
    *value_out = eval_finite_weighted_hyperbolic(sum, upper_value);
    expr_free(sum);
    return num_is_finite(*value_out);
}

/* Evaluate a recognised finite weighted cosh sum in Li1/Lerch-Phi form. */
bool expr_finite_weighted_cosh_lerch_value(const expr_t *expr, number_t *value_out)
{
    const expr_t *upper;
    const expr_t *step;
    expr_t *sum;
    long upper_value;
    expr_finite_weighted_hyperbolic_kind_t kind;

    if (!value_out || !(sum = expr_finite_weighted_cosh_from_lerch_form(expr)))
        return false;
    if (!expr_finite_weighted_hyperbolic_parts(sum, &upper, &step, &kind) ||
        !expr_summation_bound_to_long(upper, &upper_value) || upper_value < 1L) {
        expr_free(sum);
        return false;
    }
    (void)step;
    *value_out = eval_finite_weighted_hyperbolic(sum, upper_value);
    expr_free(sum);
    return num_is_finite(*value_out);
}

static number_t eval_finite_weighted_circular(const expr_t *step_expr, long upper_value,
                                              expr_finite_weighted_circular_kind_t kind)
{
    number_t step = expr_eval(step_expr);
    number_t sin_step = NUM_NAN;
    number_t cos_step = NUM_NAN;
    number_t sin_k;
    number_t cos_k;
    number_t sum = num_clone(NUM_ZERO);

    if (!num_is_real(step) || !num_is_finite(step) || num_sincos(step, &sin_step, &cos_step) != 0) {
        num_destroy(&cos_step);
        num_destroy(&sin_step);
        num_destroy(&step);
        num_destroy(&sum);
        return num_clone(NUM_NAN);
    }
    sin_k = num_clone(sin_step);
    cos_k = num_clone(cos_step);
    for (long k = 1L; k <= upper_value; ++k) {
        number_t denominator = num_create_from_long(k);
        number_t term = num_div(kind == EXPR_FINITE_WEIGHTED_CIRCULAR_SIN ? sin_k : cos_k, denominator);
        number_t updated = num_add(sum, term);

        num_destroy(&sum);
        num_destroy(&term);
        num_destroy(&denominator);
        sum = updated;
        if (k == upper_value || k == LONG_MAX)
            break;
        if ((k & 1023L) == 0L) {
            number_t argument = num_mul_long(step, k + 1L);
            number_t next_sin = NUM_NAN;
            number_t next_cos = NUM_NAN;

            if (num_sincos(argument, &next_sin, &next_cos) != 0) {
                num_destroy(&next_cos);
                num_destroy(&next_sin);
                num_destroy(&argument);
                num_destroy(&sum);
                sum = num_clone(NUM_NAN);
                break;
            }
            num_destroy(&argument);
            num_destroy(&cos_k);
            num_destroy(&sin_k);
            sin_k = next_sin;
            cos_k = next_cos;
        } else {
            number_t sin_cos = num_mul(sin_k, cos_step);
            number_t cos_sin = num_mul(cos_k, sin_step);
            number_t cos_cos = num_mul(cos_k, cos_step);
            number_t sin_sin = num_mul(sin_k, sin_step);
            number_t next_sin = num_add(sin_cos, cos_sin);
            number_t next_cos = num_sub(cos_cos, sin_sin);

            num_destroy(&sin_sin);
            num_destroy(&cos_cos);
            num_destroy(&cos_sin);
            num_destroy(&sin_cos);
            num_destroy(&cos_k);
            num_destroy(&sin_k);
            sin_k = next_sin;
            cos_k = next_cos;
        }
    }
    num_destroy(&cos_k);
    num_destroy(&sin_k);
    num_destroy(&cos_step);
    num_destroy(&sin_step);
    num_destroy(&step);
    return sum;
}

static const expr_t *expr_circular_step_from_unit_exponential(const expr_t *z)
{
    const expr_t *left;
    const expr_t *right;

    if (!z)
        return NULL;
    if (expr_is_op(z, &ops_exp) && z->a && expr_match_mul_expr(z->a, &left, &right)) {
        if (expr_is_const(left) && num_eq(left->c, NUM_I))
            return right;
        if (expr_is_const(right) && num_eq(right->c, NUM_I))
            return left;
    }
    if (expr_is_op(z, &ops_add) && expr_is_op(z->a, &ops_cos) && z->a->a &&
        expr_match_mul_expr(z->b, &left, &right)) {
        const expr_t *sine = expr_is_op(left, &ops_sin) ? left : expr_is_op(right, &ops_sin) ? right : NULL;
        const expr_t *imaginary = sine == left ? right : sine == right ? left : NULL;

        if (sine && sine->a && imaginary && expr_is_const(imaginary) && num_eq(imaginary->c, NUM_I) &&
            expr_struct_eq(z->a->a, sine->a))
            return z->a->a;
    }
    return NULL;
}

static bool expr_first_polylog1_coefficient(const expr_t *expr, number_t coefficient, number_t *coefficient_out)
{
    if (!expr || !coefficient_out)
        return false;
    if (expr_is_op(expr, &ops_polylog1)) {
        *coefficient_out = num_clone(coefficient);
        return true;
    }
    if (expr_is_op(expr, &ops_neg) && expr->a) {
        number_t negative = num_neg(coefficient);
        bool found = expr_first_polylog1_coefficient(expr->a, negative, coefficient_out);

        num_destroy(&negative);
        return found;
    }
    if ((expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_sub)) && expr->a && expr->b) {
        if (expr_first_polylog1_coefficient(expr->a, coefficient, coefficient_out))
            return true;
        if (expr_is_op(expr, &ops_sub)) {
            number_t negative = num_neg(coefficient);
            bool found = expr_first_polylog1_coefficient(expr->b, negative, coefficient_out);

            num_destroy(&negative);
            return found;
        }
        return expr_first_polylog1_coefficient(expr->b, coefficient, coefficient_out);
    }
    if (expr_is_op(expr, &ops_mul) && expr->a && expr->b) {
        if (expr_is_const(expr->a)) {
            number_t scaled = num_mul(coefficient, expr->a->c);
            bool found = expr_first_polylog1_coefficient(expr->b, scaled, coefficient_out);

            num_destroy(&scaled);
            return found;
        }
        if (expr_is_const(expr->b)) {
            number_t scaled = num_mul(coefficient, expr->b->c);
            bool found = expr_first_polylog1_coefficient(expr->a, scaled, coefficient_out);

            num_destroy(&scaled);
            return found;
        }
    }
    if (expr_is_op(expr, &ops_div) && expr->a && expr_is_const(expr->b)) {
        number_t scaled = num_div(coefficient, expr->b->c);
        bool found = expr_first_polylog1_coefficient(expr->a, scaled, coefficient_out);

        num_destroy(&scaled);
        return found;
    }
    return false;
}

static expr_t *expr_finite_weighted_circular_from_lerch_form(
    const expr_t *expr, expr_finite_weighted_circular_kind_t kind)
{
    const expr_t *base = expr;
    const expr_t *phi;
    const expr_t *z;
    const expr_t *s;
    const expr_t *a;
    const expr_t *x;
    expr_t *n = NULL;
    expr_t *candidate = NULL;
    expr_t *normalised_candidate = NULL;
    expr_t *sum = NULL;
    expr_t *difference = NULL;
    expr_t *simplified = NULL;
    expr_t *out = NULL;
    number_t s_value = NUM_NAN;
    number_t polylog_coefficient = NUM_NAN;
    bool negative = expr_is_op(expr, &ops_neg) && expr->a;

    if (negative)
        base = expr->a;
    phi = expr_find_lerch_phi(base);
    if (!phi || !expr_lerch_phi_unpack(phi, &z, &s, &a) || !(x = expr_circular_step_from_unit_exponential(z)))
        return NULL;
    s_value = expr_eval(s);
    if (!num_is_exact(s_value) || !num_is_one(s_value))
        goto cleanup;
    {
        expr_t *one = expr_new_const(NUM_ONE);
        expr_t *raw_n = one ? expr_sub(a, one) : NULL;

        n = raw_n ? expr_simplify(raw_n) : NULL;
        expr_free(raw_n);
        expr_free(one);
    }
    if (!n)
        goto cleanup;
    {
        expr_t *index = expr_new_var(NUM_NAN);
        expr_t *argument;
        expr_t *numerator;
        expr_t *term;
        expr_t *lower;

        if (index)
            expr_set_name(index, "k");
        argument = index ? expr_mul(index, x) : NULL;
        numerator = argument ? (kind == EXPR_FINITE_WEIGHTED_CIRCULAR_SIN ? expr_sin(argument) : expr_cos(argument))
                             : NULL;
        term = numerator ? expr_div(numerator, index) : NULL;
        lower = expr_new_const(NUM_ONE);
        sum = term && lower ? expr_new_finite_summation_range(term, index, lower, n) : NULL;
        candidate = sum ? expr_finite_weighted_circular_lerch_form(sum, kind) : NULL;
        expr_free(lower);
        expr_free(term);
        expr_free(numerator);
        expr_free(argument);
        expr_free(index);
    }
    normalised_candidate = candidate ? expr_simplify(candidate) : NULL;
    difference = normalised_candidate ? expr_sub(base, normalised_candidate) : NULL;
    simplified = difference ? expr_simplify(difference) : NULL;
    if (!simplified || !expr_is_exact_zero(simplified)) {
        expr_free(simplified);
        expr_free(difference);
        difference = normalised_candidate ? expr_add(base, normalised_candidate) : NULL;
        simplified = difference ? expr_simplify(difference) : NULL;
        if (simplified && expr_is_exact_zero(simplified)) {
            negative = !negative;
        } else {
            number_t coefficient_real;
            number_t coefficient_imaginary;
            bool coefficient_matches;

            if (!expr_first_polylog1_coefficient(base, NUM_ONE, &polylog_coefficient))
                goto cleanup;
            coefficient_real = num_real_part(polylog_coefficient);
            coefficient_imaginary = num_imag_part(polylog_coefficient);
            coefficient_matches = kind == EXPR_FINITE_WEIGHTED_CIRCULAR_COS
                                      ? num_is_zero(coefficient_imaginary) && !num_is_zero(coefficient_real)
                                      : num_is_zero(coefficient_real) && !num_is_zero(coefficient_imaginary);
            if (coefficient_matches)
                negative = kind == EXPR_FINITE_WEIGHTED_CIRCULAR_COS
                               ? num_to_double(coefficient_real) < 0.0
                               : num_to_double(coefficient_imaginary) > 0.0;
            num_destroy(&coefficient_imaginary);
            num_destroy(&coefficient_real);
            if (!coefficient_matches)
                goto cleanup;
        }
    }
    out = negative ? expr_neg(sum) : expr_clone(sum);

cleanup:
    num_destroy(&s_value);
    num_destroy(&polylog_coefficient);
    expr_free(simplified);
    expr_free(difference);
    expr_free(normalised_candidate);
    expr_free(candidate);
    expr_free(sum);
    expr_free(n);
    return out;
}

static bool expr_finite_weighted_circular_value(const expr_t *expr, expr_finite_weighted_circular_kind_t required_kind,
                                                number_t *value_out)
{
    const expr_t *sum = expr;
    const expr_t *upper;
    const expr_t *step;
    long upper_value;
    expr_finite_weighted_circular_kind_t kind;
    bool negative;
    expr_t *recovered = NULL;

    if (!expr || !value_out)
        return false;
    negative = expr_is_op(expr, &ops_neg) && expr->a;
    if (negative)
        sum = expr->a;
    if (!expr_finite_weighted_circular_parts(sum, &upper, &step, &kind)) {
        recovered = expr_finite_weighted_circular_from_lerch_form(expr, required_kind);
        sum = recovered;
        negative = sum && expr_is_op(sum, &ops_neg) && sum->a;
        if (negative)
            sum = sum->a;
    }
    if (!sum || !expr_finite_weighted_circular_parts(sum, &upper, &step, &kind) || kind != required_kind ||
        !expr_summation_bound_to_long(upper, &upper_value) || upper_value < 1L || upper_value > 1000000L)
        goto failure;
    *value_out = eval_finite_weighted_circular(step, upper_value, kind);
    if (negative && num_is_finite(*value_out)) {
        number_t negated = num_neg(*value_out);

        num_destroy(value_out);
        *value_out = negated;
    }
    expr_free(recovered);
    return num_is_finite(*value_out);

failure:
    expr_free(recovered);
    return false;
}

/* Evaluate a recognised finite weighted sine sum. */
bool expr_finite_weighted_sin_lerch_value(const expr_t *expr, number_t *value_out)
{
    return expr_finite_weighted_circular_value(expr, EXPR_FINITE_WEIGHTED_CIRCULAR_SIN, value_out);
}

/* Evaluate a recognised finite weighted cosine sum. */
bool expr_finite_weighted_cos_lerch_value(const expr_t *expr, number_t *value_out)
{
    return expr_finite_weighted_circular_value(expr, EXPR_FINITE_WEIGHTED_CIRCULAR_COS, value_out);
}

static expr_t *expr_finite_tangent_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *imaginary_unit = expr_new_named_const(NUM_I, "i");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *exponent = imaginary_unit ? expr_mul(imaginary_unit, step) : NULL;
    expr_t *double_exponent = exponent ? expr_mul_long(exponent, 2L) : NULL;
    expr_t *q = double_exponent ? expr_exp(double_exponent) : NULL;
    expr_t *quadruple_exponent = exponent ? expr_mul_long(exponent, 4L) : NULL;
    expr_t *q_squared = quadruple_exponent ? expr_exp(quadruple_exponent) : NULL;
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *psi_q_one = q && one ? expr_qdigamma(q, one) : NULL;
    expr_t *psi_q_upper = q && upper_plus_one ? expr_qdigamma(q, upper_plus_one) : NULL;
    expr_t *psi_q2_one = q_squared && one ? expr_qdigamma(q_squared, one) : NULL;
    expr_t *psi_q2_upper = q_squared && upper_plus_one ? expr_qdigamma(q_squared, upper_plus_one) : NULL;
    expr_t *first_difference = psi_q2_one && psi_q2_upper ? expr_sub(psi_q2_one, psi_q2_upper) : NULL;
    expr_t *second_difference = first_difference && psi_q_one ? expr_sub(first_difference, psi_q_one) : NULL;
    expr_t *numerator = second_difference && psi_q_upper ? expr_add(second_difference, psi_q_upper) : NULL;
    expr_t *quotient = numerator ? expr_div(numerator, step) : NULL;
    expr_t *i_n = imaginary_unit ? expr_mul(imaginary_unit, upper) : NULL;
    expr_t *out = i_n && quotient ? expr_add(i_n, quotient) : NULL;

    expr_free(i_n);
    expr_free(quotient);
    expr_free(numerator);
    expr_free(second_difference);
    expr_free(first_difference);
    expr_free(psi_q2_upper);
    expr_free(psi_q2_one);
    expr_free(psi_q_upper);
    expr_free(psi_q_one);
    expr_free(upper_plus_one);
    expr_free(q_squared);
    expr_free(quadruple_exponent);
    expr_free(q);
    expr_free(double_exponent);
    expr_free(exponent);
    expr_free(one);
    expr_free(imaginary_unit);
    return out;
}

static expr_t *expr_finite_tanh_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *double_step = expr_mul_long(step, -2L);
    expr_t *q = double_step ? expr_exp(double_step) : NULL;
    expr_t *q_squared = q ? expr_pow_long(q, 2L) : NULL;
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *psi_q_one = q && one ? expr_qdigamma(q, one) : NULL;
    expr_t *psi_q_upper = q && upper_plus_one ? expr_qdigamma(q, upper_plus_one) : NULL;
    expr_t *psi_q_difference = psi_q_one && psi_q_upper ? expr_sub(psi_q_one, psi_q_upper) : NULL;
    expr_t *log_q = q ? expr_log(q) : NULL;
    expr_t *first_numerator = psi_q_difference ? expr_mul_long(psi_q_difference, 2L) : NULL;
    expr_t *first_term = first_numerator && log_q ? expr_div(first_numerator, log_q) : NULL;
    expr_t *psi_q2_one = q_squared && one ? expr_qdigamma(q_squared, one) : NULL;
    expr_t *psi_q2_upper = q_squared && upper_plus_one ? expr_qdigamma(q_squared, upper_plus_one) : NULL;
    expr_t *psi_q2_difference = psi_q2_one && psi_q2_upper ? expr_sub(psi_q2_one, psi_q2_upper) : NULL;
    expr_t *log_q2 = q_squared ? expr_log(q_squared) : NULL;
    expr_t *second_numerator = psi_q2_difference ? expr_mul_long(psi_q2_difference, 4L) : NULL;
    expr_t *second_term = second_numerator && log_q2 ? expr_div(second_numerator, log_q2) : NULL;
    expr_t *first_difference = first_term ? expr_sub(upper, first_term) : NULL;
    expr_t *out = first_difference && second_term ? expr_add(first_difference, second_term) : NULL;

    expr_free(first_difference);
    expr_free(second_term);
    expr_free(second_numerator);
    expr_free(log_q2);
    expr_free(psi_q2_difference);
    expr_free(psi_q2_upper);
    expr_free(psi_q2_one);
    expr_free(first_term);
    expr_free(first_numerator);
    expr_free(log_q);
    expr_free(psi_q_difference);
    expr_free(psi_q_upper);
    expr_free(psi_q_one);
    expr_free(upper_plus_one);
    expr_free(q_squared);
    expr_free(q);
    expr_free(double_step);
    expr_free(one);
    return out;
}

static expr_t *expr_finite_tanh_progression_sum(const expr_t *upper, const expr_t *step)
{
    expr_t *index = expr_new_var(NUM_NAN);
    expr_t *argument;
    expr_t *term;
    expr_t *lower;
    expr_t *sum;

    if (index)
        expr_set_name(index, "k");
    argument = index ? expr_mul(index, step) : NULL;
    term = argument ? expr_tanh(argument) : NULL;
    lower = expr_new_const(NUM_ONE);
    sum = term && lower ? expr_new_finite_summation_range(term, index, lower, upper) : NULL;
    expr_free(lower);
    expr_free(term);
    expr_free(argument);
    expr_free(index);
    return sum;
}

static expr_t *expr_finite_qdigamma_difference(const expr_t *q, const expr_t *argument, const expr_t *upper)
{
    expr_t *upper_argument = expr_add(argument, upper);
    expr_t *first = expr_qdigamma(q, argument);
    expr_t *last = upper_argument ? expr_qdigamma(q, upper_argument) : NULL;
    expr_t *difference = first && last ? expr_sub(first, last) : NULL;

    expr_free(last);
    expr_free(first);
    expr_free(upper_argument);
    return difference;
}

static expr_t *expr_finite_cot_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *imaginary_unit = expr_new_named_const(NUM_I, "i");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *exponent = imaginary_unit ? expr_mul(imaginary_unit, step) : NULL;
    expr_t *double_exponent = exponent ? expr_mul_long(exponent, 2L) : NULL;
    expr_t *q = double_exponent ? expr_exp(double_exponent) : NULL;
    expr_t *difference = q && one ? expr_finite_qdigamma_difference(q, one, upper) : NULL;
    expr_t *log_q = q ? expr_log(q) : NULL;
    expr_t *twice_i = imaginary_unit ? expr_mul_long(imaginary_unit, 2L) : NULL;
    expr_t *numerator = twice_i && difference ? expr_mul(twice_i, difference) : NULL;
    expr_t *quotient = numerator && log_q ? expr_div(numerator, log_q) : NULL;
    expr_t *i_n = imaginary_unit ? expr_mul(imaginary_unit, upper) : NULL;
    expr_t *negative_i_n = i_n ? expr_neg(i_n) : NULL;
    expr_t *out = negative_i_n && quotient ? expr_sub(negative_i_n, quotient) : NULL;

    expr_free(negative_i_n);
    expr_free(i_n);
    expr_free(quotient);
    expr_free(numerator);
    expr_free(twice_i);
    expr_free(log_q);
    expr_free(difference);
    expr_free(q);
    expr_free(double_exponent);
    expr_free(exponent);
    expr_free(one);
    expr_free(imaginary_unit);
    return out;
}

static expr_t *expr_finite_coth_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *exponent = expr_mul_long(step, -2L);
    expr_t *q = exponent ? expr_exp(exponent) : NULL;
    expr_t *difference = q && one ? expr_finite_qdigamma_difference(q, one, upper) : NULL;
    expr_t *log_q = q ? expr_log(q) : NULL;
    expr_t *numerator = difference ? expr_mul_long(difference, 2L) : NULL;
    expr_t *quotient = numerator && log_q ? expr_div(numerator, log_q) : NULL;
    expr_t *out = quotient ? expr_add(upper, quotient) : NULL;

    expr_free(quotient);
    expr_free(numerator);
    expr_free(log_q);
    expr_free(difference);
    expr_free(q);
    expr_free(exponent);
    expr_free(one);
    return out;
}

static expr_t *expr_finite_cosecant_progression_closed_form(const expr_t *upper, const expr_t *step,
                                                            bool hyperbolic)
{
    expr_t *imaginary_unit = hyperbolic ? NULL : expr_new_named_const(NUM_I, "i");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *exponent = hyperbolic ? expr_neg(step) : (imaginary_unit ? expr_mul(imaginary_unit, step) : NULL);
    expr_t *q = exponent ? expr_exp(exponent) : NULL;
    expr_t *q_squared = q ? expr_pow_long(q, 2L) : NULL;
    expr_t *difference = q && one ? expr_finite_qdigamma_difference(q, one, upper) : NULL;
    expr_t *difference_squared = q_squared && one ? expr_finite_qdigamma_difference(q_squared, one, upper) : NULL;
    expr_t *log_q = q ? expr_log(q) : NULL;
    expr_t *log_q_squared = q_squared ? expr_log(q_squared) : NULL;
    expr_t *coefficient = hyperbolic ? expr_new_const(NUM_TWO) : expr_mul_long(imaginary_unit, 2L);
    expr_t *first_numerator = coefficient && difference ? expr_mul(coefficient, difference) : NULL;
    expr_t *second_numerator = coefficient && difference_squared ? expr_mul(coefficient, difference_squared) : NULL;
    expr_t *first = first_numerator && log_q ? expr_div(first_numerator, log_q) : NULL;
    expr_t *second = second_numerator && log_q_squared ? expr_div(second_numerator, log_q_squared) : NULL;
    expr_t *out = first && second ? (hyperbolic ? expr_sub(first, second) : expr_sub(second, first)) : NULL;

    expr_free(second);
    expr_free(first);
    expr_free(second_numerator);
    expr_free(first_numerator);
    expr_free(coefficient);
    expr_free(log_q_squared);
    expr_free(log_q);
    expr_free(difference_squared);
    expr_free(difference);
    expr_free(q_squared);
    expr_free(q);
    expr_free(exponent);
    expr_free(one);
    expr_free(imaginary_unit);
    return out;
}

static expr_t *expr_finite_secant_progression_closed_form(const expr_t *upper, const expr_t *step,
                                                          bool hyperbolic)
{
    expr_t *imaginary_unit = expr_new_named_const(NUM_I, "i");
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *exponent = hyperbolic ? expr_neg(step) : (imaginary_unit ? expr_mul(imaginary_unit, step) : NULL);
    expr_t *q = exponent ? expr_exp(exponent) : NULL;
    expr_t *log_q = q ? expr_log(q) : NULL;
    expr_t *i_pi = imaginary_unit && pi ? expr_mul(imaginary_unit, pi) : NULL;
    expr_t *twice_log_q = log_q ? expr_mul_long(log_q, 2L) : NULL;
    expr_t *shift = i_pi && twice_log_q ? expr_div(i_pi, twice_log_q) : NULL;
    expr_t *minus_argument = one && shift ? expr_sub(one, shift) : NULL;
    expr_t *plus_argument = one && shift ? expr_add(one, shift) : NULL;
    expr_t *minus_difference = q && minus_argument
                                   ? expr_finite_qdigamma_difference(q, minus_argument, upper)
                                   : NULL;
    expr_t *plus_difference = q && plus_argument ? expr_finite_qdigamma_difference(q, plus_argument, upper) : NULL;
    expr_t *difference = minus_difference && plus_difference ? expr_sub(minus_difference, plus_difference) : NULL;
    expr_t *numerator = imaginary_unit && difference ? expr_mul(imaginary_unit, difference) : NULL;
    expr_t *out = numerator && log_q ? expr_div(numerator, log_q) : NULL;

    expr_free(numerator);
    expr_free(difference);
    expr_free(plus_difference);
    expr_free(minus_difference);
    expr_free(plus_argument);
    expr_free(minus_argument);
    expr_free(shift);
    expr_free(twice_log_q);
    expr_free(i_pi);
    expr_free(log_q);
    expr_free(q);
    expr_free(exponent);
    expr_free(one);
    expr_free(pi);
    expr_free(imaginary_unit);
    return out;
}

static expr_t *expr_finite_progression_sum(const expr_t *upper, const expr_t *step,
                                           const expr_qdigamma_progression_t *progression)
{
    expr_t *index = expr_new_var(NUM_NAN);
    expr_t *argument;
    expr_t *term = NULL;
    expr_t *lower;
    expr_t *sum;

    if (index)
        expr_set_name(index, "k");
    argument = index ? expr_mul(index, step) : NULL;
    if (argument && progression && progression->apply)
        term = progression->apply(argument);
    lower = expr_new_const(NUM_ONE);
    sum = term && lower ? expr_new_finite_summation_range(term, index, lower, upper) : NULL;
    expr_free(lower);
    expr_free(term);
    expr_free(argument);
    expr_free(index);
    return sum;
}

static void expr_scan_qdigammas(const expr_t *expr, expr_qdigamma_scan_t *scan)
{
    if (!expr || !scan)
        return;
    if (expr_is_op(expr, &ops_qdigamma)) {
        ++scan->count;
        if (!scan->first) {
            scan->first = expr;
            scan->single_base = true;
        } else if (expr_struct_eq(scan->first->a, expr->a)) {
            if (!scan->second)
                scan->second = expr;
        } else {
            scan->single_base = false;
            if (!scan->alternate_first)
                scan->alternate_first = expr;
            else if (!scan->alternate_second && expr_struct_eq(scan->alternate_first->a, expr->a))
                scan->alternate_second = expr;
        }
    }
    expr_scan_qdigammas(expr->a, scan);
    expr_scan_qdigammas(expr->b, scan);
}

static bool expr_contains_imaginary_unit(const expr_t *expr);

static expr_t *expr_euler_base_step(const expr_t *q)
{
    const expr_t *left;
    const expr_t *right;
    const expr_t *cosine;
    const expr_t *imaginary_term;
    const expr_t *sine;
    bool subtract;

    if (!expr_match_add_sub_expr(q, &left, &right, &subtract) || subtract)
        return NULL;
    if (expr_is_op(left, &ops_cos)) {
        cosine = left;
        imaginary_term = right;
    } else if (expr_is_op(right, &ops_cos)) {
        cosine = right;
        imaginary_term = left;
    } else {
        return NULL;
    }
    if (expr_is_op(imaginary_term, &ops_sin))
        sine = imaginary_term;
    else if (imaginary_term && expr_is_op(imaginary_term->a, &ops_sin))
        sine = imaginary_term->a;
    else if (imaginary_term && expr_is_op(imaginary_term->b, &ops_sin))
        sine = imaginary_term->b;
    else
        return NULL;
    if (!expr_contains_imaginary_unit(imaginary_term) || !expr_struct_eq(cosine->a, sine->a))
        return NULL;
    return expr_clone(cosine->a);
}

static bool expr_contains_imaginary_unit(const expr_t *expr)
{
    number_t value;
    bool matched;

    if (!expr)
        return false;
    value = expr_eval((expr_t *)expr);
    matched = !expr->a && !expr->b && num_eq(value, NUM_I);
    num_destroy(&value);
    return matched || expr_contains_imaginary_unit(expr->a) || expr_contains_imaginary_unit(expr->b);
}

static bool expr_q_base_is_circular(const expr_t *q)
{
    expr_t *euler_step = expr_euler_base_step(q);
    const bool circular = euler_step != NULL || (expr_is_op(q, &ops_exp) && expr_contains_imaginary_unit(q->a));

    expr_free(euler_step);
    return circular;
}

static bool expr_is_exact_one_value(const expr_t *expr)
{
    number_t value = expr_eval((expr_t *)expr);
    const bool one = num_is_exact(value) && num_is_one(value);

    num_destroy(&value);
    return one;
}

static expr_t *expr_simplified_progression_step(expr_t *raw_step)
{
    expr_t *step = raw_step ? expr_simplify(raw_step) : NULL;

    expr_free(raw_step);
    return step;
}

static expr_t *expr_finite_tan_cot_step_from_q(const expr_t *q)
{
    expr_t *imaginary_unit;
    expr_t *divisor;
    expr_t *raw_step;

    if (!q)
        return NULL;
    if (!expr_is_op(q, &ops_exp) || !q->a) {
        expr_t *euler_step = expr_euler_base_step(q);
        expr_t *half_step;

        if (!euler_step)
            return NULL;
        half_step = expr_div_long(euler_step, 2L);
        expr_free(euler_step);
        return half_step;
    }

    imaginary_unit = expr_new_named_const(NUM_I, "i");
    divisor = imaginary_unit ? expr_mul_long(imaginary_unit, 2L) : NULL;
    raw_step = divisor ? expr_div(q->a, divisor) : NULL;
    expr_free(divisor);
    expr_free(imaginary_unit);
    return expr_simplified_progression_step(raw_step);
}

static expr_t *expr_finite_sec_cosec_step_from_q(const expr_t *q)
{
    const expr_t *left;
    const expr_t *right;
    const expr_t *exponent;
    expr_t *divisor;
    expr_t *imaginary_unit;
    expr_t *raw_step;
    bool negate_step = false;

    if (!q)
        return NULL;
    if (!expr_is_op(q, &ops_exp) || !q->a)
        return expr_euler_base_step(q);

    exponent = q->a;
    while (expr_is_op(exponent, &ops_neg) && exponent->a) {
        negate_step = !negate_step;
        exponent = exponent->a;
    }

    if (expr_match_mul_expr(exponent, &left, &right)) {
        number_t left_value = expr_eval((expr_t *)left);
        number_t right_value = expr_eval((expr_t *)right);
        const expr_t *step_source = num_eq(left_value, NUM_I) ? right
                                    : num_eq(right_value, NUM_I) ? left
                                                                : NULL;
        expr_t *circular_step = expr_clone(step_source);

        num_destroy(&right_value);
        num_destroy(&left_value);
        if (circular_step && negate_step) {
            expr_t *negative_step = expr_neg(circular_step);

            expr_free(circular_step);
            circular_step = negative_step;
        }
        if (circular_step)
            return circular_step;
    }

    imaginary_unit = expr_new_named_const(NUM_I, "i");
    divisor = imaginary_unit ? expr_neg(imaginary_unit) : NULL;
    raw_step = divisor ? expr_mul(q->a, divisor) : NULL;
    expr_free(divisor);
    expr_free(imaginary_unit);
    return expr_simplified_progression_step(raw_step);
}

static expr_t *expr_finite_tanh_coth_step_from_q(const expr_t *q)
{
    expr_t *raw_step;
    expr_t *negative_step;

    if (!q || !expr_is_op(q, &ops_exp) || !q->a)
        return NULL;
    raw_step = expr_div_long(q->a, 2L);
    negative_step = raw_step ? expr_neg(raw_step) : NULL;
    expr_free(raw_step);
    return expr_simplified_progression_step(negative_step);
}

static expr_t *expr_finite_sech_cosech_step_from_q(const expr_t *q)
{
    if (!q || !expr_is_op(q, &ops_exp) || !q->a)
        return NULL;
    return expr_simplified_progression_step(expr_neg(q->a));
}

static const expr_qdigamma_progression_t s_qdigamma_progressions[] = {
    {expr_tan, expr_finite_tangent_progression_closed_form, expr_finite_tan_cot_step_from_q, EXPR_QDIGAMMA_SECANT_NONE},
    {expr_cot, expr_finite_cot_progression_closed_form, expr_finite_tan_cot_step_from_q, EXPR_QDIGAMMA_SECANT_NONE},
    {expr_sec, expr_finite_sec_progression_closed_form, expr_finite_sec_cosec_step_from_q, EXPR_QDIGAMMA_SECANT_CIRCULAR},
    {expr_cosec, expr_finite_cosec_progression_closed_form, expr_finite_sec_cosec_step_from_q, EXPR_QDIGAMMA_SECANT_NONE},
    {expr_tanh, expr_finite_tanh_progression_closed_form, expr_finite_tanh_coth_step_from_q, EXPR_QDIGAMMA_SECANT_NONE},
    {expr_coth, expr_finite_coth_progression_closed_form, expr_finite_tanh_coth_step_from_q, EXPR_QDIGAMMA_SECANT_NONE},
    {expr_sech, expr_finite_sech_progression_closed_form, expr_finite_sech_cosech_step_from_q, EXPR_QDIGAMMA_SECANT_HYPERBOLIC},
    {expr_cosech, expr_finite_cosech_progression_closed_form, expr_finite_sech_cosech_step_from_q, EXPR_QDIGAMMA_SECANT_NONE},
};

static bool expr_matches_finite_qdigamma_candidate(const expr_t *expr, const expr_t *candidate)
{
    expr_t *difference;
    expr_t *simplified;
    bool matched;

    if (!expr || !candidate)
        return false;
    if (expr_struct_eq(expr, candidate))
        return true;
    difference = expr_sub(expr, candidate);
    simplified = difference ? expr_simplify(difference) : NULL;
    matched = simplified && expr_is_exact_zero(simplified);
    expr_free(simplified);
    expr_free(difference);
    return matched;
}

static expr_t *expr_recover_qdigamma_progression(const expr_t *expr, const expr_qdigamma_scan_t *scan,
                                                 const expr_t *base_qdigamma, const expr_t *upper)
{
    const bool circular_base = expr_q_base_is_circular(base_qdigamma->a);

    for (size_t progression_index = 0u;
         progression_index < sizeof(s_qdigamma_progressions) / sizeof(s_qdigamma_progressions[0]);
         ++progression_index) {
        const expr_qdigamma_progression_t *progression = &s_qdigamma_progressions[progression_index];
        const bool secant_signature = scan->count == 4u && scan->single_base &&
                                      !expr_is_exact_one_value(base_qdigamma->b) &&
                                      ((progression->secant_signature == EXPR_QDIGAMMA_SECANT_CIRCULAR &&
                                        circular_base) ||
                                       (progression->secant_signature == EXPR_QDIGAMMA_SECANT_HYPERBOLIC &&
                                        !circular_base));
        expr_t *step = progression->step_from_q(base_qdigamma->a);
        expr_t *candidate = step ? progression->closed_form(upper, step) : NULL;
        expr_t *sum;

        if (!secant_signature && !expr_matches_finite_qdigamma_candidate(expr, candidate)) {
            expr_free(candidate);
            expr_free(step);
            continue;
        }
        sum = expr_finite_progression_sum(upper, step, progression);
        expr_free(candidate);
        expr_free(step);
        return sum;
    }
    return NULL;
}

static expr_t *expr_recover_qdigamma_pair(const expr_t *expr, const expr_qdigamma_scan_t *scan,
                                          const expr_t *first, const expr_t *second)
{
    expr_t *raw_upper;
    expr_t *upper;
    expr_t *sum;
    expr_t *negative_upper;

    if (!first || !second)
        return NULL;
    raw_upper = expr_sub(second->b, first->b);
    upper = raw_upper ? expr_simplify(raw_upper) : NULL;
    expr_free(raw_upper);
    if (!upper)
        return NULL;

    sum = expr_recover_qdigamma_progression(expr, scan, first, upper);
    if (!sum) {
        negative_upper = expr_neg(upper);
        sum = negative_upper ? expr_recover_qdigamma_progression(expr, scan, first, negative_upper) : NULL;
        expr_free(negative_upper);
    }
    expr_free(upper);
    return sum;
}

/* Recover any supported finite quotient progression represented by its q-digamma identity. */
expr_t *expr_finite_progression_from_qdigamma_form(const expr_t *expr)
{
    expr_qdigamma_scan_t scan = {0};
    expr_t *sum;

    if (!expr)
        return NULL;
    expr_scan_qdigammas(expr, &scan);
    sum = expr_recover_qdigamma_pair(expr, &scan, scan.first, scan.second);
    return sum ? sum : expr_recover_qdigamma_pair(expr, &scan, scan.alternate_first, scan.alternate_second);
}

/* Recover a finite tangent progression represented by its q-digamma identity. */
expr_t *expr_finite_tangent_from_qdigamma_form(const expr_t *expr)
{
    expr_t *sum = expr_finite_progression_from_qdigamma_form(expr);
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;

    if (!sum || !expr_finite_progression_parts(sum, &upper, &step, &function_ops) || function_ops != &ops_tan) {
        expr_free(step);
        expr_free(sum);
        return NULL;
    }
    expr_free(step);
    return sum;
}

/* Recover a finite hyperbolic-tangent progression represented by its q-digamma identity. */
expr_t *expr_finite_tanh_from_qdigamma_form(const expr_t *expr)
{
    const expr_t *qdigamma = expr_find_qdigamma_with_symbolic_argument(expr);
    const expr_t *q;
    expr_t *one = NULL;
    expr_t *raw_upper = NULL;
    expr_t *upper = NULL;
    expr_t *raw_step = NULL;
    expr_t *step = NULL;
    expr_t *sum = NULL;
    expr_t *candidate = NULL;
    expr_t *difference = NULL;
    expr_t *simplified = NULL;
    expr_t *out = NULL;

    if (!expr || !qdigamma || !(q = qdigamma->a) || !expr_is_op(q, &ops_exp) || !q->a)
        return NULL;
    one = expr_new_const(NUM_ONE);
    raw_upper = one ? expr_sub(qdigamma->b, one) : NULL;
    upper = raw_upper ? expr_simplify(raw_upper) : NULL;
    raw_step = expr_div_long(q->a, -2L);
    step = raw_step ? expr_simplify(raw_step) : NULL;
    sum = upper && step ? expr_finite_tanh_progression_sum(upper, step) : NULL;
    candidate = upper && step ? expr_finite_tanh_progression_closed_form(upper, step) : NULL;
    if (!sum || !candidate)
        goto cleanup;
    if (!expr_struct_eq(expr, candidate)) {
        difference = expr_sub(expr, candidate);
        simplified = difference ? expr_simplify(difference) : NULL;
        if (!simplified || !expr_is_exact_zero(simplified))
            goto cleanup;
    }
    out = sum;
    sum = NULL;

cleanup:
    expr_free(simplified);
    expr_free(difference);
    expr_free(candidate);
    expr_free(sum);
    expr_free(step);
    expr_free(raw_step);
    expr_free(upper);
    expr_free(raw_upper);
    expr_free(one);
    return out;
}

/* Return whether an expression is the recognised finite tangent q-digamma identity. */
bool expr_is_finite_tangent_qdigamma_form(const expr_t *expr)
{
    expr_t *sum = expr_finite_tangent_from_qdigamma_form(expr);
    const bool matched = sum != NULL;

    expr_free(sum);
    return matched;
}

/* Evaluate a recognised finite tangent q-digamma identity through its finite real sum. */
bool expr_finite_tangent_qdigamma_value(const expr_t *expr, number_t *value_out)
{
    expr_t *sum;
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    long upper_value;

    if (!value_out || !(sum = expr_finite_tangent_from_qdigamma_form(expr)))
        return false;
    if (!expr_finite_progression_parts(sum, &upper, &step, &function_ops) || function_ops != &ops_tan ||
        !expr_summation_bound_to_long(upper, &upper_value)) {
        expr_free(step);
        expr_free(sum);
        return false;
    }
    expr_free(step);
    *value_out = eval_finite_qdigamma_progression(sum, upper_value);
    expr_free(sum);
    return num_is_finite(*value_out);
}

/* Evaluate any recognised finite quotient-progression q-digamma identity through its finite sum. */
bool expr_finite_qdigamma_progression_value(const expr_t *expr, number_t *value_out)
{
    expr_t *sum;
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    long upper_value;

    if (!value_out || !(sum = expr_finite_progression_from_qdigamma_form(expr)))
        return false;
    if (!expr_finite_progression_parts(sum, &upper, &step, &function_ops) ||
        (function_ops != &ops_tan && function_ops != &ops_cot && function_ops != &ops_sec &&
         function_ops != &ops_cosec && function_ops != &ops_sech) ||
        !expr_summation_bound_to_long(upper, &upper_value)) {
        expr_free(step);
        expr_free(sum);
        return false;
    }
    expr_free(step);
    *value_out = eval_finite_qdigamma_progression(sum, upper_value);
    expr_free(sum);
    return num_is_finite(*value_out);
}

static expr_t *expr_simplify_summation_with_special_forms(const expr_t *expr, expr_t *term, expr_t *bounds)
{
    return expr_simplify_binary_operator(expr, term, bounds);
}

static expr_t *expr_finite_exponential_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *q = expr_exp(step);
    expr_t *upper_step = expr_mul(upper, step);
    expr_t *q_to_upper = upper_step ? expr_exp(upper_step) : NULL;
    expr_t *numerator_factor = q_to_upper && one ? expr_sub(q_to_upper, one) : NULL;
    expr_t *numerator = q && numerator_factor ? expr_mul(q, numerator_factor) : NULL;
    expr_t *denominator = q && one ? expr_sub(q, one) : NULL;
    expr_t *out = numerator && denominator ? expr_div(numerator, denominator) : NULL;

    expr_free(denominator);
    expr_free(numerator);
    expr_free(numerator_factor);
    expr_free(q_to_upper);
    expr_free(upper_step);
    expr_free(q);
    expr_free(one);
    return out;
}

static expr_t *expr_finite_atan_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    number_t pi_value = num_const(NUM_PI);
    expr_t *imaginary_unit = expr_new_named_const(NUM_I, "i");
    expr_t *pi = expr_new_named_const(pi_value, "@pi");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *i_over_step = imaginary_unit ? expr_div(imaginary_unit, step) : NULL;
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *upper_minus = upper_plus_one && i_over_step ? expr_sub(upper_plus_one, i_over_step) : NULL;
    expr_t *lower_minus = one && i_over_step ? expr_sub(one, i_over_step) : NULL;
    expr_t *upper_plus = upper_plus_one && i_over_step ? expr_add(upper_plus_one, i_over_step) : NULL;
    expr_t *lower_plus = one && i_over_step ? expr_add(one, i_over_step) : NULL;
    expr_t *lgamma_upper_minus = upper_minus ? expr_lgamma(upper_minus) : NULL;
    expr_t *lgamma_lower_minus = lower_minus ? expr_lgamma(lower_minus) : NULL;
    expr_t *lgamma_upper_plus = upper_plus ? expr_lgamma(upper_plus) : NULL;
    expr_t *lgamma_lower_plus = lower_plus ? expr_lgamma(lower_plus) : NULL;
    expr_t *plus_difference = lgamma_upper_plus && lgamma_lower_plus
                                  ? expr_sub(lgamma_upper_plus, lgamma_lower_plus)
                                  : NULL;
    expr_t *with_lower_minus = plus_difference && lgamma_lower_minus
                                   ? expr_add(plus_difference, lgamma_lower_minus)
                                   : NULL;
    expr_t *gamma_correction = with_lower_minus && lgamma_upper_minus
                                   ? expr_sub(with_lower_minus, lgamma_upper_minus)
                                   : NULL;
    expr_t *upper_pi = pi ? expr_mul(upper, pi) : NULL;
    expr_t *signed_numerator = upper_pi ? expr_mul(upper_pi, step) : NULL;
    expr_t *absolute_step = expr_abs(step);
    expr_t *signed_denominator = absolute_step ? expr_mul_long(absolute_step, 2L) : NULL;
    expr_t *signed_contribution = signed_numerator && signed_denominator
                                      ? expr_div(signed_numerator, signed_denominator)
                                      : NULL;
    expr_t *gamma_numerator = imaginary_unit && gamma_correction ? expr_mul(imaginary_unit, gamma_correction) : NULL;
    expr_t *gamma_contribution = gamma_numerator ? expr_div_long(gamma_numerator, 2L) : NULL;
    expr_t *out = signed_contribution && gamma_contribution ? expr_add(signed_contribution, gamma_contribution) : NULL;

    num_destroy(&pi_value);
    expr_free(gamma_contribution);
    expr_free(gamma_numerator);
    expr_free(signed_contribution);
    expr_free(signed_denominator);
    expr_free(absolute_step);
    expr_free(signed_numerator);
    expr_free(upper_pi);
    expr_free(gamma_correction);
    expr_free(with_lower_minus);
    expr_free(plus_difference);
    expr_free(lgamma_lower_plus);
    expr_free(lgamma_upper_plus);
    expr_free(lgamma_lower_minus);
    expr_free(lgamma_upper_minus);
    expr_free(lower_plus);
    expr_free(upper_plus);
    expr_free(lower_minus);
    expr_free(upper_minus);
    expr_free(upper_plus_one);
    expr_free(i_over_step);
    expr_free(one);
    expr_free(pi);
    expr_free(imaginary_unit);
    return out;
}

/* Build the compact conjugate-pair derivative of a finite arctangent progression. */
expr_t *expr_finite_atan_progression_derivative_form(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    expr_t *upper_derivative = NULL;
    expr_t *step_derivative = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *one = NULL;
    expr_t *i_over_step = NULL;
    expr_t *upper_plus_one = NULL;
    expr_t *upper_plus = NULL;
    expr_t *lower_plus = NULL;
    expr_t *upper_minus = NULL;
    expr_t *lower_minus = NULL;
    expr_t *upper_plus_digamma = NULL;
    expr_t *lower_plus_digamma = NULL;
    expr_t *upper_minus_digamma = NULL;
    expr_t *lower_minus_digamma = NULL;
    expr_t *plus_difference = NULL;
    expr_t *minus_difference = NULL;
    expr_t *digamma_sum = NULL;
    expr_t *step_squared = NULL;
    expr_t *denominator = NULL;
    expr_t *scaled_numerator = NULL;
    expr_t *raw = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr_finite_progression_parts(expr, &upper, &step, &function_ops) ||
        function_ops != &ops_atan)
        goto cleanup;
    upper_derivative = expr_create_deriv(upper, wrt);
    if (!upper_derivative || !expr_is_exact_zero(upper_derivative))
        goto cleanup;
    step_derivative = expr_create_deriv(step, wrt);
    if (!step_derivative || expr_is_exact_zero(step_derivative))
        goto cleanup;

    imaginary_unit = expr_new_named_const(NUM_I, "i");
    one = expr_new_const(NUM_ONE);
    i_over_step = imaginary_unit ? expr_div(imaginary_unit, step) : NULL;
    upper_plus_one = expr_add_long(upper, 1L);
    upper_plus = upper_plus_one && i_over_step ? expr_add(upper_plus_one, i_over_step) : NULL;
    lower_plus = one && i_over_step ? expr_add(one, i_over_step) : NULL;
    upper_minus = upper_plus_one && i_over_step ? expr_sub(upper_plus_one, i_over_step) : NULL;
    lower_minus = one && i_over_step ? expr_sub(one, i_over_step) : NULL;
    upper_plus_digamma = upper_plus ? expr_digamma(upper_plus) : NULL;
    lower_plus_digamma = lower_plus ? expr_digamma(lower_plus) : NULL;
    upper_minus_digamma = upper_minus ? expr_digamma(upper_minus) : NULL;
    lower_minus_digamma = lower_minus ? expr_digamma(lower_minus) : NULL;
    plus_difference = upper_plus_digamma && lower_plus_digamma
                          ? expr_sub(upper_plus_digamma, lower_plus_digamma)
                          : NULL;
    minus_difference = upper_minus_digamma && lower_minus_digamma
                           ? expr_sub(upper_minus_digamma, lower_minus_digamma)
                           : NULL;
    digamma_sum = plus_difference && minus_difference ? expr_add(plus_difference, minus_difference) : NULL;
    step_squared = step ? expr_pow_long(step, 2L) : NULL;
    denominator = step_squared ? expr_mul_long(step_squared, 2L) : NULL;
    scaled_numerator = digamma_sum && step_derivative ? expr_mul(step_derivative, digamma_sum) : NULL;
    raw = scaled_numerator && denominator ? expr_div(scaled_numerator, denominator) : NULL;
    out = raw ? expr_simplify(raw) : NULL;

cleanup:
    expr_free(raw);
    expr_free(scaled_numerator);
    expr_free(denominator);
    expr_free(step_squared);
    expr_free(digamma_sum);
    expr_free(minus_difference);
    expr_free(plus_difference);
    expr_free(lower_minus_digamma);
    expr_free(upper_minus_digamma);
    expr_free(lower_plus_digamma);
    expr_free(upper_plus_digamma);
    expr_free(lower_minus);
    expr_free(upper_minus);
    expr_free(lower_plus);
    expr_free(upper_plus);
    expr_free(upper_plus_one);
    expr_free(i_over_step);
    expr_free(one);
    expr_free(imaginary_unit);
    expr_free(step_derivative);
    expr_free(upper_derivative);
    expr_free(step);
    return out;
}

static expr_t *expr_finite_reciprocal_loggamma_difference(const expr_t *upper, const expr_t *shift)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *upper_plus = upper_plus_one && shift ? expr_add(upper_plus_one, shift) : NULL;
    expr_t *lower_plus = one && shift ? expr_add(one, shift) : NULL;
    expr_t *upper_minus = upper_plus_one && shift ? expr_sub(upper_plus_one, shift) : NULL;
    expr_t *lower_minus = one && shift ? expr_sub(one, shift) : NULL;
    expr_t *lgamma_upper_plus = upper_plus ? expr_lgamma(upper_plus) : NULL;
    expr_t *lgamma_lower_plus = lower_plus ? expr_lgamma(lower_plus) : NULL;
    expr_t *lgamma_upper_minus = upper_minus ? expr_lgamma(upper_minus) : NULL;
    expr_t *lgamma_lower_minus = lower_minus ? expr_lgamma(lower_minus) : NULL;
    expr_t *first_difference = lgamma_upper_plus && lgamma_lower_plus
                                   ? expr_sub(lgamma_upper_plus, lgamma_lower_plus)
                                   : NULL;
    expr_t *second_difference = first_difference && lgamma_lower_minus
                                    ? expr_add(first_difference, lgamma_lower_minus)
                                    : NULL;
    expr_t *out = second_difference && lgamma_upper_minus ? expr_sub(second_difference, lgamma_upper_minus) : NULL;

    expr_free(second_difference);
    expr_free(first_difference);
    expr_free(lgamma_lower_minus);
    expr_free(lgamma_upper_minus);
    expr_free(lgamma_lower_plus);
    expr_free(lgamma_upper_plus);
    expr_free(lower_minus);
    expr_free(upper_minus);
    expr_free(lower_plus);
    expr_free(upper_plus);
    expr_free(upper_plus_one);
    expr_free(one);
    return out;
}

static expr_t *expr_finite_atanh_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *reciprocal_step = one ? expr_div(one, step) : NULL;
    expr_t *gamma_difference = reciprocal_step
                                   ? expr_finite_reciprocal_loggamma_difference(upper, reciprocal_step)
                                   : NULL;
    expr_t *negative_step = expr_neg(step);
    expr_t *log_step = expr_ln(step);
    expr_t *log_negative_step = negative_step ? expr_ln(negative_step) : NULL;
    expr_t *log_difference = log_step && log_negative_step ? expr_sub(log_step, log_negative_step) : NULL;
    expr_t *scaled_log_difference = log_difference ? expr_mul(upper, log_difference) : NULL;
    expr_t *numerator = scaled_log_difference && gamma_difference
                            ? expr_add(scaled_log_difference, gamma_difference)
                            : NULL;
    expr_t *out = numerator ? expr_div_long(numerator, 2L) : NULL;

    expr_free(numerator);
    expr_free(scaled_log_difference);
    expr_free(log_difference);
    expr_free(log_negative_step);
    expr_free(log_step);
    expr_free(negative_step);
    expr_free(gamma_difference);
    expr_free(reciprocal_step);
    expr_free(one);
    return out;
}

static expr_t *expr_finite_acot_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *imaginary_unit = expr_new_named_const(NUM_I, "i");
    expr_t *shift = imaginary_unit ? expr_div(imaginary_unit, step) : NULL;
    expr_t *numerator = shift ? expr_finite_reciprocal_loggamma_difference(upper, shift) : NULL;
    expr_t *twice_i = imaginary_unit ? expr_mul_long(imaginary_unit, 2L) : NULL;
    expr_t *out = numerator && twice_i ? expr_div(numerator, twice_i) : NULL;

    expr_free(twice_i);
    expr_free(numerator);
    expr_free(shift);
    expr_free(imaginary_unit);
    return out;
}

static expr_t *expr_finite_acoth_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *shift = one ? expr_div(one, step) : NULL;
    expr_t *numerator = shift ? expr_finite_reciprocal_loggamma_difference(upper, shift) : NULL;
    expr_t *out = numerator ? expr_div_long(numerator, 2L) : NULL;

    expr_free(numerator);
    expr_free(shift);
    expr_free(one);
    return out;
}

static expr_t *expr_finite_logarithmic_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *log_step = expr_ln(step);
    expr_t *scaled_log = log_step ? expr_mul(upper, log_step) : NULL;
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *log_factorial = upper_plus_one ? expr_lgamma(upper_plus_one) : NULL;
    expr_t *out = scaled_log && log_factorial ? expr_add(scaled_log, log_factorial) : NULL;

    expr_free(log_factorial);
    expr_free(upper_plus_one);
    expr_free(scaled_log);
    expr_free(log_step);
    return out;
}

static expr_t *expr_finite_common_logarithmic_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *ten = expr_new_const(NUM_TEN);
    expr_t *log_step = expr_log10(step);
    expr_t *scaled_log = log_step ? expr_mul(upper, log_step) : NULL;
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *log_factorial = upper_plus_one ? expr_lgamma(upper_plus_one) : NULL;
    expr_t *log_ten = ten ? expr_ln(ten) : NULL;
    expr_t *common_log_factorial = log_factorial && log_ten ? expr_div(log_factorial, log_ten) : NULL;
    expr_t *out = scaled_log && common_log_factorial ? expr_add(scaled_log, common_log_factorial) : NULL;

    expr_free(common_log_factorial);
    expr_free(log_ten);
    expr_free(log_factorial);
    expr_free(upper_plus_one);
    expr_free(scaled_log);
    expr_free(log_step);
    expr_free(ten);
    return out;
}

static expr_t *expr_finite_sec_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_secant_progression_closed_form(upper, step, false);
}

static expr_t *expr_finite_cosec_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_cosecant_progression_closed_form(upper, step, false);
}

static expr_t *expr_finite_sech_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_secant_progression_closed_form(upper, step, true);
}

static expr_t *expr_finite_cosech_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_cosecant_progression_closed_form(upper, step, true);
}

static expr_t *expr_finite_sine_family_progression_closed_form(const expr_t *upper, const expr_t *step,
                                                               bool hyperbolic, bool cosine_second_factor)
{
    expr_t *upper_times_step = expr_mul(upper, step);
    expr_t *first_argument = upper_times_step ? expr_div_long(upper_times_step, 2L) : NULL;
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *second_product = upper_plus_one ? expr_mul(upper_plus_one, step) : NULL;
    expr_t *second_argument = second_product ? expr_div_long(second_product, 2L) : NULL;
    expr_t *first_factor = first_argument ? (hyperbolic ? expr_sinh(first_argument) : expr_sin(first_argument)) : NULL;
    expr_t *second_factor = second_argument
                                ? (hyperbolic ? (cosine_second_factor ? expr_cosh(second_argument)
                                                                      : expr_sinh(second_argument))
                                              : (cosine_second_factor ? expr_cos(second_argument)
                                                                      : expr_sin(second_argument)))
                                : NULL;
    expr_t *numerator = first_factor && second_factor ? expr_mul(first_factor, second_factor) : NULL;
    expr_t *denominator_argument = expr_div_long(step, 2L);
    expr_t *denominator = denominator_argument
                              ? (hyperbolic ? expr_sinh(denominator_argument) : expr_sin(denominator_argument))
                              : NULL;
    expr_t *out = numerator && denominator ? expr_div(numerator, denominator) : NULL;

    expr_free(denominator);
    expr_free(denominator_argument);
    expr_free(numerator);
    expr_free(second_factor);
    expr_free(first_factor);
    expr_free(second_argument);
    expr_free(second_product);
    expr_free(upper_plus_one);
    expr_free(first_argument);
    expr_free(upper_times_step);
    return out;
}

static expr_t *expr_finite_sin_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_sine_family_progression_closed_form(upper, step, false, false);
}

static expr_t *expr_finite_cos_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_sine_family_progression_closed_form(upper, step, false, true);
}

static expr_t *expr_finite_sinh_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_sine_family_progression_closed_form(upper, step, true, false);
}

static expr_t *expr_finite_cosh_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_sine_family_progression_closed_form(upper, step, true, true);
}

static expr_t *expr_finite_versed_progression_closed_form(const expr_t *upper, const expr_t *step,
                                                          bool sine, bool add, bool halve)
{
    expr_t *trig_sum = sine ? expr_finite_sin_progression_closed_form(upper, step)
                            : expr_finite_cos_progression_closed_form(upper, step);
    expr_t *combined = trig_sum ? (add ? expr_add(upper, trig_sum) : expr_sub(upper, trig_sum)) : NULL;
    expr_t *out = combined && halve ? expr_div_long(combined, 2L) : expr_clone(combined);

    expr_free(combined);
    expr_free(trig_sum);
    return out;
}

static expr_t *expr_finite_versin_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, false, false, false);
}

static expr_t *expr_finite_vercos_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, false, true, false);
}

static expr_t *expr_finite_coversin_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, true, false, false);
}

static expr_t *expr_finite_covercos_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, true, true, false);
}

static expr_t *expr_finite_haversin_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, false, false, true);
}

static expr_t *expr_finite_havercos_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, false, true, true);
}

static expr_t *expr_finite_hacoversin_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, true, false, true);
}

static expr_t *expr_finite_hacovercos_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_versed_progression_closed_form(upper, step, true, true, true);
}

static expr_t *expr_finite_homogeneous_linear_progression_closed_form(const expr_t *upper, const expr_t *step,
                                                                     expr_apply_unary_fn function)
{
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *triangular_numerator = upper_plus_one ? expr_mul(upper, upper_plus_one) : NULL;
    expr_t *triangular = triangular_numerator ? expr_div_long(triangular_numerator, 2L) : NULL;
    expr_t *function_step = function ? function(step) : NULL;
    expr_t *out = triangular && function_step ? expr_mul(triangular, function_step) : NULL;

    expr_free(function_step);
    expr_free(triangular);
    expr_free(triangular_numerator);
    expr_free(upper_plus_one);
    return out;
}

static expr_t *expr_finite_linear_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *triangular_numerator = upper_plus_one ? expr_mul(upper, upper_plus_one) : NULL;
    expr_t *triangular = triangular_numerator ? expr_div_long(triangular_numerator, 2L) : NULL;
    expr_t *out = triangular ? expr_mul(step, triangular) : NULL;

    expr_free(triangular);
    expr_free(triangular_numerator);
    expr_free(upper_plus_one);
    return out;
}

static long expr_floor_div_long(long numerator, long denominator)
{
    const long quotient = numerator / denominator;
    const long remainder = numerator % denominator;

    return remainder < 0L ? quotient - 1L : quotient;
}

static bool expr_finite_small_rational_step(const expr_t *step, long *numerator_out, long *denominator_out)
{
    enum { MAX_DENOMINATOR = 32, MAX_NUMERATOR = 1000000 };
    number_t step_value;
    double approximate;
    bool matched = false;

    if (!step || !numerator_out || !denominator_out)
        return false;
    step_value = expr_eval((expr_t *)step);
    if (!num_is_real(step_value) || !num_is_finite(step_value))
        goto cleanup;
    if (num_is_integer(step_value)) {
        *numerator_out = 0L;
        *denominator_out = 1L;
        matched = true;
        goto cleanup;
    }
    approximate = num_to_double(step_value);
    if (!isfinite(approximate))
        goto cleanup;
    for (long denominator = 2L; denominator <= MAX_DENOMINATOR; ++denominator) {
        const double scaled = approximate * (double)denominator;
        long numerator;
        number_t candidate;

        if (!isfinite(scaled) || scaled < -(double)MAX_NUMERATOR - 0.5 || scaled > (double)MAX_NUMERATOR + 0.5)
            continue;
        numerator = lround(scaled);
        candidate = num_create_from_frac(numerator, denominator);
        if (num_eq(step_value, candidate)) {
            long a = labs(numerator);
            long b = denominator;

            while (b != 0L) {
                const long remainder = a % b;

                a = b;
                b = remainder;
            }
            if (a > 1L) {
                numerator /= a;
                denominator /= a;
            }
            *numerator_out = numerator;
            *denominator_out = denominator;
            matched = true;
            num_destroy(&candidate);
            break;
        }
        num_destroy(&candidate);
    }

cleanup:
    num_destroy(&step_value);
    return matched;
}

static expr_t *expr_finite_rational_rounding_progression_closed_form(const expr_t *upper, const expr_t *step,
                                                                     bool ceiling)
{
    long numerator;
    long denominator;
    bool literal_step;
    expr_t *denominator_expr = NULL;
    expr_t *upper_over_denominator = NULL;
    expr_t *period = NULL;
    expr_t *remainder = NULL;
    expr_t *period_increment = NULL;
    expr_t *period_total = NULL;
    expr_t *period_minus_one = NULL;
    expr_t *quadratic_coefficient = NULL;
    expr_t *quadratic_product = NULL;
    expr_t *quadratic_per_period = NULL;
    expr_t *partial_per_period = NULL;
    expr_t *inside_partial = NULL;
    expr_t *inside = NULL;
    expr_t *complete_and_partial = NULL;
    expr_t *residual = NULL;
    expr_t *out = NULL;

    if (!expr_finite_small_rational_step(step, &numerator, &denominator))
        return NULL;
    if (denominator == 1L)
        return expr_finite_linear_progression_closed_form(upper, step);
    literal_step = expr_is_unnamed_const(step);

    denominator_expr = expr_const_long(denominator);
    upper_over_denominator = denominator_expr ? expr_div(upper, denominator_expr) : NULL;
    period = upper_over_denominator ? expr_floor(upper_over_denominator) : NULL;
    remainder = denominator_expr ? expr_mod(upper, denominator_expr) : NULL;
    period_increment = literal_step ? expr_const_long(numerator) : expr_mul_long(step, denominator);
    if (literal_step) {
        const long period_value = numerator +
                                  (numerator + (ceiling ? 1L : -1L)) * (denominator - 1L) / 2L;

        period_total = expr_const_long(period_value);
    } else if (period_increment && denominator % 2L != 0L) {
        expr_t *scaled = expr_mul_long(step, denominator * (denominator + 1L) / 2L);

        period_total = scaled ? expr_add_long(scaled, (ceiling ? 1L : -1L) * (denominator - 1L) / 2L) : NULL;
        expr_free(scaled);
    } else if (period_increment) {
        expr_t *scaled = expr_mul_long(step, denominator * (denominator + 1L));
        expr_t *adjusted = scaled ? expr_add_long(scaled, (ceiling ? 1L : -1L) * (denominator - 1L)) : NULL;

        period_total = adjusted ? expr_div_long(adjusted, 2L) : NULL;
        expr_free(adjusted);
        expr_free(scaled);
    }
    period_minus_one = period ? expr_add_long(period, -1L) : NULL;
    quadratic_coefficient = literal_step ? expr_const_long(numerator * denominator)
                                         : expr_mul_long(step, denominator * denominator);
    quadratic_product = quadratic_coefficient && period_minus_one
                            ? expr_mul(quadratic_coefficient, period_minus_one)
                            : NULL;
    quadratic_per_period = quadratic_product ? expr_div_long(quadratic_product, 2L) : NULL;
    partial_per_period = period_increment && remainder ? expr_mul(period_increment, remainder) : NULL;
    inside_partial = quadratic_per_period && period_total ? expr_add(quadratic_per_period, period_total) : NULL;
    inside = inside_partial && partial_per_period ? expr_add(inside_partial, partial_per_period) : NULL;
    complete_and_partial = period && inside ? expr_mul(period, inside) : NULL;

    for (long index = 1L; index < denominator; ++index) {
        const long residue_value = ceiling ? -expr_floor_div_long(-numerator * index, denominator)
                                           : expr_floor_div_long(numerator * index, denominator);
        expr_t *indicator_numerator;
        expr_t *indicator;
        expr_t *indexed_step;
        expr_t *rounded_step;
        expr_t *term;

        if (residue_value == 0L)
            continue;
        indicator_numerator = expr_add_long(remainder, denominator - index);
        if (indicator_numerator) {
            expr_t *indicator_fraction = expr_div(indicator_numerator, denominator_expr);

            indicator = indicator_fraction ? expr_floor(indicator_fraction) : NULL;
            expr_free(indicator_fraction);
        } else {
            indicator = NULL;
        }
        indexed_step = literal_step ? NULL : (index == 1L ? expr_clone(step) : expr_mul_long(step, index));
        rounded_step = indexed_step ? (ceiling ? expr_ceil(indexed_step) : expr_floor(indexed_step)) : NULL;
        if (literal_step && indicator && residue_value == 1L)
            term = expr_clone(indicator);
        else if (literal_step && indicator && residue_value == -1L)
            term = expr_neg(indicator);
        else
            term = literal_step && indicator ? expr_mul_long(indicator, residue_value)
                                             : (indicator && rounded_step ? expr_mul(indicator, rounded_step) : NULL);
        if (term) {
            expr_t *combined = residual ? expr_add(residual, term) : expr_clone(term);

            expr_free(residual);
            residual = combined;
        }
        expr_free(term);
        expr_free(rounded_step);
        expr_free(indexed_step);
        expr_free(indicator);
        expr_free(indicator_numerator);
    }

    out = complete_and_partial && residual ? expr_add(complete_and_partial, residual) : expr_clone(complete_and_partial);

    expr_free(residual);
    expr_free(complete_and_partial);
    expr_free(inside);
    expr_free(inside_partial);
    expr_free(partial_per_period);
    expr_free(quadratic_per_period);
    expr_free(quadratic_product);
    expr_free(quadratic_coefficient);
    expr_free(period_minus_one);
    expr_free(period_total);
    expr_free(period_increment);
    expr_free(remainder);
    expr_free(period);
    expr_free(upper_over_denominator);
    expr_free(denominator_expr);
    return out;
}

static expr_t *expr_finite_floor_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_rational_rounding_progression_closed_form(upper, step, false);
}

static expr_t *expr_finite_ceil_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_rational_rounding_progression_closed_form(upper, step, true);
}

static expr_t *expr_finite_bit_not_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    number_t step_value = expr_eval((expr_t *)step);
    const bool integer_step = num_is_real(step_value) && num_is_finite(step_value) && num_is_integer(step_value);
    expr_t *linear_sum = integer_step ? expr_finite_linear_progression_closed_form(upper, step) : NULL;
    expr_t *negative_linear_sum = linear_sum ? expr_neg(linear_sum) : NULL;
    expr_t *out = negative_linear_sum ? expr_sub(negative_linear_sum, upper) : NULL;

    expr_free(negative_linear_sum);
    expr_free(linear_sum);
    num_destroy(&step_value);
    return out;
}

static expr_t *expr_finite_abs_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_homogeneous_linear_progression_closed_form(upper, step, expr_abs);
}

static expr_t *expr_finite_conj_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_homogeneous_linear_progression_closed_form(upper, step, expr_conj);
}

static expr_t *expr_finite_root_progression_closed_form(const expr_t *upper, const expr_t *step, long degree,
                                                       expr_apply_unary_fn function)
{
    expr_t *degree_expr = expr_const_long(degree);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *power = one && degree_expr ? expr_div(one, degree_expr) : NULL;
    expr_t *negative_power = power ? expr_neg(power) : NULL;
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *zeta = negative_power ? expr_zeta(negative_power) : NULL;
    expr_t *hurwitz = negative_power && upper_plus_one ? expr_zetah(negative_power, upper_plus_one) : NULL;
    expr_t *power_sum = zeta && hurwitz ? expr_sub(zeta, hurwitz) : NULL;
    expr_t *function_step = function ? function(step) : NULL;
    expr_t *out = function_step && power_sum ? expr_mul(function_step, power_sum) : NULL;

    expr_free(function_step);
    expr_free(power_sum);
    expr_free(hurwitz);
    expr_free(zeta);
    expr_free(upper_plus_one);
    expr_free(negative_power);
    expr_free(power);
    expr_free(one);
    expr_free(degree_expr);
    return out;
}

static expr_t *expr_finite_sqrt_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_root_progression_closed_form(upper, step, 2L, expr_sqrt);
}

static expr_t *expr_finite_cubrt_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    return expr_finite_root_progression_closed_form(upper, step, 3L, expr_cubrt);
}

static expr_t *expr_finite_normal_logpdf_progression_closed_form(const expr_t *upper, const expr_t *step)
{
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *upper_plus_one = expr_add_long(upper, 1L);
    expr_t *twice_upper = expr_mul_long(upper, 2L);
    expr_t *twice_upper_plus_one = twice_upper ? expr_add_long(twice_upper, 1L) : NULL;
    expr_t *sum_squares_numerator = upper_plus_one && twice_upper_plus_one
                                        ? expr_mul(upper, upper_plus_one)
                                        : NULL;
    expr_t *sum_squares_product = sum_squares_numerator && twice_upper_plus_one
                                      ? expr_mul(sum_squares_numerator, twice_upper_plus_one)
                                      : NULL;
    expr_t *sum_squares = sum_squares_product ? expr_div_long(sum_squares_product, 6L) : NULL;
    expr_t *step_squared = expr_pow_long(step, 2L);
    expr_t *quadratic_product = step_squared && sum_squares ? expr_mul(step_squared, sum_squares) : NULL;
    expr_t *quadratic = quadratic_product ? expr_div_long(quadratic_product, 2L) : NULL;
    expr_t *two_pi = two && pi ? expr_mul(two, pi) : NULL;
    expr_t *log_two_pi = two_pi ? expr_ln(two_pi) : NULL;
    expr_t *normalisation_product = log_two_pi ? expr_mul(upper, log_two_pi) : NULL;
    expr_t *normalisation = normalisation_product ? expr_div_long(normalisation_product, 2L) : NULL;
    expr_t *magnitude = quadratic && normalisation ? expr_add(quadratic, normalisation) : NULL;
    expr_t *out = magnitude ? expr_neg(magnitude) : NULL;

    expr_free(magnitude);
    expr_free(normalisation);
    expr_free(normalisation_product);
    expr_free(log_two_pi);
    expr_free(two_pi);
    expr_free(quadratic);
    expr_free(quadratic_product);
    expr_free(step_squared);
    expr_free(sum_squares);
    expr_free(sum_squares_product);
    expr_free(sum_squares_numerator);
    expr_free(twice_upper_plus_one);
    expr_free(twice_upper);
    expr_free(upper_plus_one);
    expr_free(pi);
    expr_free(two);
    return out;
}

/* Return the closed form supplied by the summand function for a recognised finite progression. */
expr_t *expr_finite_progression_closed_form(const expr_t *expr)
{
    const expr_t *upper;
    expr_t *step = NULL;
    const expr_ops_t *function_ops;
    expr_t *out;

    if (!expr_finite_progression_parts(expr, &upper, &step, &function_ops))
        return NULL;
    if (!function_ops->finite_progression) {
        expr_free(step);
        return NULL;
    }
    out = function_ops->finite_progression(upper, step);
    expr_free(step);
    return out;
}

/* Render a recognised native finite progression together with the exact identity supplied by its function. */
char *expr_finite_progression_identity_TeX(const expr_t *expr)
{
    const expr_t *upper;
    expr_t *step = NULL;
    expr_t *symbolic_step = NULL;
    char *summation_TeX = NULL;
    char *upper_TeX = NULL;
    char *step_TeX = NULL;
    char *identity_TeX = NULL;
    const expr_ops_t *function_ops;

    if (!expr_finite_progression_parts(expr, &upper, &step, &function_ops))
        return NULL;
    if (!function_ops->finite_progression) {
        expr_free(step);
        return NULL;
    }

    if (!step->name || !*step->name ||
        (function_ops != &ops_sin && function_ops != &ops_cos && function_ops != &ops_sinh &&
         function_ops != &ops_cosh)) {
        expr_t *closed_form = function_ops->finite_progression(upper, step);
        char *closed_form_TeX = closed_form ? expr_to_TeX_body(closed_form) : NULL;

        summation_TeX = expr_to_TeX_body(expr);
        if (summation_TeX && closed_form_TeX) {
            const char *format = "%s = %s";
            const size_t length = (size_t)snprintf(NULL, 0, format, summation_TeX, closed_form_TeX) + 1u;

            identity_TeX = malloc(length);
            if (identity_TeX)
                snprintf(identity_TeX, length, format, summation_TeX, closed_form_TeX);
        }
        free(closed_form_TeX);
        free(summation_TeX);
        expr_free(closed_form);
        expr_free(step);
        return identity_TeX;
    }
    symbolic_step = expr_new_named_var(NUM_NAN, step->name);
    summation_TeX = expr_to_TeX_body(expr);
    upper_TeX = expr_to_TeX_body(upper);
    step_TeX = symbolic_step ? expr_to_TeX_body(symbolic_step) : NULL;
    if (summation_TeX && upper_TeX && step_TeX) {
        const bool circular = function_ops == &ops_sin || function_ops == &ops_cos;
        const char *first_function = circular ? "\\sin" : "\\sinh";
        const char *second_function = function_ops == &ops_sin
                                          ? "\\sin"
                                      : function_ops == &ops_cos
                                          ? "\\cos"
                                      : function_ops == &ops_sinh ? "\\sinh" : "\\cosh";
        const char *format = "%s = \\frac{%s(\\frac{%s\\mkern-2mu %s}{2})\\mkern-2mu "
                             "%s(\\frac{%s}{2}\\mkern-2mu \\left(%s + 1\\right))}{%s(\\frac{%s}{2})}";
        const size_t length = (size_t)snprintf(NULL, 0, format, summation_TeX, first_function, upper_TeX,
                                               step_TeX, second_function, step_TeX, upper_TeX, first_function,
                                               step_TeX) +
                              1u;

        identity_TeX = malloc(length);
        if (identity_TeX)
            snprintf(identity_TeX, length, format, summation_TeX, first_function, upper_TeX, step_TeX,
                     second_function, step_TeX, upper_TeX, first_function, step_TeX);
    }

    free(step_TeX);
    free(upper_TeX);
    free(summation_TeX);
    expr_free(symbolic_step);
    expr_free(step);
    return identity_TeX;
}

static expr_t *integrate_summation(const expr_t *expr, const expr_t *wrt)
{
    expr_t *integrated_term;
    expr_t *out;

    if (!expr || !expr->a || !expr->b || !wrt)
        return NULL;
    integrated_term = expr_integrate(expr->a, wrt);
    if (!integrated_term)
        integrated_term = expr_integral(expr->a, wrt);
    out = integrated_term ? expr_new_summation(integrated_term, expr->b) : NULL;
    expr_free(integrated_term);
    return out;
}

static expr_t *integrate_harmonic_poly(const expr_t *expr, const expr_t *wrt)
{
    expr_t *degree_derivative = NULL;
    expr_t *next_degree = NULL;
    expr_t *scaled = NULL;
    expr_t *next = NULL;
    expr_t *difference = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b || !wrt || (expr->b != wrt && !expr_struct_eq(expr->b, wrt)))
        return NULL;
    degree_derivative = expr_create_deriv(expr->a, wrt);
    if (!degree_derivative || !expr_is_exact_zero(degree_derivative)) {
        expr_free(degree_derivative);
        return NULL;
    }
    expr_free(degree_derivative);

    next_degree = expr_add_long(expr->a, 1L);
    scaled = expr_mul(expr->b, expr);
    next = next_degree ? expr_harmonic_poly(next_degree, expr->b) : NULL;
    difference = scaled && next ? expr_sub(scaled, next) : NULL;
    out = difference ? expr_add(difference, expr->b) : NULL;

    expr_free(difference);
    expr_free(next);
    expr_free(scaled);
    expr_free(next_degree);
    return out;
}

static expr_t *integrate_zeta_formal(const expr_t *expr, const expr_t *wrt)
{
    return expr_integral(expr, wrt);
}

static expr_t *integrate_zetap(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt || !expr->a || expr->a != wrt)
        return NULL;
    return expr_zeta(wrt);
}

static expr_t *integrate_zetah(const expr_t *expr, const expr_t *wrt)
{
    expr_t *one;
    expr_t *s_minus_one;
    expr_t *one_minus_s;
    expr_t *shifted;
    expr_t *out;

    if (!expr || !wrt || !expr->a || !expr->b)
        return NULL;
    if (expr->b != wrt && !expr_struct_eq(expr->b, wrt))
        return expr_integral(expr, wrt);
    one = expr_new_const(NUM_ONE);
    s_minus_one = expr_sub(expr->a, one);
    one_minus_s = expr_sub(one, expr->a);
    shifted = expr_zetah(s_minus_one, expr->b);
    out = expr_div(shifted, one_minus_s);
    expr_free(shifted);
    expr_free(one_minus_s);
    expr_free(s_minus_one);
    expr_free(one);
    return out;
}

static expr_t *integrate_zatahp(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt || !expr->a || !expr->b || (expr->a != wrt && !expr_struct_eq(expr->a, wrt)))
        return expr_integral(expr, wrt);
    return expr_zetah(expr->a, expr->b);
}

static expr_t *expr_inverse_log10_internal(const expr_t *a)
{
    expr_t *ten = expr_new_const(NUM_TEN);
    expr_t *out = expr_pow_xp(ten, a);

    expr_free(ten);
    return out;
}

static expr_t *expr_inverse_sqrt_internal(const expr_t *a)
{
    return expr_pow(a, &NUM_TWO);
}

static expr_t *expr_inverse_lambert_internal(const expr_t *a)
{
    expr_t *exp_a = expr_exp(a);
    expr_t *out = exp_a ? expr_mul(a, exp_a) : NULL;

    expr_free(exp_a);
    return out;
}

const expr_ops_t ops_atan2 = {.eval = eval_atan2,
                              .deriv = deriv_atan2,
                              .reverse = expr_reverse_atan2,
                              .kind = EXPR_KIND_ATAN2,
                              .arity = EXPR_OP_BINARY,
                              .expression_name = "atan2",
                              .function_name = "atan2",
                              .TeX_name = "\\operatorname{atan2}",
                              .apply_unary = NULL,
                              .apply_binary = expr_atan2,
                              .simplify = expr_simplify_binary_operator,
                              .fold_const_unary = NULL};

const expr_ops_t ops_sin = {.eval = eval_sin,
                            .deriv = deriv_sin,
                            .reverse = expr_reverse_sin,
                            .kind = EXPR_KIND_SIN,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "sin",
                            .function_name = "sin",
                            .TeX_name = "\\sin",
                            .direct_inverse = &ops_asin,
                            .inverse_unary = expr_asin,
                            .apply_unary = expr_sin,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_sin_progression_closed_form,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_zero_to_zero};
const expr_ops_t ops_cos = {.eval = eval_cos,
                            .deriv = deriv_cos,
                            .reverse = expr_reverse_cos,
                            .kind = EXPR_KIND_COS,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "cos",
                            .function_name = "cos",
                            .TeX_name = "\\cos",
                            .direct_inverse = &ops_acos,
                            .inverse_unary = expr_acos,
                            .apply_unary = expr_cos,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_cos_progression_closed_form,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_cos_const};
const expr_ops_t ops_tan = {.eval = eval_tan,
                            .deriv = deriv_tan,
                            .reverse = expr_reverse_tan,
                            .kind = EXPR_KIND_TAN,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "tan",
                            .function_name = "tan",
                            .TeX_name = "\\tan",
                            .direct_inverse = &ops_atan,
                            .inverse_unary = expr_atan,
                            .apply_unary = expr_tan,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_tangent_progression_closed_form,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_zero_to_zero};
const expr_ops_t ops_sec = {.eval = eval_sec,
                            .deriv = deriv_sec,
                            .reverse = expr_reverse_sec,
                            .kind = EXPR_KIND_SEC,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "sec",
                            .function_name = "sec",
                            .TeX_name = "\\sec",
                            .direct_inverse = &ops_asec,
                            .inverse_unary = expr_asec,
                            .apply_unary = expr_sec,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_sec_progression_closed_form,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_cosec = {.eval = eval_cosec,
                              .deriv = deriv_cosec,
                              .reverse = expr_reverse_cosec,
                              .kind = EXPR_KIND_COSEC,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "cosec",
                              .function_name = "cosec",
                              .TeX_name = "\\operatorname{cosec}",
                              .direct_inverse = &ops_acosec,
                              .inverse_unary = expr_acosec,
                              .apply_unary = expr_cosec,
                              .apply_binary = NULL,
                              .finite_progression = expr_finite_cosec_progression_closed_form,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_cot = {.eval = eval_cot,
                            .deriv = deriv_cot,
                            .reverse = expr_reverse_cot,
                            .kind = EXPR_KIND_COT,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "cot",
                            .function_name = "cot",
                            .TeX_name = "\\cot",
                            .direct_inverse = &ops_acot,
                            .inverse_unary = expr_acot,
                            .apply_unary = expr_cot,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_cot_progression_closed_form,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_versin = {.eval = eval_versin,
                               .deriv = deriv_versin,
                               .reverse = expr_reverse_versin,
                               .kind = EXPR_KIND_VERSIN,
                               .arity = EXPR_OP_UNARY,
                               .expression_name = "versin",
                               .function_name = "versin",
                               .TeX_name = "\\operatorname{versin}",
                               .direct_inverse = &ops_arcversin,
                               .inverse_unary = expr_arcversin,
                               .apply_unary = expr_versin,
                               .apply_binary = NULL,
                               .finite_progression = expr_finite_versin_progression_closed_form,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_vercos = {.eval = eval_vercos,
                               .deriv = deriv_vercos,
                               .reverse = expr_reverse_vercos,
                               .kind = EXPR_KIND_VERCOS,
                               .arity = EXPR_OP_UNARY,
                               .expression_name = "vercos",
                               .function_name = "vercos",
                               .TeX_name = "\\operatorname{vercos}",
                               .direct_inverse = &ops_arcvercos,
                               .inverse_unary = expr_arcvercos,
                               .apply_unary = expr_vercos,
                               .apply_binary = NULL,
                               .finite_progression = expr_finite_vercos_progression_closed_form,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_coversin = {.eval = eval_coversin,
                                 .deriv = deriv_coversin,
                                 .reverse = expr_reverse_coversin,
                                 .kind = EXPR_KIND_COVERSIN,
                                 .arity = EXPR_OP_UNARY,
                                 .expression_name = "coversin",
                                 .function_name = "coversin",
                                 .TeX_name = "\\operatorname{coversin}",
                                 .direct_inverse = &ops_arccoversin,
                                 .inverse_unary = expr_arccoversin,
                                 .apply_unary = expr_coversin,
                                 .apply_binary = NULL,
                                 .finite_progression = expr_finite_coversin_progression_closed_form,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_covercos = {.eval = eval_covercos,
                                 .deriv = deriv_covercos,
                                 .reverse = expr_reverse_covercos,
                                 .kind = EXPR_KIND_COVERCOS,
                                 .arity = EXPR_OP_UNARY,
                                 .expression_name = "covercos",
                                 .function_name = "covercos",
                                 .TeX_name = "\\operatorname{covercos}",
                                 .direct_inverse = &ops_arccovercos,
                                 .inverse_unary = expr_arccovercos,
                                 .apply_unary = expr_covercos,
                                 .apply_binary = NULL,
                                 .finite_progression = expr_finite_covercos_progression_closed_form,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_haversin = {.eval = eval_haversin,
                                 .deriv = deriv_haversin,
                                 .reverse = expr_reverse_haversin,
                                 .kind = EXPR_KIND_HAVERSIN,
                                 .arity = EXPR_OP_UNARY,
                                 .expression_name = "haversin",
                                 .function_name = "haversin",
                                 .TeX_name = "\\operatorname{haversin}",
                                 .direct_inverse = &ops_archaversin,
                                 .inverse_unary = expr_archaversin,
                                 .apply_unary = expr_haversin,
                                 .apply_binary = NULL,
                                 .finite_progression = expr_finite_haversin_progression_closed_form,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_havercos = {.eval = eval_havercos,
                                 .deriv = deriv_havercos,
                                 .reverse = expr_reverse_havercos,
                                 .kind = EXPR_KIND_HAVERCOS,
                                 .arity = EXPR_OP_UNARY,
                                 .expression_name = "havercos",
                                 .function_name = "havercos",
                                 .TeX_name = "\\operatorname{havercos}",
                                 .direct_inverse = &ops_archavercos,
                                 .inverse_unary = expr_archavercos,
                                 .apply_unary = expr_havercos,
                                 .apply_binary = NULL,
                                 .finite_progression = expr_finite_havercos_progression_closed_form,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_hacoversin = {.eval = eval_hacoversin,
                                   .deriv = deriv_hacoversin,
                                   .reverse = expr_reverse_hacoversin,
                                   .kind = EXPR_KIND_HACOVERSIN,
                                   .arity = EXPR_OP_UNARY,
                                   .expression_name = "hacoversin",
                                   .function_name = "hacoversin",
                                   .TeX_name = "\\operatorname{hacoversin}",
                                   .direct_inverse = &ops_archacoversin,
                                   .inverse_unary = expr_archacoversin,
                                   .apply_unary = expr_hacoversin,
                                   .apply_binary = NULL,
                                   .finite_progression = expr_finite_hacoversin_progression_closed_form,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_hacovercos = {.eval = eval_hacovercos,
                                   .deriv = deriv_hacovercos,
                                   .reverse = expr_reverse_hacovercos,
                                   .kind = EXPR_KIND_HACOVERCOS,
                                   .arity = EXPR_OP_UNARY,
                                   .expression_name = "hacovercos",
                                   .function_name = "hacovercos",
                                   .TeX_name = "\\operatorname{hacovercos}",
                                   .direct_inverse = &ops_archacovercos,
                                   .inverse_unary = expr_archacovercos,
                                   .apply_unary = expr_hacovercos,
                                   .apply_binary = NULL,
                                   .finite_progression = expr_finite_hacovercos_progression_closed_form,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_sinh = {.eval = eval_sinh,
                             .deriv = deriv_sinh,
                             .reverse = expr_reverse_sinh,
                             .kind = EXPR_KIND_SINH,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "sinh",
                             .function_name = "sinh",
                             .TeX_name = "\\sinh",
                             .direct_inverse = &ops_asinh,
                             .inverse_unary = expr_asinh,
                             .apply_unary = expr_sinh,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_sinh_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_cosh = {.eval = eval_cosh,
                             .deriv = deriv_cosh,
                             .reverse = expr_reverse_cosh,
                             .kind = EXPR_KIND_COSH,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "cosh",
                             .function_name = "cosh",
                             .TeX_name = "\\cosh",
                             .direct_inverse = &ops_acosh,
                             .inverse_unary = expr_acosh,
                             .apply_unary = expr_cosh,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_cosh_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_tanh = {.eval = eval_tanh,
                             .deriv = deriv_tanh,
                             .reverse = expr_reverse_tanh,
                             .kind = EXPR_KIND_TANH,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "tanh",
                             .function_name = "tanh",
                             .TeX_name = "\\tanh",
                             .direct_inverse = &ops_atanh,
                             .inverse_unary = expr_atanh,
                             .apply_unary = expr_tanh,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_tanh_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_sech = {.eval = eval_sech,
                             .deriv = deriv_sech,
                             .reverse = expr_reverse_sech,
                             .kind = EXPR_KIND_SECH,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "sech",
                             .function_name = "sech",
                             .TeX_name = "\\operatorname{sech}",
                             .direct_inverse = &ops_asech,
                             .inverse_unary = expr_asech,
                             .apply_unary = expr_sech,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_sech_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_cosech = {.eval = eval_cosech,
                               .deriv = deriv_cosech,
                               .reverse = expr_reverse_cosech,
                               .kind = EXPR_KIND_COSECH,
                               .arity = EXPR_OP_UNARY,
                               .expression_name = "cosech",
                               .function_name = "cosech",
                               .TeX_name = "\\operatorname{cosech}",
                               .direct_inverse = &ops_acosech,
                               .inverse_unary = expr_acosech,
                               .apply_unary = expr_cosech,
                               .apply_binary = NULL,
                               .finite_progression = expr_finite_cosech_progression_closed_form,
                               .integrate = expr_integrate_dispatch_primitive,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_coth = {.eval = eval_coth,
                             .deriv = deriv_coth,
                             .reverse = expr_reverse_coth,
                             .kind = EXPR_KIND_COTH,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "coth",
                             .function_name = "coth",
                             .TeX_name = "\\coth",
                             .direct_inverse = &ops_acoth,
                             .inverse_unary = expr_acoth,
                             .apply_unary = expr_coth,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_coth_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_asin = {.eval = eval_asin,
                             .deriv = deriv_asin,
                             .reverse = expr_reverse_asin,
                             .kind = EXPR_KIND_ASIN,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "asin",
                             .function_name = "asin",
                             .TeX_name = "\\sin^{-1}",
                             .inverse_unary = expr_sin,
                             .apply_unary = expr_asin,
                             .apply_binary = NULL,
                             .finite_progression_term_eval = num_asin,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_asin_const};
const expr_ops_t ops_acos = {.eval = eval_acos,
                             .deriv = deriv_acos,
                             .reverse = expr_reverse_acos,
                             .kind = EXPR_KIND_ACOS,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "acos",
                             .function_name = "acos",
                             .TeX_name = "\\cos^{-1}",
                             .inverse_unary = expr_cos,
                             .apply_unary = expr_acos,
                             .apply_binary = NULL,
                             .finite_progression_term_eval = num_acos,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_acos_const};
const expr_ops_t ops_atan = {.eval = eval_atan,
                             .deriv = deriv_atan,
                             .reverse = expr_reverse_atan,
                             .kind = EXPR_KIND_ATAN,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "atan",
                             .function_name = "atan",
                             .TeX_name = "\\arctan",
                             .inverse_unary = expr_tan,
                             .apply_unary = expr_atan,
                             .apply_binary = NULL,
                             .finite_progression_term_eval = num_atan,
                             .finite_progression = expr_finite_atan_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_atan_const};
const expr_ops_t ops_asec = {.eval = eval_asec,
                             .deriv = deriv_asec,
                             .reverse = expr_reverse_asec,
                             .kind = EXPR_KIND_ASEC,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "asec",
                             .function_name = "asec",
                             .TeX_name = "\\sec^{-1}",
                             .inverse_unary = expr_sec,
                             .apply_unary = expr_asec,
                             .apply_binary = NULL,
                             .finite_progression_term_eval = num_asec,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_asec_const};
const expr_ops_t ops_acosec = {.eval = eval_acosec,
                               .deriv = deriv_acosec,
                               .reverse = expr_reverse_acosec,
                               .kind = EXPR_KIND_ACOSEC,
                               .arity = EXPR_OP_UNARY,
                               .expression_name = "acosec",
                               .function_name = "acosec",
                               .TeX_name = "\\operatorname{cosec}^{-1}",
                               .inverse_unary = expr_cosec,
                               .apply_unary = expr_acosec,
                               .apply_binary = NULL,
                               .finite_progression_term_eval = num_acosec,
                               .integrate = expr_integrate_dispatch_primitive,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = expr_fold_acosec_const};
const expr_ops_t ops_acot = {.eval = eval_acot,
                             .deriv = deriv_acot,
                             .reverse = expr_reverse_acot,
                             .kind = EXPR_KIND_ACOT,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "acot",
                             .function_name = "acot",
                             .TeX_name = "\\cot^{-1}",
                             .inverse_unary = expr_cot,
                             .apply_unary = expr_acot,
                             .apply_binary = NULL,
                             .finite_progression_term_eval = num_acot,
                             .finite_progression = expr_finite_acot_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_acot_const};
const expr_ops_t ops_arcversin = {.eval = eval_arcversin,
                                  .deriv = deriv_arcversin,
                                  .reverse = expr_reverse_arcversin,
                                  .kind = EXPR_KIND_ARCVERSIN,
                                  .arity = EXPR_OP_UNARY,
                                  .expression_name = "arcversin",
                                  .function_name = "arcversin",
                                  .TeX_name = "\\operatorname{arcversin}",
                                  .inverse_unary = expr_versin,
                                  .apply_unary = expr_arcversin,
                                  .apply_binary = NULL,
                                  .finite_progression_term_eval = num_arcversin,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_arcvercos = {.eval = eval_arcvercos,
                                  .deriv = deriv_arcvercos,
                                  .reverse = expr_reverse_arcvercos,
                                  .kind = EXPR_KIND_ARCVERCOS,
                                  .arity = EXPR_OP_UNARY,
                                  .expression_name = "arcvercos",
                                  .function_name = "arcvercos",
                                  .TeX_name = "\\operatorname{arcvercos}",
                                  .inverse_unary = expr_vercos,
                                  .apply_unary = expr_arcvercos,
                                  .apply_binary = NULL,
                                  .finite_progression_term_eval = num_arcvercos,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_arccoversin = {.eval = eval_arccoversin,
                                    .deriv = deriv_arccoversin,
                                    .reverse = expr_reverse_arccoversin,
                                    .kind = EXPR_KIND_ARCCOVERSIN,
                                    .arity = EXPR_OP_UNARY,
                                    .expression_name = "arccoversin",
                                    .function_name = "arccoversin",
                                    .TeX_name = "\\operatorname{arccoversin}",
                                    .inverse_unary = expr_coversin,
                                    .apply_unary = expr_arccoversin,
                                    .apply_binary = NULL,
                                    .finite_progression_term_eval = num_arccoversin,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_arccovercos = {.eval = eval_arccovercos,
                                    .deriv = deriv_arccovercos,
                                    .reverse = expr_reverse_arccovercos,
                                    .kind = EXPR_KIND_ARCCOVERCOS,
                                    .arity = EXPR_OP_UNARY,
                                    .expression_name = "arccovercos",
                                    .function_name = "arccovercos",
                                    .TeX_name = "\\operatorname{arccovercos}",
                                    .inverse_unary = expr_covercos,
                                    .apply_unary = expr_arccovercos,
                                    .apply_binary = NULL,
                                    .finite_progression_term_eval = num_arccovercos,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_archaversin = {.eval = eval_archaversin,
                                    .deriv = deriv_archaversin,
                                    .reverse = expr_reverse_archaversin,
                                    .kind = EXPR_KIND_ARCHAVERSIN,
                                    .arity = EXPR_OP_UNARY,
                                    .expression_name = "archaversin",
                                    .function_name = "archaversin",
                                    .TeX_name = "\\operatorname{archaversin}",
                                    .inverse_unary = expr_haversin,
                                    .apply_unary = expr_archaversin,
                                    .apply_binary = NULL,
                                    .finite_progression_term_eval = num_archaversin,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_archavercos = {.eval = eval_archavercos,
                                    .deriv = deriv_archavercos,
                                    .reverse = expr_reverse_archavercos,
                                    .kind = EXPR_KIND_ARCHAVERCOS,
                                    .arity = EXPR_OP_UNARY,
                                    .expression_name = "archavercos",
                                    .function_name = "archavercos",
                                    .TeX_name = "\\operatorname{archavercos}",
                                    .inverse_unary = expr_havercos,
                                    .apply_unary = expr_archavercos,
                                    .apply_binary = NULL,
                                    .finite_progression_term_eval = num_archavercos,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_archacoversin = {.eval = eval_archacoversin,
                                      .deriv = deriv_archacoversin,
                                      .reverse = expr_reverse_archacoversin,
                                      .kind = EXPR_KIND_ARCHACOVERSIN,
                                      .arity = EXPR_OP_UNARY,
                                      .expression_name = "archacoversin",
                                      .function_name = "archacoversin",
                                      .TeX_name = "\\operatorname{archacoversin}",
                                      .inverse_unary = expr_hacoversin,
                                      .apply_unary = expr_archacoversin,
                                      .apply_binary = NULL,
                                      .finite_progression_term_eval = num_archacoversin,
                                      .simplify = expr_simplify_unary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_archacovercos = {.eval = eval_archacovercos,
                                      .deriv = deriv_archacovercos,
                                      .reverse = expr_reverse_archacovercos,
                                      .kind = EXPR_KIND_ARCHACOVERCOS,
                                      .arity = EXPR_OP_UNARY,
                                      .expression_name = "archacovercos",
                                      .function_name = "archacovercos",
                                      .TeX_name = "\\operatorname{archacovercos}",
                                      .inverse_unary = expr_hacovercos,
                                      .apply_unary = expr_archacovercos,
                                      .apply_binary = NULL,
                                      .finite_progression_term_eval = num_archacovercos,
                                      .simplify = expr_simplify_unary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_asinh = {.eval = eval_asinh,
                              .deriv = deriv_asinh,
                              .reverse = expr_reverse_asinh,
                              .kind = EXPR_KIND_ASINH,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "asinh",
                              .function_name = "asinh",
                              .TeX_name = "\\sinh^{-1}",
                              .inverse_unary = expr_sinh,
                              .apply_unary = expr_asinh,
                              .apply_binary = NULL,
                              .finite_progression_term_eval = num_asinh,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_acosh = {.eval = eval_acosh,
                              .deriv = deriv_acosh,
                              .reverse = expr_reverse_acosh,
                              .kind = EXPR_KIND_ACOSH,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "acosh",
                              .function_name = "acosh",
                              .TeX_name = "\\cosh^{-1}",
                              .inverse_unary = expr_cosh,
                              .apply_unary = expr_acosh,
                              .apply_binary = NULL,
                              .finite_progression_term_eval = num_acosh,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_atanh = {.eval = eval_atanh,
                              .deriv = deriv_atanh,
                              .reverse = expr_reverse_atanh,
                              .kind = EXPR_KIND_ATANH,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "atanh",
                              .function_name = "atanh",
                              .TeX_name = "\\tanh^{-1}",
                              .inverse_unary = expr_tanh,
                              .apply_unary = expr_atanh,
                              .apply_binary = NULL,
                              .finite_progression_term_eval = num_atanh,
                              .finite_progression = expr_finite_atanh_progression_closed_form,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_asech = {.eval = eval_asech,
                              .deriv = deriv_asech,
                              .reverse = expr_reverse_asech,
                              .kind = EXPR_KIND_ASECH,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "asech",
                              .function_name = "asech",
                              .TeX_name = "\\operatorname{sech}^{-1}",
                              .inverse_unary = expr_sech,
                              .apply_unary = expr_asech,
                              .apply_binary = NULL,
                              .finite_progression_term_eval = num_asech,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_acosech = {.eval = eval_acosech,
                                .deriv = deriv_acosech,
                                .reverse = expr_reverse_acosech,
                                .kind = EXPR_KIND_ACOSECH,
                                .arity = EXPR_OP_UNARY,
                                .expression_name = "acosech",
                                .function_name = "acosech",
                                .TeX_name = "\\operatorname{cosech}^{-1}",
                                .inverse_unary = expr_cosech,
                                .apply_unary = expr_acosech,
                                .apply_binary = NULL,
                                .finite_progression_term_eval = num_acosech,
                                .integrate = expr_integrate_dispatch_primitive,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_acoth = {.eval = eval_acoth,
                              .deriv = deriv_acoth,
                              .reverse = expr_reverse_acoth,
                              .kind = EXPR_KIND_ACOTH,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "acoth",
                              .function_name = "acoth",
                              .TeX_name = "\\coth^{-1}",
                              .inverse_unary = expr_coth,
                              .apply_unary = expr_acoth,
                              .apply_binary = NULL,
                              .finite_progression_term_eval = num_acoth,
                              .finite_progression = expr_finite_acoth_progression_closed_form,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_exp = {.eval = eval_exp,
                            .deriv = deriv_exp,
                            .reverse = expr_reverse_exp,
                            .kind = EXPR_KIND_EXP,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "exp",
                            .function_name = "exp",
                            .TeX_name = "\\exp",
                            .direct_inverse = &ops_log,
                            .inverse_unary = expr_log,
                            .apply_unary = expr_exp,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_exponential_progression_closed_form,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_exp_const};
const expr_ops_t ops_log = {.eval = eval_log,
                            .deriv = deriv_log,
                            .reverse = expr_reverse_log,
                            .kind = EXPR_KIND_LOG,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "ln",
                            .function_name = "ln",
                            .TeX_name = "\\ln",
                            .direct_inverse = &ops_exp,
                            .inverse_unary = expr_exp,
                            .apply_unary = expr_log,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_logarithmic_progression_closed_form,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_log_const};
const expr_ops_t ops_log10 = {.eval = eval_log10,
                              .deriv = deriv_log10,
                              .reverse = expr_reverse_log10,
                              .kind = EXPR_KIND_LOG10,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "lg",
                              .function_name = "lg",
                              .TeX_name = "\\lg",
                              .inverse_unary = expr_inverse_log10_internal,
                              .apply_unary = expr_log10,
                              .apply_binary = NULL,
                              .finite_progression = expr_finite_common_logarithmic_progression_closed_form,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_sqrt = {.eval = eval_sqrt,
                             .deriv = deriv_sqrt,
                             .reverse = expr_reverse_sqrt,
                             .kind = EXPR_KIND_SQRT,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "sqrt",
                             .function_name = "sqrt",
                             .TeX_name = "\\sqrt",
                             .inverse_unary = expr_inverse_sqrt_internal,
                             .apply_unary = expr_sqrt,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_sqrt_progression_closed_form,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_sqrt_const};
const expr_ops_t ops_cubrt = {.eval = eval_cubrt,
                              .deriv = deriv_cubrt,
                              .reverse = expr_reverse_cubrt,
                              .kind = EXPR_KIND_CUBRT,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "cubrt",
                              .function_name = "cubrt",
                              .TeX_name = "\\sqrt[3]",
                              .apply_unary = expr_cubrt,
                              .apply_binary = NULL,
                              .finite_progression = expr_finite_cubrt_progression_closed_form,
                              .integrate = expr_integrate_dispatch_primitive,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = expr_fold_cubrt_const};
const expr_ops_t ops_root = {.eval = eval_root,
                             .deriv = deriv_root,
                             .reverse = expr_reverse_root,
                             .kind = EXPR_KIND_ROOT,
                             .arity = EXPR_OP_BINARY,
                             .expression_name = "root",
                             .function_name = "root",
                             .TeX_name = "\\sqrt",
                             .apply_unary = NULL,
                             .apply_binary = expr_root,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_root_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_floor = {.eval = eval_floor,
                              .deriv = deriv_floor,
                              .reverse = expr_reverse_floor,
                              .kind = EXPR_KIND_FLOOR,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "floor",
                              .function_name = "floor",
                              .TeX_name = "\\lfloor",
                              .apply_unary = expr_floor,
                              .apply_binary = NULL,
                              .finite_progression = expr_finite_floor_progression_closed_form,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = expr_fold_floor_const};
const expr_ops_t ops_ceil = {.eval = eval_ceil,
                             .deriv = deriv_ceil,
                             .reverse = expr_reverse_ceil,
                             .kind = EXPR_KIND_CEIL,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "ceil",
                             .function_name = "ceil",
                             .TeX_name = "\\lceil",
                             .apply_unary = expr_ceil,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_ceil_progression_closed_form,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_abs = {.eval = eval_abs,
                            .deriv = deriv_abs,
                            .reverse = expr_reverse_abs,
                            .kind = EXPR_KIND_ABS,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "abs",
                            .function_name = "abs",
                            .TeX_name = NULL,
                            .apply_unary = expr_abs,
                            .apply_binary = NULL,
                            .finite_progression = expr_finite_abs_progression_closed_form,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_conj = {.eval = eval_conj,
                             .deriv = deriv_conj,
                             .reverse = expr_reverse_conj,
                             .kind = EXPR_KIND_CONJ,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "conj",
                             .function_name = "conj",
                             .TeX_name = "\\operatorname{conj}",
                             .direct_inverse = &ops_conj,
                             .inverse_unary = expr_conj,
                             .apply_unary = expr_conj,
                             .apply_binary = NULL,
                             .finite_progression = expr_finite_conj_progression_closed_form,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_erf = {.eval = eval_erf,
                            .deriv = deriv_erf,
                            .reverse = expr_reverse_erf,
                            .kind = EXPR_KIND_ERF,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "erf",
                            .function_name = "erf",
                            .TeX_name = "\\operatorname{erf}",
                            .direct_inverse = &ops_erfinv,
                            .inverse_unary = expr_erfinv,
                            .apply_unary = expr_erf,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = expr_fold_erf_const};
const expr_ops_t ops_erfc = {.eval = eval_erfc,
                             .deriv = deriv_erfc,
                             .reverse = expr_reverse_erfc,
                             .kind = EXPR_KIND_ERFC,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "erfc",
                             .function_name = "erfc",
                             .TeX_name = "\\operatorname{erfc}",
                             .direct_inverse = &ops_erfcinv,
                             .inverse_unary = expr_erfcinv,
                             .apply_unary = expr_erfc,
                             .apply_binary = NULL,
                             .integrate = expr_integrate_dispatch_primitive,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = expr_fold_erfc_const};
const expr_ops_t ops_lgamma = {.eval = eval_lgamma,
                               .deriv = deriv_lgamma,
                               .reverse = expr_reverse_lgamma,
                               .kind = EXPR_KIND_LGAMMA,
                               .arity = EXPR_OP_UNARY,
                               .expression_name = "lnΓ",
                               .function_name = "lgamma",
                               .TeX_name = "\\ln\\Gamma",
                               .apply_unary = expr_lgamma,
                               .apply_binary = NULL,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_hypot = {.eval = eval_hypot,
                              .deriv = deriv_hypot,
                              .reverse = expr_reverse_hypot,
                              .kind = EXPR_KIND_HYPOT,
                              .arity = EXPR_OP_BINARY,
                              .expression_name = "hypot",
                              .function_name = "hypot",
                              .TeX_name = "\\operatorname{hypot}",
                              .apply_unary = NULL,
                              .apply_binary = expr_hypot,
                              .simplify = expr_simplify_hypot_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_erfinv = {.eval = eval_erfinv,
                               .deriv = deriv_erfinv,
                               .reverse = expr_reverse_erfinv,
                               .kind = EXPR_KIND_ERFINV,
                               .arity = EXPR_OP_UNARY,
                               .expression_name = "erfinv",
                               .function_name = "erfinv",
                               .TeX_name = "\\operatorname{erf}^{-1}",
                               .inverse_unary = expr_erf,
                               .apply_unary = expr_erfinv,
                               .apply_binary = NULL,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_erfcinv = {.eval = eval_erfcinv,
                                .deriv = deriv_erfcinv,
                                .reverse = expr_reverse_erfcinv,
                                .kind = EXPR_KIND_ERFCINV,
                                .arity = EXPR_OP_UNARY,
                                .expression_name = "erfcinv",
                                .function_name = "erfcinv",
                                .TeX_name = "\\operatorname{erfc}^{-1}",
                                .inverse_unary = expr_erfc,
                                .apply_unary = expr_erfcinv,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_gamma = {.eval = eval_gamma,
                              .deriv = deriv_gamma,
                              .reverse = expr_reverse_gamma,
                              .kind = EXPR_KIND_GAMMA,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "Γ",
                              .function_name = "gamma",
                              .TeX_name = "\\Gamma",
                              .direct_inverse = &ops_gammainv,
                              .inverse_unary = expr_gammainv,
                              .apply_unary = expr_gamma,
                              .apply_binary = NULL,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_digamma = {.eval = eval_digamma,
                                .deriv = deriv_digamma,
                                .reverse = expr_reverse_digamma,
                                .kind = EXPR_KIND_DIGAMMA,
                                .arity = EXPR_OP_UNARY,
                                .expression_name = "ψ⁽⁰⁾",
                                .function_name = "digamma",
                                .TeX_name = "\\psi^{(0)}",
                                .apply_unary = expr_digamma,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_qdigamma = {.eval = eval_qdigamma,
                                 .deriv = deriv_qdigamma,
                                 .reverse = expr_reverse_qdigamma,
                                 .kind = EXPR_KIND_QDIGAMMA,
                                 .arity = EXPR_OP_BINARY,
                                 .expression_name = "ψq",
                                 .function_name = "qdigamma",
                                 .TeX_name = "\\psi",
                                 .apply_unary = NULL,
                                 .apply_binary = expr_qdigamma,
                                 .integrate = expr_integral,
                                 .simplify = expr_simplify_binary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_trigamma = {.eval = eval_trigamma,
                                 .deriv = deriv_trigamma,
                                 .reverse = expr_reverse_trigamma,
                                 .kind = EXPR_KIND_TRIGAMMA,
                                 .arity = EXPR_OP_UNARY,
                                 .expression_name = "ψ⁽¹⁾",
                                 .function_name = "trigamma",
                                 .TeX_name = "\\psi^{(1)}",
                                 .apply_unary = expr_trigamma,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = expr_fold_trigamma_const};
const expr_ops_t ops_polygamma = {.eval = eval_polygamma,
                                  .deriv = deriv_polygamma,
                                  .reverse = expr_reverse_polygamma,
                                  .kind = EXPR_KIND_POLYGAMMA,
                                  .arity = EXPR_OP_BINARY,
                                  .expression_name = "ψ",
                                  .function_name = "polygamma",
                                  .TeX_name = "\\psi",
                                  .apply_unary = NULL,
                                  .apply_binary = expr_polygamma_xp,
                                  .simplify = expr_simplify_binary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_zeta = {.eval = eval_zeta,
                             .deriv = deriv_zeta,
                             .reverse = expr_reverse_zeta,
                             .kind = EXPR_KIND_ZETA,
                             .arity = EXPR_OP_UNARY,
                             .expression_name = "ζ",
                             .function_name = "zeta",
                             .TeX_name = "\\zeta",
                             .apply_unary = expr_zeta,
                             .apply_binary = NULL,
                             .integrate = integrate_zeta_formal,
                             .simplify = expr_simplify_unary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_zetap = {.eval = eval_zetap,
                              .deriv = deriv_zetap,
                              .kind = EXPR_KIND_ZETAP,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "ζ'",
                              .function_name = "zetap",
                              .TeX_name = "\\zeta'",
                              .apply_unary = expr_zetap,
                              .apply_binary = NULL,
                              .integrate = integrate_zetap,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_zetah = {.eval = eval_zetah,
                              .deriv = deriv_zetah,
                              .kind = EXPR_KIND_ZETAH,
                              .arity = EXPR_OP_BINARY,
                              .expression_name = "ζ",
                              .function_name = "zetah",
                              .TeX_name = "\\zeta",
                              .apply_unary = NULL,
                              .apply_binary = expr_zetah,
                              .integrate = integrate_zetah,
                              .simplify = expr_simplify_binary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_zatahp = {.eval = eval_zatahp,
                               .deriv = deriv_zatahp,
                               .kind = EXPR_KIND_ZATAHP,
                               .arity = EXPR_OP_BINARY,
                               .expression_name = "ζ'",
                               .function_name = "zatahp",
                               .TeX_name = "\\zeta'",
                               .apply_unary = NULL,
                               .apply_binary = expr_zatahp,
                               .integrate = integrate_zatahp,
                               .simplify = expr_simplify_binary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_dilog = {.eval = eval_dilog,
                              .deriv = deriv_dilog,
                              .reverse = expr_reverse_dilog,
                              .kind = EXPR_KIND_DILOG,
                              .arity = EXPR_OP_UNARY,
                              .expression_name = "dilog",
                              .function_name = "li2",
                              .TeX_name = "\\operatorname{Li}_{2}",
                              .apply_unary = expr_dilog,
                              .apply_binary = NULL,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_polylog1 = {.eval = eval_polylog1,
                                 .deriv = deriv_polylog1,
                                 .reverse = expr_reverse_polylog1,
                                 .kind = EXPR_KIND_POLYLOG1,
                                 .arity = EXPR_OP_UNARY,
                                 .expression_name = "Li1",
                                 .function_name = "li1",
                                 .TeX_name = "\\operatorname{Li}_{1}",
                                 .apply_unary = expr_polylog1,
                                 .apply_binary = NULL,
                                 .integrate = integrate_polylog1,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_polylog = {.eval = eval_polylog,
                                .deriv = deriv_polylog,
                                .reverse = expr_reverse_polylog,
                                .kind = EXPR_KIND_POLYLOG,
                                .arity = EXPR_OP_BINARY,
                                .expression_name = "polylog",
                                .function_name = "polylog",
                                .TeX_name = "\\operatorname{Li}",
                                .apply_unary = NULL,
                                .apply_binary = expr_polylog_xp,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_harmonic_poly = {.eval = eval_harmonic_poly,
                                      .deriv = deriv_harmonic_poly,
                                      .kind = EXPR_KIND_HARMONIC_POLY,
                                      .arity = EXPR_OP_BINARY,
                                      .expression_name = "Hn",
                                      .function_name = "harmonicpoly",
                                      .TeX_name = "H",
                                      .apply_unary = NULL,
                                      .apply_binary = expr_harmonic_poly,
                                      .integrate = integrate_harmonic_poly,
                                      .simplify = expr_simplify_binary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_lerch_phi = {.eval = eval_lerch_phi,
                                  .deriv = deriv_lerch_phi,
                                  .reverse_many = expr_reverse_lerch_phi_many,
                                  .kind = EXPR_KIND_LERCH_PHI,
                                  .arity = EXPR_OP_BINARY,
                                  .expression_name = "Φ",
                                  .function_name = "lerchphi",
                                  .TeX_name = "\\Phi",
                                  .apply_unary = NULL,
                                  .apply_binary = NULL,
                                  .integrate = expr_integrate_dispatch_primitive,
                                  .simplify = expr_simplify_rebuild_binary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_lerch_phi_pack = {.eval = eval_lerch_phi_pack,
                                       .deriv = deriv_lerch_phi_pack,
                                       .reverse = expr_reverse_parameter_pack,
                                       .kind = EXPR_KIND_LERCH_PHI_PACK,
                                       .arity = EXPR_OP_BINARY,
                                       .expression_name = "lerch_phi_pack",
                                       .function_name = "lerchphipack",
                                       .TeX_name = "\\operatorname{pack}",
                                       .apply_unary = NULL,
                                       .apply_binary = NULL,
                                       .simplify = expr_simplify_rebuild_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_legendre_chi = {.eval = eval_legendre_chi,
                                     .deriv = deriv_legendre_chi,
                                     .reverse = expr_reverse_legendre_chi,
                                     .kind = EXPR_KIND_LEGENDRE_CHI,
                                     .arity = EXPR_OP_BINARY,
                                     .expression_name = "legendre_chi",
                                     .function_name = "legendrechi",
                                     .TeX_name = "\\chi",
                                     .apply_unary = NULL,
                                     .apply_binary = expr_legendre_chi_xp,
                                     .simplify = expr_simplify_binary_operator,
                                     .fold_const_unary = NULL};
const expr_ops_t ops_bessel_j = {.eval = eval_bessel_j,
                                 .deriv = deriv_bessel_j,
                                 .reverse = expr_reverse_bessel_j,
                                 .kind = EXPR_KIND_BESSEL_J,
                                 .arity = EXPR_OP_BINARY,
                                 .expression_name = "BesselJ",
                                 .function_name = "besselj",
                                 .TeX_name = "J",
                                 .apply_unary = NULL,
                                 .apply_binary = expr_bessel_j,
                                 .integrate = expr_integrate_dispatch_primitive,
                                 .simplify = expr_simplify_binary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_bessel_y = {.eval = eval_bessel_y,
                                 .deriv = deriv_bessel_y,
                                 .reverse = expr_reverse_bessel_y,
                                 .kind = EXPR_KIND_BESSEL_Y,
                                 .arity = EXPR_OP_BINARY,
                                 .expression_name = "BesselY",
                                 .function_name = "bessely",
                                 .TeX_name = "Y",
                                 .apply_unary = NULL,
                                 .apply_binary = expr_bessel_y,
                                 .integrate = expr_integrate_dispatch_primitive,
                                 .simplify = expr_simplify_binary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_lommel_s = {.eval = eval_lommel_s,
                                 .deriv = deriv_lommel_s,
                                 .reverse = expr_reverse_lommel_s,
                                 .kind = EXPR_KIND_LOMMEL_S,
                                 .arity = EXPR_OP_BINARY,
                                 .expression_name = "LommelS",
                                 .function_name = "lommels",
                                 .TeX_name = "s",
                                 .apply_unary = NULL,
                                 .apply_binary = expr_lommel_s_from_pack,
                                 .integrate = expr_integrate_dispatch_primitive,
                                 .simplify = expr_simplify_rebuild_binary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_lommel_s_pack = {.eval = eval_lommel_s_pack,
                                      .deriv = deriv_lommel_s_pack,
                                      .reverse = expr_reverse_parameter_pack,
                                      .kind = EXPR_KIND_LOMMEL_S_PACK,
                                      .arity = EXPR_OP_BINARY,
                                      .diff_kind = EXPR_DIFF_NONE,
                                      .expression_name = "lommel_s_pack",
                                      .function_name = "lommelspack",
                                      .TeX_name = "\\operatorname{pack}",
                                      .apply_unary = NULL,
                                      .apply_binary = expr_lommel_s_pack,
                                      .simplify = expr_simplify_rebuild_binary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_appell_f1 = {.eval = eval_appell_f1,
                                  .deriv = deriv_appell_f1,
                                  .reverse = expr_reverse_not_differentiable,
                                  .reverse_many = expr_reverse_appell_f1_many,
                                  .kind = EXPR_KIND_APPELL_F1,
                                  .arity = EXPR_OP_BINARY,
                                  .expression_name = "F₁",
                                  .function_name = "appellf1",
                                  .TeX_name = "\\operatorname{F}_{1}",
                                  .apply_unary = NULL,
                                  .apply_binary = expr_appell_f1_from_packs,
                                  .simplify = expr_simplify_rebuild_binary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_appell_f1_pack = {.eval = eval_appell_f1_pack,
                                       .deriv = deriv_appell_f1_pack,
                                       .reverse = expr_reverse_parameter_pack,
                                       .kind = EXPR_KIND_APPELL_F1_PACK,
                                       .arity = EXPR_OP_BINARY,
                                       .diff_kind = EXPR_DIFF_NONE,
                                       .expression_name = "appell_f1_pack",
                                       .function_name = "appellf1pack",
                                       .TeX_name = "\\operatorname{pack}",
                                       .apply_unary = NULL,
                                       .apply_binary = expr_appell_f1_pack,
                                       .simplify = expr_simplify_rebuild_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_lauricella_f = {.eval = eval_lauricella_f,
                                     .deriv = deriv_lauricella_f,
                                     .reverse = expr_reverse_not_differentiable,
                                     .reverse_many = expr_reverse_lauricella_f_many,
                                     .kind = EXPR_KIND_LAURICELLA_F,
                                     .arity = EXPR_OP_BINARY,
                                     .expression_name = "LauricellaF",
                                     .function_name = "lauricellaf",
                                     .TeX_name = "F_D",
                                     .apply_unary = NULL,
                                     .apply_binary = expr_lauricella_f_from_packs,
                                     .simplify = expr_simplify_rebuild_binary_operator,
                                     .fold_const_unary = NULL};
const expr_ops_t ops_hypergeometric_pFq = {.eval = eval_hypergeometric_pFq,
                                           .deriv = deriv_hypergeometric_pFq,
                                           .reverse = expr_reverse_hypergeometric_pFq,
                                           .kind = EXPR_KIND_HYPERGEOMETRIC_PFQ,
                                           .arity = EXPR_OP_BINARY,
                                           .expression_name = "HypergeometricpFq",
                                           .function_name = "hypergeometricpfq",
                                           .TeX_name = "F",
                                           .apply_unary = NULL,
                                           .apply_binary = expr_hypergeometric_pFq_from_pack,
                                           .integrate = expr_integrate_dispatch_primitive,
                                           .simplify = expr_simplify_rebuild_binary_operator,
                                           .fold_const_unary = NULL};
const expr_ops_t ops_hypergeometric_pFq_pack = {.eval = eval_hypergeometric_pFq_pack,
                                                .deriv = deriv_hypergeometric_pFq_pack,
                                                .reverse = expr_reverse_parameter_pack,
                                                .kind = EXPR_KIND_HYPERGEOMETRIC_PFQ_PACK,
                                                .arity = EXPR_OP_BINARY,
                                                .diff_kind = EXPR_DIFF_NONE,
                                                .expression_name = "hypergeometric_pFq_pack",
                                                .function_name = "hypergeometricpfqpack",
                                                .TeX_name = "\\operatorname{pack}",
                                                .apply_unary = NULL,
                                                .apply_binary = expr_hypergeometric_pFq_pack,
                                                .simplify = expr_simplify_rebuild_binary_operator,
                                                .fold_const_unary = NULL};
const expr_ops_t ops_gammainv = {.eval = eval_gammainv,
                                 .deriv = deriv_gammainv,
                                 .reverse = expr_reverse_gammainv,
                                 .kind = EXPR_KIND_GAMMAINV,
                                 .arity = EXPR_OP_UNARY,
                                 .expression_name = "gammainv",
                                 .function_name = "gammainv",
                                 .TeX_name = "\\Gamma^{-1}",
                                 .inverse_unary = expr_gamma,
                                 .apply_unary = expr_gammainv,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_lambert_w = {.eval = eval_lambert_w,
                                  .deriv = deriv_lambert_w,
                                  .reverse = expr_reverse_lambert_w,
                                  .kind = EXPR_KIND_LAMBERT_W,
                                  .arity = EXPR_OP_UNARY,
                                  .expression_name = "W",
                                  .function_name = "w",
                                  .TeX_name = "W",
                                  .inverse_unary = expr_inverse_lambert_internal,
                                  .apply_unary = expr_lambert_w,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_lambert_wn = {.eval = eval_lambert_wn,
                                   .deriv = deriv_lambert_wn,
                                   .reverse = expr_reverse_lambert_wn,
                                   .kind = EXPR_KIND_LAMBERT_WN,
                                   .arity = EXPR_OP_BINARY,
                                   .expression_name = "Wₙ",
                                   .function_name = "wn",
                                   .TeX_name = "W",
                                   .apply_unary = NULL,
                                   .apply_binary = expr_lambert_wn_xp,
                                   .simplify = expr_simplify_binary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_lambert_w0 = {.eval = eval_lambert_w0,
                                   .deriv = deriv_lambert_w0,
                                   .reverse = expr_reverse_lambert_w0,
                                   .kind = EXPR_KIND_LAMBERT_W0,
                                   .arity = EXPR_OP_UNARY,
                                   .expression_name = "W₀",
                                   .function_name = "w0",
                                   .TeX_name = "W_{0}",
                                   .inverse_unary = expr_inverse_lambert_internal,
                                   .apply_unary = expr_lambert_w0,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_lambert_wm1 = {.eval = eval_lambert_wm1,
                                    .deriv = deriv_lambert_wm1,
                                    .reverse = expr_reverse_lambert_wm1,
                                    .kind = EXPR_KIND_LAMBERT_WM1,
                                    .arity = EXPR_OP_UNARY,
                                    .expression_name = "W₋₁",
                                    .function_name = "wm1",
                                    .TeX_name = "W_{-1}",
                                    .inverse_unary = expr_inverse_lambert_internal,
                                    .apply_unary = expr_lambert_wm1,
                                    .apply_binary = NULL,
                                    .simplify = expr_simplify_unary_operator,
                                    .fold_const_unary = NULL};
const expr_ops_t ops_normal_pdf = {.eval = eval_normal_pdf,
                                   .deriv = deriv_normal_pdf,
                                   .reverse = expr_reverse_normal_pdf,
                                   .kind = EXPR_KIND_NORMAL_PDF,
                                   .arity = EXPR_OP_UNARY,
                                   .expression_name = "normal_pdf",
                                   .function_name = "normalpdf",
                                   .TeX_name = "\\operatorname{normal\\_pdf}",
                                   .apply_unary = expr_normal_pdf,
                                   .apply_binary = NULL,
                                   .integrate = expr_integrate_dispatch_primitive,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_normal_cdf = {.eval = eval_normal_cdf,
                                   .deriv = deriv_normal_cdf,
                                   .reverse = expr_reverse_normal_cdf,
                                   .kind = EXPR_KIND_NORMAL_CDF,
                                   .arity = EXPR_OP_UNARY,
                                   .expression_name = "normal_cdf",
                                   .function_name = "normalcdf",
                                   .TeX_name = "\\operatorname{normal\\_cdf}",
                                   .apply_unary = expr_normal_cdf,
                                   .apply_binary = NULL,
                                   .integrate = expr_integrate_dispatch_primitive,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_normal_logpdf = {.eval = eval_normal_logpdf,
                                      .deriv = deriv_normal_logpdf,
                                      .reverse = expr_reverse_normal_logpdf,
                                      .kind = EXPR_KIND_NORMAL_LOGPDF,
                                      .arity = EXPR_OP_UNARY,
                                      .expression_name = "normal_logpdf",
                                      .function_name = "normallogpdf",
                                      .TeX_name = "\\operatorname{normal\\_logpdf}",
                                      .apply_unary = expr_normal_logpdf,
                                      .apply_binary = NULL,
                                      .finite_progression = expr_finite_normal_logpdf_progression_closed_form,
                                      .integrate = expr_integrate_dispatch_primitive,
                                      .simplify = expr_simplify_unary_operator,
                                      .fold_const_unary = NULL};
const expr_ops_t ops_pdf = {.eval = eval_normal_pdf,
                            .deriv = deriv_pdf,
                            .reverse = expr_reverse_normal_pdf,
                            .kind = EXPR_KIND_NORMAL_PDF,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "pdf",
                            .function_name = "pdf",
                            .TeX_name = "\\operatorname{pdf}",
                            .apply_unary = expr_pdf,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_cdf = {.eval = eval_normal_cdf,
                            .deriv = deriv_cdf,
                            .reverse = expr_reverse_normal_cdf,
                            .kind = EXPR_KIND_NORMAL_CDF,
                            .arity = EXPR_OP_UNARY,
                            .expression_name = "cdf",
                            .function_name = "cdf",
                            .TeX_name = "\\operatorname{cdf}",
                            .apply_unary = expr_cdf,
                            .apply_binary = NULL,
                            .integrate = expr_integrate_dispatch_primitive,
                            .simplify = expr_simplify_unary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_logpdf = {.eval = eval_normal_logpdf,
                               .deriv = deriv_logpdf,
                               .reverse = expr_reverse_normal_logpdf,
                               .kind = EXPR_KIND_NORMAL_LOGPDF,
                               .arity = EXPR_OP_UNARY,
                               .expression_name = "logpdf",
                               .function_name = "logpdf",
                               .TeX_name = "\\operatorname{logpdf}",
                               .apply_unary = expr_logpdf,
                               .apply_binary = NULL,
                               .finite_progression = expr_finite_normal_logpdf_progression_closed_form,
                               .integrate = expr_integrate_dispatch_primitive,
                               .simplify = expr_simplify_unary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_Ei = {.eval = eval_Ei,
                           .deriv = deriv_Ei,
                           .reverse = expr_reverse_Ei,
                           .kind = EXPR_KIND_EI,
                           .arity = EXPR_OP_UNARY,
                           .expression_name = "Ei",
                           .function_name = "ei",
                           .TeX_name = "\\operatorname{Ei}",
                           .apply_unary = expr_Ei,
                           .apply_binary = NULL,
                           .integrate = expr_integrate_dispatch_primitive,
                           .simplify = expr_simplify_unary_operator,
                           .fold_const_unary = NULL};
const expr_ops_t ops_E1 = {.eval = eval_E1,
                           .deriv = deriv_E1,
                           .reverse = expr_reverse_E1,
                           .kind = EXPR_KIND_E1,
                           .arity = EXPR_OP_UNARY,
                           .expression_name = "E1",
                           .function_name = "e1",
                           .TeX_name = "\\operatorname{E1}",
                           .apply_unary = expr_E1,
                           .apply_binary = NULL,
                           .integrate = expr_integrate_dispatch_primitive,
                           .simplify = expr_simplify_unary_operator,
                           .fold_const_unary = NULL};
const expr_ops_t ops_beta = {.eval = eval_beta,
                             .deriv = deriv_beta,
                             .reverse = expr_reverse_beta,
                             .kind = EXPR_KIND_BETA,
                             .arity = EXPR_OP_BINARY,
                             .expression_name = "beta",
                             .function_name = "beta",
                             .TeX_name = "\\operatorname{beta}",
                             .apply_unary = NULL,
                             .apply_binary = expr_beta,
                             .simplify = expr_simplify_binary_operator,
                             .fold_const_unary = NULL};
const expr_ops_t ops_indexed_symbol = {.eval = eval_formal_series_component,
                                       .deriv = deriv_indexed_symbol,
                                       .reverse = expr_reverse_not_differentiable,
                                       .kind = EXPR_KIND_INDEXED_SYMBOL,
                                       .arity = EXPR_OP_BINARY,
                                       .expression_name = "indexed",
                                       .function_name = "indexed",
                                       .TeX_name = NULL,
                                       .apply_unary = NULL,
                                       .apply_binary = NULL,
                                       .simplify = expr_simplify_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_summation = {.eval = eval_finite_summation,
                                  .deriv = deriv_summation,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_SUMMATION,
                                  .arity = EXPR_OP_BINARY,
                                  .expression_name = "sum",
                                  .function_name = "sum",
                                  .TeX_name = NULL,
                                  .apply_unary = NULL,
                                  .apply_binary = NULL,
                                  .integrate = integrate_summation,
                                  .simplify = expr_simplify_summation_with_special_forms,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_product = {.eval = eval_finite_product,
                                .deriv = deriv_product,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_PRODUCT,
                                .arity = EXPR_OP_BINARY,
                                .expression_name = "product",
                                .function_name = "product",
                                .TeX_name = NULL,
                                .apply_unary = NULL,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_logbeta = {.eval = eval_logbeta,
                                .deriv = deriv_logbeta,
                                .reverse = expr_reverse_logbeta,
                                .kind = EXPR_KIND_LOGBETA,
                                .arity = EXPR_OP_BINARY,
                                .expression_name = "lnB",
                                .function_name = "logbeta",
                                .TeX_name = "\\ln B",
                                .apply_unary = NULL,
                                .apply_binary = expr_logbeta,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_lower = {.eval = eval_gammainc_lower,
                                       .deriv = deriv_gammainc_lower,
                                       .reverse = expr_reverse_gammainc_lower,
                                       .kind = EXPR_KIND_GAMMAINC_LOWER,
                                       .arity = EXPR_OP_BINARY,
                                       .expression_name = "gammainc_lower",
                                       .function_name = "gammainclower",
                                       .TeX_name = "\\gamma",
                                       .apply_unary = NULL,
                                       .apply_binary = expr_gammainc_lower,
                                       .simplify = expr_simplify_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_upper = {.eval = eval_gammainc_upper,
                                       .deriv = deriv_gammainc_upper,
                                       .reverse = expr_reverse_gammainc_upper,
                                       .kind = EXPR_KIND_GAMMAINC_UPPER,
                                       .arity = EXPR_OP_BINARY,
                                       .expression_name = "gammainc_upper",
                                       .function_name = "gammaincupper",
                                       .TeX_name = "\\Gamma",
                                       .apply_unary = NULL,
                                       .apply_binary = expr_gammainc_upper,
                                       .simplify = expr_simplify_binary_operator,
                                       .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_P = {.eval = eval_gammainc_P,
                                   .deriv = deriv_gammainc_P,
                                   .reverse = expr_reverse_gammainc_P,
                                   .kind = EXPR_KIND_GAMMAINC_P,
                                   .arity = EXPR_OP_BINARY,
                                   .expression_name = "gammainc_P",
                                   .function_name = "gammaincp",
                                   .TeX_name = "\\operatorname{P}",
                                   .apply_unary = NULL,
                                   .apply_binary = expr_gammainc_P,
                                   .simplify = expr_simplify_binary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_gammainc_Q = {.eval = eval_gammainc_Q,
                                   .deriv = deriv_gammainc_Q,
                                   .reverse = expr_reverse_gammainc_Q,
                                   .kind = EXPR_KIND_GAMMAINC_Q,
                                   .arity = EXPR_OP_BINARY,
                                   .expression_name = "gammainc_Q",
                                   .function_name = "gammaincq",
                                   .TeX_name = "\\operatorname{Q}",
                                   .apply_unary = NULL,
                                   .apply_binary = expr_gammainc_Q,
                                   .simplify = expr_simplify_binary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_factorial = {.eval = eval_factorial,
                                  .deriv = deriv_not_differentiable,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_FACTORIAL,
                                  .arity = EXPR_OP_UNARY,
                                  .diff_kind = EXPR_DIFF_NONE,
                                  .expression_name = "factorial",
                                  .function_name = "factorial",
                                  .TeX_name = "\\operatorname{factorial}",
                                  .apply_unary = expr_factorial,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_fibonacci = {.eval = eval_fibonacci,
                                  .deriv = deriv_not_differentiable,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_FIBONACCI,
                                  .arity = EXPR_OP_UNARY,
                                  .diff_kind = EXPR_DIFF_NONE,
                                  .expression_name = "fibonacci",
                                  .function_name = "fibonacci",
                                  .TeX_name = "\\operatorname{fibonacci}",
                                  .apply_unary = expr_fibonacci,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_partition = {.eval = eval_partition,
                                  .deriv = deriv_not_differentiable,
                                  .reverse = expr_reverse_not_differentiable,
                                  .kind = EXPR_KIND_PARTITION,
                                  .arity = EXPR_OP_UNARY,
                                  .diff_kind = EXPR_DIFF_NONE,
                                  .expression_name = "partition",
                                  .function_name = "partition",
                                  .TeX_name = "\\operatorname{partition}",
                                  .apply_unary = expr_partition,
                                  .apply_binary = NULL,
                                  .simplify = expr_simplify_unary_operator,
                                  .fold_const_unary = NULL};
const expr_ops_t ops_isqrt = {.eval = eval_isqrt,
                              .deriv = deriv_not_differentiable,
                              .reverse = expr_reverse_not_differentiable,
                              .kind = EXPR_KIND_ISQRT,
                              .arity = EXPR_OP_UNARY,
                              .diff_kind = EXPR_DIFF_NONE,
                              .expression_name = "isqrt",
                              .function_name = "isqrt",
                              .TeX_name = "\\operatorname{isqrt}",
                              .apply_unary = expr_isqrt,
                              .apply_binary = NULL,
                              .simplify = expr_simplify_unary_operator,
                              .fold_const_unary = NULL};
const expr_ops_t ops_gcd = {.eval = eval_gcd,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_GCD,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .expression_name = "gcd",
                            .function_name = "gcd",
                            .TeX_name = "\\gcd",
                            .apply_unary = NULL,
                            .apply_binary = expr_gcd,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_lcm = {.eval = eval_lcm,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_LCM,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .expression_name = "lcm",
                            .function_name = "lcm",
                            .TeX_name = "\\operatorname{lcm}",
                            .apply_unary = NULL,
                            .apply_binary = expr_lcm,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_mod = {.eval = eval_mod,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_MOD,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .expression_name = "mod",
                            .function_name = "mod",
                            .TeX_name = "\\operatorname{mod}",
                            .apply_unary = NULL,
                            .apply_binary = expr_mod,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_modinv = {.eval = eval_modinv,
                               .deriv = deriv_not_differentiable,
                               .reverse = expr_reverse_not_differentiable,
                               .kind = EXPR_KIND_MODINV,
                               .arity = EXPR_OP_BINARY,
                               .diff_kind = EXPR_DIFF_NONE,
                               .expression_name = "modinv",
                               .function_name = "modinv",
                               .TeX_name = "\\operatorname{modinv}",
                               .apply_unary = NULL,
                               .apply_binary = expr_modinv,
                               .simplify = expr_simplify_binary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_is_prime = {.eval = eval_is_prime,
                                 .deriv = deriv_not_differentiable,
                                 .reverse = expr_reverse_not_differentiable,
                                 .kind = EXPR_KIND_IS_PRIME,
                                 .arity = EXPR_OP_UNARY,
                                 .diff_kind = EXPR_DIFF_NONE,
                                 .expression_name = "is_prime",
                                 .function_name = "isprime",
                                 .TeX_name = "\\operatorname{is\\_prime}",
                                 .apply_unary = expr_is_prime,
                                 .apply_binary = NULL,
                                 .simplify = expr_simplify_unary_operator,
                                 .fold_const_unary = NULL};
const expr_ops_t ops_next_prime = {.eval = eval_next_prime,
                                   .deriv = deriv_not_differentiable,
                                   .reverse = expr_reverse_not_differentiable,
                                   .kind = EXPR_KIND_NEXT_PRIME,
                                   .arity = EXPR_OP_UNARY,
                                   .diff_kind = EXPR_DIFF_NONE,
                                   .expression_name = "next_prime",
                                   .function_name = "nextprime",
                                   .TeX_name = "\\operatorname{next\\_prime}",
                                   .apply_unary = expr_next_prime,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_prev_prime = {.eval = eval_prev_prime,
                                   .deriv = deriv_not_differentiable,
                                   .reverse = expr_reverse_not_differentiable,
                                   .kind = EXPR_KIND_PREV_PRIME,
                                   .arity = EXPR_OP_UNARY,
                                   .diff_kind = EXPR_DIFF_NONE,
                                   .expression_name = "prev_prime",
                                   .function_name = "prevprime",
                                   .TeX_name = "\\operatorname{prev\\_prime}",
                                   .apply_unary = expr_prev_prime,
                                   .apply_binary = NULL,
                                   .simplify = expr_simplify_unary_operator,
                                   .fold_const_unary = NULL};
const expr_ops_t ops_bit_and = {.eval = eval_bit_and,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_BIT_AND,
                                .arity = EXPR_OP_BINARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .expression_name = "AND",
                                .function_name = "and",
                                .TeX_name = "\\operatorname{AND}",
                                .apply_unary = NULL,
                                .apply_binary = expr_bit_and,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_bit_or = {.eval = eval_bit_or,
                               .deriv = deriv_not_differentiable,
                               .reverse = expr_reverse_not_differentiable,
                               .kind = EXPR_KIND_BIT_OR,
                               .arity = EXPR_OP_BINARY,
                               .diff_kind = EXPR_DIFF_NONE,
                               .expression_name = "OR",
                               .function_name = "or",
                               .TeX_name = "\\operatorname{OR}",
                               .apply_unary = NULL,
                               .apply_binary = expr_bit_or,
                               .simplify = expr_simplify_binary_operator,
                               .fold_const_unary = NULL};
const expr_ops_t ops_bit_xor = {.eval = eval_bit_xor,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_BIT_XOR,
                                .arity = EXPR_OP_BINARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .expression_name = "XOR",
                                .function_name = "xor",
                                .TeX_name = "\\operatorname{XOR}",
                                .apply_unary = NULL,
                                .apply_binary = expr_bit_xor,
                                .simplify = expr_simplify_binary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_bit_not = {.eval = eval_bit_not,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_BIT_NOT,
                                .arity = EXPR_OP_UNARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .expression_name = "NOT",
                                .function_name = "not",
                                .TeX_name = "\\operatorname{NOT}",
                                .apply_unary = expr_bit_not,
                                .apply_binary = NULL,
                                .finite_progression = expr_finite_bit_not_progression_closed_form,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};
const expr_ops_t ops_shl = {.eval = eval_shl,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_SHL,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .expression_name = "SHL",
                            .function_name = "shl",
                            .TeX_name = "\\operatorname{SHL}",
                            .apply_unary = NULL,
                            .apply_binary = expr_shl,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_shr = {.eval = eval_shr,
                            .deriv = deriv_not_differentiable,
                            .reverse = expr_reverse_not_differentiable,
                            .kind = EXPR_KIND_SHR,
                            .arity = EXPR_OP_BINARY,
                            .diff_kind = EXPR_DIFF_NONE,
                            .expression_name = "SHR",
                            .function_name = "shr",
                            .TeX_name = "\\operatorname{SHR}",
                            .apply_unary = NULL,
                            .apply_binary = expr_shr,
                            .simplify = expr_simplify_binary_operator,
                            .fold_const_unary = NULL};
const expr_ops_t ops_factors = {.eval = eval_factors,
                                .deriv = deriv_not_differentiable,
                                .reverse = expr_reverse_not_differentiable,
                                .kind = EXPR_KIND_FACTORS,
                                .arity = EXPR_OP_UNARY,
                                .diff_kind = EXPR_DIFF_NONE,
                                .expression_name = "factors",
                                .function_name = "factors",
                                .TeX_name = "\\operatorname{factors}",
                                .apply_unary = expr_factors,
                                .apply_binary = NULL,
                                .simplify = expr_simplify_unary_operator,
                                .fold_const_unary = NULL};

expr_t *expr_apply_unary_kind(expr_op_kind_t kind, const expr_t *arg)
{
    static const expr_ops_t *const unary_ops_by_kind[EXPR_KIND_COUNT] = {
        [EXPR_KIND_NEG] = &ops_neg,
        [EXPR_KIND_SIN] = &ops_sin,
        [EXPR_KIND_COS] = &ops_cos,
        [EXPR_KIND_TAN] = &ops_tan,
        [EXPR_KIND_SEC] = &ops_sec,
        [EXPR_KIND_COSEC] = &ops_cosec,
        [EXPR_KIND_COT] = &ops_cot,
        [EXPR_KIND_VERSIN] = &ops_versin,
        [EXPR_KIND_VERCOS] = &ops_vercos,
        [EXPR_KIND_COVERSIN] = &ops_coversin,
        [EXPR_KIND_COVERCOS] = &ops_covercos,
        [EXPR_KIND_HAVERSIN] = &ops_haversin,
        [EXPR_KIND_HAVERCOS] = &ops_havercos,
        [EXPR_KIND_HACOVERSIN] = &ops_hacoversin,
        [EXPR_KIND_HACOVERCOS] = &ops_hacovercos,
        [EXPR_KIND_SINH] = &ops_sinh,
        [EXPR_KIND_COSH] = &ops_cosh,
        [EXPR_KIND_TANH] = &ops_tanh,
        [EXPR_KIND_SECH] = &ops_sech,
        [EXPR_KIND_COSECH] = &ops_cosech,
        [EXPR_KIND_COTH] = &ops_coth,
        [EXPR_KIND_ASIN] = &ops_asin,
        [EXPR_KIND_ACOS] = &ops_acos,
        [EXPR_KIND_ATAN] = &ops_atan,
        [EXPR_KIND_ASEC] = &ops_asec,
        [EXPR_KIND_ACOSEC] = &ops_acosec,
        [EXPR_KIND_ACOT] = &ops_acot,
        [EXPR_KIND_ARCVERSIN] = &ops_arcversin,
        [EXPR_KIND_ARCVERCOS] = &ops_arcvercos,
        [EXPR_KIND_ARCCOVERSIN] = &ops_arccoversin,
        [EXPR_KIND_ARCCOVERCOS] = &ops_arccovercos,
        [EXPR_KIND_ARCHAVERSIN] = &ops_archaversin,
        [EXPR_KIND_ARCHAVERCOS] = &ops_archavercos,
        [EXPR_KIND_ARCHACOVERSIN] = &ops_archacoversin,
        [EXPR_KIND_ARCHACOVERCOS] = &ops_archacovercos,
        [EXPR_KIND_ASINH] = &ops_asinh,
        [EXPR_KIND_ACOSH] = &ops_acosh,
        [EXPR_KIND_ATANH] = &ops_atanh,
        [EXPR_KIND_ASECH] = &ops_asech,
        [EXPR_KIND_ACOSECH] = &ops_acosech,
        [EXPR_KIND_ACOTH] = &ops_acoth,
        [EXPR_KIND_EXP] = &ops_exp,
        [EXPR_KIND_LOG] = &ops_log,
        [EXPR_KIND_LOG10] = &ops_log10,
        [EXPR_KIND_SQRT] = &ops_sqrt,
        [EXPR_KIND_CUBRT] = &ops_cubrt,
        [EXPR_KIND_FLOOR] = &ops_floor,
        [EXPR_KIND_CEIL] = &ops_ceil,
        [EXPR_KIND_ABS] = &ops_abs,
        [EXPR_KIND_CONJ] = &ops_conj,
        [EXPR_KIND_ERF] = &ops_erf,
        [EXPR_KIND_ERFC] = &ops_erfc,
        [EXPR_KIND_LGAMMA] = &ops_lgamma,
        [EXPR_KIND_ERFINV] = &ops_erfinv,
        [EXPR_KIND_ERFCINV] = &ops_erfcinv,
        [EXPR_KIND_GAMMA] = &ops_gamma,
        [EXPR_KIND_DIGAMMA] = &ops_digamma,
        [EXPR_KIND_QDIGAMMA] = &ops_qdigamma,
        [EXPR_KIND_TRIGAMMA] = &ops_trigamma,
        [EXPR_KIND_ZETA] = &ops_zeta,
        [EXPR_KIND_ZETAP] = &ops_zetap,
        [EXPR_KIND_ZETAH] = &ops_zetah,
        [EXPR_KIND_ZATAHP] = &ops_zatahp,
        [EXPR_KIND_LERCH_PHI] = &ops_lerch_phi,
        [EXPR_KIND_LERCH_PHI_PACK] = &ops_lerch_phi_pack,
        [EXPR_KIND_DILOG] = &ops_dilog,
        [EXPR_KIND_POLYLOG1] = &ops_polylog1,
        [EXPR_KIND_GAMMAINV] = &ops_gammainv,
        [EXPR_KIND_LAMBERT_W] = &ops_lambert_w,
        [EXPR_KIND_LAMBERT_WN] = &ops_lambert_wn,
        [EXPR_KIND_LAMBERT_W0] = &ops_lambert_w0,
        [EXPR_KIND_LAMBERT_WM1] = &ops_lambert_wm1,
        [EXPR_KIND_NORMAL_PDF] = &ops_normal_pdf,
        [EXPR_KIND_NORMAL_CDF] = &ops_normal_cdf,
        [EXPR_KIND_NORMAL_LOGPDF] = &ops_normal_logpdf,
        [EXPR_KIND_EI] = &ops_Ei,
        [EXPR_KIND_E1] = &ops_E1,
        [EXPR_KIND_FACTORIAL] = &ops_factorial,
        [EXPR_KIND_FIBONACCI] = &ops_fibonacci,
        [EXPR_KIND_PARTITION] = &ops_partition,
        [EXPR_KIND_ISQRT] = &ops_isqrt,
        [EXPR_KIND_IS_PRIME] = &ops_is_prime,
        [EXPR_KIND_NEXT_PRIME] = &ops_next_prime,
        [EXPR_KIND_PREV_PRIME] = &ops_prev_prime,
        [EXPR_KIND_BIT_NOT] = &ops_bit_not,
        [EXPR_KIND_FACTORS] = &ops_factors};
    const expr_ops_t *ops = NULL;

    if ((unsigned)kind < (unsigned)EXPR_KIND_COUNT)
        ops = unary_ops_by_kind[kind];
    if (ops && ops->apply_unary)
        return ops->apply_unary(arg);

    return NULL;
}

/* Construct the single-valued principal square root. */
expr_t *expr_sqrt(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sqrt, a);
}
/* Construct the single-valued principal cube root. */
expr_t *expr_cubrt(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cubrt, a);
}
/* Construct the single-valued principal root. */
expr_t *expr_root(const expr_t *a, const expr_t *order)
{
    return expr_math_wrap_binary(&ops_root, a, order);
}
expr_t *expr_exp(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_exp, a);
}
expr_t *expr_log(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_log, a);
}

/* Construct the natural-logarithm shorthand using the canonical log operation. */
expr_t *expr_ln(const expr_t *a)
{
    return expr_log(a);
}

expr_t *expr_log10(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_log10, a);
}

/* Construct the common-logarithm shorthand using the canonical log10 operation. */
expr_t *expr_lg(const expr_t *a)
{
    return expr_log10(a);
}
expr_t *expr_floor(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_floor, a);
}
expr_t *expr_ceil(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_ceil, a);
}
expr_t *expr_sin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sin, a);
}
expr_t *expr_cos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cos, a);
}
expr_t *expr_tan(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_tan, a);
}
expr_t *expr_sec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sec, a);
}
expr_t *expr_cosec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cosec, a);
}
expr_t *expr_cot(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cot, a);
}
expr_t *expr_versin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_versin, a);
}
expr_t *expr_vercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_vercos, a);
}
expr_t *expr_coversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_coversin, a);
}
expr_t *expr_covercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_covercos, a);
}
expr_t *expr_haversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_haversin, a);
}
expr_t *expr_havercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_havercos, a);
}
expr_t *expr_hacoversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_hacoversin, a);
}
expr_t *expr_hacovercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_hacovercos, a);
}
expr_t *expr_sinh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sinh, a);
}
expr_t *expr_cosh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cosh, a);
}
expr_t *expr_tanh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_tanh, a);
}
expr_t *expr_sech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_sech, a);
}
expr_t *expr_cosech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cosech, a);
}
expr_t *expr_coth(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_coth, a);
}
expr_t *expr_asin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asin, a);
}
expr_t *expr_acos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acos, a);
}
expr_t *expr_atan(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_atan, a);
}
expr_t *expr_asec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asec, a);
}
expr_t *expr_acosec(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acosec, a);
}
expr_t *expr_acot(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acot, a);
}
expr_t *expr_arcversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arcversin, a);
}
expr_t *expr_arcvercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arcvercos, a);
}
expr_t *expr_arccoversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arccoversin, a);
}
expr_t *expr_arccovercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_arccovercos, a);
}
expr_t *expr_archaversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archaversin, a);
}
expr_t *expr_archavercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archavercos, a);
}
expr_t *expr_archacoversin(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archacoversin, a);
}
expr_t *expr_archacovercos(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_archacovercos, a);
}
expr_t *expr_atan2(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_atan2, a, b);
}
expr_t *expr_asinh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asinh, a);
}
expr_t *expr_acosh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acosh, a);
}
expr_t *expr_atanh(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_atanh, a);
}
expr_t *expr_asech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_asech, a);
}
expr_t *expr_acosech(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acosech, a);
}
expr_t *expr_acoth(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_acoth, a);
}
expr_t *expr_abs(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_abs, a);
}
/* Construct the complex conjugate of an expression. */
expr_t *expr_conj(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_conj, a);
}
expr_t *expr_erf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erf, a);
}
expr_t *expr_erfc(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erfc, a);
}
expr_t *expr_lgamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_lgamma, a);
}
expr_t *expr_hypot(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_hypot, a, b);
}
expr_t *expr_erfinv(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erfinv, a);
}
expr_t *expr_erfcinv(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_erfcinv, a);
}
expr_t *expr_gamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_gamma, a);
}
expr_t *expr_digamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_digamma, a);
}
/* Construct a q-digamma expression. */
expr_t *expr_qdigamma(const expr_t *q, const expr_t *z)
{
    return expr_math_wrap_binary(&ops_qdigamma, q, z);
}
expr_t *expr_trigamma(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_trigamma, a);
}
/* Construct a Riemann zeta function expression. */
expr_t *expr_zeta(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_zeta, a);
}
/* Construct a first Riemann zeta derivative expression. */
expr_t *expr_zetap(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_zetap, a);
}
/* Construct a Hurwitz zeta function expression. */
expr_t *expr_zetah(const expr_t *s, const expr_t *a)
{
    return expr_math_wrap_binary(&ops_zetah, s, a);
}
/* Construct a first Hurwitz zeta derivative expression. */
expr_t *expr_zatahp(const expr_t *s, const expr_t *a)
{
    return expr_math_wrap_binary(&ops_zatahp, s, a);
}
expr_t *expr_polygamma_xp(const expr_t *order, const expr_t *arg)
{
    return expr_math_wrap_binary(&ops_polygamma, order, arg);
}
expr_t *expr_polygamma(unsigned int order, const expr_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    expr_t *order_xp = expr_new_const(order_value);
    expr_t *out = expr_polygamma_xp(order_xp, a);

    expr_free(order_xp);
    return out;
}
expr_t *expr_dilog(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_dilog, a);
}
/* Construct an order-one polylogarithm expression. */
expr_t *expr_polylog1(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_polylog1, a);
}
expr_t *expr_polylog_xp(const expr_t *order, const expr_t *arg)
{
    return expr_math_wrap_binary(&ops_polylog, order, arg);
}
expr_t *expr_polylog(unsigned int order, const expr_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    expr_t *order_xp = expr_new_const(order_value);
    expr_t *out = expr_polylog_xp(order_xp, a);

    expr_free(order_xp);
    return out;
}
/* Construct a native harmonic-polynomial expression. */
expr_t *expr_harmonic_poly(const expr_t *degree, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_harmonic_poly, degree, argument);
}
/* Construct a native Lerch-transcendent expression. */
expr_t *expr_lerch_phi(const expr_t *z, const expr_t *s, const expr_t *a)
{
    expr_t *parameters = expr_math_wrap_binary(&ops_lerch_phi_pack, z, s);
    expr_t *out = parameters ? expr_math_wrap_binary(&ops_lerch_phi, parameters, a) : NULL;

    expr_free(parameters);
    return out;
}
expr_t *expr_legendre_chi_xp(const expr_t *order, const expr_t *arg)
{
    return expr_math_wrap_binary(&ops_legendre_chi, order, arg);
}
expr_t *expr_legendre_chi(unsigned int order, const expr_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    expr_t *order_xp = expr_new_const(order_value);
    expr_t *out = expr_legendre_chi_xp(order_xp, a);

    expr_free(order_xp);
    return out;
}
expr_t *expr_bessel_j(const expr_t *order, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_bessel_j, order, argument);
}
expr_t *expr_bessel_y(const expr_t *order, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_bessel_y, order, argument);
}

expr_t *expr_lommel_s(const expr_t *mu, const expr_t *nu, const expr_t *argument)
{
    expr_t *parameters = expr_lommel_s_pack(mu, nu);
    expr_t *out = parameters ? expr_math_wrap_binary(&ops_lommel_s, parameters, argument) : NULL;

    expr_free(parameters);
    return out;
}

static expr_t *expr_lommel_s_hypergeometric_argument_derivative_expansion(const expr_t *mu, const expr_t *nu,
                                                                          const expr_t *argument)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *two = expr_new_const(NUM_TWO);
    expr_t *mu_plus_one = expr_add_long(mu, 1);
    expr_t *mu_plus_two = expr_add_long(mu, 2);
    expr_t *mu_minus_nu = expr_sub(mu, nu);
    expr_t *mu_plus_nu = expr_add(mu, nu);
    expr_t *lower_a_numerator = mu_minus_nu ? expr_add_long(mu_minus_nu, 3) : NULL;
    expr_t *lower_b_numerator = mu_plus_nu ? expr_add_long(mu_plus_nu, 3) : NULL;
    expr_t *lower_a = lower_a_numerator ? expr_div_num(lower_a_numerator, &NUM_TWO) : NULL;
    expr_t *lower_b = lower_b_numerator ? expr_div_num(lower_b_numerator, &NUM_TWO) : NULL;
    expr_t *lower_a_shifted = lower_a ? expr_add_long(lower_a, 1) : NULL;
    expr_t *lower_b_shifted = lower_b ? expr_add_long(lower_b, 1) : NULL;
    expr_t *argument_squared = expr_pow_long(argument, 2);
    expr_t *negative_argument_squared = argument_squared ? expr_neg(argument_squared) : NULL;
    number_t four = num_create_from_long(4);
    expr_t *hyper_argument = negative_argument_squared ? expr_div_num(negative_argument_squared, &four) : NULL;
    const expr_t *upper[1] = {one};
    const expr_t *upper_shifted[1] = {two};
    const expr_t *lower[2] = {lower_a, lower_b};
    const expr_t *lower_shifted[2] = {lower_a_shifted, lower_b_shifted};
    expr_t *hyper = (one && lower_a && lower_b && hyper_argument)
                        ? expr_hypergeometric_pFq(1u, upper, 2u, lower, hyper_argument)
                        : NULL;
    expr_t *hyper_shifted = (two && lower_a_shifted && lower_b_shifted && hyper_argument)
                                ? expr_hypergeometric_pFq(1u, upper_shifted, 2u, lower_shifted, hyper_argument)
                                : NULL;
    expr_t *argument_to_mu = expr_pow_xp(argument, mu);
    expr_t *first_coefficient = (mu_plus_one && argument_to_mu) ? expr_mul(mu_plus_one, argument_to_mu) : NULL;
    expr_t *first_term = (first_coefficient && hyper) ? expr_mul(first_coefficient, hyper) : NULL;
    expr_t *argument_to_mu_plus_two = mu_plus_two ? expr_pow_xp(argument, mu_plus_two) : NULL;
    expr_t *lower_product = (lower_a && lower_b) ? expr_mul(lower_a, lower_b) : NULL;
    expr_t *twice_lower_product = (two && lower_product) ? expr_mul(two, lower_product) : NULL;
    expr_t *second_numerator =
        (argument_to_mu_plus_two && hyper_shifted) ? expr_mul(argument_to_mu_plus_two, hyper_shifted) : NULL;
    expr_t *second_term = (second_numerator && twice_lower_product) ? expr_div(second_numerator, twice_lower_product) : NULL;
    expr_t *numerator = (first_term && second_term) ? expr_sub(first_term, second_term) : NULL;
    expr_t *mu_plus_one_squared = mu_plus_one ? expr_pow_long(mu_plus_one, 2) : NULL;
    expr_t *nu_squared = expr_pow_long(nu, 2);
    expr_t *denominator = (mu_plus_one_squared && nu_squared) ? expr_sub(mu_plus_one_squared, nu_squared) : NULL;
    expr_t *raw = (numerator && denominator) ? expr_div(numerator, denominator) : NULL;
    expr_t *out = raw ? expr_simplify(raw) : NULL;

    expr_free(raw);
    expr_free(denominator);
    expr_free(nu_squared);
    expr_free(mu_plus_one_squared);
    expr_free(numerator);
    expr_free(second_term);
    expr_free(second_numerator);
    expr_free(twice_lower_product);
    expr_free(lower_product);
    expr_free(argument_to_mu_plus_two);
    expr_free(first_term);
    expr_free(first_coefficient);
    expr_free(argument_to_mu);
    expr_free(hyper_shifted);
    expr_free(hyper);
    expr_free(hyper_argument);
    num_destroy(&four);
    expr_free(negative_argument_squared);
    expr_free(argument_squared);
    expr_free(lower_b_shifted);
    expr_free(lower_a_shifted);
    expr_free(lower_b);
    expr_free(lower_a);
    expr_free(lower_b_numerator);
    expr_free(lower_a_numerator);
    expr_free(mu_plus_nu);
    expr_free(mu_minus_nu);
    expr_free(mu_plus_two);
    expr_free(mu_plus_one);
    expr_free(two);
    expr_free(one);
    return out;
}

expr_t *expr_lommel_s_argument_derivative_expansion(const expr_t *mu, const expr_t *nu, const expr_t *argument)
{
    expr_t *mu_plus_nu = expr_add(mu, nu);
    expr_t *coefficient = mu_plus_nu ? expr_add_long(mu_plus_nu, -1) : NULL;
    expr_t *simplified_coefficient = coefficient ? expr_simplify(coefficient) : NULL;
    expr_t *mu_minus_one = expr_add_long(mu, -1);
    expr_t *nu_minus_one = expr_add_long(nu, -1);
    expr_t *shifted = (mu_minus_one && nu_minus_one) ? expr_lommel_s(mu_minus_one, nu_minus_one, argument) : NULL;
    expr_t *left = (coefficient && shifted) ? expr_mul(coefficient, shifted) : NULL;
    expr_t *original = expr_lommel_s(mu, nu, argument);
    expr_t *nu_times_original = original ? expr_mul(nu, original) : NULL;
    expr_t *right = nu_times_original ? expr_div(nu_times_original, argument) : NULL;
    expr_t *raw = (left && right) ? expr_sub(left, right) : NULL;
    expr_t *out = NULL;

    if (simplified_coefficient && expr_is_exact_zero(simplified_coefficient))
        out = expr_lommel_s_hypergeometric_argument_derivative_expansion(mu, nu, argument);
    else
        out = raw ? expr_simplify(raw) : NULL;

    expr_free(raw);
    expr_free(right);
    expr_free(nu_times_original);
    expr_free(original);
    expr_free(left);
    expr_free(shifted);
    expr_free(nu_minus_one);
    expr_free(mu_minus_one);
    expr_free(simplified_coefficient);
    expr_free(coefficient);
    expr_free(mu_plus_nu);
    return out;
}

static expr_t *expr_lommel_s_pack(const expr_t *mu, const expr_t *nu)
{
    return expr_math_wrap_binary(&ops_lommel_s_pack, mu, nu);
}

static expr_t *expr_lommel_s_from_pack(const expr_t *parameters, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_lommel_s, parameters, argument);
}

static expr_t *expr_appell_f1_pack(const expr_t *left, const expr_t *right)
{
    return expr_math_wrap_binary(&ops_appell_f1_pack, left, right);
}

static expr_t *expr_appell_f1_from_packs(const expr_t *params, const expr_t *vars)
{
    return expr_math_wrap_binary(&ops_appell_f1, params, vars);
}

expr_t *expr_appell_f1(const expr_t *a, const expr_t *b1, const expr_t *b2, const expr_t *c, const expr_t *x,
                       const expr_t *y)
{
    expr_t *ab = NULL;
    expr_t *bc = NULL;
    expr_t *params = NULL;
    expr_t *vars = NULL;
    expr_t *out = NULL;

    if (!a || !b1 || !b2 || !c || !x || !y)
        return NULL;

    ab = expr_appell_f1_pack(a, b1);
    bc = expr_appell_f1_pack(b2, c);
    params = ab && bc ? expr_appell_f1_pack(ab, bc) : NULL;
    vars = expr_appell_f1_pack(x, y);
    out = params && vars ? expr_math_wrap_binary(&ops_appell_f1, params, vars) : NULL;

    expr_free(vars);
    expr_free(params);
    expr_free(bc);
    expr_free(ab);
    return out;
}

static expr_t *expr_hypergeometric_pFq_pack(const expr_t *left, const expr_t *right)
{
    return expr_math_wrap_binary(&ops_hypergeometric_pFq_pack, left, right);
}

static expr_t *expr_hypergeometric_pFq_from_pack(const expr_t *parameters, const expr_t *argument)
{
    return expr_math_wrap_binary(&ops_hypergeometric_pFq, parameters, argument);
}

static expr_t *expr_lauricella_f_from_packs(const expr_t *params, const expr_t *vars)
{
    return expr_math_wrap_binary(&ops_lauricella_f, params, vars);
}

static expr_t *expr_hypergeometric_parameter_values(size_t count, const expr_t *const *values, const expr_t *empty)
{
    expr_t *list;

    if (count == 0u)
        return expr_clone(empty);
    if (!values || !values[0])
        return NULL;
    expr_retain(values[0]);
    list = (expr_t *)values[0];
    for (size_t i = 1u; list && i < count; ++i) {
        expr_t *next;

        if (!values[i]) {
            expr_free(list);
            return NULL;
        }
        next = expr_hypergeometric_pFq_pack(list, values[i]);
        expr_free(list);
        list = next;
    }
    return list;
}

static expr_t *expr_hypergeometric_parameter_set(size_t count, const expr_t *const *values)
{
    number_t count_value;
    expr_t *count_expr;
    expr_t *value_expr;
    expr_t *set;

    if (count > (size_t)LONG_MAX)
        return NULL;
    count_value = num_create_from_long((long)count);
    count_expr = expr_new_const(count_value);
    value_expr = count_expr ? expr_hypergeometric_parameter_values(count, values, count_expr) : NULL;
    set = count_expr && value_expr ? expr_hypergeometric_pFq_pack(count_expr, value_expr) : NULL;
    expr_free(value_expr);
    expr_free(count_expr);
    return set;
}

expr_t *expr_hypergeometric_pFq(size_t upper_count, const expr_t *const *upper, size_t lower_count,
                                const expr_t *const *lower, const expr_t *argument)
{
    expr_t *upper_set = NULL;
    expr_t *lower_set = NULL;
    expr_t *parameters = NULL;
    expr_t *out = NULL;

    if ((upper_count > 0u && !upper) || (lower_count > 0u && !lower) || !argument)
        return NULL;

    upper_set = expr_hypergeometric_parameter_set(upper_count, upper);
    lower_set = expr_hypergeometric_parameter_set(lower_count, lower);
    parameters = upper_set && lower_set ? expr_hypergeometric_pFq_pack(upper_set, lower_set) : NULL;
    out = parameters ? expr_math_wrap_binary(&ops_hypergeometric_pFq, parameters, argument) : NULL;

    expr_free(parameters);
    expr_free(lower_set);
    expr_free(upper_set);
    return out;
}

expr_t *expr_hypergeometric_pFq_from_args(size_t argument_count, expr_t *const *arguments)
{
    double upper_count_value;
    double lower_count_value;
    size_t upper_count;
    size_t lower_count;

    if (!arguments || argument_count < 3u || !expr_is_const(arguments[0]) || !expr_is_const(arguments[1]) ||
        !num_is_real(arguments[0]->c) || !num_is_integer(arguments[0]->c) || !num_is_real(arguments[1]->c) ||
        !num_is_integer(arguments[1]->c) || num_get_sign(arguments[0]->c) < 0 || num_get_sign(arguments[1]->c) < 0)
        return NULL;
    upper_count_value = num_to_double(arguments[0]->c);
    lower_count_value = num_to_double(arguments[1]->c);
    if (!isfinite(upper_count_value) || !isfinite(lower_count_value) || upper_count_value > 1024.0 ||
        lower_count_value > 1024.0)
        return NULL;
    upper_count = (size_t)upper_count_value;
    lower_count = (size_t)lower_count_value;
    if (argument_count != upper_count + lower_count + 3u)
        return NULL;
    return expr_hypergeometric_pFq(upper_count, (const expr_t *const *)&arguments[2], lower_count,
                                   (const expr_t *const *)&arguments[2u + upper_count], arguments[argument_count - 1u]);
}

expr_t *expr_lauricella_f(const expr_t *a, size_t variable_count, const expr_t *const *b, const expr_t *c,
                          const expr_t *const *x)
{
    expr_t *a_c = NULL;
    expr_t *b_set = NULL;
    expr_t *x_set = NULL;
    expr_t *parameters = NULL;
    expr_t *out = NULL;

    if (!a || !c || (variable_count > 0u && (!b || !x)))
        return NULL;
    a_c = expr_hypergeometric_pFq_pack(a, c);
    b_set = expr_hypergeometric_parameter_set(variable_count, b);
    x_set = expr_hypergeometric_parameter_set(variable_count, x);
    parameters = a_c && b_set ? expr_hypergeometric_pFq_pack(a_c, b_set) : NULL;
    out = parameters && x_set ? expr_math_wrap_binary(&ops_lauricella_f, parameters, x_set) : NULL;
    expr_free(parameters);
    expr_free(x_set);
    expr_free(b_set);
    expr_free(a_c);
    return out;
}

expr_t *expr_lauricella_f_from_args(size_t argument_count, expr_t *const *arguments)
{
    double count_value;
    size_t variable_count;

    if (!arguments || argument_count < 3u || !expr_is_const(arguments[0]) || !num_is_real(arguments[0]->c) ||
        !num_is_integer(arguments[0]->c) || num_get_sign(arguments[0]->c) < 0)
        return NULL;
    count_value = num_to_double(arguments[0]->c);
    if (!isfinite(count_value) || count_value > 512.0)
        return NULL;
    variable_count = (size_t)count_value;
    if (argument_count != 2u * variable_count + 3u)
        return NULL;
    return expr_lauricella_f(arguments[1], variable_count, (const expr_t *const *)&arguments[2],
                             arguments[2u + variable_count], (const expr_t *const *)&arguments[3u + variable_count]);
}

expr_t *expr_gammainv(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_gammainv, a);
}
expr_t *expr_lambert_w(const expr_t *a)
{
    expr_t *inverse = expr_simplify_try_lambert_argument((expr_t *)a);

    if (inverse)
        return inverse;
    return expr_math_wrap_unary(&ops_lambert_w, a);
}
expr_t *expr_lambert_wn_xp(const expr_t *branch, const expr_t *arg)
{
    if (branch && expr_is_const(branch)) {
        if (num_eq(branch->c, NUM_ZERO))
            return expr_lambert_w0(arg);
        if (num_eq(branch->c, NUM_NEG_ONE))
            return expr_lambert_wm1(arg);
    }
    return expr_math_wrap_binary(&ops_lambert_wn, branch, arg);
}
expr_t *expr_lambert_wn(const expr_t *branch, const expr_t *arg)
{
    return expr_lambert_wn_xp(branch, arg);
}
expr_t *expr_lambert_w0(const expr_t *a)
{
    expr_t *inverse = expr_simplify_try_lambert_argument((expr_t *)a);

    if (inverse)
        return inverse;
    return expr_math_wrap_unary(&ops_lambert_w0, a);
}
expr_t *expr_lambert_wm1(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_lambert_wm1, a);
}
expr_t *expr_normal_pdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_normal_pdf, a);
}
expr_t *expr_normal_cdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_normal_cdf, a);
}
expr_t *expr_normal_logpdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_normal_logpdf, a);
}
expr_t *expr_pdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_pdf, a);
}
expr_t *expr_cdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_cdf, a);
}
expr_t *expr_logpdf(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_logpdf, a);
}
expr_t *expr_Ei(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_Ei, a);
}
expr_t *expr_E1(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_E1, a);
}
expr_t *expr_beta(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_beta, a, b);
}
expr_t *expr_logbeta(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_logbeta, a, b);
}
expr_t *expr_new_indexed_symbol(const char *name, const expr_t *index)
{
    expr_t *symbol = expr_new_named_const(NUM_NAN, name);
    expr_t *out = symbol && index ? expr_math_wrap_binary(&ops_indexed_symbol, symbol, index) : NULL;

    expr_free(symbol);
    return out;
}

expr_t *expr_new_summation(const expr_t *term, const expr_t *index)
{
    return expr_math_wrap_binary(&ops_summation, term, index);
}
expr_t *expr_new_finite_summation(const expr_t *term, const expr_t *index, const expr_t *upper)
{
    expr_t *bounds = expr_math_wrap_binary(&ops_argument_list, index, upper);
    expr_t *out = bounds ? expr_math_wrap_binary(&ops_summation, term, bounds) : NULL;

    expr_free(bounds);
    return out;
}

/* Build a formal finite summation with explicit inclusive bounds. */
expr_t *expr_new_finite_summation_range(const expr_t *term, const expr_t *index, const expr_t *lower,
                                        const expr_t *upper)
{
    expr_t *range = expr_math_wrap_binary(&ops_argument_list, lower, upper);
    expr_t *bounds = range ? expr_math_wrap_binary(&ops_argument_list, index, range) : NULL;
    expr_t *out = bounds ? expr_math_wrap_binary(&ops_summation, term, bounds) : NULL;

    expr_free(bounds);
    expr_free(range);
    return out;
}

expr_t *expr_new_product(const expr_t *term, const expr_t *index)
{
    return expr_math_wrap_binary(&ops_product, term, index);
}

/* Build a formal finite product with explicit inclusive bounds. */
expr_t *expr_new_finite_product_range(const expr_t *term, const expr_t *index, const expr_t *lower,
                                     const expr_t *upper)
{
    expr_t *range = expr_math_wrap_binary(&ops_argument_list, lower, upper);
    expr_t *bounds = range ? expr_math_wrap_binary(&ops_argument_list, index, range) : NULL;
    expr_t *out = bounds ? expr_math_wrap_binary(&ops_product, term, bounds) : NULL;

    expr_free(bounds);
    expr_free(range);
    return out;
}
expr_t *expr_gammainc_lower(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_lower, a, b);
}
expr_t *expr_gammainc_upper(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_upper, a, b);
}
expr_t *expr_gammainc_P(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_P, a, b);
}
expr_t *expr_gammainc_Q(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gammainc_Q, a, b);
}
expr_t *expr_factorial(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_factorial, a);
}
expr_t *expr_fibonacci(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_fibonacci, a);
}
expr_t *expr_partition(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_partition, a);
}
expr_t *expr_isqrt(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_isqrt, a);
}
expr_t *expr_gcd(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_gcd, a, b);
}
expr_t *expr_lcm(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_lcm, a, b);
}
expr_t *expr_mod(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_mod, a, b);
}
expr_t *expr_modinv(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_modinv, a, b);
}
expr_t *expr_is_prime(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_is_prime, a);
}
expr_t *expr_next_prime(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_next_prime, a);
}
expr_t *expr_prev_prime(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_prev_prime, a);
}
expr_t *expr_bit_and(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_bit_and, a, b);
}
expr_t *expr_bit_or(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_bit_or, a, b);
}
expr_t *expr_bit_xor(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_bit_xor, a, b);
}
expr_t *expr_bit_not(const expr_t *a)
{
    return expr_math_wrap_unary(&ops_bit_not, a);
}
expr_t *expr_shl(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_shl, a, b);
}
expr_t *expr_shr(const expr_t *a, const expr_t *b)
{
    return expr_math_wrap_binary(&ops_shr, a, b);
}

static expr_t *expr_factor_product_from_number(number_t value)
{
    enum { FACTOR_MAX_BITS = 1024 };
    number_factors_t *factors;
    expr_t *out = NULL;

    if (!num_is_integer(value) || num_get_sign(value) <= 0 || num_bit_length(value) > FACTOR_MAX_BITS)
        return NULL;

    factors = num_factors(value);
    if (!factors)
        return NULL;

    if (factors->count == 0u) {
        num_factors_free(factors);
        return expr_new_const(NUM_ONE);
    }

    for (size_t i = 0u; i < factors->count; ++i) {
        char name[32];
        expr_t *base;
        expr_t *factor;

        snprintf(name, sizeof(name), "a%zu", i);
        base = expr_new_named_const(factors->items[i].prime, name);
        {
            string_t *prime_text = num_to_string(factors->items[i].prime);

            base->binding_expr = expr_binding_expr_new_number_text(prime_text ? string_c_str(prime_text) : "NAN");
            string_free(prime_text);
        }
        if (factors->items[i].exponent > 1u) {
            number_t exponent = num_create_from_long((long)factors->items[i].exponent);

            factor = expr_pow(base, &exponent);
            num_destroy(&exponent);
            expr_free(base);
        } else {
            factor = base;
        }

        if (out) {
            expr_t *next = expr_mul(out, factor);

            expr_free(out);
            expr_free(factor);
            out = next;
        } else {
            out = factor;
        }
    }

    num_factors_free(factors);
    return out;
}

expr_t *expr_factors(const expr_t *a)
{
    expr_t *product;

    if (!a)
        return NULL;

    product = expr_factor_product_from_number(expr_eval_num_internal(a));
    if (product)
        return product;

    return expr_math_wrap_unary(&ops_factors, a);
}

expr_t *expr_logbeta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)
{
    expr_t *one;
    expr_t *a_minus_one;
    expr_t *b_minus_one;
    expr_t *log_x;
    expr_t *one_minus_x;
    expr_t *log_one_minus_x;
    expr_t *left;
    expr_t *right;
    expr_t *sum;
    expr_t *log_beta;
    expr_t *out;

    if (!x || !a || !b)
        return NULL;

    one = expr_new_const(NUM_ONE);
    a_minus_one = expr_sub(a, one);
    b_minus_one = expr_sub(b, one);
    log_x = expr_log(x);
    one_minus_x = expr_sub(one, x);
    log_one_minus_x = expr_log(one_minus_x);
    left = expr_mul(a_minus_one, log_x);
    right = expr_mul(b_minus_one, log_one_minus_x);
    sum = expr_add(left, right);
    log_beta = expr_logbeta(a, b);
    out = expr_sub(sum, log_beta);

    expr_free(log_beta);
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(log_one_minus_x);
    expr_free(one_minus_x);
    expr_free(log_x);
    expr_free(b_minus_one);
    expr_free(a_minus_one);
    expr_free(one);
    return out;
}

expr_t *expr_beta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)
{
    expr_t *log_pdf = expr_logbeta_pdf(x, a, b);
    expr_t *out = log_pdf ? expr_exp(log_pdf) : NULL;

    expr_free(log_pdf);
    return out;
}

expr_t *expr_binomial(const expr_t *n, const expr_t *k)
{
    expr_t *one;
    expr_t *n_plus_one;
    expr_t *k_plus_one;
    expr_t *n_minus_k;
    expr_t *n_minus_k_plus_one;
    expr_t *gamma_n;
    expr_t *gamma_k;
    expr_t *gamma_n_minus_k;
    expr_t *denominator;
    expr_t *out;

    if (!n || !k)
        return NULL;

    one = expr_new_const(NUM_ONE);
    n_plus_one = expr_add(n, one);
    k_plus_one = expr_add(k, one);
    n_minus_k = expr_sub(n, k);
    n_minus_k_plus_one = expr_add(n_minus_k, one);
    gamma_n = expr_gamma(n_plus_one);
    gamma_k = expr_gamma(k_plus_one);
    gamma_n_minus_k = expr_gamma(n_minus_k_plus_one);
    denominator = expr_mul(gamma_k, gamma_n_minus_k);
    out = expr_div(gamma_n, denominator);

    expr_free(denominator);
    expr_free(gamma_n_minus_k);
    expr_free(gamma_k);
    expr_free(gamma_n);
    expr_free(n_minus_k_plus_one);
    expr_free(n_minus_k);
    expr_free(k_plus_one);
    expr_free(n_plus_one);
    expr_free(one);
    return out;
}
