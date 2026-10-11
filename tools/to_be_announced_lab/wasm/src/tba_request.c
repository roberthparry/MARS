/**
 * @file tba_request.c
 * @brief Native request ordering, upload transitions and asynchronous completions.
 *
 * Saves are serialised and coalesced. Metadata generations reject stale replies;
 * browser capabilities perform only transport, never application decisions.
 */
#include "tba_ui.h"
#include "tba_form.h"
#include "tba_view.h"
#include "tba_request.h"

static unsigned tba_request_generation[2], tba_request_upload_generation[2];
static int tba_request_saving;
static unsigned tba_request_notice;

static void tba_request_send_save(int payload)
{
    tba_request_saving = 1;
    tba_ui_request("/state", 1, payload, TBA_RESPONSE_SAVE, 0);
}

/* Save snapshots in order, coalescing edits made during an outstanding save. */
void tba_request_save(void)
{
    int payload = tba_form_collect();
    if (tba_request_saving)
        tba_ui_store("pending_save", payload);
    else
        tba_request_send_save(payload);
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
    tba_dom_set(body, "date_column", tba_ui_value(driver ? "xreg_date_column" : "target_date_column"));
    tba_dom_set(body, driver ? "selected_columns" : "value_column",
                tba_ui_value(driver ? "xreg_columns" : "target_value_column"));
    tba_ui_store(driver ? "driver_request" : "target_request", body);
    tba_ui_request("/target-columns", 1, body, driver ? TBA_RESPONSE_DRIVER : TBA_RESPONSE_TARGET, serial);
}

/* Upload a selected browser File without exposing its contents to application JS. */
void tba_request_upload(int driver)
{
    int input = tba_dom_id(driver ? "xreg-upload" : "target-upload");
    int file = tba_dom_item(tba_dom_get(input, "files"), 0);
    if (!file)
        return;
    ++tba_request_generation[driver];
    tba_ui_status(tba_dom_string(driver ? "Uploading exogenous CSV..." : "Uploading target CSV..."), "");
    tba_ui_request(driver ? "/upload-xreg" : "/upload-target", 2, file,
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
    int payload = tba_form_collect();
    tba_dom_flag(button, "disabled", 1);
    tba_ui_status(tba_dom_string("Submitting forecast..."), "");
    tba_ui_request("/forecast", 1, payload, TBA_RESPONSE_FORECAST, 0);
}

static void tba_request_columns(int driver, int data)
{
    int columns = driver ? tba_dom_get(data, "value_column_details") : 0;
    if (!columns)
        columns = tba_dom_get(data, "value_columns");
    tba_form_columns(columns, driver);
    tba_form_meta(data, driver);
}

static void tba_request_saved(int data, int success)
{
    tba_request_saving = 0;
    int pending = tba_ui_state("pending_save");
    tba_ui_store("pending_save", 0);
    if (pending) {
        tba_request_send_save(pending);
        return;
    }
    if (!success) {
        tba_ui_status(tba_ui_or(tba_dom_get(data, "error"), "Could not save worksheet state."), "error");
    } else if (tba_request_notice) {
        tba_ui_status(tba_dom_string(tba_request_notice == 1 ?
            "Target CSV uploaded. Choose the series to forecast from the dropdown." : "Exogenous CSV uploaded."), "ok");
    }
    tba_request_notice = 0;
}

static void tba_request_mobile(int data, int success)
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
void tba_request_complete(unsigned action, unsigned serial, int data, int success)
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
        tba_request_saved(data, success);
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
        tba_form_upload(tba_dom_get(data, "path"), tba_dom_get(data, "original_name"), driver);
        tba_request_columns(driver, data);
        tba_request_notice = driver ? 2 : 1;
        tba_request_save();
    } else {
        tba_request_columns(driver, data);
        tba_dom_timer(TBA_SAVE, 250);
    }
}
