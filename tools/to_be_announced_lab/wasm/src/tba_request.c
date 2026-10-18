/**
 * @file tba_request.c
 * @brief Native request ordering, local ingestion and encrypted completions.
 *
 * Settings remain browser-local. Metadata generations reject stale replies;
 * browser capabilities perform only transport, never application decisions.
 */
#include "tba_ui.h"
#include "tba_form.h"
#include "tba_view.h"
#include "tba_local.h"
#include "tba_request.h"

static unsigned tba_request_generation[2], tba_request_upload_generation[2];
/* Keep settings in this browser context only. */
void tba_request_save(void)
{
    tba_ui_store("local_settings", tba_form_collect());
}

/* Reload metadata for the selected file and columns, rejecting earlier generations. */
void tba_request_metadata(int driver)
{
    unsigned serial = ++tba_request_generation[driver];
    int path = tba_ui_value(driver ? "xreg_path" : "target_path");
    if (!tba_dom_truth(path)) {
        tba_form_meta(tba_dom_object(0), driver);
        tba_form_columns(tba_dom_object(1), driver);
        return;
    }
    int body = tba_dom_object(0);
    tba_dom_set(body, "path", path);
    tba_dom_set(body, "operation", tba_dom_string("metadata"));
    tba_dom_set(body, "data", tba_local_project(driver));
    tba_dom_set(body, "date_column", tba_ui_value(driver ? "xreg_date_column" : "target_date_column"));
    tba_dom_set(body, driver ? "selected_columns" : "value_column",
                tba_ui_value(driver ? "xreg_columns" : "target_value_column"));
    tba_ui_store(driver ? "driver_request" : "target_request", body);
    tba_ui_request("/stream", 3, body, driver ? TBA_RESPONSE_DRIVER : TBA_RESPONSE_TARGET, serial);
}

/* Read a browser File locally; no upload request is made. */
void tba_request_upload(int driver)
{
    int input = tba_dom_id(driver ? "xreg-upload" : "target-upload");
    int file = tba_dom_item(tba_dom_get(input, "files"), 0);
    if (!file)
        return;
    ++tba_request_generation[driver];
    tba_ui_status(tba_dom_string("Reading local file..."), "");
    tba_ui_request("", 2, file,
                   driver ? TBA_RESPONSE_UPLOAD_DRIVER : TBA_RESPONSE_UPLOAD_TARGET,
                   ++tba_request_upload_generation[driver]);
}

/* Submit exactly one forecast at a time. */
void tba_request_forecast(void)
{
    int button = tba_dom_id("run-button");
    if (tba_dom_truth(tba_dom_get(button, "disabled")))
        return;
    tba_dom_timer(TBA_SAVE, -1);
    int payload = tba_dom_object(0);
    tba_dom_set(payload, "operation", tba_dom_string("run"));
    int settings = tba_form_collect();
    tba_dom_set(settings, "target_path", tba_dom_string(""));
    tba_dom_set(settings, "target_display_name", tba_dom_string(""));
    tba_dom_set(settings, "xreg_path", tba_dom_string(""));
    tba_dom_set(settings, "xreg_display_name", tba_dom_string(""));
    tba_dom_set(payload, "settings", settings);
    int target = tba_local_project(0);
    if (!target) {
        tba_ui_status(tba_dom_string("Choose a local file and valid columns first."), "error");
        return;
    }
    tba_dom_set(payload, "target", target);
    if (tba_dom_truth(tba_ui_value("xreg_columns")))
        tba_dom_set(payload, "drivers", tba_local_project(1));
    tba_dom_flag(button, "disabled", 1);
    tba_ui_status(tba_dom_string("Submitting forecast..."), "");
    tba_ui_request("/stream", 3, payload, TBA_RESPONSE_FORECAST, 0);
}

static void tba_request_columns(int driver, browser_handle data)
{
    int columns = tba_local_columns(driver);
    int document = tba_ui_state(driver ? "local_driver" : "local_target");
    tba_dom_set(data, "date_candidates", tba_dom_get(document, "headers"));
    tba_form_columns(columns, driver);
    tba_form_meta(data, driver);
}

static void tba_request_mobile(browser_handle data, int success)
{
    tba_ui_write("mobile-status", "textContent", success ?
        tba_ui_or(tba_dom_get(data, "title"), "Phone access ready.") :
        tba_ui_or(tba_dom_get(data, "error"), "Phone access unavailable."));
    if (!success)
        return;
    tba_ui_write("mobile-title", "textContent", tba_ui_or(tba_dom_get(data, "title"), "Phone access"));
    tba_ui_write("mobile-hint", "textContent", tba_dom_get(data, "hint"));
    tba_ui_write("mobile-url", "textContent", tba_dom_get(data, "url"));
    tba_ui_write("mobile-url", "href", tba_ui_or(tba_dom_get(data, "url"), "#"));
    tba_ui_write("qr-box", "innerHTML", tba_dom_get(data, "qr"));
}

/* Validate completions before allowing them to replace current worksheet state. */
void tba_request_complete(unsigned action, unsigned serial, browser_handle data, int success)
{
    if (action == TBA_RESPONSE_MOBILE) {
        tba_request_mobile(data, success);
        return;
    }
    if (action == TBA_RESPONSE_COPY) {
        tba_ui_write("mobile-status", "textContent", success ? tba_dom_string("Copied phone URL.") :
                     tba_ui_or(tba_dom_get(data, "error"), "Could not copy phone URL."));
        return;
    }
    success = success && tba_dom_truth(tba_dom_get(data, "ok"));
    if (action == TBA_RESPONSE_SAVE) {
        return;
    }
    if (action == TBA_RESPONSE_FORECAST) {
        tba_dom_flag(tba_dom_id("run-button"), "disabled", 0);
        if (success)
            tba_view_result(data);
        else
            tba_ui_status(tba_ui_or(tba_dom_get(data, "error"), "Forecast failed."), "error");
        return;
    }
    int upload = action == TBA_RESPONSE_UPLOAD_TARGET || action == TBA_RESPONSE_UPLOAD_DRIVER;
    int driver = action == TBA_RESPONSE_DRIVER || action == TBA_RESPONSE_UPLOAD_DRIVER;
    if (!upload && action != TBA_RESPONSE_TARGET && action != TBA_RESPONSE_DRIVER)
        return;
    if (serial != (upload ? tba_request_upload_generation[driver] : tba_request_generation[driver]))
        return;
    if (!upload) {
        int request = tba_ui_state(driver ? "driver_request" : "target_request");
        if (!tba_dom_equal(tba_dom_get(request, "path"), tba_ui_value(driver ? "xreg_path" : "target_path")) ||
            !tba_dom_equal(tba_dom_get(request, "date_column"),
                           tba_ui_value(driver ? "xreg_date_column" : "target_date_column")) ||
            (!driver && !tba_dom_equal(tba_dom_get(request, "value_column"), tba_ui_value("target_value_column"))))
            return;
    } else {
        tba_ui_write(driver ? "xreg-upload" : "target-upload", "value", tba_dom_string(""));
    }
    if (!success) {
        tba_ui_status(tba_ui_or(tba_dom_get(data, "error"), "Could not read CSV metadata."), "error");
        return;
    }
    if (upload) {
        int document = tba_local_parse(tba_dom_get(data, "text"));
        if (!document) {
            tba_ui_status(tba_dom_string("Invalid or oversized local CSV."), "error");
            return;
        }
        tba_ui_store(driver ? "local_driver" : "local_target", document);
        int headers = tba_dom_get(document, "headers"), date = tba_dom_item(headers, 0);
        /* Heading discovery examines at most 256 locally supplied headings. */
        for (unsigned i = 0; i < tba_dom_count(headers); ++i) {
            int name = tba_dom_item(headers, i), lower = tba_dom_clean(name, 1);
            if (tba_ui_is(lower, "date") || tba_ui_is(lower, "month") || tba_ui_is(lower, "period")) {
                date = name;
                break;
            }
        }
        tba_form_upload(tba_dom_string("local"), tba_dom_get(data, "name"), driver);
        tba_ui_set(driver ? "xreg_columns" : "target_value_column", tba_dom_string(""));
        int initial = tba_dom_object(0);
        tba_dom_set(initial, "date_candidates", headers);
        tba_dom_set(initial, "date_column", date);
        tba_form_meta(initial, driver);
        tba_ui_set(driver ? "xreg_date_column" : "target_date_column", date);
        tba_form_columns(tba_local_columns(driver), driver);
        tba_ui_status(tba_dom_string("File kept locally. Preparing encrypted request..."), "");
        tba_request_metadata(driver);
    } else {
        tba_request_columns(driver, data);
        tba_ui_status(tba_dom_string("Local file ready. Encrypted request verified."), "");
        tba_dom_timer(TBA_SAVE, 250);
    }
}
