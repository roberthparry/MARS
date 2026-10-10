/**
 * @file lab_binding_sync.c
 * @brief Binding request normalisation and accepted editor projection for MARS Lab.
 *
 * Copies opaque native binding records using the original browser truthy and
 * nullish defaults, preserving sparse arrays without retaining scoped handles.
 * Projects readiness, display text and variable metadata only after the host has
 * accepted an asynchronous edit. Mathematical interpretation remains on the native
 * service; native continuations own stale-response checks while browser adapters
 * execute awaits and workspace persistence.
 */
#include "lab_dom.h"
#include "lab_binding.h"
#include "lab_binding_editor.h"
#include "lab_binding_sync.h"

/* Prepare the complete service record, preserving Array.map holes and default semantics. */
void lab_binding_sync_request(int inputs)
{
    int result = lab_dom_object(5);
    int source = lab_dom_clean(lab_dom_text(lab_binding_or(lab_dom_get(inputs, "body"), "")), 0);
    lab_dom_set(result, "source", source);
    if (!lab_dom_length(source)) {
        lab_dom_return(result);
        return;
    }
    int bindings = lab_dom_get(inputs, "bindings");
    unsigned count = lab_binding_array_count(bindings);
    if (!count) {
        lab_dom_return(result);
        return;
    }
    int normalised = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    /* Each input index is visited once, as in Array.map; temporary handles are bounded. */
    for (unsigned i = 0; i < count; ++i) {
        if (!lab_dom_index_present(bindings, i)) {
            lab_dom_push(normalised, 0);
            lab_dom_key_delete(normalised, lab_dom_numeric(i));
        } else {
            int binding = lab_dom_item(bindings, i);
            if (!lab_dom_type(binding)) {
                lab_dom_release(mark);
                lab_dom_set(result, "invalid", lab_dom_scalar(1, 1));
                lab_dom_return(result);
                return;
            }
            int entry = lab_dom_object(5);
            lab_dom_set(entry, "name", lab_dom_text(lab_binding_or(lab_dom_get(binding, "name"), "")));
            lab_dom_set(entry, "kind", lab_binding_or(lab_dom_get(binding, "kind"), "variable"));
            int value = lab_dom_get(binding, "value");
            if (!lab_dom_type(value))
                value = lab_dom_get(binding, "display");
            if (!lab_dom_type(value))
                value = lab_dom_string("?");
            lab_dom_set(entry, "value", lab_dom_text(value));
            lab_dom_push(normalised, entry);
        }
        lab_dom_release(mark);
    }
    int request = lab_dom_object(5);
    lab_dom_set(request, "action", lab_dom_string("editor"));
    lab_dom_set(request, "operation", lab_dom_string("bindings"));
    lab_dom_set(request, "mode", lab_dom_string("replace"));
    lab_dom_set(request, "text", source);
    lab_dom_set(request, "bindings", normalised);
    lab_dom_set(result, "request", request);
    lab_dom_return(result);
}

static int lab_binding_sync_flag(int data, const char *field, const char *fallback, const char *expected)
{
    int value = lab_dom_truth(data) ? lab_dom_get(data, field) : 0;
    return lab_binding_is(lab_dom_clean(lab_dom_text(lab_binding_or(value, fallback)), 1), expected);
}

/* Apply accepted editor presentation and derive metadata from the existing binding policies. */
void lab_binding_sync_apply(int editor, int inputs)
{
    int data = lab_dom_get(inputs, "data"), bindings = lab_dom_get(inputs, "bindings");
    lab_dom_write(editor, 3, "bindingRefreshValid", lab_dom_string("true"));
    lab_dom_write(editor, 3, "evaluationReady",
                  lab_dom_string(lab_binding_sync_flag(data, "evaluation_ready", "no", "yes") ? "true" : "false"));
    lab_dom_write(editor, 5, "", lab_dom_get(inputs, "displayText"));
    int result = lab_dom_object(5), variables = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(bindings); ++i) {
        int name = lab_binding_variable_name(lab_dom_item(bindings, i));
        if (name)
            lab_dom_push(variables, name);
        lab_dom_release(mark);
    }
    lab_dom_set(result, "variables", variables);
    lab_dom_set(result, "differentiable",
                lab_dom_scalar(1, !lab_binding_sync_flag(data, "differentiable", "yes", "no")));
    lab_dom_return(result);
}
