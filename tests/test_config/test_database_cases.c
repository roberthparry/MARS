/**
 * @file test_database_cases.c
 * @brief Snapshot module-specific test rows before calling mathematical or SQLite code under test.
 *
 * An indexed database registry selects validated SQL table identifiers and optional operation groups. Inputs
 * and expectations belong to the returned array; no SQLite connection or statement survives loading. Native runners decide what
 * operation means and reject unknown operations instead of interpreting arbitrary executable source.
 */
#include <stdlib.h>
#include <string.h>

#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

static bool test_database_identifier(const char *text)
{
    if (!text || !*text || strlen(text) > 128u)
        return false;
    for (size_t i = 0u; text[i]; ++i) {
        unsigned char c = (unsigned char)text[i];
        bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        if (!letter && !(i && c >= '0' && c <= '9'))
            return false;
    }
    return true;
}

static string_t *test_database_module_table(test_database_t *database, const char *module, string_t **operation)
{
    *operation = NULL;
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT table_name, operation FROM test_case_runner WHERE module = ?1");
    if (!statement)
        return NULL;
    sqlite3_bind_text(statement, 1, module, -1, SQLITE_TRANSIENT);
    bool row = sqlite3_step(statement) == SQLITE_ROW;
    const char *name = row ? (const char *)sqlite3_column_text(statement, 0) : NULL;
    string_t *table = test_database_identifier(name) ? string_new_with(name) : NULL;
    if (table && sqlite3_column_type(statement, 1) != SQLITE_NULL) {
        *operation = string_new_with((const char *)sqlite3_column_text(statement, 1));
        if (!*operation) {
            string_free(table);
            table = NULL;
        }
    }
    sqlite3_finalize(statement);
    if (!table)
        test_database_fail(database, "Unknown test-case group or invalid table mapping; run make test-db-sync.");
    return table;
}

/* Load one registered module's cases as owned strings, independently of its test implementation. */
test_database_case_t *test_database_cases_load(const string_t *root, const string_t *path, const char *module,
                                               size_t *count, string_t **error)
{
    if (count)
        *count = 0u;
    if (error)
        *error = NULL;
    if (!count || !module || !*module) {
        if (error)
            *error = string_new_with("Unknown test-case module.");
        return NULL;
    }
    test_database_t *database = test_database_open(root, path, true, error);
    string_t *operation = NULL;
    string_t *table = database ? test_database_module_table(database, module, &operation) : NULL;
    string_t *sql = table ? string_sprintf(
        "SELECT name, operation, input, expected, precision\n"
        "FROM \"%s\"\n"
        "WHERE enabled = 1%s\n"
        "ORDER BY name", string_c_str(table), operation ? " AND operation = ?1" : "") : NULL;
    sqlite3_stmt *statement = database && sql ? test_database_prepare(database, string_c_str(sql)) : NULL;
    if (statement && operation)
        sqlite3_bind_text(statement, 1, string_c_str(operation), -1, SQLITE_TRANSIENT);
    test_database_case_t *cases = calloc(1u, sizeof *cases);
    size_t capacity = cases ? 1u : 0u;
    bool ok = statement && cases;
    int status = SQLITE_DONE;
    while (ok && (status = sqlite3_step(statement)) == SQLITE_ROW) {
        if (*count == capacity) {
            if (capacity >= 100000u) {
                ok = test_database_fail(database, "A module case table exceeds the 100000-row import limit.");
                break;
            }
            size_t next = capacity < 50000u ? capacity * 2u : 100000u;
            test_database_case_t *grown = realloc(cases, next * sizeof *grown);
            if (!grown) {
                ok = false;
                break;
            }
            cases = grown;
            capacity = next;
        }
        test_database_case_t *entry = &cases[(*count)++];
        *entry = (test_database_case_t){0};
        string_t **fields[] = {&entry->name, &entry->operation, &entry->input, &entry->expected};
        for (int i = 0; ok && i < 4; ++i) {
            const char *text = (const char *)sqlite3_column_text(statement, i);
            size_t size = (size_t)sqlite3_column_bytes(statement, i);
            ok = text && (i >= 2 || strlen(text) == size) && (*fields[i] = string_new()) != NULL &&
                 !string_append_bytes(*fields[i], text, size);
        }
        int precision = sqlite3_column_int(statement, 4);
        ok = ok && precision > 0 && precision <= 10000;
        entry->precision = (unsigned)precision;
    }
    ok = ok && status == SQLITE_DONE;
    sqlite3_finalize(statement);
    string_free(sql);
    string_free(table);
    string_free(operation);
    if (!ok) {
        if (error && !*error)
            *error = string_new_with(test_database_error(database));
        test_database_cases_free(cases, *count);
        cases = NULL;
        *count = 0u;
    }
    test_database_close(database);
    return cases;
}

/* Synchronisation enumerates every declared module row once, without executing its native checking callback. */
bool test_database_import_cases(test_database_t *database)
{
    sqlite3_stmt *runners = test_database_prepare(database,
        "SELECT module, source, parent, table_name, operation FROM test_case_runner ORDER BY module");
    bool ok = runners != NULL;
    int status = SQLITE_DONE;
    while (ok && (status = sqlite3_step(runners)) == SQLITE_ROW) {
        const char *source = (const char *)sqlite3_column_text(runners, 1);
        const char *parent = (const char *)sqlite3_column_text(runners, 2);
        const char *table = (const char *)sqlite3_column_text(runners, 3);
        bool filtered = sqlite3_column_type(runners, 4) != SQLITE_NULL;
        string_t *sql = test_database_identifier(table) ? string_sprintf(
            "SELECT name\nFROM \"%s\"%s\nORDER BY name", table, filtered ? "\nWHERE operation = ?1" : "") : NULL;
        sqlite3_stmt *rows = sql ? test_database_prepare(database, string_c_str(sql)) : NULL;
        if (rows && filtered)
            sqlite3_bind_value(rows, 1, sqlite3_column_value(runners, 4));
        ok = rows && source && parent;
        const char *separator = parent ? strrchr(parent, '.') : NULL;
        string_t *ancestor = separator ? string_new() : string_new_with("");
        if (ancestor && separator && string_append_bytes(ancestor, parent, (size_t)(separator - parent))) {
            string_free(ancestor);
            ancestor = NULL;
        }
        test_database_result_t group = {.source = source, .path = parent, .name = separator ? separator + 1 : parent,
            .parent = ancestor ? string_c_str(ancestor) : NULL, .is_group = true};
        ok = ok && ancestor && test_database_catalogue(database, &group);
        string_free(ancestor);
        int row_status = SQLITE_DONE;
        while (ok && (row_status = sqlite3_step(rows)) == SQLITE_ROW) {
            const char *name = (const char *)sqlite3_column_text(rows, 0);
            string_t *path = name ? string_sprintf("%s.%s", parent, name) : NULL;
            test_database_result_t record = {.source = source, .parent = parent, .name = name,
                .path = path ? string_c_str(path) : NULL};
            ok = path && test_database_catalogue(database, &record);
            string_free(path);
        }
        ok = ok && row_status == SQLITE_DONE;
        sqlite3_finalize(rows);
        string_free(sql);
    }
    sqlite3_finalize(runners);
    return (ok && status == SQLITE_DONE) || test_database_fail(database, "Cannot synchronise module case identities.");
}

/* Release a loaded array only after all synchronous case callbacks have returned. */
void test_database_cases_free(test_database_case_t *cases, size_t count)
{
    for (size_t i = 0u; cases && i < count; ++i) {
        string_free(cases[i].name);
        string_free(cases[i].operation);
        string_free(cases[i].input);
        string_free(cases[i].expected);
    }
    free(cases);
}
