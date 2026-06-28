/* sqlite_core.c - SQLCipher-backed opaque SQLite storage for MARS objects */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC 1
#endif
#include <sqlcipher/sqlite3.h>

#include "sqlite.h"
#include "sqlite_internal.h"

struct _sqlite_t {
    sqlite3 *handle;
    string_t *error;
};

struct _sqlite_stmt_t {
    sqlite_t *db;
    sqlite3_stmt *handle;
    string_t *error;
};

static bool sqlite_text_is_empty(const string_t *text)
{
    return !text || string_byte_length(text) == 0u;
}

static void sqlite_set_error(sqlite_t *db, const char *message)
{
    if (!db)
        return;

    if (!db->error)
        db->error = string_new();
    if (!db->error)
        return;

    string_clear(db->error);
    (void)string_append_cstr(db->error, message ? message : "sqlite error");
}

static void sqlite_stmt_set_error(sqlite_stmt_t *stmt, const char *message)
{
    if (!stmt)
        return;

    if (!stmt->error)
        stmt->error = string_new();
    if (!stmt->error)
        return;

    string_clear(stmt->error);
    (void)string_append_cstr(stmt->error, message ? message : "sqlite statement error");
}

static bool sqlite_set_error_code(sqlite_t *db, int rc)
{
    const char *message = "sqlite error";

    if (db && db->handle)
        message = sqlite3_errmsg(db->handle);
    sqlite_set_error(db, message);
    return rc == SQLITE_OK || rc == SQLITE_ROW || rc == SQLITE_DONE;
}

static int sqlite_bind_text_value(sqlite3_stmt *stmt, int index, const string_t *value)
{
    return sqlite3_bind_text(stmt,
                             index,
                             string_c_str(value),
                             (int)string_byte_length(value),
                             SQLITE_TRANSIENT);
}

static string_t *sqlite_column_string(sqlite3_stmt *stmt, int column)
{
    const void *bytes = sqlite3_column_blob(stmt, column);
    int len = sqlite3_column_bytes(stmt, column);
    string_t *text = string_new();

    if (!text)
        return NULL;
    if (len > 0 && string_append_chars(text, bytes, (size_t)len) != 0) {
        string_free(text);
        return NULL;
    }
    return text;
}

static bool sqlite_prepare(sqlite_t *db, const char *sql, sqlite3_stmt **out_stmt)
{
    int rc;

    if (!db || !db->handle || !sql || !out_stmt)
        return false;

    *out_stmt = NULL;
    rc = sqlite3_prepare_v2(db->handle, sql, -1, out_stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(db, rc);
        return false;
    }
    return true;
}

static bool sqlite_step_done(sqlite_t *db, sqlite3_stmt *stmt)
{
    int rc = sqlite3_step(stmt);

    if (rc == SQLITE_DONE)
        return true;

    sqlite_set_error_code(db, rc);
    return false;
}

static bool sqlite_verify_key(sqlite_t *db)
{
    sqlite3_stmt *stmt = NULL;
    bool ok = false;

    if (!sqlite_prepare(db, "SELECT count(*) FROM sqlite_master", &stmt))
        return false;

    ok = sqlite3_step(stmt) == SQLITE_ROW;
    if (!ok)
        sqlite_set_error(db, "invalid SQLCipher key or unreadable database");

    sqlite3_finalize(stmt);
    return ok;
}

sqlite_t *sqlite_open_encrypted(const string_t *path, const string_t *key)
{
    sqlite_t *db;
    int rc;

    if (sqlite_text_is_empty(path) || sqlite_text_is_empty(key))
        return NULL;

    db = calloc(1u, sizeof(*db));
    if (!db)
        return NULL;

    db->error = string_new();
    if (!db->error) {
        free(db);
        return NULL;
    }

    rc = sqlite3_open_v2(string_c_str(path),
                         &db->handle,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                         NULL);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(db, rc);
        sqlite_close(db);
        return NULL;
    }

    rc = sqlite3_key(db->handle, string_c_str(key), (int)string_byte_length(key));
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(db, rc);
        sqlite_close(db);
        return NULL;
    }

    if (!sqlite_exec_cstr(db, "PRAGMA cipher_memory_security = ON") ||
        !sqlite_exec_cstr(db, "PRAGMA foreign_keys = ON") ||
        !sqlite_verify_key(db)) {
        sqlite_close(db);
        return NULL;
    }

    return db;
}

void sqlite_close(sqlite_t *db)
{
    if (!db)
        return;

    if (db->handle)
        sqlite3_close(db->handle);
    string_free(db->error);
    free(db);
}

const string_t *sqlite_last_error(const sqlite_t *db)
{
    return db ? db->error : NULL;
}

bool sqlite_exec(sqlite_t *db, const string_t *sql)
{
    if (!sql)
        return false;

    return sqlite_exec_cstr(db, string_c_str(sql));
}

bool sqlite_exec_cstr(sqlite_t *db, const char *sql)
{
    char *errmsg = NULL;
    int rc;

    if (!db || !db->handle || !sql)
        return false;

    rc = sqlite3_exec(db->handle, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite_set_error(db, errmsg ? errmsg : sqlite3_errmsg(db->handle));
        sqlite3_free(errmsg);
        return false;
    }

    sqlite3_free(errmsg);
    return true;
}

bool sqlite_init_object_store(sqlite_t *db)
{
    return sqlite_exec_cstr(db,
        "CREATE TABLE IF NOT EXISTS mars_object ("
        "name TEXT PRIMARY KEY NOT NULL,"
        "type TEXT NOT NULL,"
        "encoding TEXT NOT NULL,"
        "value BLOB NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ")");
}

bool sqlite_store_object(sqlite_t *db,
                         const string_t *name,
                         const string_t *type,
                         const string_t *encoding,
                         const void *data,
                         size_t data_len)
{
    sqlite3_stmt *stmt = NULL;
    bool ok = false;

    if (!db || !name || !type || !encoding || (!data && data_len > 0u)) {
        sqlite_set_error(db, "invalid object store argument");
        return false;
    }
    if (!sqlite_prepare(db,
            "INSERT INTO mars_object(name, type, encoding, value) "
            "VALUES(?1, ?2, ?3, ?4) "
            "ON CONFLICT(name) DO UPDATE SET "
            "type = excluded.type, "
            "encoding = excluded.encoding, "
            "value = excluded.value, "
            "updated_at = CURRENT_TIMESTAMP",
            &stmt)) {
        return false;
    }

    if (sqlite_bind_text_value(stmt, 1, name) != SQLITE_OK ||
        sqlite_bind_text_value(stmt, 2, type) != SQLITE_OK ||
        sqlite_bind_text_value(stmt, 3, encoding) != SQLITE_OK ||
        sqlite3_bind_blob(stmt, 4, data, (int)data_len, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite_set_error(db, sqlite3_errmsg(db->handle));
        goto done;
    }

    ok = sqlite_step_done(db, stmt);

done:
    sqlite3_finalize(stmt);
    return ok;
}

bool sqlite_load_object(sqlite_t *db,
                        const string_t *name,
                        string_t **out_type,
                        string_t **out_encoding,
                        void **out_data,
                        size_t *out_data_len)
{
    sqlite3_stmt *stmt = NULL;
    string_t *type = NULL;
    string_t *encoding = NULL;
    void *data = NULL;
    int data_len;
    int rc;
    bool ok = false;

    if (out_type)
        *out_type = NULL;
    if (out_encoding)
        *out_encoding = NULL;
    if (out_data)
        *out_data = NULL;
    if (out_data_len)
        *out_data_len = 0u;

    if (!db || !name || !out_data || !out_data_len) {
        sqlite_set_error(db, "invalid object load argument");
        return false;
    }
    if (!sqlite_prepare(db,
            "SELECT type, encoding, value FROM mars_object WHERE name = ?1",
            &stmt)) {
        return false;
    }
    if (sqlite_bind_text_value(stmt, 1, name) != SQLITE_OK) {
        sqlite_set_error(db, sqlite3_errmsg(db->handle));
        goto done;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite_set_error(db, "object not found");
        goto done;
    }
    if (rc != SQLITE_ROW) {
        sqlite_set_error_code(db, rc);
        goto done;
    }

    type = sqlite_column_string(stmt, 0);
    encoding = sqlite_column_string(stmt, 1);
    data_len = sqlite3_column_bytes(stmt, 2);
    if (!type || !encoding || data_len < 0)
        goto done;

    data = malloc((size_t)data_len + 1u);
    if (!data)
        goto done;
    if (data_len > 0)
        memcpy(data, sqlite3_column_blob(stmt, 2), (size_t)data_len);
    ((uint8_t *)data)[data_len] = 0u;

    if (out_type)
        *out_type = type;
    else
        string_free(type);
    if (out_encoding)
        *out_encoding = encoding;
    else
        string_free(encoding);
    *out_data = data;
    *out_data_len = (size_t)data_len;
    type = NULL;
    encoding = NULL;
    data = NULL;
    ok = true;

done:
    free(data);
    string_free(encoding);
    string_free(type);
    sqlite3_finalize(stmt);
    return ok;
}

void sqlite_free_object_data(void *data)
{
    free(data);
}

bool sqlite_store_string(sqlite_t *db,
                         const string_t *name,
                         const string_t *value)
{
    string_t *type = string_new_with("string_t");
    string_t *encoding = string_new_with("utf-8");
    bool ok = false;

    if (!value) {
        sqlite_set_error(db, "missing string value");
        goto done;
    }

    ok = type && encoding &&
        sqlite_store_object(db,
                            name,
                            type,
                            encoding,
                            string_c_str(value),
                            string_byte_length(value));

done:
    string_free(encoding);
    string_free(type);
    return ok;
}

bool sqlite_load_string(sqlite_t *db,
                        const string_t *name,
                        string_t **out_value)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    void *data = NULL;
    size_t data_len = 0u;
    string_t *value = NULL;
    bool ok = false;

    if (out_value)
        *out_value = NULL;
    if (!out_value)
        return false;

    if (!sqlite_load_object(db, name, &type, &encoding, &data, &data_len))
        goto done;

    if (strcmp(string_c_str(type), "string_t") != 0 ||
        strcmp(string_c_str(encoding), "utf-8") != 0) {
        sqlite_set_error(db, "stored object is not a utf-8 string_t");
        goto done;
    }

    value = string_new();
    if (!value)
        goto done;
    if (data_len > 0u && string_append_chars(value, data, data_len) != 0)
        goto done;

    *out_value = value;
    value = NULL;
    ok = true;

done:
    string_free(value);
    sqlite_free_object_data(data);
    string_free(encoding);
    string_free(type);
    return ok;
}

sqlite3 *sqlite_native_handle(sqlite_t *db)
{
    return db ? db->handle : NULL;
}

sqlite_stmt_t *sqlite_stmt_prepare(sqlite_t *db, const char *sql)
{
    sqlite_stmt_t *stmt;
    int rc;

    if (!db || !db->handle || !sql)
        return NULL;

    stmt = calloc(1u, sizeof(*stmt));
    if (!stmt)
        return NULL;
    stmt->db = db;
    stmt->error = string_new();
    if (!stmt->error) {
        free(stmt);
        return NULL;
    }

    rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt->handle, NULL);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(db, rc);
        sqlite_stmt_set_error(stmt, sqlite3_errmsg(db->handle));
        sqlite_stmt_finalize(stmt);
        return NULL;
    }
    return stmt;
}

void sqlite_stmt_finalize(sqlite_stmt_t *stmt)
{
    if (!stmt)
        return;
    if (stmt->handle)
        sqlite3_finalize(stmt->handle);
    string_free(stmt->error);
    free(stmt);
}

bool sqlite_stmt_bind_text(sqlite_stmt_t *stmt, int index, const char *value)
{
    int rc;

    if (!stmt || !stmt->handle || !value)
        return false;
    rc = sqlite3_bind_text(stmt->handle, index, value, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(stmt->db, rc);
        sqlite_stmt_set_error(stmt, sqlite3_errmsg(stmt->db->handle));
        return false;
    }
    return true;
}

bool sqlite_stmt_bind_int(sqlite_stmt_t *stmt, int index, int value)
{
    int rc;

    if (!stmt || !stmt->handle)
        return false;
    rc = sqlite3_bind_int(stmt->handle, index, value);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(stmt->db, rc);
        sqlite_stmt_set_error(stmt, sqlite3_errmsg(stmt->db->handle));
        return false;
    }
    return true;
}

bool sqlite_stmt_bind_double(sqlite_stmt_t *stmt, int index, double value)
{
    int rc;

    if (!stmt || !stmt->handle)
        return false;
    rc = sqlite3_bind_double(stmt->handle, index, value);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(stmt->db, rc);
        sqlite_stmt_set_error(stmt, sqlite3_errmsg(stmt->db->handle));
        return false;
    }
    return true;
}

bool sqlite_stmt_bind_blob(sqlite_stmt_t *stmt, int index, const void *value, size_t value_len)
{
    int rc;

    if (!stmt || !stmt->handle || (!value && value_len > 0u))
        return false;
    rc = sqlite3_bind_blob(stmt->handle, index, value, (int)value_len, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(stmt->db, rc);
        sqlite_stmt_set_error(stmt, sqlite3_errmsg(stmt->db->handle));
        return false;
    }
    return true;
}

bool sqlite_stmt_bind_null(sqlite_stmt_t *stmt, int index)
{
    int rc;

    if (!stmt || !stmt->handle)
        return false;
    rc = sqlite3_bind_null(stmt->handle, index);
    if (rc != SQLITE_OK) {
        sqlite_set_error_code(stmt->db, rc);
        sqlite_stmt_set_error(stmt, sqlite3_errmsg(stmt->db->handle));
        return false;
    }
    return true;
}

sqlite_step_result_t sqlite_stmt_step(sqlite_stmt_t *stmt)
{
    int rc;

    if (!stmt || !stmt->handle)
        return SQLITE_STEP_ERROR;
    rc = sqlite3_step(stmt->handle);
    if (rc == SQLITE_ROW)
        return SQLITE_STEP_ROW;
    if (rc == SQLITE_DONE)
        return SQLITE_STEP_DONE;
    sqlite_set_error_code(stmt->db, rc);
    sqlite_stmt_set_error(stmt, sqlite3_errmsg(stmt->db->handle));
    return SQLITE_STEP_ERROR;
}

void sqlite_stmt_reset(sqlite_stmt_t *stmt)
{
    if (!stmt || !stmt->handle)
        return;
    sqlite3_reset(stmt->handle);
}

void sqlite_stmt_clear_bindings(sqlite_stmt_t *stmt)
{
    if (!stmt || !stmt->handle)
        return;
    sqlite3_clear_bindings(stmt->handle);
}

const char *sqlite_stmt_column_text(sqlite_stmt_t *stmt, int column)
{
    const unsigned char *text;

    if (!stmt || !stmt->handle)
        return NULL;
    text = sqlite3_column_text(stmt->handle, column);
    return text ? (const char *)text : NULL;
}

int sqlite_stmt_column_int(sqlite_stmt_t *stmt, int column)
{
    if (!stmt || !stmt->handle)
        return 0;
    return sqlite3_column_int(stmt->handle, column);
}

double sqlite_stmt_column_double(sqlite_stmt_t *stmt, int column)
{
    if (!stmt || !stmt->handle)
        return 0.0;
    return sqlite3_column_double(stmt->handle, column);
}

const void *sqlite_stmt_column_blob(sqlite_stmt_t *stmt, int column)
{
    if (!stmt || !stmt->handle)
        return NULL;
    return sqlite3_column_blob(stmt->handle, column);
}

size_t sqlite_stmt_column_bytes(sqlite_stmt_t *stmt, int column)
{
    if (!stmt || !stmt->handle)
        return 0u;
    return (size_t)sqlite3_column_bytes(stmt->handle, column);
}

bool sqlite_stmt_column_is_null(sqlite_stmt_t *stmt, int column)
{
    if (!stmt || !stmt->handle)
        return true;
    return sqlite3_column_type(stmt->handle, column) == SQLITE_NULL;
}

const string_t *sqlite_stmt_last_error(const sqlite_stmt_t *stmt)
{
    return stmt ? stmt->error : NULL;
}
