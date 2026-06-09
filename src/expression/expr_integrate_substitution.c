#include <stdbool.h>
#include <stdlib.h>

#include "expr_internal.h"
#include "expr_integrate_internal.h"
#include "internal/number_internal.h"

typedef struct {
    expr_t *base;
    number_t exponent;
} integrate_factor_t;

static void integrate_free_factors_local(integrate_factor_t *factors, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        expr_free(factors[i].base);
        num_destroy(&factors[i].exponent);
    }
    free(factors);
}

static bool integrate_append_factor_local(integrate_factor_t **factors,
                                          size_t *count,
                                          size_t *capacity,
                                          const expr_t *base,
                                          number_t exponent)
{
    for (size_t i = 0; i < *count; ++i) {
        if (expr_equal_exact_local((*factors)[i].base, base)) {
            number_t sum = num_add((*factors)[i].exponent, exponent);

            num_destroy(&(*factors)[i].exponent);
            (*factors)[i].exponent = sum;
            return true;
        }
    }

    if (*count == *capacity) {
        size_t next_capacity = *capacity ? (*capacity * 2u) : 8u;
        integrate_factor_t *grown = realloc(*factors, next_capacity * sizeof(**factors));

        if (!grown)
            return false;
        *factors = grown;
        *capacity = next_capacity;
    }

    (*factors)[*count].base = expr_integrate_clone_expr(base);
    if (!(*factors)[*count].base)
        return false;
    (*factors)[*count].exponent = num_clone(exponent);
    ++(*count);
    return true;
}

static bool integrate_split_factors_local(const expr_t *expr,
                                          number_t sign,
                                          number_t *coeff_io,
                                          integrate_factor_t **factors,
                                          size_t *count,
                                          size_t *capacity)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t const_value = num_new();
    number_t exponent = num_new();
    bool ok = false;

    if (!expr)
        return true;

    if (expr_match_mul_expr(expr, &left, &right)) {
        ok = integrate_split_factors_local(left, sign, coeff_io, factors, count, capacity) &&
             integrate_split_factors_local(right, sign, coeff_io, factors, count, capacity);
        goto cleanup;
    }

    if (expr && expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b) {
        number_t neg_sign = num_neg(sign);

        ok = integrate_split_factors_local(expr->a, sign, coeff_io, factors, count, capacity) &&
             integrate_split_factors_local(expr->b, neg_sign, coeff_io, factors, count, capacity);
        num_destroy(&neg_sign);
        goto cleanup;
    }

    if (expr_match_const_value(expr, &const_value)) {
        number_t updated = num_lt(sign, NUM_ZERO) ? num_div(*coeff_io, const_value)
                                                  : num_mul(*coeff_io, const_value);

        num_destroy(coeff_io);
        *coeff_io = updated;
        ok = true;
        goto cleanup;
    }

    if (expr && expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        exponent = num_mul(expr->c, sign);
        ok = integrate_append_factor_local(factors, count, capacity, expr->a, exponent);
        goto cleanup;
    }

    if (expr && expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b &&
        expr_match_const_value(expr->b, &exponent)) {
        number_t signed_exponent = num_mul(exponent, sign);

        ok = integrate_append_factor_local(factors, count, capacity, expr->a, signed_exponent);
        num_destroy(&signed_exponent);
        goto cleanup;
    }

    ok = integrate_append_factor_local(factors, count, capacity, expr, sign);

cleanup:
    num_destroy(&exponent);
    num_destroy(&const_value);
    return ok;
}

static expr_t *integrate_rebuild_factor_product_local(number_t coeff,
                                                      integrate_factor_t *factors,
                                                      size_t count)
{
    expr_t *out = expr_new_const(coeff);

    if (!out)
        return NULL;

    for (size_t i = 0; i < count; ++i) {
        expr_t *factor = NULL;
        expr_t *next = NULL;

        if (num_eq(factors[i].exponent, NUM_ZERO))
            continue;
        if (num_eq(factors[i].exponent, NUM_ONE)) {
            factor = expr_integrate_clone_expr(factors[i].base);
        } else {
            factor = expr_pow(factors[i].base, &factors[i].exponent);
        }
        if (!factor) {
            expr_free(out);
            return NULL;
        }
        next = expr_mul(out, factor);
        expr_free(out);
        expr_free(factor);
        if (!next)
            return NULL;
        out = next;
    }

    return simplify_owned(out);
}

static bool collect_substitution_candidates(const expr_t *expr,
                                            const expr_t *root,
                                            const expr_t *wrt,
                                            const expr_t **candidates,
                                            size_t *count,
                                            size_t capacity)
{
    if (!expr || !count || *count >= capacity)
        return true;

    if (expr->a &&
        !collect_substitution_candidates(expr->a, root, wrt, candidates, count, capacity))
        return false;
    if (expr->b &&
        !collect_substitution_candidates(expr->b, root, wrt, candidates, count, capacity))
        return false;

    if (expr == root || expr == wrt || expr->ops->arity == EXPR_OP_ATOM ||
        !depends_on_wrt(expr, wrt)) {
        return true;
    }

    for (size_t i = 0; i < *count; ++i) {
        if (candidates[i] == expr)
            return true;
    }

    candidates[*count] = expr;
    ++(*count);
    return true;
}

static expr_t *substitute_candidate_with_powers(const expr_t *expr,
                                                const expr_t *candidate,
                                                const expr_t *replacement)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr)
        return NULL;

    if (expr == candidate || expr_equal_exact_local(expr, candidate)) {
        expr_retain(replacement);
        return (expr_t *)replacement;
    }

    for (int power = 2; power <= 4; ++power) {
        expr_t *candidate_power = NULL;
        expr_t *replacement_power = NULL;
        bool equal = false;
        number_t exponent = num_create_from_long(power);

        candidate_power = expr_pow(candidate, &exponent);
        replacement_power = expr_pow(replacement, &exponent);
        if (candidate_power)
            equal = expr_equal_exact_local(expr, candidate_power);
        if (equal) {
            expr_free(candidate_power);
            num_destroy(&exponent);
            return replacement_power;
        }
        expr_free(replacement_power);
        expr_free(candidate_power);
        num_destroy(&exponent);
    }

    if (expr->ops->kind == EXPR_KIND_CONST) {
        if (expr->name && *expr->name)
            return expr_new_named_const(expr->c, expr->name);
        return expr_new_const(expr->c);
    }

    if (expr->ops->kind == EXPR_KIND_VAR) {
        if (expr->name && *expr->name)
            return expr_new_named_var(expr->x, expr->name);
        return expr_new_var(expr->x);
    }

    if (expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        left = substitute_candidate_with_powers(expr->a, candidate, replacement);
        if (!left)
            return NULL;
        out = expr_pow(left, &expr->c);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_UNARY && expr->ops->apply_unary) {
        left = substitute_candidate_with_powers(expr->a, candidate, replacement);
        if (!left)
            return NULL;
        out = expr->ops->apply_unary(left);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_BINARY && expr->ops->apply_binary) {
        left = substitute_candidate_with_powers(expr->a, candidate, replacement);
        right = substitute_candidate_with_powers(expr->b, candidate, replacement);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr->ops->apply_binary(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    return NULL;
}

static expr_t *extract_exact_factor_quotient(const expr_t *expr, const expr_t *factor)
{
    integrate_factor_t *expr_factors = NULL;
    integrate_factor_t *factor_factors = NULL;
    size_t expr_count = 0u;
    size_t factor_count = 0u;
    size_t expr_capacity = 0u;
    size_t factor_capacity = 0u;
    number_t expr_coeff = num_new();
    number_t factor_coeff = num_new();
    expr_t *out = NULL;

    if (!expr || !factor)
        goto cleanup;
    if (!integrate_split_factors_local(expr, NUM_ONE, &expr_coeff, &expr_factors,
                                       &expr_count, &expr_capacity) ||
        !integrate_split_factors_local(factor, NUM_ONE, &factor_coeff, &factor_factors,
                                       &factor_count, &factor_capacity) ||
        num_eq(factor_coeff, NUM_ZERO)) {
        goto cleanup;
    }

    for (size_t i = 0; i < factor_count; ++i) {
        bool matched = false;

        for (size_t j = 0; j < expr_count; ++j) {
            if (expr_equal_exact_local(expr_factors[j].base, factor_factors[i].base)) {
                number_t updated = num_sub(expr_factors[j].exponent, factor_factors[i].exponent);

                num_destroy(&expr_factors[j].exponent);
                expr_factors[j].exponent = updated;
                matched = true;
                break;
            }
        }

        if (!matched)
            goto cleanup;
    }

    {
        number_t quotient_coeff = num_div(expr_coeff, factor_coeff);
        out = integrate_rebuild_factor_product_local(quotient_coeff, expr_factors, expr_count);
        num_destroy(&quotient_coeff);
    }

cleanup:
    num_destroy(&factor_coeff);
    num_destroy(&expr_coeff);
    integrate_free_factors_local(factor_factors, factor_count);
    integrate_free_factors_local(expr_factors, expr_count);
    return out;
}

static expr_t *integrate_exact_substitution_candidate(
    const expr_t *expr,
    const expr_t *wrt,
    const expr_t *candidate)
{
    expr_t *du = NULL;
    expr_t *ratio = NULL;
    expr_t *u = NULL;
    expr_t *transformed = NULL;
    expr_t *quotient = NULL;
    expr_t *anti_u = NULL;
    expr_t *back = NULL;
    expr_t *out = NULL;
    expr_t *vars[2];
    bool used[2];

    if (!expr || !wrt || !candidate || candidate == wrt)
        return NULL;

    du = expr_create_deriv(candidate, wrt);
    du = simplify_owned(du);
    if (!du || expr_is_exact_zero(du))
        goto cleanup;

    u = expr_new_named_var(NUM_ZERO, "u");
    quotient = extract_exact_factor_quotient(expr, du);
    if (!quotient) {
        ratio = expr_div(expr, du);
        quotient = simplify_owned(ratio);
        ratio = NULL;
    }
    if (!quotient)
        goto cleanup;

    transformed = u ? substitute_candidate_with_powers(quotient, candidate, u) : NULL;
    transformed = simplify_owned(transformed);
    if (!transformed)
        goto cleanup;

    vars[0] = (expr_t *)wrt;
    vars[1] = u;
    if (!expr_collect_var_usage(transformed, 2u, vars, used) || used[0])
        goto cleanup;

    anti_u = expr_integrate(transformed, u);
    if (!anti_u)
        goto cleanup;

    back = expr_substitute(anti_u, u, (expr_t *)candidate);
    out = simplify_owned(back);
    back = NULL;

    if (out) {
        expr_t *deriv = expr_create_deriv(out, wrt);
        expr_t *deriv_simplified = simplify_owned(deriv);
        expr_t *difference = NULL;
        expr_t *difference_simplified = NULL;

        if (deriv_simplified && !expr_equal_exact_local(deriv_simplified, expr)) {
            difference = expr_sub(deriv_simplified, expr);
            difference_simplified = simplify_owned(difference);
            difference = NULL;
        }

        if (!deriv_simplified ||
            (!expr_equal_exact_local(deriv_simplified, expr) &&
             (!difference_simplified || !expr_is_exact_zero(difference_simplified)))) {
            expr_free(difference_simplified);
            expr_free(deriv_simplified);
            expr_free(out);
            out = NULL;
        } else {
            expr_free(difference_simplified);
            expr_free(deriv_simplified);
        }
    }

cleanup:
    expr_free(back);
    expr_free(anti_u);
    expr_free(transformed);
    expr_free(quotient);
    expr_free(u);
    expr_free(ratio);
    expr_free(du);
    return out;
}

static __attribute__((unused)) expr_t *integrate_by_exact_substitution(const expr_t *expr,
                                                                       const expr_t *wrt)
{
    const expr_t *candidates[32];
    size_t count = 0u;
    number_t four = num_create_from_long(4);

    if (!expr || !wrt || expr->ops->arity == EXPR_OP_ATOM)
        goto cleanup;

    if (!collect_substitution_candidates(expr, expr, wrt, candidates, &count,
                                         sizeof(candidates) / sizeof(candidates[0]))) {
        goto cleanup;
    }

    for (size_t i = 0; i < count; ++i) {
        expr_t *out = integrate_exact_substitution_candidate(expr, wrt, candidates[i]);
        number_t exponent = num_new();

        if (out) {
            num_destroy(&four);
            return out;
        }

        if (candidates[i] &&
            ((candidates[i]->ops->kind == EXPR_KIND_POW_D &&
              num_is_real(candidates[i]->c) &&
              num_eq(candidates[i]->c, four)) ||
             (candidates[i]->ops->kind == EXPR_KIND_POW &&
              candidates[i]->b &&
              expr_match_const_value(candidates[i]->b, &exponent) &&
              num_eq(exponent, four)))) {
            expr_t *half_power = expr_pow(candidates[i]->a, &NUM_TWO);

            out = integrate_exact_substitution_candidate(expr, wrt, half_power);
            expr_free(half_power);
            if (out) {
                num_destroy(&exponent);
                num_destroy(&four);
                return out;
            }
        }
        num_destroy(&exponent);
    }

cleanup:
    num_destroy(&four);
    return NULL;
}
