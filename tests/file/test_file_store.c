/**
 * @file test_file_store.c
 * @brief SQLCipher file-content persistence tests.
 *
 * Checks import/export round trips, database argument guards and optional encrypted payloads. The README example
 * verifies content and metadata storage through the public file API.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include "file.h"
#include "array.h"
#include "sqlite.h"
#include "test_harness.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Exported tests are registered by test_file.c; this file owns no suite configuration. */
typedef struct {
    sqlite_t *db;
    string_t *name;
    file_t *source;
    file_t *destination;
} store_fixture_t;

static void store_fixture_close(store_fixture_t *fixture)
{
    sqlite_close(fixture->db);
    string_free(fixture->name);
    file_free(fixture->source);
    file_free(fixture->destination);
}

static bool store_fixture_open(store_fixture_t *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    string_t *path = string_new_with(test_case_temp_path("store.db"));
    string_t *key = string_new_with("file store test key");
    fixture->db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    string_free(path);
    string_free(key);
    fixture->name = string_new_with("stored");
    fixture->source = file_new_cstr(test_case_temp_path("source.bin"));
    fixture->destination = file_new_cstr(test_case_temp_path("destination.bin"));
    if (fixture->db && fixture->name && fixture->source && fixture->destination)
        return true;
    store_fixture_close(fixture);
    return false;
}

static unsigned char *store_pattern(size_t size, unsigned seed)
{
    unsigned char *bytes = malloc(size ? size : 1);
    if (bytes)
        for (size_t i = 0; i < size; ++i)
            bytes[i] = (unsigned char)(i * 37u + (i >> 8) + seed);
    return bytes;
}

static bool store_bytes_equal(file_t *file, const void *expected, size_t size)
{
    array_t *bytes = file_read_all_bytes(file);
    bool ok = bytes && array_size(bytes) == size &&
        (!size || memcmp(array_get(bytes, 0), expected, size) == 0);
    array_destroy(bytes);
    return ok;
}

static bool store_scalar_equals(sqlite_t *db, const char *sql, int64_t expected)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, sql);
    bool ok = stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW &&
        sqlite_stmt_column_int64(stmt, 0) == expected && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool store_set_manifest(sqlite_t *db, const void *bytes, size_t size)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, "update mars_object set value=?1 where name='stored'");
    bool ok = stmt && sqlite_stmt_bind_blob(stmt, 1, bytes, size) && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool store_no_staging_files(void)
{
    file_t *directory = file_new_cstr(test_case_temp_dir());
    array_t *entries = directory ? file_list_directory(directory) : NULL;
    bool ok = entries != NULL;
    for (size_t i = 0; entries && i < array_size(entries); ++i) {
        file_info_t *entry = *(file_info_t **)array_get(entries, i);
        const char *name = file_info_name(entry);
        if (name && strncmp(name, ".mars-output-", 13) == 0)
            ok = false;
    }
    array_destroy(entries);
    file_free(directory);
    return ok;
}

void test_file_sqlite_round_trip_sizes(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    const size_t sizes[] = {0, 1, 65536, 65537, 131089};
    unsigned char *bytes = store_pattern(131089, 9);
    ASSERT_NOT_NULL(bytes);
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        size_t size = sizes[i];
        ASSERT_TRUE(file_write_all_bytes(fixture.source, bytes, size));
        ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, size));
        ASSERT_EQ_INT(file_last_error(fixture.source), 0);
        ASSERT_TRUE(!file_is_open(fixture.source));
        ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from mars_file_chunk where name='stored'",
                                        (int64_t)((size + 65535) / 65536)));
        ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, size, i != 0, false));
        ASSERT_EQ_INT(file_last_error(fixture.destination), 0);
        ASSERT_TRUE(!file_is_open(fixture.destination));
        ASSERT_TRUE(store_bytes_equal(fixture.destination, bytes, size));
        ASSERT_TRUE(store_bytes_equal(fixture.source, bytes, size));
    }
    ASSERT_TRUE(store_no_staging_files());
    free(bytes);
    store_fixture_close(&fixture);
}

void test_file_sqlite_optional_metadata(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, "metadata", 8));
    ASSERT_TRUE(file_chmod(fixture.source, 04740));
    ASSERT_TRUE(file_set_times(fixture.source, 946684800, 0, 946684801, 123456789));
    file_info_t *original = file_get_info(fixture.source);
    ASSERT_NOT_NULL(original);
    int64_t expected_seconds, seconds;
    long expected_nanoseconds, nanoseconds;
    ASSERT_TRUE(file_info_last_write_time(original, &expected_seconds, &expected_nanoseconds));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 8));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 8, false, false));
    file_info_t *plain = file_get_info(fixture.destination);
    ASSERT_NOT_NULL(plain);
    ASSERT_EQ_INT(file_info_permissions(plain), 0600);
    ASSERT_TRUE(file_info_last_write_time(plain, &seconds, &nanoseconds));
    ASSERT_TRUE(seconds != expected_seconds || nanoseconds != expected_nanoseconds);
    file_info_free(plain);
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 8, true, true));
    file_info_t *restored = file_get_info(fixture.destination);
    ASSERT_NOT_NULL(restored);
    ASSERT_EQ_INT(file_info_permissions(restored), file_info_permissions(original) & 0777);
    ASSERT_TRUE(file_info_last_write_time(restored, &seconds, &nanoseconds));
    ASSERT_TRUE(seconds == expected_seconds && nanoseconds == expected_nanoseconds);
    ASSERT_TRUE(store_bytes_equal(fixture.destination, "metadata", 8));
    file_info_free(restored);
    file_info_free(original);
    store_fixture_close(&fixture);
}

void test_file_sqlite_limits_and_destination_preservation(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, "original", 8));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 8));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, "replacement", 11));
    ASSERT_TRUE(!file_import_sqlite(fixture.source, fixture.db, fixture.name, 10));
    ASSERT_EQ_INT(file_last_error(fixture.source), EFBIG);
    ASSERT_TRUE(!file_is_open(fixture.source));
    ASSERT_TRUE(!file_import_sqlite(fixture.source, fixture.db, fixture.name, 0));
    ASSERT_EQ_INT(file_last_error(fixture.source), EFBIG);
    ASSERT_TRUE(file_write_all_bytes(fixture.destination, "keep", 4));
    ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, 7, true, true));
    ASSERT_EQ_INT(file_last_error(fixture.destination), EFBIG);
    ASSERT_TRUE(store_bytes_equal(fixture.destination, "keep", 4));
    ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, 8, false, false));
    ASSERT_EQ_INT(file_last_error(fixture.destination), EEXIST);
    ASSERT_TRUE(store_bytes_equal(fixture.destination, "keep", 4));
    ASSERT_TRUE(file_delete(fixture.destination));
    ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, 0, false, false));
    ASSERT_EQ_INT(file_last_error(fixture.destination), EFBIG);
    ASSERT_TRUE(!file_exists(fixture.destination));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 8, false, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, "original", 8));
    ASSERT_TRUE(store_no_staging_files());
    store_fixture_close(&fixture);
}

void test_file_sqlite_corrupt_chunks(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    unsigned char *bytes = store_pattern(131089, 17);
    ASSERT_NOT_NULL(bytes);
    ASSERT_TRUE(file_write_all_bytes(fixture.source, bytes, 131089));
    ASSERT_TRUE(file_write_all_bytes(fixture.destination, "preserved", 9));
    const char *const corruptions[] = {
        "delete from mars_file_chunk where name='stored' and ordinal=0",
        "delete from mars_file_chunk where name='stored' and ordinal=1",
        "delete from mars_file_chunk where name='stored' and ordinal=2",
        "update mars_file_chunk set ordinal=-1 where name='stored' and ordinal=0",
        "update mars_file_chunk set ordinal=4294967296 where name='stored' and ordinal=0",
        "update mars_file_chunk set value=zeroblob(65536) where name='stored' and ordinal=0",
        "update mars_file_chunk set value=zeroblob(17) where name='stored' and ordinal=2",
        "update mars_file_chunk set value=zeroblob(0) where name='stored' and ordinal=1",
        "update mars_file_chunk set value=zeroblob(65537) where name='stored' and ordinal=1",
        "update mars_file_chunk set value=zeroblob(65535) where name='stored' and ordinal=1",
        "insert into mars_file_chunk values('stored', 3, x'01')"
    };
    for (size_t i = 0; i < sizeof(corruptions) / sizeof(corruptions[0]); ++i) {
        ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 131089));
        ASSERT_TRUE(sqlite_exec_cstr(fixture.db, corruptions[i]));
        ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, 131089, true, true));
        ASSERT_EQ_INT(file_last_error(fixture.destination), EBADMSG);
        ASSERT_TRUE(!file_is_open(fixture.destination));
        ASSERT_TRUE(store_bytes_equal(fixture.destination, "preserved", 9));
        ASSERT_TRUE(store_no_staging_files());
    }
    /* A failed validation must not create a destination which did not already exist. */
    ASSERT_TRUE(file_delete(fixture.destination));
    ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, 131089, false, false));
    ASSERT_EQ_INT(file_last_error(fixture.destination), EBADMSG);
    ASSERT_TRUE(!file_exists(fixture.destination));
    ASSERT_TRUE(store_no_staging_files());
    free(bytes);
    store_fixture_close(&fixture);
}

void test_file_sqlite_corrupt_or_missing_manifest(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, "contents", 8));
    ASSERT_TRUE(file_write_all_bytes(fixture.destination, "keep", 4));
    const char *const corruptions[] = {
        "delete from mars_object where name='stored'",
        "update mars_object set type='string' where name='stored'",
        "update mars_object set encoding='unknown' where name='stored'",
        "update mars_object set value=zeroblob(0) where name='stored'",
        "update mars_object set value=zeroblob(63) where name='stored'",
        "update mars_object set value=zeroblob(65) where name='stored'",
        "update mars_object set value=zeroblob(64) where name='stored'"
    };
    for (size_t i = 0; i < sizeof(corruptions) / sizeof(corruptions[0]); ++i) {
        ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 8));
        ASSERT_TRUE(sqlite_exec_cstr(fixture.db, corruptions[i]));
        ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, 8, true, true));
        ASSERT_EQ_INT(file_last_error(fixture.destination), EBADMSG);
        ASSERT_TRUE(store_bytes_equal(fixture.destination, "keep", 4));
    }
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 8));
    void *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(sqlite_load_object(fixture.db, fixture.name, NULL, NULL, &data, &size));
    ASSERT_EQ_LONG(size, 64);
    /* Version, permission bits, nanoseconds, declared length and digest are independently validated. */
    const size_t offsets[] = {7, 16, 28, 15, 32};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        unsigned char manifest[64];
        memcpy(manifest, data, sizeof(manifest));
        manifest[offsets[i]] ^= 0x80;
        ASSERT_TRUE(store_set_manifest(fixture.db, manifest, sizeof(manifest)));
        ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, UINT64_MAX, true, true));
        ASSERT_EQ_INT(file_last_error(fixture.destination), EBADMSG);
        ASSERT_TRUE(store_bytes_equal(fixture.destination, "keep", 4));
    }
    unsigned char manifest[64];
    memcpy(manifest, data, sizeof(manifest));
    memset(manifest + 8, 0xff, 8);
    ASSERT_TRUE(store_set_manifest(fixture.db, manifest, sizeof(manifest)));
    ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, INT64_MAX, true, false));
    ASSERT_EQ_INT(file_last_error(fixture.destination), EFBIG);
    ASSERT_TRUE(store_bytes_equal(fixture.destination, "keep", 4));
    ASSERT_TRUE(store_no_staging_files());
    sqlite_free_object_data(data);
    store_fixture_close(&fixture);
}

void test_file_sqlite_import_rollback(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    unsigned char *old_bytes = store_pattern(131089, 1);
    unsigned char *new_bytes = store_pattern(131089, 2);
    ASSERT_NOT_NULL(old_bytes);
    ASSERT_NOT_NULL(new_bytes);
    ASSERT_TRUE(file_write_all_bytes(fixture.source, old_bytes, 131089));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 131089));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, new_bytes, 131089));
    ASSERT_TRUE(sqlite_exec_cstr(fixture.db,
        "create table caller_marker(value integer);"
        "create trigger fail_second_chunk before insert on mars_file_chunk when new.ordinal=1 "
        "begin select raise(abort, 'injected chunk failure'); end;"
        "begin; insert into caller_marker values(1)"));
    ASSERT_TRUE(!file_import_sqlite(fixture.source, fixture.db, fixture.name, 131089));
    ASSERT_EQ_INT(file_last_error(fixture.source), EIO);
    ASSERT_TRUE(!file_is_open(fixture.source));
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from caller_marker", 1));
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from mars_file_chunk", 3));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 131089, false, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, old_bytes, 131089));
    ASSERT_TRUE(sqlite_exec_cstr(fixture.db, "rollback; drop trigger fail_second_chunk"));
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from caller_marker", 0));

    /* Successful import/export must also leave the enclosing transaction under caller control. */
    ASSERT_TRUE(sqlite_exec_cstr(fixture.db, "begin; insert into caller_marker values(2)"));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 131089));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 131089, true, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, new_bytes, 131089));
    ASSERT_TRUE(sqlite_exec_cstr(fixture.db, "rollback"));
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from caller_marker", 0));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 131089, true, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, old_bytes, 131089));
    free(new_bytes);
    free(old_bytes);
    store_fixture_close(&fixture);
}

void test_file_sqlite_replacement_clears_chunks(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    unsigned char *bytes = store_pattern(131089, 21);
    ASSERT_NOT_NULL(bytes);
    ASSERT_TRUE(file_write_all_bytes(fixture.source, bytes, 131089));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 131089));
    string_t *other = string_new_with("unrelated");
    ASSERT_NOT_NULL(other);
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, other, 131089));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, "short", 5));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 5));
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from mars_file_chunk where name='stored'", 1));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 5, false, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, "short", 5));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, NULL, 0));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 0));
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from mars_file_chunk where name='stored'", 0));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 0, true, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, NULL, 0));
    ASSERT_TRUE(file_write_all_bytes(fixture.source, bytes, 131089));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 131089));
    string_t *text = string_new_with("ordinary object");
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(sqlite_store_string(fixture.db, fixture.name, text));
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from mars_file_chunk where name='stored'", 0));
    string_t *loaded = NULL;
    ASSERT_TRUE(sqlite_load_string(fixture.db, fixture.name, &loaded));
    TEST_ASSERT_STR_EQ(string_c_str(loaded), "ordinary object");
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from mars_file_chunk where name='unrelated'", 3));
    ASSERT_TRUE(file_export_sqlite(fixture.db, other, fixture.destination, 131089, true, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, bytes, 131089));
    string_free(loaded);
    string_free(text);
    string_free(other);
    free(bytes);
    store_fixture_close(&fixture);
}

void test_file_sqlite_value_api_guards(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(fixture.db, "select ?1, typeof(?2), length(?2), ?2");
    ASSERT_NOT_NULL(stmt);
    const int64_t values[] = {INT64_MIN, INT64_MAX, INT64_C(4294967296), -INT64_C(4294967296), 0};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        ASSERT_TRUE(sqlite_stmt_bind_int64(stmt, 1, values[i]));
        ASSERT_TRUE(sqlite_stmt_bind_blob(stmt, 2, i % 2 ? "" : NULL, 0));
        ASSERT_EQ_INT(sqlite_stmt_step(stmt), SQLITE_STEP_ROW);
        ASSERT_TRUE(sqlite_stmt_column_int64(stmt, 0) == values[i]);
        TEST_ASSERT_STR_EQ(sqlite_stmt_column_text(stmt, 1), "blob");
        ASSERT_EQ_LONG(sqlite_stmt_column_int64(stmt, 2), 0);
        ASSERT_TRUE(!sqlite_stmt_column_is_null(stmt, 3));
        ASSERT_EQ_LONG(sqlite_stmt_column_bytes(stmt, 3), 0);
        ASSERT_EQ_INT(sqlite_stmt_step(stmt), SQLITE_STEP_DONE);
        sqlite_stmt_reset(stmt);
    }
    ASSERT_TRUE(!sqlite_stmt_bind_int64(NULL, 1, 1));
    ASSERT_TRUE(!sqlite_stmt_bind_int64(stmt, 0, 1));
    ASSERT_TRUE(!sqlite_stmt_bind_int64(stmt, 3, 1));
    ASSERT_TRUE(sqlite_stmt_column_int64(NULL, 0) == 0);
    ASSERT_TRUE(!sqlite_stmt_bind_blob(stmt, 2, NULL, 1));
    unsigned char byte = 42;
    ASSERT_TRUE(!sqlite_stmt_bind_blob(stmt, 2, &byte, (size_t)INT_MAX + 1));
    ASSERT_TRUE(!sqlite_stmt_bind_blob(stmt, 2, &byte, SIZE_MAX));
    sqlite_stmt_finalize(stmt);
    ASSERT_TRUE(sqlite_init_object_store(fixture.db));
    string_t *type = string_new_with("bytes");
    string_t *encoding = string_new_with("raw");
    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(encoding);
    ASSERT_TRUE(sqlite_store_object(fixture.db, fixture.name, type, encoding, NULL, 0));
    ASSERT_TRUE(store_scalar_equals(fixture.db,
        "select count(*) from mars_object where name='stored' and typeof(value)='blob' and length(value)=0", 1));
    ASSERT_TRUE(!sqlite_store_object(fixture.db, fixture.name, type, encoding, &byte, (size_t)INT_MAX + 1));
    ASSERT_TRUE(!sqlite_store_object(fixture.db, fixture.name, type, encoding, &byte, SIZE_MAX));
    ASSERT_TRUE(!sqlite_store_object(fixture.db, fixture.name, type, encoding, NULL, 1));
    void *loaded = NULL;
    size_t size = 99;
    ASSERT_TRUE(sqlite_load_object(fixture.db, fixture.name, NULL, NULL, &loaded, &size));
    ASSERT_EQ_LONG(size, 0);
    sqlite_free_object_data(loaded);
    string_free(encoding);
    string_free(type);
    store_fixture_close(&fixture);
}

void test_file_sqlite_database_path_and_argument_guards(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    ASSERT_TRUE(sqlite_database_path(NULL) == NULL);
    const char *path = sqlite_database_path(fixture.db);
    ASSERT_NOT_NULL(path);
    ASSERT_TRUE(*path != '\0');
    file_t *database = file_new_cstr(path);
    file_t *alias = file_new_cstr(test_case_temp_path("database-alias.db"));
    ASSERT_NOT_NULL(database);
    ASSERT_NOT_NULL(alias);
    ASSERT_TRUE(file_write_all_bytes(fixture.source, "safe", 4));
    ASSERT_TRUE(file_import_sqlite(fixture.source, fixture.db, fixture.name, 4));
    ASSERT_TRUE(file_create_hard_link(database, alias));
    file_t *const database_paths[] = {database, alias};
    for (size_t i = 0; i < sizeof(database_paths) / sizeof(database_paths[0]); ++i) {
        ASSERT_TRUE(!file_import_sqlite(database_paths[i], fixture.db, fixture.name, UINT64_MAX));
        ASSERT_EQ_INT(file_last_error(database_paths[i]), EINVAL);
        ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, database_paths[i], UINT64_MAX, true, false));
        ASSERT_EQ_INT(file_last_error(database_paths[i]), EINVAL);
    }
    ASSERT_TRUE(store_scalar_equals(fixture.db, "select count(*) from mars_object where name='stored'", 1));
    ASSERT_TRUE(!file_import_sqlite(fixture.source, NULL, fixture.name, 4));
    ASSERT_EQ_INT(file_last_error(fixture.source), EINVAL);
    ASSERT_TRUE(!file_export_sqlite(NULL, fixture.name, fixture.destination, 4, false, false));
    ASSERT_EQ_INT(file_last_error(fixture.destination), EINVAL);
    string_t *empty = string_new_with("");
    string_t *nul = string_new();
    ASSERT_NOT_NULL(empty);
    ASSERT_NOT_NULL(nul);
    ASSERT_EQ_INT(string_append_chars(nul, "a\0b", 3), 0);
    const string_t *const bad_names[] = {NULL, empty, nul};
    for (size_t i = 0; i < sizeof(bad_names) / sizeof(bad_names[0]); ++i) {
        ASSERT_TRUE(!file_import_sqlite(fixture.source, fixture.db, bad_names[i], 4));
        ASSERT_EQ_INT(file_last_error(fixture.source), EINVAL);
        ASSERT_TRUE(!file_export_sqlite(fixture.db, bad_names[i], fixture.destination, 4, false, false));
        ASSERT_EQ_INT(file_last_error(fixture.destination), EINVAL);
    }
    ASSERT_TRUE(file_open_read(fixture.source));
    ASSERT_TRUE(!file_import_sqlite(fixture.source, fixture.db, fixture.name, 4));
    ASSERT_EQ_INT(file_last_error(fixture.source), EBUSY);
    ASSERT_TRUE(file_is_open(fixture.source));
    ASSERT_TRUE(file_close(fixture.source));
    ASSERT_TRUE(file_create(fixture.destination));
    ASSERT_TRUE(!file_export_sqlite(fixture.db, fixture.name, fixture.destination, 4, true, false));
    ASSERT_EQ_INT(file_last_error(fixture.destination), EBUSY);
    ASSERT_TRUE(file_is_open(fixture.destination));
    ASSERT_TRUE(file_close(fixture.destination));
    ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, 4, true, false));
    ASSERT_TRUE(store_bytes_equal(fixture.destination, "safe", 4));
    string_free(nul);
    string_free(empty);
    ASSERT_TRUE(file_delete(alias));
    file_free(alias);
    file_free(database);
    store_fixture_close(&fixture);
}

void test_file_sqlite_encrypted_payload_round_trip(void)
{
    store_fixture_t fixture;
    ASSERT_TRUE(store_fixture_open(&fixture));
    file_key_t *key = file_key_generate();
    file_t *encrypted = file_new_cstr(test_case_temp_path("encrypted.bin"));
    file_t *decrypted = file_new_cstr(test_case_temp_path("decrypted.bin"));
    unsigned char *bytes = store_pattern(131089, 71);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(encrypted);
    ASSERT_NOT_NULL(decrypted);
    ASSERT_NOT_NULL(bytes);
    ASSERT_TRUE(file_write_all_bytes(fixture.source, bytes, 131089));
    for (unsigned compress = 0; compress < 2; ++compress) {
        ASSERT_TRUE(file_encrypt(fixture.source, encrypted, key, compress != 0, compress != 0));
        ASSERT_TRUE(file_import_sqlite(encrypted, fixture.db, fixture.name, UINT64_MAX));
        ASSERT_TRUE(file_export_sqlite(fixture.db, fixture.name, fixture.destination, UINT64_MAX, compress != 0, false));
        array_t *ciphertext = file_read_all_bytes(encrypted);
        ASSERT_NOT_NULL(ciphertext);
        ASSERT_TRUE(store_bytes_equal(fixture.destination, array_get(ciphertext, 0), array_size(ciphertext)));
        array_destroy(ciphertext);
        ASSERT_TRUE(file_decrypt(fixture.destination, decrypted, key, 131089, compress != 0));
        ASSERT_TRUE(store_bytes_equal(decrypted, bytes, 131089));
    }
    ASSERT_TRUE(store_no_staging_files());
    free(bytes);
    file_free(decrypted);
    file_free(encrypted);
    file_key_free(key);
    store_fixture_close(&fixture);
}

/* README example for docs/file.md: register after ordinary tests in readme_examples.
 * The body supplies the corresponding documentation code, with ASSERT calls checking API results.
 * Expected output:
 * restored: sky
 */
void example_file_sqlite_round_trip(void)
{
    string_t *path = string_new_with(test_case_temp_path("example-store.db"));
    string_t *key = string_new_with("example database key");
    string_t *name = string_new_with("notes");
    file_t *source = file_new_cstr(test_case_temp_path("notes.txt"));
    file_t *destination = file_new_cstr(test_case_temp_path("restored-notes.txt"));
    ASSERT_NOT_NULL(path);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(name);
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(destination);
    sqlite_t *db = sqlite_open_encrypted(path, key);
    ASSERT_NOT_NULL(db);
    ASSERT_TRUE(file_write_all_bytes(source, "sky\n", 4));
    ASSERT_TRUE(file_import_sqlite(source, db, name, 1024));
    ASSERT_TRUE(file_export_sqlite(db, name, destination, 1024, false, false));
    string_t *restored = file_read_all_text(destination);
    ASSERT_NOT_NULL(restored);
    TEST_ASSERT_STR_EQ(string_c_str(restored), "sky\n");
    printf("restored: %s", string_c_str(restored));
    string_free(restored);
    sqlite_close(db);
    file_free(destination);
    file_free(source);
    string_free(name);
    string_free(key);
    string_free(path);
}
