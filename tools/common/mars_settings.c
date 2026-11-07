/**
 * @file mars_settings.c
 * @brief JSON bootstrap and private SQLite application-setting persistence.
 *
 * Keeps the locator small and the main settings independent of all three content databases. No settings
 * file is sourced as shell code. Configuration files and database files are private to the current user;
 * diagnostic listings redact credentials. Explicit environment values take precedence over stored values.
 * Bulk installers delegate reads and automatic updates to an isolated settings process, retaining SQLCipher
 * memory protection for configuration without applying its global allocator policy to content databases.
 */
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sodium.h>

#include "file-t.h"
#include "json-t.h"
#include "sqlite-t.h"
#include "mars_settings.h"
#include "mars_keyring.h"
#define MARS_SETTINGS_INTERNAL_ACCESS
#include "mars_settings_internal.h"

static string_t *(*settings_key_source)(bool) = mars_keyring_password;
static string_t *settings_key;
static json_t *settings_resources;
static string_t *settings_resource_database;
static int64_t settings_resource_seconds, settings_resource_nanoseconds;
static pthread_mutex_t settings_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t settings_key_lock = PTHREAD_MUTEX_INITIALIZER;

/* Dependency injection also permits native regressions without accessing a real user's keyring. */
void mars_settings_key_provider(string_t *(*provider)(bool create))
{
    pthread_mutex_lock(&settings_lock);
    pthread_mutex_lock(&settings_key_lock);
    if (settings_key) sodium_memzero((void *)string_c_str(settings_key), string_byte_length(settings_key));
    string_free(settings_key); settings_key = NULL;
    json_free(settings_resources); settings_resources = NULL;
    string_free(settings_resource_database); settings_resource_database = NULL;
    settings_key_source = provider ? provider : mars_keyring_password;
    pthread_mutex_unlock(&settings_key_lock);
    pthread_mutex_unlock(&settings_lock);
}

static string_t *settings_home(void)
{
    const char *home = getenv("MARS_HOME");
    if (home && *home) return string_new_with(home);
    home = getenv("HOME");
    return home && *home ? string_sprintf("%s/.mars", home) : NULL;
}

static string_t *settings_parent(const string_t *path)
{
    const char *text = path ? string_c_str(path) : "", *slash = strrchr(text, '/');
    return slash ? string_sprintf("%.*s", (int)(slash == text ? 1 : slash - text), text) : string_new_with(".");
}

static string_t *settings_locator(void)
{
    const char *configured = getenv("MARS_CONFIG_FILE");
    string_t *home = settings_home();
    string_t *path = configured && *configured ? string_new_with(configured) :
                     home ? string_sprintf("%s/config/mars.json", string_c_str(home)) : NULL;
    string_free(home);
    return path;
}

/* Resolve the single bootstrap value without depending on the process's working directory. */
string_t *mars_settings_database(void)
{
    string_t *locator = settings_locator();
    file_t *file = locator ? file_new(locator) : NULL;
    bool exists = file && file_exists(file);
    string_t *result = NULL;
    if (exists) {
        file_info_t *info = file_get_info(file);
        string_t *text = info && file_info_size(info) <= 65536 ? file_read_all_text(file) : NULL;
        json_t *json = text ? json_from_text(text) : NULL;
        string_t *key = string_new_with("database");
        const string_t *value = key ? json_string_value(json_object_get(json, key)) : NULL;
        if (value && string_byte_length(value) && strlen(string_c_str(value)) == string_byte_length(value)) {
            string_t *parent = settings_parent(locator);
            result = string_starts_with(value, "/") ? string_clone(value) :
                     string_sprintf("%s/%s", string_c_str(parent), string_c_str(value));
            string_free(parent);
        }
        string_free(key); json_free(json); string_free(text); file_info_free(info);
    } else if (!getenv("MARS_CONFIG_FILE")) {
        string_t *home = settings_home();
        result = home ? string_sprintf("%s/mars.db", string_c_str(home)) : NULL;
        string_free(home);
    }
    file_free(file); string_free(locator);
    return result;
}

/* Leave a user's locator untouched; only an absent default receives the portable relative filename. */
bool mars_settings_initialise(void)
{
    string_t *locator = settings_locator(), *parent = settings_parent(locator);
    file_t *file = locator ? file_new(locator) : NULL, *directory = parent ? file_new(parent) : NULL;
    bool ok = file && directory;
    if (ok && !file_exists(file)) {
        string_t *text = string_new_with("{\n  \"database\": \"../mars.db\"\n}\n");
        mode_t previous = umask(0077);
        ok = text && file_create_directory(directory, 0700, true) &&
             file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
        umask(previous);
        ok = ok && file_write_text(file, text) && file_sync(file, false) && file_close(file);
        string_free(text);
    }
    file_free(directory); file_free(file); string_free(parent); string_free(locator);
    return ok;
}

static bool settings_name(const char *name)
{
    if (!name || strlen(name) > 128 || (strncmp(name, "MARS_", 5) && strcmp(name, "TZDIR")))
        return false;
    if (!strcmp(name, "MARS_HOME") || !strcmp(name, "MARS_CONFIG_FILE") || !strcmp(name, "MARS_CONFIG_DB")) return false;
    for (const char *p = name; *p; ++p)
        if ((*p < 'A' || *p > 'Z') && (*p < '0' || *p > '9') && *p != '_') return false;
    return true;
}

static string_t *settings_text(const char *value)
{
    string_t *text = value ? string_new() : NULL;
    if (text && string_append_utf8_exact(text, value, strlen(value))) {
        string_free(text); text = NULL;
    }
    return text;
}

static string_t *settings_value(const char *name, const char *value)
{
    if (!value) return NULL;
    size_t length = strlen(name);
    bool path = (length > 5 && !strcmp(name + length - 5, "_PATH")) ||
                (length > 5 && !strcmp(name + length - 5, "_FILE")) ||
                (length > 4 && !strcmp(name + length - 4, "_DIR")) ||
                (length > 7 && !strcmp(name + length - 7, "_BINARY")) || !strcmp(name, "MARS_CALENDAR_LIBRARY") ||
                !strcmp(name, "MARS_LAB_EXECUTABLE") || !strcmp(name, "TZDIR");
    if (!path || !*value || *value == '/') return settings_text(value);
    if (!strncmp(value, "~/", 2)) {
        const char *home = getenv("HOME");
        return home && *home ? string_sprintf("%s/%s", home, value + 2) : NULL;
    }
    string_t *database = mars_settings_database(), *parent = settings_parent(database);
    string_t *resolved = database && parent ? string_sprintf("%s/%s", string_c_str(parent), value) : NULL;
    string_free(database); string_free(parent);
    return resolved;
}

/* Resource and application settings share the same encrypted configuration database. */
sqlite_t *mars_settings_open(bool create)
{
    if (mars_settings_process_enabled()) {
        errno = EPERM;
        return NULL;
    }

    string_t *path = mars_settings_database(), *parent = settings_parent(path);
    file_t *file = path ? file_new(path) : NULL, *directory = parent ? file_new(parent) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    bool exists = info != NULL;
    bool safe = file && (info ? file_info_type(info) == FILE_TYPE_REGULAR &&
                                 file_info_owner(info) == geteuid() && file_info_link_count(info) == 1 :
                                file_last_error(file) == ENOENT);
    file_info_free(info);
    bool ready = safe && (create ? directory && file_create_directory(directory, 0700, true) : exists);
    pthread_mutex_lock(&settings_key_lock);
    if (ready && !settings_key) {
        settings_key = settings_key_source(create && !file_exists(file));
        if (!settings_key) fputs("Cannot unlock MARS settings: the desktop key store is unavailable or locked.\n", stderr);
    }
    sqlite_t *db = ready && settings_key ? sqlite_open_encrypted(path, settings_key) : NULL;
    pthread_mutex_unlock(&settings_key_lock);
    if (create && db && (!file_chmod(file, 0600) || !sqlite_exec_cstr(db,
            "create table if not exists application_setting(name text primary key,value text not null);"
            "create table if not exists resource_path(resource text primary key,path text not null)"))) {
        sqlite_close(db); db = NULL;
    }
    file_free(file); file_free(directory); string_free(parent); string_free(path);
    return db;
}

/* Cache only copied values, never a SQLite handle across the Lab's prefork worker boundary. */
string_t *mars_settings_resource(const char *name)
{
    if (mars_settings_process_enabled()) {
        string_t *input = name ? string_new_with(name) : NULL, *output = NULL;
        bool ok = input && mars_settings_process_request(MARS_SETTINGS_RESOURCE, input, &output);
        string_free(input);
        return ok ? output : NULL;
    }

    string_t *database = mars_settings_database();
    file_t *file = database ? file_new(database) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    int64_t seconds = 0;
    long nanoseconds = 0;
    bool present = info && file_info_last_write_time(info, &seconds, &nanoseconds);
    string_t *result = NULL;
    pthread_mutex_lock(&settings_lock);
    bool fresh = present && settings_resources && settings_resource_database &&
                 !string_compare(database, settings_resource_database) && seconds == settings_resource_seconds &&
                 nanoseconds == settings_resource_nanoseconds;
    if (present && !fresh) {
        json_free(settings_resources); settings_resources = NULL;
        string_free(settings_resource_database); settings_resource_database = string_clone(database);
        sqlite_t *db = mars_settings_open(false);
        sqlite_stmt_t *q = db ? sqlite_stmt_prepare(db, "select resource,path from resource_path limit 129") : NULL;
        json_t *values = json_new_object();
        unsigned count = 0;
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        bool ok = q && values;
        while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
            const char *key = sqlite_stmt_column_text(q, 0), *value = sqlite_stmt_column_text(q, 1);
            bool valid = key && value && strlen(key) <= 128 && strlen(value) <= 65536 &&
                         strlen(key) == sqlite_stmt_column_bytes(q, 0) &&
                         strlen(value) == sqlite_stmt_column_bytes(q, 1);
            string_t *k = valid ? string_new_with(key) : NULL, *v = valid ? string_new_with(value) : NULL;
            json_t *entry = v ? json_new_string(v) : NULL;
            ok = ++count <= 128 && k && entry && json_object_set(values, k, entry);
            json_free(entry); string_free(v); string_free(k);
        }
        if (ok && step == SQLITE_STEP_DONE) {
            settings_resources = values; values = NULL;
            settings_resource_seconds = seconds; settings_resource_nanoseconds = nanoseconds;
        }
        json_free(values); sqlite_stmt_finalize(q); sqlite_close(db);
    }
    string_t *key = string_new_with(name);
    const string_t *value = present && key ? json_string_value(json_object_get(settings_resources, key)) : NULL;
    if (value && string_byte_length(value)) {
        string_t *parent = settings_parent(database);
        result = string_starts_with(value, "/") ? string_clone(value) :
                 string_sprintf("%s/%s", string_c_str(parent), string_c_str(value));
        string_free(parent);
    }
    string_free(key);
    pthread_mutex_unlock(&settings_lock);
    file_info_free(info); file_free(file); string_free(database);
    return result;
}

/* Snapshot trees are private copies; erase transferred credential values before releasing them. */
static void settings_snapshot_free(json_t *values)
{
    for (size_t i = 0; i < json_object_size(values); ++i) {
        const string_t *value = json_string_value(json_object_value_at(values, i));
        if (value) sodium_memzero((void *)string_c_str(value), string_byte_length(value));
    }
    json_free(values);
}

/* Copy literal settings while encrypted database allocations remain in the settings process. */
string_t *mars_settings_snapshot(void)
{
    string_t *path = mars_settings_database();
    if (!path) return NULL;
    file_t *file = file_new(path);
    bool exists = file && file_exists(file);
    file_free(file); string_free(path);
    if (!exists) {
        string_t *locator = settings_locator();
        file_t *bootstrap = locator ? file_new(locator) : NULL;
        bool configured = bootstrap && file_exists(bootstrap);
        file_free(bootstrap); string_free(locator);
        return configured ? NULL : string_new_with("{}");
    }
    sqlite_t *db = mars_settings_open(false);
    sqlite_stmt_t *table = db ? sqlite_stmt_prepare(db,
        "select count(*) from sqlite_master where type='table' and name='application_setting'") : NULL;
    bool ok = table && sqlite_stmt_step(table) == SQLITE_STEP_ROW;
    bool present = ok && sqlite_stmt_column_int(table, 0);
    sqlite_stmt_finalize(table);
    sqlite_stmt_t *q = present ? sqlite_stmt_prepare(db, "select name,value from application_setting limit 1025") : NULL;
    json_t *values = json_new_object();
    unsigned count = 0;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    ok = ok && values && (!present || q);
    while (ok && q && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
        const char *name = sqlite_stmt_column_text(q, 0), *value = sqlite_stmt_column_text(q, 1);
        ok = ++count <= 1024 && settings_name(name) && value && strlen(value) <= 65536 &&
             strlen(name) == sqlite_stmt_column_bytes(q, 0) && strlen(value) == sqlite_stmt_column_bytes(q, 1);
        if (ok) {
            string_t *key = string_new_with(name), *resolved = settings_text(value);
            json_t *entry = resolved ? json_new_string(resolved) : NULL;
            ok = key && entry && json_object_set(values, key, entry);
            if (resolved) sodium_memzero((void *)string_c_str(resolved), string_byte_length(resolved));
            string_free(resolved); string_free(key);
            const string_t *copy = json_string_value(entry);
            if (copy) sodium_memzero((void *)string_c_str(copy), string_byte_length(copy));
            json_free(entry);
        }
    }
    ok = ok && step == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q); sqlite_close(db);
    string_t *snapshot = ok ? json_to_string(values) : NULL;
    settings_snapshot_free(values);
    return snapshot;
}

/* Read a setting from the same bounded snapshot used during application startup. */
string_t *mars_settings_read(const char *name)
{
    if (!settings_name(name)) return NULL;
    string_t *snapshot = NULL;
    bool ok = mars_settings_process_enabled()
                  ? mars_settings_process_request(MARS_SETTINGS_LOAD, NULL, &snapshot)
                  : (snapshot = mars_settings_snapshot()) != NULL;
    json_t *values = ok && snapshot ? json_from_text(snapshot) : NULL;
    ok = values && json_type(values) == JSON_OBJECT && json_object_size(values) <= 1024;
    string_t *key = ok ? string_new_with(name) : NULL;
    const string_t *value = values && key ? json_string_value(json_object_get(values, key)) : NULL;
    string_t *result = values && key ? value ? settings_value(name, string_c_str(value)) : string_new() : NULL;
    settings_snapshot_free(values);
    if (snapshot) sodium_memzero((void *)string_c_str(snapshot), string_byte_length(snapshot));
    string_free(snapshot); string_free(key);
    return result;
}

/* Populate missing variables; the configuration database key never enters the reply. */
bool mars_settings_load(void)
{
    string_t *snapshot = NULL;
    bool ok = mars_settings_process_enabled()
                  ? mars_settings_process_request(MARS_SETTINGS_LOAD, NULL, &snapshot)
                  : (snapshot = mars_settings_snapshot()) != NULL;
    json_t *values = ok && snapshot ? json_from_text(snapshot) : NULL;
    ok = ok && values && json_type(values) == JSON_OBJECT && json_object_size(values) <= 1024;
    for (size_t i = 0; ok && i < json_object_size(values); ++i) {
        const string_t *name = json_object_key_at(values, i);
        const string_t *value = json_string_value(json_object_value_at(values, i));
        ok = name && value && settings_name(string_c_str(name)) &&
             strlen(string_c_str(name)) == string_byte_length(name) && string_byte_length(value) <= 65536 &&
             strlen(string_c_str(value)) == string_byte_length(value);
    }
    for (size_t i = 0; ok && i < json_object_size(values); ++i) {
        const char *name = string_c_str(json_object_key_at(values, i));
        const char *value = string_c_str(json_string_value(json_object_value_at(values, i)));
        if (!getenv(name)) {
            string_t *resolved = settings_value(name, value);
            ok = resolved && !setenv(name, string_c_str(resolved), 0);
            if (resolved) sodium_memzero((void *)string_c_str(resolved), string_byte_length(resolved));
            string_free(resolved);
        }
    }
    settings_snapshot_free(values);
    if (snapshot) sodium_memzero((void *)string_c_str(snapshot), string_byte_length(snapshot));
    string_free(snapshot);
    return ok;
}

static bool settings_store(sqlite_t *db, const char *name, const char *value, bool replace)
{
    if (!settings_name(name) || !value || strlen(value) > 65536) return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, replace ? "insert into application_setting values(?1,?2) "
                                                      "on conflict(name) do update set value=excluded.value "
                                                      "where value<>excluded.value" :
                                                      "insert or ignore into application_setting values(?1,?2)");
    bool ok = q && sqlite_stmt_bind_text(q, 1, name) && sqlite_stmt_bind_text(q, 2, value) &&
              sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

/* Preserve an existing cache key; serialise first creation inside the encrypted database. */
string_t *mars_settings_secret(const char *name)
{
    size_t length = name ? strlen(name) : 0;
    if (!settings_name(name) ||
        !((length > 4 && !strcmp(name + length - 4, "_KEY")) ||
          (length > 7 && !strcmp(name + length - 7, "_SECRET")))) return NULL;
    if (mars_settings_process_enabled()) {
        string_t *input = string_new_with(name), *output = NULL;
        bool ok = input && mars_settings_process_request(MARS_SETTINGS_SECRET, input, &output);
        string_free(input);
        return ok ? output : NULL;
    }
    sqlite_t *db = mars_settings_initialise() ? mars_settings_open(true) : NULL;
    bool transaction = db && sqlite_exec_cstr(db, "pragma busy_timeout=30000;begin immediate");
    sqlite_stmt_t *q = transaction ? sqlite_stmt_prepare(db,
        "select value from application_setting where name=?1") : NULL;
    bool ok = q && sqlite_stmt_bind_text(q, 1, name);
    sqlite_step_result_t step = ok ? sqlite_stmt_step(q) : SQLITE_STEP_ERROR;
    string_t *secret = step == SQLITE_STEP_ROW ? settings_text(sqlite_stmt_column_text(q, 0)) :
                       step == SQLITE_STEP_DONE ? string_new() : NULL;
    ok = secret && string_byte_length(secret) <= 65536 &&
         (step != SQLITE_STEP_ROW || string_byte_length(secret) == sqlite_stmt_column_bytes(q, 0));
    sqlite_stmt_finalize(q);
    if (ok && !string_byte_length(secret)) {
        unsigned char random[48];
        char encoded[sodium_base64_ENCODED_LEN(48u, sodium_base64_VARIANT_URLSAFE_NO_PADDING)];
        ok = sodium_init() >= 0;
        if (ok) {
            randombytes_buf(random, sizeof random);
            sodium_bin2base64(encoded, sizeof encoded, random, sizeof random, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
            string_free(secret);
            secret = string_new_with(encoded);
            ok = secret && settings_store(db, name, encoded, true);
        }
        sodium_memzero(random, sizeof random); sodium_memzero(encoded, sizeof encoded);
    }
    if (ok) ok = sqlite_exec_cstr(db, "commit");
    if (!ok && transaction) sqlite_exec_cstr(db, "rollback");
    sqlite_close(db);
    if (!ok) {
        if (secret) sodium_memzero((void *)string_c_str(secret), string_byte_length(secret));
        string_free(secret); secret = NULL;
    }
    return secret;
}

/* Store a bounded JSON object atomically; neither shell syntax nor configuration sidecars are involved. */
bool mars_settings_import(const string_t *text, bool replace)
{
    if (!text || string_byte_length(text) > 1048576) return false;
    json_t *values = json_from_text(text);
    bool valid = values && json_type(values) == JSON_OBJECT && json_object_size(values) <= 1024;
    sqlite_t *db = valid && mars_settings_initialise() ? mars_settings_open(true) : NULL;
    bool transaction = db && sqlite_exec_cstr(db, "pragma busy_timeout=30000;begin immediate");
    bool ok = transaction;
    for (size_t i = 0; ok && i < json_object_size(values); ++i) {
        const string_t *name = json_object_key_at(values, i);
        const string_t *value = json_string_value(json_object_value_at(values, i));
        ok = name && value && strlen(string_c_str(name)) == string_byte_length(name) &&
             strlen(string_c_str(value)) == string_byte_length(value) &&
             settings_store(db, string_c_str(name), string_c_str(value), replace);
    }
    if (ok) ok = sqlite_exec_cstr(db, "commit");
    if (!ok && transaction) sqlite_exec_cstr(db, "rollback");
    settings_snapshot_free(values); sqlite_close(db);
    return ok;
}

/* The encrypted configuration is the sole persistent destination for automatic setting updates. */
bool mars_settings_sync(const string_t *text)
{
    if (mars_settings_process_enabled()) {
        string_t *output = NULL;
        bool ok = mars_settings_process_request(MARS_SETTINGS_SYNC, text, &output);
        string_free(output);
        return ok;
    }

    return mars_settings_import(text, true);
}

/* Edit persistent settings without reinstalling; credentials are never printed by the listing. */
int mars_settings_command(int argc, char **argv)
{
    bool list = !argc || (argc == 1 && !strcmp(argv[0], "list"));
    bool set = argc == 3 && !strcmp(argv[0], "set") && settings_name(argv[1]);
    bool unset = argc == 2 && !strcmp(argv[0], "unset") && settings_name(argv[1]);
    bool import = argc == 2 && !strcmp(argv[0], "import");
    bool initialise = argc == 1 && !strcmp(argv[0], "init");
    if (!list && !set && !unset && !import && !initialise) return 2;
    if (!list && !mars_settings_initialise()) return 1;
    if (initialise) {
        sqlite_t *db = mars_settings_open(true);
        bool ok = db && settings_store(db, "MARS_LAB_HISTORY_FILE", "lab/mars_lab_history.db", false) &&
                  settings_store(db, "MARS_LAB_OBJECT_STORE_PATH", "lab/mars_lab_object_store.sqlite3", false);
        sqlite_close(db);
        return ok ? 0 : 1;
    }
    if (import) {
        file_t *file = file_new_cstr(argv[1]);
        file_info_t *info = file ? file_get_info(file) : NULL;
        string_t *text = info && file_info_size(info) <= 1048576 ? file_read_all_text(file) : NULL;
        bool ok = text && mars_settings_import(text, false);
        string_free(text); file_info_free(info); file_free(file);
        return ok ? 0 : 1;
    }
    sqlite_t *db = mars_settings_open(!list);
    bool ok = db != NULL;
    if (set) ok = ok && settings_store(db, argv[1], argv[2], true);
    sqlite_stmt_t *q = db && list ? sqlite_stmt_prepare(db, "select name,value from application_setting order by name") :
                      db && unset ? sqlite_stmt_prepare(db, "delete from application_setting where name=?1") : NULL;
    if (list) {
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        while (q && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
            const char *name = sqlite_stmt_column_text(q, 0), *value = sqlite_stmt_column_text(q, 1);
            bool secret = strstr(name, "KEY") || strstr(name, "TOKEN") || strstr(name, "PASSWORD") || strstr(name, "SECRET");
            printf("%s = %s\n", name, secret ? "[redacted]" : value);
        }
        ok = q && step == SQLITE_STEP_DONE;
    }
    if (unset) ok = q && sqlite_stmt_bind_text(q, 1, argv[1]) && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q); sqlite_close(db);
    return ok ? 0 : 1;
}
