#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "expr_internal.h"
#include "expr_integrate_internal.h"
#include "internal/number_internal.h"

typedef expr_t *(*expr_integrate_rule_fn)(const expr_t *expr, const expr_t *wrt);

typedef struct expr_integrate_dispatch_rule {
    expr_integrate_rule_fn structural;
    expr_integrate_rule_fn primitive;
} expr_integrate_dispatch_rule_t;

static expr_t *integrate_dispatch(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_poly_times_unary_affine_kind(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind);
static expr_t *integrate_poly_times_log_affine(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_poly_over_matching_affine(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_poly_over_centered_quadratic(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_inverse_symbolic_square_sum(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_linear_poly_over_centered_quadratic_root(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_linear_poly_times_centered_quadratic_root(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_centered_quadratic_root(const expr_t *quadratic, const expr_t *wrt);
static expr_t *integrate_sqrt_one_plus_minus_affine_square(const expr_t *quadratic, const expr_t *wrt);
static expr_t *integrate_poly_times_affine_power(const expr_t *expr, const expr_t *wrt);
static expr_t *integrate_squared_unary_affine(const expr_t *expr,
                                              const expr_t *wrt,
                                              expr_pattern_unary_affine_kind_t kind);
static expr_t *integrate_cubed_unary_affine(const expr_t *expr,
                                            const expr_t *wrt,
                                            expr_pattern_unary_affine_kind_t kind);
static expr_t *integrate_same_affine_special_product(const expr_t *expr, const expr_t *wrt);
static __attribute__((unused)) expr_t *integrate_by_exact_substitution(const expr_t *expr, const expr_t *wrt);
static expr_t *clone_expr_local(const expr_t *expr);

static expr_t *div_number_owned_by_product(expr_t *expr, number_t left, number_t right)
{
    number_t denom = num_mul(left, right);

    return div_number_owned_consuming(expr, &denom);
}

static expr_t *div_number_owned_by_long_product(expr_t *expr, long left, number_t right)
{
    number_t factor = num_create_from_long(left);
    expr_t *out = div_number_owned_by_product(expr, factor, right);

    num_destroy(&factor);
    return out;
}

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

    (*factors)[*count].base = clone_expr_local(base);
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
            factor = clone_expr_local(factors[i].base);
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

static __attribute__((unused)) bool collect_substitution_candidates(const expr_t *expr,
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

static expr_t *clone_expr_local(const expr_t *expr)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr)
        return NULL;

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
        left = clone_expr_local(expr->a);
        if (!left)
            return NULL;
        out = expr_pow(left, &expr->c);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_UNARY && expr->ops->apply_unary) {
        left = clone_expr_local(expr->a);
        if (!left)
            return NULL;
        out = expr->ops->apply_unary(left);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_BINARY && expr->ops->apply_binary) {
        left = clone_expr_local(expr->a);
        right = clone_expr_local(expr->b);
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

static __attribute__((unused)) expr_t *integrate_exact_substitution_candidate(
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

static __attribute__((unused)) expr_t *integrate_by_exact_substitution(const expr_t *expr, const expr_t *wrt)
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

static expr_t *integrate_as_constant(const expr_t *expr, const expr_t *wrt)
{
    return simplify_owned(expr_mul(expr, wrt));
}

static expr_t *integrate_power_of_wrt(const expr_t *base,
                                      number_t exponent,
                                      const expr_t *wrt)
{
    number_t next_exponent;
    expr_t *power;
    expr_t *out;

    if (!is_wrt(base, wrt))
        return NULL;
    if (num_eq(exponent, NUM_NEG_ONE))
        return simplify_owned(expr_log(wrt));

    next_exponent = num_add(exponent, NUM_ONE);
    if (num_eq(next_exponent, NUM_ZERO)) {
        num_destroy(&next_exponent);
        return NULL;
    }

    power = expr_pow(wrt, &next_exponent);
    out = div_number_owned(power, next_exponent);
    num_destroy(&next_exponent);
    return out;
}

static expr_t *integrate_power_of_affine(const expr_t *base,
                                         number_t exponent,
                                         const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t next_exponent;
    expr_t *power;
    expr_t *out;

    if (!match_nonconstant_affine_linear_expr(base, wrt, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }
    if (num_eq(exponent, NUM_NEG_ONE)) {
        expr_t *log_base = expr_log(base);

        num_destroy(&constant);
        return div_number_owned_consuming(log_base, &coeff);
    }

    next_exponent = num_add(exponent, NUM_ONE);
    if (num_eq(next_exponent, NUM_ZERO)) {
        num_destroy(&next_exponent);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    power = expr_pow(base, &next_exponent);
    out = power ? div_number_owned_by_product(power, coeff, next_exponent) : NULL;

    num_destroy(&next_exponent);
    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

static number_t integrate_pow_small_number(number_t base, size_t exponent)
{
    number_t out = num_clone(NUM_ONE);

    for (size_t i = 0; i < exponent; ++i) {
        number_t next = num_mul(out, base);

        num_destroy(&out);
        out = next;
    }
    return out;
}

static bool integrate_long_perfect_square_root(long value, long *root_out)
{
    long lo = 0L;
    long hi = value;

    if (value < 0L || !root_out)
        return false;

    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2L;

        if (mid != 0L && mid > value / mid) {
            hi = mid - 1L;
            continue;
        }

        long square = mid * mid;

        if (square == value) {
            *root_out = mid;
            return true;
        }
        if (square < value) {
            lo = mid + 1L;
        } else {
            hi = mid - 1L;
        }
    }

    return false;
}

static bool integrate_exact_small_rational_sqrt(number_t value, number_t *root_out)
{
    long numerator;
    long denominator;
    long root_numerator;
    long root_denominator;
    number_t numerator_expr;
    number_t denominator_expr;

    if (!root_out ||
        !num_get_small_rational(value, &numerator, &denominator) ||
        numerator < 0L ||
        denominator <= 0L ||
        !integrate_long_perfect_square_root(numerator, &root_numerator) ||
        !integrate_long_perfect_square_root(denominator, &root_denominator) ||
        root_denominator == 0L) {
        return false;
    }

    numerator_expr = num_create_from_long(root_numerator);
    denominator_expr = num_create_from_long(root_denominator);
    num_destroy(root_out);
    *root_out = num_div(numerator_expr, denominator_expr);
    num_destroy(&denominator_expr);
    num_destroy(&numerator_expr);
    return true;
}

static bool rewrite_poly_deg4_to_affine_basis(number_t *poly,
                                              number_t from_constant,
                                              number_t from_coeff,
                                              number_t to_constant,
                                              number_t to_coeff)
{
    static const long binomial[5][5] = {
        { 1, 0, 0, 0, 0 },
        { 1, 1, 0, 0, 0 },
        { 1, 2, 1, 0, 0 },
        { 1, 3, 3, 1, 0 },
        { 1, 4, 6, 4, 1 }
    };
    number_t alpha;
    number_t scaled_to_constant;
    number_t beta;
    number_t rewritten[5];

    if (num_eq(from_constant, to_constant) && num_eq(from_coeff, to_coeff))
        return true;
    if (num_is_zero(from_constant) && num_is_zero(from_coeff))
        return true;
    if (num_is_zero(to_coeff))
        return false;

    alpha = num_div(from_coeff, to_coeff);
    scaled_to_constant = num_mul(alpha, to_constant);
    beta = num_sub(from_constant, scaled_to_constant);
    number_array_zero_local(rewritten, 5);

    for (size_t i = 0; i < 5u; ++i) {
        if (num_is_zero(poly[i]))
            continue;

        for (size_t j = 0; j <= i; ++j) {
            number_t alpha_power = integrate_pow_small_number(alpha, j);
            number_t beta_power = integrate_pow_small_number(beta, i - j);
            number_t power_product = num_mul(alpha_power, beta_power);
            number_t choose = num_create_from_long(binomial[i][j]);
            number_t scale = num_mul(power_product, choose);
            number_t term = num_mul(poly[i], scale);
            number_t next = num_add(rewritten[j], term);

            num_destroy(&rewritten[j]);
            rewritten[j] = next;
            num_destroy(&term);
            num_destroy(&scale);
            num_destroy(&choose);
            num_destroy(&power_product);
            num_destroy(&beta_power);
            num_destroy(&alpha_power);
        }
    }

    for (size_t i = 0; i < 5u; ++i) {
        num_destroy(&poly[i]);
        poly[i] = rewritten[i];
    }

    num_destroy(&beta);
    num_destroy(&scaled_to_constant);
    num_destroy(&alpha);
    return true;
}

static bool match_affine_power_factor(const expr_t *expr,
                                      const expr_t *wrt,
                                      number_t *constant_out,
                                      number_t *coeff_out,
                                      number_t *exponent_out)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !constant_out || !coeff_out || !exponent_out) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_SQRT && expr->a) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(NUM_HALF);
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(expr->c);
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent)) {
        ok = match_nonconstant_affine_linear_expr(expr->a, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(exponent);
        }
    } else {
        ok = match_nonconstant_affine_linear_expr(expr, wrt, constant_out,
                                                  coeff_out);
        if (ok) {
            num_destroy(exponent_out);
            *exponent_out = num_clone(NUM_ONE);
        }
    }

    num_destroy(&exponent);
    return ok;
}

static expr_t *build_integrated_poly_affine_power(const number_t *poly,
                                                  number_t power_constant,
                                                  number_t power_coeff,
                                                  number_t exponent,
                                                  const expr_t *wrt)
{
    expr_t *u = build_affine_from_match(wrt, power_constant, power_coeff);
    expr_t *sum = NULL;

    if (!u)
        return NULL;

    for (size_t i = 0; i < 5u; ++i) {
        number_t degree = num_create_from_long((long)i);
        number_t integrand_exponent = num_add(exponent, degree);
        number_t next_exponent = num_add(integrand_exponent, NUM_ONE);
        expr_t *term = NULL;

        if (!num_is_zero(poly[i])) {
            if (num_eq(next_exponent, NUM_ZERO)) {
                expr_t *log_u = expr_log(u);

                term = log_u ? mul_number_owned(log_u, poly[i]) : NULL;
                term = div_number_owned(term, power_coeff);
            } else {
                expr_t *u_power = expr_pow(u, &next_exponent);
                number_t denom = num_mul(power_coeff, next_exponent);

                term = u_power ? mul_number_owned(u_power, poly[i]) : NULL;
                term = div_number_owned_consuming(term, &denom);
            }
        }

        if (term) {
            if (sum) {
                expr_t *next = expr_add(sum, term);

                expr_free(sum);
                expr_free(term);
                sum = next;
            } else {
                sum = term;
            }
        }

        num_destroy(&next_exponent);
        num_destroy(&integrand_exponent);
        num_destroy(&degree);
    }

    expr_free(u);
    return simplify_owned(sum);
}

static bool is_wrt_symbolic_affine_leaf(const expr_t *expr, const expr_t *wrt)
{
    if (is_wrt(expr, wrt))
        return true;
    return expr &&
           wrt &&
           expr_is_var(expr) &&
           expr_is_var(wrt) &&
           expr->name &&
           wrt->name &&
           strcmp(expr->name, wrt->name) == 0;
}

static bool is_negated_wrt_symbolic_affine_leaf(const expr_t *expr, const expr_t *wrt)
{
    return expr &&
           expr_is_neg(expr) &&
           is_wrt_symbolic_affine_leaf(expr->a, wrt);
}

static bool match_symbolic_unit_affine_base(const expr_t *base,
                                            const expr_t *wrt,
                                            number_t *coeff_out)
{
    number_t coeff;

    if (!base || !wrt || !coeff_out)
        return false;

    if (is_wrt_symbolic_affine_leaf(base, wrt)) {
        coeff = num_clone(NUM_ONE);
    } else if (is_negated_wrt_symbolic_affine_leaf(base, wrt)) {
        coeff = num_clone(NUM_NEG_ONE);
    } else if (expr_is_op(base, &ops_add)) {
        if (is_wrt_symbolic_affine_leaf(base->a, wrt) &&
            !depends_on_wrt(base->b, wrt)) {
            coeff = num_clone(NUM_ONE);
        } else if (is_wrt_symbolic_affine_leaf(base->b, wrt) &&
                   !depends_on_wrt(base->a, wrt)) {
            coeff = num_clone(NUM_ONE);
        } else if (is_negated_wrt_symbolic_affine_leaf(base->a, wrt) &&
                   !depends_on_wrt(base->b, wrt)) {
            coeff = num_clone(NUM_NEG_ONE);
        } else if (is_negated_wrt_symbolic_affine_leaf(base->b, wrt) &&
                   !depends_on_wrt(base->a, wrt)) {
            coeff = num_clone(NUM_NEG_ONE);
        } else {
            return false;
        }
    } else if (expr_is_op(base, &ops_sub)) {
        if (is_wrt_symbolic_affine_leaf(base->a, wrt) &&
            !depends_on_wrt(base->b, wrt)) {
            coeff = num_clone(NUM_ONE);
        } else if (is_wrt_symbolic_affine_leaf(base->b, wrt) &&
                   !depends_on_wrt(base->a, wrt)) {
            coeff = num_clone(NUM_NEG_ONE);
        } else {
            return false;
        }
    } else {
        return false;
    }

    num_destroy(coeff_out);
    *coeff_out = coeff;
    return true;
}

static expr_t *integrate_power_of_symbolic_unit_affine(const expr_t *base,
                                                       number_t exponent,
                                                       const expr_t *wrt)
{
    number_t coeff = num_new();
    number_t next_exponent;
    expr_t *power;
    expr_t *out;

    if (!match_symbolic_unit_affine_base(base, wrt, &coeff)) {
        num_destroy(&coeff);
        return NULL;
    }
    if (num_eq(exponent, NUM_NEG_ONE)) {
        expr_t *log_base = expr_log(base);

        return div_number_owned_consuming(log_base, &coeff);
    }

    next_exponent = num_add(exponent, NUM_ONE);
    if (num_eq(next_exponent, NUM_ZERO)) {
        num_destroy(&next_exponent);
        num_destroy(&coeff);
        return NULL;
    }

    power = expr_pow(base, &next_exponent);
    out = power ? div_number_owned_by_product(power, coeff, next_exponent) : NULL;

    num_destroy(&next_exponent);
    num_destroy(&coeff);
    return out;
}

static expr_t *integrate_power_of_wrt_symbolic_exponent(const expr_t *base,
                                                        const expr_t *exponent,
                                                        const expr_t *wrt)
{
    expr_t *next_exponent;
    expr_t *power;
    expr_t *quotient;
    expr_t *out;

    if (!is_wrt(base, wrt) || depends_on_wrt(exponent, wrt))
        return NULL;

    next_exponent = expr_add_num(exponent, &NUM_ONE);
    power = next_exponent ? expr_pow_xp(wrt, next_exponent) : NULL;
    quotient = (power && next_exponent) ? expr_div(power, next_exponent) : NULL;

    expr_free(power);
    expr_free(next_exponent);

    out = simplify_owned(quotient);
    return out;
}

static expr_t *integrate_pow_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t exponent = num_new();
    expr_t *out;

    if (!expr || !expr->a || !expr->b) {
        num_destroy(&exponent);
        return NULL;
    }

    if (!expr_match_const_value(expr->b, &exponent)) {
        out = integrate_power_of_wrt_symbolic_exponent(expr->a, expr->b, wrt);
        num_destroy(&exponent);
        return out;
    }

    if (num_eq(exponent, NUM_TWO)) {
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SIN);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COS);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SINH);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSH);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_TAN);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SEC);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSEC);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SECH);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSECH);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_TANH);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COTH);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
    }
    number_t three = num_create_from_long(3);
    if (num_eq(exponent, three)) {
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SIN);
        if (out) {
            num_destroy(&three);
            num_destroy(&exponent);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COS);
        if (out) {
            num_destroy(&three);
            num_destroy(&exponent);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_TAN);
        if (out) {
            num_destroy(&three);
            num_destroy(&exponent);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SEC);
        if (out) {
            num_destroy(&three);
            num_destroy(&exponent);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSEC);
        if (out) {
            num_destroy(&three);
            num_destroy(&exponent);
            return out;
        }
    }
    num_destroy(&three);

    out = integrate_power_of_wrt(expr->a, exponent, wrt);
    if (out) {
        num_destroy(&exponent);
        return out;
    }

    out = integrate_power_of_affine(expr->a, exponent, wrt);
    if (!out)
        out = integrate_power_of_symbolic_unit_affine(expr->a, exponent, wrt);
    num_destroy(&exponent);
    return out;
}

static expr_t *integrate_constant_over_power_denominator(const expr_t *numerator,
                                                         const expr_t *denominator,
                                                         const expr_t *wrt)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    number_t neg_exponent;
    expr_t *inner = NULL;
    expr_t *scaled;
    expr_t *out;

    if (!numerator || !denominator || depends_on_wrt(numerator, wrt)) {
        num_destroy(&exponent);
        return NULL;
    }

    if (expr_is_pow_d_expr(denominator)) {
        base = denominator->a;
        num_destroy(&exponent);
        exponent = num_clone(denominator->c);
    } else if (denominator->ops && denominator->ops->kind == EXPR_KIND_POW &&
               denominator->b &&
               expr_match_const_value(denominator->b, &exponent)) {
        base = denominator->a;
    } else if (denominator->ops && denominator->ops->kind == EXPR_KIND_SQRT &&
               denominator->a) {
        base = denominator->a;
        num_destroy(&exponent);
        exponent = num_clone(NUM_HALF);
    } else {
        num_destroy(&exponent);
        return NULL;
    }

    neg_exponent = num_neg(exponent);
    inner = integrate_power_of_wrt(base, neg_exponent, wrt);
    if (!inner)
        inner = integrate_power_of_affine(base, neg_exponent, wrt);
    if (!inner)
        inner = integrate_power_of_symbolic_unit_affine(base, neg_exponent, wrt);
    num_destroy(&neg_exponent);
    num_destroy(&exponent);
    if (!inner)
        return NULL;

    scaled = expr_mul(numerator, inner);
    expr_free(inner);
    out = simplify_owned(scaled);
    return out;
}

static expr_t *integrate_constant_rule(const expr_t *expr, const expr_t *wrt)
{
    if (!depends_on_wrt(expr, wrt))
        return integrate_as_constant(expr, wrt);
    return NULL;
}

static expr_t *integrate_var_rule(const expr_t *expr, const expr_t *wrt)
{
    if (is_wrt(expr, wrt))
        return integrate_power_of_wrt(expr, NUM_ONE, wrt);
    return integrate_as_constant(expr, wrt);
}

static expr_t *integrate_add_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *left = integrate_dispatch(expr->a, wrt);
    expr_t *right;
    expr_t *sum;

    if (!left)
        return NULL;
    right = integrate_dispatch(expr->b, wrt);
    if (!right) {
        expr_free(left);
        return NULL;
    }
    sum = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return simplify_owned(sum);
}

static expr_t *integrate_sub_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *left = integrate_dispatch(expr->a, wrt);
    expr_t *right;
    expr_t *diff;

    if (!left)
        return NULL;
    right = integrate_dispatch(expr->b, wrt);
    if (!right) {
        expr_free(left);
        return NULL;
    }
    diff = expr_sub(left, right);
    expr_free(right);
    expr_free(left);
    return simplify_owned(diff);
}

static expr_t *integrate_neg_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *inner = integrate_dispatch(expr->a, wrt);
    expr_t *negated;

    if (!inner)
        return NULL;
    negated = expr_neg(inner);
    expr_free(inner);
    return simplify_owned(negated);
}

static expr_t *integrate_scaled_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t scale = num_new();
    const expr_t *base = NULL;
    expr_t *inner;
    expr_t *out;

    if (!expr_match_scaled_expr(expr, &scale, &base) || !base || base == expr) {
        num_destroy(&scale);
        return NULL;
    }

    inner = integrate_dispatch(base, wrt);
    if (!inner) {
        num_destroy(&scale);
        return NULL;
    }
    out = mul_number_owned(inner, scale);
    num_destroy(&scale);
    return out;
}

static bool match_poly_and_affine_power_product(const expr_t *poly_expr,
                                                const expr_t *power_expr,
                                                const expr_t *wrt,
                                                number_t *poly,
                                                number_t *power_constant,
                                                number_t *power_coeff,
                                                number_t *exponent)
{
    expr_t *vars[1];
    number_t poly_constant = num_new();
    number_t poly_coeffs[1];
    bool ok;

    vars[0] = (expr_t *)wrt;
    poly_coeffs[0] = num_new();
    ok = expr_match_affine_poly_deg4(poly_expr, 1u, vars, poly,
                                     &poly_constant, poly_coeffs) &&
         match_affine_power_factor(power_expr, wrt, power_constant, power_coeff,
                                   exponent) &&
         rewrite_poly_deg4_to_affine_basis(poly, poly_constant, poly_coeffs[0],
                                           *power_constant, *power_coeff);

    num_destroy(&poly_coeffs[0]);
    num_destroy(&poly_constant);
    return ok;
}

static expr_t *integrate_poly_times_affine_power(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t poly[5];
    number_t power_constant = num_new();
    number_t power_coeff = num_new();
    number_t exponent = num_new();
    expr_t *out = NULL;

    if (!expr)
        return NULL;

    number_array_zero_local(poly, 5);
    if (expr_match_mul_expr(expr, &left, &right)) {
        if (!match_poly_and_affine_power_product(left, right, wrt, poly,
                                                 &power_constant, &power_coeff,
                                                 &exponent)) {
            number_array_clear_local(poly, 5);
            number_array_zero_local(poly, 5);
            if (!match_poly_and_affine_power_product(right, left, wrt, poly,
                                                     &power_constant, &power_coeff,
                                                     &exponent))
                goto cleanup;
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b) {
        if (!match_poly_and_affine_power_product(expr->a, expr->b, wrt, poly,
                                                 &power_constant, &power_coeff,
                                                 &exponent))
            goto cleanup;
        number_t neg_exponent = num_neg(exponent);

        num_destroy(&exponent);
        exponent = neg_exponent;
    } else {
        goto cleanup;
    }

    out = build_integrated_poly_affine_power(poly, power_constant, power_coeff,
                                             exponent, wrt);

cleanup:
    number_array_clear_local(poly, 5);
    num_destroy(&exponent);
    num_destroy(&power_coeff);
    num_destroy(&power_constant);
    return out;
}

static expr_t *integrate_mul_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *scaled = integrate_scaled_rule(expr, wrt);
    bool left_depends;
    bool right_depends;
    expr_t *inner;
    expr_t *product;

    if (scaled)
        return scaled;

    scaled = integrate_poly_times_affine_power(expr, wrt);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_centered_quadratic_root(expr, wrt);
    if (scaled)
        return scaled;

    scaled = integrate_poly_times_unary_affine_kind(expr, wrt, EXPR_PATTERN_UNARY_EXP);
    if (scaled)
        return scaled;
    scaled = integrate_poly_times_unary_affine_kind(expr, wrt, EXPR_PATTERN_UNARY_SIN);
    if (scaled)
        return scaled;
    scaled = integrate_poly_times_unary_affine_kind(expr, wrt, EXPR_PATTERN_UNARY_COS);
    if (scaled)
        return scaled;
    scaled = integrate_poly_times_unary_affine_kind(expr, wrt, EXPR_PATTERN_UNARY_SINH);
    if (scaled)
        return scaled;
    scaled = integrate_poly_times_unary_affine_kind(expr, wrt, EXPR_PATTERN_UNARY_COSH);
    if (scaled)
        return scaled;
    scaled = integrate_poly_times_log_affine(expr, wrt);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ATAN);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ASIN);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ACOS);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ASEC);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ACOSEC);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ACOT);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ASINH);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ACOSH);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ATANH);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ASECH);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ACOSECH);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ACOTH);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ERF);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_ERFC);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_NORMAL_PDF);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_NORMAL_CDF);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_normal_logpdf_affine(expr, wrt);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_EI);
    if (scaled)
        return scaled;
    scaled = integrate_linear_poly_times_inverse_affine(expr, wrt, EXPR_PATTERN_UNARY_E1);
    if (scaled)
        return scaled;
    scaled = integrate_same_affine_special_product(expr, wrt);
    if (scaled)
        return scaled;

    left_depends = depends_on_wrt(expr->a, wrt);
    right_depends = depends_on_wrt(expr->b, wrt);
    if (left_depends && right_depends)
        return NULL;

    if (!left_depends) {
        inner = integrate_dispatch(expr->b, wrt);
        product = inner ? expr_mul(expr->a, inner) : NULL;
    } else {
        inner = integrate_dispatch(expr->a, wrt);
        product = inner ? expr_mul(inner, expr->b) : NULL;
    }
    expr_free(inner);
    return simplify_owned(product);
}

static expr_t *integrate_log_over_proportional_affine(const expr_t *expr,
                                                      const expr_t *wrt)
{
    number_t log_constant = num_new();
    number_t log_coeff = num_new();
    number_t denom_constant = num_new();
    number_t denom_coeff = num_new();
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b ||
        !match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_LOG,
                                 &log_constant, &log_coeff) ||
        !match_nonconstant_affine_linear_expr(expr->b, wrt, &denom_constant,
                                              &denom_coeff) ||
        num_is_zero(log_coeff) || num_is_zero(denom_coeff)) {
        num_destroy(&denom_coeff);
        num_destroy(&denom_constant);
        num_destroy(&log_coeff);
        num_destroy(&log_constant);
        return NULL;
    }

    number_t scale = num_div(denom_coeff, log_coeff);
    number_t scaled_log_constant = num_mul(scale, log_constant);
    if (num_eq(denom_constant, scaled_log_constant)) {
        expr_t *log_term = expr_log(expr->a->a);
        expr_t *log_sq = log_term ? expr_pow(log_term, &NUM_TWO) : NULL;
        number_t denom = num_mul(denom_coeff, NUM_TWO);

        out = div_number_owned_consuming(log_sq, &denom);
        expr_free(log_term);
    }

    num_destroy(&scaled_log_constant);
    num_destroy(&scale);
    num_destroy(&denom_coeff);
    num_destroy(&denom_constant);
    num_destroy(&log_coeff);
    num_destroy(&log_constant);
    return out;
}

static expr_t *integrate_div_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *scaled = integrate_scaled_rule(expr, wrt);
    expr_t *inner;
    expr_t *quotient;
    expr_t *inverse_term;
    expr_t *u;
    bool is_plus_square = false;

    if (scaled)
        return scaled;

    scaled = integrate_poly_times_affine_power(expr, wrt);
    if (scaled)
        return scaled;

    if (!depends_on_wrt(expr->b, wrt)) {
        inner = integrate_dispatch(expr->a, wrt);
        quotient = inner ? expr_div(inner, expr->b) : NULL;
        expr_free(inner);
        return simplify_owned(quotient);
    }

    if (is_wrt(expr->b, wrt) && !depends_on_wrt(expr->a, wrt)) {
        expr_t *log_x = expr_log(wrt);

        quotient = log_x ? expr_mul(expr->a, log_x) : NULL;
        expr_free(log_x);
        return simplify_owned(quotient);
    }

    inner = integrate_constant_over_power_denominator(expr->a, expr->b, wrt);
    if (inner)
        return inner;

    inner = integrate_log_over_proportional_affine(expr, wrt);
    if (inner)
        return inner;
    inner = integrate_inverse_symbolic_square_sum(expr, wrt);
    if (inner)
        return inner;
    inner = integrate_linear_poly_over_centered_quadratic_root(expr, wrt);
    if (inner)
        return inner;

    number_t constant = num_new();
    number_t coeff = num_new();
    number_t numer_constant = num_new();
    number_t numer_coeff = num_new();

    if (!depends_on_wrt(expr->a, wrt) && expr_match_const_value(expr->a, &numer_constant) &&
        num_eq(numer_constant, NUM_ONE) &&
        match_one_plus_minus_affine_square(expr->b, wrt, &is_plus_square, &constant, &coeff) &&
        num_eq(constant, NUM_ZERO) && !num_eq(coeff, NUM_ZERO)) {
        expr_t *out;

        u = build_affine_from_match(wrt, constant, coeff);

        inverse_term = u ? (is_plus_square ? expr_atan(u) : expr_atanh(u)) : NULL;
        expr_free(u);
        out = div_number_owned(inverse_term, coeff);
        num_destroy(&coeff);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        num_destroy(&constant);
        return out;
    }

    if (!depends_on_wrt(expr->a, wrt) && expr_match_const_value(expr->a, &numer_constant) &&
        num_eq(numer_constant, NUM_ONE) &&
        expr->b && expr->b->ops && expr->b->ops->kind == EXPR_KIND_SQRT &&
        match_one_plus_minus_affine_square(expr->b->a, wrt, &is_plus_square, &constant, &coeff) &&
        num_eq(constant, NUM_ZERO) && !num_eq(coeff, NUM_ZERO)) {
        expr_t *out;

        u = build_affine_from_match(wrt, constant, coeff);
        inverse_term = u ? (is_plus_square ? expr_asinh(u) : expr_asin(u)) : NULL;
        expr_free(u);
        out = div_number_owned(inverse_term, coeff);
        num_destroy(&coeff);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        num_destroy(&constant);
        return out;
    }

    if (!depends_on_wrt(expr->a, wrt) &&
        match_nonconstant_affine_linear_expr(expr->b, wrt, &constant, &coeff)) {
        expr_t *log_denom = expr_log(expr->b);
        expr_t *scaled_log = log_denom ? expr_mul(expr->a, log_denom) : NULL;
        expr_t *out;

        expr_free(log_denom);
        out = div_number_owned(scaled_log, coeff);
        num_destroy(&coeff);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        num_destroy(&constant);
        return out;
    }

    if (match_nonconstant_affine_linear_expr(expr->a, wrt, &numer_constant, &numer_coeff) &&
        match_nonconstant_affine_linear_expr(expr->b, wrt, &constant, &coeff) &&
        !num_eq(coeff, NUM_ZERO)) {
        number_t linear_scale = num_div(numer_coeff, coeff);
        number_t scaled_denom_const = num_mul(linear_scale, constant);
        number_t remainder = num_sub(numer_constant, scaled_denom_const);
        expr_t *linear_term = expr_mul_num(wrt, &linear_scale);
        expr_t *log_denom = expr_log(expr->b);
        expr_t *log_term = log_denom ? expr_mul_num(log_denom, &remainder) : NULL;
        expr_t *sum = (linear_term && log_term) ? expr_add(linear_term, log_term)
                                                : (linear_term ? linear_term : log_term);
        expr_t *out;

        if (sum == linear_term)
            linear_term = NULL;
        if (sum == log_term)
            log_term = NULL;
        expr_free(log_term);
        expr_free(log_denom);
        expr_free(linear_term);
        num_destroy(&remainder);
        num_destroy(&scaled_denom_const);
        out = div_number_owned(sum, coeff);
        num_destroy(&linear_scale);
        num_destroy(&coeff);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        num_destroy(&constant);
        return out;
    }

    inner = integrate_poly_over_matching_affine(expr, wrt);
    if (inner) {
        num_destroy(&coeff);
        num_destroy(&constant);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        return inner;
    }

    inner = integrate_poly_over_centered_quadratic(expr, wrt);
    if (inner) {
        num_destroy(&coeff);
        num_destroy(&constant);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        return inner;
    }

    inner = integrate_rational_partial_fractions(expr, wrt);
    if (inner) {
        num_destroy(&coeff);
        num_destroy(&constant);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        return inner;
    }

    num_destroy(&numer_coeff);
    num_destroy(&numer_constant);
    num_destroy(&coeff);
    num_destroy(&constant);
    return NULL;
}

static expr_t *integrate_pow_d_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SIN);

    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COS);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SINH);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSH);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_TAN);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SEC);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSEC);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SECH);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSECH);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_TANH);
    if (out)
        return out;
    out = integrate_squared_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COTH);
    if (out)
        return out;

    number_t three = num_create_from_long(3);
    if (num_eq(expr->c, three)) {
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SIN);
        if (out) {
            num_destroy(&three);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COS);
        if (out) {
            num_destroy(&three);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_TAN);
        if (out) {
            num_destroy(&three);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_SEC);
        if (out) {
            num_destroy(&three);
            return out;
        }
        out = integrate_cubed_unary_affine(expr, wrt, EXPR_PATTERN_UNARY_COSEC);
        if (out) {
            num_destroy(&three);
            return out;
        }
    }
    num_destroy(&three);

    out = integrate_power_of_wrt(expr->a, expr->c, wrt);

    if (out)
        return out;
    out = integrate_power_of_affine(expr->a, expr->c, wrt);
    if (out)
        return out;
    return integrate_power_of_symbolic_unit_affine(expr->a, expr->c, wrt);
}

static expr_t *integrate_sqrt_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = expr && expr->a ? integrate_sqrt_one_plus_minus_affine_square(expr->a, wrt)
                                  : NULL;

    if (out)
        return out;
    out = expr && expr->a ? integrate_centered_quadratic_root(expr->a, wrt) : NULL;
    if (out)
        return out;
    out = integrate_power_of_wrt(expr->a, NUM_HALF, wrt);
    if (out)
        return out;
    out = integrate_power_of_affine(expr->a, NUM_HALF, wrt);
    if (out)
        return out;
    return integrate_power_of_symbolic_unit_affine(expr->a, NUM_HALF, wrt);
}

bool match_affine_unary(const expr_t *expr,
                        const expr_t *wrt,
                        expr_pattern_unary_affine_kind_t kind,
                        number_t *constant_out,
                        number_t *coeff_out)
{
    return match_affine_unary_data(expr, wrt, kind, constant_out, coeff_out);
}

static expr_t *integrate_log_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *x_log_x;
    expr_t *raw;

    number_t constant = num_new();
    number_t coeff = num_new();

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_LOG,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    x_log_x = expr_mul(expr->a, expr);
    raw = x_log_x ? expr_sub(x_log_x, expr->a) : NULL;
    expr_free(x_log_x);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

static expr_t *integrate_log10_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_log10_u;
    expr_t *u_over_ln10;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_LOG10,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_log10_u = expr_mul(expr->a, expr);
    u_over_ln10 = expr->a ? expr_div_num(expr->a, &NUM_LN10) : NULL;
    raw = (u_log10_u && u_over_ln10) ? expr_sub(u_log10_u, u_over_ln10) : NULL;

    expr_free(u_over_ln10);
    expr_free(u_log10_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_affine_unary_kind(const expr_t *expr,
                                    const expr_t *wrt,
                                    expr_pattern_unary_affine_kind_t kind,
                                    expr_apply_unary_fn antiderivative_fn,
                                    number_t sign)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *anti;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, kind, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    anti = antiderivative_fn(expr->a);
    if (num_eq(sign, NUM_NEG_ONE)) {
        expr_t *negated = anti ? expr_neg(anti) : NULL;

        expr_free(anti);
        anti = negated;
    }

    out = div_number_owned_consuming(anti, &coeffs[0]);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_poly_times_unary_affine_kind(const expr_t *expr,
                                                      const expr_t *wrt,
                                                      expr_pattern_unary_affine_kind_t kind)
{
    number_t poly[5];
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *vars[1];
    expr_t *u = NULL;
    expr_t *out = NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    if (!expr_match_affine_poly_deg4_times_unary_affine_kind(expr, kind, 1u, vars,
                                                              poly, &constant, &coeff) ||
        num_eq(coeff, NUM_ZERO)) {
        number_array_clear_local(poly, 5);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    if (!u) {
        number_array_clear_local(poly, 5);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    if (kind == EXPR_PATTERN_UNARY_EXP) {
        number_t anti[5];
        expr_t *poly_expr;
        expr_t *exp_u;

        exp_antiderivative_once_local(poly, 5u, anti);
        poly_expr = build_polynomial_expr(u, anti, 5u);
        exp_u = expr_exp(u);
        out = (poly_expr && exp_u) ? expr_mul(poly_expr, exp_u) : NULL;
        expr_free(exp_u);
        expr_free(poly_expr);
        number_array_clear_local(anti, 5);
    } else {
        number_t a_src[5];
        number_t b_src[5];
        number_t a_dst[5];
        number_t b_dst[5];
        expr_t *poly_a;
        expr_t *poly_b;
        expr_t *left;
        expr_t *right;
        expr_t *first = NULL;
        expr_t *second = NULL;
        bool trig = (kind == EXPR_PATTERN_UNARY_SIN || kind == EXPR_PATTERN_UNARY_COS);

        number_array_zero_local(a_src, 5);
        number_array_zero_local(b_src, 5);
        if (kind == EXPR_PATTERN_UNARY_SIN || kind == EXPR_PATTERN_UNARY_SINH) {
            for (size_t i = 0; i < 5u; ++i) {
                num_destroy(&a_src[i]);
                a_src[i] = num_clone(poly[i]);
            }
        } else {
            for (size_t i = 0; i < 5u; ++i) {
                num_destroy(&b_src[i]);
                b_src[i] = num_clone(poly[i]);
            }
        }

        if (trig) {
            trig_antiderivative_once_local(a_src, b_src, 5u, a_dst, b_dst);
            first = expr_sin(u);
            second = expr_cos(u);
        } else {
            hyperbolic_antiderivative_once_local(a_src, b_src, 5u, a_dst, b_dst);
            first = expr_sinh(u);
            second = expr_cosh(u);
        }

        poly_a = build_polynomial_expr(u, a_dst, 5u);
        poly_b = build_polynomial_expr(u, b_dst, 5u);
        left = (poly_a && first) ? expr_mul(poly_a, first) : NULL;
        right = (poly_b && second) ? expr_mul(poly_b, second) : NULL;
        out = (left && right) ? expr_add(left, right) : NULL;

        expr_free(right);
        expr_free(left);
        expr_free(poly_b);
        expr_free(poly_a);
        expr_free(second);
        expr_free(first);
        number_array_clear_local(a_dst, 5);
        number_array_clear_local(b_dst, 5);
        number_array_clear_local(a_src, 5);
        number_array_clear_local(b_src, 5);
    }

    expr_free(u);
    number_array_clear_local(poly, 5);
    num_destroy(&constant);
    return div_number_owned_consuming(out, &coeff);
}

static expr_t *integrate_poly_times_log_affine(const expr_t *expr, const expr_t *wrt)
{
    number_t poly[5];
    number_t q[6];
    number_t t[6];
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *vars[1];
    expr_t *u;
    expr_t *q_poly;
    expr_t *log_u;
    expr_t *first_term;
    expr_t *t_poly;
    expr_t *raw;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    number_array_zero_local(q, 6);
    number_array_zero_local(t, 6);
    if (!expr_match_affine_poly_deg4_times_unary_affine_kind(expr, EXPR_PATTERN_UNARY_LOG,
                                                              1u, vars, poly, &constant, &coeff) ||
        num_eq(coeff, NUM_ZERO)) {
        number_array_clear_local(t, 6);
        number_array_clear_local(q, 6);
        number_array_clear_local(poly, 5);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    for (size_t i = 0; i < 5u; ++i) {
        number_t denom = num_create_from_long((long)(i + 1u));
        number_t q_coeff = num_div(poly[i], denom);

        num_destroy(&q[i + 1u]);
        q[i + 1u] = q_coeff;
        num_destroy(&denom);
    }
    for (size_t i = 1; i < 6u; ++i) {
        number_t denom = num_create_from_long((long)i);
        number_t t_coeff = num_div(q[i], denom);

        num_destroy(&t[i]);
        t[i] = t_coeff;
        num_destroy(&denom);
    }

    u = build_affine_from_match(wrt, constant, coeff);
    q_poly = u ? build_polynomial_expr(u, q, 6u) : NULL;
    log_u = u ? expr_log(u) : NULL;
    first_term = (q_poly && log_u) ? expr_mul(q_poly, log_u) : NULL;
    t_poly = u ? build_polynomial_expr(u, t, 6u) : NULL;
    raw = (first_term && t_poly) ? expr_sub(first_term, t_poly) : NULL;

    expr_free(t_poly);
    expr_free(first_term);
    expr_free(log_u);
    expr_free(q_poly);
    expr_free(u);
    number_array_clear_local(t, 6);
    number_array_clear_local(q, 6);
    number_array_clear_local(poly, 5);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

static expr_t *integrate_poly_over_matching_affine(const expr_t *expr, const expr_t *wrt)
{
    number_t poly[5];
    number_t anti[5];
    number_t numer_constant = num_new();
    number_t numer_coeff = num_new();
    number_t denom_constant = num_new();
    number_t denom_coeff = num_new();
    expr_t *vars[1];
    expr_t *u;
    expr_t *poly_term;
    expr_t *log_u = NULL;
    expr_t *log_term = NULL;
    expr_t *raw = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    number_array_zero_local(anti, 5);
    if (!expr_match_affine_poly_deg4(expr->a, 1u, vars, poly, &numer_constant, &numer_coeff) ||
        !match_nonconstant_affine_linear_expr(expr->b, wrt, &denom_constant, &denom_coeff) ||
        !affine_linear_match_eq(numer_constant, numer_coeff, denom_constant, denom_coeff) ||
        num_eq(denom_coeff, NUM_ZERO)) {
        number_array_clear_local(anti, 5);
        number_array_clear_local(poly, 5);
        num_destroy(&denom_coeff);
        num_destroy(&denom_constant);
        num_destroy(&numer_coeff);
        num_destroy(&numer_constant);
        return NULL;
    }

    for (size_t i = 1; i < 5u; ++i) {
        number_t denom = num_create_from_long((long)i);
        number_t coeff_i = num_div(poly[i], denom);

        num_destroy(&anti[i]);
        anti[i] = coeff_i;
        num_destroy(&denom);
    }

    u = build_affine_from_match(wrt, denom_constant, denom_coeff);
    poly_term = u ? build_polynomial_expr(u, anti, 5u) : NULL;
    if (!num_eq(poly[0], NUM_ZERO) && u) {
        log_u = expr_log(u);
        log_term = log_u ? expr_mul_num(log_u, &poly[0]) : NULL;
        expr_free(log_u);
    }
    if (poly_term && log_term) {
        raw = expr_add(poly_term, log_term);
    } else if (poly_term) {
        raw = poly_term;
        poly_term = NULL;
    } else if (log_term) {
        raw = log_term;
        log_term = NULL;
    }

    expr_free(log_term);
    expr_free(poly_term);
    expr_free(u);
    number_array_clear_local(anti, 5);
    number_array_clear_local(poly, 5);
    num_destroy(&numer_coeff);
    num_destroy(&numer_constant);
    num_destroy(&denom_constant);
    return div_number_owned_consuming(raw, &denom_coeff);
}

static expr_t *integrate_poly_over_centered_quadratic(const expr_t *expr, const expr_t *wrt)
{
    number_t numer[5];
    number_t denom[5];
    number_t numer_constant = num_new();
    number_t numer_coeff = num_new();
    number_t denom_constant = num_new();
    number_t denom_coeff = num_new();
    expr_t *vars[1];
    expr_t *poly_part = NULL;
    expr_t *log_part = NULL;
    expr_t *atan_part = NULL;
    expr_t *sum = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(numer, 5);
    number_array_zero_local(denom, 5);
    if (!expr_match_affine_poly_deg4(expr->a, 1u, vars, numer, &numer_constant,
                                     &numer_coeff) ||
        !expr_match_affine_poly_deg4(expr->b, 1u, vars, denom, &denom_constant,
                                     &denom_coeff) ||
        !((num_eq(numer_constant, NUM_ZERO) && num_eq(numer_coeff, NUM_ONE)) ||
          (num_is_zero(numer_constant) && num_is_zero(numer_coeff))) ||
        !num_eq(denom_constant, NUM_ZERO) ||
        !num_eq(denom_coeff, NUM_ONE) ||
        !num_is_zero(denom[1]) ||
        !num_is_zero(denom[3]) ||
        !num_is_zero(denom[4]) ||
        num_is_zero(denom[0]) ||
        num_is_zero(denom[2])) {
        goto cleanup;
    }

    number_t q0 = num_div(numer[2], denom[2]);
    number_t q1 = num_div(numer[3], denom[2]);
    number_t half_q1 = num_mul(q1, NUM_HALF);
    number_t c_q0 = num_mul(denom[0], q0);
    number_t c_q1 = num_mul(denom[0], q1);
    number_t r0 = num_sub(numer[0], c_q0);
    number_t r1 = num_sub(numer[1], c_q1);

    if (!num_is_zero(q0) || !num_is_zero(q1)) {
        expr_t *linear = expr_mul_num(wrt, &q0);
        expr_t *x_sq = expr_pow(wrt, &NUM_TWO);
        expr_t *quadratic = x_sq ? expr_mul_num(x_sq, &half_q1) : NULL;

        if (linear && quadratic) {
            poly_part = expr_add(linear, quadratic);
        } else if (linear) {
            poly_part = linear;
            linear = NULL;
        } else if (quadratic) {
            poly_part = quadratic;
            quadratic = NULL;
        }
        expr_free(quadratic);
        expr_free(x_sq);
        expr_free(linear);
    }

    if (!num_is_zero(r1)) {
        number_t two_k = num_mul(NUM_TWO, denom[2]);
        expr_t *log_denom = expr_log(expr->b);
        expr_t *scaled_log = log_denom ? expr_mul_num(log_denom, &r1) : NULL;

        log_part = div_number_owned_consuming(scaled_log, &two_k);
        expr_free(log_denom);
    }

    if (!num_is_zero(r0)) {
        number_t k_over_c = num_div(denom[2], denom[0]);
        bool use_atanh = num_lt(k_over_c, NUM_ZERO);
        number_t abs_c = num_lt(denom[0], NUM_ZERO) ? num_neg(denom[0])
                                                    : num_clone(denom[0]);
        number_t abs_k = num_lt(denom[2], NUM_ZERO) ? num_neg(denom[2])
                                                    : num_clone(denom[2]);
        number_t scale_base = num_div(abs_c, abs_k);
        number_t numeric_scale = num_new();
        bool exact_numeric_scale = integrate_exact_small_rational_sqrt(scale_base,
                                                                       &numeric_scale);
        number_t denom_coeff = use_atanh
                                 ? (num_lt(denom[0], NUM_ZERO) ? num_neg(abs_k)
                                                               : num_clone(abs_k))
                                 : num_clone(denom[2]);

        expr_t *scale_base_expr = exact_numeric_scale ? NULL : expr_new_const(scale_base);
        expr_t *scale_raw = scale_base_expr ? expr_sqrt(scale_base_expr) : NULL;
        expr_t *scale = exact_numeric_scale ? expr_new_const(numeric_scale)
                                            : simplify_owned(scale_raw);
        expr_t *arg = scale ? expr_div(wrt, scale) : NULL;
        expr_t *inverse_arg = arg ? (use_atanh ? expr_atanh(arg)
                                               : expr_atan(arg))
                                  : NULL;
        expr_t *scaled_inverse = inverse_arg ? expr_mul_num(inverse_arg, &r0) : NULL;
        expr_t *denom_expr = scale ? expr_mul_num(scale, &denom_coeff) : NULL;

        atan_part = (scaled_inverse && denom_expr) ? expr_div(scaled_inverse, denom_expr)
                                                   : NULL;
        atan_part = simplify_owned(atan_part);
        expr_free(denom_expr);
        expr_free(scaled_inverse);
        expr_free(inverse_arg);
        expr_free(arg);
        expr_free(scale);
        expr_free(scale_base_expr);
        num_destroy(&denom_coeff);
        num_destroy(&numeric_scale);
        num_destroy(&scale_base);
        num_destroy(&abs_k);
        num_destroy(&abs_c);
        num_destroy(&k_over_c);
    }

    if (poly_part && log_part) {
        sum = expr_add(poly_part, log_part);
    } else if (poly_part) {
        sum = poly_part;
        poly_part = NULL;
    } else if (log_part) {
        sum = log_part;
        log_part = NULL;
    }
    if (sum && atan_part) {
        expr_t *next = expr_add(sum, atan_part);

        expr_free(sum);
        sum = next;
    } else if (!sum && atan_part) {
        sum = atan_part;
        atan_part = NULL;
    }

    num_destroy(&r1);
    num_destroy(&r0);
    num_destroy(&c_q1);
    num_destroy(&c_q0);
    num_destroy(&half_q1);
    num_destroy(&q1);
    num_destroy(&q0);

cleanup:
    expr_free(atan_part);
    expr_free(log_part);
    expr_free(poly_part);
    number_array_clear_local(denom, 5);
    number_array_clear_local(numer, 5);
    num_destroy(&denom_coeff);
    num_destroy(&denom_constant);
    num_destroy(&numer_coeff);
    num_destroy(&numer_constant);
    return simplify_owned(sum);
}

static bool match_square_of_expr(const expr_t *expr, const expr_t **base_out)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !base_out) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a &&
        num_eq(expr->c, NUM_TWO)) {
        *base_out = expr->a;
        ok = true;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent) &&
               num_eq(exponent, NUM_TWO)) {
        *base_out = expr->a;
        ok = true;
    }

    num_destroy(&exponent);
    return ok;
}

static bool match_symbolic_square_sum_denominator(const expr_t *expr,
                                                  const expr_t *wrt,
                                                  const expr_t **symbol_base_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *left_base = NULL;
    const expr_t *right_base = NULL;
    bool is_sub = false;

    if (!expr || !wrt || !symbol_base_out ||
        !expr_match_add_sub_expr(expr, &left, &right, &is_sub) ||
        is_sub ||
        !match_square_of_expr(left, &left_base) ||
        !match_square_of_expr(right, &right_base))
        return false;

    if (is_wrt(left_base, wrt) && !depends_on_wrt(right_base, wrt)) {
        *symbol_base_out = right_base;
        return true;
    }
    if (is_wrt(right_base, wrt) && !depends_on_wrt(left_base, wrt)) {
        *symbol_base_out = left_base;
        return true;
    }
    return false;
}

static expr_t *integrate_inverse_symbolic_square_sum(const expr_t *expr,
                                                     const expr_t *wrt)
{
    number_t numer_constant = num_new();
    const expr_t *symbol_base = NULL;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b ||
        depends_on_wrt(expr->a, wrt) ||
        !expr_match_const_value(expr->a, &numer_constant) ||
        !num_eq(numer_constant, NUM_ONE) ||
        !match_symbolic_square_sum_denominator(expr->b, wrt, &symbol_base))
        goto cleanup;

    expr_t *arg = expr_div(wrt, symbol_base);
    expr_t *atan_arg = arg ? expr_atan(arg) : NULL;
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *reciprocal = one ? expr_div(one, symbol_base) : NULL;

    out = (reciprocal && atan_arg) ? expr_mul(reciprocal, atan_arg) : NULL;
    out = simplify_owned(out);
    expr_free(reciprocal);
    expr_free(one);
    expr_free(atan_arg);
    expr_free(arg);

cleanup:
    num_destroy(&numer_constant);
    return out;
}

static expr_t *add_terms_owned(expr_t *left, expr_t *right)
{
    expr_t *sum;

    if (!left)
        return right;
    if (!right)
        return left;

    sum = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return sum;
}

static bool match_linear_poly_in_wrt(const expr_t *expr,
                                     const expr_t *wrt,
                                     number_t *poly)
{
    number_t basis_constant = num_new();
    number_t basis_coeff = num_new();
    expr_t *vars[1];
    bool ok;

    vars[0] = (expr_t *)wrt;
    ok = expr_match_affine_poly_deg4(expr, 1u, vars, poly, &basis_constant,
                                     &basis_coeff) &&
         rewrite_poly_deg4_to_affine_basis(poly, basis_constant, basis_coeff,
                                           NUM_ZERO, NUM_ONE) &&
         num_is_zero(poly[2]) &&
         num_is_zero(poly[3]) &&
         num_is_zero(poly[4]);

    num_destroy(&basis_coeff);
    num_destroy(&basis_constant);
    return ok;
}

static bool match_centered_quadratic_expr(const expr_t *expr,
                                          const expr_t *wrt,
                                          number_t *constant_out,
                                          number_t *quad_coeff_out)
{
    number_t poly[5];
    number_t basis_constant = num_new();
    number_t basis_coeff = num_new();
    expr_t *vars[1];
    bool ok;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    ok = expr_match_affine_poly_deg4(expr, 1u, vars, poly, &basis_constant,
                                     &basis_coeff) &&
         rewrite_poly_deg4_to_affine_basis(poly, basis_constant, basis_coeff,
                                           NUM_ZERO, NUM_ONE) &&
         num_is_zero(poly[1]) &&
         num_is_zero(poly[3]) &&
         num_is_zero(poly[4]) &&
         !num_is_zero(poly[0]) &&
         !num_is_zero(poly[2]) &&
         num_is_real(poly[0]) &&
         num_is_real(poly[2]);
    if (ok) {
        num_destroy(constant_out);
        *constant_out = num_clone(poly[0]);
        num_destroy(quad_coeff_out);
        *quad_coeff_out = num_clone(poly[2]);
    }

    number_array_clear_local(poly, 5);
    num_destroy(&basis_coeff);
    num_destroy(&basis_constant);
    return ok;
}

static expr_t *integrate_sqrt_one_plus_minus_affine_square(const expr_t *quadratic,
                                                           const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    bool is_plus_square = false;
    expr_t *out = NULL;

    if (quadratic &&
        match_one_plus_minus_affine_square(quadratic, wrt, &is_plus_square,
                                           &constant, &coeff) &&
        !num_eq(coeff, NUM_ZERO)) {
        expr_t *u = build_affine_from_match(wrt, constant, coeff);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *u_root = (u && root) ? expr_mul(u, root) : NULL;
        expr_t *inverse = u ? (is_plus_square ? expr_asinh(u) : expr_asin(u)) : NULL;
        expr_t *sum = add_terms_owned(u_root, inverse);
        expr_t *half = sum ? mul_number_owned(sum, NUM_HALF) : NULL;

        out = div_number_owned(half, coeff);
        expr_free(root);
        expr_free(u);
    }

    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_centered_quadratic_inverse_root(const expr_t *quadratic,
                                                         const expr_t *wrt)
{
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *out = NULL;

    if (!match_centered_quadratic_expr(quadratic, wrt, &constant, &quad))
        goto cleanup;

    if (num_gt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t ratio = num_div(quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t denom = num_sqrt(quad);
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asinh(arg) : NULL;

        out = div_number_owned(inverse, denom);
        expr_free(arg);
        num_destroy(&denom);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
    } else if (num_gt(constant, NUM_ZERO) && num_lt(quad, NUM_ZERO)) {
        number_t neg_quad = num_neg(quad);
        number_t ratio = num_div(neg_quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t denom = num_sqrt(neg_quad);
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asin(arg) : NULL;

        out = div_number_owned(inverse, denom);
        expr_free(arg);
        num_destroy(&denom);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_quad);
    } else if (num_lt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t neg_constant = num_neg(constant);
        number_t ratio = num_div(quad, neg_constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t denom = num_sqrt(quad);
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_acosh(arg) : NULL;

        out = div_number_owned(inverse, denom);
        expr_free(arg);
        num_destroy(&denom);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_constant);
    }

cleanup:
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_centered_quadratic_root(const expr_t *quadratic, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *out = NULL;

    if (!match_centered_quadratic_expr(quadratic, wrt, &constant, &quad))
        goto cleanup;

    if (num_gt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t ratio = num_div(quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t sqrt_quad = num_sqrt(quad);
        number_t denom = num_mul(NUM_TWO, sqrt_quad);
        number_t inverse_scale = num_div(constant, denom);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *x_root = root ? expr_mul(wrt, root) : NULL;
        expr_t *first = x_root ? mul_number_owned(x_root, NUM_HALF) : NULL;
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asinh(arg) : NULL;
        expr_t *second = inverse ? mul_number_owned(inverse, inverse_scale) : NULL;

        out = simplify_owned(add_terms_owned(first, second));
        expr_free(arg);
        expr_free(root);
        num_destroy(&inverse_scale);
        num_destroy(&denom);
        num_destroy(&sqrt_quad);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
    } else if (num_gt(constant, NUM_ZERO) && num_lt(quad, NUM_ZERO)) {
        number_t neg_quad = num_neg(quad);
        number_t ratio = num_div(neg_quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t sqrt_neg_quad = num_sqrt(neg_quad);
        number_t denom = num_mul(NUM_TWO, sqrt_neg_quad);
        number_t inverse_scale = num_div(constant, denom);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *x_root = root ? expr_mul(wrt, root) : NULL;
        expr_t *first = x_root ? mul_number_owned(x_root, NUM_HALF) : NULL;
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asin(arg) : NULL;
        expr_t *second = inverse ? mul_number_owned(inverse, inverse_scale) : NULL;

        out = simplify_owned(add_terms_owned(first, second));
        expr_free(arg);
        expr_free(root);
        num_destroy(&inverse_scale);
        num_destroy(&denom);
        num_destroy(&sqrt_neg_quad);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_quad);
    } else if (num_lt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t neg_constant = num_neg(constant);
        number_t ratio = num_div(quad, neg_constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t sqrt_quad = num_sqrt(quad);
        number_t denom = num_mul(NUM_TWO, sqrt_quad);
        number_t inverse_scale = num_div(neg_constant, denom);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *x_root = root ? expr_mul(wrt, root) : NULL;
        expr_t *first = x_root ? mul_number_owned(x_root, NUM_HALF) : NULL;
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_acosh(arg) : NULL;
        expr_t *second = inverse ? mul_number_owned(inverse, inverse_scale) : NULL;
        expr_t *neg_second = second ? expr_neg(second) : NULL;

        out = simplify_owned(add_terms_owned(first, neg_second));
        expr_free(second);
        expr_free(arg);
        expr_free(root);
        num_destroy(&inverse_scale);
        num_destroy(&denom);
        num_destroy(&sqrt_quad);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_constant);
    }

cleanup:
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_linear_poly_over_centered_quadratic_root(const expr_t *expr,
                                                                  const expr_t *wrt)
{
    number_t poly[5];
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *root_part = NULL;
    expr_t *inverse_part = NULL;
    expr_t *out = NULL;

    number_array_zero_local(poly, 5);
    if (!expr || !expr->a || !expr->b ||
        !expr->b->ops ||
        expr->b->ops->kind != EXPR_KIND_SQRT ||
        !expr->b->a ||
        !match_linear_poly_in_wrt(expr->a, wrt, poly) ||
        !match_centered_quadratic_expr(expr->b->a, wrt, &constant, &quad))
        goto cleanup;

    if (!num_is_zero(poly[0])) {
        expr_t *inverse = integrate_centered_quadratic_inverse_root(expr->b->a, wrt);

        inverse_part = inverse ? mul_number_owned(inverse, poly[0]) : NULL;
    }
    if (!num_is_zero(poly[1])) {
        expr_t *root = expr_sqrt(expr->b->a);
        expr_t *scaled = root ? mul_number_owned(root, poly[1]) : NULL;

        root_part = div_number_owned(scaled, quad);
    }

    out = simplify_owned(add_terms_owned(inverse_part, root_part));
    inverse_part = NULL;
    root_part = NULL;

cleanup:
    expr_free(root_part);
    expr_free(inverse_part);
    number_array_clear_local(poly, 5);
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_linear_poly_times_centered_quadratic_root(const expr_t *expr,
                                                                   const expr_t *wrt)
{
    const expr_t *poly_expr = NULL;
    const expr_t *sqrt_expr = NULL;
    number_t poly[5];
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *linear_part = NULL;
    expr_t *root_part = NULL;
    expr_t *out = NULL;

    number_array_zero_local(poly, 5);
    if (!expr || !expr->a || !expr->b)
        goto cleanup;

    if (expr->a->ops && expr->a->ops->kind == EXPR_KIND_SQRT) {
        sqrt_expr = expr->a;
        poly_expr = expr->b;
    } else if (expr->b->ops && expr->b->ops->kind == EXPR_KIND_SQRT) {
        sqrt_expr = expr->b;
        poly_expr = expr->a;
    } else {
        goto cleanup;
    }

    if (!sqrt_expr->a ||
        !match_linear_poly_in_wrt(poly_expr, wrt, poly) ||
        !match_centered_quadratic_expr(sqrt_expr->a, wrt, &constant, &quad))
        goto cleanup;

    if (!num_is_zero(poly[0])) {
        expr_t *root_integral = integrate_centered_quadratic_root(sqrt_expr->a, wrt);

        root_part = root_integral ? mul_number_owned(root_integral, poly[0]) : NULL;
    }
    if (!num_is_zero(poly[1])) {
        number_t three = num_create_from_long(3);
        number_t three_halves = num_div(three, NUM_TWO);
        number_t denom = num_mul(three, quad);
        expr_t *power = expr_pow(sqrt_expr->a, &three_halves);
        expr_t *scaled = power ? mul_number_owned(power, poly[1]) : NULL;

        linear_part = div_number_owned(scaled, denom);
        num_destroy(&denom);
        num_destroy(&three_halves);
        num_destroy(&three);
    }

    out = simplify_owned(add_terms_owned(root_part, linear_part));
    root_part = NULL;
    linear_part = NULL;

cleanup:
    expr_free(linear_part);
    expr_free(root_part);
    number_array_clear_local(poly, 5);
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}

typedef expr_t *(*expr_binary_build_fn)(const expr_t *left, const expr_t *right);
typedef expr_t *(*squared_unary_raw_fn)(const expr_t *u);
typedef expr_t *(*cubed_unary_raw_fn)(const expr_t *u);

typedef struct squared_unary_rule {
    squared_unary_raw_fn build_raw;
    long divisor_factor;
} squared_unary_rule_t;

typedef struct cubed_unary_rule {
    cubed_unary_raw_fn build_raw;
} cubed_unary_rule_t;

static expr_t *build_double_angle_squared_raw(const expr_t *u,
                                              expr_apply_unary_fn oscillation_fn,
                                              bool scaled_first,
                                              expr_binary_build_fn combine)
{
    expr_t *two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
    expr_t *oscillation = (two_u && oscillation_fn) ? oscillation_fn(two_u) : NULL;
    expr_t *scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
    expr_t *raw = NULL;

    if (scaled_u && oscillation)
        raw = scaled_first ? combine(scaled_u, oscillation) : combine(oscillation, scaled_u);
    expr_free(scaled_u);
    expr_free(oscillation);
    expr_free(two_u);
    return raw;
}

static expr_t *build_sin_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sin, true, expr_sub);
}

static expr_t *build_cos_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sin, true, expr_add);
}

static expr_t *build_sinh_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sinh, false, expr_sub);
}

static expr_t *build_cosh_squared_raw(const expr_t *u)
{
    return build_double_angle_squared_raw(u, expr_sinh, false, expr_add);
}

static expr_t *build_unary_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    return (u && unary_fn) ? unary_fn(u) : NULL;
}

static expr_t *build_unary_minus_u_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    expr_t *term = build_unary_raw(u, unary_fn);
    expr_t *raw = (term && u) ? expr_sub(term, u) : NULL;

    expr_free(term);
    return raw;
}

static expr_t *build_u_minus_unary_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    expr_t *term = build_unary_raw(u, unary_fn);
    expr_t *raw = (u && term) ? expr_sub(u, term) : NULL;

    expr_free(term);
    return raw;
}

static expr_t *build_neg_unary_raw(const expr_t *u, expr_apply_unary_fn unary_fn)
{
    expr_t *term = build_unary_raw(u, unary_fn);
    expr_t *raw = term ? expr_neg(term) : NULL;

    expr_free(term);
    return raw;
}

static expr_t *build_tan_squared_raw(const expr_t *u)
{
    return build_unary_minus_u_raw(u, expr_tan);
}

static expr_t *build_sec_squared_raw(const expr_t *u)
{
    return build_unary_raw(u, expr_tan);
}

static expr_t *build_cosec_squared_raw(const expr_t *u)
{
    return build_neg_unary_raw(u, expr_cot);
}

static expr_t *build_sech_squared_raw(const expr_t *u)
{
    return build_unary_raw(u, expr_tanh);
}

static expr_t *build_cosech_squared_raw(const expr_t *u)
{
    return build_neg_unary_raw(u, expr_coth);
}

static expr_t *build_tanh_squared_raw(const expr_t *u)
{
    return build_u_minus_unary_raw(u, expr_tanh);
}

static expr_t *build_coth_squared_raw(const expr_t *u)
{
    return build_u_minus_unary_raw(u, expr_coth);
}

static expr_t *build_sin_cubed_raw(const expr_t *u)
{
    number_t three = num_create_from_long(3);
    expr_t *cos_u = expr_cos(u);
    expr_t *cos_cubed = cos_u ? expr_pow(cos_u, &three) : NULL;
    expr_t *third = cos_cubed ? expr_mul_num(cos_cubed, &NUM_ONE_THIRD) : NULL;
    expr_t *raw = (third && cos_u) ? expr_sub(third, cos_u) : NULL;

    num_destroy(&three);
    expr_free(third);
    expr_free(cos_cubed);
    expr_free(cos_u);
    return raw;
}

static expr_t *build_cos_cubed_raw(const expr_t *u)
{
    number_t three = num_create_from_long(3);
    expr_t *sin_u = expr_sin(u);
    expr_t *sin_cubed = sin_u ? expr_pow(sin_u, &three) : NULL;
    expr_t *third = sin_cubed ? expr_mul_num(sin_cubed, &NUM_ONE_THIRD) : NULL;
    expr_t *raw = (sin_u && third) ? expr_sub(sin_u, third) : NULL;

    num_destroy(&three);
    expr_free(third);
    expr_free(sin_cubed);
    expr_free(sin_u);
    return raw;
}

static expr_t *build_tan_cubed_raw(const expr_t *u)
{
    expr_t *tan_u = expr_tan(u);
    expr_t *tan_sq = tan_u ? expr_pow(tan_u, &NUM_TWO) : NULL;
    expr_t *half_tan_sq = tan_sq ? expr_mul_num(tan_sq, &NUM_HALF) : NULL;
    expr_t *cos_u = expr_cos(u);
    expr_t *log_cos = cos_u ? expr_log(cos_u) : NULL;
    expr_t *raw = (half_tan_sq && log_cos) ? expr_add(half_tan_sq, log_cos) : NULL;

    expr_free(log_cos);
    expr_free(cos_u);
    expr_free(half_tan_sq);
    expr_free(tan_sq);
    expr_free(tan_u);
    return raw;
}

static expr_t *build_sec_cubed_raw(const expr_t *u)
{
    expr_t *sec_u = expr_sec(u);
    expr_t *tan_u = expr_tan(u);
    expr_t *product = (sec_u && tan_u) ? expr_mul(sec_u, tan_u) : NULL;
    expr_t *sum = (sec_u && tan_u) ? expr_add(sec_u, tan_u) : NULL;
    expr_t *log_sum = sum ? expr_log(sum) : NULL;
    expr_t *raw_sum = (product && log_sum) ? expr_add(product, log_sum) : NULL;
    expr_t *raw = raw_sum ? expr_mul_num(raw_sum, &NUM_HALF) : NULL;

    expr_free(raw_sum);
    expr_free(log_sum);
    expr_free(sum);
    expr_free(product);
    expr_free(tan_u);
    expr_free(sec_u);
    return raw;
}

static expr_t *build_cosec_cubed_raw(const expr_t *u)
{
    expr_t *cosec_u = expr_cosec(u);
    expr_t *cot_u = expr_cot(u);
    expr_t *product = (cosec_u && cot_u) ? expr_mul(cosec_u, cot_u) : NULL;
    expr_t *sum = (cosec_u && cot_u) ? expr_add(cosec_u, cot_u) : NULL;
    expr_t *log_sum = sum ? expr_log(sum) : NULL;
    expr_t *raw_sum = (product && log_sum) ? expr_add(product, log_sum) : NULL;
    expr_t *half = raw_sum ? expr_mul_num(raw_sum, &NUM_HALF) : NULL;
    expr_t *raw = half ? expr_neg(half) : NULL;

    expr_free(half);
    expr_free(raw_sum);
    expr_free(log_sum);
    expr_free(sum);
    expr_free(product);
    expr_free(cot_u);
    expr_free(cosec_u);
    return raw;
}

static const squared_unary_rule_t squared_unary_rules[EXPR_PATTERN_UNARY_COUNT] = {
    [EXPR_PATTERN_UNARY_SIN] = { build_sin_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_COS] = { build_cos_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_SINH] = { build_sinh_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_COSH] = { build_cosh_squared_raw, 4 },
    [EXPR_PATTERN_UNARY_TAN] = { build_tan_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_SEC] = { build_sec_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COSEC] = { build_cosec_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_SECH] = { build_sech_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COSECH] = { build_cosech_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_TANH] = { build_tanh_squared_raw, 1 },
    [EXPR_PATTERN_UNARY_COTH] = { build_coth_squared_raw, 1 }
};

static const cubed_unary_rule_t cubed_unary_rules[EXPR_PATTERN_UNARY_COUNT] = {
    [EXPR_PATTERN_UNARY_SIN] = { build_sin_cubed_raw },
    [EXPR_PATTERN_UNARY_COS] = { build_cos_cubed_raw },
    [EXPR_PATTERN_UNARY_TAN] = { build_tan_cubed_raw },
    [EXPR_PATTERN_UNARY_SEC] = { build_sec_cubed_raw },
    [EXPR_PATTERN_UNARY_COSEC] = { build_cosec_cubed_raw }
};

static const squared_unary_rule_t *find_squared_unary_rule(expr_pattern_unary_affine_kind_t kind)
{
    const squared_unary_rule_t *rule;

    if ((unsigned)kind >= (unsigned)EXPR_PATTERN_UNARY_COUNT)
        return NULL;
    rule = &squared_unary_rules[kind];
    return rule->build_raw ? rule : NULL;
}

static const cubed_unary_rule_t *find_cubed_unary_rule(expr_pattern_unary_affine_kind_t kind)
{
    const cubed_unary_rule_t *rule;

    if ((unsigned)kind >= (unsigned)EXPR_PATTERN_UNARY_COUNT)
        return NULL;
    rule = &cubed_unary_rules[kind];
    return rule->build_raw ? rule : NULL;
}

static expr_t *integrate_squared_unary_affine(const expr_t *expr,
                                              const expr_t *wrt,
                                              expr_pattern_unary_affine_kind_t kind)
{
    const squared_unary_rule_t *rule = find_squared_unary_rule(kind);
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u;
    expr_t *raw = NULL;

    if (!rule ||
        !expr || !expr->a || !num_eq(expr->c, NUM_TWO) ||
        !match_affine_unary_data(expr->a, wrt, kind, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    raw = u ? rule->build_raw(u) : NULL;
    expr_free(u);
    num_destroy(&constant);
    if (rule->divisor_factor == 1)
        return div_number_owned_consuming(raw, &coeff);
    raw = div_number_owned_by_long_product(raw, rule->divisor_factor, coeff);
    num_destroy(&coeff);
    return raw;
}

static expr_t *integrate_cubed_unary_affine(const expr_t *expr,
                                            const expr_t *wrt,
                                            expr_pattern_unary_affine_kind_t kind)
{
    const cubed_unary_rule_t *rule = find_cubed_unary_rule(kind);
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u;
    expr_t *raw = NULL;

    if (!rule ||
        !expr || !expr->a ||
        !match_affine_unary_data(expr->a, wrt, kind, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    raw = u ? rule->build_raw(u) : NULL;
    expr_free(u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

static bool match_affine_unary_power_data(const expr_t *expr,
                                          const expr_t *wrt,
                                          expr_pattern_unary_affine_kind_t kind,
                                          number_t exponent,
                                          number_t *constant_out,
                                          number_t *coeff_out)
{
    number_t matched_exponent = num_new();
    const expr_t *base = NULL;
    bool ok = false;

    if (!expr)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D) {
        base = expr->a;
        ok = num_eq(expr->c, exponent);
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &matched_exponent)) {
        base = expr->a;
        ok = num_eq(matched_exponent, exponent);
    }

    if (ok)
        ok = match_affine_unary_data(base, wrt, kind, constant_out, coeff_out);

cleanup:
    num_destroy(&matched_exponent);
    return ok;
}

static bool match_exp_and_unary_affine_product(const expr_t *expr,
                                               const expr_t *wrt,
                                               expr_pattern_unary_affine_kind_t kind,
                                               number_t *exp_constant,
                                               number_t *exp_coeff,
                                               number_t *unary_constant,
                                               number_t *unary_coeff)
{
    if (!expr || !expr->a || !expr->b)
        return false;

    if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP,
                                exp_constant, exp_coeff) &&
        match_affine_unary_data(expr->b, wrt, kind,
                                unary_constant, unary_coeff))
        return true;

    return match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_EXP,
                                   exp_constant, exp_coeff) &&
           match_affine_unary_data(expr->a, wrt, kind,
                                   unary_constant, unary_coeff);
}

static expr_t *integrate_exp_times_trig_affine_product(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind)
{
    number_t exp_constant = num_new();
    number_t exp_coeff = num_new();
    number_t trig_constant = num_new();
    number_t trig_coeff = num_new();
    expr_t *exp_u = NULL;
    expr_t *trig_v = NULL;
    expr_t *sin_v = NULL;
    expr_t *cos_v = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *sum = NULL;
    expr_t *product = NULL;
    expr_t *out = NULL;

    if (!match_exp_and_unary_affine_product(expr, wrt, kind,
                                            &exp_constant, &exp_coeff,
                                            &trig_constant, &trig_coeff))
        goto cleanup;

    exp_u = build_affine_from_match(wrt, exp_constant, exp_coeff);
    trig_v = build_affine_from_match(wrt, trig_constant, trig_coeff);
    if (!exp_u || !trig_v)
        goto cleanup;

    {
        expr_t *exp_arg = exp_u;

        exp_u = exp_arg ? expr_exp(exp_arg) : NULL;
        expr_free(exp_arg);
        exp_u = simplify_owned(exp_u);
    }
    if (kind == EXPR_PATTERN_UNARY_SIN) {
        sin_v = expr_sin(trig_v);
        cos_v = expr_cos(trig_v);
        left = sin_v ? expr_mul_num(sin_v, &exp_coeff) : NULL;
        right = cos_v ? expr_mul_num(cos_v, &trig_coeff) : NULL;
        sum = (left && right) ? expr_sub(left, right) : NULL;
    } else if (kind == EXPR_PATTERN_UNARY_COS) {
        cos_v = expr_cos(trig_v);
        sin_v = expr_sin(trig_v);
        left = cos_v ? expr_mul_num(cos_v, &exp_coeff) : NULL;
        right = sin_v ? expr_mul_num(sin_v, &trig_coeff) : NULL;
        sum = (left && right) ? expr_add(left, right) : NULL;
    } else {
        goto cleanup;
    }

    product = (exp_u && sum) ? expr_mul(exp_u, sum) : NULL;
    if (product) {
        number_t exp_coeff_sq = num_mul(exp_coeff, exp_coeff);
        number_t trig_coeff_sq = num_mul(trig_coeff, trig_coeff);
        number_t denom = num_add(exp_coeff_sq, trig_coeff_sq);

        out = div_number_owned(product, denom);
        product = NULL;
        num_destroy(&denom);
        num_destroy(&trig_coeff_sq);
        num_destroy(&exp_coeff_sq);
    }

cleanup:
    expr_free(product);
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(cos_v);
    expr_free(sin_v);
    expr_free(trig_v);
    expr_free(exp_u);
    num_destroy(&trig_coeff);
    num_destroy(&trig_constant);
    num_destroy(&exp_coeff);
    num_destroy(&exp_constant);
    return out;
}

static expr_t *integrate_exp_times_hyperbolic_affine_product(
    const expr_t *expr,
    const expr_t *wrt,
    expr_pattern_unary_affine_kind_t kind)
{
    number_t exp_constant = num_new();
    number_t exp_coeff = num_new();
    number_t hyper_constant = num_new();
    number_t hyper_coeff = num_new();
    number_t sum_coeff = num_new();
    number_t diff_coeff = num_new();
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *u_plus_v = NULL;
    expr_t *u_minus_v = NULL;
    expr_t *exp_sum = NULL;
    expr_t *exp_diff = NULL;
    expr_t *first = NULL;
    expr_t *second = NULL;
    expr_t *combined = NULL;
    expr_t *out = NULL;

    if (!match_exp_and_unary_affine_product(expr, wrt, kind,
                                            &exp_constant, &exp_coeff,
                                            &hyper_constant, &hyper_coeff))
        goto cleanup;

    num_destroy(&sum_coeff);
    sum_coeff = num_add(exp_coeff, hyper_coeff);
    num_destroy(&diff_coeff);
    diff_coeff = num_sub(exp_coeff, hyper_coeff);
    if (num_eq(sum_coeff, NUM_ZERO) || num_eq(diff_coeff, NUM_ZERO))
        goto cleanup;

    u = build_affine_from_match(wrt, exp_constant, exp_coeff);
    v = build_affine_from_match(wrt, hyper_constant, hyper_coeff);
    u_plus_v = (u && v) ? expr_add(u, v) : NULL;
    u_minus_v = (u && v) ? expr_sub(u, v) : NULL;
    exp_sum = u_plus_v ? expr_exp(u_plus_v) : NULL;
    exp_diff = u_minus_v ? expr_exp(u_minus_v) : NULL;
    first = exp_sum ? div_number_owned(exp_sum, sum_coeff) : NULL;
    exp_sum = NULL;
    second = exp_diff ? div_number_owned(exp_diff, diff_coeff) : NULL;
    exp_diff = NULL;

    if (kind == EXPR_PATTERN_UNARY_SINH) {
        combined = (first && second) ? expr_sub(first, second) : NULL;
    } else if (kind == EXPR_PATTERN_UNARY_COSH) {
        combined = (first && second) ? expr_add(first, second) : NULL;
    }
    out = combined ? mul_number_owned(combined, NUM_HALF) : NULL;
    combined = NULL;

cleanup:
    expr_free(combined);
    expr_free(second);
    expr_free(first);
    expr_free(exp_diff);
    expr_free(exp_sum);
    expr_free(u_minus_v);
    expr_free(u_plus_v);
    expr_free(v);
    expr_free(u);
    num_destroy(&diff_coeff);
    num_destroy(&sum_coeff);
    num_destroy(&hyper_coeff);
    num_destroy(&hyper_constant);
    num_destroy(&exp_coeff);
    num_destroy(&exp_constant);
    return out;
}

static bool match_sinh_cosh_affine_product(const expr_t *expr,
                                           const expr_t *wrt,
                                           number_t *sinh_constant,
                                           number_t *sinh_coeff,
                                           number_t *cosh_constant,
                                           number_t *cosh_coeff)
{
    if (!expr || !expr->a || !expr->b)
        return false;

    if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SINH,
                                sinh_constant, sinh_coeff) &&
        match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH,
                                cosh_constant, cosh_coeff))
        return true;

    return match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SINH,
                                   sinh_constant, sinh_coeff) &&
           match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSH,
                                   cosh_constant, cosh_coeff);
}

static expr_t *integrate_sinh_cosh_affine_product(const expr_t *expr,
                                                  const expr_t *wrt)
{
    number_t sinh_constant = num_new();
    number_t sinh_coeff = num_new();
    number_t cosh_constant = num_new();
    number_t cosh_coeff = num_new();
    number_t sum_coeff = num_new();
    number_t diff_coeff = num_new();
    expr_t *u = NULL;
    expr_t *v = NULL;
    expr_t *u_plus_v = NULL;
    expr_t *u_minus_v = NULL;
    expr_t *cosh_sum = NULL;
    expr_t *cosh_diff = NULL;
    expr_t *first = NULL;
    expr_t *second = NULL;
    expr_t *combined = NULL;
    expr_t *out = NULL;

    if (!match_sinh_cosh_affine_product(expr, wrt,
                                        &sinh_constant, &sinh_coeff,
                                        &cosh_constant, &cosh_coeff))
        goto cleanup;

    num_destroy(&sum_coeff);
    sum_coeff = num_add(sinh_coeff, cosh_coeff);
    num_destroy(&diff_coeff);
    diff_coeff = num_sub(sinh_coeff, cosh_coeff);
    if (num_eq(sum_coeff, NUM_ZERO) || num_eq(diff_coeff, NUM_ZERO))
        goto cleanup;

    u = build_affine_from_match(wrt, sinh_constant, sinh_coeff);
    v = build_affine_from_match(wrt, cosh_constant, cosh_coeff);
    u_plus_v = (u && v) ? expr_add(u, v) : NULL;
    u_minus_v = (u && v) ? expr_sub(u, v) : NULL;
    cosh_sum = u_plus_v ? expr_cosh(u_plus_v) : NULL;
    cosh_diff = u_minus_v ? expr_cosh(u_minus_v) : NULL;
    first = cosh_sum ? div_number_owned(cosh_sum, sum_coeff) : NULL;
    cosh_sum = NULL;
    second = cosh_diff ? div_number_owned(cosh_diff, diff_coeff) : NULL;
    cosh_diff = NULL;
    combined = (first && second) ? expr_add(first, second) : NULL;
    out = combined ? mul_number_owned(combined, NUM_HALF) : NULL;
    combined = NULL;

cleanup:
    expr_free(combined);
    expr_free(second);
    expr_free(first);
    expr_free(cosh_diff);
    expr_free(cosh_sum);
    expr_free(u_minus_v);
    expr_free(u_plus_v);
    expr_free(v);
    expr_free(u);
    num_destroy(&diff_coeff);
    num_destroy(&sum_coeff);
    num_destroy(&cosh_coeff);
    num_destroy(&cosh_constant);
    num_destroy(&sinh_coeff);
    num_destroy(&sinh_constant);
    return out;
}

static expr_t *integrate_same_affine_special_product(const expr_t *expr, const expr_t *wrt)
{
    number_t c1 = num_new();
    number_t k1 = num_new();
    number_t c2 = num_new();
    number_t k2 = num_new();
    expr_t *u;
    expr_t *out = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    out = integrate_exp_times_trig_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SIN);
    if (out)
        goto cleanup;

    out = integrate_exp_times_trig_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COS);
    if (out)
        goto cleanup;

    out = integrate_exp_times_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_SINH);
    if (out)
        goto cleanup;

    out = integrate_exp_times_hyperbolic_affine_product(expr, wrt, EXPR_PATTERN_UNARY_COSH);
    if (out)
        goto cleanup;

    out = integrate_sinh_cosh_affine_product(expr, wrt);
    if (out)
        goto cleanup;

    if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, &c1, &k1) &&
        match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
        affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sin_u;
        expr_t *sin_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sin_u = u ? expr_sin(u) : NULL;
        sin_sq = sin_u ? expr_pow(sin_u, &NUM_TWO) : NULL;
        expr_free(sin_u);
        expr_free(u);
        out = div_number_owned_by_product(sin_sq, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sin_u;
        expr_t *sin_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sin_u = u ? expr_sin(u) : NULL;
        sin_sq = sin_u ? expr_pow(sin_u, &NUM_TWO) : NULL;
        expr_free(sin_u);
        expr_free(u);
        out = div_number_owned_by_product(sin_sq, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
        match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, &c2, &k2) &&
        affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *exp_u;
        expr_t *sin_u;
        expr_t *cos_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        exp_u = u ? expr_exp(u) : NULL;
        sin_u = u ? expr_sin(u) : NULL;
        cos_u = u ? expr_cos(u) : NULL;
        diff = (sin_u && cos_u) ? expr_sub(sin_u, cos_u) : NULL;
        out = (exp_u && diff) ? expr_mul(exp_u, diff) : NULL;
        expr_free(diff);
        expr_free(cos_u);
        expr_free(sin_u);
        expr_free(exp_u);
        expr_free(u);
        out = div_number_owned_by_product(out, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *exp_u;
        expr_t *sin_u;
        expr_t *cos_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        exp_u = u ? expr_exp(u) : NULL;
        sin_u = u ? expr_sin(u) : NULL;
        cos_u = u ? expr_cos(u) : NULL;
        sum = (sin_u && cos_u) ? expr_add(sin_u, cos_u) : NULL;
        out = (exp_u && sum) ? expr_mul(exp_u, sum) : NULL;
        expr_free(sum);
        expr_free(cos_u);
        expr_free(sin_u);
        expr_free(exp_u);
        expr_free(u);
        out = div_number_owned_by_product(out, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SINH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *exp_two_u;
        expr_t *scaled_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        exp_two_u = two_u ? expr_exp(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        diff = (exp_two_u && scaled_u) ? expr_sub(exp_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(exp_two_u);
        expr_free(two_u);
        expr_free(u);
        out = div_number_owned_by_long_product(diff, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_EXP, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *exp_two_u;
        expr_t *scaled_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        exp_two_u = two_u ? expr_exp(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sum = (exp_two_u && scaled_u) ? expr_add(exp_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(exp_two_u);
        expr_free(two_u);
        expr_free(u);
        out = div_number_owned_by_long_product(sum, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sin_u;
        expr_t *sin_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sin_u = u ? expr_sin(u) : NULL;
        sin_sq = sin_u ? expr_pow(sin_u, &NUM_TWO) : NULL;
        expr_free(sin_u);
        expr_free(u);
        out = div_number_owned_by_product(sin_sq, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SEC, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_TAN, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        u = build_affine_from_match(wrt, c1, k1);
        out = u ? expr_sec(u) : NULL;
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSEC, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COT, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *cosec_u;

        u = build_affine_from_match(wrt, c1, k1);
        cosec_u = u ? expr_cosec(u) : NULL;
        out = cosec_u ? expr_neg(cosec_u) : NULL;
        expr_free(cosec_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (((match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_SEC, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_TAN, &c2, &k2)) ||
                (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_TAN, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_SEC, NUM_TWO, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sec_u;
        expr_t *sec_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sec_u = u ? expr_sec(u) : NULL;
        sec_sq = sec_u ? expr_pow(sec_u, &NUM_TWO) : NULL;
        expr_free(sec_u);
        expr_free(u);
        out = div_number_owned_by_product(sec_sq, NUM_TWO, k1);
    } else if (((match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSEC, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COT, &c2, &k2)) ||
                (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COT, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSEC, NUM_TWO, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *cosec_u;
        expr_t *cosec_sq;
        expr_t *neg_cosec_sq;

        u = build_affine_from_match(wrt, c1, k1);
        cosec_u = u ? expr_cosec(u) : NULL;
        cosec_sq = cosec_u ? expr_pow(cosec_u, &NUM_TWO) : NULL;
        neg_cosec_sq = cosec_sq ? expr_neg(cosec_sq) : NULL;
        expr_free(cosec_sq);
        expr_free(cosec_u);
        expr_free(u);
        out = div_number_owned_by_product(neg_cosec_sq, NUM_TWO, k1);
    } else if (((match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SEC, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSEC, &c2, &k2)) ||
                (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSEC, &c1, &k1) &&
                 match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SEC, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *tan_u;

        u = build_affine_from_match(wrt, c1, k1);
        tan_u = u ? expr_tan(u) : NULL;
        out = tan_u ? expr_log(tan_u) : NULL;
        expr_free(tan_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (((match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, NUM_TWO, &c2, &k2)) ||
                (match_affine_unary_power_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, NUM_TWO, &c1, &k1) &&
                 match_affine_unary_power_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, NUM_TWO, &c2, &k2))) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        number_t four = num_create_from_long(4);
        number_t eight = num_create_from_long(8);
        number_t thirty_two = num_create_from_long(32);
        expr_t *four_u;
        expr_t *sin_four_u;
        expr_t *u_for_linear_term;
        expr_t *u_term;
        expr_t *sin_term;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        four_u = u ? expr_mul_num(u, &four) : NULL;
        sin_four_u = four_u ? expr_sin(four_u) : NULL;
        u_for_linear_term = build_affine_from_match(wrt, c1, k1);
        u_term = u_for_linear_term ? div_number_owned(u_for_linear_term, eight) : NULL;
        sin_term = sin_four_u ? div_number_owned(sin_four_u, thirty_two) : NULL;
        diff = (u_term && sin_term) ? expr_sub(u_term, sin_term) : NULL;
        expr_free(sin_term);
        expr_free(u_term);
        expr_free(four_u);
        expr_free(u);
        out = div_number_owned(diff, k1);
        num_destroy(&thirty_two);
        num_destroy(&eight);
        num_destroy(&four);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SIN, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SIN, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sin_two_u;
        expr_t *scaled_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sin_two_u = two_u ? expr_sin(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        diff = (scaled_u && sin_two_u) ? expr_sub(scaled_u, sin_two_u) : NULL;
        expr_free(scaled_u);
        expr_free(sin_two_u);
        expr_free(two_u);
        expr_free(u);
        out = div_number_owned_by_long_product(diff, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COS, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COS, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sin_two_u;
        expr_t *scaled_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sin_two_u = two_u ? expr_sin(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sum = (scaled_u && sin_two_u) ? expr_add(scaled_u, sin_two_u) : NULL;
        expr_free(scaled_u);
        expr_free(sin_two_u);
        expr_free(two_u);
        expr_free(u);
        out = div_number_owned_by_long_product(sum, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SINH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sinh_u;
        expr_t *sinh_sq;

        u = build_affine_from_match(wrt, c1, k1);
        sinh_u = u ? expr_sinh(u) : NULL;
        sinh_sq = sinh_u ? expr_pow(sinh_u, &NUM_TWO) : NULL;
        expr_free(sinh_u);
        expr_free(u);
        out = div_number_owned_by_product(sinh_sq, NUM_TWO, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SECH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_TANH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *sech_u;

        u = build_affine_from_match(wrt, c1, k1);
        sech_u = u ? expr_sech(u) : NULL;
        out = sech_u ? expr_neg(sech_u) : NULL;
        expr_free(sech_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSECH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COTH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *cosech_u;

        u = build_affine_from_match(wrt, c1, k1);
        cosech_u = u ? expr_cosech(u) : NULL;
        out = cosech_u ? expr_neg(cosech_u) : NULL;
        expr_free(cosech_u);
        expr_free(u);
        out = div_number_owned(out, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_SINH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_SINH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sinh_two_u;
        expr_t *scaled_u;
        expr_t *diff;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sinh_two_u = two_u ? expr_sinh(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        diff = (sinh_two_u && scaled_u) ? expr_sub(sinh_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(sinh_two_u);
        expr_free(two_u);
        expr_free(u);
        out = div_number_owned_by_long_product(diff, 4, k1);
    } else if (match_affine_unary_data(expr->a, wrt, EXPR_PATTERN_UNARY_COSH, &c1, &k1) &&
               match_affine_unary_data(expr->b, wrt, EXPR_PATTERN_UNARY_COSH, &c2, &k2) &&
               affine_linear_match_eq(c1, k1, c2, k2)) {
        expr_t *two_u;
        expr_t *sinh_two_u;
        expr_t *scaled_u;
        expr_t *sum;

        u = build_affine_from_match(wrt, c1, k1);
        two_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sinh_two_u = two_u ? expr_sinh(two_u) : NULL;
        scaled_u = u ? expr_mul_num(u, &NUM_TWO) : NULL;
        sum = (sinh_two_u && scaled_u) ? expr_add(sinh_two_u, scaled_u) : NULL;
        expr_free(scaled_u);
        expr_free(sinh_two_u);
        expr_free(two_u);
        expr_free(u);
        out = div_number_owned_by_long_product(sum, 4, k1);
    }

cleanup:
    num_destroy(&k2);
    num_destroy(&c2);
    num_destroy(&k1);
    num_destroy(&c1);
    return out;
}

static const expr_integrate_dispatch_rule_t integrate_dispatch_rules[EXPR_KIND_COUNT] = {
    [EXPR_KIND_CONST] = { .structural = integrate_constant_rule },
    [EXPR_KIND_VAR] = { .structural = integrate_var_rule },
    [EXPR_KIND_ADD] = { .structural = integrate_add_rule },
    [EXPR_KIND_SUB] = { .structural = integrate_sub_rule },
    [EXPR_KIND_NEG] = { .structural = integrate_neg_rule },
    [EXPR_KIND_MUL] = { .structural = integrate_mul_rule },
    [EXPR_KIND_DIV] = { .structural = integrate_div_rule },
    [EXPR_KIND_POW] = { .structural = integrate_pow_rule },
    [EXPR_KIND_POW_D] = { .structural = integrate_pow_d_rule },
    [EXPR_KIND_SQRT] = { .primitive = integrate_sqrt_rule },
    [EXPR_KIND_LOG] = { .primitive = integrate_log_rule },
    [EXPR_KIND_LOG10] = { .primitive = integrate_log10_rule },
    [EXPR_KIND_EXP] = { .primitive = integrate_exp_rule },
    [EXPR_KIND_SIN] = { .primitive = integrate_sin_rule },
    [EXPR_KIND_COS] = { .primitive = integrate_cos_rule },
    [EXPR_KIND_TAN] = { .primitive = integrate_tan_rule },
    [EXPR_KIND_SEC] = { .primitive = integrate_sec_rule },
    [EXPR_KIND_COSEC] = { .primitive = integrate_cosec_rule },
    [EXPR_KIND_COT] = { .primitive = integrate_cot_rule },
    [EXPR_KIND_SINH] = { .primitive = integrate_sinh_rule },
    [EXPR_KIND_COSH] = { .primitive = integrate_cosh_rule },
    [EXPR_KIND_COSECH] = { .primitive = integrate_cosech_rule },
    [EXPR_KIND_TANH] = { .primitive = integrate_tanh_rule },
    [EXPR_KIND_SECH] = { .primitive = integrate_sech_rule },
    [EXPR_KIND_COTH] = { .primitive = integrate_coth_rule },
    [EXPR_KIND_ASIN] = { .primitive = integrate_asin_rule },
    [EXPR_KIND_ACOS] = { .primitive = integrate_acos_rule },
    [EXPR_KIND_ATAN] = { .primitive = integrate_atan_rule },
    [EXPR_KIND_ASEC] = { .primitive = integrate_asec_rule },
    [EXPR_KIND_ACOSEC] = { .primitive = integrate_acosec_rule },
    [EXPR_KIND_ACOT] = { .primitive = integrate_acot_rule },
    [EXPR_KIND_ASINH] = { .primitive = integrate_asinh_rule },
    [EXPR_KIND_ACOSH] = { .primitive = integrate_acosh_rule },
    [EXPR_KIND_ATANH] = { .primitive = integrate_atanh_rule },
    [EXPR_KIND_ASECH] = { .primitive = integrate_asech_rule },
    [EXPR_KIND_ACOSECH] = { .primitive = integrate_acosech_rule },
    [EXPR_KIND_ACOTH] = { .primitive = integrate_acoth_rule },
    [EXPR_KIND_ERF] = { .primitive = integrate_erf_rule },
    [EXPR_KIND_ERFC] = { .primitive = integrate_erfc_rule },
    [EXPR_KIND_NORMAL_PDF] = { .primitive = integrate_normal_pdf_rule },
    [EXPR_KIND_NORMAL_CDF] = { .primitive = integrate_normal_cdf_rule },
    [EXPR_KIND_NORMAL_LOGPDF] = { .primitive = integrate_normal_logpdf_rule },
    [EXPR_KIND_EI] = { .primitive = integrate_ei_rule },
    [EXPR_KIND_E1] = { .primitive = integrate_e1_rule }
};

static const expr_integrate_dispatch_rule_t *integrate_dispatch_rule_for_kind(expr_op_kind_t kind)
{
    if ((unsigned)kind >= (unsigned)EXPR_KIND_COUNT)
        return NULL;
    return &integrate_dispatch_rules[kind];
}

expr_t *expr_integrate_dispatch_primitive(const expr_t *expr, const expr_t *wrt)
{
    const expr_integrate_dispatch_rule_t *rule;

    if (!expr || !expr->ops)
        return NULL;

    rule = integrate_dispatch_rule_for_kind(expr->ops->kind);
    return (rule && rule->primitive) ? rule->primitive(expr, wrt) : NULL;
}

static expr_t *integrate_dispatch(const expr_t *expr, const expr_t *wrt)
{
    const expr_integrate_dispatch_rule_t *rule;

    if (!expr || !wrt)
        return NULL;

    if (!depends_on_wrt(expr, wrt))
        return integrate_as_constant(expr, wrt);

    rule = integrate_dispatch_rule_for_kind(expr->ops->kind);
    if (rule && rule->structural)
        return rule->structural(expr, wrt);

    if (expr->ops->integrate)
        return expr->ops->integrate(expr, wrt);

    /*
     * Exact subtree u-substitution needs stronger factor extraction and
     * equivalence checking before it is safe to enable as a general fallback.
     */
    return NULL;
}

expr_t *expr_integrate(const expr_t *expr, const expr_t *wrt)
{
    expr_t *simplified;
    expr_t *raw;

    if (!expr || !wrt || !expr_is_var(wrt))
        return NULL;

    simplified = expr_simplify(expr);
    if (!simplified)
        return NULL;

    raw = integrate_dispatch(simplified, wrt);
    expr_free(simplified);
    return simplify_owned(raw);
}
