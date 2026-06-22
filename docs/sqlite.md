# `sqlite_t`

`sqlite_t` is an opaque SQLCipher-backed SQLite handle for storing MARS objects
in encrypted database files. Callers use ordinary SQLite concepts, while the
implementation requires a non-empty key and verifies that the encrypted database
can be read before returning a handle.

The class is intentionally small for now: it gives MARS a secure object store
without turning the rest of the codebase into a database wrapper. Higher-level
modules can build their own tables on top of `sqlite_exec()` and keep their
domain logic in MARS.

## Capabilities

- open or create an encrypted SQLCipher database
- reject empty keys
- reject databases opened with the wrong key
- enable foreign-key checks for each connection
- run SQL text through `sqlite_exec()` or `sqlite_exec_cstr()`
- create the built-in `mars_object` table
- store and load named binary objects with type and encoding metadata
- store and load `string_t` values as UTF-8 objects

## Example: Encrypted String Storage

```c
#include "sqlite.h"
#include "ustring.h"

int main(void) {
    string_t *path = string_new_with("secure.db");
    string_t *key = string_new_with("correct horse battery staple");
    string_t *name = string_new_with("note");
    string_t *value = string_new_with("classified: Mars dust");
    string_t *loaded = NULL;
    sqlite_t *db = sqlite_open_encrypted(path, key);

    if (!db)
        return 1;

    sqlite_init_object_store(db);
    sqlite_store_string(db, name, value);
    sqlite_load_string(db, name, &loaded);

    string_free(loaded);
    sqlite_close(db);
    string_free(value);
    string_free(name);
    string_free(key);
    string_free(path);
    return 0;
}
```

## Keys and Encryption

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
with `sqlite_free_blob()`.

`sqlite_store_string()` and `sqlite_load_string()` are convenience wrappers for
UTF-8 `string_t` values.

## Ownership

Every successful `sqlite_open_encrypted()` result must be released with
`sqlite_close()`. Loaded strings are ordinary `string_t` values owned by the
caller and must be released with `string_free()`.

When `sqlite_load_object()` succeeds, `out_type`, `out_encoding`, and `out_data`
are owned by the caller. Use `string_free()` for strings and `sqlite_free_blob()`
for the data blob.

## API Reference

All declarations are in `include/sqlite.h`.

### Types

- `sqlite_t` — opaque encrypted SQLite connection

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
- `sqlite_free_blob(data)` — release object data returned by `sqlite_load_object()`
- `sqlite_store_string(db, name, value)` — store a UTF-8 string
- `sqlite_load_string(db, name, out_value)` — load a UTF-8 string
