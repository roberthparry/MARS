/**
 * @file tba_form.c
 * @brief Worksheet state, selections and metadata controls implemented in C/WASM.
 *
 * Uses scoped DOM capabilities for rendering. The native model and civil-period
 * modules own suggestions and arithmetic; no forecast values are parsed here.
 */
#include "tba_ui.h"
#include "tba_model.h"
#include "tba_period.h"
#include "tba_request.h"
#include "tba_form.h"

static unsigned tba_form_edited;
static int tba_form_season_edited, tba_form_hydrating;
static const char *const tba_form_orders[] = {"p", "d", "q", "P", "D", "Q"};
static const char *const tba_form_fields[] = {
    "target_path", "target_display_name", "target_date_column", "target_value_column",
    "xreg_path", "xreg_display_name", "xreg_date_column", "xreg_columns", "outlier_mode", "outlier_dates",
    "models", "frequency", "year_type", "forecast_end_date", "p", "d", "q", "P", "D", "Q",
    "season_period", "criterion", "level"
};
static const char *const tba_form_models[] = {"regression", "arima", "arimax", "sarima", "sarimax", "auto-arima"};
static const char *const tba_form_labels[] = {"Regression", "ARIMA", "ARIMAX", "SARIMA", "SARIMAX", "Auto-ARIMA"};

static unsigned tba_form_code(const char *group, jsref key)
{
    return (unsigned)tba_dom_number(tba_dom_key_get(tba_ui_state(group), tba_dom_clean(key, 1)));
}

static unsigned tba_form_strength(void)
{
    return tba_form_code("strengths", tba_dom_get(tba_ui_state("target_meta"), "seasonality_strength"));
}

static unsigned tba_form_mask(void)
{
    int selected = tba_ui_checked("input[name='model-choice']:checked");
    unsigned mask = 0;
    for (unsigned i = 0; i < tba_dom_count(selected); ++i) {
        unsigned code = tba_form_code("models", tba_dom_item(selected, i));
        if (code)
            mask |= 1u << (code - 1);
    }
    return mask;
}

static void tba_form_option(jsref parts, jsref value, jsref label, int selected)
{
    tba_ui_add(parts, "<option value=\"");
    tba_ui_escaped(parts, value);
    tba_ui_add(parts, selected ? "\" selected>" : "\">");
    tba_ui_escaped(parts, label);
    tba_ui_add(parts, "</option>");
}

static void tba_form_select(const char *field, jsref columns, jsref fallback, const char *empty)
{
    int node = tba_ui_field(field), selected = tba_ui_value(field), parts = tba_dom_object(1);
    if (!tba_dom_truth(selected))
        selected = fallback;
    int matched = 0;
    unsigned count = tba_dom_count(columns);
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = tba_dom_mark();
        int value = tba_dom_item(columns, i);
        int match = tba_dom_equal(value, selected);
        matched |= match;
        tba_form_option(parts, value, value, match);
        tba_dom_release(mark);
    }
    if (!count) {
        int label = tba_ui_or(selected, empty);
        tba_form_option(parts, tba_dom_text(selected), label, 1);
    }
    tba_dom_write(node, 1, "", tba_ui_join(parts));
    tba_dom_write(node, 5, "", matched || !count ? tba_dom_text(selected) : tba_dom_item(columns, 0));
}

static void tba_form_seasons(void)
{
    unsigned frequency = tba_form_code("frequencies", tba_ui_value("frequency"));
    int current = tba_ui_integer(tba_ui_value("season_period"));
    int parts = tba_dom_object(1);
    for (unsigned i = 0; i < tba_model_option_count(frequency); ++i) {
        int value = tba_model_option_value(frequency, i);
        tba_form_option(parts, tba_dom_format(value, -1),
                        tba_dom_string(tba_model_option_label(frequency, i)), value == current);
    }
    tba_dom_write(tba_ui_field("season_period"), 1, "", tba_ui_join(parts));
}

static void tba_form_horizon(void)
{
    int meta = tba_ui_state("target_meta"), drivers = tba_ui_state("driver_meta");
    int target_text = tba_dom_get(meta, "usable_end_date_iso");
    if (!tba_dom_truth(target_text))
        target_text = tba_dom_get(meta, "end_date_iso");
    int driver_text = tba_dom_get(drivers, "usable_end_date_iso");
    if (!tba_dom_truth(driver_text))
        driver_text = tba_dom_get(drivers, "end_date_iso");
    int target = tba_ui_integer(tba_dom_date(target_text, 0));
    int driver = tba_ui_integer(tba_dom_date(driver_text, 0));
    unsigned frequency = tba_form_code("frequencies", tba_ui_value("frequency"));
    unsigned count = tba_period_options(target, driver, frequency, tba_ui_is(tba_ui_value("year_type"), "fiscal"));
    int parts = tba_dom_object(1), current = tba_ui_value("forecast_end_date"), last = 0;
    int matched = 0, year = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = tba_dom_mark();
        int date = tba_period_option(i), next_year = date / 10000;
        if (year != next_year) {
            if (year)
                tba_ui_add(parts, "</optgroup>");
            year = next_year;
            tba_ui_add(parts, "<optgroup label=\"");
            tba_dom_push(parts, tba_dom_format(year, 0));
            tba_ui_add(parts, "\">");
        }
        int iso = tba_dom_date(tba_dom_numeric(date), 1);
        int match = tba_dom_equal(current, iso);
        matched |= match;
        tba_form_option(parts, iso, tba_dom_date(tba_dom_numeric(date), 2), match);
        last = date;
        tba_dom_release(mark);
    }
    if (year)
        tba_ui_add(parts, "</optgroup>");
    if (!count)
        tba_form_option(parts, tba_dom_string(""), tba_dom_string("Choose a forecast end date"), 1);
    tba_dom_write(tba_ui_field("forecast_end_date"), 1, "", tba_ui_join(parts));
    if (count)
        tba_ui_set("forecast_end_date", matched ? current : tba_dom_date(tba_dom_numeric(last), 1));
}

static void tba_form_sync(unsigned kind)
{
    static const char *const selectors[] = {
        "input[name='model-choice']:checked", "input[name='xreg-column-choice']:checked",
        "input[name='outlier-choice']:checked"
    };
    static const char *const fields[] = {"models", "xreg_columns", "outlier_dates"};
    int selected = tba_ui_checked(selectors[kind]);
    if (!kind && !tba_dom_count(selected)) {
        int first = tba_dom_query(0, "input[name='model-choice']");
        tba_dom_flag(first, "checked", 1);
        if (first)
            tba_dom_push(selected, tba_dom_read(first, 5, ""));
    }
    int text = tba_dom_join(selected, ", ");
    unsigned count = tba_dom_count(selected);
    tba_ui_set(fields[kind], text);
    if (kind == 0) {
        int labels = tba_dom_object(1);
        for (unsigned i = 0; i < count; ++i) {
            unsigned code = tba_form_code("models", tba_dom_item(selected, i));
            tba_dom_push(labels, code ? tba_dom_string(tba_form_labels[code - 1]) : tba_dom_item(selected, i));
        }
        tba_ui_write("model-summary", "textContent", tba_ui_text("Selected: ", tba_dom_join(labels, ", "), ""));
        tba_ui_write("model-summary", "title", text);
    } else if (kind == 1) {
        tba_ui_write("xreg-columns-summary", "textContent", tba_ui_text("Selected: ", tba_ui_or(text, "none yet"), ""));
        tba_ui_write("xreg-columns-summary", "title", text);
        tba_ui_write("xreg-columns-status", "textContent", count ?
                     tba_ui_text("", tba_dom_format(count, 0),
                                 count == 1 ? " exogenous column selected." : " exogenous columns selected.") :
                     tba_dom_string("Choose one or more driver columns from the list."));
        tba_ui_write("xreg-columns-debug", "textContent", tba_ui_text("Will send: ", tba_ui_or(text, "(none)"), ""));
    } else {
        tba_ui_write("outlier-status", "textContent", count ?
                     tba_ui_text("", tba_dom_format(count, 0), " outliers selected for possible handling.") :
                     tba_dom_string("No outliers selected for handling."));
    }
}

static void tba_form_checkbox(jsref parts, const char *name, jsref value, jsref label,
                              jsref title, int selected, int usable)
{
    tba_ui_add(parts, usable ? "<label class=\"multi-select-item\" title=\"" :
                              "<label class=\"multi-select-item is-disabled\" title=\"");
    tba_ui_escaped(parts, title);
    tba_ui_add(parts, "\"><input type=\"checkbox\" name=\"");
    tba_ui_add(parts, name);
    tba_ui_add(parts, "\" value=\"");
    tba_ui_escaped(parts, value);
    tba_ui_add(parts, "\"");
    if (selected && usable)
        tba_ui_add(parts, " checked");
    if (!usable)
        tba_ui_add(parts, " disabled");
    tba_ui_add(parts, "><span>");
    tba_ui_escaped(parts, label);
    tba_ui_add(parts, "</span></label>");
}

static void tba_form_outliers(jsref outliers)
{
    int set = tba_ui_selection(tba_ui_value("outlier_dates")), parts = tba_dom_object(1);
    int all = !tba_dom_count(tba_dom_keys(set));
    unsigned count = tba_dom_count(outliers);
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = tba_dom_mark();
        int item = tba_dom_item(outliers, i), date = tba_dom_get(item, "date_iso");
        double score = tba_dom_number(tba_dom_get(item, "score"));
        if (score < 0)
            score = -score;
        const char *likelihood = score >= 20 ? "extremely likely" : score >= 10 ? "very likely" :
                                 score >= 6 ? "likely" : "possibly";
        int label = tba_dom_object(1);
        tba_dom_push(label, tba_dom_get(item, "date"));
        tba_ui_add(label, ": ");
        tba_dom_push(label, tba_dom_get(item, "value"));
        tba_ui_add(label, " (");
        tba_ui_add(label, likelihood);
        tba_ui_add(label, ")");
        int text = tba_ui_join(label);
        tba_form_checkbox(parts, "outlier-choice", date, text, text, all || tba_dom_truth(tba_dom_key_get(set, date)), 1);
        tba_dom_release(mark);
    }
    if (!count)
        tba_ui_add(parts, "<div class=\"multi-select-placeholder\">No target points are currently flagged here.</div>");
    tba_ui_write("outlier-list", "innerHTML", tba_ui_join(parts));
    tba_dom_class(tba_dom_query(0, ".outlier-panel"), "no-outliers", !count);
    tba_form_sync(2);
    if (!count)
        tba_ui_write("outlier-status", "textContent", tba_dom_string("No outliers detected."));
}

/* Refresh target options or the indexed driver checkbox list. */
void tba_form_columns(jsref columns, int driver)
{
    if (!driver) {
        tba_form_select("target_value_column", columns, 0, "Choose a column");
        tba_dom_write(tba_ui_field("target_value_column"), 2, "title", tba_ui_value("target_value_column"));
        tba_ui_write("target-picker-status", "textContent",
                     tba_ui_text("Chosen target: ", tba_ui_value("target_value_column"), ""));
        return;
    }
    int set = tba_ui_selection(tba_ui_value("xreg_columns")), parts = tba_dom_object(1);
    for (unsigned i = 0; i < tba_dom_count(columns); ++i) {
        unsigned mark = tba_dom_mark();
        int column = tba_dom_item(columns, i), object = tba_dom_type(column) == 5;
        int name = object ? tba_dom_get(column, "name") : column;
        int usable = !object || tba_dom_truth(tba_dom_get(column, "usable"));
        int reason = object ? tba_dom_get(column, "reason") : 0;
        tba_form_checkbox(parts, "xreg-column-choice", name, name, tba_dom_truth(reason) ? reason : name,
                          tba_dom_truth(tba_dom_key_get(set, name)), usable);
        tba_dom_release(mark);
    }
    if (!tba_dom_count(columns))
        tba_ui_add(parts, "<div class=\"multi-select-placeholder\">Choose a local CSV to select additional columns.</div>");
    tba_ui_write("xreg-columns-list", "innerHTML", tba_ui_join(parts));
    tba_form_sync(1);
}

/* Local selection markers and display names never resolve server paths. */
void tba_form_upload(jsref path, jsref name, int driver)
{
    tba_ui_set(driver ? "xreg_display_name" : "target_display_name", name);
    if (!tba_dom_truth(name)) {
        int parts = tba_dom_split(path, "/");
        name = tba_dom_count(parts) ? tba_dom_item(parts, tba_dom_count(parts) - 1) : 0;
    }
    name = tba_ui_or(name, "No file chosen");
    tba_ui_set(driver ? "xreg_path" : "target_path", path);
    tba_ui_write(driver ? "xreg-upload-name" : "target-upload-name", "textContent", name);
    tba_ui_write(driver ? "xreg-path-readout" : "target-path-readout", "value", name);
    tba_ui_write(driver ? "xreg-path-readout" : "target-path-readout", "title", name);
}

static void tba_form_meta_line(jsref parts, const char *label, jsref value)
{
    tba_ui_add(parts, "<div><strong>");
    tba_ui_add(parts, label);
    tba_ui_add(parts, ":</strong> ");
    tba_ui_escaped(parts, tba_ui_or(value, "Unknown"));
    tba_ui_add(parts, "</div>");
}

/* Store metadata as values and render all user text through escaping primitives. */
void tba_form_meta(jsref data, int driver)
{
    tba_ui_store(driver ? "driver_meta" : "target_meta", data);
    int parts = tba_dom_object(1);
    tba_form_meta_line(parts, "Detected frequency", tba_dom_get(data, "detected_frequency_label"));
    tba_form_meta_line(parts, "Date column", tba_dom_get(data, "date_column"));
    if (!driver)
        tba_form_meta_line(parts, "Series", tba_dom_get(data, "value_column"));
    int range = tba_dom_object(1);
    tba_dom_push(range, tba_dom_get(data, "start_date"));
    tba_ui_add(range, " to ");
    tba_dom_push(range, tba_dom_get(data, "end_date"));
    tba_form_meta_line(parts, "File date range", tba_ui_join(range));
    int usable = tba_dom_object(1);
    tba_dom_push(usable, tba_dom_get(data, "usable_start_date"));
    tba_ui_add(usable, " to ");
    tba_dom_push(usable, tba_dom_get(data, "usable_end_date"));
    tba_form_meta_line(parts, "Usable series range", tba_ui_join(usable));
    int season = tba_dom_get(data, "seasonality_label");
    if (tba_dom_truth(season)) {
        int line = tba_dom_object(1);
        tba_dom_push(line, season);
        tba_ui_add(line, " (score ");
        tba_dom_push(line, tba_dom_get(data, "seasonality_score"));
        tba_ui_add(line, ", lag ");
        tba_dom_push(line, tba_dom_get(data, "seasonality_lag"));
        tba_ui_add(line, ")");
        static const char *const classes[] = {
            "seasonality-none", "seasonality-none", "seasonality-weak", "seasonality-moderate",
            "seasonality-strong", "seasonality-very-strong"
        };
        unsigned strength = tba_form_code("strengths", tba_dom_get(data, "seasonality_strength"));
        tba_ui_add(parts, "<div class=\"seasonality-line\"><strong>Seasonality:</strong> "
                          "<span class=\"seasonality-badge ");
        tba_ui_add(parts, classes[strength < 6 ? strength : 0]);
        tba_ui_add(parts, "\">");
        tba_ui_escaped(parts, tba_ui_join(line));
        tba_ui_add(parts, "</span></div>");
    }
    tba_ui_write(driver ? "xreg-meta" : "target-meta", "innerHTML", tba_ui_join(parts));
    tba_form_select(driver ? "xreg_date_column" : "target_date_column", tba_dom_get(data, "date_candidates"),
                    tba_dom_get(data, "date_column"), "No date columns found");
    if (!driver) {
        tba_form_outliers(tba_dom_get(data, "outliers"));
        int frequency = tba_dom_get(data, "detected_frequency");
        if (tba_dom_truth(frequency)) {
            tba_ui_set("frequency", frequency);
            tba_ui_write("detected-frequency-readout", "value", tba_dom_get(data, "detected_frequency_label"));
            tba_form_seasons();
            if (!tba_form_hydrating)
                tba_form_suggest(0);
        }
    }
    tba_form_horizon();
    tba_form_warning();
}

/* Render the native policy advisory without a second implementation in JS. */
void tba_form_warning(void)
{
    unsigned code = tba_model_warning(tba_form_mask(), tba_form_strength(),
        tba_ui_integer(tba_dom_get(tba_ui_state("target_meta"), "seasonality_lag")),
        tba_ui_integer(tba_ui_value("season_period")), tba_ui_integer(tba_ui_value("P")),
        tba_ui_integer(tba_ui_value("D")), tba_ui_integer(tba_ui_value("Q")));
    tba_ui_write("seasonal-setup-warning", "textContent", code ?
                 tba_ui_text("Seasonal settings check: ", tba_dom_string(tba_model_warning_text(code)), "") :
                 tba_dom_string(""));
    tba_ui_write("seasonal-setup-warning", "class",
                 tba_ui_text("setup-advisory ", tba_dom_string(code ? tba_model_warning_class(code) : "hidden"), ""));
}

/* Respect edited fields while applying the shared suggestion policy. */
void tba_form_suggest(int force)
{
    if (force) {
        tba_form_edited = 0;
        tba_form_season_edited = 0;
    }
    unsigned strength = tba_form_strength(), mask = tba_form_mask();
    unsigned model = tba_model_preferred(mask, strength);
    int lag = tba_ui_integer(tba_dom_get(tba_ui_state("target_meta"), "seasonality_lag"));
    int clear = strength >= 3 && lag > 1 && (mask & 56u);
    tba_form_seasons();
    for (unsigned i = 0; i < 6; ++i) {
        int current = tba_ui_value(tba_form_orders[i]);
        if (tba_model_replace(force, clear, !!(tba_form_edited & (1u << i)), !tba_dom_truth(current), tba_ui_is(current, "0")))
            tba_ui_set(tba_form_orders[i], tba_dom_format(tba_model_order(model, strength, i), -1));
    }
    if (force || clear || !tba_form_season_edited || !tba_dom_truth(tba_ui_value("season_period"))) {
        int period = tba_model_season(tba_form_code("frequencies", tba_ui_value("frequency")), strength, lag);
        if (period >= 0)
            tba_ui_set("season_period", tba_dom_format(period, -1));
    }
    static const char *const names[] = {"criterion", "level"};
    static const char *const defaults[] = {"aic", "0.95"};
    for (unsigned i = 0; i < 2; ++i) {
        int current = tba_ui_value(names[i]);
        if (tba_model_replace(force, clear, 1, !tba_dom_truth(current), tba_ui_is(current, "0")))
            tba_ui_set(names[i], tba_dom_string(defaults[i]));
    }
    tba_form_warning();
}

/* Collect recognised scalar fields only; unchecked checkboxes never overwrite selections. */
jsref tba_form_collect(void)
{
    tba_form_sync(0);
    tba_form_sync(1);
    tba_form_sync(2);
    int payload = tba_dom_object(0);
    for (unsigned i = 0; i < sizeof tba_form_fields / sizeof *tba_form_fields; ++i) {
        unsigned mark = tba_dom_mark();
        tba_dom_set(payload, tba_form_fields[i], tba_ui_value(tba_form_fields[i]));
        tba_dom_release(mark);
    }
    return payload;
}

/* Event names determine which dependent native controls need refreshing. */
void tba_form_change(event_jsref event)
{
    int target = tba_dom_get(event, "target"), name = tba_dom_get(target, "name");
    int id = tba_dom_get(target, "id");
    if (tba_ui_is(id, "target-upload") || tba_ui_is(id, "xreg-upload")) {
        if (tba_ui_is(tba_dom_get(event, "type"), "change"))
            tba_request_upload(tba_ui_is(id, "xreg-upload"));
        return;
    }
    for (unsigned i = 0; i < 6; ++i)
        if (tba_ui_is(name, tba_form_orders[i]))
            tba_form_edited |= 1u << i;
    if (tba_ui_is(name, "season_period"))
        tba_form_season_edited = 1;
    if (tba_ui_is(name, "model-choice")) {
        tba_form_sync(0);
        tba_form_suggest(0);
    }
    if (tba_ui_is(name, "xreg-column-choice"))
        tba_form_sync(1);
    if (tba_ui_is(name, "outlier-choice"))
        tba_form_sync(2);
    if (tba_ui_is(tba_dom_get(event, "type"), "change")) {
        if (tba_ui_is(name, "target_value_column") || tba_ui_is(name, "target_date_column"))
            tba_request_metadata(0);
        if (tba_ui_is(name, "xreg_date_column") || tba_ui_is(name, "xreg-column-choice"))
            tba_request_metadata(1);
    }
    if (tba_ui_is(name, "year_type") || tba_ui_is(name, "frequency"))
        tba_form_horizon();
    tba_form_warning();
    tba_dom_timer(TBA_SAVE, 250);
}

static void tba_form_index(const char *key, const char *const *names, unsigned count)
{
    int index = tba_dom_object(0);
    for (unsigned i = 0; i < count; ++i)
        tba_dom_set(index, names[i], tba_dom_numeric(i + 1));
    tba_ui_store(key, index);
}

/* Bootstrap exactly once from typed server configuration. */
void tba_form_init(jsref config)
{
    static const char *const frequencies[] = {"daily", "monthly", "quarterly", "yearly"};
    static const char *const strengths[] = {"none", "weak", "moderate", "strong", "very strong"};
    tba_form_index("frequencies", frequencies, 4);
    tba_form_index("strengths", strengths, 5);
    tba_form_index("models", tba_form_models, 6);
    int state = tba_dom_get(config, "state");
    tba_form_hydrating = 1;
    for (unsigned i = 0; i < sizeof tba_form_fields / sizeof *tba_form_fields; ++i)
        tba_ui_set(tba_form_fields[i], tba_dom_get(state, tba_form_fields[i]));
    int selected = tba_ui_selection(tba_dom_get(state, "models")), inputs = tba_dom_all(0, "input[name='model-choice']");
    for (unsigned i = 0; i < tba_dom_count(inputs); ++i) {
        int input = tba_dom_item(inputs, i);
        tba_dom_flag(input, "checked", tba_dom_truth(tba_dom_key_get(selected, tba_dom_read(input, 5, ""))));
    }
    tba_form_sync(0);
    tba_form_upload(tba_ui_value("target_path"), tba_ui_value("target_display_name"), 0);
    tba_form_upload(tba_ui_value("xreg_path"), tba_ui_value("xreg_display_name"), 1);
    tba_form_meta(tba_dom_get(config, "target_meta"), 0);
    tba_form_meta(tba_dom_get(config, "xreg_meta"), 1);
    tba_form_columns(tba_dom_get(config, "target_columns"), 0);
    tba_form_columns(tba_dom_get(config, "xreg_columns"), 1);
    tba_ui_set("frequency", tba_dom_get(state, "frequency"));
    tba_ui_set("year_type", tba_dom_get(state, "year_type"));
    tba_form_hydrating = 0;
    tba_form_suggest(0);
    if (!tba_dom_count(tba_dom_get(config, "target_columns")))
        tba_request_metadata(0);
    if (!tba_dom_count(tba_dom_get(config, "xreg_columns")))
        tba_request_metadata(1);
}
