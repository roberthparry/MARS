/**
 * @file lab_widgets.h
 * @brief Application API: calendar control projection through the scoped browser DOM bridge.
 *
 * Builds month options, weekday headings and the six-week date-picker grid from
 * the existing C calendar model. Owns popup lifecycle, anchor selection, placement,
 * date commits and local-versus-GMT Today policy. Browser code supplies structured
 * dates, clock readings, viewport dimensions and synchronous DOM capabilities.
 * The host state object retains browser references, never C handle numbers. Focus
 * and change events are deferred until projection finishes and handle scopes close.
 * No mathematical or date text is parsed; invalid months leave the DOM intact.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_WASM_WIDGETS_H
#define LAB_WASM_WIDGETS_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Date-picker navigation operations with stable browser ABI values. */
typedef enum {
    LAB_PICKER_SET          = 0, /**< Set a year and month. */
    LAB_PICKER_SHIFT_MONTHS = 1, /**< Shift by signed whole months. */
    LAB_PICKER_SHIFT_YEARS  = 2, /**< Shift by signed whole years. */
} lab_picker_operation_t;

/**
 * @brief Project the six-week calendar grid and labels to DOM controls.
 * @param year Displayed civil year, an integer in 1..9999.
 * @param month Displayed calendar month, January 1 through December 12.
 * @param selected_year Civil year of the selected date to highlight, normally 1..9999.
 * @param selected_month Selected calendar month, January 1 through December 12.
 * @param selected_day Selected day of month, 1..31 subject to its month and year.
 * @param today_year Civil year of today's date to highlight, normally 1..9999.
 * @param today_month Today's calendar month, January 1 through December 12.
 * @param today_day Today's day of month, 1..31 subject to its month and year.
 * @return True on success; false for an invalid displayed month/year or missing required controls, without changing DOM.
 * @details An invalid selected or today date suppresses its highlight without rejecting the displayed month.
 */
bool lab_widgets_picker(double year, double month, double selected_year, double selected_month, double selected_day,
                       double today_year, double today_month, double today_day);

/**
 * @brief Publish the shell's visible rectangle, falling back to the union of its child controls.
 * @param shell Scoped date-control shell-node handle.
 * @details lab_dom_return supplies left, right, top, bottom, width and height in client CSS pixels,
 * or null when neither the shell nor its date input/button provides a visible rectangle.
 */
void lab_widgets_picker_anchor(jsref shell);

/** @brief Select the anchor and project viewport-clamped popup geometry.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param shell Scoped date-control shell node to anchor the popup.
 * @param viewport_width Viewport width in CSS pixels; zero uses the document element's client width.
 * @param viewport_height Viewport height in CSS pixels; zero uses the document element's client height.
 */
void lab_widgets_picker_place(jsref state, jsref shell, double viewport_width, double viewport_height);

/** @brief Hide the popup and clear both native navigation and host references before any focus event.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param restore_focus True queues focus on the former opener after clearing picker ownership.
 */
void lab_widgets_picker_close(jsref state, bool restore_focus);

/** @brief Write a date and dispatch one deferred bubbling change only when its authored value differs.
 * @param input Scoped destination date-input node, or zero for no action.
 * @param value Scoped authored date string to write without parsing.
 * @return True if the input text changed and a change event was queued; false if absent or unchanged.
 */
bool lab_widgets_date_commit(jsref input, jsref value);

/**
 * @brief Select the GMT date for the almanac and the local date for other inputs.
 * @param input Scoped date-input node; exactly the almanac date input selects UTC.
 * @param local_date Scoped current local calendar-date string.
 * @param utc_date Scoped current UTC calendar-date string.
 * @details Publishes the selected string unchanged through lab_dom_return; no input is committed.
 */
void lab_widgets_picker_today_date(jsref input, jsref local_date, jsref utc_date);

/** @brief Install the almanac GMT date and time together, emitting just one change for the complete moment.
 * @param input Scoped destination date-input node.
 * @param local_date Scoped local date string used for non-almanac inputs.
 * @param utc_moment Scoped record containing UTC date and time strings for the almanac controls.
 */
void lab_widgets_picker_today(jsref input, jsref local_date, jsref utc_moment);

/** @brief Project the open native month and position it; a closed picker cannot be reopened by a stale render.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param selected Scoped date record with numeric year (1..9999), month (1..12) and day, or zero when invalid.
 * @param today Scoped current-date record with numeric year (1..9999), month (1..12) and day.
 * @param viewport_width Viewport width in CSS pixels; zero uses the document element's client width.
 * @param viewport_height Viewport height in CSS pixels; zero uses the document element's client height.
 */
void lab_widgets_picker_render(jsref state, jsref selected, jsref today,
                               double viewport_width, double viewport_height);

/** @brief Choose the selected date or today's date, transfer popup ownership and project one complete calendar.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param input Scoped date-input node receiving picker commits.
 * @param button Scoped opener button used for anchoring and focus restoration.
 * @param selected Scoped date record with numeric year (1..9999), month (1..12) and day, or zero when invalid.
 * @param today Scoped current-date record with numeric year (1..9999), month (1..12) and day.
 * @param fallback_year Browser's current local year, used when the selected/today year is non-finite.
 * @param viewport_width Viewport width in CSS pixels; zero uses the document element's client width.
 * @param viewport_height Viewport height in CSS pixels; zero uses the document element's client height.
 */
void lab_widgets_picker_open(jsref state, jsref input, jsref button, jsref selected,
                             jsref today, double fallback_year, double viewport_width, double viewport_height);

/** @brief Format an accepted native date for commit; return whether the caller should refresh the calendar.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param packed Accepted civil date encoded as YYYYMMDD.
 * @param commit True writes the date to the input; false only reports whether it can be applied.
 * @return True for a valid date with an open picker and owned input; false otherwise, without committing.
 */
bool lab_widgets_picker_apply(jsref state, int packed, bool commit);

/** @brief Set a month/year (0), shift months (1) or shift years (2), preserving and clamping the selected day.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param operation Set year/month 0, shift months 1 or shift years 2; other values are rejected.
 * @param first Requested year for operation 0; signed whole-month or whole-year increment otherwise.
 * @param second Requested one-based month (January 1 through December 12) for operation 0; ignored for shifts.
 * @param selected Scoped date record with numeric year (1..9999), month (1..12) and day, or zero when invalid.
 * @param fallback_year Browser's current local year, used when setting a non-finite year.
 * @param commit True commits the resulting date; false only moves the displayed calendar.
 * @return True when the resulting date can be applied; false for a closed picker or invalid request.
 */
bool lab_widgets_picker_move(jsref state, lab_picker_operation_t operation, double first, double second,
                             jsref selected, double fallback_year, bool commit);

#endif
