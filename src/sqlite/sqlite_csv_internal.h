/**
 * @file sqlite_csv_internal.h
 * @brief Private CSV import schema, conversion and transaction support.
 *
 * Shared only by SQLite's CSV implementation units. Public consumers use
 * sqlite-t.h; CSV syntax and stream ownership remain behind csv-t.h.
 */
#ifndef MARS_SQLITE_CSV_INTERNAL_H
#define MARS_SQLITE_CSV_INTERNAL_H

#include <locale.h>

#include "dictionary-t.h"
#include "sqlite_internal.h"

/** @brief Owned column plan built before insertion. */
typedef struct {
    string_t *name;
    sqlite_csv_type_t type;
    bool automatic, wide_integer;
} sqlite_csv_plan_t;

/** @brief Per-call state; neither the database nor source/writer is owned here. */
typedef struct {
    sqlite_t *db;
    sqlite3 *native;
    sqlite_csv_options_t options;
    sqlite_csv_report_t *report;
    sqlite_csv_plan_t *columns;
    size_t count;
    const string_t *table_name;
    string_t *table;
    locale_t locale;
    dictionary_t *names;
    string_t **keys;
} sqlite_csv_context_t;

/** @brief Set first diagnostic. @param c Context. @param code errno code. @param message Text. @return False. */
bool sqlite_csv_fail(sqlite_csv_context_t *c, int code, const char *message);

/** @brief Capture a native SQLite failure. @param c Context. @return False. */
bool sqlite_csv_sql_error(sqlite_csv_context_t *c);

/** @brief Capture a CSV failure. @param c Context. @param csv Stream. @return False. */
bool sqlite_csv_stream_error(sqlite_csv_context_t *c, csv_t *csv);

/** @brief Quote one identifier. @param text Borrowed name. @return Owned SQL token or NULL. */
string_t *sqlite_csv_identifier(const string_t *text);

/** @brief Build an ASCII-folded SQLite identifier key. @param name Text. @return Owned key or NULL. */
string_t *sqlite_csv_key(const string_t *name);

/** @brief Read one numeric token. @param c Context. @param text Text. @param integer Integer result.
 * @param real Floating result. @param significant Significant mantissa digits. @param leading Leading-zero flag.
 * @return INTEGER or REAL on numeric success, otherwise TEXT. */
sqlite_csv_type_t sqlite_csv_number(sqlite_csv_context_t *c, const string_t *text, int64_t *integer,
                                   double *real, size_t *significant, bool *leading);

/** @brief Validate or infer a row. @param c Context. @param csv Reader. @param infer Update AUTO columns.
 * @param stmt Optional prepared insert to bind. @return Success. */
bool sqlite_csv_row(sqlite_csv_context_t *c, csv_t *csv, bool infer, sqlite3_stmt *stmt);

/** @brief Initialise column names/types. @param c Context. @param csv Reader after first next call.
 * @param stream Require explicit types. @return Success. */
bool sqlite_csv_columns(sqlite_csv_context_t *c, csv_t *csv, bool stream);

/** @brief Create or validate destination table and prepare insertion. @param c Context.
 * @param statement Receives owned native statement. @return Success. */
bool sqlite_csv_schema(sqlite_csv_context_t *c, sqlite3_stmt **statement);

/** @brief Free per-call allocations. @param c Context, safe when partly initialised. */
void sqlite_csv_clear(sqlite_csv_context_t *c);

#endif
