/**
 * @file expr_notation_output.c
 * @brief Compose an expression result and its native binding envelope.
 *
 * The expression module chooses which body and bindings belong to the result.
 * TeX constructors supply all enclosure and separator syntax; no native graph
 * or binding implementation is inspected by this composition layer.
 */
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_notation_internal.h"

/* Select expressions whose native display should remain a single semantic body. */
bool expr_notation_requires_unwrapped_body(const expr_t *expression)
{
    if (!expression)
        return false;
    if (expression->binding_expr && !expr_is_const(expression))
        return true;
    expr_cartesian_composition_t view;
    if (!expr_cartesian_composition_init(expression, &view))
        return false;
    expr_cartesian_composition_clear(&view);
    return true;
}

/* Project a native expression body through its semantic callback context. */
char *expr_to_TeX_body(const expr_t *expression)
{
    TeX_context_t *context = expression ? expr_TeX_context(expression) : NULL;
    TeX_t *notation = context ? TeX_from_context(context) : NULL;
    char *result = notation ? strdup(TeX_source(notation)) : NULL;
    TeX_free(notation);
    TeX_context_free(context);
    return result;
}

/* Preserve authored operations and term order in an independent semantic snapshot. */
char *expr_to_TeX_operation_body(const expr_t *expression)
{
    if (!expression)
        return NULL;
    notation_build_t *build = notation_build_new();
    notation_build_source_order(build, true);
    const TeX_model_node_t *root = notation_build_expr(build, expression);
    string_t *text = notation_build_render(build, root);
    char *result = text ? strdup(string_c_str(text)) : NULL;
    string_free(text);
    notation_build_free(build);
    return result;
}

 /* Preserve the native output entry point while delegating envelope syntax to TeX. */
string_t *expr_to_TeX_text(const expr_t *expression)
{
    char *body_text = NULL, *binding_text = NULL;
    if (expr_to_TeX_parts(expression, &body_text, &binding_text) != 0)
        return expr_to_text_expr(expression);
    TeX_t *body = body_text ? TeX_raw(body_text) : NULL;
    bool bound = binding_text && *binding_text;
    TeX_t *bindings = bound ? TeX_raw(binding_text) : NULL;
    TeX_t *notation = body && bound && bindings ? TeX_bound(body, bindings) : NULL;
    string_t *result = bound ? (notation ? TeX_render(notation) : NULL) : (body ? TeX_render(body) : NULL);
    TeX_free(notation);
    TeX_free(bindings);
    TeX_free(body);
    free(binding_text);
    free(body_text);
    return result;
}
