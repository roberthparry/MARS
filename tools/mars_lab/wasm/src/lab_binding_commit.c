/**
 * @file lab_binding_commit.c
 * @brief Binding merge payloads and accepted value/kind update plans for MARS Lab.
 *
 * Keeps worksheet choices and completion ordering in C while browser adapters
 * execute the asynchronous operations selected by native freshness guards. Source and binding values remain
 * opaque native mathematics. Every captured entry is visited once when building
 * a payload, with temporary handles released per entry. No handle survives a call.
 */
#include "lab_dom.h"
#include "lab_binding.h"
#include "lab_binding_commit.h"

static void lab_binding_commit_emit(int calls, const char *service, unsigned count, int first, int second)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Build the request only; input controls and their captured binding records remain unchanged. */
void lab_binding_commit_request(unsigned mode, int source, int snapshot)
{
    unsigned count = lab_binding_array_count(snapshot);
    if (mode > 6 || !count) {
        lab_dom_return(0);
        return;
    }
    int request = lab_dom_object(5), bindings = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        if (lab_dom_index_present(snapshot, i)) {
            lab_dom_push(bindings, lab_dom_get(lab_dom_item(snapshot, i), "binding"));
        } else {
            lab_dom_push(bindings, 0);
            lab_dom_key_delete(bindings, lab_dom_numeric(i));
        }
        lab_dom_release(mark);
    }
    lab_dom_set(request, "action", lab_dom_string("editor"));
    lab_dom_set(request, "operation", lab_dom_string("bindings"));
    lab_dom_set(request, "mode", lab_dom_string("merge"));
    lab_dom_set(request, "text", source);
    lab_dom_set(request, "bindings", bindings);
    lab_dom_set(request, "unset_constants", lab_dom_scalar(1, mode != 0));
    lab_dom_return(request);
}

/* Order accepted updates without executing callbacks or rewriting getter-backed workspace views. */
void lab_binding_commit_plan(unsigned mode, unsigned operation, int source, int updated, int context)
{
    if (mode > 6 || operation > 1) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    int accepted = !lab_dom_equal(updated, source) && (operation || lab_dom_truth(updated));
    int wait = accepted && mode == 0 && operation == 1;
    lab_dom_set(plan, "calls", calls);
    lab_dom_set(plan, "result", lab_dom_scalar(1, accepted));
    lab_dom_set(plan, "wait", lab_dom_scalar(1, wait));
    if (accepted) {
        if (wait) {
            int body = lab_dom_clean(lab_dom_read(lab_dom_get(context, "editor"), 5, ""), 0);
            lab_binding_commit_emit(calls, "applyMarsBindingExpression", 2, updated, body);
        } else {
            if (mode == 0) {
                lab_binding_commit_emit(calls, "source", 1, updated, 0);
                lab_binding_commit_emit(calls, "cache", 2, lab_dom_get(context, "snapshot"),
                                        lab_dom_get(context, "editor"));
            } else {
                lab_binding_commit_emit(calls, "applyUpdatedBindingExpression", 1, updated, 0);
                lab_binding_commit_emit(calls, "refreshVariableValuesFromEditor", operation ? 0 : 1,
                                        operation ? 0 : lab_dom_get(context, "isCurrent"), 0);
            }
            lab_binding_commit_emit(calls, "updateHistoryButtons", 0, 0, 0);
            lab_binding_commit_emit(calls, "saveCurrentModeEditorState", 0, 0, 0);
        }
    }
    lab_dom_return(plan);
}
