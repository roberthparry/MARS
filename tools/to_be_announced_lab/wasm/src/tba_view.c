/**
 * @file tba_view.c
 * @brief Native forecast cards and comparison-tab presentation.
 *
 * Displays server-produced summaries and tables without reinterpreting model
 * output. Structured chart values are passed to the native SVG renderer.
 */
#include "tba_ui.h"
#include "tba_view.h"

/* Publish one completed forecast and its downloadable result. */
void tba_view_result(browser_jsref data)
{
    tba_ui_store("result", data);
    int multi = tba_dom_truth(tba_dom_get(data, "multi"));
    tba_ui_write("metric-model", "textContent", multi ?
        tba_ui_text("", tba_dom_format(tba_dom_count(tba_dom_get(data, "results")), 0), " models") :
        tba_ui_or(tba_dom_get(data, "model"), "Forecast"));
    tba_ui_write("metric-fit-rows", "textContent", multi ? tba_dom_string("Varies") :
                 tba_ui_or(tba_dom_get(data, "fit_rows"), "-"));
    tba_ui_write("metric-stationary", "textContent", tba_dom_string(multi ? "Mixed" :
                 tba_dom_truth(tba_dom_get(data, "stationary")) ? "Yes" : "No"));
    tba_ui_write("metric-invertible", "textContent", tba_dom_string(multi ? "Mixed" :
                 tba_dom_truth(tba_dom_get(data, "invertible")) ? "Yes" : "No"));
    int summary = tba_dom_get(data, "summary_html");
    if (multi) {
        int parts = tba_dom_object(1);
        tba_ui_add(parts, "<div class=\"comparison-stack\">");
        tba_dom_push(parts, summary);
        tba_ui_add(parts, "<div class=\"table-wrap comparison-table-wrap\"><div class=\"table-scroll\">"
                          "<table><thead>");
        tba_dom_push(parts, tba_dom_get(data, "table_head"));
        tba_ui_add(parts, "</thead><tbody>");
        tba_dom_push(parts, tba_dom_get(data, "table_body"));
        tba_ui_add(parts, "</tbody></table></div></div></div>");
        summary = tba_ui_join(parts);
    } else {
        tba_ui_write("forecast-head", "innerHTML", tba_dom_get(data, "table_head"));
        tba_ui_write("forecast-body", "innerHTML", tba_dom_get(data, "table_body"));
    }
    tba_ui_write("summary-box", "innerHTML", summary);
    tba_dom_class(tba_dom_query(0, ".forecast-table-wrap"), "hidden", multi);
    int note = tba_dom_object(1);
    tba_ui_add(note, "Target modelled in this run: ");
    tba_dom_push(note, tba_ui_or(tba_dom_get(data, "target_value_column_used"), "(none)"));
    tba_ui_add(note, " | Drivers used in this run: ");
    tba_dom_push(note, tba_ui_or(tba_dom_get(data, "xreg_columns_used"), "(none)"));
    if (multi)
        tba_ui_add(note, " | Downloads contain the combined multi-model summary and comparison CSV.");
    tba_ui_write("drivers-used-note", "textContent", tba_ui_join(note));
    static const char *const ids[] = {"download-summary", "download-forecast"};
    static const char *const keys[] = {"summary_text", "forecast_csv"};
    for (unsigned i = 0; i < 2; ++i) {
        int present = tba_dom_truth(tba_dom_get(data, keys[i]));
        tba_dom_class(tba_dom_id(ids[i]), "disabled", !present);
        tba_ui_write(ids[i], "aria-disabled", tba_dom_string(present ? "false" : "true"));
    }
    tba_view_chart(tba_dom_get(data, "chart"));
    tba_ui_status(tba_dom_string("Forecast complete."), "ok");
}

/* Delegate tab selection from the stable summary container. */
void tba_view_tab(event_jsref event)
{
    int button = tba_dom_closest(tba_dom_get(event, "target"), ".comparison-tab");
    int tabs = tba_dom_closest(button, ".comparison-tabs");
    if (!tabs)
        return;
    tba_dom_prevent(event);
    int id = tba_dom_read(button, 2, "data-tab-target"), buttons = tba_dom_all(tabs, ".comparison-tab");
    for (unsigned i = 0; i < tba_dom_count(buttons); ++i) {
        int node = tba_dom_item(buttons, i), active = tba_dom_equal(node, button);
        tba_dom_class(node, "is-active", active);
        tba_dom_write(node, 2, "aria-selected", tba_dom_string(active ? "true" : "false"));
    }
    int panels = tba_dom_all(tabs, ".comparison-tab-panel");
    for (unsigned i = 0; i < tba_dom_count(panels); ++i) {
        int panel = tba_dom_item(panels, i);
        tba_dom_class(panel, "is-active", tba_dom_equal(tba_dom_get(panel, "id"), id));
    }
}
