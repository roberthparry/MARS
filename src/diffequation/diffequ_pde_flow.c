#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

/* Dependency bookkeeping is quadratic only in this explicitly bounded number of coordinates. */
enum { DE_FLOW_MAX_COORDINATES = 32 };

typedef struct {
    expr_t *slope;
    expr_t *offset;
    expr_t *parameter;
    expr_t *flow;
    expr_t *invariant;
    size_t dependencies;
} de_flow_node_t;

/* Named parameters survive integration-tree copies; disjoint suffix sequences prevent mutual collisions. */
static expr_t *de_flow_parameter(const diffequ_t *de, const expr_t *dependent, size_t index)
{
    for (size_t serial = index + 1u; ; serial += de->independent_count) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "K%zu", serial);
        expr_t *parameter = expr_new_named_var(NUM_NAN, buffer);
        if (!parameter)
            return NULL;
        const char *name = expr_symbol_name(parameter), *field = expr_symbol_name(dependent);
        bool used = de_constant(de, name) || (field && strcmp(name, field) == 0);
        for (size_t j = 0u; !used && j < de->independent_count; ++j) {
            const char *coordinate = expr_symbol_name(de->independent_vars[j]);
            used = coordinate && strcmp(name, coordinate) == 0;
        }
        if (!used)
            return parameter;
        expr_free(parameter);
    }
}

static bool de_flow_time_only(const expr_t *expr, const diffequ_t *de, size_t clock, const expr_t *dependent)
{
    if (!expr || de_expr_uses(expr, dependent))
        return false;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (i != clock && de_expr_uses(expr, de->independent_vars[i]))
            return false;
    if (de_expr_uses(expr, de->independent_vars[clock]))
        return true;
    number_t value = expr_eval(expr);
    bool finite = num_is_finite(value);
    num_destroy(&value);
    return finite;
}

/* Resolve one linear scalar characteristic using the already constructed upstream flows. */
static bool de_flow_resolve(de_flow_node_t *nodes, const bool *edges, size_t index, size_t clock,
                            const diffequ_t *de, const expr_t *scale, const expr_t *dependent)
{
    const size_t n = de->independent_count;
    const expr_t *time = de->independent_vars[clock];
    de_flow_node_t *node = &nodes[index];
    expr_t *drift = expr_clone(node->offset);
    for (size_t j = 0u; drift && j < n; ++j) {
        if (!edges[index * n + j])
            continue;
        expr_t *next = nodes[j].flow ? expr_substitute(drift, de->independent_vars[j], nodes[j].flow) : NULL;
        expr_free(drift);
        drift = next;
    }
    expr_t *rate = expr_div_simplify_owned(expr_clone(node->slope), expr_clone(scale));
    expr_t *primitive = rate ? expr_integrate(rate, time) : NULL;
    expr_t *negative = primitive ? expr_neg(primitive) : NULL;
    expr_t *decay = negative ? expr_simplify_owned(expr_exp(negative)) : NULL;
    expr_t *growth = primitive ? expr_simplify_owned(expr_exp(primitive)) : NULL;
    expr_t *normalised = drift ? expr_div_simplify_owned(expr_clone(drift), expr_clone(scale)) : NULL;
    expr_t *weighted = decay && normalised
        ? expr_mul_simplify_owned(expr_clone(decay), expr_clone(normalised)) : NULL;
    expr_t *shift = weighted ? expr_integrate(weighted, time) : NULL;
    node->parameter = de_flow_parameter(de, dependent, index);
    if (decay && growth && shift && node->parameter && !expr_is_exact_zero(decay) && !expr_is_exact_zero(growth)) {
        node->flow = expr_mul_simplify_owned(expr_clone(growth),
                                             expr_add_simplify_owned(expr_clone(node->parameter), expr_clone(shift)));
        expr_t *resolved_shift = expr_clone(shift);
        /* Each upstream invariant is already free of auxiliary parameters, so substitution cannot capture them. */
        for (size_t j = 0u; resolved_shift && j < n; ++j) {
            if (j == index || !nodes[j].invariant)
                continue;
            expr_t *next = expr_substitute(resolved_shift, nodes[j].parameter, nodes[j].invariant);
            expr_free(resolved_shift);
            resolved_shift = next;
        }
        node->invariant = resolved_shift
            ? expr_sub_simplify_owned(expr_mul(decay, de->independent_vars[index]), resolved_shift) : NULL;
    }
    expr_free(shift); expr_free(weighted); expr_free(normalised);
    expr_free(growth); expr_free(decay); expr_free(negative); expr_free(primitive); expr_free(rate); expr_free(drift);
    return node->flow && node->invariant;
}

/* Construct triangular characteristic invariants in dependency order, returning them in input coordinate order. */
bool de_pde_affine_flow_invariants(const diffequ_t *de, const expr_t *dependent, size_t clock,
                                  expr_t *const *coefficients, expr_t **invariants)
{
    size_t n = de->independent_count;
    if (n < 2u || n > DE_FLOW_MAX_COORDINATES || clock >= n)
        return false;
    de_flow_node_t *nodes = calloc(n, sizeof(*nodes));
    bool *edges = calloc(n * n, sizeof(*edges));
    size_t *queue = calloc(n, sizeof(*queue));
    size_t head = 0u, tail = 0u;
    bool valid = false;
    if (!nodes || !edges || !queue)
        goto cleanup;
    for (size_t i = 0u; i < n; ++i) {
        if (i == clock)
            continue;
        if (!de_linear_decompose(coefficients[i], de->independent_vars[i], &nodes[i].slope, &nodes[i].offset) ||
            !de_flow_time_only(nodes[i].slope, de, clock, dependent) || de_expr_uses(nodes[i].offset, dependent) ||
            !expr_collect_var_usage(nodes[i].offset, n, de->independent_vars, edges + i * n))
            goto cleanup;
        edges[i * n + clock] = false;
        if (edges[i * n + i])
            goto cleanup;
        for (size_t j = 0u; j < n; ++j)
            nodes[i].dependencies += edges[i * n + j] ? 1u : 0u;
        if (!nodes[i].dependencies)
            queue[tail++] = i;
    }
    /* Kahn's ordering detects cycles without trying every permutation of the coordinates. */
    while (head < tail) {
        size_t index = queue[head++];
        if (!de_flow_resolve(nodes, edges, index, clock, de, coefficients[clock], dependent))
            goto cleanup;
        for (size_t i = 0u; i < n; ++i)
            if (edges[i * n + index] && --nodes[i].dependencies == 0u)
                queue[tail++] = i;
    }
    if (head != n - 1u)
        goto cleanup;
    /* No integration parameter may escape into the result or its arbitrary-function arguments. */
    for (size_t i = 0u; i < n; ++i) {
        if (i == clock)
            continue;
        for (size_t j = 0u; j < n; ++j)
            if (j != clock && de_expr_uses(nodes[i].invariant, nodes[j].parameter))
                goto cleanup;
    }
    for (size_t i = 0u, count = 0u; i < n; ++i) {
        if (i == clock)
            continue;
        invariants[count++] = nodes[i].invariant;
        nodes[i].invariant = NULL;
    }
    valid = true;
cleanup:
    for (size_t i = 0u; nodes && i < n; ++i) {
        expr_free(nodes[i].invariant); expr_free(nodes[i].flow); expr_free(nodes[i].parameter);
        expr_free(nodes[i].offset); expr_free(nodes[i].slope);
    }
    free(queue); free(edges); free(nodes);
    return valid;
}
