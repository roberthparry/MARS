/**
 * @file expr_notation_output.c
 * @brief Select expression bodies, bindings and native naming scopes for TeX output.
 *
 * Expression owns authored-versus-result semantics, temporary names and the
 * binding envelope. Semantic contexts remain internal to these output APIs.
 * TeX constructs and serialises the selected notation without exposing syntax
 * to this module or requiring applications to configure a rendering context.
 */
#include <stdlib.h>
#include <string.h>

#include "expression-t.h"
#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
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

/* Compose a selected antiderivative definition, consuming the owned body on every path. */
TeX_t *expr_notation_append_primitive(TeX_t *body, const expr_t *initial)
{
    if (!body || !initial)
        return body;
    TeX_context_t *context = expr_notation_primitive_context(initial);
    TeX_t *definition = context ? TeX_from_context(context) : NULL;
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_notation(builder, body);
    TeX_push_notation(builder, definition);
    TeX_sequence(builder, 2);
    TeX_t *result = TeX_builder_finish(builder);
    TeX_builder_free(builder);
    TeX_free(definition);
    TeX_context_free(context);
    TeX_free(body);
    return result;
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

static int expr_to_TeX_parts_mode(const expr_t *expression, char **body_out, char **bindings_out,
                                  bool preserve_root_symbol)
{
    expr_init_singletons();
    if (!body_out || !bindings_out)
        return -1;
    *body_out = NULL;
    *bindings_out = NULL;
    expr_t *initial = NULL;
    expr_t *display = expression ? expr_notation_primitive_display(expression, &initial) : NULL;
    TeX_context_t *surface = !display ? expr_notation_surface_context(expression, preserve_root_symbol) : NULL;
    if (surface || !expression) {
        TeX_t *notation = surface ? TeX_from_context(surface) : NULL;
        if (!surface) {
            TeX_builder_t *builder = TeX_builder_new();
            TeX_push_raw(builder, "NULL");
            notation = TeX_builder_finish(builder);
            TeX_builder_free(builder);
        }
        *body_out = notation ? strdup(TeX_source(notation)) : NULL;
        *bindings_out = strdup("");
        TeX_free(notation);
        TeX_context_free(surface);
    } else {
        autoname_table_t names;
        autoname_init(&names);
        assign_unnamed_vars_dfs((expr_t *)expression, &names);
        TeX_context_t *context = expr_TeX_result_context(display ? display : expression);
        TeX_t *body = context ? TeX_from_context(context) : NULL;
        TeX_context_free(context);
        body = expr_notation_append_primitive(body, initial);
        TeX_context_t *binding_context = NULL;
        bool ready = body && expr_notation_bindings_context(expression, &binding_context);
        TeX_t *bindings = binding_context ? TeX_from_context(binding_context) : NULL;
        if (ready && (!binding_context || bindings)) {
            *body_out = strdup(TeX_source(body));
            *bindings_out = strdup(bindings ? TeX_source(bindings) : "");
        }
        TeX_free(bindings);
        TeX_context_free(binding_context);
        TeX_free(body);
        autoname_restore(&names);
    }
    expr_free(display);
    expr_free(initial);
    if (!*body_out || !*bindings_out) {
        free(*body_out);
        free(*bindings_out);
        *body_out = NULL;
        *bindings_out = NULL;
        return -1;
    }
    return 0;
}

/* Select the ordinary expression body and its independently owned binding notation. */
int expr_to_TeX_parts(const expr_t *expression, char **body_out, char **bindings_out)
{
    return expr_to_TeX_parts_mode(expression, body_out, bindings_out, false);
}

/* Preserve a bound root variable when another mathematical object requests its notation. */
int expr_to_TeX_symbolic_parts(const expr_t *expression, char **body_out, char **bindings_out)
{
    return expr_to_TeX_parts_mode(expression, body_out, bindings_out, true);
}

/* Preserve the native output entry point while delegating envelope syntax to TeX. */
string_t *expr_to_TeX_text(const expr_t *expression)
{
    char *body_text = NULL, *binding_text = NULL;
    if (expr_to_TeX_parts(expression, &body_text, &binding_text) != 0)
        return NULL;
    bool bound = binding_text && *binding_text;
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_raw(builder, body_text);
    if (bound) {
        TeX_push_raw(builder, binding_text);
        TeX_bound(builder, true);
    }
    TeX_t *notation = TeX_builder_finish(builder);
    string_t *result = notation ? TeX_render(notation) : NULL;
    TeX_free(notation);
    TeX_builder_free(builder);
    free(binding_text);
    free(body_text);
    return result;
}
