# `sqlite_t`

`sqlite_t` is MARS's opaque SQLCipher-backed SQLite handle. It opens encrypted
database files, exposes a small amount of direct SQL execution, provides a
built-in encrypted object store, and now includes a public prepared-statement
API for higher-level modules such as `jurisdiction_t` and `almanac_t`.

The wrapper is intentionally modest. It does not try to hide SQLite concepts or
replace SQLite with a MARS-specific query language. Instead, it gives the rest
of the codebase:

- encrypted database opening with key validation
- a small convenience object store for named blobs and strings
- a safe public prepared-statement layer so other modules do not need sqlite
  internals
- normal SQLite semantics for schema design and SQL execution

The file module's `file_import_sqlite` and `file_export_sqlite` helpers store
contents and selected metadata, not live handles or descriptors. The object
store initialiser also creates the chunk table and replacement-cleanup trigger.
Transfers use savepoints and bounded 64 KiB chunks, with a content hash checked
before export publication. Database encryption already protects payloads;
additional file encryption is explicit and optional. See the
[file guide](file.md#transactional-sqlcipher-file-storage) for details and examples.

`sqlite_database_path` returns SQLite's borrowed main-database filename.
`sqlite_stmt_bind_int64` and `sqlite_stmt_column_int64` preserve signed 64-bit
offsets and sizes. Blob binding and object storage reject lengths above
`INT_MAX` before conversion; an empty blob remains a blob even when its data
pointer is NULL. Database-specific size limits still apply.

## Capabilities

- open or create an encrypted SQLCipher database
- reject empty keys
- reject databases opened with the wrong key
- enable foreign-key checks for each connection
- run SQL text through `sqlite_exec()` or `sqlite_exec_cstr()`
- create the built-in `mars_object` table
- store and load named binary objects with type and encoding metadata
- store and load `string_t` values as UTF-8 objects
- prepare parameterised SQL statements
- bind text, integers, doubles, and `NULL`
- step through result rows without exposing raw `sqlite3_stmt *`
- read text, integer, double, and `NULL` column values
- report statement-level errors
- import CSV files with full-file type inference, or explicitly typed streams
- export read-only query results through the CSV writer

## Example: Encrypted String Storage

```c
#include <stdio.h>

#include "sqlite-t.h"
#include "string-t.h"

int main(void) {
    string_t *path = string_new_with("secure.db");
    string_t *key = string_new_with("correct horse battery staple");
    string_t *name = string_new_with("note");
    string_t *value = string_new_with("机密：Mars dust 🚀🔴");
    string_t *loaded = NULL;
    sqlite_t *db = sqlite_open_encrypted(path, key);

    if (!db)
        return 1;

    sqlite_init_object_store(db);
    sqlite_store_string(db, name, value);
    sqlite_load_string(db, name, &loaded);
    string_printf("%S\n", loaded);

    string_free(loaded);
    sqlite_close(db);
    string_free(value);
    string_free(name);
    string_free(key);
    string_free(path);
    return 0;
}
```

Output:

```text
机密：Mars dust 🚀🔴
```

## Example: Prepared Statement Query

```c
#include <stdio.h>

#include "sqlite-t.h"
#include "string-t.h"

int main(void) {
    string_t *path = string_new_with("secure.db");
    string_t *key = string_new_with("correct horse battery staple");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    sqlite_stmt_t *stmt = NULL;

    if (!db)
        return 1;

    sqlite_exec_cstr(db,
        "CREATE TABLE IF NOT EXISTS city ("
        "  name TEXT NOT NULL,"
        "  population INTEGER NOT NULL"
        ");");
    sqlite_exec_cstr(db,
        "DELETE FROM city;");
    sqlite_exec_cstr(db,
        "INSERT INTO city(name, population) VALUES "
        "('Liverpool', 486100),"
        "('Leeds', 536280),"
        "('Manchester', 395500),"
        "('Rhyl', 25743);");

    stmt = sqlite_stmt_prepare(db,
        "SELECT name, population FROM city WHERE population >= ?1 ORDER BY population DESC;");
    if (!stmt || !sqlite_stmt_bind_int(stmt, 1, 350000)) {
        sqlite_stmt_finalize(stmt);
        sqlite_close(db);
        string_free(key);
        string_free(path);
        return 1;
    }

    while (sqlite_stmt_step(stmt) == SQLITE_STEP_ROW) {
        printf("%s: %d\n",
               sqlite_stmt_column_text(stmt, 0),
               sqlite_stmt_column_int(stmt, 1));
    }

    sqlite_stmt_finalize(stmt);
    sqlite_close(db);
    string_free(key);
    string_free(path);
    return 0;
}
```

Output:

```text
Leeds: 536280
Liverpool: 486100
Manchester: 395500
```

## Keys And Encryption

`sqlite_open_encrypted()` takes the database path and key as `string_t` values.
Both must contain at least one encoded byte. The key is passed to SQLCipher
using the string's UTF-8 byte length, not its character count.

The wrapper enables SQLCipher memory security where supported and checks the
database by reading `sqlite_master`. Opening with the wrong key therefore fails
early instead of producing confusing errors later.

## Foreign Keys

SQLite foreign-key enforcement is enabled per connection, so `sqlite_t` turns it
on automatically after opening the database:

```sql
PRAGMA foreign_keys = ON;
```

Schema created through `sqlite_exec()` can use normal SQLite foreign keys,
including `ON DELETE CASCADE` and `ON UPDATE CASCADE`.

## Object Store

`sqlite_init_object_store()` creates this table if needed:

```sql
CREATE TABLE IF NOT EXISTS mars_object (
    name TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    encoding TEXT NOT NULL,
    value BLOB NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

`sqlite_store_object()` upserts by `name`. `sqlite_load_object()` returns copies
of the metadata and a heap-allocated blob owned by the caller. Release that blob
with `sqlite_free_object_data()`.

`sqlite_store_string()` and `sqlite_load_string()` are convenience wrappers for
UTF-8 `string_t` values.

## Example: Storing an `array_t`

`sqlite_store_object()` stores bytes, not live container internals, so opaque
MARS objects should expose a serialisation pair that produces a type label, an
encoding label, and a payload buffer. For a plain `array_t` with bytewise-stable
elements, `array_serialize()` and `array_deserialise()` provide that route.

```c
#include <stdint.h>
#include <stdlib.h>

#include "array-t.h"
#include "sqlite-t.h"
#include "string-t.h"

int main(void) {
    string_t *path = string_new_with("secure.db");
    string_t *key = string_new_with("correct horse battery staple");
    string_t *name = string_new_with("fibonacci");
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *loaded_type = NULL;
    string_t *loaded_encoding = NULL;
    sqlite_t *db = sqlite_open_encrypted(path, key);
    array_t *values = array_create(sizeof(int32_t), NULL, NULL);
    array_t *loaded_values = NULL;
    void *payload = NULL;
    void *loaded_payload = NULL;
    size_t payload_len = 0u;
    size_t loaded_payload_len = 0u;
    int32_t nums[] = {1, 1, 2, 3, 5, 8};
    size_t i;

    if (!db || !values)
        return 1;

    for (i = 0; i < sizeof(nums) / sizeof(nums[0]); ++i)
        array_add(values, &nums[i]);

    array_serialize(values, &type, &encoding, &payload, &payload_len);
    sqlite_init_object_store(db);
    sqlite_store_object(db, name, type, encoding, payload, payload_len);
    sqlite_load_object(db, name,
                       &loaded_type, &loaded_encoding,
                       &loaded_payload, &loaded_payload_len);

    loaded_values = array_deserialise(loaded_payload, loaded_payload_len,
                                      loaded_type, loaded_encoding);
    for (i = 0; i < array_size(loaded_values); ++i)
        printf("%d ", *(int32_t *)array_get(loaded_values, i));
    printf("\n");

    array_destroy(loaded_values);
    sqlite_free_object_data(loaded_payload);
    free(payload);
    string_free(loaded_encoding);
    string_free(loaded_type);
    array_destroy(values);
    sqlite_close(db);
    string_free(encoding);
    string_free(type);
    string_free(name);
    string_free(key);
    string_free(path);
    return 0;
}
```

Output:

```text
1 1 2 3 5 8
```

## Example: Storing a `timeseries_t`

Opaque MARS objects are stored through `sqlite_store_object()` as serialised
payloads. `timeseries_t` now exposes `ts_serialize()` and `ts_deserialise()`,
so callers can keep the storage round-trip in one obvious public path.

```c
#include <stdlib.h>

#include "datetime-t.h"
#include "sqlite-t.h"
#include "timeseries-t.h"
#include "string-t.h"

int main(void) {
    string_t *path = string_new_with("secure.db");
    string_t *key = string_new_with("correct horse battery staple");
    string_t *name = string_new_with("weekly-signal");
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *loaded_type = NULL;
    string_t *loaded_encoding = NULL;
    datetime_t *start = datetime_from_string("2026-01-01");
    const double values[] = {12.5, 13.0, 15.25};
    timeseries_t *series = NULL;
    timeseries_t *loaded_series = NULL;
    string_t *loaded_text = NULL;
    void *payload = NULL;
    size_t payload_len = 0u;
    void *loaded_payload = NULL;
    size_t loaded_payload_len = 0u;
    sqlite_t *db = sqlite_open_encrypted(path, key);

    if (!db || !start)
        return 1;

    series = ts_new_regular_from_doubles(values, 3u, start,
                                         TS_FREQ_DAILY, TS_YEAR_CALENDAR);
    ts_serialize(series, &type, &encoding, &payload, &payload_len);

    sqlite_init_object_store(db);
    sqlite_store_object(db, name, type, encoding, payload, payload_len);
    sqlite_load_object(db, name,
                       &loaded_type, &loaded_encoding,
                       &loaded_payload, &loaded_payload_len);

    loaded_series = ts_deserialise(loaded_payload, loaded_payload_len,
                                   loaded_type, loaded_encoding);
    loaded_text = ts_to_text(loaded_series, TS_STRING_CSV);
    string_printf("%S", loaded_text);

    string_free(loaded_text);
    ts_free(loaded_series);
    sqlite_free_object_data(loaded_payload);
    free(payload);
    string_free(loaded_encoding);
    string_free(loaded_type);
    ts_free(series);
    datetime_dealloc(start);
    sqlite_close(db);
    string_free(encoding);
    string_free(type);
    string_free(name);
    string_free(key);
    string_free(path);
    return 0;
}
```

Output:

```text
date,value
01/01/2026,12.5
02/01/2026,13
03/01/2026,15.25
```

## Example: Storing an `expr_t`

`expr_t` now follows the same public pattern through `expr_serialize()` and
`expr_deserialise()`, which keeps the SQLite example short and avoids repeating
payload-shaping logic at each call site.

```c
#include <stdlib.h>

#include "expression-t.h"
#include "sqlite-t.h"
#include "string-t.h"

int main(void) {
    string_t *path = string_new_with("secure.db");
    string_t *key = string_new_with("correct horse battery staple");
    string_t *name = string_new_with("trajectory");
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *loaded_type = NULL;
    string_t *loaded_encoding = NULL;
    string_t *source = string_new_with("{ exp(@pi*sqrt(H_9)) | ; H_9 = 163 }");
    expr_t *expr = expr_from_text(source, NULL);
    expr_t *loaded_expr = NULL;
    string_t *loaded_roundtrip = NULL;
    string_t *value_text = NULL;
    number_t value;
    void *payload = NULL;
    size_t payload_len = 0u;
    void *loaded_payload = NULL;
    size_t loaded_payload_len = 0u;
    sqlite_t *db = sqlite_open_encrypted(path, key);

    if (!db || !expr)
        return 1;

    expr_serialize(expr, &type, &encoding, &payload, &payload_len);

    sqlite_init_object_store(db);
    sqlite_store_object(db, name, type, encoding, payload, payload_len);
    sqlite_load_object(db, name,
                       &loaded_type, &loaded_encoding,
                       &loaded_payload, &loaded_payload_len);

    loaded_expr = expr_deserialise(loaded_payload, loaded_payload_len,
                                   loaded_type, loaded_encoding);
    loaded_roundtrip = expr_to_text(loaded_expr, style_EXPRESSION);
    value = expr_eval(loaded_expr);
    value_text = num_to_string(value);
    string_printf("%S\n", loaded_roundtrip);
    string_printf("%S\n", value_text);

    string_free(value_text);
    num_destroy(&value);
    string_free(loaded_roundtrip);
    expr_free(loaded_expr);
    sqlite_free_object_data(loaded_payload);
    free(payload);
    string_free(loaded_encoding);
    string_free(loaded_type);
    expr_free(expr);
    sqlite_close(db);
    string_free(source);
    string_free(encoding);
    string_free(type);
    string_free(name);
    string_free(key);
    string_free(path);
    return 0;
}
```

Output:

```text
{ exp(π·√(H₉)) | ; H₉ = 163 }
262537412640768743.99999999999925007259719818568887935385633733699086270753741037821064791011860731295118134618606450419308388794975386404490572871447719681485232243203911647829148864228272013117831706501045222687801444841770346969463355707681723887681000923706539519386506362757657888558223948114276912100832
```

## Prepared Statements

For modules that need structured queries, `sqlite_t` exposes an opaque
`sqlite_stmt_t` handle instead of asking callers to include SQLite internals.

The workflow is:

1. prepare SQL with `sqlite_stmt_prepare()`
2. bind parameters with `sqlite_stmt_bind_*()`
3. call `sqlite_stmt_step()` until it returns `SQLITE_STEP_DONE`
4. read column values from each `SQLITE_STEP_ROW`
5. release the statement with `sqlite_stmt_finalize()`

Parameter indices are 1-based, matching SQLite itself, so the first placeholder
is `?1`, the second is `?2`, and so on.

Column accessors read values from the current row only. Text returned by
`sqlite_stmt_column_text()` is owned by SQLite and remains valid only until the
statement is stepped again, reset, or finalised.

`sqlite_stmt_reset()` lets a caller run the same prepared statement again, and
`sqlite_stmt_clear_bindings()` removes any previously bound parameters.

On failure, `sqlite_stmt_last_error()` returns statement-level error text. For
broader connection errors, use `sqlite_last_error()`.

## Ownership

Every successful `sqlite_open_encrypted()` result must be released with
`sqlite_close()`. Loaded strings are ordinary `string_t` values owned by the
caller and must be released with `string_free()`.

When `sqlite_load_object()` succeeds, `out_type`, `out_encoding`, and `out_data`
are owned by the caller. Use `string_free()` for strings and
`sqlite_free_object_data()`
for the data blob.

Prepared statements created with `sqlite_stmt_prepare()` are owned by the
caller and must be released with `sqlite_stmt_finalize()`. It is safe to call
`sqlite_close(NULL)`, `sqlite_stmt_finalize(NULL)`, and
`sqlite_free_object_data(NULL)`.

## API Reference

All declarations are in `include/sqlite-t.h`.

### Types

- `sqlite_t` — opaque encrypted SQLite connection
- `sqlite_stmt_t` — opaque prepared statement
- `sqlite_step_result_t` — result of stepping a prepared statement

### Connection

- `sqlite_open_encrypted(path, key)` — open or create an encrypted database
- `sqlite_close(db)` — close the connection and release the handle
- `sqlite_last_error(db)` — return the most recent error string held by the handle

### SQL Execution

- `sqlite_exec(db, sql)` — run SQL from a `string_t`
- `sqlite_exec_cstr(db, sql)` — run SQL from a C string

### Object Store

- `sqlite_init_object_store(db)` — create the default object table
- `sqlite_store_object(db, name, type, encoding, data, data_len)` — store a named object
- `sqlite_load_object(db, name, out_type, out_encoding, out_data, out_data_len)` — load a named object
- `sqlite_free_object_data(data)` — release object data returned by `sqlite_load_object()`
- `sqlite_store_string(db, name, value)` — store a UTF-8 string
- `sqlite_load_string(db, name, out_value)` — load a UTF-8 string

### Prepared Statements

- `sqlite_stmt_prepare(db, sql)` — prepare parameterised SQL
- `sqlite_stmt_finalize(stmt)` — release a prepared statement
- `sqlite_stmt_bind_text(stmt, index, value)` — bind UTF-8 text
- `sqlite_stmt_bind_int(stmt, index, value)` — bind an integer
- `sqlite_stmt_bind_double(stmt, index, value)` — bind a double
- `sqlite_stmt_bind_null(stmt, index)` — bind SQL `NULL`
- `sqlite_stmt_step(stmt)` — advance to the next row or completion
- `sqlite_stmt_reset(stmt)` — reset a statement for reuse
- `sqlite_stmt_clear_bindings(stmt)` — remove bound parameter values
- `sqlite_stmt_column_text(stmt, column)` — read text from the current row
- `sqlite_stmt_column_int(stmt, column)` — read an integer from the current row
- `sqlite_stmt_column_double(stmt, column)` — read a double from the current row
- `sqlite_stmt_column_is_null(stmt, column)` — test whether the current column is `NULL`
- `sqlite_stmt_last_error(stmt)` — return the most recent statement error

### `sqlite_stmt_bind_blob()`

Reports whether the condition described by stmt bind blob holds.

```c
bool sqlite_stmt_bind_blob(sqlite_stmt_t *stmt, int index, const void *value, size_t value_len);
```

### `sqlite_stmt_column_blob()`

Returns the public result described by stmt column blob.

```c
const void *sqlite_stmt_column_blob(sqlite_stmt_t *stmt, int column);
```

### `sqlite_stmt_column_bytes()`

Returns the public result described by stmt column bytes.

```c
size_t sqlite_stmt_column_bytes(sqlite_stmt_t *stmt, int column);
```

## CSV ingestion and export

CSV syntax belongs to [csv_t](csv.md); SQLite owns column policies, schema
creation, binding and transactions. These helpers add no external dependency.
File data is read through `file_t`, parsed as `string_t` fields and inserted
with one reusable prepared statement. Importer buffering follows the configured
CSV record limits and column plan, not the number of imported rows; SQLite's
database, cache and transaction-journal storage are separate.

### Public API

| API | Behaviour |
| --- | --- |
| `sqlite_csv_options_default()` | Return headers-on, strict-width, create-new-table, empty-as-NULL defaults. Decimal inference is off. |
| `sqlite_import_csv(db, input, table, options, report)` | Borrow an open seekable file, infer across the entire remaining input, rewind and insert. NULL options use defaults. |
| `sqlite_import_csv_reader(db, reader, table, options, refill, context, report)` | Borrow a fresh CSV reader and import once using explicit column types. Call the optional refill callback on `CSV_NEED_INPUT`. |
| `sqlite_export_csv(db, query, writer, header, report)` | Stream one read-only result to a borrowed fresh writer; optionally emit names, then finish the writer. |

Import and export functions return `bool`. Optional `sqlite_csv_report_t` output is reset
first. `rows` counts imported rows on success; failed imports report zero,
whereas failed exports retain the number of completely written records.
`record` and `column` identify the one-based CSV location when available;
records include a consumed header. `code` is an errno-style diagnostic,
`sqlite_code` captures a native database failure, and `csv_error` preserves
CSV parser/writer details. Zero record/column denotes no applicable position.
`sqlite_last_error` supplies a readable diagnostic without echoing field values.

### Column policies and names

`sqlite_csv_options_t` contains:

- `csv`: file-reader options and bounds; ignored when supplying your own reader.
- `columns` and `column_count`: optional positional overrides, exactly one
  `sqlite_csv_column_t` per input column. Each has a borrowed replacement
  `name` (NULL uses the header) and `type`.
- `create_table`: true creates a new table and fails if it exists; false appends
  without deleting or replacing existing rows.
- `empty_is_null`: true treats every empty field as SQL NULL, including quoted
  empties. False preserves empty text, which is incompatible with a numeric override.
- `infer_real`: explicitly permit approximate decimal inference.

Types are `SQLITE_CSV_AUTO`, `SQLITE_CSV_INTEGER`, `SQLITE_CSV_REAL` and
`SQLITE_CSV_TEXT`. AUTO is available only for the two-pass file importer.
Names are matched to existing columns independently of table column order.
Columns omitted from the CSV receive database defaults or NULL, subject to
constraints. Headerless input requires replacement names. Duplicate/empty
input names are acceptable only when replacements produce unique, non-empty
names. Matching uses SQLite's ASCII case folding, not Unicode case folding.
Names and values retain their Unicode spelling; the importer does not trim them.

Table names designate one unqualified name in the `main` schema. A dot is
part of the identifier, not a schema separator. Identifiers are quoted and
values bound: neither headers nor CSV values are interpolated as SQL code.
Existing views are rejected. INTEGER, REAL and TEXT affinities are supported
for destination columns, including declarations such as INT and VARCHAR.
NUMERIC, BLOB and untyped destination columns are rejected. Inferred/overridden
types must match the destination affinity; choose TEXT overrides when importing
numeric-looking text into TEXT columns. The importer deliberately adds stricter
validation than SQLite's normal [type-affinity rules](https://www.sqlite.org/datatype3.html).

### Conservative inference

The whole first pass participates; there is no sampling cutoff.

| Data in a column | Inferred type |
| --- | --- |
| Signed whole numbers within the 64-bit range | INTEGER |
| Leading-zero identifiers, negative integer zero, dates or other text | TEXT |
| Decimal/exponent notation with default options | TEXT |
| Decimal/exponent notation with `infer_real = true`, at most 15 significant digits and finite, non-underflowing conversion | REAL |
| Oversized integers, high-precision decimals, mixed text/numbers | TEXT |
| Mixed REAL and integers outside the exact binary64 integer range | TEXT |
| Only empty fields | TEXT, with NULL or empty values according to policy |

Explicit INTEGER accepts signed 64-bit decimal integers only. Explicit REAL
authorises rounding and accepts finite decimal numbers, including large integer
spellings; overflow, underflow, NaN, infinity and trailing junk fail.
Numeric conversion uses a private C numeric locale, never a process-wide locale
change. Leading/trailing whitespace is not stripped. TEXT overrides preserve
values without numeric conversion. Boolean/date interpretation is left to callers.

### Streaming and atomicity

A file import starts at the current byte position, leaves the borrowed handle
open at EOF on success and attempts to restore its starting position on failure.
Keep the input unchanged during both passes: this is not a snapshot of a file
being modified concurrently. Types, widths and row counts are rechecked, but
compatible content changes are not guaranteed to be detected.

For non-replayable input, supply explicit types through
`sqlite_import_csv_reader`. Configure the reader's headers yourself. Already-fed
final input and file-backed readers need no callback. A `sqlite_csv_refill_fn`
must feed a non-empty chunk or final EOF, returning true; false cancels the
import. The callback context remains caller-owned. It must not re-enter the
importer or use the database connection. The function is synchronous and holds
its transaction while requesting subsequent chunks.

Imports use a [savepoint](https://www.sqlite.org/lang_savepoint.html) around schema
creation and insertion, preserving an enclosing transaction. Parsing, conversion,
constraint, cancellation or write failures roll back the import; successful
imports do not commit a caller's transaction. Triggers retain normal SQL effects:
a trigger explicitly rolling back the outer transaction cannot be contained.
Rollback-cleanup failures are reported; inspect the connection before reusing it.
Calls require exclusive use of their database connection until they return.

### Export limitations

Pass one read-only SELECT or WITH query with result columns and no parameters.
Its first keyword, after whitespace, must be SELECT or WITH; leading comments
and PRAGMAs are rejected before preparation because some PRAGMAs have preparation-time
effects. Multiple statements and mutating SQL are rejected. Query text is trusted SQL supplied by
the application, not CSV data. Use SQL expressions to select or transform data
explicitly before export.

The writer controls delimiter, quoting, BOM and line endings. INTEGER output is
exact, REAL output uses 17 significant digits, and TEXT preserves its UTF-8
bytes. BLOBs require an explicit SQL encoding such as `hex(column)`; non-finite
reals, malformed UTF-8 and embedded NUL fail. NULL and empty text both produce
empty fields, so this is tabular interchange, not a lossless database backup.
A header-only file is emitted for an empty result when headers are enabled.

Export calls `csv_finish` but does not free the writer or close its file.
Failures can leave partial output. Choose file-module staging when atomic
publication is required. Reading/writing source CSV is not itself encrypted;
only storage in the SQLCipher database is encrypted at rest.

### Example: infer a CSV file

This complete programme creates a temporary input, imports it, reports its
storage types and removes the input.

```c
#include <stdlib.h>

#include "sqlite-t.h"

int main(void)
{
    char directory[] = "/tmp/mars-csv-import-XXXXXX";
    if (!mkdtemp(directory))
        return 1;
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
    if (ok)
        string_printf("Imported %zu rows (code: %s, count: %s)\n", report.rows,
                  sqlite_stmt_column_text(stmt, 0), sqlite_stmt_column_text(stmt, 1));
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
    return ok ? 0 : 1;
}
```

Output:

```text
Imported 2 rows (code: text, count: integer)
```

### Example: an explicitly typed reader

This example uses a single final chunk. The file-to-file streaming example below
shows the refill callback and a file-backed output writer together.

```c
#include "sqlite-t.h"

int main(void)
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
    if (ok)
        string_printf("Imported %zu rows\n", report.rows);
    csv_free(reader);
    sqlite_close(db);
    string_free(text);
    string_free(table);
    string_free(key);
    string_free(path);
    return ok ? 0 : 1;
}
```

Output:

```text
Imported 2 rows
```

### Example: export query results

```c
#include "sqlite-t.h"

int main(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("example key");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_t *query = string_new_with("SELECT 'Zoë' AS name, 'hello, world' AS note");
    csv_options_t options = csv_options_default();
    options.crlf = false;
    csv_t *writer = csv_writer_new(NULL, &options);
    bool ok = db && writer && sqlite_export_csv(db, query, writer, true, NULL);
    if (ok)
        string_printf("%S", csv_output(writer));
    csv_free(writer);
    string_free(query);
    sqlite_close(db);
    string_free(key);
    string_free(path);
    return ok ? 0 : 1;
}
```

Output:

```text
name,note
Zoë,"hello, world"
```

### Example: stream a CSV file in and query results out

This complete programme creates a small fixture, imports it through a refill
callback, then exports rows directly to another file. Neither transfer collects
the entire CSV in memory. The in-memory database is just for this demonstration;
use a database file instead of `:memory:` for large datasets.

The seven-byte input buffer deliberately splits the UTF-8 spelling of `Zoë`
and the quoted field `"Ada, A."` across callbacks. Normally use a larger buffer,
such as 4096 bytes. `string_append_bytes` preserves unfinished UTF-8 sequences;
do not normalise individual chunks. `csv_feed` copies the bytes, so the callback
can immediately release its temporary string. At EOF the callback supplies a
final empty chunk. Returning false cancels the import and rolls it back.

The reader must be created with a NULL file for the callback to run.
`example_csv_refill` is the callback implementation; `refill` holds its function
pointer, and `&source` is the caller-owned context passed to every invocation.
The context carries the borrowed input file and a call counter. The call to
`sqlite_import_csv_reader(..., refill, &source, ...)` registers both: the importer,
not `main`, invokes the callback whenever its reader needs input. The context
must remain valid until this synchronous import returns.

Export needs no callback: `csv_writer_new(output, ...)`
writes each encoded record to the open file, unlike a NULL-file writer, which
collects bounded output in memory. `sqlite_export_csv` finishes the writer;
the caller checks file-close errors and releases both handles. Failed exports
may leave a partial file; use staging before publishing production output.

```c
#include <stdlib.h>

#include "sqlite-t.h"

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

int main(void)
{
    char directory[] = "/tmp/mars-csv-stream-XXXXXX";
    if (!mkdtemp(directory))
        return 1;
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
    return ok ? 0 : 1;
}
```

Output:

```text
Refill callback: 6 calls (including EOF)
Imported 2 rows; exported 2 rows
```

The exported CSV preserves both rows, the UTF-8 name and the quoting around the
name containing a comma. The example removes its temporary directory afterwards.

These four examples run last in `tests/sqlite/test_sqlite_csv.c`, after the
ordinary tests in the existing `make -j1 test_sqlite` suite.
