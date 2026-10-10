/**
 * @file lab_forms.c
 * @brief Structured date, numeric form and integrator-row policy for the Lab browser.
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
 */
#include <stdint.h>

#include "../include/lab_forms.h"

static int lab_forms_finite(double value)
{
    return value == value && value <= 1.7976931348623157e308 && value >= -1.7976931348623157e308;
}

static int lab_forms_integer(double value, int low, int high)
{
    return lab_forms_finite(value) && value >= low && value <= high && value == (int)value;
}

static int lab_forms_month_days(int year, int month)
{
    static const unsigned char days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static int lab_forms_date_ok(double year, double month, double day)
{
    return lab_forms_integer(year, 1, 9999) && lab_forms_integer(month, 1, 12) &&
           lab_forms_integer(day, 1, lab_forms_month_days((int)year, (int)month));
}

static int lab_forms_serial(int year, int month, int day)
{
    static const unsigned short before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int previous = year - 1;
    return previous * 365 + previous / 4 - previous / 100 + previous / 400 + before[month - 1] + day - 1 +
           (month > 2 && lab_forms_month_days(year, 2) == 29);
}

/* Validate browser-supplied numeric date components, without reading text. */
int lab_forms_date_valid(double year, double month, double day)
{
    return lab_forms_date_ok(year, month, day);
}

/* Return the Gregorian month length, or zero for invalid components. */
int lab_forms_days_in_month(double year, double month)
{
    return lab_forms_integer(year, 1, 9999) && lab_forms_integer(month, 1, 12)
               ? lab_forms_month_days((int)year, (int)month)
               : 0;
}

/* Clamp numeric picker values before any integer conversion; NaN uses the fallback. */
int lab_forms_clamp(double value, int low, int high, double fallback)
{
    if (low > high)
        return low;
    if (!lab_forms_finite(value))
        value = lab_forms_finite(fallback) ? fallback : low;
    return value <= low ? low : value >= high ? high : (int)value;
}

/* Return a Monday-first weekday for the first day of the requested month. */
int lab_forms_month_weekday(double year, double month)
{
    return lab_forms_date_ok(year, month, 1) ? lab_forms_serial((int)year, (int)month, 1) % 7 : -1;
}

/* Shift by whole months, saturating at January 0001 and December 9999. */
int lab_forms_shift_month(double year, double month, double delta)
{
    if (!lab_forms_date_ok(year, month, 1) || !lab_forms_integer(delta, -120000, 120000))
        return 0;
    int index = ((int)year - 1) * 12 + (int)month - 1 + (int)delta;
    index = index < 0 ? 0 : index > 119987 ? 119987 : index;
    return (index / 12 + 1) * 10000 + (index % 12 + 1) * 100 + 1;
}

/* Shift the visible year using a bounded whole-year increment. */
int lab_forms_shift_year(double year, double delta)
{
    if (!lab_forms_integer(year, 1, 9999) || !lab_forms_integer(delta, -120000, 120000))
        return 0;
    return lab_forms_clamp(year + delta, 1, 9999, year);
}

/* Return one of the 42 calendar cells; zero disables cells outside supported years. */
int lab_forms_calendar_cell(double year, double month, double slot)
{
    if (!lab_forms_date_ok(year, month, 1) || !lab_forms_integer(slot, 0, 41))
        return 0;
    int y = (int)year, m = (int)month;
    int day = (int)slot - lab_forms_serial(y, m, 1) % 7 + 1;
    if (day < 1) {
        if (--m == 0) {
            m = 12;
            --y;
        }
        if (y < 1)
            return 0;
        day += lab_forms_month_days(y, m);
    } else if (day > lab_forms_month_days(y, m)) {
        day -= lab_forms_month_days(y, m);
        if (++m == 13) {
            m = 1;
            ++y;
        }
        if (y > 9999)
            return 0;
    }
    return y * 10000 + m * 100 + day;
}

/* Read the year component of a packed calendar result. */
int lab_forms_date_year(int packed)
{
    return packed / 10000;
}

/* Read the month component of a packed calendar result. */
int lab_forms_date_month(int packed)
{
    return packed / 100 % 100;
}

/* Read the day component of a packed calendar result. */
int lab_forms_date_day(int packed)
{
    return packed % 100;
}

/* Prepare the complete six-week grid and selection policy in one bounded host call. */
const uint32_t *lab_forms_calendar_grid(double year, double month, double selected_year, double selected_month,
                                        double selected_day, double today_year, double today_month, double today_day)
{
    static uint32_t grid[42][4];
    if (!lab_forms_date_ok(year, month, 1))
        return 0;
    int selected_valid = lab_forms_date_ok(selected_year, selected_month, selected_day);
    int today_valid = lab_forms_date_ok(today_year, today_month, today_day);
    /* A calendar always contains exactly six seven-day rows, independent of input size. */
    for (unsigned slot = 0; slot < 42; ++slot) {
        int packed = lab_forms_calendar_cell(year, month, slot);
        uint32_t y = packed / 10000, m = packed / 100 % 100, d = packed % 100;
        unsigned flags = packed ? 1u : 2u;
        if (packed && (y != year || m != month))
            flags |= 2u;
        if (packed && today_valid && y == today_year && m == today_month && d == today_day)
            flags |= 4u;
        if (packed && selected_valid && y == selected_year && m == selected_month && d == selected_day)
            flags |= 8u;
        grid[slot][0] = y;
        grid[slot][1] = m;
        grid[slot][2] = d;
        grid[slot][3] = flags;
    }
    return &grid[0][0];
}

/* Preserve the selected day where possible when changing month or year. */
int lab_forms_clamp_day(double year, double month, double day)
{
    int days = lab_forms_days_in_month(year, month);
    return days ? lab_forms_clamp(day, 1, days, 1) : 0;
}

/* Select the year-navigation step from browser modifier flags. */
int lab_forms_year_step(int control, int shift)
{
    return control ? 100 : shift ? 10 : 1;
}

static int lab_forms_picker_year, lab_forms_picker_month;

/* Open a picker with bounded structured components; the host supplies its local current year. */
int lab_forms_picker_open(double year, double month, double fallback_year)
{
    lab_forms_picker_year = lab_forms_clamp(year, 1, 9999, fallback_year);
    lab_forms_picker_month = lab_forms_clamp(month, 1, 12, 1);
    return lab_forms_picker_year * 10000 + lab_forms_picker_month * 100 + 1;
}

/* Expose the visible month as a packed date; zero denotes a closed picker. */
int lab_forms_picker_date(void)
{
    return lab_forms_picker_year ? lab_forms_picker_year * 10000 + lab_forms_picker_month * 100 + 1 : 0;
}

/* Closing discards navigation state; subsequent edits cannot reopen the picker. */
void lab_forms_picker_close(void)
{
    lab_forms_picker_year = lab_forms_picker_month = 0;
}

/* Select a month/year and return a safe committed date, preserving the input's day when possible. */
int lab_forms_picker_set(double year, double month, double day, double fallback_year)
{
    if (!lab_forms_picker_year)
        return 0;
    lab_forms_picker_open(year, month, fallback_year);
    return lab_forms_picker_date() - 1 + lab_forms_clamp_day(lab_forms_picker_year, lab_forms_picker_month, day);
}

/* Navigate an open picker; invalid or fractional increments leave it unchanged. */
int lab_forms_picker_shift(double delta, int years, double day)
{
    if (!lab_forms_picker_year)
        return 0;
    int next = years ? lab_forms_shift_year(lab_forms_picker_year, delta)
                     : lab_forms_shift_month(lab_forms_picker_year, lab_forms_picker_month, delta);
    if (!next)
        return 0;
    int year = years ? next : lab_forms_date_year(next);
    int month = years ? lab_forms_picker_month : lab_forms_date_month(next);
    return lab_forms_picker_set(year, month, day, lab_forms_picker_year);
}

/* Compare numeric coordinates, rejecting missing/non-finite values and negative tolerances. */
int lab_forms_nearly_equal(double left, double right, double tolerance)
{
    if (!lab_forms_finite(left) || !lab_forms_finite(right) || !lab_forms_finite(tolerance) || tolerance < 0)
        return 0;
    double difference = left - right;
    return (difference < 0 ? -difference : difference) <= tolerance;
}

/* Convert a structured civil time to Unix milliseconds for browser timezone APIs. */
double lab_forms_utc_milliseconds(double year, double month, double day, double hour, double minute, double second)
{
    if (!lab_forms_date_ok(year, month, day) || !lab_forms_integer(hour, 0, 24) || !lab_forms_integer(minute, 0, 59) ||
        !lab_forms_integer(second, 0, 59))
        return __builtin_nan("");
    return ((double)(lab_forms_serial((int)year, (int)month, (int)day) - 719162) * 86400 + hour * 3600 + minute * 60 +
            second) *
           1000;
}

/* Derive a timezone offset after the browser has supplied its locale time components. */
double lab_forms_offset_hours(double local_milliseconds, double utc_milliseconds)
{
    if (!lab_forms_finite(local_milliseconds) || !lab_forms_finite(utc_milliseconds))
        return __builtin_nan("");
    double offset = (local_milliseconds - utc_milliseconds) / 3600000;
    return offset >= -24 && offset <= 24 ? offset : __builtin_nan("");
}

/* Round an offset to two decimal places, using browser-compatible half-up rounding. */
double lab_forms_round_offset(double value)
{
    if (!lab_forms_finite(value) || value < -24 || value > 24)
        return __builtin_nan("");
    double scaled = value * 100 + 0.5;
    int whole = (int)scaled;
    if (scaled < whole)
        --whole;
    return whole / 100.0;
}

/* Prevent removal of the final row or final integration variable. */
int lab_forms_row_removable(int free_row, int row_count, int bound_count)
{
    return row_count > 1 && (free_row || bound_count > 1);
}

/* Choose the first available name from nine preferred names followed by x1..x99. */
int lab_forms_name_candidate(uint32_t first, uint32_t second, uint32_t third, uint32_t fourth)
{
    if (~first)
        return __builtin_ctz(~first);
    if (~second)
        return 32 + __builtin_ctz(~second);
    if (~third)
        return 64 + __builtin_ctz(~third);
    uint32_t available = ~fourth & 4095u;
    return available ? 96 + __builtin_ctz(available) : 0;
}
