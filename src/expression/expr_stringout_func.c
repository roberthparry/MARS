#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

static void emit_name_c(sbuf_t *b, const char *name)
{
    emit_name_func(b, name);
}

static void emit_assignment_value(sbuf_t *b, char *value)
{
    static const struct {
        const char *symbol;
        const char *alias;
    } aliases[] = {
        {"π", "@pi"},
        {"φ", "@phi"},
        {"γ", "@gamma"},
        {"τ", "@tau"},
    };
    char *cursor = value;

    if (!cursor) {
        sbuf_puts(b, "NAN");
        return;
    }

    while (*cursor) {
        char *match = NULL;
        size_t alias_index = 0u;

        for (size_t i = 0u; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
            char *candidate = strstr(cursor, aliases[i].symbol);

            if (candidate && (!match || candidate < match)) {
                match = candidate;
                alias_index = i;
            }
        }
        if (!match) {
            sbuf_puts(b, cursor);
            break;
        }

        *match = '\0';
        sbuf_puts(b, cursor);
        *match = aliases[alias_index].symbol[0];
        sbuf_puts(b, aliases[alias_index].alias);
        cursor = match + strlen(aliases[alias_index].symbol);
    }
}

static void emit_function_output_arg_list(sbuf_t *b, const varlist_t *vl, const varlist_t *cl)
{
    for (size_t i = 0u; i < vl->count; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        emit_name_c(b, expr_name_or_default(vl->vars[i], "x"));
    }
    for (size_t i = 0u; i < cl->count; ++i) {
        if (vl->count > 0u || i > 0u)
            sbuf_puts(b, ", ");
        emit_name_c(b, cl->vars[i]->name);
    }
}

static bool binding_is_array(const expr_t *binding)
{
    return binding && expr_binding_expr_is_array(binding->binding_expr);
}

static void emit_bindings(sbuf_t *b, const varlist_t *bindings, bool constants)
{
    for (size_t i = 0u; i < bindings->count; ++i) {
        const expr_t *binding = bindings->vars[i];
        char *value = binding_rhs_c_string_local(binding);
        bool unknown = !value || strcmp(value, "NAN") == 0;

        if (constants) {
            if (binding_is_array(binding))
                sbuf_puts(b, "array const ");
            else
                sbuf_puts(b, "const ");
        }
        emit_name_c(b, expr_name_or_default(binding, "x"));
        sbuf_puts(b, " = ");
        if (unknown)
            sbuf_putc(b, '?');
        else
            emit_assignment_value(b, value);
        sbuf_puts(b, ".\n");
        free(value);
    }
}

static void emit_function_param_list(sbuf_t *b, const varlist_t *vl, const varlist_t *cl)
{
    if (vl->count == 0u && cl->count == 0u)
        return;

    for (size_t i = 0u; i < vl->count; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        if (binding_is_array(vl->vars[i]))
            sbuf_puts(b, "array ");
        emit_name_c(b, expr_name_or_default(vl->vars[i], "x"));
    }
    for (size_t i = 0u; i < cl->count; ++i) {
        if (vl->count > 0u || i > 0u)
            sbuf_puts(b, ", ");
        sbuf_puts(b, binding_is_array(cl->vars[i]) ? "array const " : "const ");
        emit_name_c(b, cl->vars[i]->name);
    }
}

typedef struct {
    const expr_t **nodes;
    char **names;
    bool *constants;
    size_t count;
    size_t capacity;
} function_temporary_table_t;

typedef struct {
    const expr_t **nodes;
    size_t *incoming;
    size_t count;
    size_t capacity;
} function_dag_table_t;

static void function_temporary_table_init(function_temporary_table_t *table)
{
    memset(table, 0, sizeof(*table));
}

static void function_temporary_table_free(function_temporary_table_t *table)
{
    if (!table)
        return;
    for (size_t index = 0u; index < table->count; ++index)
        free(table->names[index]);
    free(table->constants);
    free(table->names);
    free(table->nodes);
    function_temporary_table_init(table);
}

static void function_dag_table_init(function_dag_table_t *table)
{
    memset(table, 0, sizeof(*table));
}

static void function_dag_table_free(function_dag_table_t *table)
{
    if (!table)
        return;
    free(table->incoming);
    free(table->nodes);
    function_dag_table_init(table);
}

static size_t function_dag_table_find(const function_dag_table_t *table, const expr_t *node)
{
    for (size_t index = 0u; index < table->count; ++index) {
        if (table->nodes[index] == node || expr_struct_eq(table->nodes[index], node))
            return index;
    }
    return (size_t)-1;
}

static size_t function_dag_table_register(function_dag_table_t *table, const expr_t *node)
{
    size_t found;
    size_t index;

    if (!node)
        return (size_t)-1;
    found = function_dag_table_find(table, node);
    if (found != (size_t)-1)
        return found;

    if (table->count == table->capacity) {
        size_t capacity = table->capacity ? table->capacity * 2u : 32u;
        const expr_t **nodes = realloc(table->nodes, capacity * sizeof(*nodes));
        size_t *incoming;

        if (!nodes)
            return (size_t)-1;
        table->nodes = nodes;
        incoming = realloc(table->incoming, capacity * sizeof(*incoming));
        if (!incoming)
            return (size_t)-1;
        table->incoming = incoming;
        table->capacity = capacity;
    }

    index = table->count++;
    table->nodes[index] = node;
    table->incoming[index] = 0u;
    if (node->a) {
        size_t child = function_dag_table_register(table, node->a);

        if (child != (size_t)-1)
            table->incoming[child]++;
    }
    if (node->b) {
        size_t child = function_dag_table_register(table, node->b);

        if (child != (size_t)-1)
            table->incoming[child]++;
    }
    return index;
}

static bool function_temporary_table_contains(const function_temporary_table_t *table, const expr_t *node)
{
    for (size_t index = 0u; index < table->count; ++index) {
        if (table->nodes[index] == node || expr_struct_eq(table->nodes[index], node))
            return true;
    }
    return false;
}

static bool function_node_is_constant(const expr_t *node)
{
    if (!node)
        return true;
    if (expr_is_var(node))
        return false;
    if (expr_is_const(node))
        return true;
    return function_node_is_constant(node->a) && function_node_is_constant(node->b);
}

static bool function_temporary_name_is_available(const char *name, const varlist_t *variables,
                                                 const varlist_t *constants)
{
    for (size_t index = 0u; index < variables->count; ++index) {
        if (strcmp(name, expr_name_or_default(variables->vars[index], "x")) == 0)
            return false;
    }
    for (size_t index = 0u; index < constants->count; ++index) {
        if (strcmp(name, expr_name_or_default(constants->vars[index], "x")) == 0)
            return false;
    }
    return true;
}

static bool function_temporary_table_name_is_available(const function_temporary_table_t *table, const char *name,
                                                        const varlist_t *variables, const varlist_t *constants)
{
    if (!function_temporary_name_is_available(name, variables, constants))
        return false;
    for (size_t index = 0u; index < table->count; ++index) {
        if (strcmp(name, table->names[index]) == 0)
            return false;
    }
    return true;
}

static bool function_temporary_has_simple_constant_label(const expr_t *node)
{
    if (!node || !function_node_is_constant(node))
        return false;
    if (node->ops->arity == EXPR_OP_UNARY)
        return node->a && (expr_is_const(node->a) || expr_is_var(node->a));
    if (expr_is_op(node, &ops_root))
        return node->a && node->b && (expr_is_const(node->a) || expr_is_var(node->a)) &&
               (expr_is_const(node->b) || expr_is_var(node->b));
    return false;
}

static char *function_temporary_semantic_name_dup(const expr_t *node)
{
    sbuf_t rendered;
    const char *rendered_text;
    char *name;
    size_t length;
    size_t compact_length = 0u;
    size_t output_index = 2u;

    if (!function_temporary_has_simple_constant_label(node))
        return NULL;

    sbuf_init(&rendered);
    emit_func(node, &rendered, PREC_LOWEST);
    length = sbuf_len(&rendered);
    rendered_text = sbuf_c_str(&rendered);
    for (size_t index = 0u; index < length; ++index) {
        if (rendered_text[index] != ' ')
            ++compact_length;
    }
    if (compact_length == 0u || compact_length > 20u) {
        sbuf_free(&rendered);
        return NULL;
    }

    name = malloc(compact_length + strlen("$[]") + 1u);
    if (name) {
        name[0] = '$';
        name[1] = '[';
        for (size_t index = 0u; index < length; ++index) {
            if (rendered_text[index] != ' ')
                name[output_index++] = rendered_text[index];
        }
        name[output_index++] = ']';
        name[output_index] = '\0';
    }
    sbuf_free(&rendered);
    return name;
}

static bool function_temporary_table_append(function_temporary_table_t *table, const expr_t *node,
                                            const varlist_t *variables, const varlist_t *constants)
{
    char candidate[48];
    char *semantic_name;
    bool is_constant = function_node_is_constant(node);
    size_t ordinal = 1u;

    if (table->count == table->capacity) {
        size_t capacity = table->capacity ? table->capacity * 2u : 16u;
        const expr_t **nodes = realloc(table->nodes, capacity * sizeof(*nodes));
        char **names;
        bool *constant_flags;

        if (!nodes)
            return false;
        table->nodes = nodes;
        names = realloc(table->names, capacity * sizeof(*names));
        if (!names)
            return false;
        table->names = names;
        constant_flags = realloc(table->constants, capacity * sizeof(*constant_flags));
        if (!constant_flags)
            return false;
        table->constants = constant_flags;
        table->capacity = capacity;
    }

    semantic_name = function_temporary_semantic_name_dup(node);
    if (semantic_name && !function_temporary_table_name_is_available(table, semantic_name, variables, constants)) {
        free(semantic_name);
        semantic_name = NULL;
    }
    if (!semantic_name) {
        do {
            snprintf(candidate, sizeof(candidate), "%c%zu", is_constant ? 'c' : 'v', ordinal++);
        } while (!function_temporary_table_name_is_available(table, candidate, variables, constants));
        semantic_name = expr_tostring_xstrdup(candidate);
    }

    table->names[table->count] = semantic_name;
    if (!table->names[table->count])
        return false;
    table->nodes[table->count] = node;
    table->constants[table->count] = is_constant;
    table->count++;
    return true;
}

static bool function_collect_shared_temporaries(const expr_t *node, const expr_t *root,
                                                const function_dag_table_t *dag,
                                                function_temporary_table_t *temporaries,
                                                const varlist_t *variables, const varlist_t *constants)
{
    size_t dag_index;

    if (!node || expr_is_const(node) || expr_is_var(node) || function_temporary_table_contains(temporaries, node))
        return true;
    if (!function_collect_shared_temporaries(node->a, root, dag, temporaries, variables, constants) ||
        !function_collect_shared_temporaries(node->b, root, dag, temporaries, variables, constants))
        return false;
    if (node == root)
        return true;
    dag_index = function_dag_table_find(dag, node);
    if (dag_index == (size_t)-1 || dag->incoming[dag_index] < 2u)
        return true;
    if (node->ops->arity == EXPR_OP_UNARY && node->a && !expr_is_const(node->a) && !expr_is_var(node->a) &&
        !function_temporary_table_contains(temporaries, node->a) &&
        !function_temporary_table_append(temporaries, node->a, variables, constants))
        return false;
    return function_temporary_table_append(temporaries, node, variables, constants);
}

static bool function_append_hierarchical_temporary(const expr_t *node, function_temporary_table_t *temporaries,
                                                   const varlist_t *variables, const varlist_t *constants)
{
    sbuf_t rendered;

    if (!node || expr_is_const(node) || expr_is_var(node) || function_temporary_table_contains(temporaries, node))
        return true;

    sbuf_init(&rendered);
    emit_func_with_temporaries(node, &rendered, PREC_LOWEST, temporaries->nodes,
                               (const char *const *)temporaries->names, temporaries->count, node);
    if (sbuf_len(&rendered) + strlen("    const r1 = .") > 130u) {
        if (!function_append_hierarchical_temporary(node->a, temporaries, variables, constants) ||
            !function_append_hierarchical_temporary(node->b, temporaries, variables, constants)) {
            sbuf_free(&rendered);
            return false;
        }
    }
    sbuf_free(&rendered);
    return function_temporary_table_append(temporaries, node, variables, constants);
}

static void emit_function_body(sbuf_t *b, const expr_t *root, const varlist_t *variables, const varlist_t *constants)
{
    function_dag_table_t dag;
    function_temporary_table_t temporaries;
    sbuf_t direct;

    function_dag_table_init(&dag);
    function_temporary_table_init(&temporaries);

    if (function_dag_table_register(&dag, root) == (size_t)-1 ||
        !function_collect_shared_temporaries(root, root, &dag, &temporaries, variables, constants)) {
        function_dag_table_free(&dag);
        function_temporary_table_free(&temporaries);
        sbuf_puts(b, "    return ");
        emit_func(root, b, PREC_LOWEST);
        sbuf_puts(b, ".\n");
        return;
    }

    sbuf_init(&direct);
    emit_func_with_temporaries(root, &direct, PREC_LOWEST, temporaries.nodes,
                               (const char *const *)temporaries.names, temporaries.count, root);
    if (sbuf_len(&direct) + strlen("    return .") > 130u &&
        (!function_append_hierarchical_temporary(root->a, &temporaries, variables, constants) ||
         !function_append_hierarchical_temporary(root->b, &temporaries, variables, constants))) {
        sbuf_free(&direct);
        function_dag_table_free(&dag);
        function_temporary_table_free(&temporaries);
        sbuf_puts(b, "    return ");
        emit_func(root, b, PREC_LOWEST);
        sbuf_puts(b, ".\n");
        return;
    }
    sbuf_free(&direct);

    if (temporaries.count == 0u) {
        function_dag_table_free(&dag);
        function_temporary_table_free(&temporaries);
        sbuf_puts(b, "    return ");
        emit_func(root, b, PREC_LOWEST);
        sbuf_puts(b, ".\n");
        return;
    }

    for (size_t index = 0u; index < temporaries.count; ++index) {
        sbuf_puts(b, "    ");
        if (temporaries.constants[index])
            sbuf_puts(b, "const ");
        sbuf_puts(b, temporaries.names[index]);
        sbuf_puts(b, " = ");
        emit_func_with_temporaries(temporaries.nodes[index], b, PREC_LOWEST, temporaries.nodes,
                                   (const char *const *)temporaries.names, temporaries.count,
                                   temporaries.nodes[index]);
        sbuf_puts(b, ".\n");
    }
    sbuf_putc(b, '\n');
    sbuf_puts(b, "    return ");
    emit_func_with_temporaries(root, b, PREC_LOWEST, temporaries.nodes, (const char *const *)temporaries.names,
                               temporaries.count, root);
    sbuf_puts(b, ".\n");
    function_dag_table_free(&dag);
    function_temporary_table_free(&temporaries);
}

string_t *expr_to_text_function(const expr_t *f)
{
    sbuf_t b;
    autoname_table_t vnames;
    varlist_t vl;
    varlist_t cl;
    const expr_t *g = f;
    const char *fname = "expr";
    string_t *out;

    sbuf_init(&b);

    if (f && f->binding_expr && !expr_is_const(f) && !expr_binding_expr_is_array(f->binding_expr)) {
        char *rhs = expr_binding_expr_to_function_string(f->binding_expr);

        if (rhs) {
            sbuf_puts(&b, "expression expr() {\n");
            sbuf_puts(&b, "    return ");
            sbuf_puts(&b, rhs);
            sbuf_puts(&b, ".\n");
            sbuf_puts(&b, "}\n\n");
            sbuf_puts(&b, "output(expr()).");
            free(rhs);

            out = sbuf_to_string(&b);
            sbuf_free(&b);
            return out;
        }
    }

    autoname_init(&vnames);
    assign_unnamed_vars_dfs((expr_t *)f, &vnames);

    varlist_init(&vl);
    find_vars_dfs(g, &vl);

    varlist_init(&cl);
    find_explicit_named_consts_dfs(f, &cl);
    find_named_consts_dfs(g, &cl);

    sbuf_puts(&b, "expression ");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    emit_function_param_list(&b, &vl, &cl);
    sbuf_puts(&b, ") {\n");
    emit_function_body(&b, g, &vl, &cl);
    sbuf_puts(&b, "}\n\n");

    emit_bindings(&b, &vl, false);
    emit_bindings(&b, &cl, true);
    sbuf_puts(&b, "output(");
    sbuf_puts(&b, fname);
    sbuf_putc(&b, '(');
    emit_function_output_arg_list(&b, &vl, &cl);
    sbuf_puts(&b, ")).");

    out = sbuf_to_string(&b);
    sbuf_free(&b);
    free(vl.vars);
    free(cl.vars);
    autoname_restore(&vnames);
    return out;
}
