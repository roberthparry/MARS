/**
 * @file test_database_seed.c
 * @brief Discover and import module-owned SQL definitions without a compiled module list.
 *
 * The shared schema lives in tests/test_database.sql. Each test module may supply tests/<module>/test_<module>.sql;
 * files are discovered once, sorted deterministically and applied inside the caller's transaction. Fixture copies
 * use the same discovery, preventing storage tests from accidentally exercising an incomplete seed catalogue.
 */
#include <string.h>
#include <sodium.h>

#include "array-t.h"
#include "file-t.h"
#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

typedef struct {
    string_t *module;
    string_t *path;
} test_sql_source_t;

static void test_sql_source_free(void *value)
{
    test_sql_source_t *source = value;
    string_free(source->module);
    string_free(source->path);
}

static int test_sql_source_compare(const void *left, const void *right)
{
    return string_compare(((const test_sql_source_t *)left)->module, ((const test_sql_source_t *)right)->module);
}

static array_t *test_sql_sources(const string_t *root)
{
    const char *base = root ? string_c_str(root) : ".";
    string_t *path = string_sprintf("%s/tests", base);
    file_t *directory = path ? file_new(path) : NULL;
    array_t *sources = array_create(sizeof(test_sql_source_t), NULL, test_sql_source_free);
    bool ok = directory && sources && file_open_directory(directory);
    file_info_t *entry = NULL;
    while (ok && (ok = file_read_directory(directory, &entry)) && entry) {
        if (file_info_type(entry) == FILE_TYPE_DIRECTORY) {
            const char *name = file_info_name(entry);
            test_sql_source_t source = {.module = string_new_with(name),
                .path = string_sprintf("%s/tests/%s/test_%s.sql", base, name, name)};
            file_t *candidate = source.path ? file_new(source.path) : NULL;
            ok = source.module && candidate;
            if (ok && file_exists(candidate)) {
                ok = array_add(sources, &source);
                if (ok)
                    source = (test_sql_source_t){0};
            }
            file_free(candidate);
            test_sql_source_free(&source);
        }
        file_info_free(entry);
        entry = NULL;
    }
    file_info_free(entry);
    file_free(directory);
    string_free(path);
    if (!ok) {
        array_destroy(sources);
        return NULL;
    }
    array_sort(sources, test_sql_source_compare);
    return sources;
}

static bool test_sql_hash_file(crypto_hash_sha256_state *hash, const string_t *root, const char *relative)
{
    string_t *path = string_sprintf("%s/%s", root ? string_c_str(root) : ".", relative);
    file_t *file = path ? file_new(path) : NULL;
    string_t *digest = file ? file_sha256(file) : NULL;
    bool ok = digest != NULL;
    if (ok) {
        crypto_hash_sha256_update(hash, (const unsigned char *)relative, strlen(relative) + 1u);
        crypto_hash_sha256_update(hash, (const unsigned char *)string_c_str(digest), string_byte_length(digest));
    }
    string_free(digest);
    file_free(file);
    string_free(path);
    return ok;
}

/* Stable content identities detect pulled changes even when their timestamps predate the last import. */
string_t *test_database_sources_fingerprint(const string_t *root)
{
    array_t *sources = test_sql_sources(root);
    crypto_hash_sha256_state hash;
    crypto_hash_sha256_init(&hash);
    static const unsigned char format[] = "MARS test sources v1";
    crypto_hash_sha256_update(&hash, format, sizeof format);
    bool ok = sources && test_sql_hash_file(&hash, root, "tests/test_database.sql") &&
              test_sql_hash_file(&hash, root, "tests/test_config.json");
    for (size_t i = 0u; ok && i < array_size(sources); ++i) {
        const char *module = string_c_str(((const test_sql_source_t *)array_get(sources, i))->module);
        string_t *relative = string_sprintf("tests/%s/test_%s.sql", module, module);
        ok = relative && test_sql_hash_file(&hash, root, string_c_str(relative));
        string_free(relative);
    }
    unsigned char digest[crypto_hash_sha256_BYTES];
    char fingerprint[crypto_hash_sha256_BYTES * 2u + 1u];
    crypto_hash_sha256_final(&hash, digest);
    sodium_bin2hex(fingerprint, sizeof fingerprint, digest, sizeof digest);
    array_destroy(sources);
    return ok ? string_new_with(fingerprint) : NULL;
}

/* A missing marker is stale, not evidence that any cases or history should be removed. */
bool test_database_sync_current(test_database_t *database, const string_t *root, bool *current)
{
    if (!current)
        return test_database_fail(database, "A synchronisation comparison requires a destination.");
    *current = false;
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = 'test_source_sync'\n"
        "    AND (SELECT user_version FROM pragma_user_version) = 5");
    bool ok = statement && sqlite3_step(statement) == SQLITE_ROW;
    bool present = ok && sqlite3_column_int(statement, 0) != 0;
    sqlite3_finalize(statement);
    if (!ok || !present)
        return ok;
    string_t *fingerprint = test_database_sources_fingerprint(root);
    if (!fingerprint)
        return test_database_fail(database, "Cannot fingerprint the test catalogue and SQL sources.");
    statement = test_database_prepare(database,
        "SELECT source_sha256 = ?1 FROM test_source_sync WHERE id = 1");
    if (statement)
        sqlite3_bind_text(statement, 1, string_c_str(fingerprint), -1, SQLITE_TRANSIENT);
    int status = statement ? sqlite3_step(statement) : SQLITE_ERROR;
    ok = status == SQLITE_ROW || status == SQLITE_DONE;
    if (status == SQLITE_ROW)
        *current = sqlite3_column_int(statement, 0) != 0;
    sqlite3_finalize(statement);
    string_free(fingerprint);
    return ok || test_database_fail(database, NULL);
}

static bool test_sql_import(test_database_t *database, const string_t *path)
{
    file_t *file = path ? file_new(path) : NULL;
    array_t *bytes = file ? file_read_all_bytes(file) : NULL;
    string_t *sql = bytes ? string_new() : NULL;
    bool ok = sql && !string_append_bytes(sql, array_size(bytes) ? array_get(bytes, 0u) : NULL, array_size(bytes)) &&
              test_database_exec(database, string_c_str(sql));
    if (!ok) {
        string_t *diagnostic = string_sprintf("Cannot import test SQL %s: %s", path ? string_c_str(path) : "(null)",
            bytes ? test_database_error(database) : "file cannot be read");
        test_database_fail(database, diagnostic ? string_c_str(diagnostic) : "Cannot import module test definitions.");
        string_free(diagnostic);
    }
    string_free(sql);
    array_destroy(bytes);
    file_free(file);
    return ok;
}

static bool test_sql_registry_upgrade(test_database_t *database, const char *column, const char *sql)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT count(*) FROM pragma_table_info('test_case_runner') WHERE name = ?1");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, column, -1, SQLITE_STATIC);
    bool row = sqlite3_step(statement) == SQLITE_ROW;
    bool exists = row && sqlite3_column_int(statement, 0) != 0;
    sqlite3_finalize(statement);
    return row && (exists || test_database_exec(database, sql));
}

/* Import every discovered module atomically with the shared schema and preserve existing case identities. */
bool test_database_seed(test_database_t *database, const string_t *root)
{
    string_t *path = string_sprintf("%s/tests/test_database.sql", root ? string_c_str(root) : ".");
    array_t *sources = test_sql_sources(root);
    bool ok = sources && test_sql_import(database, path) &&
              test_sql_registry_upgrade(database, "table_name", "ALTER TABLE test_case_runner ADD COLUMN table_name TEXT") &&
              test_sql_registry_upgrade(database, "operation", "ALTER TABLE test_case_runner ADD COLUMN operation TEXT") &&
              test_database_history_upgrade(database);
    for (size_t i = 0u; ok && i < array_size(sources); ++i)
        ok = test_sql_import(database, ((const test_sql_source_t *)array_get(sources, i))->path);
    string_free(path);
    array_destroy(sources);
    if (!ok && !database->error)
        test_database_fail(database, "Cannot discover module SQL definitions.");
    return ok;
}

static bool test_sql_copy(const string_t *source, const string_t *destination)
{
    file_t *from = source ? file_new(source) : NULL, *to = destination ? file_new(destination) : NULL;
    bool ok = from && to && file_copy(from, to, false);
    file_free(from);
    file_free(to);
    return ok;
}

/* Copy only authored SQL inputs; database contents, profiles and run history are never copied into fixtures. */
bool test_database_sources_copy(const string_t *source_root, const string_t *destination_root)
{
    const char *source = source_root ? string_c_str(source_root) : ".";
    const char *destination = destination_root ? string_c_str(destination_root) : NULL;
    array_t *sources = destination ? test_sql_sources(source_root) : NULL;
    string_t *base = destination ? string_sprintf("%s/tests", destination) : NULL;
    file_t *directory = base ? file_new(base) : NULL;
    string_t *schema = string_sprintf("%s/tests/test_database.sql", source);
    string_t *copy = destination ? string_sprintf("%s/tests/test_database.sql", destination) : NULL;
    bool ok = sources && directory && (file_exists(directory) || file_create_directory(directory, 0700u, true)) &&
              test_sql_copy(schema, copy);
    for (size_t i = 0u; ok && i < array_size(sources); ++i) {
        const test_sql_source_t *entry = array_get(sources, i);
        string_t *parent = string_sprintf("%s/tests/%s", destination, string_c_str(entry->module));
        file_t *folder = parent ? file_new(parent) : NULL;
        string_t *target = string_sprintf("%s/tests/%s/test_%s.sql", destination,
                                           string_c_str(entry->module), string_c_str(entry->module));
        ok = folder && (file_exists(folder) || file_create_directory(folder, 0700u, true)) &&
             test_sql_copy(entry->path, target);
        string_free(target);
        file_free(folder);
        string_free(parent);
    }
    file_free(directory);
    string_free(base);
    string_free(schema);
    string_free(copy);
    array_destroy(sources);
    return ok;
}
