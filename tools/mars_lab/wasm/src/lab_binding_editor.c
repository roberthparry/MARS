/**
 * @file lab_binding_editor.c
 * @brief Editor binding selection and goal-start precedence for MARS Lab.
 *
 * Selects visible parameters and native editor metadata, projects editor
 * readiness, and combines supplied, authored and solved goal starts. Native
 * mathematical text is copied without interpretation. Native continuations own
 * asynchronous request ordering and stale-response checks. Browser handles are
 * scoped to a synchronous call; name indexes retain exact native identities.
 */
#include "lab_dom.h"
#include "lab_binding.h"
#include "lab_binding_editor.h"

/* Share the exact native variable-kind and trimmed-name policy with row reconciliation. */
int lab_binding_variable_name(int binding)
{
    int name = lab_binding_name(binding);
    if (lab_binding_is(lab_dom_text(lab_binding_or(lab_dom_get(binding, "kind"), "variable")), "constant"))
        return 0;
    return lab_dom_length(name) ? name : 0;
}

/* Select variable identities in native discovery order; constants do not become calculus arguments. */
void lab_binding_variables(int bindings)
{
    int names = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(bindings); ++i) {
        int name = lab_binding_variable_name(lab_dom_item(bindings, i));
        if (name)
            lab_dom_push(names, name);
        lab_dom_release(mark);
    }
    lab_dom_return(names);
}

static int lab_binding_selection(unsigned mode, int bindings, int bound_names)
{
    int selected = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(bindings); ++i) {
        int binding = lab_dom_item(bindings, i);
        if (mode != 4 || !lab_dom_member(bound_names, lab_binding_name(binding)))
            lab_dom_push(selected, binding);
        lab_dom_release(mark);
    }
    return selected;
}

/* Hide integration-bound names from editable parameters without changing native binding records. */
void lab_binding_select(unsigned mode, int bindings, int bound_names)
{
    lab_dom_return(lab_binding_selection(mode, bindings, bound_names));
}

static int lab_binding_editor_body(int text, int metadata)
{
    int body = lab_dom_get(metadata, "body");
    return lab_dom_type(body) ? body : lab_dom_clean(lab_binding_or(text, ""), 0);
}

/* Select and project editor metadata; return opaque source/display bytes for the native workspace owner. */
void lab_binding_editor(unsigned mode, int inputs, int bound_names)
{
    int metadata = lab_dom_get(inputs, "metadata"), text = lab_dom_get(inputs, "text");
    int body_text = lab_dom_get(inputs, "body"), ready = lab_dom_get(inputs, "ready");
    int body = lab_dom_type(body_text) ? lab_binding_editor_body(body_text, lab_dom_get(inputs, "bodyMetadata"))
                                       : lab_binding_editor_body(text, metadata);
    int bindings = lab_dom_get(inputs, "bindings");
    if (lab_dom_type(bindings) != 4)
        bindings = mode == 0 ? 0 : lab_dom_get(metadata, "bindings");
    int selected = lab_binding_selection(mode, bindings, bound_names);
    int editor = lab_dom_query(0, "#expr"), result = lab_dom_object(5);
    lab_dom_set(result, "fullText", lab_dom_clean(lab_binding_or(text, ""), 0));
    lab_dom_set(result, "displayText", body);
    lab_dom_set(result, "bindings", selected);
    lab_dom_set(result, "refresh", lab_dom_scalar(1, mode == 0 && !lab_dom_type(ready)));
    lab_dom_write(editor, 3, "bindingRefreshValid", lab_dom_string("true"));
    if (lab_dom_type(ready))
        lab_dom_write(editor, 3, "evaluationReady",
                      lab_dom_string(lab_binding_is(lab_dom_clean(ready, 1), "yes") ? "true" : "false"));
    else
        lab_dom_remove(editor, "data-evaluation-ready");
    lab_dom_write(editor, 5, "", body);
    lab_dom_return(result);
}

/* Choose cached authored then solved values over supplied starts, excluding source constants. */
void lab_binding_goal_starts(int source, int solved, int provided, int cache)
{
    int result = lab_binding_copy(provided);
    if (!lab_dom_truth(source) || !lab_dom_truth(solved)) {
        lab_dom_return(result);
        return;
    }
    int index = lab_dom_map_new(), solved_bindings = lab_dom_get(solved, "bindings");
    int source_bindings = lab_dom_get(source, "bindings");
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(solved_bindings); ++i) {
        int binding = lab_dom_item(solved_bindings, i);
        lab_dom_map_set(index, lab_dom_get(binding, "name"), binding);
        lab_dom_release(mark);
    }
    for (unsigned i = 0; i < lab_binding_array_count(source_bindings); ++i) {
        int binding = lab_dom_item(source_bindings, i), name = lab_dom_get(binding, "name");
        if (!lab_binding_is(lab_dom_get(binding, "kind"), "constant")) {
            int candidate = lab_dom_map_get(index, name), value = lab_dom_map_get(cache, name);
            if (!lab_dom_truth(value) && !lab_dom_truth(lab_dom_get(candidate, "unset")))
                value = lab_dom_get(candidate, "value");
            if (lab_dom_truth(value))
                lab_dom_key_set(result, name, value);
        }
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Fill only falsey supplied starts from native goal metadata, preserving exact symbolic values. */
void lab_binding_goal_prepare(int editor, int provided)
{
    int result = lab_dom_object(5), start = lab_binding_copy(provided);
    int defaults = lab_dom_get(editor, "starts"), keys = lab_dom_keys(defaults);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_dom_count(keys); ++i) {
        int key = lab_dom_item(keys, i);
        if (!lab_dom_truth(lab_dom_key_get(start, key)))
            lab_dom_key_set(start, key, lab_dom_key_get(defaults, key));
        lab_dom_release(mark);
    }
    lab_dom_set(result, "expression", lab_dom_get(editor, "goal_expression"));
    lab_dom_set(result, "start", start);
    lab_dom_return(result);
}
