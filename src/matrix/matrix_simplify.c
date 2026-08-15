#include <limits.h>
#include <stdlib.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static bool mat_number_entries_are_exact_real(const matrix_t *A)
{
    if (!A || matrix_is_symbolic(A))
        return false;
    for (size_t row = 0u; row < A->rows; ++row) {
        for (size_t col = 0u; col < A->cols; ++col) {
            number_t value = mat_get_num(A, row, col);
            bool exact_real = num_is_exact(value) && num_is_real(value);

            num_destroy(&value);
            if (!exact_real)
                return false;
        }
    }
    return true;
}

static expr_t *mat_exact_expr_quotient(expr_t *numerator, const expr_t *denominator)
{
    expr_t *quotient;

    if (!numerator || !denominator) {
        expr_free(numerator);
        return NULL;
    }
    quotient = expr_div(numerator, denominator);
    expr_free(numerator);
    return quotient;
}

/* Simplify every symbolic entry in place. */
int mat_simplify_symbolic_inplace(matrix_t *A)
{
    if (!A)
        return -1;
    if (A->elem != &expr_elem)
        return 0;
    for (size_t row = 0u; row < A->rows; ++row) {
        for (size_t col = 0u; col < A->cols; ++col) {
            expr_t *entry = NULL;
            expr_t *simplified;

            mat_get(A, row, col, &entry);
            if (!entry)
                continue;
            simplified = expr_simplify(entry);
            if (!simplified)
                return -1;
            mat_set(A, row, col, &simplified);
            expr_free(simplified);
        }
    }
    return 0;
}

/* Return a copy with every symbolic entry simplified. */
matrix_t *mat_simplify_symbolic(const matrix_t *A)
{
    matrix_t *C;

    if (!A)
        return NULL;
    C = mat_copy_preserving_store(A);
    if (!C)
        return NULL;
    if (mat_simplify_symbolic_inplace(C) != 0) {
        mat_free(C);
        return NULL;
    }
    return C;
}

/* Replace an owned symbolic result with its simplified equivalent. */
matrix_t *mat_finalize_symbolic_result(matrix_t *A)
{
    matrix_t *simplified;

    if (!A || A->elem != &expr_elem)
        return A;
    simplified = mat_simplify_symbolic(A);
    mat_free(A);
    return simplified;
}

/* Retain an exact algebraic principal square root instead of evaluating its surds. */
matrix_t *mat_simplify_exact_principal_sqrt(const matrix_t *A)
{
    matrix_t *result = NULL;
    expr_t *determinant_expr = NULL;
    expr_t *determinant_root = NULL;
    expr_t *denominator_argument = NULL;
    expr_t *denominator = NULL;
    number_t determinant = number_invalid();
    number_t trace = number_invalid();
    number_t denominator_value = number_invalid();

    if (!A || A->rows != A->cols || (A->rows != 1u && A->rows != 2u) || !mat_number_entries_are_exact_real(A))
        return NULL;
    if (A->rows == 1u) {
        number_t value = mat_get_num(A, 0u, 0u);
        expr_t *argument = expr_new_const(value);
        expr_t *root = argument ? expr_sqrt(argument) : NULL;

        num_destroy(&value);
        expr_free(argument);
        root = expr_simplify_owned(root);
        result = root ? mat_create_expr(1u, 1u, &root) : NULL;
        expr_free(root);
        return result;
    }
    if (mat_det(A, &determinant) != 0 || mat_trace(A, &trace) != 0)
        goto cleanup;
    if (num_sign(determinant) >= 0 && num_sign(trace) < 0)
        goto cleanup;
    determinant_expr = expr_new_const(determinant);
    determinant_root = determinant_expr ? expr_sqrt(determinant_expr) : NULL;
    determinant_root = expr_simplify_owned(determinant_root);
    if (!determinant_root)
        goto cleanup;
    denominator_argument = expr_mul_simplify_owned(expr_const_long(2), expr_clone(determinant_root));
    denominator_argument = expr_add_simplify_owned(expr_new_const(trace), denominator_argument);
    denominator = denominator_argument ? expr_sqrt(denominator_argument) : NULL;
    denominator = expr_simplify_owned(denominator);
    if (!denominator)
        goto cleanup;
    denominator_value = expr_eval(denominator);
    if (!num_is_finite(denominator_value) || num_is_zero(denominator_value))
        goto cleanup;
    result = mat_new_expr(2u, 2u);
    if (!result)
        goto cleanup;
    for (size_t row = 0u; row < 2u; ++row) {
        for (size_t col = 0u; col < 2u; ++col) {
            number_t value = mat_get_num(A, row, col);
            expr_t *numerator = expr_new_const(value);
            expr_t *entry;

            num_destroy(&value);
            if (row == col)
                numerator = expr_add_simplify_owned(numerator, expr_clone(determinant_root));
            entry = mat_exact_expr_quotient(numerator, denominator);
            if (!entry) {
                mat_free(result);
                result = NULL;
                goto cleanup;
            }
            mat_set(result, row, col, &entry);
            expr_free(entry);
        }
    }
    result = mat_finalize_symbolic_result(result);

cleanup:
    num_destroy(&denominator_value);
    num_destroy(&trace);
    num_destroy(&determinant);
    expr_free(denominator);
    expr_free(denominator_argument);
    expr_free(determinant_root);
    expr_free(determinant_expr);
    return result;
}

/* Prepare one matrix entry for the style-independent beautification pass. */
expr_t *mat_simplify_expression_for_beautification(const expr_t *entry)
{
    expr_t *prepared = NULL;

    if (!entry)
        return NULL;
    if (expr_contains_half_scaled_symbolic_power(entry))
        prepared = expr_factor_common_post_calculus(entry);
    if (!prepared) {
        expr_retain((expr_t *)entry);
        prepared = (expr_t *)entry;
    }
    return prepared;
}

/* Apply the common-factor simplification used after matrix calculus. */
expr_t *mat_simplify_post_calculus_expression(expr_t *entry)
{
    expr_t *factored;

    if (!entry)
        return NULL;
    factored = expr_factor_common_post_calculus(entry);
    if (!factored)
        return entry;
    expr_free(entry);
    return factored;
}

/* Rewrite A^(n/2) as an integer matrix power times the principal square root. */
matrix_t *mat_simplify_exact_half_integer_power(const matrix_t *A, long numerator)
{
    matrix_t *integer_power;
    matrix_t *principal_root;
    matrix_t *result;
    long integer_exponent;

    if (!A || numerator == LONG_MIN || numerator == 1L || numerator % 2L == 0L)
        return NULL;
    integer_exponent = (numerator - 1L) / 2L;
    if (integer_exponent < INT_MIN || integer_exponent > INT_MAX)
        return NULL;
    integer_power = mat_pow_int(A, (int)integer_exponent);
    principal_root = mat_simplify_exact_principal_sqrt(A);
    result = integer_power && principal_root ? mat_mul(integer_power, principal_root) : NULL;
    mat_free(principal_root);
    mat_free(integer_power);
    return result;
}

static const expr_t *mat_symbolic_exponent_core(const expr_t *exponent)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    number_t value = number_invalid();

    if (!exponent)
        return NULL;
    if (expr_match_add_sub_expr(exponent, &left, &right, &is_sub)) {
        bool left_constant = expr_match_const_value(left, &value);

        num_destroy(&value);
        if (left_constant && !is_sub)
            return right;
        if (expr_match_const_value(right, &value)) {
            num_destroy(&value);
            return left;
        }
        num_destroy(&value);
    }
    return expr_match_const_value(exponent, &value) ? (num_destroy(&value), NULL) : (num_destroy(&value), exponent);
}

static bool mat_expr_contains_power_family(const expr_t *expr, const expr_t *base, const expr_t *exponent_core,
                                           bool denominator_context)
{
    const expr_t *candidate_base = NULL;
    const expr_t *candidate_exponent = NULL;
    const expr_t *candidate_core;

    if (!expr)
        return false;
    if (denominator_context && expr_match_pow_expr(expr, &candidate_base, &candidate_exponent)) {
        candidate_core = mat_symbolic_exponent_core(candidate_exponent);
        if (candidate_core && expr_simplify_same_factor(candidate_base, base) &&
            expr_simplify_same_factor(candidate_core, exponent_core)) {
            return true;
        }
    }
    {
        const expr_t *numerator = NULL;
        const expr_t *denominator = NULL;

        if (expr_match_div_expr(expr, &numerator, &denominator))
            return mat_expr_contains_power_family(numerator, base, exponent_core, false) ||
                   mat_expr_contains_power_family(denominator, base, exponent_core, true);
    }
    return mat_expr_contains_power_family(expr_first_child(expr), base, exponent_core, denominator_context) ||
           mat_expr_contains_power_family(expr_second_child(expr), base, exponent_core, denominator_context);
}

static expr_t *mat_find_common_reciprocal_symbolic_power(const expr_t *expr, expr_t *const *entries, size_t count,
                                                         bool denominator_context)
{
    const expr_t *base = NULL;
    const expr_t *exponent = NULL;
    const expr_t *core;

    if (!expr)
        return NULL;
    if (denominator_context && expr_match_pow_expr(expr, &base, &exponent) &&
        (core = mat_symbolic_exponent_core(exponent)) != NULL) {
        size_t index;

        for (index = 1u; index < count; ++index)
            if (!mat_expr_contains_power_family(entries[index], base, core, false))
                break;
        if (index == count) {
            expr_t *power = expr_pow_xp(base, core);
            expr_t *factor = power ? expr_div(expr_const_one(), power) : NULL;

            expr_free(power);
            return expr_simplify_owned(factor);
        }
    }
    {
        const expr_t *numerator = NULL;
        const expr_t *denominator = NULL;
        expr_t *found;

        if (expr_match_div_expr(expr, &numerator, &denominator)) {
            found = mat_find_common_reciprocal_symbolic_power(numerator, entries, count, false);
            return found ? found : mat_find_common_reciprocal_symbolic_power(denominator, entries, count, true);
        }
        found = mat_find_common_reciprocal_symbolic_power(expr_first_child(expr), entries, count, denominator_context);
        return found ? found : mat_find_common_reciprocal_symbolic_power(expr_second_child(expr), entries, count,
                                                                         denominator_context);
    }
}

static expr_t *mat_reduce_denominator_power(const expr_t *expr, const expr_t *base, const expr_t *exponent_core)
{
    const expr_t *power_base = NULL;
    const expr_t *power_exponent = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (expr_match_pow_expr(expr, &power_base, &power_exponent) && expr_simplify_same_factor(power_base, base)) {
        expr_t *reduced_exponent = expr_sub_simplify_owned(expr_clone(power_exponent), expr_clone(exponent_core));
        number_t reduced_value = reduced_exponent ? expr_eval(reduced_exponent) : num_new();
        expr_t *reduced_power = num_is_finite(reduced_value) && num_eq(reduced_value, NUM_NEG_ONE)
                                    ? expr_div_simplify_owned(expr_const_one(), expr_clone(base))
                                    : reduced_exponent ? expr_pow_xp(base, reduced_exponent) : NULL;

        num_destroy(&reduced_value);
        expr_free(reduced_exponent);
        return expr_simplify_owned(reduced_power);
    }
    if (expr_match_mul_expr(expr, &left, &right)) {
        bool reduce_left = mat_expr_contains_power_family(left, base, exponent_core, true);
        expr_t *reduced = reduce_left ? mat_reduce_denominator_power(left, base, exponent_core)
                                      : mat_expr_contains_power_family(right, base, exponent_core, true)
                                            ? mat_reduce_denominator_power(right, base, exponent_core)
                                            : NULL;
        expr_t *out;

        if (!reduced)
            return NULL;
        out = expr_mul_simplify_owned(reduced, expr_clone(reduce_left ? right : left));
        return out;
    }
    return NULL;
}

static expr_t *mat_cancel_reciprocal_power(const expr_t *expr, const expr_t *base, const expr_t *exponent_core)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    bool is_sub = false;

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        expr_t *left_out = mat_cancel_reciprocal_power(left, base, exponent_core);
        expr_t *right_out = mat_cancel_reciprocal_power(right, base, exponent_core);

        if (!left_out || !right_out) {
            expr_free(right_out);
            expr_free(left_out);
            return NULL;
        }
        return is_sub ? expr_sub_simplify_owned(left_out, right_out)
                      : expr_add_simplify_owned(left_out, right_out);
    }
    if (expr_match_mul_expr(expr, &left, &right)) {
        bool left_contains = mat_expr_contains_power_family(left, base, exponent_core, false);
        expr_t *reduced = mat_cancel_reciprocal_power(left_contains ? left : right, base, exponent_core);

        return reduced ? expr_mul_simplify_owned(reduced, expr_clone(left_contains ? right : left)) : NULL;
    }
    if (expr_match_div_expr(expr, &numerator, &denominator)) {
        const expr_t *denominator_left = NULL;
        const expr_t *denominator_right = NULL;

        if (expr_match_mul_expr(denominator, &denominator_left, &denominator_right)) {
            const expr_t *power = NULL;
            const expr_t *remainder = NULL;
            const expr_t *power_base = NULL;
            const expr_t *power_exponent = NULL;

            if (expr_match_pow_expr(denominator_left, &power_base, &power_exponent) &&
                expr_simplify_same_factor(power_base, base)) {
                power = denominator_left;
                remainder = denominator_right;
            } else if (expr_match_pow_expr(denominator_right, &power_base, &power_exponent) &&
                       expr_simplify_same_factor(power_base, base)) {
                power = denominator_right;
                remainder = denominator_left;
            }
            if (power && remainder) {
                expr_t *reduced_exponent =
                    expr_sub_simplify_owned(expr_clone(power_exponent), expr_clone(exponent_core));
                number_t reduced_value = reduced_exponent ? expr_eval(reduced_exponent) : num_new();
                bool reciprocal = num_is_finite(reduced_value) && num_eq(reduced_value, NUM_NEG_ONE);

                num_destroy(&reduced_value);
                expr_free(reduced_exponent);
                if (reciprocal) {
                    expr_t *new_numerator = expr_mul_simplify_owned(expr_clone(numerator), expr_clone(base));

                    return new_numerator
                               ? expr_div_simplify_owned(new_numerator, expr_clone(remainder))
                               : NULL;
                }
            }
        }
        expr_t *reduced_denominator = mat_reduce_denominator_power(denominator, base, exponent_core);

        return reduced_denominator
                   ? expr_div_simplify_owned(expr_clone(numerator), reduced_denominator)
                   : NULL;
    }
    return NULL;
}

static bool mat_top_level_has_reciprocal_factor(const expr_t *expr, const expr_t *denominator)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *numerator = NULL;
    const expr_t *candidate_denominator = NULL;
    const expr_t *negated = NULL;
    bool subtraction = false;

    if (!expr || !denominator)
        return false;
    if (expr_match_div_expr(expr, &numerator, &candidate_denominator) &&
        expr_simplify_same_factor(candidate_denominator, denominator)) {
        return true;
    }
    if (expr_match_mul_expr(expr, &left, &right))
        return mat_top_level_has_reciprocal_factor(left, denominator) ||
               mat_top_level_has_reciprocal_factor(right, denominator);
    if (expr_match_add_sub_expr(expr, &left, &right, &subtraction))
        return mat_top_level_has_reciprocal_factor(left, denominator) &&
               mat_top_level_has_reciprocal_factor(right, denominator);
    return expr_match_neg_expr(expr, &negated) && mat_top_level_has_reciprocal_factor(negated, denominator);
}

static expr_t *mat_find_common_reciprocal_factor(const expr_t *expr, expr_t *const *entries, size_t count)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *negated = NULL;
    bool subtraction = false;
    expr_t *found;

    if (!expr)
        return NULL;
    if (expr_match_div_expr(expr, &numerator, &denominator)) {
        size_t index;

        for (index = 1u; index < count; ++index)
            if (!mat_top_level_has_reciprocal_factor(entries[index], denominator))
                break;
        if (index == count)
            return expr_div_simplify_owned(expr_const_one(), expr_clone(denominator));
    }
    if (expr_match_mul_expr(expr, &left, &right) || expr_match_add_sub_expr(expr, &left, &right, &subtraction)) {
        found = mat_find_common_reciprocal_factor(left, entries, count);
        return found ? found : mat_find_common_reciprocal_factor(right, entries, count);
    }
    return expr_match_neg_expr(expr, &negated) ? mat_find_common_reciprocal_factor(negated, entries, count) : NULL;
}

static expr_t *mat_cancel_reciprocal_factor(const expr_t *expr, const expr_t *denominator)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *numerator = NULL;
    const expr_t *candidate_denominator = NULL;
    const expr_t *negated = NULL;
    bool subtraction = false;

    if (!expr || !denominator)
        return NULL;
    if (expr_match_div_expr(expr, &numerator, &candidate_denominator) &&
        expr_simplify_same_factor(candidate_denominator, denominator)) {
        return expr_simplify(numerator);
    }
    if (expr_match_mul_expr(expr, &left, &right)) {
        expr_t *reduced = mat_cancel_reciprocal_factor(left, denominator);

        if (reduced)
            return expr_mul_simplify_owned(reduced, expr_clone(right));
        reduced = mat_cancel_reciprocal_factor(right, denominator);
        return reduced ? expr_mul_simplify_owned(expr_clone(left), reduced) : NULL;
    }
    if (expr_match_add_sub_expr(expr, &left, &right, &subtraction)) {
        expr_t *left_out = mat_cancel_reciprocal_factor(left, denominator);
        expr_t *right_out = mat_cancel_reciprocal_factor(right, denominator);

        if (!left_out || !right_out) {
            expr_free(right_out);
            expr_free(left_out);
            return NULL;
        }
        return subtraction ? expr_sub_simplify_owned(left_out, right_out)
                           : expr_add_simplify_owned(left_out, right_out);
    }
    if (!expr_match_neg_expr(expr, &negated))
        return NULL;
    return expr_negate_owned(mat_cancel_reciprocal_factor(negated, denominator));
}

/* Extract one factor shared by every completed expression entry. */
int mat_simplify_common_expression_factor(expr_t **entries, size_t count, expr_t **factor_out)
{
    expr_t **quotients;
    expr_t *factor;

    if (!factor_out)
        return -1;
    *factor_out = NULL;
    if (!entries || count == 0u)
        return 0;
    factor = mat_find_common_reciprocal_symbolic_power(entries[0], entries, count, false);
    if (!factor)
        factor = mat_find_common_reciprocal_factor(entries[0], entries, count);
    if (!factor)
        factor = expr_simplify_find_common_factor(entries, count);
    if (!factor)
        return 0;
    quotients = calloc(count, sizeof(*quotients));
    if (!quotients) {
        expr_free(factor);
        return -1;
    }
    for (size_t index = 0u; index < count; ++index) {
        const expr_t *factor_numerator = NULL;
        const expr_t *factor_denominator = NULL;
        number_t factor_numerator_value = num_new();
        bool reciprocal_factor = expr_match_div_expr(factor, &factor_numerator, &factor_denominator) &&
                                 expr_match_const_value(factor_numerator, &factor_numerator_value) &&
                                 num_eq(factor_numerator_value, NUM_ONE);
        const expr_t *power_base = NULL;
        const expr_t *power_exponent = NULL;

        num_destroy(&factor_numerator_value);
        if (reciprocal_factor && expr_match_pow_expr(factor_denominator, &power_base, &power_exponent)) {
            quotients[index] = mat_cancel_reciprocal_power(entries[index], power_base, power_exponent);
        } else {
            quotients[index] = expr_simplify_extract_common_factor_quotient(entries[index], factor);
            if (!quotients[index] && reciprocal_factor)
                quotients[index] = mat_cancel_reciprocal_factor(entries[index], factor_denominator);
        }
        if (!quotients[index]) {
            expr_t *raw = expr_div(entries[index], factor);

            quotients[index] = expr_simplify_owned(raw);
            if (quotients[index]) {
                expr_t *resimplified = expr_simplify(quotients[index]);

                if (resimplified) {
                    expr_free(quotients[index]);
                    quotients[index] = resimplified;
                }
            }
        }
        if (!quotients[index]) {
            for (size_t cleanup = 0u; cleanup < count; ++cleanup)
                expr_free(quotients[cleanup]);
            free(quotients);
            expr_free(factor);
            return 0;
        }
    }
    for (size_t index = 0u; index < count; ++index) {
        expr_free(entries[index]);
        entries[index] = quotients[index];
    }
    free(quotients);
    *factor_out = factor;
    return 0;
}
