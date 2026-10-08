/**
 * @file tba_forecast.c
 * @brief Validated, bounded native-worker orchestration for forecasting requests.
 *
 * Uses independent private fit inputs and argument vectors, with a 60-second
 * limit per model. Fit copies are removed on every exit path; uploaded originals
 * remain untouched. The existing worker continues to own mathematical fitting.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "lab_process.h"
#include "tba_forecast.h"
#include "tba_series.h"

static const char *const tba_forecast_tokens[] = {"regression", "arima", "arimax", "sarima", "sarimax", "auto-arima"};
static const char *const tba_forecast_labels[] = {"Regression", "ARIMA", "ARIMAX", "SARIMA", "SARIMAX", "Auto-ARIMA"};

/* Enumerate the fixed native-worker vocabulary. */
const char *tba_forecast_model(size_t index)
{
    return index < 6 ? tba_forecast_tokens[index] : NULL;
}

/* Resolve one of six presentation labels. */
const char *tba_forecast_label(const char *token)
{
    for (size_t i = 0; i < 6; ++i)
        if (token && !strcmp(token, tba_forecast_tokens[i]))
            return tba_forecast_labels[i];
    return token ? token : "Forecast";
}

/* Normalise a bounded, validated model selection in stable display order. */
json_t *tba_forecast_models(const char *selection)
{
    string_t *text = string_new_with(selection && *selection ? selection : "sarimax");
    size_t count = 0;
    string_t **parts = text ? string_split(text, ",", &count) : NULL;
    bool selected[6] = {false};
    bool ok = text && count <= 32;
    for (size_t p = 0; ok && p < count; ++p) {
        string_trim(parts[p]);
        string_to_lower(parts[p]);
        bool found = !string_byte_length(parts[p]);
        for (size_t i = 0; i < 6; ++i)
            if (string_view_equals_literal(string_view_all(parts[p]), tba_forecast_tokens[i])) {
                selected[i] = true;
                found = true;
                break;
            }
        ok = found;
    }
    json_t *result = ok ? json_new_array() : NULL;
    for (size_t i = 0; result && i < 6; ++i)
        if (selected[i] && !tba_json_append(result, tba_forecast_tokens[i])) {
            json_free(result);
            result = NULL;
        }
    string_split_free(parts, count);
    string_free(text);
    if (result && !json_array_size(result)) {
        json_free(result);
        result = NULL;
    }
    return result;
}

static bool tba_forecast_numbers(const json_t *state)
{
    static const char *const keys[] = {"p", "d", "q", "P", "D", "Q", "season_period", "level"};
    for (size_t i = 0; i < sizeof keys / sizeof *keys; ++i) {
        string_t *text = string_new_with(tba_json_text(state, keys[i]));
        double value;
        bool ok = text && tba_text_number(text, &value);
        string_free(text);
        if (!ok || (i == 7 ? value <= 0 || value >= 1 :
                     value < 0 || value != floor(value) || value > (i == 6 ? 365 : 20)))
            return false;
    }
    const char *criterion = tba_json_text(state, "criterion"), *year = tba_json_text(state, "year_type");
    return (!strcmp(criterion, "aic") || !strcmp(criterion, "bic")) &&
           (!strcmp(year, "fiscal") || !strcmp(year, "calendar"));
}

static void tba_forecast_remove(string_t *path)
{
    if (path) {
        file_t *file = file_new(path);
        if (file)
            file_delete(file);
        file_free(file);
        string_free(path);
    }
}

static json_t *tba_forecast_one(tba_app_t *app, const json_t *state, const char *model)
{
    json_t *result = NULL, *meta = NULL, *ends = NULL, *overlap = NULL;
    tba_series_t *target = NULL, *drivers = NULL;
    string_t *target_fit = NULL, *driver_fit = NULL, *note = NULL, *fit_csv = NULL;
    string_t *driver_csv = NULL, *driver_note = NULL, *output = NULL, *horizon_text = NULL;
    const char *error = "The selected target CSV or date column could not be read.";
    target = tba_series_open(tba_json_text(state, "target_path"), tba_json_text(state, "target_date_column"));
    if (!target)
        goto done;
    meta = tba_series_details(target, tba_json_text(state, "target_value_column"), "");
    if (!meta)
        goto done;
    const char *selection = tba_json_text(state, "xreg_columns");
    if (*selection && *tba_json_text(state, "xreg_path")) {
        drivers = tba_series_open(tba_json_text(state, "xreg_path"), tba_json_text(state, "xreg_date_column"));
        error = "The selected exogenous CSV or date column could not be read.";
        if (!drivers)
            goto done;
    }
    overlap = tba_series_overlap(drivers, selection);
    error = "Choose at most 32 valid driver columns.";
    if (!overlap)
        goto done;
    const char *frequency = tba_json_text(meta, "detected_frequency");
    if (!strcmp(frequency, "unknown"))
        frequency = tba_json_text(state, "frequency");
    if (!tba_json_string(meta, "detected_frequency", frequency))
        goto done;
    frequency = tba_json_text(meta, "detected_frequency");
    bool fiscal = !strcmp(tba_json_text(state, "year_type"), "fiscal");
    ends = tba_series_ends(meta, drivers, selection, fiscal);
    error = "No usable future forecast periods are available.";
    if (!ends || !json_array_size(ends))
        goto done;
    int end = tba_date_cstr(tba_json_text(state, "forecast_end_date"));
    if (!end) {
        size_t index = json_array_size(ends) < 6 ? json_array_size(ends) - 1 : 5;
        end = tba_date_parse(json_string_value(json_array_get(ends, index)));
    }
    end = tba_date_end(end, frequency, fiscal);
    if (drivers && *selection) {
        bool found = false;
        for (size_t i = 0; i < json_array_size(ends); ++i)
            found = found || tba_date_parse(json_string_value(json_array_get(ends, i))) == end;
        error = "The selected forecast end has no fully usable exogenous observations.";
        if (!found)
            goto done;
    }
    int last = tba_date_cstr(tba_json_text(meta, "usable_end_date_iso"));
    size_t horizon = tba_date_horizon(last, end, frequency, fiscal);
    error = "Choose a forecast end after the last usable target observation (at most 1200 periods).";
    if (!last || !horizon || horizon > 1200)
        goto done;
    horizon_text = string_sprintf("%zu", horizon);
    fit_csv = tba_series_adjust(target, tba_json_text(state, "target_value_column"),
                                tba_json_text(state, "outlier_mode"), tba_json_text(state, "outlier_dates"), &note);
    target_fit = fit_csv ? tba_app_upload(app, fit_csv) : NULL;
    if (drivers) {
        driver_csv = tba_series_adjust(drivers, "", "flag", "", &driver_note);
        driver_fit = driver_csv ? tba_app_upload(app, driver_csv) : NULL;
    }
    error = "Could not prepare private forecast input copies.";
    if (!target_fit || !horizon_text || (drivers && !driver_fit))
        goto done;
    const char *argv[] = {tba_app_binary(app), "--target", string_c_str(target_fit),
        "--target-date-column", tba_json_text(meta, "date_column"),
        "--target-value-column", tba_json_text(state, "target_value_column"),
        "--xreg", driver_fit ? string_c_str(driver_fit) : "", "--xreg-date-column", tba_json_text(state, "xreg_date_column"),
        "--xreg-cols", selection, "--model", model, "--frequency", frequency,
        "--year-type", tba_json_text(state, "year_type"), "--horizon", string_c_str(horizon_text),
        "--p", tba_json_text(state, "p"), "--d", tba_json_text(state, "d"), "--q", tba_json_text(state, "q"),
        "--P", tba_json_text(state, "P"), "--D", tba_json_text(state, "D"), "--Q", tba_json_text(state, "Q"),
        "--season-period", tba_json_text(state, "season_period"), "--criterion", tba_json_text(state, "criterion"),
        "--level", tba_json_text(state, "level"), NULL};
    int status = -1;
    error = "The native forecast worker failed, timed out or exceeded its output limit.";
    if (!lab_proc_run(argv, NULL, 60000, 16u * 1024u * 1024u, &output, &status))
        goto done;
    result = json_from_text(output);
    if (json_type(result) != JSON_OBJECT || (status && tba_json_bool(result, "ok"))) {
        json_free(result);
        result = NULL;
        goto done;
    }
    string_t *summary = string_new_with(tba_json_text(result, "summary_text"));
    if (summary && note && string_byte_length(note))
        string_append_format(summary, "\n%S", note);
    if (summary && json_object_size(overlap))
        string_append_format(summary, "\n%s: %s %s", tba_json_text(overlap, "title"),
                             tba_json_text(overlap, "message"), tba_json_text(overlap, "action"));
    json_t *plausibility = tba_forecast_plausibility(tba_json_text(result, "forecast_csv"));
    if (summary && json_object_size(plausibility))
        string_append_format(summary, "\n%s: %s %s", tba_json_text(plausibility, "title"),
                             tba_json_text(plausibility, "message"), tba_json_text(plausibility, "action"));
    string_t *rendered = summary ? tba_forecast_summary(string_c_str(summary), selection) : NULL;
    string_t *iso = tba_date_text(end, false), *uk = tba_date_text(end, true);
    bool ok = summary && rendered && iso && uk && plausibility &&
              tba_json_string(result, "summary_text", string_c_str(summary)) &&
              tba_json_string(result, "summary_html", string_c_str(rendered)) &&
              tba_json_string(result, "forecast_end_date", string_c_str(iso)) &&
              tba_json_string(result, "forecast_end_label", string_c_str(uk)) &&
              tba_json_string(result, "xreg_columns_used", selection) &&
              tba_json_string(result, "target_value_column_used", tba_json_text(state, "target_value_column")) &&
              tba_json_set(result, "collinearity_warning", overlap) && tba_json_set(result, "plausibility_warning", plausibility);
    string_free(summary);
    string_free(rendered);
    string_free(iso);
    string_free(uk);
    json_free(plausibility);
    if (!ok) {
        json_free(result);
        result = NULL;
        error = "Could not construct the forecast response.";
    }
done:
    tba_series_free(target);
    tba_series_free(drivers);
    json_free(meta);
    json_free(ends);
    json_free(overlap);
    string_free(note);
    string_free(driver_note);
    string_free(fit_csv);
    string_free(driver_csv);
    string_free(output);
    string_free(horizon_text);
    tba_forecast_remove(target_fit);
    tba_forecast_remove(driver_fit);
    if (!result)
        result = tba_json_error(error);
    if (result)
        tba_json_string(result, "model", tba_forecast_label(model));
    return result;
}

/* Run allowed models sequentially and publish only a complete successful result. */
json_t *tba_forecast_run(tba_app_t *app, const json_t *payload)
{
    if (!app || json_type(payload) != JSON_OBJECT)
        return tba_json_error("Forecast settings must be an object.");
    json_t *state = tba_app_state(app);
    for (size_t i = 0; state && i < json_object_size(state); ++i) {
        const string_t *key = json_object_key_at(state, i);
        const json_t *value = json_object_get(payload, key);
        if (value && !json_object_set(state, key, value)) {
            json_free(state);
            state = NULL;
        }
    }
    json_t *models = state ? tba_forecast_models(tba_json_text(state, "models")) : NULL;
    if (!models || !tba_forecast_numbers(state)) {
        json_free(models);
        json_free(state);
        return tba_json_error("Invalid model, order, season, confidence level, criterion or year type.");
    }
    json_t *results = json_new_array();
    for (size_t i = 0; results && i < json_array_size(models); ++i) {
        const char *model = string_c_str(json_string_value(json_array_get(models, i)));
        json_t *result = tba_forecast_one(app, state, model);
        bool ok = result && json_array_append(results, result);
        json_free(result);
        if (!ok) {
            json_free(results);
            results = NULL;
        }
    }
    json_t *result = !results ? NULL : json_array_size(results) == 1 ? json_clone(json_array_get(results, 0)) :
                     tba_forecast_combine(results);
    if (tba_json_bool(result, "ok") && !tba_app_result_save(app, result)) {
        json_free(result);
        result = tba_json_error("Forecast completed, but its downloadable result could not be saved.");
    }
    json_free(results);
    json_free(models);
    json_free(state);
    return result;
}
