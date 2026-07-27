#include <stdbool.h>
#include <stdlib.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

typedef struct {
    const expr_t *expr;
    bool expanded;
} substitution_candidate_frame_t;

typedef struct {
    size_t index;
    long value;
} substitution_poly_term_spec_t;

enum {
    substitution_poly_coeffs = 65u,
    substitution_poly_term_spec_capacity = 2u
};

typedef struct {
    number_t constant[substitution_poly_coeffs];
    number_t companion[substitution_poly_coeffs];
} exp_unary_poly_pair_t;

typedef struct {
    expr_op_kind_t candidate_kind;
    expr_op_kind_t companion_kind;
    bool has_companion;
    number_t candidate_deriv[substitution_poly_coeffs];
    number_t companion_deriv[substitution_poly_coeffs];
    number_t companion_square[substitution_poly_coeffs];
} exp_unary_substitution_rule_t;

typedef struct {
    bool has_companion;
    expr_op_kind_t companion_kind;
    size_t candidate_deriv_count;
    substitution_poly_term_spec_t candidate_deriv[substitution_poly_term_spec_capacity];
    size_t companion_deriv_count;
    substitution_poly_term_spec_t companion_deriv[substitution_poly_term_spec_capacity];
    size_t companion_square_count;
    substitution_poly_term_spec_t companion_square[substitution_poly_term_spec_capacity];
} exp_unary_substitution_rule_spec_t;

typedef enum {
    EXP_UNARY_PAIR_CONSTANT,
    EXP_UNARY_PAIR_COMPANION
} exp_unary_pair_slot_t;

typedef struct {
    expr_op_kind_t squared_kind;
    size_t term_count;
    substitution_poly_term_spec_t terms[substitution_poly_term_spec_capacity];
} exp_unary_square_relation_spec_t;

typedef struct {
    bool supported;
    exp_unary_pair_slot_t slot;
    size_t term_count;
    substitution_poly_term_spec_t terms[substitution_poly_term_spec_capacity];
} exp_unary_double_angle_spec_t;

typedef enum {
    EXP_UNARY_DOUBLE_ANGLE_OUTER_INVALID = 0,
    EXP_UNARY_DOUBLE_ANGLE_OUTER_SIN,
    EXP_UNARY_DOUBLE_ANGLE_OUTER_COS,
    EXP_UNARY_DOUBLE_ANGLE_OUTER_SINH,
    EXP_UNARY_DOUBLE_ANGLE_OUTER_COSH,
    EXP_UNARY_DOUBLE_ANGLE_OUTER_COUNT
} exp_unary_double_angle_outer_slot_t;

typedef struct {
    exp_unary_double_angle_spec_t entries[EXP_UNARY_DOUBLE_ANGLE_OUTER_COUNT];
} exp_unary_double_angle_rule_set_t;

typedef struct {
    exp_unary_substitution_rule_spec_t substitution;
    bool has_square_relation;
    exp_unary_square_relation_spec_t square_relation;
    exp_unary_double_angle_rule_set_t double_angle;
} exp_unary_rule_entry_t;

enum {
    exp_unary_rule_kind_first = EXPR_KIND_SIN,
    exp_unary_rule_kind_last = EXPR_KIND_COTH,
    exp_unary_rule_kind_count =
        exp_unary_rule_kind_last - exp_unary_rule_kind_first + 1,
    exp_unary_double_angle_trig_kind_first = EXPR_KIND_SIN,
    exp_unary_double_angle_trig_kind_last = EXPR_KIND_COS,
    exp_unary_double_angle_hyperbolic_kind_first = EXPR_KIND_SINH,
    exp_unary_double_angle_hyperbolic_kind_last = EXPR_KIND_COSH
};

static const exp_unary_rule_entry_t exp_unary_rule_entries[] = {
    {
        .substitution = {
            true, EXPR_KIND_COS,
            1u, { { 0u, 1 } },
            1u, { { 1u, -1 } },
            2u, { { 0u, 1 }, { 2u, -1 } }
        },
        .double_angle = {
            .entries = {
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_SIN] = {
                    true, EXP_UNARY_PAIR_COMPANION, 1u, { { 1u, 2 } }
                },
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_COS] = {
                    true, EXP_UNARY_PAIR_CONSTANT, 2u, { { 0u, 1 }, { 2u, -2 } }
                }
            }
        }
    },
    {
        .substitution = {
            true, EXPR_KIND_SIN,
            1u, { { 0u, -1 } },
            1u, { { 1u, 1 } },
            2u, { { 0u, 1 }, { 2u, -1 } }
        },
        .double_angle = {
            .entries = {
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_SIN] = {
                    true, EXP_UNARY_PAIR_COMPANION, 1u, { { 1u, 2 } }
                },
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_COS] = {
                    true, EXP_UNARY_PAIR_CONSTANT, 2u, { { 0u, -1 }, { 2u, 2 } }
                }
            }
        }
    },
    {
        .substitution = {
            false, EXPR_KIND_CONST,
            2u, { { 0u, 1 }, { 2u, 1 } },
            0u, { { 0u, 0 } },
            0u, { { 0u, 0 } }
        },
        .has_square_relation = true,
        .square_relation = { EXPR_KIND_SEC, 2u, { { 0u, 1 }, { 2u, 1 } } }
    },
    {
        .substitution = {
            true, EXPR_KIND_TAN,
            1u, { { 1u, 1 } },
            1u, { { 2u, 1 } },
            2u, { { 0u, -1 }, { 2u, 1 } }
        }
    },
    {
        .substitution = {
            true, EXPR_KIND_COT,
            1u, { { 1u, -1 } },
            1u, { { 2u, -1 } },
            2u, { { 0u, -1 }, { 2u, 1 } }
        }
    },
    {
        .substitution = {
            false, EXPR_KIND_CONST,
            2u, { { 0u, -1 }, { 2u, -1 } },
            0u, { { 0u, 0 } },
            0u, { { 0u, 0 } }
        },
        .has_square_relation = true,
        .square_relation = { EXPR_KIND_COSEC, 2u, { { 0u, 1 }, { 2u, 1 } } }
    },
    {
        .substitution = {
            true, EXPR_KIND_COSH,
            1u, { { 0u, 1 } },
            1u, { { 1u, 1 } },
            2u, { { 0u, 1 }, { 2u, 1 } }
        },
        .double_angle = {
            .entries = {
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_SINH] = {
                    true, EXP_UNARY_PAIR_COMPANION, 1u, { { 1u, 2 } }
                },
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_COSH] = {
                    true, EXP_UNARY_PAIR_CONSTANT, 2u, { { 0u, 1 }, { 2u, 2 } }
                }
            }
        }
    },
    {
        .substitution = {
            true, EXPR_KIND_SINH,
            1u, { { 0u, 1 } },
            1u, { { 1u, 1 } },
            2u, { { 0u, -1 }, { 2u, 1 } }
        },
        .double_angle = {
            .entries = {
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_SINH] = {
                    true, EXP_UNARY_PAIR_COMPANION, 1u, { { 1u, 2 } }
                },
                [EXP_UNARY_DOUBLE_ANGLE_OUTER_COSH] = {
                    true, EXP_UNARY_PAIR_CONSTANT, 2u, { { 0u, -1 }, { 2u, 2 } }
                }
            }
        }
    },
    {
        .substitution = {
            false, EXPR_KIND_CONST,
            2u, { { 0u, 1 }, { 2u, -1 } },
            0u, { { 0u, 0 } },
            0u, { { 0u, 0 } }
        },
        .has_square_relation = true,
        .square_relation = { EXPR_KIND_SECH, 2u, { { 0u, 1 }, { 2u, -1 } } }
    },
    {
        .substitution = {
            true, EXPR_KIND_TANH,
            1u, { { 1u, -1 } },
            1u, { { 2u, 1 } },
            2u, { { 0u, 1 }, { 2u, -1 } }
        }
    },
    {
        .substitution = {
            true, EXPR_KIND_COTH,
            1u, { { 1u, -1 } },
            1u, { { 2u, -1 } },
            2u, { { 0u, 1 }, { 2u, 1 } }
        }
    },
    {
        .substitution = {
            false, EXPR_KIND_CONST,
            2u, { { 0u, 1 }, { 2u, -1 } },
            0u, { { 0u, 0 } },
            0u, { { 0u, 0 } }
        },
        .has_square_relation = true,
        .square_relation = { EXPR_KIND_COSECH, 2u, { { 0u, -1 }, { 2u, 1 } } }
    }
};

_Static_assert(sizeof(exp_unary_rule_entries) / sizeof(exp_unary_rule_entries[0]) ==
                   exp_unary_rule_kind_count,
               "exp_unary_rule_entries must follow EXPR_KIND_SIN..EXPR_KIND_COTH");

static const exp_unary_rule_entry_t *
exp_unary_rule_entry_for_kind(expr_op_kind_t kind)
{
    unsigned int index = 0u;

    if ((unsigned)kind < (unsigned)exp_unary_rule_kind_first ||
        (unsigned)kind > (unsigned)exp_unary_rule_kind_last) {
        return NULL;
    }

    index = (unsigned)kind - (unsigned)exp_unary_rule_kind_first;
    if (index >= sizeof(exp_unary_rule_entries) /
                 sizeof(exp_unary_rule_entries[0])) {
        return NULL;
    }

    return &exp_unary_rule_entries[index];
}

static exp_unary_double_angle_outer_slot_t
exp_unary_double_angle_outer_slot_for_kind(expr_op_kind_t kind)
{
    if ((unsigned)kind >= (unsigned)exp_unary_double_angle_trig_kind_first &&
        (unsigned)kind <= (unsigned)exp_unary_double_angle_trig_kind_last) {
        return (exp_unary_double_angle_outer_slot_t)
            (EXP_UNARY_DOUBLE_ANGLE_OUTER_SIN +
             ((unsigned)kind -
              (unsigned)exp_unary_double_angle_trig_kind_first));
    }

    if ((unsigned)kind >=
            (unsigned)exp_unary_double_angle_hyperbolic_kind_first &&
        (unsigned)kind <=
            (unsigned)exp_unary_double_angle_hyperbolic_kind_last) {
        return (exp_unary_double_angle_outer_slot_t)
            (EXP_UNARY_DOUBLE_ANGLE_OUTER_SINH +
             ((unsigned)kind -
              (unsigned)exp_unary_double_angle_hyperbolic_kind_first));
    }

    return EXP_UNARY_DOUBLE_ANGLE_OUTER_INVALID;
}

static bool collect_substitution_candidates(const expr_t *expr,
                                            const expr_t *root,
                                            const expr_t *wrt,
                                            const expr_t **candidates,
                                            size_t *count,
                                            size_t capacity)
{
    substitution_candidate_frame_t *stack = NULL;
    size_t stack_count = 0u;
    size_t stack_capacity = 0u;
    bool ok = true;

    if (!expr || !count || *count >= capacity)
        return true;

    stack_capacity = 32u;
    stack = malloc(stack_capacity * sizeof(*stack));
    if (!stack)
        return false;
    stack[stack_count++] = (substitution_candidate_frame_t){ expr, false };

    while (stack_count > 0u && *count < capacity) {
        substitution_candidate_frame_t frame = stack[--stack_count];
        const expr_t *node = frame.expr;

        if (!node)
            continue;

        if (!frame.expanded && (node->a || node->b)) {
            if (stack_count + 3u > stack_capacity) {
                size_t next_capacity = stack_capacity;

                while (stack_count + 3u > next_capacity)
                    next_capacity *= 2u;
                substitution_candidate_frame_t *grown =
                    realloc(stack, next_capacity * sizeof(*stack));

                if (!grown) {
                    ok = false;
                    goto cleanup;
                }
                stack = grown;
                stack_capacity = next_capacity;
            }
            stack[stack_count++] = (substitution_candidate_frame_t){ node, true };
            if (node->b)
                stack[stack_count++] = (substitution_candidate_frame_t){ node->b, false };
            if (node->a)
                stack[stack_count++] = (substitution_candidate_frame_t){ node->a, false };
            continue;
        }

        if (node == root || node == wrt || node->ops->arity == EXPR_OP_ATOM ||
            !depends_on_wrt(node, wrt)) {
            continue;
        }

        for (size_t i = 0; i < *count; ++i) {
            if (candidates[i] == node)
                goto next_node;
        }

        candidates[*count] = node;
        ++(*count);

next_node:
        ;
    }

cleanup:
    free(stack);
    return ok;
}

static bool expr_matches_unary_arg(const expr_t *expr,
                                   expr_op_kind_t kind,
                                   const expr_t *arg);

static expr_t *substitution_build_replacement_poly_from_specs(
    const expr_t *replacement,
    const substitution_poly_term_spec_t *terms,
    size_t count)
{
    expr_t *sum = NULL;

    if (!replacement || !terms)
        return NULL;

    for (size_t i = 0u; i < count; ++i) {
        number_t coeff = num_create_from_long(terms[i].value);
        expr_t *base = NULL;
        expr_t *term = NULL;

        if (terms[i].index == 0u) {
            term = expr_new_const(coeff);
        } else {
            if (terms[i].index == 1u) {
                base = expr_retain_expr(replacement);
            } else {
                number_t exponent = num_create_from_long((long)terms[i].index);

                base = expr_pow(replacement, &exponent);
                num_destroy(&exponent);
            }
            term = base ? expr_mul_num(base, &coeff) : NULL;
            expr_free(base);
        }

        num_destroy(&coeff);
        if (!term) {
            expr_free(sum);
            return NULL;
        }
        sum = expr_add_owned(sum, term);
    }

    return simplify_owned(sum);
}

static expr_t *substitute_candidate_square_relation(const expr_t *expr,
                                                    const expr_t *candidate,
                                                    const expr_t *replacement)
{
    const exp_unary_rule_entry_t *entry = NULL;
    const exp_unary_substitution_rule_spec_t *substitution = NULL;
    const exp_unary_square_relation_spec_t *square_relation = NULL;
    const expr_t *base = NULL;
    expr_op_kind_t base_kind = EXPR_KIND_CONST;
    number_t exponent = num_new();
    expr_t *out = NULL;

    if (!expr || !candidate || !replacement ||
        !candidate->ops || !candidate->a) {
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D &&
        expr->a && num_eq(expr->c, NUM_TWO)) {
        base = expr->a;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent) &&
               num_eq(exponent, NUM_TWO)) {
        base = expr->a;
    }

    if (!base || !base->ops || !base->a ||
        !expr_simplify_same_factor(base->a, candidate->a)) {
        goto cleanup;
    }

    base_kind = base->ops->kind;
    if (base_kind == candidate->ops->kind) {
        out = expr_pow(replacement, &NUM_TWO);
        goto cleanup;
    }

    entry = exp_unary_rule_entry_for_kind(candidate->ops->kind);
    substitution = entry ? &entry->substitution : NULL;
    if (substitution &&
        substitution->has_companion &&
        base_kind == substitution->companion_kind) {
        out = substitution_build_replacement_poly_from_specs(
            replacement,
            substitution->companion_square,
            substitution->companion_square_count);
        goto cleanup;
    }

    square_relation = (entry && entry->has_square_relation)
                          ? &entry->square_relation
                          : NULL;
    if (square_relation && base_kind == square_relation->squared_kind) {
        out = substitution_build_replacement_poly_from_specs(
            replacement,
            square_relation->terms,
            square_relation->term_count);
        goto cleanup;
    }

cleanup:
    num_destroy(&exponent);
    return out;
}

static expr_t *substitute_candidate_with_powers(const expr_t *expr,
                                                const expr_t *candidate,
                                                const expr_t *replacement)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;
    const expr_ops_t *reciprocal_ops = NULL;

    if (!expr)
        return NULL;

    if (expr == candidate || expr_equal_exact_local(expr, candidate)) {
        expr_retain(replacement);
        return (expr_t *)replacement;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a &&
        expr_equal_exact_local(expr->a, candidate)) {
        return expr_pow(replacement, &expr->c);
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b &&
        expr_equal_exact_local(expr->a, candidate)) {
        number_t exponent = num_new();
        bool matched = expr_match_const_value(expr->b, &exponent);

        if (matched) {
            out = expr_pow(replacement, &exponent);
            num_destroy(&exponent);
            return out;
        }
        num_destroy(&exponent);
    }

    out = substitute_candidate_square_relation(expr, candidate, replacement);
    if (out)
        return out;

    if (candidate && candidate->ops && candidate->a)
        reciprocal_ops = expr_ops_reciprocal_unary(candidate->ops);
    if (reciprocal_ops && expr_matches_unary_arg(expr, reciprocal_ops->kind,
                                                candidate->a)) {
        number_t exponent = num_neg(NUM_ONE);

        out = expr_pow(replacement, &exponent);
        num_destroy(&exponent);
        return out;
    }

    if (reciprocal_ops && expr->ops && expr->a &&
        expr_matches_unary_arg(expr->a, reciprocal_ops->kind, candidate->a) &&
        expr->ops->kind == EXPR_KIND_POW_D) {
        number_t exponent = num_neg(expr->c);

        out = expr_pow(replacement, &exponent);
        num_destroy(&exponent);
        return out;
    }

    if (reciprocal_ops && expr->ops && expr->a && expr->b &&
        expr_matches_unary_arg(expr->a, reciprocal_ops->kind, candidate->a) &&
        expr->ops->kind == EXPR_KIND_POW) {
        number_t exponent = num_new();
        bool matched = expr_match_const_value(expr->b, &exponent);

        if (matched) {
            number_t neg_exponent = num_neg(exponent);

            out = expr_pow(replacement, &neg_exponent);
            num_destroy(&neg_exponent);
            num_destroy(&exponent);
            return out;
        }
        num_destroy(&exponent);
    }

    if (expr->ops->kind == EXPR_KIND_CONST) {
        return expr_clone(expr);
    }

    if (expr->ops->kind == EXPR_KIND_VAR) {
        return expr_clone(expr);
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

static bool expr_contains_division_or_inverse_power(const expr_t *expr)
{
    number_t exponent = num_new();
    bool found = false;

    if (!expr || !expr->ops)
        goto cleanup;

    if (expr->ops->kind == EXPR_KIND_DIV) {
        found = true;
        goto cleanup;
    }

    if (expr->ops->kind == EXPR_KIND_POW_D &&
        num_is_real(expr->c) && num_lt(expr->c, NUM_ZERO)) {
        found = true;
        goto cleanup;
    }

    if (expr->ops->kind == EXPR_KIND_POW && expr->b &&
        expr_match_const_value(expr->b, &exponent) &&
        num_is_real(exponent) && num_lt(exponent, NUM_ZERO)) {
        found = true;
        goto cleanup;
    }

    if (expr->ops->arity == EXPR_OP_UNARY) {
        found = expr_contains_division_or_inverse_power(expr->a);
    } else if (expr->ops->arity == EXPR_OP_BINARY) {
        found = expr_contains_division_or_inverse_power(expr->a) ||
                expr_contains_division_or_inverse_power(expr->b);
    }

cleanup:
    num_destroy(&exponent);
    return found;
}

static bool expr_matches_unary_arg(const expr_t *expr,
                                   expr_op_kind_t kind,
                                   const expr_t *arg)
{
    return expr && expr->ops && expr->ops->kind == kind &&
           expr->a && arg && expr_simplify_same_factor(expr->a, arg);
}

static expr_t *expr_build_square(const expr_t *expr)
{
    return expr ? expr_pow(expr, &NUM_TWO) : NULL;
}

static expr_t *mul_long_owned_local(expr_t *expr, long factor)
{
    number_t factor_num = num_create_from_long(factor);
    expr_t *out = mul_number_owned(expr, factor_num);

    num_destroy(&factor_num);
    return out;
}

static expr_t *expr_build_scaled_square_plus_const(const expr_t *expr,
                                                   long square_coeff,
                                                   long const_coeff)
{
    expr_t *square_term = NULL;
    expr_t *const_term = NULL;
    expr_t *out = NULL;
    long square_abs = square_coeff < 0 ? -square_coeff : square_coeff;
    long const_abs = const_coeff < 0 ? -const_coeff : const_coeff;

    if (!expr)
        return NULL;

    if (square_coeff != 0) {
        square_term = expr_build_square(expr);
        if (!square_term)
            goto cleanup;
        if (square_abs != 1L)
            square_term = mul_long_owned_local(square_term, square_abs);
        if (!square_term)
            goto cleanup;
    }

    if (const_coeff != 0) {
        number_t const_num = num_create_from_long(const_abs);

        const_term = expr_new_const(const_num);
        num_destroy(&const_num);
        if (!const_term)
            goto cleanup;
    }

    if (square_term && const_term) {
        if (square_coeff > 0 && const_coeff > 0) {
            out = expr_add(square_term, const_term);
        } else if (square_coeff > 0 && const_coeff < 0) {
            out = expr_sub(square_term, const_term);
        } else if (square_coeff < 0 && const_coeff > 0) {
            out = expr_sub(const_term, square_term);
        } else {
            out = expr_add(square_term, const_term);
            out = expr_negate_owned(out);
        }
    } else if (square_term) {
        out = expr_retain_expr(square_term);
        if (square_coeff < 0)
            out = expr_negate_owned(out);
    } else if (const_term) {
        out = expr_retain_expr(const_term);
        if (const_coeff < 0)
            out = expr_negate_owned(out);
    }

cleanup:
    expr_free(square_term);
    expr_free(const_term);
    return out;
}

static expr_t *expr_build_one_minus_square(const expr_t *expr)
{
    return expr_build_scaled_square_plus_const(expr, -1L, 1L);
}

static expr_t *expr_build_one_minus_two_square(const expr_t *expr)
{
    return expr_build_scaled_square_plus_const(expr, -2L, 1L);
}

static expr_t *expr_build_two_square_minus_one(const expr_t *expr)
{
    return expr_build_scaled_square_plus_const(expr, 2L, -1L);
}

static expr_t *rewrite_trig_candidate_relations(const expr_t *expr,
                                                const expr_t *wrt,
                                                const expr_t *candidate,
                                                const expr_t *replacement);

static expr_t *rewrite_trig_divided_by_candidate_derivative(
    const expr_t *expr,
    const expr_t *wrt,
    const expr_t *candidate,
    const expr_t *replacement);

typedef expr_t *(*trig_rewrite_fn_t)(const expr_t *expr,
                                     const expr_t *wrt,
                                     const expr_t *candidate,
                                     const expr_t *replacement);

static expr_t *rewrite_trig_add_sub_children(const expr_t *expr,
                                             const expr_t *wrt,
                                             const expr_t *candidate,
                                             const expr_t *replacement,
                                             trig_rewrite_fn_t rewrite)
{
    const expr_t *left_node = NULL;
    const expr_t *right_node = NULL;
    bool is_sub = false;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr_match_add_sub_expr(expr, &left_node, &right_node, &is_sub))
        return NULL;

    left = rewrite(left_node, wrt, candidate, replacement);
    right = rewrite(right_node, wrt, candidate, replacement);
    if (!left || !right)
        goto cleanup;

    out = is_sub ? expr_sub(left, right) : expr_add(left, right);

cleanup:
    expr_free(left);
    expr_free(right);
    return out;
}

static expr_t *rewrite_trig_divided_add_sub_children(
    const expr_t *expr,
    const expr_t *denominator,
    const expr_t *wrt,
    const expr_t *candidate,
    const expr_t *replacement,
    trig_rewrite_fn_t rewrite)
{
    const expr_t *left_node = NULL;
    const expr_t *right_node = NULL;
    bool is_sub = false;
    expr_t *left_div = NULL;
    expr_t *right_div = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr_match_add_sub_expr(expr, &left_node, &right_node, &is_sub))
        return NULL;

    left_div = expr_div((expr_t *)left_node, (expr_t *)denominator);
    right_div = expr_div((expr_t *)right_node, (expr_t *)denominator);
    left = rewrite(left_div, wrt, candidate, replacement);
    right = rewrite(right_div, wrt, candidate, replacement);
    if (!left || !right)
        goto cleanup;

    out = is_sub ? expr_sub(left, right) : expr_add(left, right);

cleanup:
    expr_free(left);
    expr_free(right);
    expr_free(left_div);
    expr_free(right_div);
    return out;
}

static expr_t *rewrite_trig_candidate_relations(const expr_t *expr,
                                                const expr_t *wrt,
                                                const expr_t *candidate,
                                                const expr_t *replacement)
{
    const expr_t *arg = NULL;
    expr_op_kind_t denominator_kind = EXPR_KIND_CONST;
    const expr_t *left_node = NULL;
    const expr_t *right_node = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr || !candidate || !replacement)
        return NULL;

    if (expr == candidate || expr_simplify_same_factor(expr, candidate))
        return expr_retain_expr(replacement);

    if (expr_matches_unary_arg(candidate, EXPR_KIND_SIN, candidate->a)) {
        arg = candidate->a;
        denominator_kind = EXPR_KIND_COS;

        if (expr->ops && expr->ops->kind == EXPR_KIND_COS &&
            expr_match_double_argument(expr->a, arg)) {
            return expr_build_one_minus_two_square(replacement);
        }

        if (expr->ops && expr->ops->kind == EXPR_KIND_SIN &&
            expr_match_double_argument(expr->a, arg)) {
            expr_t *other = expr_cos(arg);
            expr_t *scaled = other ? mul_number_owned(expr_clone(replacement),
                                                      NUM_TWO)
                                   : NULL;
            out = (scaled && other) ? expr_mul(scaled, other) : NULL;
            expr_free(scaled);
            expr_free(other);
            return out;
        }

        if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D &&
            num_eq(expr->c, NUM_TWO) &&
            expr_matches_unary_arg(expr->a, EXPR_KIND_COS, arg)) {
            return expr_build_one_minus_square(replacement);
        }

        if (expr_match_mul_expr(expr, &left_node, &right_node) &&
            expr_matches_unary_arg(left_node, EXPR_KIND_COS, arg) &&
            expr_matches_unary_arg(right_node, EXPR_KIND_COS, arg)) {
            return expr_build_one_minus_square(replacement);
        }

        if (expr && expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b &&
            expr_matches_unary_arg(expr->b, EXPR_KIND_COS, arg)) {
            expr_t *arg_clone = expr_clone(arg);
            expr_t *double_arg = arg_clone ? expr_mul_num(arg_clone, &NUM_TWO) : NULL;
            expr_t *double_sin = double_arg ? expr_sin(double_arg) : NULL;

            expr_free(arg_clone);
            expr_free(double_arg);
            if (double_sin && expr_simplify_same_factor(expr->a, double_sin)) {
                expr_free(double_sin);
                return mul_number_owned(expr_clone(replacement), NUM_TWO);
            }
            expr_free(double_sin);
        }
    }

    if (expr_matches_unary_arg(candidate, EXPR_KIND_COS, candidate->a)) {
        arg = candidate->a;
        denominator_kind = EXPR_KIND_SIN;

        if (expr->ops && expr->ops->kind == EXPR_KIND_COS &&
            expr_match_double_argument(expr->a, arg)) {
            return expr_build_two_square_minus_one(replacement);
        }

        if (expr->ops && expr->ops->kind == EXPR_KIND_SIN &&
            expr_match_double_argument(expr->a, arg)) {
            expr_t *other = expr_sin(arg);
            expr_t *scaled = other ? mul_number_owned(expr_clone(replacement),
                                                      NUM_TWO)
                                   : NULL;
            out = (scaled && other) ? expr_mul(scaled, other) : NULL;
            expr_free(scaled);
            expr_free(other);
            return out;
        }

        if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D &&
            num_eq(expr->c, NUM_TWO) &&
            expr_matches_unary_arg(expr->a, EXPR_KIND_SIN, arg)) {
            return expr_build_one_minus_square(replacement);
        }

        if (expr_match_mul_expr(expr, &left_node, &right_node) &&
            expr_matches_unary_arg(left_node, EXPR_KIND_SIN, arg) &&
            expr_matches_unary_arg(right_node, EXPR_KIND_SIN, arg)) {
            return expr_build_one_minus_square(replacement);
        }

        if (expr && expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b &&
            expr_matches_unary_arg(expr->b, EXPR_KIND_SIN, arg)) {
            expr_t *arg_clone = expr_clone(arg);
            expr_t *double_arg = arg_clone ? expr_mul_num(arg_clone, &NUM_TWO) : NULL;
            expr_t *double_sin = double_arg ? expr_sin(double_arg) : NULL;

            expr_free(arg_clone);
            expr_free(double_arg);
            if (double_sin && expr_simplify_same_factor(expr->a, double_sin)) {
                expr_free(double_sin);
                return mul_number_owned(expr_clone(replacement), NUM_TWO);
            }
            expr_free(double_sin);
        }
    }

    if (arg && (denominator_kind == EXPR_KIND_COS || denominator_kind == EXPR_KIND_SIN) &&
        expr_match_mul_expr(expr, &left_node, &right_node)) {
        const expr_t *companion = NULL;
        const expr_t *other = NULL;

        if (expr_matches_unary_arg(left_node, denominator_kind, arg)) {
            companion = left_node;
            other = right_node;
        } else if (expr_matches_unary_arg(right_node, denominator_kind, arg)) {
            companion = right_node;
            other = left_node;
        }

        if (companion && other) {
            expr_t *other_div = expr_div((expr_t *)other, (expr_t *)companion);
            expr_t *other_rewritten = rewrite_trig_candidate_relations(
                other_div, wrt, candidate, replacement);
            expr_t *companion_sq = other_rewritten
                                       ? expr_build_one_minus_square(replacement)
                                       : NULL;

            out = (companion_sq && other_rewritten)
                      ? expr_mul(companion_sq, other_rewritten)
                      : NULL;
            expr_free(companion_sq);
            expr_free(other_rewritten);
            expr_free(other_div);
            if (out)
                return out;
        }
    }

    if (arg && (denominator_kind == EXPR_KIND_COS || denominator_kind == EXPR_KIND_SIN) &&
        expr && expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b &&
        expr_matches_unary_arg(expr->b, denominator_kind, arg)) {
        if (expr_simplify_same_factor(expr->a, expr->b))
            return expr_new_const(NUM_ONE);

        {
            expr_t *factored = expr_simplify_extract_exact_factor_quotient(
                expr->a, expr->b);

            if (factored) {
                expr_t *rewritten_factored = rewrite_trig_candidate_relations(
                    factored, wrt, candidate, replacement);

                expr_free(factored);
                if (rewritten_factored)
                    return rewritten_factored;
            }
        }

        if (expr->a->ops && expr->a->ops->kind == EXPR_KIND_NEG &&
            expr->a->a) {
            expr_t *inner_div = expr_div((expr_t *)expr->a->a, (expr_t *)expr->b);
            expr_t *inner = rewrite_trig_candidate_relations(inner_div, wrt,
                                                             candidate, replacement);

            expr_free(inner_div);
            return expr_negate_owned(inner);
        }

        {
            number_t numer_scale = num_new();
            const expr_t *numer_base = NULL;

            if (expr_match_scaled_expr(expr->a, &numer_scale, &numer_base) &&
                numer_base && numer_base != expr->a) {
                expr_t *base_div = expr_div((expr_t *)numer_base, (expr_t *)expr->b);
                expr_t *base_rewritten = rewrite_trig_candidate_relations(
                    base_div, wrt, candidate, replacement);
                expr_t *scaled = base_rewritten ? mul_number_owned(base_rewritten,
                                                                   numer_scale)
                                                : NULL;

                expr_free(base_div);
                num_destroy(&numer_scale);
                return scaled;
            }
            num_destroy(&numer_scale);
        }

        if (expr->a->ops &&
            (expr->a->ops->kind == EXPR_KIND_ADD ||
             expr->a->ops->kind == EXPR_KIND_SUB) &&
            expr->a->a && expr->a->b) {
            return rewrite_trig_divided_add_sub_children(
                expr->a, expr->b, wrt, candidate, replacement,
                rewrite_trig_candidate_relations);
        }

        if (expr_match_mul_expr(expr->a, &left_node, &right_node)) {
            if (expr_simplify_same_factor(left_node, expr->b))
                return rewrite_trig_candidate_relations(right_node, wrt, candidate, replacement);
            if (expr_simplify_same_factor(right_node, expr->b))
                return rewrite_trig_candidate_relations(left_node, wrt, candidate, replacement);

            {
                expr_t *left_div = expr_div((expr_t *)left_node, (expr_t *)expr->b);
                expr_t *right_rewritten = rewrite_trig_candidate_relations(
                    right_node, wrt, candidate, replacement);

                left = rewrite_trig_candidate_relations(left_div, wrt, candidate, replacement);
                expr_free(left_div);
                if (left && right_rewritten && !depends_on_wrt(left, wrt)) {
                    out = expr_mul(left, right_rewritten);
                    expr_free(right_rewritten);
                    expr_free(left);
                    return out;
                }
                expr_free(right_rewritten);
                expr_free(left);

                expr_t *right_div = expr_div((expr_t *)right_node, (expr_t *)expr->b);
                expr_t *left_rewritten = rewrite_trig_candidate_relations(
                    left_node, wrt, candidate, replacement);

                right = rewrite_trig_candidate_relations(right_div, wrt, candidate, replacement);
                expr_free(right_div);
                if (left_rewritten && right && !depends_on_wrt(right, wrt)) {
                    out = expr_mul(left_rewritten, right);
                    expr_free(right);
                    expr_free(left_rewritten);
                    return out;
                }
                expr_free(right);
                expr_free(left_rewritten);
            }
        }
    }

    if (expr->ops->kind == EXPR_KIND_CONST) {
        return expr_clone(expr);
    }

    if (expr->ops->kind == EXPR_KIND_VAR) {
        return expr_clone(expr);
    }

    if (expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        left = rewrite_trig_candidate_relations(expr->a, wrt, candidate, replacement);
        if (!left)
            return NULL;
        out = expr_pow(left, &expr->c);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_UNARY && expr->ops->apply_unary) {
        left = rewrite_trig_candidate_relations(expr->a, wrt, candidate, replacement);
        if (!left)
            return NULL;
        out = expr->ops->apply_unary(left);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_BINARY && expr->ops->apply_binary) {
        left = rewrite_trig_candidate_relations(expr->a, wrt, candidate, replacement);
        right = rewrite_trig_candidate_relations(expr->b, wrt, candidate, replacement);
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

static expr_t *rewrite_trig_divided_by_candidate_derivative(
    const expr_t *expr,
    const expr_t *wrt,
    const expr_t *candidate,
    const expr_t *replacement)
{
    const expr_t *arg = NULL;
    expr_op_kind_t denominator_kind = EXPR_KIND_CONST;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;
    const expr_t *left_node = NULL;
    const expr_t *right_node = NULL;
    bool is_sub = false;

    if (!expr || !wrt || !candidate || !replacement)
        return NULL;

    if (expr_matches_unary_arg(candidate, EXPR_KIND_SIN, candidate->a)) {
        arg = candidate->a;
        denominator_kind = EXPR_KIND_COS;
    } else if (expr_matches_unary_arg(candidate, EXPR_KIND_COS, candidate->a)) {
        arg = candidate->a;
        denominator_kind = EXPR_KIND_SIN;
    } else {
        return NULL;
    }

    if (expr_matches_unary_arg(expr, denominator_kind, arg))
        return expr_new_const(NUM_ONE);

    if (denominator_kind == EXPR_KIND_COS &&
        expr && expr->ops && expr->ops->kind == EXPR_KIND_SUB &&
        expr_matches_unary_arg(expr->b, EXPR_KIND_COS, arg) &&
        expr->a && expr->a->ops && expr->a->ops->kind == EXPR_KIND_SUB &&
        expr->a->a && expr->a->b &&
        expr->a->a->ops && expr->a->a->ops->kind == EXPR_KIND_MUL &&
        expr_matches_unary_arg(expr->a->a->a, EXPR_KIND_COS, arg) &&
        expr_matches_unary_arg(expr->a->b, EXPR_KIND_SIN, expr->a->b->a) &&
        expr_match_double_argument(expr->a->b->a, arg) &&
        expr->a->a->b && expr->a->a->b->ops && expr->a->a->b->ops->kind == EXPR_KIND_SUB) {
        expr_t *u_sq = expr_build_square(replacement);
        expr_t *three_u = mul_long_owned_local(expr_clone(replacement), 3L);
        expr_t *sum = (u_sq && three_u) ? expr_add(u_sq, three_u) : NULL;

        expr_free(three_u);
        expr_free(u_sq);
        return sum ? expr_negate_owned(sum) : NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_SIN &&
        expr_match_double_argument(expr->a, arg)) {
        return mul_number_owned(expr_clone(replacement), NUM_TWO);
    }

    if (expr_match_add_sub_expr(expr, &left_node, &right_node, &is_sub))
        return rewrite_trig_add_sub_children(
            expr, wrt, candidate, replacement,
            rewrite_trig_divided_by_candidate_derivative);

    if (expr_match_mul_expr(expr, &left_node, &right_node)) {
        if (expr_matches_unary_arg(left_node, denominator_kind, arg))
            return rewrite_trig_candidate_relations(right_node, wrt, candidate, replacement);
        if (expr_matches_unary_arg(right_node, denominator_kind, arg))
            return rewrite_trig_candidate_relations(left_node, wrt, candidate, replacement);

        left = rewrite_trig_divided_by_candidate_derivative(left_node, wrt, candidate, replacement);
        if (left) {
            right = rewrite_trig_candidate_relations(right_node, wrt, candidate, replacement);
            if (right && !depends_on_wrt(left, wrt)) {
                out = expr_mul(left, right);
                expr_free(right);
                expr_free(left);
                return out;
            }
            expr_free(right);
            expr_free(left);
        }

        right = rewrite_trig_divided_by_candidate_derivative(right_node, wrt, candidate, replacement);
        if (right) {
            left = rewrite_trig_candidate_relations(left_node, wrt, candidate, replacement);
            if (left && !depends_on_wrt(right, wrt)) {
                out = expr_mul(left, right);
                expr_free(left);
                expr_free(right);
                return out;
            }
            expr_free(left);
            expr_free(right);
        }
    }

    return NULL;
}

static expr_t *extract_wrt_independent_scale(const expr_t *expr,
                                             const expr_t *factor,
                                             const expr_t *wrt)
{
    expr_t *scale = NULL;

    if (!expr || !factor || !wrt)
        return NULL;

    scale = expr_simplify_extract_exact_factor_quotient(expr, factor);
    scale = simplify_owned(scale);
    if (!scale && expr_simplify_additive_terms_equal(expr, factor))
        scale = expr_const_one();
    if (scale && depends_on_wrt(scale, wrt)) {
        expr_free(scale);
        scale = NULL;
    }

    return scale;
}

static expr_t *integrate_exp_chain_successor_candidate(
    const expr_t *expr,
    const expr_t *wrt,
    const expr_t *exp_factor)
{
    expr_t *quotient = NULL;
    expr_t *first_deriv = NULL;
    expr_t *second_deriv = NULL;
    expr_t *first_deriv_squared = NULL;
    expr_t *expected = NULL;
    expr_t *expected_deriv = NULL;
    expr_t *first_times_expected = NULL;
    expr_t *third_expected = NULL;
    expr_t *scale = NULL;
    expr_t *base = NULL;
    expr_t *base_deriv = NULL;
    expr_t *scaled = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !exp_factor ||
        !exp_factor->ops || exp_factor->ops->kind != EXPR_KIND_EXP ||
        !exp_factor->a || !depends_on_wrt(exp_factor->a, wrt)) {
        return NULL;
    }

    quotient = expr_simplify_extract_common_factor_quotient(expr, exp_factor);
    quotient = simplify_owned(quotient);
    if (!quotient)
        goto cleanup;

    first_deriv = expr_create_deriv(exp_factor->a, wrt);
    first_deriv = simplify_owned(first_deriv);
    if (!first_deriv || expr_is_exact_zero(first_deriv))
        goto cleanup;

    second_deriv = expr_create_deriv(first_deriv, wrt);
    second_deriv = simplify_owned(second_deriv);
    first_deriv_squared = expr_pow(first_deriv, &NUM_TWO);
    expected = (first_deriv_squared && second_deriv)
                   ? expr_add(first_deriv_squared, second_deriv)
                   : NULL;
    expected = simplify_owned(expected);
    if (!expected)
        goto cleanup;

    base = expr_mul((expr_t *)exp_factor, first_deriv);
    if (!base)
        goto cleanup;

    scale = extract_wrt_independent_scale(quotient, expected, wrt);
    if (!scale) {
        base_deriv = expr_create_deriv(base, wrt);
        base_deriv = simplify_owned(base_deriv);
        scale = extract_wrt_independent_scale(expr, base_deriv, wrt);
    }

    if (scale) {
        scaled = expr_mul(scale, base);
        out = simplify_owned(scaled);
        scaled = NULL;
        if (out)
            goto cleanup;
    }

    /*
     * One rung higher in the same exp(f) chain:
     *     D(exp(f) * (f'^2 + f'')) =
     *         exp(f) * (f' * (f'^2 + f'') + D(f'^2 + f'')).
     */
    expr_free(base);
    base = NULL;
    expr_free(scale);
    scale = NULL;
    expr_free(base_deriv);
    base_deriv = NULL;
    expected_deriv = expr_create_deriv(expected, wrt);
    expected_deriv = simplify_owned(expected_deriv);
    first_times_expected = expr_mul(first_deriv, expected);
    third_expected = (first_times_expected && expected_deriv)
                         ? expr_add(first_times_expected, expected_deriv)
                         : NULL;
    third_expected = simplify_owned(third_expected);
    if (!third_expected)
        goto cleanup;

    scale = extract_wrt_independent_scale(quotient, third_expected, wrt);
    if (!scale)
        goto cleanup;

    base = expr_mul((expr_t *)exp_factor, expected);
    scaled = base ? expr_mul(scale, base) : NULL;
    out = simplify_owned(scaled);
    scaled = NULL;

cleanup:
    expr_free(scaled);
    expr_free(base_deriv);
    expr_free(base);
    expr_free(scale);
    expr_free(third_expected);
    expr_free(first_times_expected);
    expr_free(expected_deriv);
    expr_free(expected);
    expr_free(first_deriv_squared);
    expr_free(second_deriv);
    expr_free(first_deriv);
    expr_free(quotient);
    return out;
}

static bool exp_factor_stack_push(const expr_t ***stack,
                                  size_t *count,
                                  size_t *capacity,
                                  const expr_t *expr)
{
    const expr_t **grown = NULL;
    size_t next_capacity = 0u;

    if (!expr)
        return true;

    if (*count == *capacity) {
        next_capacity = *capacity ? *capacity * 2u : 32u;
        grown = realloc(*stack, next_capacity * sizeof(**stack));
        if (!grown)
            return false;
        *stack = grown;
        *capacity = next_capacity;
    }

    (*stack)[(*count)++] = expr;
    return true;
}

static bool collect_exp_factor_candidates(const expr_t *expr,
                                          const expr_t *wrt,
                                          const expr_t **candidates,
                                          size_t *count,
                                          size_t capacity)
{
    const expr_t **stack = NULL;
    size_t stack_count = 0u;
    size_t stack_capacity = 0u;
    bool ok = true;

    if (!expr || !wrt || !candidates || !count || *count >= capacity)
        return true;

    if (!exp_factor_stack_push(&stack, &stack_count, &stack_capacity, expr))
        return false;

    while (stack_count > 0u && *count < capacity) {
        const expr_t *node = stack[--stack_count];

        if (!node || !node->ops)
            continue;

        if (node->ops->kind == EXPR_KIND_EXP && node->a &&
            depends_on_wrt(node->a, wrt)) {
            bool seen = false;

            for (size_t i = 0u; i < *count; ++i) {
                if (candidates[i] == node ||
                    expr_simplify_same_factor(candidates[i], node)) {
                    seen = true;
                    break;
                }
            }
            if (!seen)
                candidates[(*count)++] = node;
        }

        if (!exp_factor_stack_push(&stack, &stack_count, &stack_capacity,
                                   node->b)) {
            ok = false;
            goto cleanup;
        }
        if (!exp_factor_stack_push(&stack, &stack_count, &stack_capacity,
                                   node->a)) {
            ok = false;
            goto cleanup;
        }
    }

cleanup:
    free(stack);
    return ok;
}

static expr_t *integrate_exp_chain_successor_product(const expr_t *expr,
                                                     const expr_t *wrt)
{
    const expr_t *candidates[32];
    size_t count = 0u;

    if (!expr || !wrt)
        return NULL;
    if (!collect_exp_factor_candidates(expr, wrt, candidates, &count,
                                       sizeof(candidates) / sizeof(candidates[0]))) {
        return NULL;
    }

    for (size_t i = 0u; i < count; ++i) {
        if (candidates[i] && candidates[i]->ops &&
            candidates[i]->ops->kind == EXPR_KIND_EXP) {
            expr_t *out = integrate_exp_chain_successor_candidate(
                expr, wrt, candidates[i]);

            if (out)
                return out;
        }
    }

    return NULL;
}

static expr_t *integrate_poly_times_exp_relaxed(const expr_t *expr,
                                                const expr_t *wrt)
{
    const expr_t *target = expr;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *poly_expr = NULL;
    const expr_t *exp_expr = NULL;
    number_t poly_scale = num_clone(NUM_ONE);
    expr_t *vars[1];
    number_t poly[5];
    number_t anti[5];
    number_t poly_constant = num_new();
    number_t poly_coeffs[1];
    expr_t *poly_anti = NULL;
    expr_t *exp_clone = NULL;
    expr_t *out = NULL;

    number_array_zero_local(poly, 5);
    number_array_zero_local(anti, 5);
    poly_coeffs[0] = num_new();

    if (!expr || !wrt)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG && expr->a) {
        target = expr->a;
        num_destroy(&poly_scale);
        poly_scale = num_neg(NUM_ONE);
    }

    if (!expr_match_mul_expr(target, &left, &right))
        goto cleanup;

    if (expr_matches_unary_arg(left, EXPR_KIND_EXP, wrt)) {
        exp_expr = left;
        poly_expr = right;
    } else if (left && left->ops && left->ops->kind == EXPR_KIND_NEG &&
               expr_matches_unary_arg(left->a, EXPR_KIND_EXP, wrt)) {
        exp_expr = left->a;
        poly_expr = right;
        num_destroy(&poly_scale);
        poly_scale = num_neg(NUM_ONE);
    } else if (expr_matches_unary_arg(right, EXPR_KIND_EXP, wrt)) {
        exp_expr = right;
        poly_expr = left;
    } else if (right && right->ops && right->ops->kind == EXPR_KIND_NEG &&
               expr_matches_unary_arg(right->a, EXPR_KIND_EXP, wrt)) {
        exp_expr = right->a;
        poly_expr = left;
        num_destroy(&poly_scale);
        poly_scale = num_neg(NUM_ONE);
    } else {
        goto cleanup;
    }

    vars[0] = (expr_t *)wrt;
    if (!expr_match_affine_poly_deg4(poly_expr, 1u, vars, poly, &poly_constant, poly_coeffs) ||
        !num_eq(poly_constant, NUM_ZERO) ||
        !num_eq(poly_coeffs[0], NUM_ONE)) {
        goto cleanup;
    }

    for (size_t i = 0; i < 5u; ++i) {
        number_t scaled = num_mul(poly[i], poly_scale);

        num_destroy(&poly[i]);
        poly[i] = scaled;
    }

    exp_antiderivative_once_local(poly, 5u, anti);
    poly_anti = build_polynomial_expr(wrt, anti, 5u);
    exp_clone = expr_clone(exp_expr);
    out = (poly_anti && exp_clone) ? expr_mul(poly_anti, exp_clone) : NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(exp_clone);
    expr_free(poly_anti);
    num_destroy(&poly_scale);
    num_destroy(&poly_coeffs[0]);
    num_destroy(&poly_constant);
    number_array_clear_local(anti, 5);
    number_array_clear_local(poly, 5);
    return out;
}

static void substitution_poly_set_long(number_t *poly,
                                       size_t count,
                                       size_t index,
                                       long value)
{
    if (!poly || index >= count)
        return;

    num_destroy(&poly[index]);
    poly[index] = num_create_from_long(value);
}

static void substitution_poly_apply_term_specs(
    number_t *poly,
    size_t count,
    const substitution_poly_term_spec_t *terms,
    size_t term_count)
{
    if (!poly || !terms)
        return;

    for (size_t i = 0u; i < term_count; ++i)
        substitution_poly_set_long(poly, count, terms[i].index, terms[i].value);
}

static bool substitution_poly_is_zero(const number_t *poly, size_t count)
{
    if (!poly)
        return true;

    for (size_t i = 0; i < count; ++i) {
        if (!num_eq(poly[i], NUM_ZERO))
            return false;
    }
    return true;
}

static size_t substitution_poly_used_count(const number_t *poly, size_t count)
{
    for (size_t i = count; i-- > 0u;) {
        if (!num_eq(poly[i], NUM_ZERO))
            return i + 1u;
    }
    return 1u;
}

static size_t exp_unary_poly_pair_used_count(const exp_unary_poly_pair_t *pair)
{
    size_t constant_count = substitution_poly_used_count(pair->constant,
                                                         substitution_poly_coeffs);
    size_t companion_count = substitution_poly_used_count(pair->companion,
                                                          substitution_poly_coeffs);
    size_t used = constant_count > companion_count ? constant_count : companion_count;

    /*
     * The differential operators can shift degrees a little while solving
     * backwards. A small cushion keeps compact higher derivatives available
     * without forcing every ordinary case through a full 65x65 solve.
     */
    used += 4u;
    if (used < 8u)
        used = 8u;
    if (used > substitution_poly_coeffs)
        used = substitution_poly_coeffs;
    return used;
}

static bool substitution_poly_eq(const number_t *left,
                                 const number_t *right,
                                 size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (!num_eq(left[i], right[i]))
            return false;
    }
    return true;
}

static void substitution_poly_copy(number_t *dst,
                                   const number_t *src,
                                   size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static bool substitution_poly_accumulate_number(number_t *poly,
                                                size_t count,
                                                size_t index,
                                                number_t value)
{
    number_t next;

    if (index >= count)
        return num_eq(value, NUM_ZERO);
    if (num_eq(value, NUM_ZERO))
        return true;

    next = num_add(poly[index], value);
    num_destroy(&poly[index]);
    poly[index] = next;
    return true;
}

static bool substitution_poly_accumulate_scaled_long(number_t *poly,
                                                     size_t count,
                                                     size_t index,
                                                     number_t value,
                                                     long scale)
{
    number_t scale_num;
    number_t scaled;
    bool ok;

    if (scale == 0 || num_eq(value, NUM_ZERO))
        return true;

    scale_num = num_create_from_long(scale);
    scaled = num_mul(value, scale_num);
    ok = substitution_poly_accumulate_number(poly, count, index, scaled);
    num_destroy(&scaled);
    num_destroy(&scale_num);
    return ok;
}

static bool substitution_poly_add_scaled(number_t *dst,
                                         const number_t *src,
                                         size_t count,
                                         long scale)
{
    for (size_t i = 0; i < count; ++i) {
        if (!substitution_poly_accumulate_scaled_long(dst, count, i, src[i], scale))
            return false;
    }
    return true;
}

static bool substitution_poly_mul(const number_t *left,
                                  const number_t *right,
                                  number_t *out,
                                  size_t count)
{
    number_array_reset_zero_local(out, count);

    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            number_t term = num_mul(left[i], right[j]);
            bool ok = substitution_poly_accumulate_number(out, count, i + j,
                                                          term);

            num_destroy(&term);
            if (!ok)
                return false;
        }
    }
    return true;
}

static void exp_unary_poly_pair_zero(exp_unary_poly_pair_t *pair)
{
    number_array_zero_local(pair->constant, substitution_poly_coeffs);
    number_array_zero_local(pair->companion, substitution_poly_coeffs);
}

static void exp_unary_poly_pair_clear(exp_unary_poly_pair_t *pair)
{
    number_array_clear_local(pair->companion, substitution_poly_coeffs);
    number_array_clear_local(pair->constant, substitution_poly_coeffs);
}

static void exp_unary_poly_pair_reset(exp_unary_poly_pair_t *pair)
{
    number_array_reset_zero_local(pair->constant, substitution_poly_coeffs);
    number_array_reset_zero_local(pair->companion, substitution_poly_coeffs);
}

static void exp_unary_poly_pair_copy(exp_unary_poly_pair_t *dst,
                                     const exp_unary_poly_pair_t *src)
{
    substitution_poly_copy(dst->constant, src->constant,
                           substitution_poly_coeffs);
    substitution_poly_copy(dst->companion, src->companion,
                           substitution_poly_coeffs);
}

static bool exp_unary_poly_pair_add_scaled(exp_unary_poly_pair_t *dst,
                                           const exp_unary_poly_pair_t *src,
                                           long scale)
{
    return substitution_poly_add_scaled(dst->constant, src->constant,
                                        substitution_poly_coeffs, scale) &&
           substitution_poly_add_scaled(dst->companion, src->companion,
                                        substitution_poly_coeffs, scale);
}

static bool exp_unary_poly_pair_mul(const exp_unary_poly_pair_t *left,
                                    const exp_unary_poly_pair_t *right,
                                    const exp_unary_substitution_rule_t *rule,
                                    exp_unary_poly_pair_t *out)
{
    number_t pure[substitution_poly_coeffs];
    number_t companion_square_product[substitution_poly_coeffs];
    number_t companion_square_reduced[substitution_poly_coeffs];
    number_t left_companion[substitution_poly_coeffs];
    number_t right_companion[substitution_poly_coeffs];
    bool ok = false;

    number_array_zero_local(pure, substitution_poly_coeffs);
    number_array_zero_local(companion_square_product, substitution_poly_coeffs);
    number_array_zero_local(companion_square_reduced, substitution_poly_coeffs);
    number_array_zero_local(left_companion, substitution_poly_coeffs);
    number_array_zero_local(right_companion, substitution_poly_coeffs);
    exp_unary_poly_pair_reset(out);

    if (!substitution_poly_mul(left->constant, right->constant, pure,
                               substitution_poly_coeffs) ||
        !substitution_poly_mul(left->companion, right->companion,
                               companion_square_product,
                               substitution_poly_coeffs) ||
        !substitution_poly_mul(companion_square_product,
                               rule->companion_square,
                               companion_square_reduced,
                               substitution_poly_coeffs) ||
        !substitution_poly_mul(left->constant, right->companion,
                               left_companion, substitution_poly_coeffs) ||
        !substitution_poly_mul(left->companion, right->constant,
                               right_companion, substitution_poly_coeffs)) {
        goto cleanup;
    }

    substitution_poly_copy(out->constant, pure, substitution_poly_coeffs);
    if (!substitution_poly_add_scaled(out->constant, companion_square_reduced,
                                      substitution_poly_coeffs, 1) ||
        !substitution_poly_add_scaled(out->companion, left_companion,
                                      substitution_poly_coeffs, 1) ||
        !substitution_poly_add_scaled(out->companion, right_companion,
                                      substitution_poly_coeffs, 1)) {
        goto cleanup;
    }
    ok = true;

cleanup:
    number_array_clear_local(right_companion, substitution_poly_coeffs);
    number_array_clear_local(left_companion, substitution_poly_coeffs);
    number_array_clear_local(companion_square_reduced, substitution_poly_coeffs);
    number_array_clear_local(companion_square_product, substitution_poly_coeffs);
    number_array_clear_local(pure, substitution_poly_coeffs);
    return ok;
}

static bool exp_unary_poly_pair_pow(const exp_unary_poly_pair_t *base,
                                    unsigned int exponent,
                                    const exp_unary_substitution_rule_t *rule,
                                    exp_unary_poly_pair_t *out)
{
    exp_unary_poly_pair_t acc;
    exp_unary_poly_pair_t next;
    bool ok = true;

    exp_unary_poly_pair_zero(&acc);
    exp_unary_poly_pair_zero(&next);
    exp_unary_poly_pair_reset(out);
    substitution_poly_set_long(out->constant, substitution_poly_coeffs, 0u, 1);
    exp_unary_poly_pair_copy(&acc, out);

    for (unsigned int i = 0u; i < exponent; ++i) {
        if (!exp_unary_poly_pair_mul(&acc, base, rule, &next)) {
            ok = false;
            break;
        }
        exp_unary_poly_pair_copy(&acc, &next);
    }

    if (ok)
        exp_unary_poly_pair_copy(out, &acc);

    exp_unary_poly_pair_clear(&next);
    exp_unary_poly_pair_clear(&acc);
    return ok;
}

static void exp_unary_rule_zero(exp_unary_substitution_rule_t *rule)
{
    rule->candidate_kind = EXPR_KIND_CONST;
    rule->companion_kind = EXPR_KIND_CONST;
    rule->has_companion = false;
    number_array_zero_local(rule->candidate_deriv, substitution_poly_coeffs);
    number_array_zero_local(rule->companion_deriv, substitution_poly_coeffs);
    number_array_zero_local(rule->companion_square, substitution_poly_coeffs);
}

static void exp_unary_rule_clear(exp_unary_substitution_rule_t *rule)
{
    number_array_clear_local(rule->companion_square, substitution_poly_coeffs);
    number_array_clear_local(rule->companion_deriv, substitution_poly_coeffs);
    number_array_clear_local(rule->candidate_deriv, substitution_poly_coeffs);
}

static bool exp_unary_rule_init(exp_unary_substitution_rule_t *rule,
                                expr_op_kind_t kind)
{
    const exp_unary_rule_entry_t *entry = NULL;
    const exp_unary_substitution_rule_spec_t *spec = NULL;

    exp_unary_rule_zero(rule);

    entry = exp_unary_rule_entry_for_kind(kind);
    if (!entry) {
        exp_unary_rule_clear(rule);
        return false;
    }

    spec = &entry->substitution;
    rule->candidate_kind = kind;
    rule->companion_kind = spec->companion_kind;
    rule->has_companion = spec->has_companion;
    substitution_poly_apply_term_specs(rule->candidate_deriv,
                                       substitution_poly_coeffs,
                                       spec->candidate_deriv,
                                       spec->candidate_deriv_count);
    substitution_poly_apply_term_specs(rule->companion_deriv,
                                       substitution_poly_coeffs,
                                       spec->companion_deriv,
                                       spec->companion_deriv_count);
    substitution_poly_apply_term_specs(rule->companion_square,
                                       substitution_poly_coeffs,
                                       spec->companion_square,
                                       spec->companion_square_count);
    return true;
}

static bool exp_unary_square_relation_for_kind(expr_op_kind_t squared_kind,
                                               const exp_unary_substitution_rule_t *rule,
                                               exp_unary_poly_pair_t *out)
{
    exp_unary_poly_pair_reset(out);

    if (squared_kind == rule->candidate_kind) {
        substitution_poly_set_long(out->constant, substitution_poly_coeffs,
                                   2u, 1);
        return true;
    }

    if (rule->has_companion && squared_kind == rule->companion_kind) {
        substitution_poly_copy(out->constant, rule->companion_square,
                               substitution_poly_coeffs);
        return true;
    }

    const exp_unary_rule_entry_t *entry =
        exp_unary_rule_entry_for_kind(rule->candidate_kind);
    const exp_unary_square_relation_spec_t *spec =
        (entry && entry->has_square_relation) ? &entry->square_relation : NULL;

    if (spec && spec->squared_kind == squared_kind) {
        substitution_poly_apply_term_specs(out->constant,
                                           substitution_poly_coeffs,
                                           spec->terms,
                                           spec->term_count);
        return true;
    }

    return false;
}

static bool exp_unary_pair_from_expr(const expr_t *expr,
                                     const expr_t *wrt,
                                     const expr_t *candidate,
                                     const exp_unary_substitution_rule_t *rule,
                                     exp_unary_poly_pair_t *out);

static bool exp_unary_pair_from_power(const expr_t *base,
                                      unsigned int exponent,
                                      const expr_t *wrt,
                                      const expr_t *candidate,
                                      const exp_unary_substitution_rule_t *rule,
                                      exp_unary_poly_pair_t *out)
{
    exp_unary_poly_pair_t base_pair;
    bool ok = false;

    exp_unary_poly_pair_zero(&base_pair);

    if (base && base->ops &&
        expr_matches_unary_arg(base, base->ops->kind, candidate->a) &&
        exp_unary_square_relation_for_kind(base->ops->kind, rule, out) &&
        exponent == 2u) {
        ok = true;
        goto cleanup;
    }

    if (!exp_unary_pair_from_expr(base, wrt, candidate, rule, &base_pair))
        goto cleanup;
    ok = exp_unary_poly_pair_pow(&base_pair, exponent, rule, out);

cleanup:
    exp_unary_poly_pair_clear(&base_pair);
    return ok;
}

static bool exp_unary_pair_from_double_angle(const expr_t *expr,
                                             const expr_t *arg,
                                             const exp_unary_substitution_rule_t *rule,
                                             exp_unary_poly_pair_t *out)
{
    if (!expr || !arg || !expr->ops || !expr->a ||
        !expr_match_double_argument(expr->a, arg)) {
        return false;
    }

    exp_unary_poly_pair_reset(out);

    const exp_unary_rule_entry_t *entry =
        exp_unary_rule_entry_for_kind(rule->candidate_kind);
    const exp_unary_double_angle_rule_set_t *rule_set =
        entry ? &entry->double_angle : NULL;
    exp_unary_double_angle_outer_slot_t outer_slot =
        exp_unary_double_angle_outer_slot_for_kind(expr->ops->kind);

    if (rule_set &&
        outer_slot != EXP_UNARY_DOUBLE_ANGLE_OUTER_INVALID &&
        outer_slot < EXP_UNARY_DOUBLE_ANGLE_OUTER_COUNT) {
        const exp_unary_double_angle_spec_t *spec =
            &rule_set->entries[outer_slot];

        if (spec->supported) {
            number_t *target = spec->slot == EXP_UNARY_PAIR_COMPANION
                                   ? out->companion
                                   : out->constant;

            substitution_poly_apply_term_specs(target,
                                               substitution_poly_coeffs,
                                               spec->terms,
                                               spec->term_count);
            return true;
        }
    }

    return false;
}

static bool exp_unary_pair_from_expr(const expr_t *expr,
                                     const expr_t *wrt,
                                     const expr_t *candidate,
                                     const exp_unary_substitution_rule_t *rule,
                                     exp_unary_poly_pair_t *out)
{
    const expr_t *left_node = NULL;
    const expr_t *right_node = NULL;
    bool is_sub = false;
    number_t const_value = num_new();
    number_t exponent_value = num_new();
    exp_unary_poly_pair_t left;
    exp_unary_poly_pair_t right;
    exp_unary_poly_pair_t product;
    bool ok = false;

    exp_unary_poly_pair_zero(&left);
    exp_unary_poly_pair_zero(&right);
    exp_unary_poly_pair_zero(&product);
    exp_unary_poly_pair_reset(out);

    if (!expr || !candidate || !rule)
        goto cleanup;

    if (expr == candidate || expr_simplify_same_factor(expr, candidate)) {
        substitution_poly_set_long(out->constant, substitution_poly_coeffs,
                                   1u, 1);
        ok = true;
        goto cleanup;
    }

    if (rule->has_companion &&
        expr_matches_unary_arg(expr, rule->companion_kind, candidate->a)) {
        substitution_poly_set_long(out->companion, substitution_poly_coeffs,
                                   0u, 1);
        ok = true;
        goto cleanup;
    }

    if (expr_match_const_value(expr, &const_value)) {
        num_destroy(&out->constant[0]);
        out->constant[0] = num_clone(const_value);
        ok = true;
        goto cleanup;
    }

    if (!depends_on_wrt(expr, wrt)) {
        goto cleanup;
    }

    if (exp_unary_pair_from_double_angle(expr, candidate->a, rule, out)) {
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG && expr->a) {
        if (!exp_unary_pair_from_expr(expr->a, wrt, candidate, rule, &left))
            goto cleanup;
        ok = exp_unary_poly_pair_add_scaled(out, &left, -1);
        goto cleanup;
    }

    if (expr_match_add_sub_expr(expr, &left_node, &right_node, &is_sub)) {
        if (!exp_unary_pair_from_expr(left_node, wrt, candidate, rule, &left) ||
            !exp_unary_pair_from_expr(right_node, wrt, candidate, rule, &right)) {
            goto cleanup;
        }
        if (!exp_unary_poly_pair_add_scaled(out, &left, 1) ||
            !exp_unary_poly_pair_add_scaled(out, &right, is_sub ? -1 : 1)) {
            goto cleanup;
        }
        ok = true;
        goto cleanup;
    }

    if (expr_match_mul_expr(expr, &left_node, &right_node)) {
        if (!exp_unary_pair_from_expr(left_node, wrt, candidate, rule, &left) ||
            !exp_unary_pair_from_expr(right_node, wrt, candidate, rule, &right) ||
            !exp_unary_poly_pair_mul(&left, &right, rule, &product)) {
            goto cleanup;
        }
        exp_unary_poly_pair_copy(out, &product);
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b) {
        if (!exp_unary_pair_from_expr(expr->a, wrt, candidate, rule, &left))
            goto cleanup;

        if (!depends_on_wrt(expr->b, wrt) &&
            expr_match_const_value(expr->b, &const_value) &&
            !num_eq(const_value, NUM_ZERO)) {
            for (size_t i = 0; i < substitution_poly_coeffs; ++i) {
                num_destroy(&out->constant[i]);
                out->constant[i] = num_div(left.constant[i], const_value);
                num_destroy(&out->companion[i]);
                out->companion[i] = num_div(left.companion[i], const_value);
            }
            ok = true;
        }
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        unsigned int exponent = 0u;

        if (!expr_integrate_number_matches_uint_at_most(
                expr->c, substitution_poly_coeffs - 1u, &exponent)) {
            goto cleanup;
        }
        ok = exp_unary_pair_from_power(expr->a, exponent, wrt, candidate,
                                       rule, out);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b &&
        expr_match_const_value(expr->b, &exponent_value)) {
        unsigned int exponent = 0u;

        if (!expr_integrate_number_matches_uint_at_most(
                exponent_value, substitution_poly_coeffs - 1u, &exponent)) {
            goto cleanup;
        }
        ok = exp_unary_pair_from_power(expr->a, exponent, wrt, candidate,
                                       rule, out);
        goto cleanup;
    }

cleanup:
    exp_unary_poly_pair_clear(&product);
    exp_unary_poly_pair_clear(&right);
    exp_unary_poly_pair_clear(&left);
    num_destroy(&exponent_value);
    num_destroy(&const_value);
    return ok;
}

static bool substitution_poly_apply_differential(const number_t *m,
                                                 const number_t *n,
                                                 const number_t *p,
                                                 number_t *out,
                                                 size_t count)
{
    number_array_reset_zero_local(out, count);

    for (size_t i = 0; i < count; ++i) {
        if (i > 0u) {
            for (size_t j = 0; j < count; ++j) {
                number_t scale = num_create_from_long((long)i);
                number_t scaled = num_mul(p[i], scale);
                number_t term = num_mul(scaled, m[j]);
                bool ok = substitution_poly_accumulate_number(out, count,
                                                              i - 1u + j,
                                                              term);

                num_destroy(&term);
                num_destroy(&scaled);
                num_destroy(&scale);
                if (!ok)
                    return false;
            }
        }

        for (size_t j = 0; j < count; ++j) {
            number_t term = num_mul(p[i], n[j]);
            bool ok = substitution_poly_accumulate_number(out, count,
                                                          i + j, term);

            num_destroy(&term);
            if (!ok)
                return false;
        }
    }
    return true;
}

static bool substitution_solve_polynomial_differential(const number_t *m,
                                                       const number_t *n,
                                                       const number_t *q,
                                                       number_t *out,
                                                       size_t count)
{
    number_t matrix[substitution_poly_coeffs][substitution_poly_coeffs + 1u];
    number_t check[substitution_poly_coeffs];
    size_t pivot_cols[substitution_poly_coeffs];
    size_t pivot_count = 0u;
    bool ok = false;

    if (count == 0u || count > substitution_poly_coeffs)
        return false;

    for (size_t row = 0; row < count; ++row) {
        for (size_t col = 0; col <= count; ++col)
            matrix[row][col] = num_new();
    }
    number_array_zero_local(check, count);
    number_array_reset_zero_local(out, count);

    for (size_t col = 0; col < count; ++col) {
        if (col > 0u) {
            for (size_t j = 0; j < count && col - 1u + j < count; ++j) {
                number_t scale = num_create_from_long((long)col);
                number_t scaled = num_mul(m[j], scale);

                substitution_poly_accumulate_number(&matrix[col - 1u + j][col],
                                                    1u, 0u, scaled);
                num_destroy(&scaled);
                num_destroy(&scale);
            }
        }

        for (size_t j = 0; j < count && col + j < count; ++j) {
            substitution_poly_accumulate_number(&matrix[col + j][col],
                                                1u, 0u, n[j]);
        }
    }

    for (size_t row = 0; row < count; ++row) {
        num_destroy(&matrix[row][count]);
        matrix[row][count] = num_clone(q[row]);
    }

    for (size_t col = 0u, row = 0u; col < count && row < count; ++col) {
        size_t pivot = row;

        while (pivot < count && num_eq(matrix[pivot][col], NUM_ZERO))
            ++pivot;
        if (pivot == count)
            continue;

        if (pivot != row) {
            for (size_t j = 0u; j <= count; ++j) {
                number_t tmp = matrix[row][j];

                matrix[row][j] = matrix[pivot][j];
                matrix[pivot][j] = tmp;
            }
        }

        for (size_t r = 0; r < count; ++r) {
            number_t factor;

            if (r == row || num_eq(matrix[r][col], NUM_ZERO))
                continue;

            factor = num_div(matrix[r][col], matrix[row][col]);
            for (size_t c = 0u; c <= count; ++c) {
                number_t scaled = num_mul(factor, matrix[row][c]);
                number_t next = num_sub(matrix[r][c], scaled);

                num_destroy(&matrix[r][c]);
                matrix[r][c] = next;
                num_destroy(&scaled);
            }
            num_destroy(&factor);
        }

        pivot_cols[pivot_count++] = col;
        ++row;
    }

    for (size_t row = 0; row < count; ++row) {
        bool all_zero = true;

        for (size_t col = 0; col < count; ++col) {
            if (!num_eq(matrix[row][col], NUM_ZERO)) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && !num_eq(matrix[row][count], NUM_ZERO))
            goto cleanup;
    }

    for (size_t row = 0; row < pivot_count; ++row) {
        size_t pivot_col = pivot_cols[row];
        number_t value = num_div(matrix[row][count], matrix[row][pivot_col]);

        num_destroy(&out[pivot_col]);
        out[pivot_col] = value;
    }

    if (!substitution_poly_apply_differential(m, n, out, check, count) ||
        !substitution_poly_eq(check, q, count)) {
        goto cleanup;
    }

    ok = true;

cleanup:
    number_array_clear_local(check, count);
    for (size_t row = 0; row < count; ++row) {
        for (size_t col = 0; col <= count; ++col)
            num_destroy(&matrix[row][col]);
    }
    return ok;
}

static bool exp_unary_build_constant_operator(
    const exp_unary_substitution_rule_t *rule,
    number_t *m,
    number_t *n)
{
    number_t ca[substitution_poly_coeffs];
    bool ok = false;

    number_array_zero_local(ca, substitution_poly_coeffs);
    number_array_reset_zero_local(m, substitution_poly_coeffs);
    number_array_reset_zero_local(n, substitution_poly_coeffs);

    if (!substitution_poly_mul(rule->companion_square, rule->candidate_deriv,
                               ca, substitution_poly_coeffs)) {
        goto cleanup;
    }

    substitution_poly_copy(m, ca, substitution_poly_coeffs);
    substitution_poly_copy(n, ca, substitution_poly_coeffs);
    if (!substitution_poly_add_scaled(n, rule->companion_deriv,
                                      substitution_poly_coeffs, 1)) {
        goto cleanup;
    }
    ok = true;

cleanup:
    number_array_clear_local(ca, substitution_poly_coeffs);
    return ok;
}

static expr_t *build_exp_unary_pair_expr(const expr_t *candidate,
                                         const expr_t *arg,
                                         const exp_unary_substitution_rule_t *rule,
                                         const number_t *pure,
                                         const number_t *companion_poly,
                                         size_t coeff_count)
{
    expr_t *pure_expr = NULL;
    expr_t *companion_expr = NULL;
    expr_t *companion_poly_expr = NULL;
    expr_t *companion_term = NULL;
    expr_t *sum = NULL;

    pure_expr = build_polynomial_expr(candidate, pure, coeff_count);
    if (!pure_expr)
        goto cleanup;

    if (rule->has_companion &&
        !substitution_poly_is_zero(companion_poly, coeff_count)) {
        companion_expr = expr_apply_unary_kind(rule->companion_kind, arg);
        companion_poly_expr = build_polynomial_expr(candidate, companion_poly,
                                                    coeff_count);
        companion_term = (companion_expr && companion_poly_expr)
                             ? expr_mul(companion_expr, companion_poly_expr)
                             : NULL;
        if (!companion_term)
            goto cleanup;
        sum = expr_add(pure_expr, companion_term);
    } else {
        sum = expr_retain_expr(pure_expr);
    }

cleanup:
    expr_free(companion_term);
    expr_free(companion_poly_expr);
    expr_free(companion_expr);
    expr_free(pure_expr);
    return simplify_owned(sum);
}

static expr_t *integrate_exp_unary_substitution_product(const expr_t *expr,
                                                        const expr_t *wrt,
                                                        const expr_t *candidate)
{
    exp_unary_substitution_rule_t rule;
    exp_unary_poly_pair_t quotient_pair;
    number_t pure[substitution_poly_coeffs];
    number_t companion[substitution_poly_coeffs];
    number_t constant_m[substitution_poly_coeffs];
    number_t constant_n[substitution_poly_coeffs];
    number_t arg_constant = num_new();
    number_t arg_coeff = num_new();
    expr_t *exp_candidate = NULL;
    expr_t *quotient = NULL;
    expr_t *factor = NULL;
    expr_t *exp_clone = NULL;
    expr_t *raw = NULL;
    expr_t *out = NULL;
    size_t solve_count = substitution_poly_coeffs;
    bool rule_ready = false;

    exp_unary_poly_pair_zero(&quotient_pair);
    number_array_zero_local(pure, substitution_poly_coeffs);
    number_array_zero_local(companion, substitution_poly_coeffs);
    number_array_zero_local(constant_m, substitution_poly_coeffs);
    number_array_zero_local(constant_n, substitution_poly_coeffs);

    if (!expr || !wrt || !candidate || !candidate->ops || !candidate->a)
        goto cleanup;

    rule_ready = exp_unary_rule_init(&rule, candidate->ops->kind);
    if (!rule_ready)
        goto cleanup;

    if (!match_nonconstant_affine_linear_expr(candidate->a, wrt, &arg_constant,
                                              &arg_coeff) ||
        num_eq(arg_coeff, NUM_ZERO)) {
        goto cleanup;
    }

    exp_candidate = expr_exp(candidate);
    quotient = exp_candidate
                   ? expr_simplify_extract_common_factor_quotient(expr, exp_candidate)
                   : NULL;
    if (!quotient)
        goto cleanup;

    if (!exp_unary_pair_from_expr(quotient, wrt, candidate, &rule,
                                  &quotient_pair)) {
        goto cleanup;
    }
    solve_count = exp_unary_poly_pair_used_count(&quotient_pair);

    if (rule.has_companion) {
        if (!substitution_solve_polynomial_differential(
                rule.candidate_deriv, rule.candidate_deriv,
                quotient_pair.companion, pure, solve_count) ||
            !exp_unary_build_constant_operator(&rule, constant_m, constant_n) ||
            !substitution_solve_polynomial_differential(
                constant_m, constant_n, quotient_pair.constant, companion,
                solve_count)) {
            goto cleanup;
        }
    } else {
        if (!substitution_poly_is_zero(quotient_pair.companion,
                                       solve_count) ||
            !substitution_solve_polynomial_differential(
                rule.candidate_deriv, rule.candidate_deriv,
                quotient_pair.constant, pure, solve_count)) {
            goto cleanup;
        }
    }

    factor = build_exp_unary_pair_expr(candidate, candidate->a, &rule, pure,
                                       companion, solve_count);
    exp_clone = expr_exp(candidate);
    raw = (exp_clone && factor) ? expr_mul(exp_clone, factor) : NULL;
    out = div_number_owned(raw, arg_coeff);
    raw = NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(raw);
    expr_free(exp_clone);
    expr_free(factor);
    expr_free(quotient);
    expr_free(exp_candidate);
    if (rule_ready)
        exp_unary_rule_clear(&rule);
    number_array_clear_local(constant_n, substitution_poly_coeffs);
    number_array_clear_local(constant_m, substitution_poly_coeffs);
    number_array_clear_local(companion, substitution_poly_coeffs);
    number_array_clear_local(pure, substitution_poly_coeffs);
    exp_unary_poly_pair_clear(&quotient_pair);
    num_destroy(&arg_coeff);
    num_destroy(&arg_constant);
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
    bool used[2] = { false, false };
    bool used_relation_rewrite = false;
    bool derivative_contains_inverse = false;
    bool trusted_antiderivative = false;

    if (!expr || !wrt || !candidate || candidate == wrt)
        return NULL;

    du = expr_create_deriv(candidate, wrt);
    du = simplify_owned(du);
    if (!du || expr_is_exact_zero(du))
        goto cleanup;
    derivative_contains_inverse = expr_contains_division_or_inverse_power(du);

    u = expr_new_named_var(NUM_ZERO, "u");
    quotient = expr_simplify_extract_exact_factor_quotient(expr, du);
    if (!quotient) {
        ratio = expr_div(expr, du);
        quotient = simplify_owned(ratio);
        ratio = NULL;
    }
    if (!quotient)
        goto cleanup;

    transformed = u ? substitute_candidate_with_powers(quotient, candidate, u) : NULL;
    transformed = simplify_owned(transformed);
    if (transformed && u && depends_on_wrt(transformed, wrt)) {
        const expr_t *product_left = NULL;
        const expr_t *product_right = NULL;
        expr_t *rewritten = rewrite_trig_candidate_relations(transformed, wrt, candidate, u);
        expr_t *direct = NULL;

        if (expr_match_mul_expr(expr, &product_left, &product_right)) {
            expr_t *left_rewritten = rewrite_trig_candidate_relations(
                product_left, wrt, candidate, u);
            expr_t *right_divided = rewrite_trig_divided_by_candidate_derivative(
                product_right, wrt, candidate, u);

            if (left_rewritten && right_divided &&
                !depends_on_wrt(left_rewritten, wrt) &&
                !depends_on_wrt(right_divided, wrt)) {
                direct = expr_mul(left_rewritten, right_divided);
            }
            expr_free(right_divided);
            expr_free(left_rewritten);

            if (!direct) {
                expr_t *right_rewritten = rewrite_trig_candidate_relations(
                    product_right, wrt, candidate, u);
                expr_t *left_divided = rewrite_trig_divided_by_candidate_derivative(
                    product_left, wrt, candidate, u);

                if (right_rewritten && left_divided &&
                    !depends_on_wrt(right_rewritten, wrt) &&
                    !depends_on_wrt(left_divided, wrt)) {
                    direct = expr_mul(left_divided, right_rewritten);
                }
                expr_free(left_divided);
                expr_free(right_rewritten);
            }
        }

        if (!direct) {
            direct = rewrite_trig_divided_by_candidate_derivative(
                expr, wrt, candidate, u);
        }

        direct = simplify_owned(direct);
        if (direct && !depends_on_wrt(direct, wrt)) {
            expr_free(rewritten);
            rewritten = direct;
        } else {
            expr_free(direct);
        }

        if (rewritten && expr_equal_exact_local(rewritten, transformed)) {
            expr_free(rewritten);
        } else if (rewritten) {
            expr_free(transformed);
            transformed = simplify_owned(rewritten);
            used_relation_rewrite = true;
        }
    }
    if (transformed) {
        expr_t *normalized =
            expr_simplify_normalize_negated_mul_factor(transformed);

        if (normalized) {
            expr_free(transformed);
            transformed = simplify_owned(normalized);
        }
    }
    if (!transformed)
        goto cleanup;

    vars[0] = (expr_t *)wrt;
    vars[1] = u;
    if (!expr_collect_var_usage(transformed, 2u, vars, used) || used[0])
        goto cleanup;

    anti_u = expr_integrate(transformed, u);
    if (anti_u && !used_relation_rewrite && !derivative_contains_inverse)
        trusted_antiderivative = true;
    if (!anti_u) {
        anti_u = integrate_poly_times_exp_relaxed(transformed, u);
        trusted_antiderivative = (anti_u != NULL);
    }
    if (!anti_u)
        goto cleanup;

    back = expr_substitute(anti_u, u, (expr_t *)candidate);
    out = simplify_owned(back);
    back = NULL;

    if (out && !trusted_antiderivative) {
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

expr_t *integrate_exact_substitution_product(const expr_t *expr,
                                             const expr_t *wrt)
{
    const expr_t *candidates[32];
    size_t count = 0u;
    number_t four = num_create_from_long(4);
    expr_t *chain = NULL;

    if (!expr || !wrt || expr->ops->arity == EXPR_OP_ATOM)
        goto cleanup;

    chain = integrate_exp_chain_successor_product(expr, wrt);
    if (chain) {
        num_destroy(&four);
        return chain;
    }

    if (!collect_substitution_candidates(expr, expr, wrt, candidates, &count,
                                         sizeof(candidates) / sizeof(candidates[0]))) {
        goto cleanup;
    }

    for (size_t i = 0; i < count; ++i) {
        expr_t *out = integrate_exp_unary_substitution_product(expr, wrt,
                                                               candidates[i]);

        if (out) {
            num_destroy(&four);
            return out;
        }

        out = integrate_exact_substitution_candidate(expr, wrt, candidates[i]);

        if (out) {
            num_destroy(&four);
            return out;
        }

        if (candidates[i]) {
            number_t exponent = num_new();
            bool fourth_power =
                (candidates[i]->ops->kind == EXPR_KIND_POW_D &&
                 num_is_real(candidates[i]->c) &&
                 num_eq(candidates[i]->c, four)) ||
                (candidates[i]->ops->kind == EXPR_KIND_POW &&
                 candidates[i]->b &&
                 expr_match_const_value(candidates[i]->b, &exponent) &&
                 num_eq(exponent, four));

            if (fourth_power) {
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
    }

cleanup:
    num_destroy(&four);
    return NULL;
}
