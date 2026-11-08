/**
 * @file lab_solution_pages.c
 * @brief Native ownership and append-only presentation of resumable solution lists.
 *
 * Only an explicit native solution-page capability enables this path. Ordinary mathematical cards never gain
 * a continuation. Browser code supplies visibility events and asynchronous transport; C owns cursors, admission,
 * generation checks, duplicate suppression, copy metadata and DOM installation. All JavaScript handles are
 * borrowed within one synchronous bridge call; persistent records are attached to the owning card.
 */
#include "lab_dom.h"
#include "lab_mode.h"
#include "lab_requests.h"
#include "lab_workspace.h"
#include "lab_layout.h"
#include "lab_result.h"

static jsref page_state(void)
{
    int node = lab_dom_query(0, "#rendered"), state = lab_dom_get(node, "labSolutionPages");
    if (!state || lab_workspace_mode() != LAB_MODE_EQUATION ||
        lab_dom_to_number(lab_dom_get(state, "context")) != lab_request_context() ||
        lab_dom_to_number(lab_dom_get(state, "main")) != lab_request_latest_main() ||
        lab_dom_to_number(lab_dom_get(state, "bits")) != lab_workspace_requested_precision(LAB_MODE_EQUATION) ||
        !lab_dom_equal(lab_dom_get(state, "editor"), lab_dom_read(lab_dom_query(0, "#expr"), LAB_DOM_VALUE, "")))
        return 0;
    return state;
}

/* Attach a capability only after the initial native solution rendering is installed. */
void lab_solution_pages_install(jsref data)
{
    if (!lab_dom_equal(lab_dom_get(data, "solution_page"), lab_dom_string("yes")))
        return;
    int node = lab_dom_query(0, "#rendered"), state = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(state, "context", lab_dom_numeric(lab_request_context()));
    lab_dom_set(state, "main", lab_dom_numeric(lab_request_latest_main()));
    lab_dom_set(state, "bits", lab_dom_numeric(lab_workspace_requested_precision(LAB_MODE_EQUATION)));
    lab_dom_set(state, "editor", lab_dom_read(lab_dom_query(0, "#expr"), LAB_DOM_VALUE, ""));
    lab_dom_set(state, "source", lab_dom_get(data, "input"));
    lab_dom_set(state, "precision", lab_dom_get(data, "solutions_precision"));
    lab_dom_set(state, "after", lab_dom_get(data, "solutions_after"));
    lab_dom_set(state, "background", lab_dom_get(data, "solutions_background"));
    lab_dom_set(state, "more", lab_dom_get(data, "solutions_more"));
    lab_dom_set(state, "pages", lab_dom_numeric(1));
    int copies = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_push(copies, lab_dom_get(data, "solution_copy_TeX"));
    lab_dom_set(state, "copies", copies);
    int heading = lab_dom_create("div"), footer = lab_dom_create("button");
    lab_dom_write(heading, LAB_DOM_TEXT, "", lab_dom_string("Non-trivial zeros found"));
    lab_dom_class(heading, "solution-page-heading", 1);
    lab_dom_prepend(node, heading);
    lab_dom_write(footer, LAB_DOM_TEXT, "", lab_dom_string("…"));
    lab_dom_write(footer, LAB_DOM_ATTRIBUTE, "type", lab_dom_string("button"));
    lab_dom_write(footer, LAB_DOM_ATTRIBUTE, "aria-label", lab_dom_string("More zeros; scroll or activate to continue"));
    lab_dom_class(footer, "solution-more", 1);
    lab_dom_append(node, footer);
    lab_dom_set(state, "footer", footer);
    lab_dom_set(node, "labSolutionPages", state);
}

/* Let the browser cancel obsolete network work without duplicating native ownership policy. */
bool lab_solution_pages_valid(void)
{
    return page_state() != 0;
}

/* Return an opaque request only once per cursor and only for background completion or a visible continuation. */
void lab_solution_pages_request(bool visible)
{
    int state = page_state();
    lab_dom_return(0);
    if (!state || lab_dom_truth(lab_dom_get(state, "pending")) ||
        !lab_dom_equal(lab_dom_get(state, "more"), lab_dom_string("yes")) ||
        (!visible && !lab_dom_equal(lab_dom_get(state, "background"), lab_dom_string("yes"))) ||
        lab_dom_to_number(lab_dom_get(state, "pages")) >= 128)
        return;
    int request = lab_dom_object(LAB_VALUE_OBJECT), payload = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(payload, "equation", lab_dom_get(state, "source"));
    lab_dom_set(payload, "precision", lab_dom_numeric(lab_dom_to_number(lab_dom_get(state, "precision"))));
    lab_dom_set(payload, "solutions_after", lab_dom_get(state, "after"));
    lab_dom_set(payload, "solution_pages", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    lab_dom_set(request, "payload", payload);
    lab_dom_set(request, "after", lab_dom_get(state, "after"));
    lab_dom_set(state, "pending", request);
    int footer = lab_dom_get(state, "footer");
    lab_dom_write(footer, LAB_DOM_ATTRIBUTE, "aria-busy", lab_dom_string("true"));
    lab_dom_write(footer, LAB_DOM_ATTRIBUTE, "title", lab_dom_string("Calculating more zeros…"));
    lab_dom_return(request);
}

/* Append only a complete current page; failures retain every existing row and allow a later retry. */
void lab_solution_pages_finish(jsref request, jsref data)
{
    int state = page_state();
    lab_dom_return(0);
    if (!state || !lab_dom_equal(lab_dom_get(state, "pending"), request))
        return;
    lab_dom_delete(state, "pending");
    int footer = lab_dom_get(state, "footer"), node = lab_dom_query(0, "#rendered");
    lab_dom_write(footer, LAB_DOM_ATTRIBUTE, "aria-busy", lab_dom_string("false"));
    int next = lab_dom_get(data, "solutions_after"), svg = lab_dom_get(data, "svg");
    int rows = lab_dom_get(data, "solutions_rows");
    bool empty = lab_dom_truth(rows) && lab_dom_to_number(rows) == 0;
    bool ok = lab_dom_truth(lab_dom_get(data, "ok")) && (empty || lab_dom_length(svg)) &&
              lab_dom_equal(lab_dom_get(data, "solution_page"), lab_dom_string("yes")) &&
              lab_dom_to_number(next) > lab_dom_to_number(lab_dom_get(request, "after"));
    if (!ok) {
        lab_dom_write(footer, LAB_DOM_ATTRIBUTE, "title", lab_dom_string("More zeros unavailable; scroll or tap to retry"));
        return;
    }
    if (!empty) {
        int page = lab_dom_create("div");
        lab_dom_class(page, "solution-page", 1);
        lab_dom_class(page, "rendered-zoom-frame", 1);
        lab_dom_write(page, LAB_DOM_HTML, "", svg);
        lab_dom_append(node, page);
        lab_dom_append(node, footer);
        lab_dom_push(lab_dom_get(state, "copies"), lab_dom_get(data, "solution_copy_TeX"));
    }
    lab_dom_set(state, "after", next);
    lab_dom_set(state, "more", lab_dom_get(data, "solutions_more"));
    lab_dom_set(state, "background", lab_dom_get(data, "solutions_background"));
    lab_dom_set(state, "pages", lab_dom_numeric(lab_dom_to_number(lab_dom_get(state, "pages")) + 1));
    lab_dom_write(footer, LAB_DOM_ATTRIBUTE, "title", lab_dom_string("More zeros…"));
    lab_dom_class(footer, "hidden", !lab_dom_equal(lab_dom_get(state, "more"), lab_dom_string("yes")));
    lab_dom_schedule(LAB_DOM_SCHEDULE_ZOOM, lab_dom_closest(node, ".result-card"));
    lab_dom_return(lab_dom_join_array(lab_dom_get(state, "copies"), "\n\n"));
}
