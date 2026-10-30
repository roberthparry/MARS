/**
 * @file test_lab_support.c
 * @brief JSON assertions and private filesystem fixtures for native Lab tests.
 *
 * Isolates environment changes in a forked child so tests cannot read or overwrite
 * the user's worksheets. The parent reaps each fixture and removes its private
 * tree through the file module, without following symbolic links. Traversal is
 * bounded by depth and entry count. No cases are run concurrently.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
/* Keep the POSIX alternate signal-stack typedef distinct from the MARS stack API. */
#define stack_t posix_signal_stack_t
#include <sys/wait.h>
#undef stack_t
#include <unistd.h>

#include "file-t.h"
#include "array-t.h"
#include "sqlite-t.h"
#include "string-t.h"

#include "test_lab_support.h"

/* Independently seed image rows so HTTP tests exercise runtime reads without depending on installer objects. */
bool test_lab_texture_database(const char *directory)
{
    string_t *path = string_sprintf("%s/textures.db", directory);
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    bool ok = db && sqlite_exec_cstr(db,
        "begin;create table globe_texture_dataset(id,format_version);insert into globe_texture_dataset values(1,2);"
        "create table globe_texture(level,column_index,row_index,width,height,byte_count,sha256,png,"
        "primary key(level,column_index,row_index));");
    sqlite_stmt_t *q = ok ? sqlite_stmt_prepare(db, "insert into globe_texture values(?1,?2,?3,?4,?5,?6,?7,?8)") : NULL;
    static const unsigned widths[] = {2048, 8192, 1800, 1800}, heights[] = {1024, 4096, 1800, 1800};
    ok = ok && q;
    for (unsigned level = 0; ok && level < 4; ++level) {
        for (unsigned row = 0; ok && row < (level == 2 ? 6u : 1u); ++row) {
            for (unsigned column = 0; ok && column < (level == 3 ? 2u : level == 2 ? 12u : 1u); ++column) {
                unsigned width = level == 3 && column == 1 ? 3600 : widths[level];
                unsigned height = level == 3 ? width : heights[level];
                string_t *name = level == 3 ? string_sprintf("packaging/globe-db/textures/regional/%02u.png", column) :
                                level == 2 ? string_sprintf("packaging/globe-db/textures/earth_tiles/%02u-%02u.png", column, row)
                                : string_sprintf("packaging/globe-db/textures/earth_surface%s.png", level ? "_detail" : "");
                file_t *file = name ? file_new(name) : NULL;
                array_t *bytes = file ? file_read_all_bytes(file) : NULL;
                string_t *hash = bytes ? file_sha256(file) : NULL;
                ok = bytes && hash && sqlite_stmt_bind_int(q, 1, level) && sqlite_stmt_bind_int(q, 2, column) &&
                     sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_bind_int(q, 4, width) &&
                     sqlite_stmt_bind_int(q, 5, height) && sqlite_stmt_bind_int64(q, 6, array_size(bytes)) &&
                     sqlite_stmt_bind_text(q, 7, string_c_str(hash)) &&
                     sqlite_stmt_bind_blob(q, 8, array_get(bytes, 0), array_size(bytes)) && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
                sqlite_stmt_reset(q);
                string_free(hash); array_destroy(bytes); file_free(file); string_free(name);
            }
        }
    }
    sqlite_stmt_finalize(q);
    ok = ok && sqlite_exec_cstr(db,
        "create table globe_texture_region(slot integer primary key,west,south,east,north);"
        "insert into globe_texture_region values(0,-3.85,52.166666667,-3.60,52.333333333)");
    ok = ok && sqlite_exec_cstr(db, "commit") && !setenv("MARS_GLOBE_DB_PATH", string_c_str(path), 1) &&
         !setenv("MARS_GLOBE_DB_KEY", "", 1);
    sqlite_close(db); string_free(path);
    return ok;
}

/* Parse a fixture literal using the public JSON API. */
json_t *test_lab_json(const char *text)
{
    string_t *source = string_new_with(text);
    json_t *result = source ? json_from_text(source) : NULL;
    string_free(source);
    return result;
}

/* Borrow a keyed value without retaining a temporary key. */
const json_t *test_lab_member(const json_t *object, const char *key)
{
    string_t *name = string_new_with(key);
    const json_t *result = name ? json_object_get(object, name) : NULL;
    string_free(name);
    return result;
}

/* Borrow response text for immediate assertions. */
const char *test_lab_text(const json_t *object, const char *key)
{
    const string_t *value = json_string_value(test_lab_member(object, key));
    return value ? string_c_str(value) : "";
}

/* Require a boolean response flag, rather than treating missing values as false. */
bool test_lab_ok(const json_t *object, bool expected)
{
    bool value = !expected;
    return json_bool_value(test_lab_member(object, "ok"), &value) && value == expected;
}

/* Used only beneath this fixture's private directory, after its child has exited. */
static bool remove_fixture_tree(file_t *directory, unsigned depth, size_t *remaining)
{
    if (depth > 8 || !file_open_directory(directory))
        return false;
    bool ok = true;
    for (;;) {
        file_info_t *entry = NULL;
        if (!file_read_directory(directory, &entry)) {
            string_fprintf(stderr, "Fixture directory read failed: %s, errno=%d\n", file_path(directory), errno);
            ok = false;
            break;
        }
        if (!entry)
            break;
        if (!*remaining) {
            file_info_free(entry);
            ok = false;
            break;
        }
        --*remaining;
        string_t *path = string_sprintf("%s/%s", file_path(directory), file_info_name(entry));
        file_t *child = path ? file_new(path) : NULL;
        bool removed =
            child && (file_info_type(entry) == FILE_TYPE_DIRECTORY ? remove_fixture_tree(child, depth + 1, remaining)
                                                                   : file_delete(child));
        if (!removed)
            string_fprintf(stderr, "Fixture entry removal failed: %s, errno=%d\n", string_c_str(path), errno);
        ok = removed && ok;
        file_free(child);
        string_free(path);
        file_info_free(entry);
    }
    bool closed = file_close(directory);
    return ok && closed && file_remove_directory(directory);
}

/* Install only the small private SQL fixture; never copy or alter the user's database. */
bool test_lab_catalogue_database(const char *directory)
{
    string_t *path = string_sprintf("%s/jurisdiction.db", directory);
    string_t *key = string_new_with("lab-catalogue-fixture-key");
    file_t *source = file_new_cstr("tools/mars_lab/tests/fixtures/jurisdiction.sql");
    string_t *sql = source ? file_read_all_text(source) : NULL;
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    bool ok = db && sql && sqlite_exec(db, sql);
    sqlite_close(db);
    if (ok)
        ok = !setenv("MARS_JURISDICTION_DB_PATH", string_c_str(path), 1) &&
             !setenv("MARS_JURISDICTION_DB_KEY", string_c_str(key), 1);
    file_free(source);
    string_free(sql);
    string_free(key);
    string_free(path);
    return ok;
}

/* Add real packaged artwork only to a caller-owned, disposable catalogue. */
bool test_lab_offshore_database(void)
{
    string_t *path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    file_t *metadata = file_new_cstr("packaging/jurisdiction-db/flag_emblems.sql");
    file_t *artwork = file_new_cstr("packaging/jurisdiction-db/flags/zz-sea.svg");
    string_t *sql = metadata ? file_read_all_text(metadata) : NULL;
    string_t *svg = artwork ? file_read_all_text(artwork) : NULL;
    bool ok = db && sql && svg && sqlite_exec(db, sql) && sqlite_exec_cstr(db,
        "create table if not exists jurisdiction_country_flag(iso_country_code text primary key,svg text not null)");
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db,
        "insert or replace into jurisdiction_country_flag values('ZZ-SEA',?1)") : NULL;
    ok = ok && insert && sqlite_stmt_bind_text(insert, 1, string_c_str(svg)) &&
         sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(insert); sqlite_close(db); string_free(path); string_free(key);
    file_free(metadata); file_free(artwork); string_free(sql); string_free(svg);
    return ok;
}

/* Isolate state and environment changes in a disposable, sequential child fixture. */
bool test_lab_isolated(bool (*callback)(const char *directory))
{
    const char *temporary = getenv("TMPDIR");
    string_t *parent = string_new_with(temporary && *temporary ? temporary : "/tmp");
    file_t *root = parent ? file_create_temp_directory(parent) : NULL;
    string_free(parent);
    if (!root)
        return false;
    const char *directory = file_path(root);
    pid_t child = fork();
    if (!child) {
        char *working_directory = getcwd(NULL, 0);
        string_t *state = string_sprintf("%s/state.json", directory);
        string_t *asset = working_directory ? string_sprintf("%s/tools/mars_lab/assets/index.html", working_directory) : NULL;
        bool ready = state && asset && !setenv("MARS_HOME", directory, 1) &&
                     !setenv("MARS_LAB_STATE_FILE", string_c_str(state), 1) &&
                     !setenv("MARS_LAB_ASSET_FILE", string_c_str(asset), 1) &&
                     !setenv("MARS_LAB_APP_NAME", "MARS Lab", 1) && !unsetenv("MARS_LAB_SUBTITLE") &&
                     !unsetenv("MARS_LAB_CONTROL_QUERY_PARAM");
        string_free(state);
        string_free(asset);
        free(working_directory);
        bool passed = ready && callback(directory);
        /* Release this process's inherited handle; only the parent removes the directory. */
        file_free(root);
        _exit(passed ? 0 : 1);
    }
    int status = 0;
    pid_t waited = -1;
    if (child > 0) {
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    bool ok = waited == child && child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!ok)
        string_fprintf(stderr, "Lab isolated fixture: child=%ld, waited=%ld, wait status=%d\n", (long)child,
                       (long)waited, status);
    /* Browser profiles and cached plot assets can exceed a thousand entries during integration tests. */
    size_t remaining = 4096;
    bool removed = root && remove_fixture_tree(root, 0, &remaining);
    if (!removed)
        string_fprintf(stderr, "Lab private fixture cleanup failed: %s, errno=%d\n", directory, errno);
    file_free(root);
    return removed && ok;
}
