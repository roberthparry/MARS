/**
 * @file lab_forms.h
 * @brief Private interface: structured date, numeric form and integrator-row policy for the Lab browser.
 *
 * Freestanding WebAssembly functions accept numbers and presence flags only.
 * Gregorian dates use years 1..9999; invalid dates return zero, invalid weekdays
 * return -1 and invalid numeric results return NaN. Packed dates are YYYYMMDD,
 * with component accessors so the host need not reproduce date arithmetic.
 * DOM access, timezone lookup and display formatting remain browser operations.
 * Text and expression parsing require native string_t support and are not
 * implemented here. No expression simplification or mathematical rewriting occurs.
 * The calendar grid export borrows 42 rows of four uint32_t words: year, month,
 * day and flags (valid 1, outside month 2, today 4, selected 8). The host copies
 * the 672-byte result before another call. Invalid months return null without
 * replacing the previous grid; invalid selected/today dates match no cells.
 * The picker controller additionally owns open/closed state and the visible
 * month/year. Selection and navigation return a packed committed date with the
 * input day clamped to that month. Closed or invalid navigation returns zero
 * without changing state. The host retains only DOM references and supplies
 * its current local year for invalid numeric year input.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_FORMS_H
#define LAB_WASM_FORMS_H

#include <stdint.h>

/**
 * @brief Borrow a structured six-week grid from the calendar model.
 * @param year Displayed year, 1..9999. @param month Displayed month, 1..12.
 * @param selected_year Selected year. @param selected_month Selected month. @param selected_day Selected day.
 * @param today_year Current year. @param today_month Current month. @param today_day Current day.
 * @return Borrowed 42 groups of year, month, day and flags; null for invalid displayed dates.
 * Invalid selected/today dates add no highlighting. The next call reuses the buffer.
 */
const uint32_t *lab_forms_calendar_grid(double year, double month, double selected_year, double selected_month,
                                        double selected_day, double today_year, double today_month, double today_day);

/** @brief Compare finite coordinates within an absolute tolerance. @param left First coordinate.
 * @param right Second coordinate. @param tolerance Non-negative absolute tolerance. @return Matching flag. */
int lab_forms_nearly_equal(double left, double right, double tolerance);

/** @brief Validate browser-supplied numeric date components, without reading text.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param month Whole month number in 1..12.
 * @param day Whole day number valid for the supplied year and month.
 * @return One for a valid supported civil date; zero otherwise.
 */
int lab_forms_date_valid(double year, double month, double day);

/** @brief Return the Gregorian month length, or zero for invalid components.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param month Whole month number in 1..12.
 * @return Month length, 28..31, or zero for invalid/non-integral components.
 */
int lab_forms_days_in_month(double year, double month);

/** @brief Clamp numeric picker values before any integer conversion; NaN uses the fallback.
 * @param value Candidate value; non-finite input uses fallback.
 * @param low Inclusive lower integer limit; returned directly when low exceeds high.
 * @param high Inclusive upper integer limit.
 * @param fallback Replacement for non-finite value; a non-finite fallback uses low.
 * @return Clamped integer, truncating an in-range fractional value towards zero.
 */
int lab_forms_clamp(double value, int low, int high, double fallback);

/** @brief Return a Monday-first weekday for the first day of the requested month.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param month Whole month number in 1..12.
 * @return Monday-based weekday, 0..6, or -1 for invalid components.
 */
int lab_forms_month_weekday(double year, double month);

/** @brief Shift by whole months, saturating at January 0001 and December 9999.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param month Whole month number in 1..12.
 * @param delta Signed whole number of months in -120000..120000.
 * @return First day of the clamped destination month as YYYYMM01, or zero for invalid input.
 */
int lab_forms_shift_month(double year, double month, double delta);

/** @brief Shift the visible year using a bounded whole-year increment.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param delta Signed whole number of years in -120000..120000.
 * @return Destination year clamped to 1..9999, or zero for invalid input.
 */
int lab_forms_shift_year(double year, double delta);

/** @brief Return one of the 42 calendar cells; zero disables cells outside supported years.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param month Whole month number in 1..12.
 * @param slot Whole cell index, 0..41, in the Monday-first six-week grid.
 * @return Cell date as YYYYMMDD, or zero for invalid input or a cell outside years 1..9999.
 */
int lab_forms_calendar_cell(double year, double month, double slot);

/** @brief Read the year component of a packed calendar result.
 * @param packed Civil date encoded as YYYYMMDD; this accessor does not validate it.
 * @return Year component, obtained by integer division by 10000.
 */
int lab_forms_date_year(int packed);

/** @brief Read the month component of a packed calendar result.
 * @param packed Civil date encoded as YYYYMMDD; this accessor does not validate it.
 * @return Month component, obtained from the middle two decimal digits.
 */
int lab_forms_date_month(int packed);

/** @brief Read the day component of a packed calendar result.
 * @param packed Civil date encoded as YYYYMMDD; this accessor does not validate it.
 * @return Day component, obtained from the final two decimal digits.
 */
int lab_forms_date_day(int packed);

/** @brief Preserve the selected day where possible when changing month or year.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param month Whole month number in 1..12.
 * @param day Requested day, truncated and clamped to the month; non-finite values use day 1.
 * @return Clamped day number, or zero for an invalid year or month.
 */
int lab_forms_clamp_day(double year, double month, double day);

/** @brief Select the year-navigation step from browser modifier flags.
 * @param control Non-zero when the Control modifier is pressed; takes precedence over Shift.
 * @param shift Non-zero when the Shift modifier is pressed.
 * @return Year step: 100 for Control, otherwise 10 for Shift, otherwise 1.
 */
int lab_forms_year_step(int control, int shift);

/** @brief Open a picker with bounded structured components; the host supplies its local current year.
 * @param year Requested year, clamped to 1..9999; non-finite input uses fallback_year.
 * @param month Requested month, clamped to 1..12; non-finite input uses January.
 * @param fallback_year Replacement for a non-finite year; clamped to 1..9999, with non-finite values using 1.
 * @return First day of the opened month as YYYYMM01.
 */
int lab_forms_picker_open(double year, double month, double fallback_year);

/** @brief Expose the visible month as a packed date; zero denotes a closed picker.
 * @return First day of the visible month as YYYYMM01, or zero when closed.
 */
int lab_forms_picker_date(void);

/** @brief Closing discards navigation state; subsequent edits cannot reopen the picker.
 */
void lab_forms_picker_close(void);

/** @brief Select a month/year and return a safe committed date, preserving the input's day when possible.
 * @param year Requested year, clamped to 1..9999; non-finite input uses fallback_year.
 * @param month Requested month, clamped to 1..12; non-finite input uses January.
 * @param day Requested day, truncated and clamped to the destination month; non-finite values use 1.
 * @param fallback_year Replacement for a non-finite year; clamped to 1..9999, with non-finite values using 1.
 * @return Committed date as YYYYMMDD, or zero when the picker is closed.
 */
int lab_forms_picker_set(double year, double month, double day, double fallback_year);

/** @brief Navigate an open picker; invalid or fractional increments leave it unchanged.
 * @param delta Signed whole increment in -120000..120000.
 * @param years Non-zero shifts by years; zero shifts by months.
 * @param day Requested day to preserve, clamped to the destination month; non-finite values use 1.
 * @return Destination date as YYYYMMDD, or zero for a closed picker or invalid increment.
 */
int lab_forms_picker_shift(double delta, int years, double day);

/** @brief Convert a structured civil time to Unix milliseconds for browser timezone APIs.
 * @param year Whole proleptic Gregorian year in 1..9999.
 * @param month Whole month number in 1..12.
 * @param day Whole day number valid for the supplied year and month.
 * @param hour Whole hour in 0..24; hour 24 rolls into the following day.
 * @param minute Whole minute in 0..59.
 * @param second Whole second in 0..59; leap seconds are not accepted.
 * @return Unix milliseconds treating the supplied civil time as UTC, or NaN for invalid components.
 */
double lab_forms_utc_milliseconds(double year, double month, double day, double hour, double minute, double second);

/** @brief Derive a timezone offset after the browser has supplied its locale time components.
 * @param local_milliseconds Finite Unix milliseconds for local civil components interpreted as UTC.
 * @param utc_milliseconds Finite Unix milliseconds for the actual UTC instant.
 * @return Local-minus-UTC offset in hours; NaN for non-finite input or offsets outside -24..24.
 */
double lab_forms_offset_hours(double local_milliseconds, double utc_milliseconds);

/** @brief Round an offset to two decimal places, using browser-compatible half-up rounding.
 * @param value Finite timezone offset in hours, within -24..24.
 * @return Offset rounded to hundredths of an hour; NaN for unsupported input.
 */
double lab_forms_round_offset(double value);

/** @brief Prevent removal of the final row or final integration variable.
 * @param free_row Non-zero when the candidate row is a free parameter rather than an integration bound.
 * @param row_count Total number of integrator rows, including free parameters.
 * @param bound_count Number of integration-bound rows.
 * @return One if removal leaves a row and, when removing a bound, another bound; zero otherwise.
 */
int lab_forms_row_removable(int free_row, int row_count, int bound_count);

/** @brief Choose the first available name from nine preferred names followed by x1..x99.
 * @param first Occupied-name bitmask for candidate indices 0..31; set bits mark names already in use.
 * @param second Occupied-name bitmask for candidate indices 32..63.
 * @param third Occupied-name bitmask for candidate indices 64..95.
 * @param fourth Occupied-name bitmask for candidate indices 96..107; only the lowest 12 bits are used.
 * @return First available candidate index, 0..107; zero also serves as the fallback when all names are occupied.
 */
int lab_forms_name_candidate(uint32_t first, uint32_t second, uint32_t third, uint32_t fourth);

#endif
