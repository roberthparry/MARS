/**
 * @file sqlite_csv_export.c
 * @brief Bounded read-only query export through the CSV writer.
 *
 * Emits result names and typed values without interpreting CSV syntax. BLOBs,
 * non-finite reals, embedded NUL and malformed UTF-8 are rejected. The caller
 * chooses output file ownership and any atomic-publication policy.
 */
#define MARS_SQLITE_INTERNAL_ACCESS
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "sqlite_csv_internal.h"

static bool sqlite_csv_query(sqlite_csv_context_t *c, const string_t *query, sqlite3_stmt **stmt)
{
    size_t length = string_byte_length(query);
    if (!query || !length || length > INT_MAX)
        return sqlite_csv_fail(c, EINVAL, "CSV export requires one read-only query");
    string_view_t view = string_view_all(query);
    /* Some PRAGMAs take effect during prepare. Reject non-query prefixes before preparing anything. */
    string_view_t trimmed = string_view_trim(view);
    size_t word_length = 0;
    unsigned char byte;
    while (string_view_peek_ascii(trimmed, word_length, &byte) &&
           ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')))
        ++word_length;
    string_view_t word_view = string_view_slice(trimmed, 0, word_length);
    string_t *word = string_from_view(&word_view);
    string_t *key = word ? sqlite_csv_key(word) : NULL;
    bool query_prefix = key && (string_view_equals_literal(string_view_all(key), "select") ||
                                string_view_equals_literal(string_view_all(key), "with"));
    string_free(key);
    string_free(word);
    if (!query_prefix)
        return sqlite_csv_fail(c, EINVAL, "CSV export query must begin with SELECT or WITH");
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch;
        if (string_view_peek_ascii(view, i, &ch) && !ch)
            return sqlite_csv_fail(c, EINVAL, "CSV export query contains NUL");
    }
    const char *tail = NULL;
    if (sqlite3_prepare_v2(c->native, string_c_str(query), (int)length, stmt, &tail) != SQLITE_OK)
        return sqlite_csv_sql_error(c);
    if (!*stmt || !sqlite3_stmt_readonly(*stmt) || !sqlite3_column_count(*stmt) || sqlite3_bind_parameter_count(*stmt))
        return sqlite_csv_fail(c, EINVAL, "CSV export requires a read-only result without parameters");
    size_t offset = (size_t)(tail - string_c_str(query));
    if (string_view_length(string_view_trim(string_view_slice(view, offset, length - offset))))
        return sqlite_csv_fail(c, EINVAL, "CSV export accepts only one statement");
    return true;
}

static string_t *sqlite_csv_cell(sqlite_csv_context_t *c, sqlite3_stmt *stmt, int column)
{
    int type = sqlite3_column_type(stmt, column);
    if (type == SQLITE_BLOB) {
        sqlite_csv_fail(c, EINVAL, "CSV cannot represent a binary BLOB without an explicit SQL encoding");
        return NULL;
    }
    string_t *text = string_new();
    if (!text)
        return NULL;
    if (type == SQLITE_NULL)
        return text;
    if (type == SQLITE_FLOAT) {
        double number = sqlite3_column_double(stmt, column);
        char bytes[64];
        if (!isfinite(number)) {
            sqlite_csv_fail(c, ERANGE, "CSV export rejects non-finite REAL values");
            string_free(text);
            return NULL;
        }
        sqlite3_snprintf(sizeof(bytes), bytes, "%!.17g", number);
        if (string_append_cstr(text, bytes) == 0)
            return text;
    } else {
        const char *bytes = (const char *)sqlite3_column_text(stmt, column);
        int length = sqlite3_column_bytes(stmt, column);
        if (bytes && string_append_bytes(text, bytes, (size_t)length) == 0)
            return text;
    }
    string_free(text);
    return NULL;
}

/* Export typed rows without retaining the entire query result in memory. */
bool sqlite_export_csv(sqlite_t *db, const string_t *query, csv_t *writer, bool header, sqlite_csv_report_t *report)
{
    sqlite_csv_report_t local;
    if (!report)
        report = &local;
    *report = (sqlite_csv_report_t){0};
    sqlite_csv_context_t c = {.db = db, .native = sqlite_native_handle(db), .report = report};
    sqlite3_stmt *stmt = NULL;
    bool ok = c.native && writer;
    if (!ok)
        sqlite_csv_fail(&c, EINVAL, "CSV export requires a database and writer");
    if (ok)
        ok = sqlite_csv_query(&c, query, &stmt);
    size_t count = ok ? (size_t)sqlite3_column_count(stmt) : 0;
    string_t **fields = ok ? calloc(count, sizeof(*fields)) : NULL;
    if (ok && !fields)
        ok = sqlite_csv_fail(&c, ENOMEM, "Allocating CSV export row failed");
    if (ok && header) {
        for (size_t i = 0; ok && i < count; ++i) {
            const char *name = sqlite3_column_name(stmt, (int)i);
            fields[i] = string_new();
            /* Column names are NUL-terminated SQLite metadata, not input parsing. */
            size_t length = 0;
            while (name && name[length])
                ++length;
            ok = fields[i] && name && string_append_utf8_exact(fields[i], name, length) == 0;
        }
        if (!ok)
            sqlite_csv_fail(&c, EILSEQ, "CSV result column name is unavailable or invalid UTF-8");
        if (ok && !csv_set_header(writer, (const string_t *const *)fields, count))
            ok = sqlite_csv_stream_error(&c, writer);
        for (size_t i = 0; i < count; ++i) {
            string_free(fields[i]);
            fields[i] = NULL;
        }
    }
    int rc = SQLITE_DONE;
    while (ok && (rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        report->record = report->rows + (header ? 2 : 1);
        for (size_t i = 0; ok && i < count; ++i) {
            report->column = i + 1;
            fields[i] = sqlite_csv_cell(&c, stmt, (int)i);
            if (!fields[i])
                ok = sqlite_csv_fail(&c, ENOMEM, "Reading CSV export value failed");
        }
        if (ok && !csv_write(writer, (const string_t *const *)fields, count))
            ok = sqlite_csv_stream_error(&c, writer);
        for (size_t i = 0; i < count; ++i) {
            string_free(fields[i]);
            fields[i] = NULL;
        }
        if (ok)
            ++report->rows;
    }
    if (ok && rc != SQLITE_DONE)
        ok = sqlite_csv_sql_error(&c);
    sqlite3_finalize(stmt);
    free(fields);
    if (ok && !csv_finish(writer))
        ok = sqlite_csv_stream_error(&c, writer);
    if (ok) {
        report->record = report->column = 0;
        sqlite_set_error(db, "");
    }
    return ok;
}
