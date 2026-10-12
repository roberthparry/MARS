/**
 * @file sqlite_csv.c
 * @brief Two-pass file ingestion and one-pass typed CSV streaming into SQLite.
 *
 * Owns import orchestration and a nested savepoint, not input files or readers.
 * The first file pass determines every column from all records. The second
 * validates against that plan and inserts bound values using one statement.
 */

#include <errno.h>
#include <stdint.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"

#define MARS_SQLITE_INTERNAL_ACCESS
#include "sqlite_csv_internal.h"

/* Prefer exact spelling unless approximate decimal inference is explicitly enabled. */
sqlite_csv_options_t sqlite_csv_options_default(void)
{
    sqlite_csv_options_t options = {.csv = csv_options_default(), .create_table = true, .empty_is_null = true};
    options.csv.first_row_header = true;
    options.csv.strict_columns = true;
    return options;
}

static bool sqlite_csv_start(sqlite_csv_context_t *c, sqlite_t *db, const string_t *table,
                             const sqlite_csv_options_t *options, sqlite_csv_report_t *report)
{
    *report = (sqlite_csv_report_t){0};
    *c = (sqlite_csv_context_t){.db = db, .native = sqlite_native_handle(db), .report = report, .table_name = table,
                                .options = options ? *options : sqlite_csv_options_default()};
    if (!c->native)
        return sqlite_csv_fail(c, EINVAL, "CSV import requires an open database");
    c->table = sqlite_csv_identifier(table);
    c->locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (!c->table || !c->locale)
        return sqlite_csv_fail(c, EINVAL, "Invalid CSV table identifier or numeric locale");
    return computation_step_current("sqlite.csv.begin") ||
           sqlite_csv_fail(c, errno, "CSV import stopped");
}

static csv_status_t sqlite_csv_next(sqlite_csv_context_t *c, csv_t *reader,
                                   sqlite_csv_refill_fn refill, void *context)
{
    csv_status_t status;
    while ((status = csv_next(reader)) == CSV_NEED_INPUT) {
        if (!refill || !refill(reader, context)) {
            sqlite_csv_fail(c, ECANCELED, "CSV input ended before final EOF or was cancelled");
            return CSV_ERROR;
        }
    }
    if (status == CSV_ERROR)
        sqlite_csv_stream_error(c, reader);
    return status;
}

static bool sqlite_csv_insert(sqlite_csv_context_t *c, csv_t *reader, csv_status_t status,
                               sqlite_csv_refill_fn refill, void *context, size_t expected, bool compare_count)
{
    if (sqlite3_exec(c->native, "SAVEPOINT mars_csv_import", NULL, NULL, NULL) != SQLITE_OK)
        return sqlite_csv_sql_error(c);
    sqlite3_stmt *stmt = NULL;
    bool ok = sqlite_csv_schema(c, &stmt);
    size_t rows = 0;
    while (ok && status == CSV_ROW) {
        c->report->record = csv_record_count(reader);
        ok = computation_step_current("sqlite.csv.insert") ||
             sqlite_csv_fail(c, errno, "CSV import stopped");
        if (ok)
            ok = sqlite_csv_row(c, reader, false, stmt);
        if (ok && sqlite3_step(stmt) != SQLITE_DONE)
            ok = sqlite_csv_sql_error(c);
        if (ok && (sqlite3_reset(stmt) != SQLITE_OK || sqlite3_clear_bindings(stmt) != SQLITE_OK))
            ok = sqlite_csv_sql_error(c);
        if (ok) {
            ++rows;
            ++c->report->record;
            status = sqlite_csv_next(c, reader, refill, context);
        }
    }
    ok = ok && status == CSV_END;
    if (ok && compare_count && rows != expected)
        ok = sqlite_csv_fail(c, EINVAL, "CSV file changed between inference and insertion");
    if (ok && !computation_poll_current())
        ok = sqlite_csv_fail(c, errno, "CSV import stopped");
    sqlite3_finalize(stmt);
    if (ok && sqlite3_exec(c->native, "RELEASE mars_csv_import", NULL, NULL, NULL) != SQLITE_OK)
        ok = sqlite_csv_sql_error(c);
    if (!ok) {
        /* Native cleanup deliberately leaves the copied first diagnostic intact. */
        int rollback = sqlite3_exec(c->native, "ROLLBACK TO mars_csv_import", NULL, NULL, NULL);
        int release = sqlite3_exec(c->native, "RELEASE mars_csv_import", NULL, NULL, NULL);
        if ((rollback != SQLITE_OK || release != SQLITE_OK) && !sqlite3_get_autocommit(c->native))
            sqlite_set_error(c->db, "CSV import failed and savepoint cleanup failed; inspect the enclosing transaction");
        return false;
    }
    c->report->rows = rows;
    c->report->record = c->report->column = 0;
    return true;
}

/* Replay a borrowed file so late records participate in schema inference. */
bool sqlite_import_csv(sqlite_t *db, file_t *input, const string_t *table,
                       const sqlite_csv_options_t *options, sqlite_csv_report_t *report)
{
    sqlite_csv_report_t local;
    sqlite_csv_context_t c;
    bool ok = sqlite_csv_start(&c, db, table, options, report ? report : &local);
    int64_t start = 0;
    bool positioned = ok && input && file_tell(input, &start);
    if (ok && !positioned)
        ok = sqlite_csv_fail(&c, EINVAL, "CSV inference requires an open seekable file");
    csv_t *reader = ok ? csv_reader_new(input, &c.options.csv) : NULL;
    if (ok && !reader)
        ok = sqlite_csv_fail(&c, errno ? errno : EINVAL, "Creating CSV reader failed");
    csv_status_t status = ok ? sqlite_csv_next(&c, reader, NULL, NULL) : CSV_ERROR;
    size_t rows = 0;
    size_t header_width = reader ? csv_header_count(reader) : 0;
    if (ok && status != CSV_ERROR)
        c.report->record = 1;
    if (ok)
        ok = status != CSV_ERROR && sqlite_csv_columns(&c, reader, false);
    if (ok)
        c.report->record = header_width ? 2 : 1;
    while (ok && status == CSV_ROW) {
        c.report->record = csv_record_count(reader);
        ok = computation_step_current("sqlite.csv.infer") ||
             sqlite_csv_fail(&c, errno, "CSV inference stopped");
        if (ok)
            ok = sqlite_csv_row(&c, reader, true, NULL);
        if (ok) {
            ++rows;
            ++c.report->record;
            status = sqlite_csv_next(&c, reader, NULL, NULL);
        }
    }
    ok = ok && status == CSV_END;
    csv_free(reader);
    reader = NULL;
    if (ok && !file_seek(input, start, FILE_SEEK_BEGIN))
        ok = sqlite_csv_fail(&c, errno ? errno : EIO, "Rewinding CSV input failed");
    if (ok) {
        reader = csv_reader_new(input, &c.options.csv);
        if (!reader)
            ok = sqlite_csv_fail(&c, ENOMEM, "Recreating CSV reader failed");
    }
    if (ok) {
        status = sqlite_csv_next(&c, reader, NULL, NULL);
        ok = status != CSV_ERROR;
        if (ok && csv_header_count(reader) != header_width)
            ok = sqlite_csv_fail(&c, EINVAL, "CSV header width changed between passes");
        for (size_t i = 0; ok && i < csv_header_count(reader); ++i) {
            if ((!c.options.columns || !c.options.columns[i].name) &&
                string_compare(c.columns[i].name, csv_header(reader, i)) != 0)
                ok = sqlite_csv_fail(&c, EINVAL, "CSV header changed between passes");
        }
        if (ok)
            ok = sqlite_csv_insert(&c, reader, status, NULL, NULL, rows, true);
    }
    csv_free(reader);
    if (!ok && positioned)
        file_seek(input, start, FILE_SEEK_BEGIN);
    if (ok)
        sqlite_set_error(db, "");
    sqlite_csv_clear(&c);
    return ok;
}

/* A non-replayable stream must supply types instead of guessing from a prefix. */
bool sqlite_import_csv_reader(sqlite_t *db, csv_t *reader, const string_t *table,
                              const sqlite_csv_options_t *options, sqlite_csv_refill_fn refill, void *context,
                              sqlite_csv_report_t *report)
{
    sqlite_csv_report_t local;
    sqlite_csv_context_t c;
    bool ok = sqlite_csv_start(&c, db, table, options, report ? report : &local);
    if (ok && (!reader || !options || !options->columns))
        ok = sqlite_csv_fail(&c, EINVAL, "Stream imports require a reader and explicit column types");
    csv_status_t status = ok ? sqlite_csv_next(&c, reader, refill, context) : CSV_ERROR;
    if (ok)
        ok = status != CSV_ERROR && sqlite_csv_columns(&c, reader, true);
    if (ok)
        ok = sqlite_csv_insert(&c, reader, status, refill, context, 0, false);
    if (ok)
        sqlite_set_error(db, "");
    sqlite_csv_clear(&c);
    return ok;
}
