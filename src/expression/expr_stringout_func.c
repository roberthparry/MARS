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

static void emit_assignment_value(sbuf_t *b, const char *value)
{
    string_t *source;
    string_t *translated;
    string_cursor_t *cursor;
    bool failed = false;

    if (!value) {
        sbuf_puts(b, "NAN");
        return;
    }

    source = string_new_with(value);
    translated = string_new();
    cursor = source ? string_cursor_new(source) : NULL;
    if (!source || !translated || !cursor)
        failed = true;

    while (!failed && !string_cursor_done(cursor)) {
        rune_t symbol = string_cursor_peek(cursor);
        const char *alias = expr_greek_symbol_alias(symbol);
        int status = alias ? string_append_cstr(translated, alias) : string_append_rune(translated, symbol);

        if (status != 0)
            failed = true;
        string_cursor_next(cursor);
    }

    emit_func_fragment(b, failed ? value : string_c_str(translated));
    string_cursor_free(cursor);
    string_free(translated);
    string_free(source);
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

struct expr_function_temporaries {
    function_temporary_table_t temporaries;
    autoname_table_t autonames;
    varlist_t variables;
    varlist_t constants;
};

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
    if (found != (size_t)-1) {
        size_t child = function_dag_table_register(table, node->a);

        if (child != (size_t)-1)
            table->incoming[child]++;
        child = function_dag_table_register(table, node->b);
        if (child != (size_t)-1)
            table->incoming[child]++;
        return found;
    }

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

#define FUNCTION_FACTOR_LIMIT 64u

static size_t function_flatten_mul_factors(const expr_t *node, const expr_t **factors, size_t count)
{
    if (!node || count >= FUNCTION_FACTOR_LIMIT)
        return count;
    if (expr_is_mul(node)) {
        count = function_flatten_mul_factors(node->a, factors, count);
        return function_flatten_mul_factors(node->b, factors, count);
    }
    factors[count++] = node;
    return count;
}

static bool function_product_parts(const expr_t *node, const expr_t **numerator, const expr_t **denominator)
{
    if (expr_is_op(node, &ops_div)) {
        *numerator = node->a;
        *denominator = node->b;
        return true;
    }
    if (expr_is_mul(node)) {
        *numerator = node;
        *denominator = NULL;
        return true;
    }
    return false;
}

static bool function_product_factors_are_subset(const expr_t *candidate, const expr_t *container)
{
    const expr_t *candidate_factors[FUNCTION_FACTOR_LIMIT];
    const expr_t *container_factors[FUNCTION_FACTOR_LIMIT];
    bool matched[FUNCTION_FACTOR_LIMIT] = {false};
    const expr_t *candidate_numerator = NULL;
    const expr_t *candidate_denominator = NULL;
    const expr_t *container_numerator = NULL;
    const expr_t *container_denominator = NULL;
    size_t candidate_count;
    size_t container_count;

    if (!function_product_parts(candidate, &candidate_numerator, &candidate_denominator) ||
        !function_product_parts(container, &container_numerator, &container_denominator) ||
        ((candidate_denominator || container_denominator) &&
         (!candidate_denominator || !container_denominator ||
          !expr_struct_eq(candidate_denominator, container_denominator))))
        return false;
    candidate_count = function_flatten_mul_factors(candidate_numerator, candidate_factors, 0u);
    container_count = function_flatten_mul_factors(container_numerator, container_factors, 0u);
    if (candidate_count + (candidate_denominator ? 1u : 0u) < 2u || candidate_count > container_count)
        return false;

    for (size_t candidate_index = 0u; candidate_index < candidate_count; ++candidate_index) {
        bool found = false;

        for (size_t container_index = 0u; container_index < container_count; ++container_index) {
            if (!matched[container_index] &&
                (candidate_factors[candidate_index] == container_factors[container_index] ||
                 expr_struct_eq(candidate_factors[candidate_index], container_factors[container_index]))) {
                matched[container_index] = true;
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

static size_t function_count_factored_occurrences(const expr_t *node, const expr_t *candidate,
                                                  bool parent_is_product)
{
    const bool node_is_product = expr_is_mul(node) || expr_is_op(node, &ops_div);
    size_t count = 0u;

    if (!node)
        return 0u;
    if (node_is_product && !parent_is_product && function_product_factors_are_subset(candidate, node))
        count++;
    count += function_count_factored_occurrences(node->a, candidate, node_is_product);
    count += function_count_factored_occurrences(node->b, candidate, expr_is_mul(node));
    return count;
}

static void function_dag_table_register_factored_occurrences(function_dag_table_t *table,
                                                             const expr_t *const *roots, size_t root_count)
{
    for (size_t candidate_index = 0u; candidate_index < table->count; ++candidate_index) {
        size_t occurrences = 0u;

        if (!expr_is_mul(table->nodes[candidate_index]) && !expr_is_op(table->nodes[candidate_index], &ops_div))
            continue;
        for (size_t root_index = 0u; root_index < root_count; ++root_index)
            occurrences += function_count_factored_occurrences(roots[root_index], table->nodes[candidate_index],
                                                               false);
        if (occurrences > table->incoming[candidate_index])
            table->incoming[candidate_index] = occurrences;
    }
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

static bool function_temporary_table_append(function_temporary_table_t *table, const expr_t *node,
                                            const varlist_t *variables, const varlist_t *constants)
{
    char candidate[48];
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

    do {
        snprintf(candidate, sizeof(candidate), "%c%zu", is_constant ? 'c' : 'v', ordinal++);
    } while (!function_temporary_table_name_is_available(table, candidate, variables, constants));

    table->names[table->count] = expr_tostring_xstrdup(candidate);
    if (!table->names[table->count])
        return false;
    table->nodes[table->count] = node;
    table->constants[table->count] = is_constant;
    table->count++;
    return true;
}

static void function_temporary_table_group_constants(function_temporary_table_t *table)
{
    size_t constant_count = 0u;

    if (!table)
        return;

    for (size_t index = 0u; index < table->count; ++index) {
        const expr_t *node;
        char *name;

        if (!table->constants[index])
            continue;

        node = table->nodes[index];
        name = table->names[index];
        for (size_t move = index; move > constant_count; --move) {
            table->nodes[move] = table->nodes[move - 1u];
            table->names[move] = table->names[move - 1u];
            table->constants[move] = table->constants[move - 1u];
        }
        table->nodes[constant_count] = node;
        table->names[constant_count] = name;
        table->constants[constant_count] = true;
        constant_count++;
    }
}

static const char *function_temporary_reciprocal_exp_name(const function_temporary_table_t *table, size_t index)
{
    const expr_t *node;
    const expr_t *argument;
    const expr_t *opposite;
    bool argument_is_negative;

    if (!table || index >= table->count || !(node = table->nodes[index]) ||
        !expr_is_op(node, &ops_exp) || !(argument = node->a))
        return NULL;
    argument_is_negative = expr_is_op(argument, &ops_neg) && argument->a;
    opposite = argument_is_negative ? argument->a : argument;
    for (size_t candidate = 0u; candidate < index; ++candidate) {
        const expr_t *prior = table->nodes[candidate];
        const expr_t *prior_argument;

        if (!prior || !expr_is_op(prior, &ops_exp) || !(prior_argument = prior->a))
            continue;
        if (argument_is_negative) {
            if (expr_struct_eq(prior_argument, opposite))
                return table->names[candidate];
        } else if (expr_is_op(prior_argument, &ops_neg) && prior_argument->a &&
                   expr_struct_eq(prior_argument->a, opposite)) {
            return table->names[candidate];
        }
    }
    return NULL;
}

static void emit_function_temporary_value(sbuf_t *buffer, const function_temporary_table_t *table, size_t index)
{
    const char *reciprocal_exp = function_temporary_reciprocal_exp_name(table, index);

    if (reciprocal_exp) {
        sbuf_puts(buffer, "1/");
        sbuf_puts(buffer, reciprocal_exp);
        return;
    }
    emit_func_with_temporaries(table->nodes[index], buffer, PREC_LOWEST, table->nodes,
                               (const char *const *)table->names, table->count, table->nodes[index]);
}

static bool function_collect_shared_temporaries(const expr_t *node, const expr_t *root,
                                                const function_dag_table_t *dag,
                                                function_temporary_table_t *temporaries,
                                                const varlist_t *variables, const varlist_t *constants)
{
    const expr_t *first_child;
    size_t dag_index;

    if (!node || expr_is_const(node) || expr_is_var(node) || function_temporary_table_contains(temporaries, node))
        return true;
    dag_index = function_dag_table_find(dag, node);
    if (node != root && dag_index != (size_t)-1 && dag->incoming[dag_index] >= 2u &&
        expr_is_op(node, &ops_neg)) {
        if (node->a && (expr_is_const(node->a) || expr_is_var(node->a)))
            return true;
        return function_temporary_table_append(temporaries, node, variables, constants);
    }
    first_child = expr_is_op(node, &ops_exp) && expr_is_op(node->a, &ops_neg) && node->a->a ? node->a->a : node->a;
    if (!function_collect_shared_temporaries(first_child, root, dag, temporaries, variables, constants) ||
        !function_collect_shared_temporaries(node->b, root, dag, temporaries, variables, constants))
        return false;
    if (node == root)
        return true;
    if (dag_index == (size_t)-1 || dag->incoming[dag_index] < 2u)
        return true;
    /* Packing nodes are an internal representation detail of variadic
     * hypergeometric functions, not expressions in the MARS language. */
    if (expr_is_op(node, &ops_hypergeometric_pFq_pack))
        return true;
    if (node->ops->arity == EXPR_OP_UNARY && node->a && !expr_is_const(node->a) && !expr_is_var(node->a) &&
        !expr_is_op(node->a, &ops_neg) &&
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
    if (sbuf_len(&rendered) + strlen("    const r1 = .") > 120u) {
        if (!function_append_hierarchical_temporary(node->a, temporaries, variables, constants) ||
            !function_append_hierarchical_temporary(node->b, temporaries, variables, constants)) {
            sbuf_free(&rendered);
            return false;
        }
    }
    sbuf_free(&rendered);
    return function_temporary_table_append(temporaries, node, variables, constants);
}

static bool function_append_additive_term_temporaries(const expr_t *node, function_temporary_table_t *temporaries,
                                                      const varlist_t *variables, const varlist_t *constants)
{
    if (!node || expr_is_const(node) || expr_is_var(node))
        return true;
    if (expr_is_op(node, &ops_add) || expr_is_op(node, &ops_sub))
        return function_append_additive_term_temporaries(node->a, temporaries, variables, constants) &&
               function_append_additive_term_temporaries(node->b, temporaries, variables, constants);
    if (expr_is_op(node, &ops_neg) && node->a)
        node = node->a;
    return function_temporary_table_append(temporaries, node, variables, constants);
}

static bool function_append_readable_temporaries(const expr_t *node, function_temporary_table_t *temporaries,
                                                 const varlist_t *variables, const varlist_t *constants)
{
    if (node && (expr_is_op(node, &ops_add) || expr_is_op(node, &ops_sub)))
        return function_append_additive_term_temporaries(node, temporaries, variables, constants);
    if (node && expr_is_mul(node) && node->a && node->b) {
        if (expr_is_op(node->a, &ops_add) || expr_is_op(node->a, &ops_sub))
            return function_append_additive_term_temporaries(node->a, temporaries, variables, constants) &&
                   function_append_hierarchical_temporary(node->b, temporaries, variables, constants);
        if (expr_is_op(node->b, &ops_add) || expr_is_op(node->b, &ops_sub))
            return function_append_hierarchical_temporary(node->a, temporaries, variables, constants) &&
                   function_append_additive_term_temporaries(node->b, temporaries, variables, constants);
    }
    return function_append_hierarchical_temporary(node, temporaries, variables, constants);
}

static bool function_append_root_readable_temporaries(const expr_t *root, function_temporary_table_t *temporaries,
                                                      const varlist_t *variables, const varlist_t *constants)
{
    if (root && (expr_is_op(root, &ops_add) || expr_is_op(root, &ops_sub)))
        return function_append_additive_term_temporaries(root, temporaries, variables, constants);
    return root && function_append_readable_temporaries(root->a, temporaries, variables, constants) &&
           function_append_readable_temporaries(root->b, temporaries, variables, constants);
}

static void emit_function_body(sbuf_t *b, const expr_t *root, const varlist_t *variables, const varlist_t *constants)
{
    function_dag_table_t dag;
    function_temporary_table_t temporaries;
    sbuf_t direct;

    function_dag_table_init(&dag);
    function_temporary_table_init(&temporaries);

    if (function_dag_table_register(&dag, root) == (size_t)-1) {
        function_dag_table_free(&dag);
        function_temporary_table_free(&temporaries);
        sbuf_puts(b, "    return ");
        emit_func(root, b, PREC_LOWEST);
        sbuf_puts(b, ".\n");
        return;
    }
    function_dag_table_register_factored_occurrences(&dag, &root, 1u);
    if (!function_collect_shared_temporaries(root, root, &dag, &temporaries, variables, constants)) {
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
    if (sbuf_len(&direct) + strlen("    return .") > 105u &&
        !function_append_root_readable_temporaries(root, &temporaries, variables, constants)) {
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

    function_temporary_table_group_constants(&temporaries);
    for (size_t index = 0u; index < temporaries.count; ++index) {
        if (index > 0u && temporaries.constants[index - 1u] && !temporaries.constants[index])
            sbuf_putc(b, '\n');
        sbuf_puts(b, "    ");
        if (temporaries.constants[index])
            sbuf_puts(b, "const ");
        sbuf_puts(b, temporaries.names[index]);
        sbuf_puts(b, " = ");
        emit_function_temporary_value(b, &temporaries, index);
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

/* Build one temporary plan shared by several function expression roots. */
expr_function_temporaries_t *expr_function_temporaries_new(const expr_t *const *roots, size_t count)
{
    expr_function_temporaries_t *plan = calloc(1u, sizeof(*plan));
    function_dag_table_t dag;
    bool ok = plan != NULL;

    if (!plan)
        return NULL;

    function_temporary_table_init(&plan->temporaries);
    autoname_init(&plan->autonames);
    varlist_init(&plan->variables);
    varlist_init(&plan->constants);
    function_dag_table_init(&dag);

    for (size_t index = 0u; ok && index < count; ++index) {
        if (!roots[index])
            continue;
        assign_unnamed_vars_dfs((expr_t *)roots[index], &plan->autonames);
        find_vars_dfs(roots[index], &plan->variables);
        find_explicit_named_consts_dfs(roots[index], &plan->constants);
        find_named_consts_dfs(roots[index], &plan->constants);
        ok = function_dag_table_register(&dag, roots[index]) != (size_t)-1;
    }

    if (ok)
        function_dag_table_register_factored_occurrences(&dag, roots, count);

    for (size_t index = 0u; ok && index < count; ++index) {
        if (roots[index])
            ok = function_collect_shared_temporaries(roots[index], roots[index], &dag, &plan->temporaries,
                                                     &plan->variables, &plan->constants);
    }

    for (size_t index = 0u; ok && index < count; ++index) {
        size_t occurrences = 0u;

        if (!roots[index] || expr_is_const(roots[index]) || expr_is_var(roots[index]) ||
            function_temporary_table_contains(&plan->temporaries, roots[index]))
            continue;
        for (size_t candidate = 0u; candidate < count; ++candidate) {
            if (roots[candidate] &&
                (roots[candidate] == roots[index] || expr_struct_eq(roots[candidate], roots[index])))
                occurrences++;
        }
        if (occurrences > 1u)
            ok = function_temporary_table_append(&plan->temporaries, roots[index], &plan->variables,
                                                 &plan->constants);
    }

    for (size_t index = 0u; ok && index < count; ++index) {
        sbuf_t direct;

        if (!roots[index])
            continue;
        sbuf_init(&direct);
        emit_func_with_temporaries(roots[index], &direct, PREC_LOWEST, plan->temporaries.nodes,
                                   (const char *const *)plan->temporaries.names, plan->temporaries.count,
                                   roots[index]);
        if (sbuf_len(&direct) + strlen("    return .") > 130u)
            ok = function_append_hierarchical_temporary(roots[index]->a, &plan->temporaries, &plan->variables,
                                                        &plan->constants) &&
                 function_append_hierarchical_temporary(roots[index]->b, &plan->temporaries, &plan->variables,
                                                        &plan->constants);
        sbuf_free(&direct);
    }

    function_dag_table_free(&dag);
    if (!ok) {
        expr_function_temporaries_free(plan);
        return NULL;
    }
    function_temporary_table_group_constants(&plan->temporaries);
    return plan;
}

/* Render declarations from a shared function-temporary plan. */
string_t *expr_function_temporaries_declarations_text(const expr_function_temporaries_t *plan)
{
    sbuf_t buffer;
    string_t *text;

    if (!plan)
        return NULL;
    sbuf_init(&buffer);
    for (size_t index = 0u; index < plan->temporaries.count; ++index) {
        if (index > 0u && plan->temporaries.constants[index - 1u] && !plan->temporaries.constants[index])
            sbuf_putc(&buffer, '\n');
        sbuf_puts(&buffer, "    ");
        if (plan->temporaries.constants[index])
            sbuf_puts(&buffer, "const ");
        sbuf_puts(&buffer, plan->temporaries.names[index]);
        sbuf_puts(&buffer, " = ");
        emit_function_temporary_value(&buffer, &plan->temporaries, index);
        sbuf_puts(&buffer, ".\n");
    }
    if (plan->temporaries.count > 0u)
        sbuf_putc(&buffer, '\n');
    text = sbuf_to_string(&buffer);
    sbuf_free(&buffer);
    return text;
}

/* Render one expression using a shared function-temporary plan. */
string_t *expr_function_temporaries_expression_text(const expr_function_temporaries_t *plan, const expr_t *expr)
{
    sbuf_t buffer;
    string_t *text;

    if (!plan || !expr)
        return NULL;
    sbuf_init(&buffer);
    emit_func_with_temporaries(expr, &buffer, PREC_LOWEST, plan->temporaries.nodes,
                               (const char *const *)plan->temporaries.names, plan->temporaries.count, NULL);
    text = sbuf_to_string(&buffer);
    sbuf_free(&buffer);
    return text;
}

/* Release a shared function-temporary plan. */
void expr_function_temporaries_free(expr_function_temporaries_t *plan)
{
    if (!plan)
        return;
    function_temporary_table_free(&plan->temporaries);
    free(plan->variables.vars);
    free(plan->constants.vars);
    autoname_restore(&plan->autonames);
    free(plan);
}

/* Serialise one expression body using the notation accepted in a MARS function. */
string_t *expr_to_function_body_text(const expr_t *expr)
{
    sbuf_t buffer;
    autoname_table_t names;
    string_t *text;

    if (!expr)
        return string_new_with("NULL");

    sbuf_init(&buffer);
    autoname_init(&names);
    assign_unnamed_vars_dfs((expr_t *)expr, &names);
    emit_func(expr, &buffer, PREC_LOWEST);
    text = sbuf_to_string(&buffer);
    autoname_restore(&names);
    sbuf_free(&buffer);
    return text;
}

/* Serialise one expression body to an owned C string in MARS function notation. */
char *expr_to_function_body(const expr_t *expr)
{
    string_t *text = expr_to_function_body_text(expr);
    char *out = text ? expr_tostring_xstrdup(string_c_str(text)) : NULL;

    string_free(text);
    return out;
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
            emit_func_fragment(&b, rhs);
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
