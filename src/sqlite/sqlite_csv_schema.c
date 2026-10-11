/**
 * @file sqlite_csv_schema.c
 * @brief CSV destination names, schema validation and parameterised SQL.
 *
 * Quotes identifiers independently of field values and indexes column names
 * with SQLite-compatible ASCII case folding. Existing tables must have matching
 * supported affinities; unsupported coercions are never silently accepted.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define MARS_SQLITE_INTERNAL_ACCESS
#include "sqlite_csv_internal.h"

string_t *sqlite_csv_identifier(const string_t *text)
{
    if (!text || !string_byte_length(text) || string_byte_length(text) > INT_MAX)
        return NULL;
    string_t *out = string_new_with("\"");
    string_view_t view = string_view_all(text);
    size_t start = 0, length = string_view_length(view);
    for (size_t i = 0; out && i < length; ++i) {
        unsigned char ch;
        if (!string_view_peek_ascii(view, i, &ch))
            continue;
        if (!ch)
            goto fail;
        if (ch == '"') {
            if (string_append_utf8_exact(out, string_c_str(text) + start, i - start) != 0 ||
                string_append_bytes(out, "\"\"", 2) != 0)
                goto fail;
            start = i + 1;
        }
    }
    if (out && string_append_utf8_exact(out, string_c_str(text) + start, length - start) == 0 &&
        string_append_bytes(out, "\"", 1) == 0)
        return out;
fail:
    string_free(out);
    return NULL;
}

string_t *sqlite_csv_key(const string_t *name)
{
    string_t *key = string_new();
    string_view_t view = string_view_all(name);
    for (size_t i = 0; key && i < string_view_length(view); ++i) {
        unsigned char ch = (unsigned char)string_c_str(name)[i];
        if (ch >= 'A' && ch <= 'Z')
            ch += 'a' - 'A';
        char byte = (char)ch;
        if (string_append_bytes(key, &byte, 1) != 0) {
            string_free(key);
            return NULL;
        }
    }
    return key;
}

static size_t sqlite_csv_hash(const void *key)
{
    return string_hash(*(const string_t *const *)key);
}

static int sqlite_csv_compare(const void *a, const void *b)
{
    return string_compare(*(const string_t *const *)a, *(const string_t *const *)b);
}

void sqlite_csv_clear(sqlite_csv_context_t *c)
{
    dictionary_destroy(c->names);
    for (size_t i = 0; i < c->count; ++i) {
        if (c->columns)
            string_free(c->columns[i].name);
        if (c->keys)
            string_free(c->keys[i]);
    }
    free(c->keys);
    free(c->columns);
    string_free(c->table);
    if (c->locale)
        freelocale(c->locale);
}

bool sqlite_csv_columns(sqlite_csv_context_t *c, csv_t *csv, bool stream)
{
    size_t width = csv_header_count(csv);
    if (!width)
        width = csv_field_count(csv);
    if (!width)
        width = c->options.column_count;
    int limit = sqlite3_limit(c->native, SQLITE_LIMIT_VARIABLE_NUMBER, -1);
    if (!width || width > (size_t)limit || width > (size_t)sqlite3_limit(c->native, SQLITE_LIMIT_COLUMN, -1))
        return sqlite_csv_fail(c, EOVERFLOW, "CSV width is empty or exceeds SQLite limits");
    if ((c->options.columns && c->options.column_count != width) ||
        (!c->options.columns && c->options.column_count) || (stream && !c->options.columns))
        return sqlite_csv_fail(c, EINVAL, "CSV column overrides must match the input width");
    c->count = width;
    c->columns = calloc(width, sizeof(*c->columns));
    c->keys = calloc(width, sizeof(*c->keys));
    c->names = dictionary_create(sizeof(string_t *), sizeof(size_t), sqlite_csv_hash, sqlite_csv_compare,
                                  NULL, NULL, NULL, NULL, NULL);
    if (!c->columns || !c->keys || !c->names)
        return sqlite_csv_fail(c, ENOMEM, "Allocating CSV column plan failed");
    for (size_t i = 0; i < width; ++i) {
        c->report->column = i + 1;
        const sqlite_csv_column_t *override = c->options.columns ? &c->options.columns[i] : NULL;
        const string_t *name = override && override->name ? override->name : csv_header(csv, i);
        sqlite_csv_type_t type = override ? override->type : SQLITE_CSV_AUTO;
        if (type < SQLITE_CSV_AUTO || type > SQLITE_CSV_TEXT || (stream && type == SQLITE_CSV_AUTO))
            return sqlite_csv_fail(c, EINVAL, "Stream imports require explicit INTEGER, REAL or TEXT types");
        string_t *quoted = sqlite_csv_identifier(name);
        if (!quoted)
            return sqlite_csv_fail(c, EINVAL, "CSV columns require valid non-empty UTF-8 names");
        string_free(quoted);
        c->columns[i] = (sqlite_csv_plan_t){.name = string_clone(name), .type = type,
                                          .automatic = type == SQLITE_CSV_AUTO};
        c->keys[i] = sqlite_csv_key(name);
        if (!c->columns[i].name || !c->keys[i])
            return sqlite_csv_fail(c, ENOMEM, "Copying CSV column name failed");
        size_t duplicate;
        if (dictionary_get(c->names, &c->keys[i], &duplicate))
            return sqlite_csv_fail(c, EEXIST, "CSV column names are ambiguous under ASCII case folding");
        if (!dictionary_set(c->names, &c->keys[i], &i))
            return sqlite_csv_fail(c, ENOMEM, "Indexing CSV columns failed");
    }
    c->report->column = 0;
    return true;
}

static sqlite_csv_type_t sqlite_csv_affinity(const char *declaration)
{
    string_t *text = string_new_with(declaration ? declaration : "");
    string_t *key = text ? sqlite_csv_key(text) : NULL;
    sqlite_csv_type_t type = SQLITE_CSV_AUTO;
    if (key) {
        if (string_find(key, "int") >= 0)
            type = SQLITE_CSV_INTEGER;
        else if (string_find(key, "char") >= 0 || string_find(key, "clob") >= 0 || string_find(key, "text") >= 0)
            type = SQLITE_CSV_TEXT;
        else if (string_byte_length(key) && string_find(key, "blob") < 0 &&
                 (string_find(key, "real") >= 0 || string_find(key, "floa") >= 0 || string_find(key, "doub") >= 0))
            type = SQLITE_CSV_REAL;
    }
    string_free(key);
    string_free(text);
    return type;
}

static bool sqlite_csv_existing(sqlite_csv_context_t *c)
{
    sqlite3_stmt *table = NULL;
    const char *lookup = "SELECT type FROM main.sqlite_master WHERE name = ? COLLATE NOCASE";
    if (sqlite3_prepare_v2(c->native, lookup, -1, &table, NULL) != SQLITE_OK)
        return sqlite_csv_sql_error(c);
    int rc = sqlite3_bind_text(table, 1, string_c_str(c->table_name),
                               (int)string_byte_length(c->table_name), SQLITE_TRANSIENT);
    if (rc == SQLITE_OK)
        rc = sqlite3_step(table);
    bool is_table = rc == SQLITE_ROW && sqlite3_column_text(table, 0) &&
                    sqlite3_stricmp((const char *)sqlite3_column_text(table, 0), "table") == 0;
    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
        sqlite_csv_sql_error(c);
    sqlite3_finalize(table);
    if (!is_table)
        return sqlite_csv_fail(c, EINVAL, "CSV destination must be an existing main-schema table");
    string_t *sql = string_new_with("PRAGMA main.table_info(");
    bool ok = sql && string_append_bytes(sql, string_c_str(c->table), string_byte_length(c->table)) == 0 &&
              string_append_cstr(sql, ")") == 0;
    sqlite3_stmt *stmt = NULL;
    if (ok)
        ok = sqlite3_prepare_v2(c->native, string_c_str(sql), -1, &stmt, NULL) == SQLITE_OK;
    string_free(sql);
    if (!ok)
        return sqlite_csv_sql_error(c);
    size_t found = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        string_t *text = string_new();
        ok = text && string_append_utf8_exact(text, name, (size_t)sqlite3_column_bytes(stmt, 1)) == 0;
        string_t *key = ok ? sqlite_csv_key(text) : NULL;
        string_free(text);
        size_t column;
        if (!key) {
            ok = sqlite_csv_fail(c, ENOMEM, "Reading destination schema failed");
        } else if (dictionary_get(c->names, &key, &column)) {
            ++found;
            c->report->column = column + 1;
            if (sqlite_csv_affinity((const char *)sqlite3_column_text(stmt, 2)) != c->columns[column].type)
                ok = sqlite_csv_fail(c, EINVAL, "CSV column type is incompatible with destination affinity");
        }
        string_free(key);
        if (!ok)
            break;
    }
    if (ok && rc != SQLITE_DONE)
        ok = sqlite_csv_sql_error(c);
    sqlite3_finalize(stmt);
    if (ok && found != c->count)
        ok = sqlite_csv_fail(c, EINVAL, "Destination table is missing one or more CSV columns");
    c->report->column = ok ? 0 : c->report->column;
    return ok;
}

static string_t *sqlite_csv_sql(sqlite_csv_context_t *c, bool create)
{
    static const char *const types[] = {NULL, "INTEGER", "REAL", "TEXT"};
    string_t *sql = string_new_with(create ? "CREATE TABLE main." : "INSERT INTO main.");
    bool ok = sql && string_append_bytes(sql, string_c_str(c->table), string_byte_length(c->table)) == 0 &&
              string_append_cstr(sql, " (") == 0;
    for (size_t i = 0; ok && i < c->count; ++i) {
        string_t *name = sqlite_csv_identifier(c->columns[i].name);
        ok = name && (!i || string_append_cstr(sql, ",") == 0) &&
             string_append_bytes(sql, string_c_str(name), string_byte_length(name)) == 0;
        if (ok && create)
            ok = string_append_cstr(sql, " ") == 0 && string_append_cstr(sql, types[c->columns[i].type]) == 0;
        string_free(name);
    }
    ok = ok && string_append_cstr(sql, ")") == 0;
    if (ok && !create) {
        ok = string_append_cstr(sql, " VALUES (") == 0;
        for (size_t i = 0; ok && i < c->count; ++i)
            ok = string_append_cstr(sql, i ? ",?" : "?") == 0;
        ok = ok && string_append_cstr(sql, ")") == 0;
    }
    if (!ok) {
        string_free(sql);
        sql = NULL;
    }
    return sql;
}

bool sqlite_csv_schema(sqlite_csv_context_t *c, sqlite3_stmt **statement)
{
    for (size_t i = 0; i < c->count; ++i)
        if (c->columns[i].type == SQLITE_CSV_AUTO)
            c->columns[i].type = SQLITE_CSV_TEXT;
    if (c->options.create_table) {
        string_t *sql = sqlite_csv_sql(c, true);
        if (!sql)
            return sqlite_csv_fail(c, ENOMEM, "Building CSV table schema failed");
        int rc = sqlite3_exec(c->native, string_c_str(sql), NULL, NULL, NULL);
        string_free(sql);
        if (rc != SQLITE_OK)
            return sqlite_csv_sql_error(c);
    } else if (!sqlite_csv_existing(c)) {
        return false;
    }
    string_t *sql = sqlite_csv_sql(c, false);
    if (!sql)
        return sqlite_csv_fail(c, ENOMEM, "Building CSV insert failed");
    int rc = sqlite3_prepare_v2(c->native, string_c_str(sql), -1, statement, NULL);
    string_free(sql);
    return rc == SQLITE_OK || sqlite_csv_sql_error(c);
}
