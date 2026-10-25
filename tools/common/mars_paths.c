/**
 * @file mars_paths.c
 * @brief Indexed SQLite resource settings shared by native MARS applications.
 *
 * The resource_path table is independent of content databases and browsing history. The small mars.json
 * bootstrap file selects mars.db; MARS_CONFIG_FILE may select another bootstrap file.
 * Build defaults live in mars-config.db beside the executable. The executable path is obtained from
 * the operating system, not a compiled developer pathname. Explicit legacy source roots remain supported.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "sqlite-t.h"
#include "mars_paths.h"
#include "mars_settings.h"

static const struct { const char *key, *environment, *legacy; } resources[MARS_PATH_COUNT] = {
    [MARS_PATH_GLOBE]        = {"globe_sources",        "MARS_GLOBE_SOURCE_DIR",        "packaging/globe-db"},
    [MARS_PATH_TEXTURES]     = {"globe_textures",       "MARS_GLOBE_TEXTURE_DIR",       "packaging/globe-db/textures"},
    [MARS_PATH_JURISDICTION] = {"jurisdiction_sources", "MARS_JURISDICTION_SOURCE_DIR", "packaging/jurisdiction-db"},
    [MARS_PATH_ALMANAC]      = {"almanac_sources",      "MARS_ALMANAC_SOURCE_DIR",      "packaging/almanac-db"},
    [MARS_PATH_ASSETS]       = {"lab_assets",           "MARS_LAB_ASSET_DIR",           "tools/mars_lab/assets"},
    [MARS_PATH_SCRIPTS]      = {"lab_scripts",          "MARS_LAB_SCRIPT_DIR",          "tools/mars_lab/build/typescript"},
    [MARS_PATH_WASM]         = {"lab_wasm",             "MARS_LAB_WASM_DIR",            NULL},
    [MARS_PATH_ICONS]        = {"lab_icons",            "MARS_LAB_ICON_DIR",            "packaging/linux"},
    [MARS_PATH_WORKER]       = {"lab_worker",           "MARS_LAB_EXECUTABLE",          NULL},
    [MARS_PATH_LOCALES]      = {"locale_sources",       "MARS_LOCALE_SOURCE_DIR",       "tools/mars_config/data/locales"},
    [MARS_PATH_ZONEINFO]     = {"zoneinfo",             "TZDIR",                       NULL},
    [MARS_PATH_TEMPORARY]    = {"temporary",            "TMPDIR",                      NULL},
    [MARS_PATH_DOCUMENTATION] = {"system_documentation", "MARS_SYSTEM_DOC_DIR",         NULL},
    [MARS_PATH_PRIVATE_ASSETS] = {"private_assets", "MARS_PRIVATE_ASSET_DIR", "tools/to_be_announced_lab/assets"},
    [MARS_PATH_PRIVATE_SCRIPTS] = {"private_scripts", "MARS_PRIVATE_SCRIPT_DIR", "tools/to_be_announced_lab/build/typescript"},
    [MARS_PATH_PRIVATE_WASM] = {"private_wasm", "MARS_PRIVATE_WASM_DIR", NULL},
};

static string_t *path_join(const string_t *base, const char *name)
{
    if (!base || !string_byte_length(base)) return NULL;
    return name && *name ? string_sprintf("%s/%s", string_c_str(base), name) : string_clone(base);
}

static string_t *path_parent(const string_t *path)
{
    const char *text = path ? string_c_str(path) : "", *slash = strrchr(text, '/');
    return slash ? string_sprintf("%.*s", (int)(slash == text ? 1 : slash - text), text) : string_new_with(".");
}

static string_t *path_configuration(void)
{
    return mars_settings_database();
}

static string_t *path_defaults(void)
{
    file_t *self = file_new_cstr("/proc/self/exe");
    file_info_t *info = self ? file_get_info(self) : NULL;
    const char *target = info ? file_info_link_target(info) : NULL;
    string_t *executable = target ? string_new_with(target) : NULL;
    string_t *parent = executable ? path_parent(executable) : NULL;
    string_t *path = parent ? path_join(parent, "mars-config.db") : NULL;
    file_t *file = path ? file_new(path) : NULL;
    /* Native regression executables share the enclosing build's configuration. */
    if (file && !file_exists(file)) {
        string_t *above = path_parent(parent);
        string_free(path);
        path = path_join(above, "mars-config.db");
        string_free(above);
    }
    file_free(file); string_free(parent); string_free(executable); file_info_free(info); file_free(self);
    return path;
}

static string_t *path_read(const string_t *database, const char *key)
{
    file_t *file = database ? file_new(database) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    sqlite_t *db = info && file_info_type(info) == FILE_TYPE_REGULAR ? sqlite_open_plain(database) : NULL;
    sqlite_stmt_t *q = db ? sqlite_stmt_prepare(db, "select path from resource_path where resource=?1") : NULL;
    string_t *path = NULL;
    if (q && sqlite_stmt_bind_text(q, 1, key) && sqlite_stmt_step(q) == SQLITE_STEP_ROW) {
        const char *text = sqlite_stmt_column_text(q, 0);
        if (text && *text && strlen(text) == sqlite_stmt_column_bytes(q, 0)) {
            string_t *parent = path_parent(database);
            path = *text == '/' ? string_new_with(text) : path_join(parent, text);
            string_free(parent);
        }
    }
    sqlite_stmt_finalize(q); sqlite_close(db); file_info_free(info); file_free(file);
    return path;
}

/* Resolve explicit inputs before persistent overrides, then compatible development/build defaults. */
string_t *mars_path_resource(mars_path_kind_t kind, const string_t *source_root, const char *relative)
{
    if ((unsigned)kind >= MARS_PATH_COUNT) return NULL;
    const char *environment = getenv(resources[kind].environment);
    string_t *base = environment && *environment ? string_new_with(environment) : NULL;
    if (!base && source_root && string_byte_length(source_root) && resources[kind].legacy)
        base = path_join(source_root, resources[kind].legacy);
    if (!base) base = mars_settings_resource(resources[kind].key);
    const char *legacy = getenv("MARS_ROOT");
    if (!base && legacy && *legacy && resources[kind].legacy)
        base = string_sprintf("%s/%s", legacy, resources[kind].legacy);
    string_t *defaults = base ? NULL : path_defaults();
    if (!base) base = path_read(defaults, resources[kind].key);
    string_t *result = path_join(base, relative);
    string_free(defaults); string_free(base);
    return result;
}

/* Resolve once before forking; calculation children inherit paths without opening the desktop key store. */
bool mars_paths_load(void)
{
    if (!mars_settings_load()) return false;
    bool ok = true;
    for (unsigned i = 0; ok && i < MARS_PATH_COUNT; ++i) {
        if (getenv(resources[i].environment)) continue;
        string_t *value = mars_path_resource(i, NULL, "");
        if (value) ok = !setenv(resources[i].environment, string_c_str(value), 0);
        string_free(value);
    }
    return ok;
}

/* Logical seed names are independent of their configured physical locations. */
string_t *mars_path_source(const string_t *source_root, const char *name)
{
    if (!name) return NULL;
    if (*name == '/') return string_new_with(name);
    if (*name == '@') {
        const char *slash = strchr(name, '/');
        size_t length = slash ? (size_t)(slash - name - 1) : 0;
        /* Fixed application namespaces, selected only for enumerated server routes. */
        static const struct { const char *name; mars_path_kind_t kind; } aliases[] = {
            {"assets", MARS_PATH_ASSETS}, {"scripts", MARS_PATH_SCRIPTS}, {"wasm", MARS_PATH_WASM},
            {"icons", MARS_PATH_ICONS}, {"private_scripts", MARS_PATH_PRIVATE_SCRIPTS}
        };
        for (unsigned i = 0; slash && i < sizeof aliases / sizeof *aliases; ++i)
            if (strlen(aliases[i].name) == length && !strncmp(name + 1, aliases[i].name, length))
                return mars_path_resource(aliases[i].kind, NULL, slash + 1);
        return NULL;
    }
    /* Three fixed package namespaces; arbitrary SQL include paths retain explicit-root semantics. */
    const mars_path_kind_t kinds[] = {MARS_PATH_GLOBE, MARS_PATH_JURISDICTION, MARS_PATH_ALMANAC};
    for (unsigned i = 0; i < sizeof kinds / sizeof *kinds; ++i) {
        const char *prefix = resources[kinds[i]].legacy;
        size_t length = strlen(prefix);
        if (!strncmp(name, prefix, length) && name[length] == '/')
            return mars_path_resource(kinds[i], source_root, name + length + 1);
    }
    return path_join(source_root, name);
}

/* Validate the resolved file against its configured directory before libc sees an absolute TZ value. */
string_t *mars_path_zoneinfo(const string_t *name)
{
    if (!name || !string_byte_length(name) || string_starts_with(name, "/")) return NULL;
    string_t *base = mars_path_resource(MARS_PATH_ZONEINFO, NULL, "");
    string_t *path = path_join(base, string_c_str(name));
    file_t *directory = base ? file_new(base) : NULL, *file = path ? file_new(path) : NULL;
    file_t *root = directory ? file_resolve(directory) : NULL, *resolved = file ? file_resolve(file) : NULL;
    string_t *prefix = root ? string_sprintf("%s/", file_path(root)) : NULL;
    string_t *canonical = resolved ? string_new_with(file_path(resolved)) : NULL;
    unsigned char magic[4];
    size_t count = 0;
    bool valid = canonical && prefix && string_starts_with(canonical, string_c_str(prefix)) &&
                 file_open_read(resolved) && file_read(resolved, magic, sizeof magic, &count) && count == 4 &&
                 !memcmp(magic, "TZif", 4);
    if (!valid) { string_free(canonical); canonical = NULL; }
    string_free(prefix); file_free(root); file_free(resolved); file_free(directory); file_free(file);
    string_free(path); string_free(base);
    return canonical;
}

/* Use the configured location or the C library's temporary-directory default. */
file_t *mars_path_temporary(void)
{
    string_t *parent = mars_path_resource(MARS_PATH_TEMPORARY, NULL, "");
    if (!parent) parent = string_new_with(P_tmpdir);
    file_t *directory = parent ? file_create_temp_directory(parent) : NULL;
    string_free(parent);
    return directory;
}

static int path_kind(const char *key)
{
    /* Command parsing only: a bounded table of fixed, human-readable setting names. */
    for (unsigned i = 0; key && i < MARS_PATH_COUNT; ++i)
        if (!strcmp(key, resources[i].key)) return (int)i;
    return -1;
}

static sqlite_t *path_open_settings(const string_t *path, bool seed)
{
    string_t *parent = path_parent(path);
    file_t *directory = parent ? file_new(parent) : NULL;
    bool ready = path && directory && file_create_directory(directory, 0700, true);
    sqlite_t *db = ready ? (seed ? sqlite_open_plain(path) : mars_settings_open(true)) : NULL;
    bool ok = db && sqlite_exec_cstr(db,
        "create table if not exists resource_path(resource text primary key,path text not null)");
    file_t *file = path ? file_new(path) : NULL;
    ok = ok && file && file_chmod(file, 0600);
    file_free(file);
    if (!ok) { sqlite_close(db); db = NULL; }
    file_free(directory); string_free(parent);
    return db;
}

static bool path_store(sqlite_t *db, const char *key, const char *value, bool replace)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, replace ? "insert or replace into resource_path values(?1,?2)" :
                                                      "insert or ignore into resource_path values(?1,?2)");
    bool ok = q && value && *value && sqlite_stmt_bind_text(q, 1, key) && sqlite_stmt_bind_text(q, 2, value) &&
              sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

/* Keep configuration changes explicit; normal startup never writes a resource setting. */
int mars_paths_command(int argc, char **argv)
{
    bool seed = argc >= 2 && !strcmp(argv[0], "seed");
    bool list = !argc || (argc == 1 && !strcmp(argv[0], "list"));
    bool init = argc == 1 && !strcmp(argv[0], "init");
    bool set = argc == 3 && !strcmp(argv[0], "set") && path_kind(argv[1]) >= 0 && *argv[2];
    bool unset = argc == 2 && !strcmp(argv[0], "unset") && path_kind(argv[1]) >= 0;
    if ((!seed && !list && !init && !set && !unset) || (seed && (argc < 4 || argc % 2))) return 2;
    if (!seed && !list && !mars_settings_initialise()) return 1;
    string_t *path = seed ? string_new_with(argv[1]) : path_configuration();
    if (list) {
        printf("Configuration: %s\n", path ? string_c_str(path) : "unavailable");
        for (unsigned i = 0; i < MARS_PATH_COUNT; ++i) {
            string_t *value = mars_path_resource(i, NULL, "");
            printf("%s = %s\n", resources[i].key, value ? string_c_str(value) : "not configured");
            string_free(value);
        }
        string_free(path);
        return 0;
    }
    sqlite_t *db = path ? path_open_settings(path, seed) : NULL;
    bool ok = db && sqlite_exec_cstr(db, "begin immediate");
    if (seed)
        for (int i = 2; ok && i < argc; i += 2)
            ok = path_kind(argv[i]) >= 0 && path_store(db, argv[i], argv[i + 1], true);
    if (set) ok = ok && path_store(db, argv[1], argv[2], true);
    if (unset) {
        sqlite_stmt_t *q = ok ? sqlite_stmt_prepare(db, "delete from resource_path where resource=?1") : NULL;
        ok = q && sqlite_stmt_bind_text(q, 1, argv[1]) && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
        sqlite_stmt_finalize(q);
    }
    if (init) {
        string_t *defaults = path_defaults();
        for (unsigned i = 0; ok && i < MARS_PATH_COUNT; ++i) {
            const char *explicit_path = getenv(resources[i].environment);
            string_t *value = explicit_path && *explicit_path ? string_new_with(explicit_path) :
                                                               path_read(defaults, resources[i].key);
            ok = value && path_store(db, resources[i].key, string_c_str(value), false);
            string_free(value);
        }
        string_free(defaults);
    }
    if (ok) ok = sqlite_exec_cstr(db, "commit");
    else if (db) sqlite_exec_cstr(db, "rollback");
    if (!ok) fputs("Cannot update MARS resource configuration.\n", stderr);
    sqlite_close(db); string_free(path);
    return ok ? 0 : 1;
}
