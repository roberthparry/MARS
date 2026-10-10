/**
 * @file lab_solver_view.c
 * @brief Native responsive solver request preparation, freshness and publication.
 *
 * Returns browser-owned snapshots of exact native solver inputs and their owners.
 * The browser awaits and cancels requests; C chooses whether wrapping is needed,
 * deduplicates live work, rejects stale completions and projects the proper variant
 * using the current viewport. No handles survive a synchronous entry point and no
 * mathematical text or SVG grammar is parsed or reconstructed by this module.
 */
#include "lab_dom.h"
#include "lab_layout.h"
#include "lab_solver_view.h"

static int lab_solver_view_text(int node, const char *key)
{
    int value = lab_dom_read(node, 3, key);
    return lab_dom_truth(value) ? value : lab_dom_string("");
}

static int lab_solver_view_enabled(int mode, int node)
{
    return lab_dom_equal(mode, lab_dom_string("diffequation")) && node && lab_dom_has_class(node, "equation-function");
}

static void lab_solver_view_install(int compact, int wrapped)
{
    int use_wrapped = lab_dom_truth(wrapped) && lab_layout_solver_wrapped();
    lab_layout_solver_install(use_wrapped ? wrapped : compact, lab_dom_string(use_wrapped ? "wrapped" : "compact"));
}

/* Compare the complete browser snapshot without interpreting source text or retaining its handles. */
int lab_solver_view_current(int mode, int snapshot, int active, unsigned live, int restored_owner)
{
    if (!snapshot || !live || !lab_dom_equal(snapshot, active))
        return 0;
    int node = lab_dom_query(0, "#functionStyle");
    return lab_solver_view_enabled(mode, node) && lab_dom_equal(node, lab_dom_get(snapshot, "node")) &&
           lab_dom_equal(restored_owner, lab_dom_get(snapshot, "restoredOwner")) &&
           lab_dom_equal(lab_solver_view_text(node, "solverParentToken"), lab_dom_get(snapshot, "parentToken")) &&
           lab_dom_equal(lab_dom_read(node, 3, "solverCompactSvg"), lab_dom_get(snapshot, "compactSvg")) &&
           lab_dom_equal(lab_dom_read(node, 3, "solverWrappedTex"), lab_dom_get(snapshot, "wrappedTex"));
}

/* Project cached variants immediately or return one request plan whose lifetime belongs to the browser. */
void lab_solver_view_prepare(int mode, int pending, unsigned pending_live, int restored_owner)
{
    lab_dom_return(0);
    int node = lab_dom_query(0, "#functionStyle");
    if (!lab_solver_view_enabled(mode, node))
        return;
    int compact = lab_solver_view_text(node, "solverCompactSvg");
    if (!lab_dom_truth(compact))
        return;
    int wrapped = lab_solver_view_text(node, "solverWrappedSvg");
    if (!lab_layout_solver_wrapped() || lab_dom_truth(wrapped)) {
        lab_solver_view_install(compact, wrapped);
        return;
    }
    if (lab_solver_view_current(mode, pending, pending, pending_live, restored_owner))
        return;
    int source = lab_solver_view_text(node, "solverWrappedTex");
    if (!lab_dom_truth(source))
        return;
    int parent = lab_solver_view_text(node, "solverParentToken");
    int snapshot = lab_dom_object(5), options = lab_dom_object(5), payload = lab_dom_object(5);
    lab_dom_set(snapshot, "node", node);
    lab_dom_set(snapshot, "compactSvg", compact);
    lab_dom_set(snapshot, "wrappedTex", source);
    lab_dom_set(snapshot, "parentToken", parent);
    lab_dom_set(snapshot, "restoredOwner", restored_owner);
    lab_dom_set(snapshot, "mode", mode);
    lab_dom_set(snapshot, "operation", lab_dom_string(restored_owner ? "solverRestoreRender" : "solverRender"));
    lab_dom_set(options, "parent", lab_dom_numeric(restored_owner ? 0 : lab_dom_number(parent)));
    lab_dom_set(options, "input", lab_dom_scalar(1, 1));
    lab_dom_set(snapshot, "options", options);
    lab_dom_set(payload, "tex", source);
    lab_dom_set(snapshot, "payload", payload);
    lab_dom_set(snapshot, "endpoint", lab_dom_string("/render_TeX"));
    lab_dom_return(snapshot);
}

/* Reject stale replies before any cache or DOM write, then use current rather than captured geometry. */
int lab_solver_view_publish(int mode, int snapshot, int active, unsigned live, int restored_owner, int response,
                            int data, unsigned failed)
{
    if (!lab_solver_view_current(mode, snapshot, active, live, restored_owner))
        return 0;
    int compact = lab_dom_get(snapshot, "compactSvg");
    if (failed) {
        lab_layout_solver_install(compact, lab_dom_string("compact"));
        return 1;
    }
    int wrapped = lab_dom_string("");
    if (lab_dom_truth(lab_dom_get(response, "ok")) && lab_dom_truth(lab_dom_get(data, "ok"))) {
        int svg = lab_dom_get(data, "svg");
        if (lab_dom_truth(svg))
            wrapped = lab_dom_text(svg);
    }
    lab_dom_write(lab_dom_get(snapshot, "node"), 3, "solverWrappedSvg", wrapped);
    lab_solver_view_install(compact, wrapped);
    return 1;
}
