/**
 * @file tba_ui.h
 * @brief Shared scoped values and entry points for the native browser application.
 *
 * C owns application state transitions. Persistent structured values are stored
 * in the host context; handles are temporary and must never survive an entry.
 */
#ifndef TBA_UI_H
#define TBA_UI_H
#include "tba_handle.h"
#include "tba_dom.h"

enum {
    TBA_CHANGE = 1, TBA_SUBMIT, TBA_SAVE, TBA_SUGGEST, TBA_HELP, TBA_CHART,
    TBA_COPY, TBA_TAB, TBA_CHART_CHANGE, TBA_HOVER, TBA_LEAVE, TBA_DOWNLOAD_SUMMARY, TBA_DOWNLOAD_CSV,
    TBA_RESPONSE_SAVE = 20, TBA_RESPONSE_FORECAST, TBA_RESPONSE_TARGET, TBA_RESPONSE_DRIVER,
    TBA_RESPONSE_UPLOAD_TARGET, TBA_RESPONSE_UPLOAD_DRIVER, TBA_RESPONSE_MOBILE, TBA_RESPONSE_COPY
};

/**
 * @brief Initialise the C application.
 * @param config Scoped decoded configuration.
 */
void tba_ui_init(browser_jsref config);

/**
 * @brief Dispatch a browser event.
 * @param action Scoped numeric action.
 * @param event Scoped browser event or zero.
 */
void tba_ui_event(browser_jsref action, event_jsref event);

/**
 * @brief Dispatch an asynchronous completion.
 * @param action Scoped action.
 * @param serial Scoped generation.
 * @param data Scoped result.
 * @param success Scoped HTTP-success boolean.
 */
void tba_ui_response(browser_jsref action, browser_jsref serial, browser_jsref data, browser_jsref success);

/**
 * @brief Borrow a form control.
 * @param name Static control name.
 * @return Scoped node.
 */
node_jsref tba_ui_field(const char *name);

/**
 * @brief Read a form value.
 * @param name Static control name.
 * @return Scoped text.
 */
browser_jsref tba_ui_value(const char *name);

/**
 * @brief Set a form value.
 * @param name Static control name.
 * @param value Scoped value.
 */
void tba_ui_set(const char *name, browser_jsref value);

/**
 * @brief Read retained application data.
 * @param key Static key.
 * @return Scoped value.
 */
browser_jsref tba_ui_state(const char *key);

/**
 * @brief Retain an application value, never its handle.
 * @param key Static key.
 * @param value Scoped value.
 */
void tba_ui_store(const char *key, browser_jsref value);

/**
 * @brief Compare text to a static token.
 * @param value Scoped text.
 * @param text Static token.
 * @return Equality.
 */
int tba_ui_is(browser_jsref value, const char *text);

/**
 * @brief Convert a control to a bounded integer.
 * @param value Scoped value.
 * @return Integer, or zero if invalid.
 */
int tba_ui_integer(browser_jsref value);

/**
 * @brief Choose non-empty text or a literal.
 * @param value Scoped text.
 * @param fallback Static default.
 * @return Scoped text.
 */
browser_jsref tba_ui_or(browser_jsref value, const char *fallback);

/**
 * @brief Concatenate opaque text with literals.
 * @param before Static prefix.
 * @param value Scoped text.
 * @param after Static suffix.
 * @return Scoped text.
 */
browser_jsref tba_ui_text(const char *before, browser_jsref value, const char *after);

/**
 * @brief Append literal markup to a fragment array.
 * @param parts Scoped array.
 * @param text Static text.
 */
void tba_ui_add(browser_jsref parts, const char *text);

/**
 * @brief Append escaped text to a fragment array.
 * @param parts Scoped array.
 * @param value Scoped text.
 */
void tba_ui_escaped(browser_jsref parts, browser_jsref value);

/**
 * @brief Join a fragment array.
 * @param parts Scoped fragments.
 * @return Scoped text.
 */
browser_jsref tba_ui_join(browser_jsref parts);

/**
 * @brief Write a node property.
 * @param id Exact ID.
 * @param key Property name.
 * @param value Scoped text.
 */
void tba_ui_write(const char *id, const char *key, browser_jsref value);

/**
 * @brief Publish status text.
 * @param text Scoped message.
 * @param kind Static class suffix.
 */
void tba_ui_status(browser_jsref text, const char *kind);

/**
 * @brief Display one main pane.
 * @param pane Results 0, help 1, chart 2.
 */
void tba_ui_pane(unsigned pane);

/**
 * @brief Build a set of comma-separated selections.
 * @param text Scoped text.
 * @return Scoped null-prototype index.
 */
browser_jsref tba_ui_selection(browser_jsref text);

/**
 * @brief Collect checked values.
 * @param selector Static selector.
 * @return Scoped array.
 */
browser_jsref tba_ui_checked(const char *selector);

/**
 * @brief Queue a browser capability request.
 * @param path Static route.
 * @param kind GET 0, POST 1, local file read 2, encrypted stream 3.
 * @param body Scoped data.
 * @param action Completion action.
 * @param serial Request generation.
 */
void tba_ui_request(const char *path, unsigned kind, browser_jsref body, unsigned action, unsigned serial);
#endif
