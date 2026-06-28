#ifndef MARS_SQLITE_H
#define MARS_SQLITE_H

/**
 * @file sqlite.h
 * @brief Opaque SQLCipher-backed storage for MARS objects.
 *
 * The public type is intentionally named sqlite_t because callers should think
 * in terms of SQLite semantics. The implementation uses SQLCipher and refuses
 * to open a database without a non-empty key, so files created through this API
 * are encrypted at rest.
 */

#include <stdbool.h>
#include <stddef.h>

#include "ustring.h"

/**
 * @brief Opaque encrypted SQLite database handle.
 */
typedef struct _sqlite_t sqlite_t;

/**
 * @brief Opaque prepared statement handle.
 */
typedef struct _sqlite_stmt_t sqlite_stmt_t;

/**
 * @brief Result of stepping a prepared statement.
 */
typedef enum _sqlite_step_result_t {
    /** Stepping failed; inspect statement or database error text. */
    SQLITE_STEP_ERROR = -1,
    /** Stepping completed with no more rows. */
    SQLITE_STEP_DONE = 0,
    /** A row is available for column access. */
    SQLITE_STEP_ROW = 1
} sqlite_step_result_t;

/**
 * @brief Opens an encrypted SQLCipher database.
 *
 * @param path Filesystem path to the database file.
 * @param key Non-empty SQLCipher key used to open the database.
 * @return An open database handle, or `NULL` on failure.
 */
sqlite_t *sqlite_open_encrypted(const string_t *path, const string_t *key);

/**
 * @brief Closes an open database handle.
 *
 * Safe to call with `NULL`.
 *
 * @param db Database handle to close.
 */
void sqlite_close(sqlite_t *db);

/**
 * @brief Returns the most recent database-level error message.
 *
 * The returned string is owned by the database handle and remains valid until
 * the next database operation that updates the error state or the handle is
 * closed.
 *
 * @param db Database handle.
 * @return Last error text, or `NULL` if unavailable.
 */
const string_t *sqlite_last_error(const sqlite_t *db);

/**
 * @brief Executes SQL supplied as a `string_t`.
 *
 * This is intended for statements that do not need parameter binding.
 *
 * @param db Database handle.
 * @param sql SQL text to execute.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_exec(sqlite_t *db, const string_t *sql);

/**
 * @brief Executes SQL supplied as a C string.
 *
 * @param db Database handle.
 * @param sql SQL text to execute.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_exec_cstr(sqlite_t *db, const char *sql);

/**
 * @brief Creates the default key-value object store tables if needed.
 *
 * @param db Database handle.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_init_object_store(sqlite_t *db);

/**
 * @brief Stores a typed binary payload in the default object store.
 *
 * Existing objects with the same name are replaced.
 *
 * @param db Database handle.
 * @param name Object key.
 * @param type Application-defined object type label.
 * @param encoding Application-defined encoding label.
 * @param data Raw payload bytes.
 * @param data_len Number of payload bytes.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_store_object(sqlite_t *db,
                         const string_t *name,
                         const string_t *type,
                         const string_t *encoding,
                         const void *data,
                         size_t data_len);

/**
 * @brief Loads a typed binary payload from the default object store.
 *
 * Output values are allocated by the callee. The caller owns any non-`NULL`
 * outputs and must release them with `string_free()` or
 * `sqlite_free_object_data()`
 * as appropriate.
 *
 * @param db Database handle.
 * @param name Object key.
 * @param out_type Receives the stored type label.
 * @param out_encoding Receives the stored encoding label.
 * @param out_data Receives the payload bytes.
 * @param out_data_len Receives the payload length in bytes.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_load_object(sqlite_t *db,
                        const string_t *name,
                        string_t **out_type,
                        string_t **out_encoding,
                        void **out_data,
                        size_t *out_data_len);

/**
 * @brief Frees object payload memory returned by this API.
 *
 * Safe to call with `NULL`.
 *
 * @param data Payload pointer returned by `sqlite_load_object()`.
 */
void sqlite_free_object_data(void *data);

/**
 * @brief Stores a UTF-8 string value in the default object store.
 *
 * @param db Database handle.
 * @param name Object key.
 * @param value String value to store.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_store_string(sqlite_t *db,
                         const string_t *name,
                         const string_t *value);

/**
 * @brief Loads a UTF-8 string value from the default object store.
 *
 * The caller owns the returned string and must release it with
 * `string_free()`.
 *
 * @param db Database handle.
 * @param name Object key.
 * @param out_value Receives the loaded string.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_load_string(sqlite_t *db,
                        const string_t *name,
                        string_t **out_value);

/**
 * @brief Prepares a parameterised SQL statement.
 *
 * @param db Database handle.
 * @param sql SQL text containing SQLite parameters such as `?1`.
 * @return Prepared statement handle, or `NULL` on failure.
 */
sqlite_stmt_t *sqlite_stmt_prepare(sqlite_t *db, const char *sql);

/**
 * @brief Finalises a prepared statement and releases its resources.
 *
 * Safe to call with `NULL`.
 *
 * @param stmt Prepared statement handle.
 */
void sqlite_stmt_finalize(sqlite_stmt_t *stmt);

/**
 * @brief Binds UTF-8 text to a statement parameter.
 *
 * Parameter indexes are 1-based, following SQLite convention.
 *
 * @param stmt Prepared statement handle.
 * @param index 1-based parameter index.
 * @param value Text value to bind.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_stmt_bind_text(sqlite_stmt_t *stmt, int index, const char *value);

/**
 * @brief Binds an integer to a statement parameter.
 *
 * @param stmt Prepared statement handle.
 * @param index 1-based parameter index.
 * @param value Integer value to bind.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_stmt_bind_int(sqlite_stmt_t *stmt, int index, int value);

/**
 * @brief Binds a double to a statement parameter.
 *
 * @param stmt Prepared statement handle.
 * @param index 1-based parameter index.
 * @param value Floating-point value to bind.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_stmt_bind_double(sqlite_stmt_t *stmt, int index, double value);

/**
 * @brief Binds a binary blob to a statement parameter.
 *
 * @param stmt Prepared statement handle.
 * @param index 1-based parameter index.
 * @param value Blob payload bytes.
 * @param value_len Blob payload size in bytes.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_stmt_bind_blob(sqlite_stmt_t *stmt, int index, const void *value, size_t value_len);

/**
 * @brief Binds SQL `NULL` to a statement parameter.
 *
 * @param stmt Prepared statement handle.
 * @param index 1-based parameter index.
 * @return `true` on success, otherwise `false`.
 */
bool sqlite_stmt_bind_null(sqlite_stmt_t *stmt, int index);

/**
 * @brief Advances a prepared statement.
 *
 * @param stmt Prepared statement handle.
 * @return `SQLITE_STEP_ROW`, `SQLITE_STEP_DONE`, or `SQLITE_STEP_ERROR`.
 */
sqlite_step_result_t sqlite_stmt_step(sqlite_stmt_t *stmt);

/**
 * @brief Resets a prepared statement so it can be executed again.
 *
 * Safe to call with `NULL`.
 *
 * @param stmt Prepared statement handle.
 */
void sqlite_stmt_reset(sqlite_stmt_t *stmt);

/**
 * @brief Clears all parameter bindings on a prepared statement.
 *
 * Safe to call with `NULL`.
 *
 * @param stmt Prepared statement handle.
 */
void sqlite_stmt_clear_bindings(sqlite_stmt_t *stmt);

/**
 * @brief Returns the current row's text value for a column.
 *
 * The returned pointer is owned by SQLite and is only valid until the
 * statement is stepped, reset, or finalised.
 *
 * @param stmt Prepared statement handle.
 * @param column Zero-based column index.
 * @return Column text, or `NULL`.
 */
const char *sqlite_stmt_column_text(sqlite_stmt_t *stmt, int column);

/**
 * @brief Returns the current row's integer value for a column.
 *
 * @param stmt Prepared statement handle.
 * @param column Zero-based column index.
 * @return Integer value, or `0` if unavailable.
 */
int sqlite_stmt_column_int(sqlite_stmt_t *stmt, int column);

/**
 * @brief Returns the current row's floating-point value for a column.
 *
 * @param stmt Prepared statement handle.
 * @param column Zero-based column index.
 * @return Floating-point value, or `0.0` if unavailable.
 */
double sqlite_stmt_column_double(sqlite_stmt_t *stmt, int column);

/**
 * @brief Returns the current row's blob value for a column.
 *
 * The returned pointer is owned by SQLite and is only valid until the
 * statement is stepped, reset, or finalised.
 *
 * @param stmt Prepared statement handle.
 * @param column Zero-based column index.
 * @return Blob pointer, or `NULL`.
 */
const void *sqlite_stmt_column_blob(sqlite_stmt_t *stmt, int column);

/**
 * @brief Returns the current row blob size for a column in bytes.
 *
 * @param stmt Prepared statement handle.
 * @param column Zero-based column index.
 * @return Blob size in bytes, or `0` if unavailable.
 */
size_t sqlite_stmt_column_bytes(sqlite_stmt_t *stmt, int column);

/**
 * @brief Reports whether the current row column is SQL `NULL`.
 *
 * @param stmt Prepared statement handle.
 * @param column Zero-based column index.
 * @return `true` if the column is `NULL`, otherwise `false`.
 */
bool sqlite_stmt_column_is_null(sqlite_stmt_t *stmt, int column);

/**
 * @brief Returns the most recent statement-level error message.
 *
 * The returned string is owned by the statement and remains valid until the
 * next statement operation that updates the error state or the statement is
 * finalised.
 *
 * @param stmt Prepared statement handle.
 * @return Last error text, or `NULL` if unavailable.
 */
const string_t *sqlite_stmt_last_error(const sqlite_stmt_t *stmt);

#endif /* MARS_SQLITE_H */
