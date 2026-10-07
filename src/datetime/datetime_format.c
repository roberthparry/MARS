/**
 * @file datetime_format.c
 * @brief Date and time token formatting.
 *
 * Parses format tokens with string cursors and builds formatted text, including calendar names and ordinal forms.
 * Use this unit for presentation conventions, not date arithmetic or timezone policy.
 *
 * This is part of the datetime.h implementation. Jurisdiction holiday policy belongs to the jurisdiction module,
 * while catalogue-backed apparent sky positions belong to almanac.
 */

/* Date/time token formatting and string-builder helpers. */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "datetime_internal.h"
#include "ustring.h"

static char datetime_ascii_upper(char ch)
{
    return (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
}

static int datetime_cursor_peek_ascii(const string_cursor_t *cursor, char *out)
{
    unsigned char ascii = 0u;

    if (!cursor || string_cursor_done(cursor) || !string_cursor_peek_ascii(cursor, &ascii))
        return 0;
    if (out)
        *out = (char)ascii;
    return 1;
}

static int datetime_cursor_peek_matches(const string_cursor_t *cursor, char lower, char upper)
{
    char ch = '\0';

    return datetime_cursor_peek_ascii(cursor, &ch) && (ch == lower || ch == upper);
}

static int datetime_cursor_advance(string_cursor_t *cursor)
{
    return string_cursor_next(cursor) == 0;
}

static size_t datetime_cursor_count_run(const string_cursor_t *cursor, char lower, char upper)
{
    string_cursor_t *scan = string_cursor_clone(cursor);
    size_t count = 0u;

    if (!scan)
        return 0u;

    while (datetime_cursor_peek_matches(scan, lower, upper)) {
        count++;
        if (!datetime_cursor_advance(scan))
            break;
    }

    string_cursor_free(scan);
    return count;
}

static int datetime_cursor_second_is(const string_cursor_t *cursor, char expected)
{
    string_cursor_t *scan = string_cursor_clone(cursor);
    char ch = '\0';
    int matches = 0;

    if (!scan)
        return 0;
    if (datetime_cursor_advance(scan) && datetime_cursor_peek_ascii(scan, &ch) && ch == expected)
        matches = 1;

    string_cursor_free(scan);
    return matches;
}

static void datetime_cursor_skip_run(string_cursor_t *cursor, size_t count)
{
    while (count-- > 0u && !string_cursor_done(cursor))
        (void)datetime_cursor_advance(cursor);
}

static char *datetime_format_export(const string_t *text)
{
    const char *raw = string_c_str(text);
    size_t len = string_view_length(string_view_all(text)) + 1u;
    char *out = malloc(len);

    if (!out)
        return NULL;
    memcpy(out, raw, len);
    return out;
}

static void datetime_format_append_text(string_t *out, int *failed, const char *text)
{
    if (*failed)
        return;
    if (string_append_cstr(out, text) != 0)
        *failed = 1;
}

static void datetime_format_append_name(string_t *out, int *failed, const char *name, size_t max_chars, int title_case,
    int upper_case)
{
    string_t *text;
    string_cursor_t *cursor;
    size_t written = 0u;

    if (*failed)
        return;

    text = string_new_with(name);
    cursor = text ? string_cursor_new(text) : NULL;
    if (!text || !cursor) {
        *failed = 1;
        goto done;
    }

    while (!string_cursor_done(cursor) && written < max_chars) {
        char ascii = '\0';
        rune_t rune = string_cursor_peek(cursor);

        if (rune_to_ascii(rune, &ascii) && (upper_case || (title_case && written == 0u))) {
            if (string_append_char(out, datetime_ascii_upper(ascii)) != 0) {
                *failed = 1;
                goto done;
            }
        } else if (string_append_rune(out, rune) != 0) {
            *failed = 1;
            goto done;
        }

        written++;
        if (!datetime_cursor_advance(cursor)) {
            *failed = 1;
            goto done;
        }
    }

done:
    string_cursor_free(cursor);
    string_free(text);
}

static void datetime_format_append_char(string_t *out, int *failed, char ch)
{
    if (*failed)
        return;
    if (string_append_char(out, ch) != 0)
        *failed = 1;
}

static void datetime_format_append_format(string_t *out, int *failed, const char *format, int value)
{
    if (*failed)
        return;
    if (string_append_format(out, format, value) < 0)
        *failed = 1;
}

/* Format a datetime as a newly allocated string object. */
string_t *datetime_format_text(const datetime_t *dttm, const string_t *format)
{
    static const char *weekdayNames[] = {NULL,        "sunday",   "monday", "tuesday",
                                         "wednesday", "thursday", "friday", "saturday"};
    static const char *monthNames[] = {NULL,   "january", "february",  "march",   "april",    "may",     "june",
                                       "july", "august",  "september", "october", "november", "december"};
    string_t *formattedString;
    string_cursor_t *cursor;
    int append_failed = 0;

    if (!dttm || !format)
        return NULL;
    if (dttm->year == SHRT_MAX && datetime_year(dttm) == SHRT_MAX)
        return NULL;

    formattedString = string_new();
    cursor = formattedString ? string_cursor_new(format) : NULL;

    if (!formattedString || !cursor) {
        string_free(formattedString);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        char marker = '\0';

        if (!datetime_cursor_peek_ascii(cursor, &marker)) {
            if (string_append_rune(formattedString, string_cursor_peek(cursor)) != 0)
                append_failed = 1;
            (void)datetime_cursor_advance(cursor);
            continue;
        }

        if (marker == '%') {
            (void)datetime_cursor_advance(cursor);
            if (string_cursor_done(cursor)) {
                datetime_format_append_char(formattedString, &append_failed, '%');
                break;
            }

            char token = '\0';
            (void)datetime_cursor_peek_ascii(cursor, &token);
            switch (token) {
                case '%':
                    datetime_format_append_char(formattedString, &append_failed, '%');
                    (void)datetime_cursor_advance(cursor);
                    break;

                case 'd':
                case 'D': {
                    size_t run = datetime_cursor_count_run(cursor, 'd', 'D');
                    int all_caps = token == 'D' && datetime_cursor_second_is(cursor, 'D');
                    switch (run) {
                        case 1:
                            datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->day);
                            datetime_cursor_skip_run(cursor, 1u);
                            break;
                        case 2:
                            datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->day);
                            datetime_cursor_skip_run(cursor, 2u);
                            break;
                        case 3:
                            datetime_format_append_name(formattedString, &append_failed,
                                                        weekdayNames[datetime_weekday(dttm)], 3u,
                                                        token == 'D' && !all_caps, all_caps);
                            datetime_cursor_skip_run(cursor, 3u);
                            break;
                        case 4:
                        default:
                            datetime_format_append_name(formattedString, &append_failed,
                                                        weekdayNames[datetime_weekday(dttm)], SIZE_MAX,
                                                        token == 'D' && !all_caps, all_caps);
                            datetime_cursor_skip_run(cursor, 4u);
                            break;
                    }
                    break;
                }

                case 'o':
                case 'O': {
                    const char *suffix = "th";

                    if (!(10 < dttm->day && dttm->day < 20)) {
                        switch (dttm->day % 10) {
                            case 1:
                                suffix = "st";
                                break;
                            case 2:
                                suffix = "nd";
                                break;
                            case 3:
                                suffix = "rd";
                                break;
                            default:
                                suffix = "th";
                                break;
                        }
                    }
                    datetime_format_append_name(formattedString, &append_failed, suffix, SIZE_MAX, 0, token == 'O');
                    (void)datetime_cursor_advance(cursor);
                    break;
                }

                case 'q':
                case 'Q': {
                    const char *suffix = "ᵗʰ";

                    if (!(10 < dttm->day && dttm->day < 20)) {
                        switch (dttm->day % 10) {
                            case 1:
                                suffix = "ˢᵗ";
                                break;
                            case 2:
                                suffix = "ⁿᵈ";
                                break;
                            case 3:
                                suffix = "ʳᵈ";
                                break;
                            default:
                                suffix = "ᵗʰ";
                                break;
                        }
                    }
                    datetime_format_append_name(formattedString, &append_failed, suffix, SIZE_MAX, 0, 0);
                    (void)datetime_cursor_advance(cursor);
                    break;
                }

                case 'm':
                case 'M': {
                    size_t run = datetime_cursor_count_run(cursor, 'm', 'M');
                    int all_caps = token == 'M' && datetime_cursor_second_is(cursor, 'M');
                    switch (run) {
                        case 1:
                            datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->month);
                            datetime_cursor_skip_run(cursor, 1u);
                            break;
                        case 2:
                            datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->month);
                            datetime_cursor_skip_run(cursor, 2u);
                            break;
                        case 3:
                            datetime_format_append_name(formattedString, &append_failed, monthNames[dttm->month], 3u,
                                                        token == 'M' && !all_caps, all_caps);
                            datetime_cursor_skip_run(cursor, 3u);
                            break;
                        case 4:
                        default:
                            datetime_format_append_name(formattedString, &append_failed, monthNames[dttm->month],
                                                        SIZE_MAX, token == 'M' && !all_caps, all_caps);
                            datetime_cursor_skip_run(cursor, 4u);
                            break;
                    }
                    break;
                }

                case 'y':
                case 'Y': {
                    size_t run = datetime_cursor_count_run(cursor, 'y', 'Y');
                    if (run < 4u) {
                        int year = dttm->year - 100 * (dttm->year / 100);
                        datetime_format_append_format(formattedString, &append_failed, "%02i", year);
                        datetime_cursor_skip_run(cursor, run);
                    } else {
                        datetime_format_append_format(formattedString, &append_failed, "%i", dttm->year);
                        datetime_cursor_skip_run(cursor, 4u);
                    }
                    break;
                }

                default:
                    datetime_format_append_char(formattedString, &append_failed, '%');
                    break;
            }
        } else if (marker == '@') {
            (void)datetime_cursor_advance(cursor);
            if (string_cursor_done(cursor)) {
                datetime_format_append_char(formattedString, &append_failed, '%');
                break;
            }

            char token = '\0';
            (void)datetime_cursor_peek_ascii(cursor, &token);
            switch (token) {
                case 'h':
                case 'H': {
                    int hour = dttm->hour;
                    if (token == 'h' && dttm->hour > 12)
                        hour -= 12;
                    (void)datetime_cursor_advance(cursor);
                    if (datetime_cursor_peek_matches(cursor, 'h', 'H')) {
                        datetime_format_append_format(formattedString, &append_failed, "%02i", hour);
                        (void)datetime_cursor_advance(cursor);
                    } else {
                        datetime_format_append_format(formattedString, &append_failed, "%i", hour);
                    }
                    break;
                }

                case 'M':
                case 'm':
                    (void)datetime_cursor_advance(cursor);
                    if (datetime_cursor_peek_matches(cursor, 'm', 'M')) {
                        datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->minute);
                        (void)datetime_cursor_advance(cursor);
                    } else {
                        datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->minute);
                    }
                    break;

                case 'S':
                case 's':
                    (void)datetime_cursor_advance(cursor);
                    if (datetime_cursor_peek_matches(cursor, 's', 'S')) {
                        datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->second);
                        (void)datetime_cursor_advance(cursor);
                    } else {
                        datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->second);
                    }
                    break;

                case 'P':
                    if (dttm->hour >= 12)
                        datetime_format_append_text(formattedString, &append_failed, "PM");
                    else
                        datetime_format_append_text(formattedString, &append_failed, "AM");
                    (void)datetime_cursor_advance(cursor);
                    break;

                case 'p':
                    if (dttm->hour >= 12)
                        datetime_format_append_text(formattedString, &append_failed, "pm");
                    else
                        datetime_format_append_text(formattedString, &append_failed, "am");
                    (void)datetime_cursor_advance(cursor);
                    break;

                case '@':
                    datetime_format_append_char(formattedString, &append_failed, '@');
                    (void)datetime_cursor_advance(cursor);
                    break;

                default:
                    datetime_format_append_char(formattedString, &append_failed, '@');
                    break;
            }
        } else {
            if (marker == '^')
                (void)datetime_cursor_advance(cursor);
            else {
                datetime_format_append_char(formattedString, &append_failed, marker);
                (void)datetime_cursor_advance(cursor);
            }
        }
    }

    if (append_failed)
        goto error;

    string_cursor_free(cursor);
    return formattedString;

error:
    string_cursor_free(cursor);
    string_free(formattedString);
    return NULL;
}

/* Format a datetime as a newly allocated C string. */
char *datetime_format(const datetime_t *dttm, const char *format)
{
    string_t *format_text;
    string_t *formatted_text;
    char *result;

    if (format == NULL)
        return NULL;

    format_text = string_new_with(format);
    formatted_text = format_text ? datetime_format_text(dttm, format_text) : NULL;
    string_free(format_text);
    if (!formatted_text)
        return NULL;

    result = datetime_format_export(formatted_text);
    string_free(formatted_text);
    return result;
}
