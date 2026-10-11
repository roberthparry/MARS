/**
 * @file sqlite_t.h
 * @brief Opaque SQLCipher-backed storage for MARS objects.
 *
 * Use this module for encrypted persistent data with SQLite query semantics.
 * Opaque database and statement handles support SQL execution, parameter binding,
 * typed result access and transactions. Object-storage helpers persist supported
 * MARS values without exposing database implementation details.
 * CSV ingestion adds conservative whole-file type inference, explicit streaming
 * schemas and savepoint-protected insertion. Query export delegates escaping
 * to csv_t; CSV syntax remains the responsibility of the CSV module.
 *
 * File-content import/export is available through file_t.h; live handles and open
 * descriptors are not persistent objects. Applications are responsible for key
 * management, transaction policy and handling query or storage failures. Encryption
 * at rest does not replace application-level access control.
 *
 * The public type is intentionally named sqlite_t because callers should think
 * in terms of SQLite semantics. The implementation uses SQLCipher and refuses
 * to open a database without a non-empty key, so files created through this API
 * are encrypted at rest.
 */

#ifndef MARS_SQLITE_H
#define MARS_SQLITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "string_t.h"
#include "csv_t.h"

/**
 * @brief Opaque encrypted SQLite database handle.
 */
typedef struct _sqlite_t sqlite_t;

/**
 * @brief Returns the borrowed main database filename.
 * @param db Database handle.
 * @return Filename valid until close, an empty string for an in-memory database, or NULL for an invalid handle.
 */
const char *sqlite_database_path(const sqlite_t *db);

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
bool sqlite_store_object(sqlite_t *db, const string_t *name, const string_t *type, const string_t *encoding,
                         const void *data, size_t data_len);

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
bool sqlite_load_object(sqlite_t *db, const string_t *name, string_t **out_type, string_t **out_encoding,
                        void **out_data, size_t *out_data_len);

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
bool sqlite_store_string(sqlite_t *db, const string_t *name, const string_t *value);

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
bool sqlite_load_string(sqlite_t *db, const string_t *name, string_t **out_value);

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
 * @brief Binds a signed 64-bit integer without narrowing.
 * @param stmt Prepared statement handle.
 * @param index 1-based parameter index.
 * @param value Integer value.
 * @return True on success.
 */
bool sqlite_stmt_bind_int64(sqlite_stmt_t *stmt, int index, int64_t value);

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
 * @brief Reads a signed 64-bit column value without narrowing.
 * @param stmt Prepared statement handle.
 * @param column Zero-based column index.
 * @return Integer value, or zero for an invalid statement.
 */
int64_t sqlite_stmt_column_int64(sqlite_stmt_t *stmt, int column);

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

/** @brief Import column policy; AUTO is available only for replayable file imports. */
typedef enum {
    SQLITE_CSV_AUTO,
    SQLITE_CSV_INTEGER,
    SQLITE_CSV_REAL,
    SQLITE_CSV_TEXT
} sqlite_csv_type_t;

/** @brief Positional column override; names and values are borrowed during import. */
typedef struct {
    const string_t *name; /**< Replacement destination name, or NULL to use the CSV header. */
    sqlite_csv_type_t type; /**< Required storage policy, or AUTO for file inference. */
} sqlite_csv_column_t;

/** @brief Import policy; initialise with sqlite_csv_options_default(). */
typedef struct {
    csv_options_t csv; /**< File-reader policy; default consumes headers and requires uniform row widths. */
    const sqlite_csv_column_t *columns; /**< Optional positional names/types; exactly one per input column. */
    size_t column_count; /**< Number of overrides, or zero when columns is NULL. */
    bool create_table; /**< Create a new main-schema table; true by default. False appends to an existing table. */
    bool empty_is_null; /**< Interpret empty fields as NULL; true by default. Otherwise retain empty text. */
    bool infer_real; /**< Permit approximate REAL inference for at most 15 significant digits; false by default. */
} sqlite_csv_options_t;

/** @brief Import/export result; no caller-owned allocations. */
typedef struct {
    size_t rows; /**< Successful data rows; zero on failed import, completed rows on failed export. */
    size_t record; /**< One-based CSV record at failure, including a consumed header; zero before reading. */
    size_t column; /**< One-based column at failure, or zero for a row/database-level failure. */
    int code; /**< errno-style failure code, or zero on success. */
    int sqlite_code; /**< Underlying SQLite failure code, or zero for non-SQL failures. */
    csv_error_t csv_error; /**< Underlying CSV failure details, otherwise zero. */
} sqlite_csv_report_t;

/**
 * @brief Supply another chunk when a streamed import needs input.
 * @param reader Borrowed CSV reader; callback must call csv_feed(), marking its final chunk.
 * @param context Borrowed caller context.
 * @return True after feeding a non-empty chunk or final EOF; false cancels and rolls back the import.
 */
typedef bool (*sqlite_csv_refill_fn)(csv_t *reader, void *context);

/** @brief Return conservative CSV import defaults. @return Policy with headers, NULL empties and new-table creation. */
sqlite_csv_options_t sqlite_csv_options_default(void);

/**
 * @brief Infer a replayable CSV file's columns, then atomically import its rows.
 *
 * Reads from the current position of an open seekable file twice. The caller must keep its contents unchanged.
 * Headerless input requires explicit names. All columns must have the same width. Names must be non-empty and unique
 * under SQLite's ASCII case-insensitive matching; replacement names permit otherwise ambiguous input headers.
 * Integers outside signed 64-bit range, leading-zero identifiers and mixed columns infer TEXT. Decimal text stays
 * TEXT unless infer_real is enabled; explicit REAL overrides authorise rounding. Empty-only columns infer TEXT.
 * Existing tables are appended to by column name, without replacing rows; INTEGER, REAL and TEXT affinities are
 * supported. Incompatible inferred/declared types fail rather than relying on SQLite's permissive coercions.
 * A savepoint protects table creation and all inserts, including inside a caller transaction. Schema triggers still
 * have their normal effects; in particular a trigger that rolls back the outer transaction cannot be contained.
 *
 * @param db Borrowed open database, exclusively used for the duration of the call.
 * @param input Borrowed open readable seekable file; never closed. At EOF on success; restored on failure if possible.
 * @param table Borrowed unqualified main-schema table name, treated as one quoted identifier, never SQL.
 * @param options Borrowed policy, or NULL for defaults; copied for the duration of the call.
 * @param report Optional output report, reset before work. Failure details also appear in sqlite_last_error(db).
 * @return True on success; false on malformed input, incompatible schema, I/O or database failure.
 */
bool sqlite_import_csv(sqlite_t *db, file_t *input, const string_t *table,
                       const sqlite_csv_options_t *options, sqlite_csv_report_t *report);

/**
 * @brief Import a CSV reader once using explicit column types and a savepoint.
 * @param db Borrowed database; exclusive use is required during the call and refill callbacks.
 * @param reader Borrowed fresh reader; not freed. Configure headers before calling; options.csv is ignored.
 * @param table Borrowed unqualified destination table name.
 * @param options Required policy with columns and non-AUTO types; names may come from the reader's header.
 * @param refill Optional callback after CSV_NEED_INPUT; NULL rejects incomplete input. Must make progress.
 * @param context Borrowed callback context, passed unchanged; must not use the database or re-enter the importer.
 * @param report Optional reset output report; import failures report zero successful rows.
 * @return True on success; false rolls back the import. Reader position is not restored.
 */
bool sqlite_import_csv_reader(sqlite_t *db, csv_t *reader, const string_t *table,
                              const sqlite_csv_options_t *options, sqlite_csv_refill_fn refill, void *context,
                              sqlite_csv_report_t *report);

/**
 * @brief Stream one read-only SQL result through a CSV writer and finish it.
 * @param db Borrowed database; exclusive use is required during export.
 * @param query Borrowed single read-only SELECT or WITH query with no parameters; never user data. The first keyword
 * must be SELECT or WITH, ignoring leading whitespace; leading SQL comments and PRAGMAs are rejected before prepare.
 * @param writer Borrowed fresh CSV writer; never freed. File ownership remains with its caller.
 * @param header True writes result column names first, even for an empty result.
 * @param report Optional reset output report; rows counts completed records even on failure.
 * @return True after csv_finish(); false for invalid SQL, BLOB/NUL/non-UTF-8 data or output failure. Output may be
 * partial. NULL exports as an empty field and is indistinguishable from empty text; REAL uses 17 significant digits.
 */
bool sqlite_export_csv(sqlite_t *db, const string_t *query, csv_t *writer, bool header, sqlite_csv_report_t *report);

#endif /* MARS_SQLITE_H */
