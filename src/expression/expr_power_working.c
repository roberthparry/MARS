/**
 * @file expr_power_working.c
 * @brief Retain exact power reductions as native mathematical working.
 *
 * Native integration presentation uses this expression-owned traversal before endpoint expressions are simplified.
 * Each distinct graph node is inspected once using a pointer dictionary. Only certified rational powers are
 * reported; variable bindings, irrational roots and complex branches are not replaced by numerical estimates.
 */

#include <stdint.h>

#include "dictionary-t.h"
#include "string-t.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static size_t power_working_hash(const void *key)
{
    uintptr_t pointer = (uintptr_t)*(const expr_t *const *)key;
    return (size_t)((pointer >> 4) ^ (pointer >> 19));
}

static int power_working_compare(const void *left, const void *right)
{
    uintptr_t a = (uintptr_t)*(const expr_t *const *)left;
    uintptr_t b = (uintptr_t)*(const expr_t *const *)right;
    return a == b ? 0 : (a < b ? -1 : 1);
}

static bool append_power_working(const expr_t *expr, string_t *working)
{
    if (!expr_is_op(expr, &ops_pow) && !expr_is_op(expr, &ops_pow_d))
        return true;

    expr_t *base = expr_simplify(expr->a);
    expr_t *exponent = expr_is_op(expr, &ops_pow) ? expr_simplify(expr->b) : expr_new_const(expr->c);
    number_t exact = NUM_NAN;
    bool ok = base && exponent;
    if (ok && expr_simplify_is_plain_real_const(base) && expr_simplify_is_plain_real_const(exponent) &&
        expr_fold_rational_power_const(&base->c, exponent->c, &exact)) {
        expr_t *power = expr_pow(base, &exponent->c), *result = expr_new_const(exact);
        string_t *line = power && result ? expr_sprintf_text("%nu = %nu", power, result) : NULL;
        ok = line && (!string_byte_length(working) || string_append_cstr(working, "\n") == 0) &&
             string_append_string(working, line) == 0;
        string_free(line);
        expr_free(result);
        expr_free(power);
    }
    num_destroy(&exact);
    expr_free(exponent);
    expr_free(base);
    return ok;
}

static bool visit_power_working(const expr_t *expr, dictionary_t *visited, string_t *working)
{
    if (!expr)
        return true;
    bool seen = true;
    if (dictionary_get(visited, &expr, &seen))
        return true;
    if (!dictionary_set(visited, &expr, &seen))
        return false;
    return visit_power_working(expr->a, visited, working) && visit_power_working(expr->b, visited, working) &&
           append_power_working(expr, working);
}

/* Produce independent native working without modifying or retaining the source graph. */
string_t *expr_exact_power_working(const expr_t *expr)
{
    string_t *working = expr ? string_new() : NULL;
    dictionary_t *visited = dictionary_create(sizeof(expr_t *), sizeof(bool), power_working_hash,
                                              power_working_compare, NULL, NULL, NULL, NULL, NULL);
    bool ok = working && visited && visit_power_working(expr, visited, working);
    dictionary_destroy(visited);
    if (!ok) {
        string_free(working);
        return NULL;
    }
    return working;
}
