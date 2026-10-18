/**
 * @file tba_chart_view.c
 * @brief Native SVG charts, legend controls and pointer tooltips.
 *
 * Walks each bounded series once to build paths and hover points. Scoped handles
 * are released per observation; the browser retains values, not native handles.
 * Numeric geometry belongs to tba_chart; no CSV or mathematical parsing occurs.
 */
#include "tba_ui.h"
#include "tba_chart.h"
#include "tba_view.h"

static const char *const tba_chart_colours[] = {
    "#2f6fed", "#e0529c", "#1f9d78", "#ef8a17", "#7757d6", "#c94c4c", "#0f7c95", "#7a6a18"
};
static int tba_chart_hover_index = -1;

static double tba_chart_value(jsref array, unsigned index)
{
    int value = tba_dom_item(array, index);
    return tba_dom_type(value) == 2 ? tba_dom_number(value) : __builtin_nan("");
}

static void tba_chart_number(jsref parts, double value)
{
    tba_dom_push(parts, tba_dom_format(value, 2));
}

static jsref tba_chart_label(double value)
{
    double magnitude = value < 0 ? -value : value;
    int decimals = magnitude >= 1000 ? 0 : magnitude >= 100 ? 1 : magnitude >= 10 ? 2 : 3;
    return tba_dom_format(value, -decimals - 2);
}

static void tba_chart_legend(jsref parts, jsref id, jsref label, const char *colour, int checked)
{
    tba_ui_add(parts, checked ? "<label class=\"chart-legend-item\">" :
                              "<label class=\"chart-legend-item is-off\">");
    tba_ui_add(parts, "<input type=\"checkbox\" name=\"chart-series-toggle\" data-series-id=\"");
    tba_ui_escaped(parts, id);
    tba_ui_add(parts, checked ? "\" checked>" : "\">");
    tba_ui_add(parts, "<span class=\"chart-legend-swatch\" style=\"--swatch:");
    tba_ui_add(parts, colour);
    tba_ui_add(parts, "\"></span>");
    tba_ui_escaped(parts, label);
    tba_ui_add(parts, "</label>");
}

static void tba_chart_attributes(jsref parts, jsref id, jsref parent, const char *kind)
{
    tba_ui_add(parts, " data-chart-series-id=\"");
    tba_ui_escaped(parts, id);
    tba_ui_add(parts, "\"");
    if (parent) {
        tba_ui_add(parts, " data-chart-parent-id=\"");
        tba_ui_escaped(parts, parent);
        tba_ui_add(parts, "\" data-confidence-kind=\"");
        tba_ui_add(parts, kind);
        tba_ui_add(parts, "\"");
    }
}

static void tba_chart_series(jsref parts, jsref hover, jsref labels, jsref values,
                             jsref id, jsref label, const char *colour, jsref parent,
                             const char *kind, const char *class_name)
{
    int path = tba_dom_object(1), points = tba_dom_object(1), active = 0;
    unsigned count = tba_dom_count(values), limit = tba_dom_count(labels);
    if (count > limit)
        count = limit;
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = tba_dom_mark();
        double value = tba_chart_value(values, i);
        if (__builtin_isfinite(value)) {
            double x = tba_chart_x(i), y = tba_chart_y(value);
            tba_ui_add(path, active ? "L" : "M");
            tba_chart_number(path, x);
            tba_ui_add(path, ",");
            tba_chart_number(path, y);
            int point = tba_dom_object(0);
            tba_dom_set(point, "x", tba_dom_numeric(x));
            tba_dom_set(point, "y", tba_dom_numeric(y));
            tba_dom_set(point, "value", tba_dom_numeric(value));
            tba_dom_set(point, "date", tba_dom_item(labels, i));
            tba_dom_push(points, point);
            active = 1;
        } else {
            active = 0;
        }
        tba_dom_release(mark);
    }
    if (!tba_dom_count(points))
        return;
    int entry = tba_dom_object(0), index = tba_dom_format(tba_dom_count(hover), 0), path_text = tba_ui_join(path);
    tba_dom_set(entry, "label", label);
    tba_dom_set(entry, "colour", tba_dom_string(colour));
    tba_dom_set(entry, "points", points);
    tba_dom_push(hover, entry);
    tba_ui_add(parts, "<path class=\"chart-line ");
    tba_ui_add(parts, class_name);
    tba_ui_add(parts, "\"");
    tba_chart_attributes(parts, id, parent, kind);
    tba_ui_add(parts, " style=\"stroke:");
    tba_ui_add(parts, colour);
    tba_ui_add(parts, "\" d=\"");
    tba_dom_push(parts, path_text);
    tba_ui_add(parts, "\"></path><path class=\"chart-hover-path\"");
    tba_chart_attributes(parts, id, parent, kind);
    tba_ui_add(parts, " data-series-index=\"");
    tba_dom_push(parts, index);
    tba_ui_add(parts, "\" d=\"");
    tba_dom_push(parts, path_text);
    tba_ui_add(parts, "\"></path>");
    if (!parent && !tba_ui_is(id, "actual")) {
        int point = tba_dom_item(points, tba_dom_count(points) - 1);
        tba_ui_add(parts, "<circle class=\"chart-point\" r=\"4.4\"");
        tba_chart_attributes(parts, id, 0, "");
        tba_ui_add(parts, " cx=\"");
        tba_chart_number(parts, tba_dom_number(tba_dom_get(point, "x")));
        tba_ui_add(parts, "\" cy=\"");
        tba_chart_number(parts, tba_dom_number(tba_dom_get(point, "y")));
        tba_ui_add(parts, "\" fill=\"");
        tba_ui_add(parts, colour);
        tba_ui_add(parts, "\"></circle>");
    }
}

static void tba_chart_x_tick(jsref parts, double x, jsref label, const char *anchor)
{
    tba_ui_add(parts, "<line class=\"chart-grid-line\" y1=\"24\" y2=\"378\" x1=\"");
    tba_chart_number(parts, x);
    tba_ui_add(parts, "\" x2=\"");
    tba_chart_number(parts, x);
    tba_ui_add(parts, "\"></line><text class=\"chart-axis-label\" y=\"412\" x=\"");
    tba_chart_number(parts, x);
    tba_ui_add(parts, "\" text-anchor=\"");
    tba_ui_add(parts, anchor);
    tba_ui_add(parts, "\">");
    tba_ui_escaped(parts, label);
    tba_ui_add(parts, "</text>");
}

static void tba_chart_axes(jsref parts, jsref labels, jsref data)
{
    for (unsigned i = 0; i < 5; ++i) {
        double value = tba_chart_y_tick(i), y = tba_chart_y(value);
        tba_ui_add(parts, "<line class=\"chart-grid-line\" x1=\"68\" x2=\"1012\" y1=\"");
        tba_chart_number(parts, y);
        tba_ui_add(parts, "\" y2=\"");
        tba_chart_number(parts, y);
        tba_ui_add(parts, "\"></line><text class=\"chart-axis-label\" x=\"58\" text-anchor=\"end\" y=\"");
        tba_chart_number(parts, y + 4);
        tba_ui_add(parts, "\">");
        tba_ui_escaped(parts, tba_chart_label(value));
        tba_ui_add(parts, "</text>");
    }
    unsigned count = tba_dom_count(labels), ticks = count < 6 ? count : 6;
    int previous = -1, timed = tba_chart_time_axis();
    for (unsigned i = 0; i < ticks; ++i) {
        int index = tba_chart_tick(i, ticks);
        if (index < 0 || index == previous)
            continue;
        previous = index;
        double x = tba_chart_x((unsigned)index);
        if (timed && x > 1011)
            continue;
        tba_chart_x_tick(parts, x, tba_dom_item(labels, (unsigned)index),
                         !i ? "start" : i == ticks - 1 && !timed ? "end" : "middle");
    }
    if (timed) {
        int label = tba_dom_get(data, "forecastEndLabel");
        if (!tba_dom_truth(label))
            label = tba_dom_date(tba_dom_numeric(tba_chart_end()), 3);
        tba_chart_x_tick(parts, 1012, label, "end");
        tba_ui_add(parts, "<line class=\"chart-forecast-end-line\" x1=\"1012\" x2=\"1012\" y1=\"24\" y2=\"378\">"
                          "</line><text class=\"chart-forecast-end-label\" x=\"1004\" y=\"40\" text-anchor=\"end\">"
                          "Forecast until</text>");
    }
    tba_ui_add(parts, "<path class=\"chart-axis\" d=\"M68,24V378H1012\"></path>");
}

static void tba_chart_include_values(jsref values, unsigned count)
{
    double *buffer = tba_chart_buffer(1);
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = tba_dom_mark();
        buffer[i] = tba_chart_value(values, i);
        tba_dom_release(mark);
    }
    tba_chart_include(count);
}

/* Render a bounded structured chart with shared scales and escaped labels. */
void tba_view_chart(jsref data)
{
    tba_chart_hover_index = -1;
    tba_ui_store("hover", 0);
    int labels = tba_dom_get(data, "labels"), dates = tba_dom_get(data, "dates");
    int series = tba_dom_get(data, "series"), actual = tba_dom_get(data, "actual");
    unsigned count = tba_dom_count(labels), models = tba_dom_count(series);
    int valid = count && count <= 32768 && models <= 6;
    if (valid) {
        double *buffer = tba_chart_buffer(0);
        for (unsigned i = 0; i < count; ++i) {
            unsigned mark = tba_dom_mark();
            buffer[i] = tba_chart_value(dates, i);
            tba_dom_release(mark);
        }
        valid = tba_chart_begin(count, tba_dom_number(tba_dom_get(data, "end")));
    }
    if (valid) {
        tba_chart_include_values(actual, count);
        for (unsigned i = 0; i < models; ++i) {
            unsigned mark = tba_dom_mark();
            int item = tba_dom_item(series, i);
            tba_chart_include_values(tba_dom_get(item, "values"), count);
            tba_chart_include_values(tba_dom_get(item, "lower"), count);
            tba_chart_include_values(tba_dom_get(item, "upper"), count);
            tba_dom_release(mark);
        }
        valid = tba_chart_finish();
    }
    if (!valid) {
        tba_ui_write("forecast-chart-body", "class", tba_dom_string("forecast-chart-body forecast-chart-empty"));
        tba_ui_write("forecast-chart-body", "textContent", tba_dom_string("No plottable forecast values were returned."));
        return;
    }
    int parts = tba_dom_object(1), hover = tba_dom_object(1);
    tba_ui_add(parts, "<div class=\"chart-legend\"><span class=\"chart-legend-title\">Legend</span>");
    tba_chart_legend(parts, tba_dom_string("actual"), tba_dom_string("Actual"), "rgba(49,20,61,0.9)", 1);
    for (unsigned i = 0; i < models; ++i) {
        unsigned mark = tba_dom_mark();
        tba_chart_legend(parts, tba_ui_text("model-", tba_dom_format(i, 0), ""),
                         tba_dom_get(tba_dom_item(series, i), "label"), tba_chart_colours[i], 1);
        tba_dom_release(mark);
    }
    tba_chart_legend(parts, tba_dom_string("confidence-lower"), tba_dom_string("Lower confidence"), "#cc6b19", 0);
    tba_chart_legend(parts, tba_dom_string("confidence-upper"), tba_dom_string("Upper confidence"), "#b33951", 0);
    tba_ui_add(parts, "</div><svg class=\"forecast-chart-svg\" viewBox=\"0 0 1040 430\" role=\"img\" "
                      "aria-label=\"Actual and forecast chart\"><rect width=\"1040\" height=\"430\" "
                      "fill=\"rgba(8,29,22,0.52)\"></rect>");
    tba_chart_axes(parts, labels, data);
    for (unsigned pass = 0; pass < 3; ++pass) {
        for (unsigned i = 0; i < models; ++i) {
            unsigned mark = tba_dom_mark();
            int item = tba_dom_item(series, i), id = tba_ui_text("model-", tba_dom_format(i, 0), "");
            int label = tba_dom_get(item, "label");
            if (pass < 2) {
                const char *kind = pass ? "upper" : "lower";
                tba_chart_series(parts, hover, labels, tba_dom_get(item, kind),
                    tba_dom_string(pass ? "confidence-upper" : "confidence-lower"),
                    tba_ui_text("", label, pass ? " upper confidence" : " lower confidence"),
                    tba_chart_colours[i], id, kind,
                    pass ? "chart-confidence-line chart-confidence-upper" : "chart-confidence-line");
            } else {
                tba_chart_series(parts, hover, labels, tba_dom_get(item, "values"), id, label,
                                  tba_chart_colours[i], 0, "", "chart-model-line");
            }
            tba_dom_release(mark);
        }
    }
    tba_chart_series(parts, hover, labels, actual, tba_dom_string("actual"), tba_dom_string("Actual"),
                      "rgba(49,20,61,0.9)", 0, "", "chart-actual-line");
    tba_ui_add(parts, "<circle class=\"chart-hover-marker\" r=\"5.5\" visibility=\"hidden\"></circle>"
                      "</svg><div class=\"chart-tooltip hidden\"></div>");
    tba_ui_store("hover", hover);
    tba_ui_write("forecast-chart-body", "class", tba_dom_string("forecast-chart-body"));
    tba_ui_write("forecast-chart-body", "innerHTML", tba_ui_join(parts));
    tba_view_visibility();
}

/* Index legend selections once, then apply parent and confidence visibility. */
void tba_view_visibility(void)
{
    int body = tba_dom_id("forecast-chart-body"), set = tba_dom_object(0);
    int inputs = tba_dom_all(body, "input[name='chart-series-toggle']");
    for (unsigned i = 0; i < tba_dom_count(inputs); ++i) {
        unsigned mark = tba_dom_mark();
        int node = tba_dom_item(inputs, i), checked = tba_dom_truth(tba_dom_get(node, "checked"));
        tba_dom_key_set(set, tba_dom_read(node, 2, "data-series-id"), tba_dom_numeric(checked));
        tba_dom_class(tba_dom_closest(node, ".chart-legend-item"), "is-off", !checked);
        tba_dom_release(mark);
    }
    int nodes = tba_dom_all(body, "[data-chart-series-id]");
    for (unsigned i = 0; i < tba_dom_count(nodes); ++i) {
        unsigned mark = tba_dom_mark();
        int node = tba_dom_item(nodes, i), parent = tba_dom_read(node, 2, "data-chart-parent-id");
        int visible = tba_dom_truth(tba_dom_key_get(set, tba_dom_read(node, 2, "data-chart-series-id")));
        if (tba_dom_truth(parent))
            visible = visible && tba_dom_truth(tba_dom_key_get(set, parent));
        tba_dom_write(node, 4, "display", tba_dom_string(visible ? "" : "none"));
        tba_dom_release(mark);
    }
}

/* Clear hover presentation when leaving the chart or hiding a series. */
void tba_view_leave(void)
{
    tba_dom_class(tba_dom_query(0, ".chart-tooltip"), "hidden", 1);
    tba_dom_write(tba_dom_query(0, ".chart-hover-marker"), 2, "visibility", tba_dom_string("hidden"));
}

static double tba_chart_clamp(double value, double minimum, double maximum)
{
    if (maximum < minimum)
        maximum = minimum;
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

/* Reuse numeric hover buffers until the pointer changes series. */
void tba_view_hover(event_jsref event)
{
    int path = tba_dom_closest(tba_dom_get(event, "target"), ".chart-hover-path");
    if (!path) {
        tba_view_leave();
        return;
    }
    int index = (int)tba_dom_number(tba_dom_read(path, 2, "data-series-index"));
    int series = tba_dom_item(tba_ui_state("hover"), (unsigned)index), points = tba_dom_get(series, "points");
    unsigned count = tba_dom_count(points);
    if (!count || count > 32768)
        return;
    if (index != tba_chart_hover_index) {
        for (unsigned i = 0; i < count; ++i) {
            unsigned mark = tba_dom_mark();
            int point = tba_dom_item(points, i);
            tba_chart_buffer(2)[i] = tba_dom_number(tba_dom_get(point, "x"));
            tba_chart_buffer(3)[i] = tba_dom_number(tba_dom_get(point, "y"));
            tba_dom_release(mark);
        }
        tba_chart_hover_index = index;
    }
    int svg = tba_dom_query(0, ".forecast-chart-svg");
    int nearest = tba_chart_nearest(count, tba_dom_cursor(svg, event, 0), tba_dom_cursor(svg, event, 1));
    if (nearest < 0) {
        tba_view_leave();
        return;
    }
    int point = tba_dom_item(points, (unsigned)nearest), marker = tba_dom_query(svg, ".chart-hover-marker");
    tba_dom_write(marker, 2, "visibility", tba_dom_string("visible"));
    tba_dom_write(marker, 2, "cx", tba_dom_get(point, "x"));
    tba_dom_write(marker, 2, "cy", tba_dom_get(point, "y"));
    tba_dom_write(marker, 2, "fill", tba_dom_get(series, "colour"));
    int tip = tba_dom_query(0, ".chart-tooltip"), parts = tba_dom_object(1);
    tba_ui_add(parts, "<div class=\"chart-tooltip-title\">");
    tba_ui_escaped(parts, tba_dom_get(series, "label"));
    tba_ui_add(parts, "</div><div><strong>");
    tba_ui_escaped(parts, tba_dom_get(point, "date"));
    tba_ui_add(parts, "</strong></div><div class=\"chart-tooltip-meta\">Value: ");
    tba_ui_escaped(parts, tba_chart_label(tba_dom_number(tba_dom_get(point, "value"))));
    tba_ui_add(parts, "</div>");
    tba_dom_write(tip, 1, "", tba_ui_join(parts));
    int body = tba_dom_id("forecast-chart-body");
    double left = tba_dom_number(tba_dom_get(event, "clientX")) - tba_dom_measure(body, 0);
    double top = tba_dom_number(tba_dom_get(event, "clientY")) - tba_dom_measure(body, 1);
    tba_dom_write(tip, 4, "left", tba_ui_text("", tba_dom_format(
        tba_chart_clamp(left, 8, tba_dom_measure(body, 2) - 180), 2), "px"));
    tba_dom_write(tip, 4, "top", tba_ui_text("", tba_dom_format(
        tba_chart_clamp(top, 34, tba_dom_measure(body, 3) - 34), 2), "px"));
    tba_dom_class(tip, "hidden", 0);
}
