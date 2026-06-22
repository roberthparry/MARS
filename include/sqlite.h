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

typedef struct _sqlite_t sqlite_t;

sqlite_t *sqlite_open_encrypted(const string_t *path, const string_t *key);
void sqlite_close(sqlite_t *db);

const string_t *sqlite_last_error(const sqlite_t *db);

bool sqlite_exec(sqlite_t *db, const string_t *sql);
bool sqlite_exec_cstr(sqlite_t *db, const char *sql);

bool sqlite_init_object_store(sqlite_t *db);

bool sqlite_store_object(sqlite_t *db,
                         const string_t *name,
                         const string_t *type,
                         const string_t *encoding,
                         const void *data,
                         size_t data_len);

bool sqlite_load_object(sqlite_t *db,
                        const string_t *name,
                        string_t **out_type,
                        string_t **out_encoding,
                        void **out_data,
                        size_t *out_data_len);

void sqlite_free_blob(void *data);

bool sqlite_store_string(sqlite_t *db,
                         const string_t *name,
                         const string_t *value);

bool sqlite_load_string(sqlite_t *db,
                        const string_t *name,
                        string_t **out_value);

#endif /* MARS_SQLITE_H */
