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

## Example: Encrypted String Storage

```c
#include <stdio.h>

#include "sqlite.h"
#include "ustring.h"

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

#include "sqlite.h"
#include "ustring.h"

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

#include "array.h"
#include "sqlite.h"
#include "ustring.h"

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

#include "datetime.h"
#include "sqlite.h"
#include "timeseries.h"
#include "ustring.h"

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

#include "expression.h"
#include "sqlite.h"
#include "ustring.h"

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

All declarations are in `include/sqlite.h`.

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
