/**
 * @file lab_history.c
 * @brief Transactional, size-bounded history records for independent browser profiles.
 *
 * Stores one versioned Protobuf bundle per browser and worksheet mode. Updates use
 * revision and incarnation comparisons inside BEGIN IMMEDIATE, so prefork workers
 * and old tabs cannot overwrite a newer record, including after eviction/recreation.
 * Bind parameters never become SQL text. Limits cover payloads, rows, main-file
 * pages and rollback-journal growth; database failures preserve earlier commits.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "array-t.h"
#include "file-t.h"
#include "sqlite-t.h"
#include "lab_state.h"
#include "lab_wire.h"
#include "lab_history.h"

enum { HISTORY_SNAPSHOT = 262144, HISTORY_MODE = 1048576, HISTORY_TOTAL = 16777216, HISTORY_ROWS = 448 };
static const char *const history_modes[] = {"expression", "equation", "diffequation", "matrix",
                                           "integrator", "datetime", "almanac"};

static const json_t *history_get(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static const char *history_text(const json_t *object, const char *name)
{
    const string_t *text = json_string_value(history_get(object, name));
    return text ? string_c_str(text) : "";
}

static bool history_put(json_t *object, const char *name, json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = object && key && value && json_object_set(object, key, value);
    string_free(key);
    json_free(value);
    return ok;
}

static json_t *history_string(const char *text)
{
    string_t *value = string_new_with(text);
    json_t *result = value ? json_new_string(value) : NULL;
    string_free(value);
    return result;
}

static json_t *history_number(int64_t value)
{
    string_t *text = string_sprintf("%lld", (long long)value);
    json_t *result = text ? json_new_number(text) : NULL;
    string_free(text);
    return result;
}

static int64_t history_integer(const json_t *object, const char *key)
{
    number_t number;
    if (!json_number_value(history_get(object, key), &number)) return -1;
    double value = num_to_double(number);
    num_destroy(&number);
    return isfinite(value) && value >= 0 && value <= 9007199254740991.0 && floor(value) == value ? (int64_t)value : -1;
}

static bool history_id(const json_t *object, const char *key)
{
    const string_t *value = json_string_value(history_get(object, key));
    if (!value || string_byte_length(value) != 32) return false;
    const char *text = string_c_str(value);
    for (unsigned i = 0; i < 32; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return true;
}

static bool history_snapshot(const json_t *binary, unsigned mode)
{
    const string_t *bytes = json_string_value(history_get(binary, "$lab_binary"));
    if (!bytes || !string_byte_length(bytes) || string_byte_length(bytes) > HISTORY_SNAPSHOT) return false;
    json_t *envelope = lab_wire_decode(string_c_str(bytes), string_byte_length(bytes));
    const json_t *state = history_get(envelope, "state");
    const string_t *name = json_string_value(history_get(state, "mode"));
    bool ok = json_type(state) == JSON_OBJECT && name && string_view_equals_literal(string_view_all(name), history_modes[mode]) &&
              json_type(history_get(state, "text")) == JSON_STRING;
    json_free(envelope);
    return ok;
}

static bool history_valid(const json_t *history, unsigned mode)
{
    if (history_integer(history, "version") != 1 || !history_snapshot(history_get(history, "current"), mode)) return false;
    const char *const sides[] = {"back", "forward"};
    for (unsigned side = 0; side < 2; ++side) {
        const json_t *entries = history_get(history, sides[side]);
        if (json_type(entries) != JSON_ARRAY || json_array_size(entries) > 64) return false;
        for (size_t i = 0; i < json_array_size(entries); ++i)
            if (!history_snapshot(json_array_get(entries, i), mode)) return false;
    }
    return true;
}

static sqlite_t *history_open(void)
{
    const char *configured = getenv("MARS_LAB_HISTORY_FILE");
    string_t *path = configured && *configured ? string_new_with(configured) : lab_state_private_path("mars_lab_history.db");
    const char *slash = path ? strrchr(string_c_str(path), '/') : NULL;
    string_t *parent = slash ? string_sprintf("%.*s", (int)(slash == string_c_str(path) ? 1 :
                                           slash - string_c_str(path)), string_c_str(path)) : string_new_with(".");
    file_t *directory = parent ? file_new(parent) : NULL;
    file_t *file = path ? file_new(path) : NULL;
    mode_t mask = umask(0077);
    bool private_file = file && directory && file_create_directory(directory, 0700, true) &&
                        file_open(file, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_READ_WRITE) &&
                        file_close(file) && file_chmod(file, 0600);
    sqlite_t *db = private_file ? sqlite_open_plain(path) : NULL;
    bool ok = db && sqlite_exec_cstr(db,
        "PRAGMA busy_timeout=3000; PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL;"
        "PRAGMA page_size=4096; PRAGMA max_page_count=8192;"
        "CREATE TABLE IF NOT EXISTS history_v1("
        "client TEXT NOT NULL, mode INTEGER NOT NULL, epoch TEXT NOT NULL, session TEXT NOT NULL,"
        "revision INTEGER NOT NULL, touched INTEGER NOT NULL, payload BLOB NOT NULL,"
        "PRIMARY KEY(client,mode));"
        "CREATE INDEX IF NOT EXISTS history_v1_age ON history_v1(touched,client,mode);");
    const char *const checks[] = {"PRAGMA page_size", "PRAGMA max_page_count"};
    const int expected[] = {4096, 8192};
    for (unsigned i = 0; ok && i < 2; ++i) {
        sqlite_stmt_t *query = sqlite_stmt_prepare(db, checks[i]);
        ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW && sqlite_stmt_column_int(query, 0) == expected[i];
        sqlite_stmt_finalize(query);
    }
    umask(mask);
    file_free(directory);
    string_free(parent);
    file_free(file);
    string_free(path);
    if (!ok) { sqlite_close(db); db = NULL; }
    return db;
}

static json_t *history_load(sqlite_t *db, const char *client)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db,
        "SELECT mode,epoch,revision,payload FROM history_v1 WHERE client=?1 ORDER BY touched,mode");
    json_t *rows = json_new_array();
    bool ok = rows && stmt && sqlite_stmt_bind_text(stmt, 1, client);
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        json_t *row = json_new_object();
        json_t *history = lab_wire_decode(sqlite_stmt_column_blob(stmt, 3), sqlite_stmt_column_bytes(stmt, 3));
        ok = history && history_put(row, "mode", history_number(sqlite_stmt_column_int(stmt, 0))) &&
             history_put(row, "epoch", history_string(sqlite_stmt_column_text(stmt, 1))) &&
             history_put(row, "revision", history_number(sqlite_stmt_column_int64(stmt, 2)));
        ok = history_put(row, "history", history) && ok;
        ok = ok && json_array_append(rows, row);
        json_free(row);
    }
    sqlite_stmt_finalize(stmt);
    if (!ok || step != SQLITE_STEP_DONE) { json_free(rows); return NULL; }
    return rows;
}

static bool history_prune(sqlite_t *db, const char *client, int mode)
{
    /* At most 449 records exist inside this transaction, so the aggregate scan is strictly bounded. */
    for (unsigned attempt = 0; attempt <= HISTORY_ROWS; ++attempt) {
        sqlite_stmt_t *count = sqlite_stmt_prepare(db, "SELECT count(*),coalesce(sum(length(payload)),0) FROM history_v1");
        bool ok = count && sqlite_stmt_step(count) == SQLITE_STEP_ROW;
        bool fits = ok && sqlite_stmt_column_int64(count, 0) <= HISTORY_ROWS &&
                    sqlite_stmt_column_int64(count, 1) <= HISTORY_TOTAL;
        sqlite_stmt_finalize(count);
        if (!ok || fits) return ok;
        sqlite_stmt_t *remove = sqlite_stmt_prepare(db,
            "DELETE FROM history_v1 WHERE rowid=(SELECT rowid FROM history_v1 "
            "WHERE NOT(client=?1 AND mode=?2) ORDER BY touched,client,mode LIMIT 1)");
        ok = remove && sqlite_stmt_bind_text(remove, 1, client) && sqlite_stmt_bind_int(remove, 2, mode) &&
             sqlite_stmt_step(remove) == SQLITE_STEP_DONE;
        sqlite_stmt_finalize(remove);
        if (!ok) return false;
    }
    return false;
}

/* Persist only validated records and publish a response after the transaction commits. */
json_t *lab_history_request(const json_t *request, unsigned *status)
{
    *status = 400;
    json_t *response = json_new_object();
    if (!history_id(request, "client") || !history_id(request, "session")) goto done;
    const char *action = history_text(request, "action"), *client = history_text(request, "client");
    bool save = !strcmp(action, "save");
    if (!save && strcmp(action, "load")) goto done;
    int64_t mode = history_integer(request, "mode"), revision = history_integer(request, "revision");
    const json_t *history = history_get(request, "history");
    array_t *bytes = NULL;
    if (save) {
        if (mode < 0 || mode >= 7 || revision < 0 || revision >= 9007199254740991LL ||
            (revision && !history_id(request, "epoch")) || !history_valid(history, (unsigned)mode)) goto done;
        bytes = lab_wire_encode(history);
        if (!bytes || array_size(bytes) > HISTORY_MODE) { array_destroy(bytes); *status = 413; goto done; }
    }
    sqlite_t *db = history_open();
    *status = 503;
    if (!db) { array_destroy(bytes); goto done; }
    if (!save) {
        json_t *rows = history_load(db, client);
        if (rows && history_put(response, "records", rows)) *status = 200;
    } else if (sqlite_exec_cstr(db, "BEGIN IMMEDIATE")) {
        sqlite_stmt_t *find = sqlite_stmt_prepare(db, "SELECT epoch,revision FROM history_v1 WHERE client=?1 AND mode=?2");
        bool ok = find && sqlite_stmt_bind_text(find, 1, client) && sqlite_stmt_bind_int(find, 2, (int)mode);
        sqlite_step_result_t step = ok ? sqlite_stmt_step(find) : SQLITE_STEP_ERROR;
        bool matches = step == SQLITE_STEP_ROW ?
            revision == sqlite_stmt_column_int64(find, 1) &&
            !strcmp(history_text(request, "epoch"), sqlite_stmt_column_text(find, 0)) :
            step == SQLITE_STEP_DONE && revision == 0;
        sqlite_stmt_finalize(find);
        const char *epoch = revision ? history_text(request, "epoch") : history_text(request, "session");
        if (!matches) *status = step == SQLITE_STEP_ERROR ? 503 : 409;
        sqlite_stmt_t *write = matches ? sqlite_stmt_prepare(db,
            "INSERT OR REPLACE INTO history_v1(client,mode,epoch,session,revision,touched,payload) "
            "VALUES(?1,?2,?3,?4,?5,(SELECT coalesce(max(touched),0)+1 FROM history_v1),?6)") : NULL;
        ok = write && sqlite_stmt_bind_text(write, 1, client) && sqlite_stmt_bind_int(write, 2, (int)mode) &&
             sqlite_stmt_bind_text(write, 3, epoch) && sqlite_stmt_bind_text(write, 4, history_text(request, "session")) &&
             sqlite_stmt_bind_int64(write, 5, revision + 1) &&
             sqlite_stmt_bind_blob(write, 6, array_get(bytes, 0), array_size(bytes)) &&
             sqlite_stmt_step(write) == SQLITE_STEP_DONE;
        sqlite_stmt_finalize(write);
        ok = ok && history_prune(db, client, (int)mode) && sqlite_exec_cstr(db, "COMMIT");
        if (ok) {
            *status = 200;
            history_put(response, "epoch", history_string(epoch));
            history_put(response, "revision", history_number(revision + 1));
        } else sqlite_exec_cstr(db, "ROLLBACK");
    }
    sqlite_close(db);
    array_destroy(bytes);
done:
    history_put(response, "ok", json_new_bool(*status == 200));
    return response;
}
