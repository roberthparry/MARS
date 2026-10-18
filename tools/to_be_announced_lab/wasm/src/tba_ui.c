/**
 * @file tba_ui.c
 * @brief Scoped presentation utilities and browser event dispatch in WebAssembly.
 *
 * Application data lives in the host context; native globals hold only scalar
 * policy state. Event effects are deferred until each synchronous scope returns.
 */
#include "tba_form.h"
#include "tba_request.h"
#include "tba_view.h"
#include "tba_ui.h"

/* Resolve named controls through the form's native elements collection. */
node_jsref tba_ui_field(const char *name)
{
    return tba_dom_get(tba_dom_get(tba_dom_id("forecast-form"), "elements"), name);
}

/* Read a control without retaining a node handle. */
browser_jsref tba_ui_value(const char *name)
{
    return tba_dom_read(tba_ui_field(name), 5, "");
}

/* Copy a scoped value into a browser-owned control. */
void tba_ui_set(const char *name, browser_jsref value)
{
    tba_dom_write(tba_ui_field(name), 5, "", value);
}

/* Borrow browser-owned application data for this call only. */
browser_jsref tba_ui_state(const char *key)
{
    return tba_dom_get(tba_dom_root(), key);
}

/* Store values, not numeric handles, across event boundaries. */
void tba_ui_store(const char *key, browser_jsref value)
{
    tba_dom_set(tba_dom_root(), key, value);
}

/* Compare static tokens through exact host string equality. */
int tba_ui_is(browser_jsref value, const char *text)
{
    return tba_dom_equal(value, tba_dom_string(text));
}

/* Malformed numeric controls must not trap WebAssembly integer conversion. */
int tba_ui_integer(browser_jsref value)
{
    double number = tba_dom_number(value);
    return __builtin_isfinite(number) && number >= -2147483647 && number <= 2147483647 ? (int)number : 0;
}

/* Preserve zero as text whilst supplying an absent/empty fallback. */
browser_jsref tba_ui_or(browser_jsref value, const char *fallback)
{
    int text = tba_dom_text(value);
    return tba_dom_truth(text) ? text : tba_dom_string(fallback);
}

/* Join opaque Unicode with static presentation labels. */
browser_jsref tba_ui_text(const char *before, browser_jsref value, const char *after)
{
    int parts = tba_dom_object(1);
    tba_dom_push(parts, tba_dom_string(before));
    tba_dom_push(parts, tba_dom_text(value));
    tba_dom_push(parts, tba_dom_string(after));
    return tba_dom_join(parts, "");
}

/* Append a literal fragment. */
void tba_ui_add(browser_jsref parts, const char *text)
{
    tba_dom_push(parts, tba_dom_string(text));
}

/* Escape all user-provided text before markup insertion. */
void tba_ui_escaped(browser_jsref parts, browser_jsref value)
{
    tba_dom_push(parts, tba_dom_escape(value));
}

/* Join fragments once rather than repeatedly copying growing output. */
browser_jsref tba_ui_join(browser_jsref parts)
{
    return tba_dom_join(parts, "");
}

/* Text and HTML properties have separate browser capability kinds. */
void tba_ui_write(const char *id, const char *key, browser_jsref value)
{
    unsigned kind = tba_ui_is(tba_dom_string(key), "innerHTML") ? 1 :
                    tba_ui_is(tba_dom_string(key), "textContent") ? 0 :
                    tba_ui_is(tba_dom_string(key), "value") ? 5 : 2;
    tba_dom_write(tba_dom_id(id), kind, key, value);
}

/* Keep status ownership in the native application. */
void tba_ui_status(browser_jsref text, const char *kind)
{
    tba_ui_write("status", "textContent", text);
    tba_ui_write("status", "class", tba_ui_text("status ", tba_dom_string(kind), ""));
}

/* Switch panes without changing forecast state. */
void tba_ui_pane(unsigned pane)
{
    tba_dom_class(tba_dom_query(0, ".result-grid"), "hidden", pane != 0);
    tba_dom_class(tba_dom_id("help-pane"), "hidden", pane != 1);
    tba_dom_class(tba_dom_id("chart-pane"), "hidden", pane != 2);
    tba_dom_class(tba_dom_query(0, ".layout"), "chart-mode", pane == 2);
    tba_ui_write("chart-toggle", "textContent", tba_dom_string(pane == 2 ? "Results" : "Chart"));
    tba_ui_write("help-toggle", "textContent", tba_dom_string(pane == 1 ? "Results" : "Help"));
    tba_ui_status(tba_dom_string(pane == 1 ? "Help" : pane == 2 ? "Chart" : "Ready"), "");
}

/* Index a bounded selection once for constant-time membership checks. */
browser_jsref tba_ui_selection(browser_jsref text)
{
    int set = tba_dom_object(0), parts = tba_dom_split(text, ",");
    for (unsigned i = 0; i < tba_dom_count(parts); ++i) {
        unsigned mark = tba_dom_mark();
        int name = tba_dom_clean(tba_dom_item(parts, i), 0);
        if (tba_dom_truth(name))
            tba_dom_key_set(set, name, tba_dom_numeric(1));
        tba_dom_release(mark);
    }
    return set;
}

/* Collect selected DOM values without interpreting their mathematical contents. */
browser_jsref tba_ui_checked(const char *selector)
{
    int result = tba_dom_object(1), list = tba_dom_all(0, selector);
    for (unsigned i = 0; i < tba_dom_count(list); ++i) {
        unsigned mark = tba_dom_mark();
        tba_dom_push(result, tba_dom_read(tba_dom_item(list, i), 5, ""));
        tba_dom_release(mark);
    }
    return result;
}

/* Native code selects routes, payloads, authentication and completion actions. */
void tba_ui_request(const char *path, unsigned kind, browser_jsref body, unsigned action, unsigned serial)
{
    int config = tba_ui_state("config"), headers = tba_dom_object(0);
    int token = tba_dom_get(config, "control_token");
    if (tba_dom_truth(token))
        tba_dom_set(headers, "X-Dval-Lab-Control", token);
    int url = tba_ui_text("", tba_dom_get(config, "base_path"), path);
    tba_dom_request(url, kind, body, headers, action, serial);
}

/* Initialise controls and delegate all browser events into C. */
void tba_ui_init(browser_jsref config)
{
    tba_ui_store("config", config);
    tba_form_init(config);
    static const struct { const char *id, *event; unsigned action; } bindings[] = {
        {"forecast-form", "input", TBA_CHANGE}, {"forecast-form", "change", TBA_CHANGE},
        {"forecast-form", "submit", TBA_SUBMIT}, {"suggest-button", "click", TBA_SUGGEST},
        {"help-toggle", "click", TBA_HELP}, {"chart-toggle", "click", TBA_CHART},
        {"refresh-mobile", "click", TBA_COPY}, {"summary-box", "click", TBA_TAB},
        {"forecast-chart-body", "change", TBA_CHART_CHANGE},
        {"forecast-chart-body", "mousemove", TBA_HOVER}, {"forecast-chart-body", "mouseleave", TBA_LEAVE},
        {"download-summary", "click", TBA_DOWNLOAD_SUMMARY}, {"download-forecast", "click", TBA_DOWNLOAD_CSV}
    };
    for (unsigned i = 0; i < sizeof bindings / sizeof *bindings; ++i)
        tba_dom_listen(tba_dom_id(bindings[i].id), bindings[i].event, bindings[i].action);
    tba_ui_request("/mobile-access", 0, 0, TBA_RESPONSE_MOBILE, 0);
}

/* Dispatch the fixed action vocabulary; all state transitions stay native. */
void tba_ui_event(browser_jsref action_handle, event_jsref event)
{
    unsigned action = (unsigned)tba_dom_number(action_handle);
    switch (action) {
        case TBA_CHANGE:
            tba_form_change(event);
            break;
        case TBA_SUBMIT:
            tba_dom_prevent(event);
            tba_request_forecast();
            break;
        case TBA_SAVE:
            tba_request_save();
            break;
        case TBA_SUGGEST:
            tba_form_suggest(1);
            tba_ui_status(tba_dom_string("Suggested settings applied."), "ok");
            tba_dom_timer(TBA_SAVE, 250);
            break;
        case TBA_HELP:
            tba_ui_pane(tba_dom_has_class(tba_dom_id("help-pane"), "hidden") ? 1 : 0);
            break;
        case TBA_CHART:
            tba_ui_pane(tba_dom_has_class(tba_dom_id("chart-pane"), "hidden") ? 2 : 0);
            break;
        case TBA_COPY:
            tba_dom_clipboard(tba_dom_read(tba_dom_id("mobile-url"), 0, ""), TBA_RESPONSE_COPY);
            break;
        case TBA_TAB:
            tba_view_tab(event);
            break;
        case TBA_CHART_CHANGE:
            tba_view_visibility();
            tba_view_leave();
            break;
        case TBA_HOVER:
            tba_view_hover(event);
            break;
        case TBA_LEAVE:
            tba_view_leave();
            break;
        case TBA_DOWNLOAD_SUMMARY:
        case TBA_DOWNLOAD_CSV: {
            int result = tba_ui_state("result");
            int csv = action == TBA_DOWNLOAD_CSV;
            tba_dom_prevent(event);
            int text = tba_dom_get(result, csv ? "forecast_csv" : "summary_text");
            if (tba_dom_truth(text))
                tba_dom_download(tba_dom_string(csv ? "forecast.csv" : "summary.txt"), text,
                                 csv ? "text/csv;charset=utf-8" : "text/plain;charset=utf-8");
            break;
        }
        default:
            break;
    }
}

/* Convert only action scalars; structured values remain opaque. */
void tba_ui_response(browser_jsref action, browser_jsref serial, browser_jsref data, browser_jsref success)
{
    tba_request_complete((unsigned)tba_dom_number(action), (unsigned)tba_dom_number(serial),
                         data, tba_dom_truth(success));
}
