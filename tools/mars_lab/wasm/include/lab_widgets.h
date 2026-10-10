/**
 * @file lab_widgets.h
 * @brief Private interface: calendar control projection through the scoped browser DOM bridge.
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
 */
#ifndef LAB_WASM_WIDGETS_H
#define LAB_WASM_WIDGETS_H

/**
 * @brief Project the calendar grid and labels to DOM controls.
 * @param year Displayed year. @param month Displayed month.
 * @param selected_year Selected year. @param selected_month Selected month. @param selected_day Selected day.
 * @param today_year Current year. @param today_month Current month. @param today_day Current day.
 * @return One on success; zero for invalid month or absent controls, without changing the DOM.
 */
int lab_widgets_picker(double year, double month, double selected_year, double selected_month, double selected_day,
                       double today_year, double today_month, double today_day);

/** @brief Return the shell's visible rectangle, or the union of its two child controls, through the host.
 * @param shell Scoped date-control shell node whose visible anchor rectangle is measured.
 */
void lab_widgets_picker_anchor(int shell);

/** @brief Select the anchor and project viewport-clamped popup geometry.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param shell Scoped date-control shell node to anchor the popup.
 * @param viewport_width Viewport width in CSS pixels; zero uses the document element's client width.
 * @param viewport_height Viewport height in CSS pixels; zero uses the document element's client height.
 */
void lab_widgets_picker_place(int state, int shell, double viewport_width, double viewport_height);

/** @brief Hide the popup and clear both native navigation and host references before any focus event.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param restore_focus Non-zero queues focus on the former opener after clearing picker ownership.
 */
void lab_widgets_picker_close(int state, int restore_focus);

/** @brief Write a date and dispatch one deferred bubbling change only when its authored value differs.
 * @param input Scoped destination date-input node, or zero for no action.
 * @param value Scoped authored date string to write without parsing.
 * @return One if the input text changed and a change event was queued; zero if absent or unchanged.
 */
int lab_widgets_date_commit(int input, int value);

/** @brief Select GMT for the almanac date and the local calendar date for every other input.
 * @param input Scoped date-input node; the almanac input selects UTC.
 * @param local_date Scoped current local date string.
 * @param utc_date Scoped current UTC date string.
 */
void lab_widgets_picker_today_date(int input, int local_date, int utc_date);

/** @brief Install the almanac GMT date and time together, emitting just one change for the complete moment.
 * @param input Scoped destination date-input node.
 * @param local_date Scoped local date string used for non-almanac inputs.
 * @param utc_moment Scoped record containing UTC date and time strings for the almanac controls.
 */
void lab_widgets_picker_today(int input, int local_date, int utc_moment);

/** @brief Project the open native month and position it; a closed picker cannot be reopened by a stale render.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param selected Scoped date record with numeric year, month and day, or zero when invalid.
 * @param today Scoped current-date record with numeric year, month and day.
 * @param viewport_width Viewport width in CSS pixels; zero uses the document element's client width.
 * @param viewport_height Viewport height in CSS pixels; zero uses the document element's client height.
 */
void lab_widgets_picker_render(int state, int selected, int today, double viewport_width, double viewport_height);

/** @brief Choose the selected date or today's date, transfer popup ownership and project one complete calendar.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param input Scoped date-input node receiving picker commits.
 * @param button Scoped opener button used for anchoring and focus restoration.
 * @param selected Scoped date record with numeric year, month and day, or zero when invalid.
 * @param today Scoped current-date record with numeric year, month and day.
 * @param fallback_year Browser's current local year, used when the selected/today year is non-finite.
 * @param viewport_width Viewport width in CSS pixels; zero uses the document element's client width.
 * @param viewport_height Viewport height in CSS pixels; zero uses the document element's client height.
 */
void lab_widgets_picker_open(int state, int input, int button, int selected, int today, double fallback_year,
                             double viewport_width, double viewport_height);

/** @brief Format an accepted native date for commit; return whether the caller should refresh the calendar.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param packed Accepted civil date encoded as YYYYMMDD.
 * @param commit Non-zero writes the date to the input; zero only reports whether it can be applied.
 * @return One for a valid date with an open picker and owned input; zero otherwise, without committing.
 */
int lab_widgets_picker_apply(int state, int packed, int commit);

/** @brief Set a month/year (0), shift months (1) or shift years (2), preserving and clamping the selected day.
 * @param state Scoped mutable picker record containing input, button and shell nodes.
 * @param operation Set year/month 0, shift months 1 or shift years 2; other values are rejected.
 * @param first Requested year for operation 0; signed whole-month or whole-year increment otherwise.
 * @param second Requested month for operation 0; ignored for shifts.
 * @param selected Scoped date record with numeric year, month and day, or zero when invalid.
 * @param fallback_year Browser's current local year, used when setting a non-finite year.
 * @param commit Non-zero commits the resulting date; zero only moves the displayed calendar.
 * @return One when the resulting date can be applied; zero for a closed picker or invalid request.
 */
int lab_widgets_picker_move(int state, unsigned operation, double first, double second, int selected,
                            double fallback_year, int commit);

#endif
