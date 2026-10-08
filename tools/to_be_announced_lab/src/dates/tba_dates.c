/**
 * @file tba_dates.c
 * @brief String-cursor date parsing and native Gregorian forecast arithmetic.
 *
 * Rejects impossible dates and trailing input. Period stepping is bounded and
 * preserves month-end clamping without libc timezone or locale dependencies.
 */
#include <string.h>

#include "datetime.h"
#include "tba_dates.h"

/* Parse precisely three numeric components separated by a consistent delimiter. */
int tba_date_parse(const string_t *text)
{
    if (!text || string_byte_length(text) > 32)
        return 0;
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        return 0;
    string_cursor_skip_spaces(cursor);
    unsigned parts[3] = {0}, widths[3] = {0};
    unsigned char delimiter = 0, ch;
    bool ok = true;
    for (size_t i = 0; ok && i < 3; ++i) {
        while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9' && widths[i] < 4) {
            parts[i] = parts[i] * 10 + ch - '0';
            widths[i]++;
            string_cursor_next(cursor);
        }
        ok = widths[i] > 0;
        if (i < 2) {
            ok = ok && string_cursor_peek_ascii(cursor, &ch) && (ch == '/' || ch == '-') &&
                 (!delimiter || delimiter == ch);
            delimiter = ch;
            string_cursor_next(cursor);
        }
    }
    string_cursor_skip_spaces(cursor);
    ok = ok && string_cursor_done(cursor);
    unsigned year = widths[0] == 4 ? parts[0] : parts[2];
    unsigned month = parts[1], day = widths[0] == 4 ? parts[2] : parts[0];
    if (widths[0] != 4 && widths[2] == 2)
        year += year <= 68 ? 2000 : 1900;
    ok = ok && year >= 1 && year <= 9999 && month <= 12 && day <= 31 &&
         datetime_valid_ymd((short)year, (month_t)month, (uint8_t)day);
    string_cursor_free(cursor);
    return ok ? (int)(year * 10000 + month * 100 + day) : 0;
}

/* Adapt borrowed text to the cursor parser. */
int tba_date_cstr(const char *text)
{
    string_t *s = string_new_with(text ? text : "");
    int date = tba_date_parse(s);
    string_free(s);
    return date;
}

/* Format a validated date for controls or UK labels. */
string_t *tba_date_text(int date, bool uk)
{
    if (!date)
        return string_new();
    return uk ? string_sprintf("%02d/%02d/%04d", date % 100, date / 100 % 100, date / 10000)
              : string_sprintf("%04d-%02d-%02d", date / 10000, date / 100 % 100, date % 100);
}

/* Use MARS Gregorian day arithmetic. */
long tba_date_jdn(int date)
{
    return date ? datetime_ymd_to_jdn((short)(date / 10000), (month_t)(date / 100 % 100), (uint8_t)(date % 100)) : 0;
}

/* Advance by a day or the specified month interval. */
int tba_date_next(int date, const char *frequency)
{
    if (!date || !frequency)
        return 0;
    if (!strcmp(frequency, "daily")) {
        datetime_t *value = datetime_alloc();
        int result = 0;
        if (value && datetime_init_jdn(value, tba_date_jdn(date) + 1))
            result = datetime_year(value) * 10000 + datetime_month(value) * 100 + datetime_day(value);
        datetime_dealloc(value);
        return result <= 99991231 ? result : 0;
    }
    int months = !strcmp(frequency, "monthly") ? 1 : !strcmp(frequency, "quarterly") ? 3 :
                 !strcmp(frequency, "yearly") ? 12 : 0;
    if (!months)
        return 0;
    int index = date / 10000 * 12 + date / 100 % 100 - 1 + months;
    int year = index / 12, month = index % 12 + 1, day = date % 100;
    if (year > 9999)
        return 0;
    int last = datetime_days_in_month((short)year, (month_t)month);
    return year * 10000 + month * 100 + (day < last ? day : last);
}

/* Normalise to month, quarter or calendar/fiscal year end. */
int tba_date_end(int date, const char *frequency, bool fiscal)
{
    if (!date || !frequency)
        return 0;
    int year = date / 10000, month = date / 100 % 100;
    if (!strcmp(frequency, "daily"))
        return date;
    if (!strcmp(frequency, "quarterly"))
        month = (month + 2) / 3 * 3;
    else if (!strcmp(frequency, "yearly")) {
        if (fiscal && month > 3)
            year++;
        month = fiscal ? 3 : 12;
    } else if (strcmp(frequency, "monthly"))
        return 0;
    return year <= 9999 ? year * 10000 + month * 100 + datetime_days_in_month((short)year, (month_t)month) : 0;
}

/* Count a bounded sequence of strictly advancing periods. */
size_t tba_date_horizon(int start, int end, const char *frequency, bool fiscal)
{
    end = tba_date_end(end, frequency, fiscal);
    for (size_t count = 1; start && end > start && count <= 100000; ++count) {
        int next = tba_date_end(tba_date_next(start, frequency), frequency, fiscal);
        if (next <= start)
            return 0;
        if (next >= end)
            return count;
        start = next;
    }
    return 0;
}

/* Return a label from the bounded frequency vocabulary. */
const char *tba_date_frequency_label(const char *frequency)
{
    static const char *const tokens[] = {"daily", "monthly", "quarterly", "yearly"};
    static const char *const labels[] = {"Daily", "Monthly", "Quarterly", "Yearly"};
    for (size_t i = 0; i < 4; ++i)
        if (frequency && !strcmp(frequency, tokens[i]))
            return labels[i];
    return "Unknown";
}
