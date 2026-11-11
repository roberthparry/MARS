/**
 * @file expr_integer.c
 * @brief Exact integer expression operations and scalar projections of multi-result integer routines.
 *
 * Keeps arbitrary-width operands in number_t throughout evaluation. Packed ternary operands retain their
 * individual expression identities for binding replay, native output and semantic TeX presentation.
 * Discrete functions deliberately have no continuous derivative; ordinary formal calculus remains available.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "expression-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_maths.h"

static expr_t *integer_unary(const expr_ops_t *ops, const expr_t *value)
{
    if (!value)
        return NULL;
    expr_retain(value);
    return expr_new_unary_internal(ops, value);
}

static expr_t *integer_binary(const expr_ops_t *ops, const expr_t *left, const expr_t *right)
{
    if (!left || !right)
        return NULL;
    expr_retain(left);
    expr_retain(right);
    return expr_new_binary_internal(ops, left, right);
}

/* Keep the operand pair unevaluated, including when both operands are constants. */
expr_t *expr_integer_pack(const expr_t *left, const expr_t *right)
{
    return integer_binary(&ops_integer_pack, left, right);
}

expr_t *expr_integer_from_pack(const expr_ops_t *ops, const expr_t *parameters, const expr_t *third)
{
    return integer_binary(ops, parameters, third);
}

static expr_t *integer_ternary(const expr_ops_t *ops, const expr_t *a, const expr_t *b, const expr_t *c)
{
    expr_t *pair = expr_integer_pack(a, b);
    expr_t *result = pair ? expr_integer_from_pack(ops, pair, c) : NULL;
    expr_free(pair);
    return result;
}

static expr_t *powmod_from_pack(const expr_t *pair, const expr_t *modulus)
{
    return expr_integer_from_pack(&ops_powmod, pair, modulus);
}

static expr_t *divmod_from_pack(const expr_t *pair, const expr_t *component)
{
    return expr_integer_from_pack(&ops_divmod, pair, component);
}

static expr_t *gcdext_from_pack(const expr_t *pair, const expr_t *component)
{
    return expr_integer_from_pack(&ops_gcdext, pair, component);
}

bool expr_integer_unpack(const expr_t *expression, const expr_t **a, const expr_t **b, const expr_t **c)
{
    if (!expression || !expr_is_op(expression->a, &ops_integer_pack) || !expression->b)
        return false;
    *a = expression->a->a;
    *b = expression->a->b;
    *c = expression->b;
    return *a && *b;
}

static bool integer_index(number_t value, size_t *index)
{
    if (!num_is_exact(value) || !num_is_integer(value) || !num_is_real(value) || num_get_sign(value) < 0)
        return false;
    string_t *text = num_to_string(value);
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = text ? strtoull(string_c_str(text), &end, 10) : 0;
    bool ok = text && end && !*end && !errno && parsed <= SIZE_MAX;
    string_free(text);
    if (ok)
        *index = (size_t)parsed;
    return ok;
}

static number_t eval_integer_pack(expr_t *expression)
{
    (void)expression;
    return NUM_NAN;
}

static number_t eval_powmod(expr_t *expression)
{
    const expr_t *a, *b, *c;
    if (!expr_integer_unpack(expression, &a, &b, &c))
        return NUM_NAN;
    return num_powmod(expr_eval_num_internal((expr_t *)a), expr_eval_num_internal((expr_t *)b),
                      expr_eval_num_internal((expr_t *)c));
}

static number_t eval_bit_length(expr_t *expression)
{
    number_t value = expr_eval_num_internal(expression->a);
    if (!num_is_exact(value) || !num_is_real(value) || !num_is_integer(value))
        return NUM_NAN;
    string_t *text = string_sprintf("%zu", num_bit_length(value));
    number_t result = text ? num_create_from_text(text) : NUM_NAN;
    string_free(text);
    return result;
}

static number_t eval_bit_edit(expr_t *expression)
{
    number_t value = expr_eval_num_internal(expression->a);
    size_t bit;
    if (!num_is_exact(value) || !num_is_real(value) || !num_is_integer(value) ||
        !integer_index(expr_eval_num_internal(expression->b), &bit))
        return NUM_NAN;
    if (expression->ops == &ops_test_bit)
        return num_create_from_long(num_test_bit(value, bit) ? 1 : 0);
    return expression->ops == &ops_set_bit ? num_set_bit(value, bit) : num_clear_bit(value, bit);
}

static number_t eval_prove_prime(expr_t *expression)
{
    return num_create_from_long(num_prove_prime(expr_eval_num_internal(expression->a)));
}

static number_t eval_integer_component(expr_t *expression)
{
    const expr_t *a, *b, *c;
    size_t component;
    bool extended = expression->ops == &ops_gcdext;
    if (!expr_integer_unpack(expression, &a, &b, &c) ||
        !integer_index(expr_eval_num_internal((expr_t *)c), &component) || component >= (extended ? 3u : 2u))
        return NUM_NAN;
    number_t left = expr_eval_num_internal((expr_t *)a);
    number_t right = expr_eval_num_internal((expr_t *)b);
    number_t results[3] = {NUM_NAN, NUM_NAN, NUM_NAN};
    int status = extended ? num_gcdext(left, right, results, results + 1, results + 2)
                          : num_divmod(left, right, results, results + 1);
    number_t result = status == 0 ? num_clone(results[component]) : NUM_NAN;
    for (size_t i = 0; i < 3; ++i)
        num_destroy(results + i);
    return result;
}

const expr_ops_t ops_integer_pack = {
    .eval = eval_integer_pack, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_INTEGER_PACK, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "integer_parameters", .function_name = "integer_parameters",
    .apply_binary = expr_integer_pack, .simplify = expr_simplify_rebuild_binary_operator
};

const expr_ops_t ops_powmod = {
    .eval = eval_powmod, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_POWMOD, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "powmod", .function_name = "powmod",
    .apply_binary = powmod_from_pack, .simplify = expr_simplify_binary_operator
};

const expr_ops_t ops_bit_length = {
    .eval = eval_bit_length, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_BIT_LENGTH, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "bit_length", .function_name = "bit_length",
    .apply_unary = expr_bit_length, .simplify = expr_simplify_unary_operator
};

const expr_ops_t ops_set_bit = {
    .eval = eval_bit_edit, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_SET_BIT, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "set_bit", .function_name = "set_bit",
    .apply_binary = expr_set_bit, .simplify = expr_simplify_binary_operator
};

const expr_ops_t ops_clear_bit = {
    .eval = eval_bit_edit, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_CLEAR_BIT, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "clear_bit", .function_name = "clear_bit",
    .apply_binary = expr_clear_bit, .simplify = expr_simplify_binary_operator
};

const expr_ops_t ops_test_bit = {
    .eval = eval_bit_edit, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_TEST_BIT, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "test_bit", .function_name = "test_bit",
    .apply_binary = expr_test_bit, .simplify = expr_simplify_binary_operator
};

const expr_ops_t ops_prove_prime = {
    .eval = eval_prove_prime, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_PROVE_PRIME, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "prove_prime", .function_name = "prove_prime",
    .apply_unary = expr_prove_prime, .simplify = expr_simplify_unary_operator
};

const expr_ops_t ops_divmod = {
    .eval = eval_integer_component, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_DIVMOD, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "divmod", .function_name = "divmod",
    .apply_binary = divmod_from_pack, .simplify = expr_simplify_binary_operator
};

const expr_ops_t ops_gcdext = {
    .eval = eval_integer_component, .deriv = deriv_not_differentiable, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_GCDEXT, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .expression_name = "gcdext", .function_name = "gcdext",
    .apply_binary = gcdext_from_pack, .simplify = expr_simplify_binary_operator
};

/* Construct exact modular exponentiation without expanding the power. */
expr_t *expr_powmod(const expr_t *base, const expr_t *exponent, const expr_t *modulus)
{
    return integer_ternary(&ops_powmod, base, exponent, modulus);
}

/* Construct the integer bit-length value function. */
expr_t *expr_bit_length(const expr_t *value)
{
    return integer_unary(&ops_bit_length, value);
}

/* Construct an exact integer bit-setting operation. */
expr_t *expr_set_bit(const expr_t *value, const expr_t *index)
{
    return integer_binary(&ops_set_bit, value, index);
}

/* Construct an exact integer bit-clearing operation. */
expr_t *expr_clear_bit(const expr_t *value, const expr_t *index)
{
    return integer_binary(&ops_clear_bit, value, index);
}

/* Construct an integer bit predicate, returning zero or one. */
expr_t *expr_test_bit(const expr_t *value, const expr_t *index)
{
    return integer_binary(&ops_test_bit, value, index);
}

/* Preserve the prime prover's prime, composite and unknown outcomes. */
expr_t *expr_prove_prime(const expr_t *value)
{
    return integer_unary(&ops_prove_prime, value);
}

/* Select the quotient or remainder of exact truncated integer division. */
expr_t *expr_divmod(const expr_t *value, const expr_t *divisor, const expr_t *component)
{
    return integer_ternary(&ops_divmod, value, divisor, component);
}

/* Select the greatest common divisor or either Bezout coefficient. */
expr_t *expr_gcdext(const expr_t *a, const expr_t *b, const expr_t *component)
{
    return integer_ternary(&ops_gcdext, a, b, component);
}
