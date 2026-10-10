/**
 * @file lab_binding_integrator_state.c
 * @brief Native selection and service ordering for integrator binding responses.
 *
 * Resolves native editor-cache metadata, filters integration-bound names and chooses
 * editor, binding-card and saved-source updates. Server mathematics and binding records
 * remain opaque. Each binding is visited once with Set membership and scoped temporary
 * handles, so large responses do not exhaust the synchronous browser handle table.
 * Existing browser services retain asynchronous preparation and live-state accessors.
 */
#include "lab_dom.h"
#include "lab_binding.h"
#include "lab_binding_integrator_state.h"

static void lab_binding_integrator_state_emit(int calls, const char *service, unsigned count, int first, int second,
                                              int third)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    const int values[] = {first, second, third};
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(args, values[i]);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Select exact native metadata and defer stateful editor services beyond the scoped DOM call. */
void lab_binding_integrator_state(int data, int fallback, int editors, int bound_names)
{
    int expression = lab_dom_get(data, "binding_expression");
    if (!lab_dom_truth(expression))
        expression = fallback;
    expression = lab_dom_clean(lab_binding_or(expression, ""), 0);
    int metadata = lab_dom_map_get(editors, expression), sorted = lab_dom_get(metadata, "expression");
    if (lab_dom_truth(sorted))
        expression = sorted;
    /* Canonical server text has its own cache key; do not infer wrapping from the first lookup. */
    metadata = lab_dom_map_get(editors, lab_dom_clean(lab_binding_or(expression, ""), 0));
    int body = lab_dom_get(data, "expression");
    if (!lab_dom_truth(body))
        body = lab_dom_read(lab_dom_query(0, "#expr"), 5, "");
    body = lab_dom_clean(lab_binding_or(body, ""), 0);
    int bindings = lab_dom_get(data, "binding_values"), editable = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(bindings); ++i) {
        int binding = lab_dom_item(bindings, i);
        if (!lab_dom_member(bound_names, lab_binding_name(binding)))
            lab_dom_push(editable, binding);
        lab_dom_release(mark);
    }
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    if (lab_dom_truth(expression) && lab_dom_truth(lab_dom_get(metadata, "wrapped"))) {
        lab_binding_integrator_state_emit(calls, "setExpressionEditor", 3, expression, editable,
                                          lab_dom_length(body) ? body : 0);
        if (!lab_dom_count(editable))
            lab_binding_integrator_state_emit(calls, "clearVariableValues", 0, 0, 0, 0);
        lab_binding_integrator_state_emit(calls, "setIntegratorBindingExpression", 1, expression, 0, 0);
    } else if (lab_dom_count(editable)) {
        lab_binding_integrator_state_emit(calls, "renderVariableValues", 1, editable, 0, 0);
    } else {
        lab_binding_integrator_state_emit(calls, "clearVariableValues", 0, 0, 0, 0);
    }
    lab_dom_return(plan);
}
