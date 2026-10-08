/**
 * @file lab_calendar_parse.c
 * @brief Cursor-based scalar parsing shared by native calendar adapters.
 *
 * Delimited fields retain empty columns. Decimal input is checked as ASCII
 * syntax before conversion, so non-finite values and trailing text never turn
 * into plausible default numbers. C strings are only borrowed API boundaries.
 */
#include <math.h>

#include "lab_calendar_internal.h"

/* Preserve empty fields by using the view-based split API. */
string_t **lab_cal_split(const string_t *text, const char *delimiter, size_t *count)
{
    string_t *separator = string_new_with(delimiter);
    string_t **parts = string_split_string(text, separator, count);
    string_free(separator);
    return parts;
}

static unsigned lab_cal_digits(string_cursor_t *cursor, long double *value)
{
    unsigned count = 0;
    unsigned char ch;
    while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9') {
        *value = *value * 10 + ch - '0';
        ++count;
        string_cursor_next(cursor);
    }
    return count;
}

/* Decode a finite decimal without raw byte scans or locale-sensitive conversion. */
bool lab_cal_number(const char *text, double *value)
{
    string_t *source = string_new_with(text);
    string_cursor_t *cursor = string_cursor_new(source);
    if (!cursor) {
        string_free(source);
        return false;
    }
    bool negative = string_cursor_consume(cursor, "-");
    if (!negative)
        string_cursor_consume(cursor, "+");
    long double significand = 0, fraction = 0, exponent = 0;
    unsigned whole_count = lab_cal_digits(cursor, &significand), fraction_count = 0;
    if (string_cursor_consume(cursor, "."))
        fraction_count = lab_cal_digits(cursor, &fraction);
    bool valid = whole_count || fraction_count;
    bool exponent_negative = false;
    if (string_cursor_consume(cursor, "e") || string_cursor_consume(cursor, "E")) {
        exponent_negative = string_cursor_consume(cursor, "-");
        if (!exponent_negative)
            string_cursor_consume(cursor, "+");
        valid = lab_cal_digits(cursor, &exponent) && valid;
    }
    valid = valid && string_cursor_done(cursor) && exponent <= 10000;
    long double result = (significand + fraction * powl(10, -(long double)fraction_count)) *
                         powl(10, exponent_negative ? -exponent : exponent);
    *value = (double)(negative ? -result : result);
    valid = valid && isfinite(*value);
    string_cursor_free(cursor);
    string_free(source);
    return valid;
}

/* Reject signs, overflow and empty integer fields. */
bool lab_cal_integer(const char *text, unsigned maximum, unsigned *value)
{
    string_t *source = string_new_with(text);
    string_cursor_t *cursor = string_cursor_new(source);
    unsigned result = 0, count = 0;
    unsigned char ch;
    bool valid = cursor != NULL;
    while (valid && string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9') {
        unsigned digit = ch - '0';
        if (digit > maximum || result > (maximum - digit) / 10) {
            valid = false;
            break;
        }
        result = result * 10 + digit;
        ++count;
        string_cursor_next(cursor);
    }
    valid = valid && count && string_cursor_done(cursor);
    string_cursor_free(cursor);
    string_free(source);
    if (valid)
        *value = result;
    return valid;
}
