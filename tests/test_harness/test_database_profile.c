/**
 * @file test_database_profile.c
 * @brief Import reviewable test configuration and resolve saved selection profiles with dictionary indexes.
 *
 * Synchronisation merges identities without deleting tests; missing names remain unconfirmed for review.
 * Profiles are copied into memory and their SQLite connection is released before case execution. Each lookup
 * visits only its dotted ancestors; selected-descendant indexes allow nested group traversal without scanning
 * the catalogue. JSON remains the default selection source and can be snapshotted without modifying it.
 */
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "dictionary-t.h"
#include "json-t.h"
#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

/** @brief Owned immutable selection rules and ancestor traversal index. */
struct _test_profile_t {
    dictionary_t *rules;
    dictionary_t *descendants;
    bool default_enabled;
};

static bool test_database_profile_name(const char *name)
{
    return name && *name && strlen(name) <= 128u;
}

static const json_t *test_database_member(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

/* New profiles preserve existing user choices. */
bool test_database_profile_create(test_database_t *database, const char *name, bool default_enabled)
{
    if (!test_database_profile_name(name))
        return test_database_fail(database, "A profile name must contain between one and 128 bytes.");
    sqlite3_stmt *statement = test_database_prepare(database,
        "INSERT OR IGNORE INTO test_profile(name, default_enabled) VALUES (?1, ?2)");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, default_enabled);
    return test_database_write(database, statement);
}

/* Rules use bound values; source paths and test names never become SQL source. */
bool test_database_profile_set(test_database_t *database, const char *profile, const char *source,
                               const char *path, bool enabled)
{
    if (!test_database_profile_name(profile) || !source || !*source || !path || !*path)
        return test_database_fail(database, "A selection requires a profile, source and complete test/group path.");
    sqlite3_stmt *statement = test_database_prepare(database,
        "INSERT INTO test_selection(profile, source, path, enabled) VALUES (?1, ?2, ?3, ?4)\n"
        "ON CONFLICT(profile, source, path) DO UPDATE SET enabled = excluded.enabled");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, profile, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, enabled);
    return test_database_write(database, statement);
}

static bool test_database_import_group(test_database_t *database, const char *source, const json_t *group,
                                       const char *parent, const char *profile, unsigned depth)
{
    if (!group || json_type(group) != JSON_OBJECT || depth > 128u)
        return test_database_fail(database, "Invalid or excessively nested test configuration.");
    /* Import must visit each authored declaration once; lookups during execution remain indexed. */
    for (size_t i = 0u; i < json_object_size(group); ++i) {
        const string_t *key = json_object_key_at(group, i);
        const json_t *value = json_object_value_at(group, i);
        const char *name = key ? string_c_str(key) : NULL;
        if (name && !strcmp(name, "enabled"))
            continue;
        if (!name || strlen(name) != string_byte_length(key))
            return test_database_fail(database, "Test catalogue names cannot contain embedded NUL bytes.");
        bool nested = json_type(value) == JSON_OBJECT, enabled = true;
        const json_t *flag = nested ? test_database_member(value, "enabled") : value;
        if (flag && !json_bool_value(flag, &enabled))
            return test_database_fail(database, "Test selection entries must be booleans or groups.");
        string_t *path = *parent ? string_sprintf("%s.%s", parent, name) : string_new_with(name);
        test_database_result_t record = {.source = source, .path = path ? string_c_str(path) : NULL,
            .name = name, .parent = parent, .is_group = nested};
        bool ok = path && test_database_catalogue(database, &record) &&
                  (!profile || test_database_profile_set(database, profile, source, string_c_str(path), enabled)) &&
                  (!nested || test_database_import_group(database, source, value, string_c_str(path), profile, depth + 1u));
        string_free(path);
        if (!ok)
            return false;
    }
    return true;
}

/* A snapshot replaces one profile atomically; catalogue-only imports preserve existing profile selections. */
bool test_database_import(test_database_t *database, const string_t *root, const char *profile, bool review_missing)
{
    if (profile && !test_database_profile_name(profile))
        return test_database_fail(database, "Invalid profile name.");
    string_t *fingerprint = test_database_sources_fingerprint(root);
    string_t *path = string_sprintf("%s/tests/test_config.json", root ? string_c_str(root) : ".");
    json_t *configuration = path ? json_from_file(path) : NULL;
    string_free(path);
    bool ok = fingerprint && configuration && json_type(configuration) == JSON_OBJECT &&
              test_database_exec(database, "BEGIN IMMEDIATE") &&
              test_database_seed(database, root) &&
              test_database_exec(database, "CREATE TEMP TABLE IF NOT EXISTS catalogue_seen(\n"
                                            "    source TEXT, path TEXT, PRIMARY KEY(source, path)) WITHOUT ROWID;\n"
                                            "DELETE FROM catalogue_seen;");
    if (database)
        database->importing = ok;
    if (ok && profile) {
        sqlite3_stmt *statement = test_database_prepare(database, "DELETE FROM test_profile WHERE name = ?1");
        if (statement)
            sqlite3_bind_text(statement, 1, profile, -1, SQLITE_TRANSIENT);
        ok = test_database_write(database, statement) && test_database_profile_create(database, profile, true);
    }
    for (size_t i = 0u; ok && i < json_object_size(configuration); ++i) {
        const string_t *source = json_object_key_at(configuration, i);
        ok = source && strlen(string_c_str(source)) == string_byte_length(source) &&
             test_database_import_group(database, string_c_str(source), json_object_value_at(configuration, i),
                                         "", profile, 0u);
    }
    ok = ok && test_database_import_cases(database);
    if (ok && review_missing)
        ok = test_database_exec(database,
            "UPDATE test_catalogue SET confirmed = 0 WHERE NOT EXISTS (\n"
            "    SELECT 1 FROM catalogue_seen AS seen\n"
            "    WHERE seen.source = test_catalogue.source AND seen.path = test_catalogue.path\n"
            ");");
    if (database)
        database->importing = false;
    if (ok) {
        string_t *after = test_database_sources_fingerprint(root);
        ok = after && string_compare(fingerprint, after) == 0;
        string_free(after);
        if (!ok)
            test_database_fail(database, "Test sources changed during synchronisation; retry the operation.");
    }
    if (ok && review_missing) {
        sqlite3_stmt *statement = test_database_prepare(database,
            "INSERT INTO test_source_sync(id, source_sha256) VALUES (1, ?1)\n"
            "ON CONFLICT(id) DO UPDATE SET\n"
            "    source_sha256 = excluded.source_sha256,\n"
            "    synced_utc = excluded.synced_utc");
        if (statement)
            sqlite3_bind_text(statement, 1, string_c_str(fingerprint), -1, SQLITE_TRANSIENT);
        ok = test_database_write(database, statement);
    }
    if (ok)
        ok = test_database_exec(database, "COMMIT");
    else {
        if (database && database->connection)
            sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        if (!configuration)
            test_database_fail(database, "Cannot read the default JSON test configuration.");
        else if (!fingerprint)
            test_database_fail(database, "Cannot fingerprint the test catalogue and SQL sources.");
    }
    string_free(fingerprint);
    json_free(configuration);
    return ok;
}

static size_t test_profile_hash(const void *key)
{
    return (size_t)string_hash(*(string_t *const *)key);
}

static int test_profile_compare(const void *left, const void *right)
{
    return string_compare(*(string_t *const *)left, *(string_t *const *)right);
}

static void test_profile_destroy_key(void *key)
{
    string_free(*(string_t **)key);
}

static void test_profile_clone_key(void *destination, const void *source)
{
    *(string_t **)destination = string_clone(*(string_t *const *)source);
}

static dictionary_t *test_profile_dictionary(void)
{
    return dictionary_create(sizeof(string_t *), sizeof(bool), test_profile_hash, test_profile_compare, test_profile_clone_key,
                               test_profile_destroy_key, NULL, NULL, NULL);
}

static string_t *test_profile_key(const char *source, const char *path)
{
    return string_sprintf("%zu:%s%s", strlen(source), source, path);
}

static bool test_profile_store(dictionary_t *dictionary, const char *source, const char *path, bool value)
{
    string_t *key = test_profile_key(source, path);
    bool existing = false;
    bool ok = key && dictionary_set(dictionary, &key, &value) && dictionary_get(dictionary, &key, &existing);
    string_free(key);
    return ok;
}

static int test_profile_lookup(const dictionary_t *dictionary, const char *source, const char *path, bool *value)
{
    string_t *key = test_profile_key(source, path);
    int found = key ? dictionary_get(dictionary, &key, value) : -1;
    string_free(key);
    return found;
}

static bool test_profile_ancestors(test_profile_t *profile, const char *source, const char *path, bool storing)
{
    string_t *text = string_new_with(path), *prefix = string_new();
    size_t count = 0u;
    string_t **parts = text ? string_split(text, ".", &count) : NULL;
    bool ok = parts && prefix && count && count <= 128u, matched = false;
    for (size_t i = 0u; ok && i < count; ++i) {
        ok = (!i || !string_append_cstr(prefix, ".")) && !string_append_string(prefix, parts[i]);
        if (ok && storing && i + 1u < count)
            ok = test_profile_store(profile->descendants, source, string_c_str(prefix), true);
        if (ok && !storing) {
            bool enabled = false;
            int found = test_profile_lookup(profile->rules, source, string_c_str(prefix), &enabled);
            if (found < 0) {
                ok = false;
                break;
            }
            if (found) {
                if (!enabled) {
                    ok = false;
                    break;
                }
                matched = true;
            }
        }
    }
    if (ok && !storing && !matched) {
        bool descendant = false;
        int found = test_profile_lookup(profile->descendants, source, path, &descendant);
        ok = found >= 0;
        matched = profile->default_enabled || found > 0;
    }
    string_split_free(parts, count);
    string_free(prefix);
    string_free(text);
    return ok && (storing || matched);
}

/* Profiles never leave a prepared statement or connection attached to their snapshot. */
test_profile_t *test_database_profile_load(test_database_t *database, const char *name)
{
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT default_enabled FROM test_profile WHERE name = ?1");
    if (!statement)
        return NULL;
    sqlite3_bind_text(statement, 1, name ? name : "", -1, SQLITE_TRANSIENT);
    test_profile_t *profile = sqlite3_step(statement) == SQLITE_ROW ? calloc(1u, sizeof *profile) : NULL;
    if (profile)
        profile->default_enabled = sqlite3_column_int(statement, 0) != 0;
    sqlite3_finalize(statement);
    if (!profile) {
        test_database_fail(database, "The requested test profile does not exist or cannot be loaded.");
        return NULL;
    }
    profile->rules = test_profile_dictionary();
    profile->descendants = test_profile_dictionary();
    statement = test_database_prepare(database,
        "SELECT source, path, enabled FROM test_selection WHERE profile = ?1 ORDER BY source, path");
    bool ok = statement && profile->rules && profile->descendants;
    if (statement)
        sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
    int status = SQLITE_DONE;
    while (ok && (status = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *source = (const char *)sqlite3_column_text(statement, 0);
        const char *path = (const char *)sqlite3_column_text(statement, 1);
        bool enabled = sqlite3_column_int(statement, 2) != 0;
        ok = source && path && test_profile_store(profile->rules, source, path, enabled) &&
             (!enabled || test_profile_ancestors(profile, source, path, true));
    }
    ok = ok && status == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) {
        test_database_fail(database, "Cannot read or index the requested test profile.");
        test_profile_free(profile);
        return NULL;
    }
    return profile;
}

/* Release all copied keys and rules. */
void test_profile_free(test_profile_t *profile)
{
    if (!profile)
        return;
    dictionary_destroy(profile->rules);
    dictionary_destroy(profile->descendants);
    free(profile);
}

/* Ancestor depth, rather than catalogue size, bounds every indexed selection query. */
bool test_profile_enabled(const test_profile_t *profile, const char *source, const char *path)
{
    return profile && source && path && *path &&
           test_profile_ancestors((test_profile_t *)profile, source, path, false);
}
