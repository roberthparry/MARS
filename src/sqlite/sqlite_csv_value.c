/**
 * @file sqlite_csv_value.c
 * @brief Conservative CSV numeric classification and bound-value conversion.
 *
 * Scans string_t views using a locale-independent decimal grammar. Inference
 * preserves identifiers and precision-sensitive values as text; explicit
 * numeric policies reject invalid or out-of-range values before insertion.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SQLITE_INTERNAL_ACCESS
#include "sqlite_csv_internal.h"

bool sqlite_csv_fail(sqlite_csv_context_t *c, int code, const char *message)
{
    if (!c->report->code) {
        c->report->code = code;
        sqlite_set_diagnostic_error(c->db, "sqlite.csv", code, message, NULL);
    }
    errno = c->report->code;
    return false;
}

bool sqlite_csv_sql_error(sqlite_csv_context_t *c)
{
    if (!c->report->code) {
        c->report->sqlite_code = sqlite3_extended_errcode(c->native);
        const char *message = sqlite3_errmsg(c->native);
        diagnostic_t *cause = diagnostic_new(DIAGNOSTIC_ERROR, "sqlite", c->report->sqlite_code, message, NULL, NULL);
        sqlite_csv_fail(c, EIO, message);
        sqlite_set_diagnostic_error(c->db, "sqlite.csv", EIO, message, cause);
        diagnostic_free(cause);
    }
    errno = c->report->code;
    return false;
}

bool sqlite_csv_stream_error(sqlite_csv_context_t *c, csv_t *csv)
{
    if (c->report->code) {
        errno = c->report->code;
        return false;
    }
    c->report->csv_error = csv_error(csv);
    c->report->record = c->report->csv_error.record;
    c->report->column = c->report->csv_error.field;
    int code = c->report->csv_error.code ? c->report->csv_error.code : EINVAL;
    diagnostic_t *cause = csv_diagnostic(csv);
    sqlite_csv_fail(c, code, "CSV stream failure");
    sqlite_set_diagnostic_error(c->db, "sqlite.csv", code, "CSV stream failure", cause);
    diagnostic_free(cause);
    errno = code;
    return false;
}

static bool sqlite_csv_digit(string_view_t view, size_t at, unsigned char *digit)
{
    return string_view_peek_ascii(view, at, digit) && *digit >= '0' && *digit <= '9';
}

sqlite_csv_type_t sqlite_csv_number(sqlite_csv_context_t *c, const string_t *text, int64_t *integer,
                                   double *real, size_t *significant, bool *leading)
{
    string_view_t view = string_view_all(text);
    size_t length = string_view_length(view), at = 0, digits = 0;
    unsigned char ch = 0;
    bool negative = false, decimal = false, nonzero = false, overflow = false;
    uint64_t magnitude = 0;
    *significant = 0;
    *leading = false;
    if (string_view_peek_ascii(view, at, &ch) && (ch == '+' || ch == '-')) {
        negative = ch == '-';
        ++at;
    }
    size_t first = at;
    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    while (sqlite_csv_digit(view, at, &ch)) {
        unsigned digit = ch - '0';
        if (at == first && digit == 0)
            *leading = true;
        nonzero = nonzero || digit != 0;
        *significant += nonzero;
        if (magnitude > (limit - digit) / 10)
            overflow = true;
        else if (!overflow)
            magnitude = magnitude * 10 + digit;
        ++at;
        ++digits;
    }
    *leading = *leading && digits > 1;
    if (string_view_peek_ascii(view, at, &ch) && ch == '.') {
        decimal = true;
        ++at;
        while (sqlite_csv_digit(view, at, &ch)) {
            nonzero = nonzero || ch != '0';
            *significant += nonzero;
            ++at;
            ++digits;
        }
    }
    if (!digits)
        return SQLITE_CSV_TEXT;
    if (string_view_peek_ascii(view, at, &ch) && (ch == 'e' || ch == 'E')) {
        decimal = true;
        ++at;
        if (string_view_peek_ascii(view, at, &ch) && (ch == '+' || ch == '-'))
            ++at;
        size_t exponent = at;
        while (sqlite_csv_digit(view, at, &ch))
            ++at;
        if (at == exponent)
            return SQLITE_CSV_TEXT;
    }
    if (at != length)
        return SQLITE_CSV_TEXT;
    if (!decimal && !overflow) {
        *integer = negative ? magnitude == (uint64_t)INT64_MAX + 1 ? INT64_MIN : -(int64_t)magnitude
                            : (int64_t)magnitude;
        *real = negative && !magnitude ? -0.0 : (double)*integer;
        /* Keep a signed zero's spelling unless a numeric type is explicitly requested. */
        *leading = *leading || (negative && !magnitude);
        return SQLITE_CSV_INTEGER;
    }
    errno = 0;
    char *end;
    *real = strtod_l(string_c_str(text), &end, c->locale);
    if (errno == ERANGE || !isfinite(*real) || end != string_c_str(text) + length)
        return SQLITE_CSV_TEXT;
    return SQLITE_CSV_REAL;
}

static bool sqlite_csv_value(sqlite_csv_context_t *c, size_t index, const string_t *text, bool infer,
                             sqlite3_stmt *stmt)
{
    sqlite_csv_plan_t *column = &c->columns[index];
    if (!string_byte_length(text) && c->options.empty_is_null)
        return !stmt || sqlite3_bind_null(stmt, (int)index + 1) == SQLITE_OK || sqlite_csv_sql_error(c);
    int64_t integer = 0;
    double real = 0;
    size_t significant = 0;
    bool leading = false;
    sqlite_csv_type_t kind = SQLITE_CSV_TEXT;
    if (column->type != SQLITE_CSV_TEXT)
        kind = sqlite_csv_number(c, text, &integer, &real, &significant, &leading);
    if (infer && column->automatic) {
        if (kind == SQLITE_CSV_INTEGER && (integer > INT64_C(9007199254740992) ||
                                           integer < -INT64_C(9007199254740992)))
            column->wide_integer = true;
        if (leading || (kind == SQLITE_CSV_REAL && (!c->options.infer_real || significant > 15)))
            kind = SQLITE_CSV_TEXT;
        if (column->type == SQLITE_CSV_AUTO)
            column->type = kind;
        else if (column->type != kind)
            column->type = column->type == SQLITE_CSV_TEXT || kind == SQLITE_CSV_TEXT ? SQLITE_CSV_TEXT : SQLITE_CSV_REAL;
        if (column->type == SQLITE_CSV_REAL && column->wide_integer)
            column->type = SQLITE_CSV_TEXT;
        return true;
    }
    if (column->type == SQLITE_CSV_INTEGER && kind != SQLITE_CSV_INTEGER)
        return sqlite_csv_fail(c, ERANGE, "CSV field is not a signed 64-bit integer");
    if (column->type == SQLITE_CSV_REAL && kind != SQLITE_CSV_INTEGER && kind != SQLITE_CSV_REAL)
        return sqlite_csv_fail(c, ERANGE, "CSV field is not a finite decimal number");
    if (!stmt)
        return true;
    int rc;
    if (column->type == SQLITE_CSV_INTEGER)
        rc = sqlite3_bind_int64(stmt, (int)index + 1, integer);
    else if (column->type == SQLITE_CSV_REAL)
        rc = sqlite3_bind_double(stmt, (int)index + 1, real);
    else {
        if (string_byte_length(text) > INT_MAX)
            return sqlite_csv_fail(c, EOVERFLOW, "CSV text exceeds SQLite's binding limit");
        rc = sqlite3_bind_text(stmt, (int)index + 1, string_c_str(text), (int)string_byte_length(text), SQLITE_TRANSIENT);
    }
    return rc == SQLITE_OK || sqlite_csv_sql_error(c);
}

bool sqlite_csv_row(sqlite_csv_context_t *c, csv_t *csv, bool infer, sqlite3_stmt *stmt)
{
    c->report->column = 0;
    if (csv_field_count(csv) != c->count)
        return sqlite_csv_fail(c, EINVAL, "CSV record width differs from the schema");
    for (size_t i = 0; i < c->count; ++i) {
        c->report->column = i + 1;
        if (!sqlite_csv_value(c, i, csv_field(csv, i), infer, stmt))
            return false;
    }
    c->report->column = 0;
    return true;
}
