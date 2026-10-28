/**
 * @file expr_notation_unary.c
 * @brief Supply operation names and unary meaning to presentation adapters.
 *
 * Expression interprets its private operation kinds and atomic arguments.
 * Consumers receive canonical names and signal classification, not TeX syntax.
 */
#include "expression-t.h"

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/* Interpret supported Greek aliases and single Unicode Greek symbols. */
bool expr_notation_is_greek_symbol(const char *name)
{
    if (!name || !*name)
        return false;
    string_t *text = string_new_with(*name == '@' ? name + 1 : name);
    if (!text)
        return false;
    string_t *normalised = expr_normalise_greek_alias_text(text);
    bool greek = normalised != NULL;
    if (!greek) {
        string_cursor_t *cursor = string_cursor_new(text);
        if (cursor) {
            unsigned int codepoint = rune_value(string_cursor_peek(cursor));
            string_cursor_next(cursor);
            greek = string_cursor_done(cursor) &&
                ((codepoint >= 0x0370u && codepoint <= 0x03ffu) || (codepoint >= 0x1f00u && codepoint <= 0x1fffu));
            string_cursor_free(cursor);
        }
    }
    string_free(normalised);
    string_free(text);
    return greek;
}

/* Supply canonical mathematical identifiers while retaining operation-table ownership. */
bool expr_notation_operation_names(const expr_t *expression, expr_notation_operation_names_t *result)
{
    if (!expression || !expression->ops || !result)
        return false;
    *result = (expr_notation_operation_names_t){
        .canonical_name = expression->ops->function_name,
        .expression_name = expression->ops->expression_name
    };
    return true;
}

/* Classify operation kinds by direct indexing; no operation-name search is required. */
bool expr_notation_unary(const expr_t *expression, expr_notation_unary_t *result)
{
    static const bool signals[EXPR_KIND_COUNT] = {
        [EXPR_KIND_SGN]             = true,
        [EXPR_KIND_STEP]            = true,
        [EXPR_KIND_RECT]            = true,
        [EXPR_KIND_TRI]             = true,
        [EXPR_KIND_CIRC]            = true,
        [EXPR_KIND_SINC]            = true,
        [EXPR_KIND_DELTA]           = true,
        [EXPR_KIND_ANALYTIC_DELTA]  = true,
        [EXPR_KIND_PRINCIPAL_VALUE] = true,
        [EXPR_KIND_FINITE_PART]     = true
    };
    if (!expression || !expression->ops || expression->ops->arity != EXPR_OP_UNARY || !result)
        return false;
    unsigned kind = expression->ops->kind;
    const expr_t *argument = expression->a;
    const char *argument_name = argument && (expr_is_var(argument) || expr_is_const(argument))
        ? argument->name : NULL;
    *result = (expr_notation_unary_t){
        .name = expr_is_op(expression, &ops_abs) ? NULL : expression->ops->function_name,
        .atomic_argument_name = argument_name,
        .signal_function = kind < EXPR_KIND_COUNT && signals[kind]
    };
    return true;
}
