/**
 * @file test_sqlite_csv.c
 * @brief CSV schema inference, streaming, rollback and query export regressions.
 *
 * Uses encrypted in-memory databases and temporary CSV files. Checks exact
 * preservation, safe identifiers, native constraints and bounded chunk input.
 * Complete SQLite-guide examples are registered in the final output lane.
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite-t.h"
#include "test_harness.h"
#include "test_sqlite_csv.h"

static sqlite_t *test_csv_database(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("CSV test key");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_free(key);
    string_free(path);
    return db;
}

static file_t *test_csv_source(const char *bytes)
{
    file_t *file = file_new_cstr(test_case_temp_path("input.csv"));
    bool ok = file && file_write_all_bytes(file, bytes, strlen(bytes)) &&
              file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ);
    if (!ok) {
        file_free(file);
        file = NULL;
    }
    return file;
}

static bool test_csv_import(sqlite_t *db, const char *source, const char *name,
                             const sqlite_csv_options_t *options, sqlite_csv_report_t *report)
{
    file_t *file = test_csv_source(source);
    string_t *table = string_new_with(name);
    bool ok = file && table && sqlite_import_csv(db, file, table, options, report);
    file_close(file);
    file_delete(file);
    file_free(file);
    string_free(table);
    return ok;
}

static string_t *test_csv_scalar(sqlite_t *db, const char *sql)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, sql);
    string_t *result = NULL;
    if (stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW) {
        const char *bytes = sqlite_stmt_column_text(stmt, 0);
        result = string_new();
        if (!result || string_append_bytes(result, bytes, sqlite_stmt_column_bytes(stmt, 0)) != 0) {
            string_free(result);
            result = NULL;
        }
    }
    sqlite_stmt_finalize(stmt);
    return result;
}

static void test_sqlite_csv_inference(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    sqlite_csv_report_t report;
    const char *data = "n,decimal,code,big,precise,mixed,empty,note\n"
                       "-9223372036854775808,1.25,00123,9223372036854775808,0.1234567890123456789,1,,\"é,🙂\"\n"
                       "9223372036854775807,2.5,00456,99999999999999999999,3.5,late text,,\"two\nlines\"\n";
    TEST_ASSERT_TRUE(test_csv_import(db, data, "values", NULL, &report), "infer entire file");
    TEST_ASSERT_INT_EQ(report.rows, 2);
    string_t *value = test_csv_scalar(db, "SELECT typeof(n)||'|'||typeof(decimal)||'|'||typeof(code)||'|'||"
                                        "typeof(big)||'|'||typeof(precise)||'|'||typeof(mixed)||'|'||typeof(empty) "
                                        "FROM \"values\" ORDER BY rowid LIMIT 1");
    TEST_ASSERT_STR_EQ(string_c_str(value), "integer|text|text|text|text|text|null");
    string_free(value);
    value = test_csv_scalar(db, "SELECT code||'|'||big||'|'||precise||'|'||mixed FROM \"values\" WHERE rowid=1");
    TEST_ASSERT_STR_EQ(string_c_str(value), "00123|9223372036854775808|0.1234567890123456789|1");
    string_free(value);
    sqlite_csv_options_t options = sqlite_csv_options_default();
    options.infer_real = true;
    TEST_ASSERT_TRUE(test_csv_import(db, "r,wide,small\n1.25,9007199254740993,1e-999\n2,0.5,2\n",
                                    "approximate", &options, &report), "real inference only when safe policy permits");
    value = test_csv_scalar(db, "SELECT typeof(r)||'|'||typeof(wide)||'|'||typeof(small) FROM approximate LIMIT 1");
    TEST_ASSERT_STR_EQ(string_c_str(value), "real|text|text");
    string_free(value);
    TEST_ASSERT_TRUE(test_csv_import(db, "n\n", "header_only", NULL, &report), "empty table from header");
    TEST_ASSERT_INT_EQ(report.rows, 0);
    options.empty_is_null = false;
    TEST_ASSERT_TRUE(test_csv_import(db, "n\n\n1\n", "empty_text", &options, &report), "empty text forces text type");
    value = test_csv_scalar(db, "SELECT typeof(n)||':'||length(n) FROM empty_text ORDER BY rowid LIMIT 1");
    TEST_ASSERT_STR_EQ(string_c_str(value), "text:0");
    string_free(value);
    sqlite_close(db);
}

static void test_sqlite_csv_mapping_and_identifiers(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "CREATE TABLE target (label TEXT, n INTEGER, omitted TEXT DEFAULT 'kept')"),
                     "destination has different column order");
    sqlite_csv_options_t options = sqlite_csv_options_default();
    options.create_table = false;
    sqlite_csv_report_t report;
    TEST_ASSERT_TRUE(test_csv_import(db, "N,label\n7,seven\n", "target", &options, &report), "map by folded header name");
    string_t *value = test_csv_scalar(db, "SELECT label||'|'||n||'|'||omitted FROM target");
    TEST_ASSERT_STR_EQ(string_c_str(value), "seven|7|kept");
    string_free(value);
    TEST_ASSERT_TRUE(!test_csv_import(db, "N,label\n00123,code\n", "target", &options, &report),
                     "do not coerce an inferred identifier into an integer");
    TEST_ASSERT_INT_EQ(report.code, EINVAL);
    const char *bad[] = {"a,A\n1,2", ",b\n1,2", "missing\n1"};
    for (size_t i = 0; i < 3; ++i)
        TEST_ASSERT_TRUE(!test_csv_import(db, bad[i], "target", &options, &report), "reject ambiguous or absent names");
    options.create_table = true;
    TEST_ASSERT_TRUE(test_csv_import(db, "\"a\"\"; DROP TABLE target;--\"\n\"'); DELETE FROM target;--\"\n",
                                    "x\"; DROP TABLE target;--", &options, &report), "identifiers are never SQL");
    value = test_csv_scalar(db, "SELECT count(*) FROM target");
    TEST_ASSERT_STR_EQ(string_c_str(value), "1");
    string_free(value);
    string_t *name = string_new_with("value");
    sqlite_csv_column_t column = {.name = name, .type = SQLITE_CSV_INTEGER};
    options.columns = &column;
    options.column_count = 1;
    options.csv.first_row_header = false;
    TEST_ASSERT_TRUE(test_csv_import(db, "00123\n-4\n", "explicit", &options, &report), "explicit headerless integer");
    value = test_csv_scalar(db, "SELECT value FROM explicit ORDER BY rowid LIMIT 1");
    TEST_ASSERT_STR_EQ(string_c_str(value), "123");
    string_free(value);
    column.type = SQLITE_CSV_REAL;
    TEST_ASSERT_TRUE(test_csv_import(db, "123456789012345678901\n", "real_override", &options, &report),
                     "explicit real authorises rounding");
    string_free(name);
    sqlite_close(db);
}

typedef struct {
    const char *source;
    size_t size, offset, chunk;
    bool cancel;
} test_csv_chunks_t;

static bool test_csv_refill(csv_t *reader, void *context)
{
    test_csv_chunks_t *chunks = context;
    if (chunks->cancel)
        return false;
    size_t count = chunks->size - chunks->offset;
    if (count > chunks->chunk)
        count = chunks->chunk;
    string_t *text = string_new();
    bool ok = text && string_append_bytes(text, chunks->source + chunks->offset, count) == 0;
    chunks->offset += count;
    ok = ok && csv_feed(reader, text, chunks->offset == chunks->size);
    string_free(text);
    return ok;
}

static bool test_csv_stream(sqlite_t *db, const char *data, const char *name, sqlite_csv_options_t *options,
                             sqlite_csv_report_t *report, bool cancel)
{
    csv_t *reader = csv_reader_new(NULL, &options->csv);
    string_t *table = string_new_with(name);
    test_csv_chunks_t chunks = {.source = data, .size = strlen(data), .chunk = 1, .cancel = cancel};
    bool ok = sqlite_import_csv_reader(db, reader, table, options, test_csv_refill, &chunks, report);
    string_free(table);
    csv_free(reader);
    return ok;
}

static void test_sqlite_csv_stream_rollback(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "CREATE TABLE target (n INTEGER UNIQUE, note TEXT);"
                                        "BEGIN; INSERT INTO target VALUES (5, 'outer')"), "outer transaction");
    sqlite_csv_options_t options = sqlite_csv_options_default();
    sqlite_csv_column_t columns[] = {{NULL, SQLITE_CSV_INTEGER}, {NULL, SQLITE_CSV_TEXT}};
    options.columns = columns;
    options.column_count = 2;
    options.create_table = false;
    sqlite_csv_report_t report;
    TEST_ASSERT_TRUE(!test_csv_stream(db, "n,note\n1,new\n5,duplicate\n", "target", &options, &report, false),
                     "constraint aborts entire import");
    TEST_ASSERT_INT_EQ(report.rows, 0);
    TEST_ASSERT_INT_EQ(report.record, 3);
    TEST_ASSERT_TRUE(report.sqlite_code != 0, "constraint diagnostic preserved");
    string_t *value = test_csv_scalar(db, "SELECT count(*) FROM target");
    TEST_ASSERT_STR_EQ(string_c_str(value), "1");
    string_free(value);
    TEST_ASSERT_TRUE(test_csv_stream(db, "n,note\n1,\"é,🙂\"\n2,\"a\nline\"\n", "target", &options, &report, false),
                     "bytewise stream with explicit schema");
    TEST_ASSERT_INT_EQ(report.rows, 2);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "ROLLBACK"), "import did not commit outer transaction");
    value = test_csv_scalar(db, "SELECT count(*) FROM target");
    TEST_ASSERT_STR_EQ(string_c_str(value), "0");
    string_free(value);
    options.create_table = true;
    TEST_ASSERT_TRUE(!test_csv_stream(db, "n,note\n1,ok\n2,\"unfinished", "broken", &options, &report, false),
                     "malformed later row rolls back new table");
    TEST_ASSERT_TRUE(report.csv_error.code != 0, "CSV error retained");
    value = test_csv_scalar(db, "SELECT count(*) FROM sqlite_master WHERE name='broken'");
    TEST_ASSERT_STR_EQ(string_c_str(value), "0");
    string_free(value);
    TEST_ASSERT_TRUE(!test_csv_stream(db, "n,note\n", "cancelled", &options, &report, true), "cancel callback");
    TEST_ASSERT_INT_EQ(report.code, ECANCELED);
    columns[0].type = SQLITE_CSV_AUTO;
    TEST_ASSERT_TRUE(!test_csv_stream(db, "n,note\n1,one\n", "auto_stream", &options, &report, false),
                     "non-replayable inference is disallowed");
    sqlite_close(db);
}

static void test_sqlite_csv_bad_input(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    const char *bad[] = {"n\n1\n\"unfinished", "n\n1\n2,extra", "n\n\xff", ""};
    sqlite_csv_report_t report;
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        file_t *file = test_csv_source(bad[i]);
        string_t *table = string_new_with("bad");
        TEST_ASSERT_NOT_NULL(file);
        TEST_ASSERT_TRUE(!sqlite_import_csv(db, file, table, NULL, &report), "malformed file rejected");
        TEST_ASSERT_TRUE(file_is_open(file), "borrowed source stays open");
        int64_t offset = -1;
        TEST_ASSERT_TRUE(file_tell(file, &offset) && offset == 0, "source rewound on failure");
        TEST_ASSERT_INT_EQ(report.rows, 0);
        file_close(file);
        file_delete(file);
        file_free(file);
        string_free(table);
    }
    sqlite_csv_options_t options = sqlite_csv_options_default();
    sqlite_csv_column_t column = {NULL, SQLITE_CSV_INTEGER};
    options.columns = &column;
    options.column_count = 1;
    const char *numbers[] = {"9223372036854775808", "1.5", "NaN", "0x10", " 1", "1tail"};
    for (size_t i = 0; i < sizeof(numbers) / sizeof(*numbers); ++i) {
        string_t *data = string_sprintf("n\n%s\n", numbers[i]);
        TEST_ASSERT_TRUE(!test_csv_import(db, string_c_str(data), "bad_number", &options, &report), "strict integer");
        TEST_ASSERT_INT_EQ(report.record, 2);
        TEST_ASSERT_INT_EQ(report.column, 1);
        string_free(data);
    }
    column.type = SQLITE_CSV_REAL;
    TEST_ASSERT_TRUE(!test_csv_import(db, "n\n1e999\n", "overflow", &options, &report), "non-finite real rejected");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "CREATE TABLE numeric_target(n NUMERIC)"), "unsupported affinity fixture");
    options.create_table = false;
    TEST_ASSERT_TRUE(!test_csv_import(db, "n\n1.5\n", "numeric_target", &options, &report), "no NUMERIC coercion");
    sqlite_close(db);
}

static void test_sqlite_csv_boundaries(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    sqlite_csv_report_t report;
    string_t *table = string_new_with("positioned");
    file_t *file = test_csv_source("skip\nn\n1\n");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(file_seek(file, 5, FILE_SEEK_BEGIN), "non-zero starting position");
    TEST_ASSERT_TRUE(sqlite_import_csv(db, file, table, NULL, &report), "import remaining file");
    TEST_ASSERT_INT_EQ(report.rows, 1);
    TEST_ASSERT_TRUE(file_is_open(file), "successful import borrows handle");
    TEST_ASSERT_TRUE(file_seek(file, 5, FILE_SEEK_BEGIN), "rewind fixture");
    TEST_ASSERT_TRUE(!sqlite_import_csv(db, file, table, NULL, &report), "never overwrite existing table");
    TEST_ASSERT_TRUE(!sqlite_import_csv(NULL, file, table, NULL, &report), "missing database");
    TEST_ASSERT_INT_EQ(report.code, EINVAL);
    TEST_ASSERT_TRUE(!sqlite_import_csv(db, NULL, table, NULL, &report), "missing input");
    file_close(file);
    file_delete(file);
    file_free(file);
    sqlite_csv_options_t options = sqlite_csv_options_default();
    sqlite_csv_column_t column = {NULL, SQLITE_CSV_INTEGER};
    options.columns = &column;
    options.column_count = 1;
    csv_t *reader = csv_reader_new(NULL, &options.csv);
    TEST_ASSERT_TRUE(!sqlite_import_csv_reader(db, reader, table, &options, NULL, NULL, &report),
                     "unfinished chunk input needs a callback");
    TEST_ASSERT_INT_EQ(report.code, ECANCELED);
    csv_free(reader);
    /* Supplied names are metadata, not a physical CSV header record. */
    reader = csv_reader_new(NULL, NULL);
    string_t *name = string_new_with("n");
    const string_t *names[] = {name};
    TEST_ASSERT_TRUE(csv_set_header(reader, names, 1), "provide stream names");
    string_t *invalid_number = string_new_with("bad\n");
    TEST_ASSERT_TRUE(csv_feed(reader, invalid_number, true), "headerless invalid number");
    TEST_ASSERT_TRUE(!sqlite_import_csv_reader(db, reader, table, &options, NULL, NULL, &report),
                     "existing table rejection occurs before conversion");
    string_t *fresh_table = string_new_with("provided_header");
    csv_free(reader);
    reader = csv_reader_new(NULL, NULL);
    TEST_ASSERT_TRUE(csv_set_header(reader, names, 1) && csv_feed(reader, invalid_number, true), "retry fresh reader");
    TEST_ASSERT_TRUE(!sqlite_import_csv_reader(db, reader, fresh_table, &options, NULL, NULL, &report),
                     "typed conversion fails on first physical record");
    TEST_ASSERT_INT_EQ(report.record, 1);
    csv_free(reader);
    string_free(fresh_table);
    string_free(invalid_number);
    string_free(name);
    options.column_count = 2;
    TEST_ASSERT_TRUE(!test_csv_import(db, "n\n1", "width", &options, &report), "override width mismatch");
    options.column_count = 1;
    options.empty_is_null = false;
    TEST_ASSERT_TRUE(!test_csv_import(db, "n\n\n", "numeric_empty", &options, &report), "empty text is not integer");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "CREATE VIEW v AS SELECT n FROM positioned"), "view fixture");
    options.create_table = false;
    TEST_ASSERT_TRUE(!test_csv_import(db, "n\n2", "v", &options, &report), "views are not import destinations");
    string_t *value = test_csv_scalar(db, "SELECT count(*) FROM positioned");
    TEST_ASSERT_STR_EQ(string_c_str(value), "1");
    string_free(value);
    string_free(table);
    sqlite_close(db);
}

static void test_sqlite_csv_export(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "CREATE TABLE t (n INTEGER, note TEXT, r REAL);"
                                        "INSERT INTO t VALUES (7, 'say \"yes\", please', 1.25), (8, NULL, 2.5)"),
                     "export fixture");
    csv_options_t options = csv_options_default();
    options.crlf = false;
    csv_t *writer = csv_writer_new(NULL, &options);
    string_t *query = string_new_with("SELECT n,note,r FROM t ORDER BY n");
    sqlite_csv_report_t report;
    TEST_ASSERT_TRUE(sqlite_export_csv(db, query, writer, true, &report), "export query");
    TEST_ASSERT_INT_EQ(report.rows, 2);
    TEST_ASSERT_STR_EQ(string_c_str(csv_output(writer)), "n,note,r\n7,\"say \"\"yes\"\", please\",1.25\n8,,2.5\n");
    string_free(query);
    csv_free(writer);
    writer = csv_writer_new(NULL, &options);
    query = string_new_with("SELECT CAST(x'65cc81' AS TEXT), 1.2345678901234567");
    TEST_ASSERT_TRUE(sqlite_export_csv(db, query, writer, false, &report), "exact Unicode and binary64 spelling");
    TEST_ASSERT_STR_EQ(string_c_str(csv_output(writer)), "e\xcc\x81,1.2345678901234567\n");
    string_free(query);
    csv_free(writer);
    const char *invalid[] = {"DELETE FROM t", "SELECT 1; DELETE FROM t", "SELECT ?1", "SELECT x'0102'",
                            "SELECT CAST(x'610062' AS TEXT)", "SELECT CAST(x'ff' AS TEXT)", "SELECT 1e999",
                            "PRAGMA foreign_keys = OFF", "WITH x AS (SELECT 1) DELETE FROM t"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        writer = csv_writer_new(NULL, &options);
        query = string_new_with(invalid[i]);
        TEST_ASSERT_TRUE(!sqlite_export_csv(db, query, writer, false, &report), "unsafe or unrepresentable export");
        TEST_ASSERT_TRUE(report.code != 0, "failure diagnosed");
        string_free(query);
        csv_free(writer);
    }
    writer = csv_writer_new(NULL, &options);
    query = string_new_with(" WITH rows(n) AS (SELECT 3) SELECT n FROM rows");
    TEST_ASSERT_TRUE(sqlite_export_csv(db, query, writer, false, &report), "read-only common table expression");
    TEST_ASSERT_STR_EQ(string_c_str(csv_output(writer)), "3\n");
    string_free(query);
    csv_free(writer);
    string_t *foreign_keys = test_csv_scalar(db, "PRAGMA foreign_keys");
    TEST_ASSERT_STR_EQ(string_c_str(foreign_keys), "1");
    string_free(foreign_keys);
    writer = csv_writer_new(NULL, &options);
    query = string_new_with("SELECT n FROM t WHERE 0");
    TEST_ASSERT_TRUE(sqlite_export_csv(db, query, writer, true, &report), "empty query still has headers");
    TEST_ASSERT_STR_EQ(string_c_str(csv_output(writer)), "n\n");
    string_free(query);
    csv_free(writer);
    options.max_buffer_bytes = 4;
    writer = csv_writer_new(NULL, &options);
    query = string_new_with("SELECT 'long value'");
    TEST_ASSERT_TRUE(!sqlite_export_csv(db, query, writer, false, &report), "output limit enforced");
    TEST_ASSERT_INT_EQ(report.code, EOVERFLOW);
    string_free(query);
    csv_free(writer);
    sqlite_close(db);
}

/* README example: scan the whole file and retain identifier spelling. */
static void example_sqlite_csv_file(void)
{
    char directory[] = "/tmp/mars-csv-import-XXXXXX";
    TEST_ASSERT_NOT_NULL(mkdtemp(directory));
    string_t *source_path = string_sprintf("%s/input.csv", directory);
    file_t *input = file_new(source_path), *dir = file_new_cstr(directory);
    string_t *text = string_new_with("code,count\n00123,7\n00456,12\n");
    string_t *path = string_new_with(":memory:"), *key = string_new_with("example key");
    string_t *table = string_new_with("measurements");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    sqlite_csv_report_t report;
    bool ok = db && input && file_write_all_text(input, text) &&
              file_open(input, FILE_MODE_OPEN, FILE_ACCESS_READ) && sqlite_import_csv(db, input, table, NULL, &report);
    sqlite_stmt_t *stmt = ok ? sqlite_stmt_prepare(db, "SELECT typeof(code), typeof(count) FROM measurements LIMIT 1") : NULL;
    ok = ok && stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    TEST_ASSERT_TRUE(ok, "README file import");
    string_printf("Imported %zu rows (code: %s, count: %s)\n", report.rows,
                  sqlite_stmt_column_text(stmt, 0), sqlite_stmt_column_text(stmt, 1));
    TEST_ASSERT_INT_EQ(report.rows, 2);
    TEST_ASSERT_STR_EQ(sqlite_stmt_column_text(stmt, 0), "text");
    TEST_ASSERT_STR_EQ(sqlite_stmt_column_text(stmt, 1), "integer");
    sqlite_stmt_finalize(stmt);
    file_close(input);
    file_delete(input);
    file_remove_directory(dir);
    file_free(input);
    file_free(dir);
    sqlite_close(db);
    string_free(table);
    string_free(key);
    string_free(path);
    string_free(text);
    string_free(source_path);
}

/* README example: an explicit schema permits a one-pass reader import. */
static void example_sqlite_csv_stream(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("example key");
    string_t *table = string_new_with("measurements");
    string_t *text = string_new_with("name,count\nZoë,7\nAda,12\n");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    sqlite_csv_options_t options = sqlite_csv_options_default();
    sqlite_csv_column_t columns[] = {{NULL, SQLITE_CSV_TEXT}, {NULL, SQLITE_CSV_INTEGER}};
    options.columns = columns;
    options.column_count = 2;
    csv_t *reader = csv_reader_new(NULL, &options.csv);
    sqlite_csv_report_t report;
    bool ok = db && reader && csv_feed(reader, text, true) &&
              sqlite_import_csv_reader(db, reader, table, &options, NULL, NULL, &report);
    TEST_ASSERT_TRUE(ok, "README import");
    string_printf("Imported %zu rows\n", report.rows);
    TEST_ASSERT_INT_EQ(report.rows, 2);
    csv_free(reader);
    sqlite_close(db);
    string_free(text);
    string_free(table);
    string_free(key);
    string_free(path);
}

/* README example: query values and headers are escaped by csv_t. */
static void example_sqlite_csv_export(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("example key");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_t *query = string_new_with("SELECT 'Zoë' AS name, 'hello, world' AS note");
    csv_options_t options = csv_options_default();
    options.crlf = false;
    csv_t *writer = csv_writer_new(NULL, &options);
    bool ok = db && writer && sqlite_export_csv(db, query, writer, true, NULL);
    TEST_ASSERT_TRUE(ok, "README export");
    string_printf("%S", csv_output(writer));
    TEST_ASSERT_STR_EQ(string_c_str(csv_output(writer)), "name,note\nZoë,\"hello, world\"\n");
    csv_free(writer);
    string_free(query);
    sqlite_close(db);
    string_free(key);
    string_free(path);
}

/* Caller-owned state passed unchanged to every refill invocation. */
typedef struct {
    file_t *input;
    size_t calls;
} example_csv_source_t;

/* Feed raw bytes: a chunk may end halfway through a UTF-8 character. */
static bool example_csv_refill(csv_t *reader, void *context)
{
    example_csv_source_t *source = context;
    char bytes[7];
    size_t count = 0;
    ++source->calls;
    if (!file_read(source->input, bytes, sizeof(bytes), &count))
        return false;
    if (count == 0)
        return csv_feed(reader, NULL, true);
    string_t *chunk = string_new();
    bool ok = chunk && string_append_bytes(chunk, bytes, count) == 0 && csv_feed(reader, chunk, false);
    string_free(chunk);
    return ok;
}

/* README example: callback-fed file input and incremental file output. */
static void example_sqlite_csv_file_stream(void)
{
    char directory[] = "/tmp/mars-csv-stream-XXXXXX";
    TEST_ASSERT_NOT_NULL(mkdtemp(directory));
    string_t *source_path = string_sprintf("%s/input.csv", directory);
    string_t *output_path = string_sprintf("%s/output.csv", directory);
    file_t *input = file_new(source_path), *output = file_new(output_path), *dir = file_new_cstr(directory);
    string_t *text = string_new_with("name,count\nZoë,7\n\"Ada, A.\",12\n");
    string_t *path = string_new_with(":memory:"), *key = string_new_with("example key");
    string_t *table = string_new_with("measurements");
    string_t *query = string_new_with("SELECT name,count FROM measurements ORDER BY count");
    sqlite_t *db = sqlite_open_encrypted(path, key);

    sqlite_csv_options_t options = sqlite_csv_options_default();
    sqlite_csv_column_t columns[] = {{NULL, SQLITE_CSV_TEXT}, {NULL, SQLITE_CSV_INTEGER}};
    options.columns = columns;
    options.column_count = 2;
    csv_t *reader = csv_reader_new(NULL, &options.csv);
    csv_options_t output_options = csv_options_default();
    output_options.crlf = false;
    csv_t *writer = NULL;
    bool file_backed = false;
    sqlite_csv_report_t imported = {0}, exported = {0};

    example_csv_source_t source = {.input = input, .calls = 0};
    sqlite_csv_refill_fn refill = example_csv_refill;
    bool ok = db && input && output && dir && text && table && query && reader &&
              file_write_all_text(input, text) && file_open(input, FILE_MODE_OPEN, FILE_ACCESS_READ);
    if (ok) {
        /* Register our callback and its context; the importer calls it until EOF. */
        ok = sqlite_import_csv_reader(db, reader, table, &options, refill, &source, &imported);
    }
    if (ok)
        ok = file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    if (ok) {
        writer = csv_writer_new(output, &output_options);
        ok = writer && sqlite_export_csv(db, query, writer, true, &exported);
        file_backed = writer && csv_output(writer) == NULL;
    }
    /* Export finishes the CSV writer, but borrowed files remain ours to close. */
    csv_free(writer);
    csv_free(reader);
    if (!file_close(input))
        ok = false;
    if (!file_close(output))
        ok = false;
    if (ok) {
        string_printf("Refill callback: %zu calls (including EOF)\n", source.calls);
        string_printf("Imported %zu rows; exported %zu rows\n", imported.rows, exported.rows);
    }

    /* Only this tiny fixture is read back in full, to verify the streamed output. */
    string_t *actual = ok ? file_read_all_text(output) : NULL;
    bool verified = actual && string_compare(actual, text) == 0;
    string_free(actual);

    /* The demonstration removes its temporary files; keep output.csv in a real application. */
    file_delete(input);
    file_delete(output);
    file_remove_directory(dir);
    file_free(input);
    file_free(output);
    file_free(dir);
    sqlite_close(db);
    string_free(query);
    string_free(table);
    string_free(key);
    string_free(path);
    string_free(text);
    string_free(output_path);
    string_free(source_path);
    TEST_ASSERT_TRUE(ok, "README callback import and file export");
    TEST_ASSERT_TRUE(file_backed, "export writes to the file, not a collected memory result");
    TEST_ASSERT_TRUE(verified, "streamed UTF-8 and quoted fields round trip exactly");
    TEST_ASSERT_INT_EQ(source.calls, 6);
    TEST_ASSERT_INT_EQ(imported.rows, 2);
    TEST_ASSERT_INT_EQ(exported.rows, 2);
}

static void test_sqlite_diagnostic_lifetimes(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    errno = ERANGE;
    bool ok = !sqlite_diagnostic(db) && errno == ERANGE;
    ok = !sqlite_exec_cstr(db, "SELECT * FROM missing_diagnostic_table") && ok;
    diagnostic_t *first = sqlite_diagnostic(db);
    ok = first && !strcmp(diagnostic_domain(first), "sqlite") && diagnostic_code(first) != 0 && ok;
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, "SELECT ?1");
    TEST_ASSERT_NOT_NULL(stmt);
    ok = !sqlite_stmt_bind_int(stmt, 2, 7) && ok;
    diagnostic_t *binding = sqlite_stmt_diagnostic(stmt);
    ok = binding && diagnostic_code(binding) == 25 && ok; /* SQLITE_RANGE, not a stale connection code. */
    sqlite_stmt_finalize(stmt);
    ok = sqlite_exec_cstr(db, "CREATE TABLE unique_values(value INTEGER UNIQUE); INSERT INTO unique_values VALUES(1)") && ok;
    stmt = sqlite_stmt_prepare(db, "INSERT INTO unique_values VALUES(1)");
    TEST_ASSERT_NOT_NULL(stmt);
    ok = sqlite_stmt_step(stmt) == SQLITE_STEP_ERROR && ok;
    diagnostic_t *constraint = sqlite_stmt_diagnostic(stmt);
    ok = constraint && (diagnostic_code(constraint) & 255) == 19 && ok;
    sqlite_stmt_finalize(stmt);
    sqlite_close(db);
    ok = first && strstr(diagnostic_message(first), "missing_diagnostic_table") &&
         binding && diagnostic_code(binding) == 25 && constraint && diagnostic_message(constraint) && ok;
    diagnostic_free(first);
    diagnostic_free(binding);
    diagnostic_free(constraint);
    ok = !sqlite_diagnostic(NULL) && errno == EINVAL && !sqlite_stmt_diagnostic(NULL) && errno == EINVAL && ok;
    TEST_ASSERT_TRUE(ok, "database and statement diagnostics retain the original code after updates and closure");
}

static void test_sqlite_csv_diagnostic_chain(void)
{
    sqlite_t *db = test_csv_database();
    TEST_ASSERT_NOT_NULL(db);
    sqlite_csv_report_t report;
    bool ok = !test_csv_import(db, "name\n\"unfinished", "data", NULL, &report);
    diagnostic_t *error = sqlite_diagnostic(db);
    const diagnostic_t *cause = error ? diagnostic_cause(error) : NULL;
    ok = error && !strcmp(diagnostic_domain(error), "sqlite.csv") && diagnostic_code(error) == report.code &&
         cause && !strcmp(diagnostic_domain(cause), "csv") && diagnostic_location(cause) &&
         diagnostic_location(cause)->offset == report.csv_error.byte &&
         diagnostic_location(cause)->line == report.csv_error.line && ok;
    ok = test_csv_import(db, "name\nvalid\n", "valid_data", NULL, &report) && ok;
    errno = ERANGE;
    ok = !sqlite_diagnostic(db) && errno == ERANGE && ok;
    sqlite_close(db);
    ok = cause && strstr(diagnostic_message(cause), "record 2") && ok;
    diagnostic_free(error);
    TEST_ASSERT_TRUE(ok, "ingestion chains retain CSV locations after rollback, reader cleanup and subsequent success");
}

/* README example: error records survive database closure without retaining the connection. */
static void example_sqlite_diagnostic(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("example key");
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    bool failed = db && !sqlite_exec_cstr(db, "SELECT * FROM missing_table");
    diagnostic_t *error = failed ? sqlite_diagnostic(db) : NULL;
    sqlite_close(db);
    string_free(path);
    string_free(key);
    if (error)
        printf("%s: %s\n", diagnostic_domain(error), diagnostic_message(error));
    bool ok = error != NULL;
    diagnostic_free(error);
    TEST_ASSERT_TRUE(ok, "README retained database error");
}

/* Register the ordinary cases in the existing SQLite suite. */
void test_sqlite_csv_cases(void)
{
    TEST_RUN_IN_GROUP(test_sqlite_csv_inference, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_csv_mapping_and_identifiers, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_csv_stream_rollback, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_csv_bad_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_csv_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_csv_export, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_diagnostic_lifetimes, tests, NULL);
    TEST_RUN_IN_GROUP(test_sqlite_csv_diagnostic_chain, tests, NULL);
}

/* Keep README demonstrations last in the suite. */
void test_sqlite_csv_examples(void)
{
    TEST_RUN_OUTPUT_IN_GROUP(example_sqlite_csv_file, readme_examples);
    TEST_RUN_OUTPUT_IN_GROUP(example_sqlite_csv_stream, readme_examples);
    TEST_RUN_OUTPUT_IN_GROUP(example_sqlite_csv_export, readme_examples);
    TEST_RUN_OUTPUT_IN_GROUP(example_sqlite_csv_file_stream, readme_examples);
    TEST_RUN_OUTPUT_IN_GROUP(example_sqlite_diagnostic, readme_examples);
}
