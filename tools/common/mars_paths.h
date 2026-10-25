/**
 * @file mars_paths.h
 * @brief Shared application resource configuration for native MARS tools.
 *
 * Resolves named resources from a separate SQLite configuration store, explicit environment overrides,
 * or a build-generated configuration beside the executable. Relative configured paths are relative to
 * their configuration database, never the caller's working directory. Encrypted settings hold the
 * content-database locations and credentials. These are application facilities, not library interfaces.
 */
#ifndef MARS_APP_PATHS_H
#define MARS_APP_PATHS_H

#include "string-t.h"
#include "file-t.h"

/** @brief Load settings and resolved resources before starting child processes; explicit environment values win. */
bool mars_paths_load(void);

/** @brief Indexed resource categories; file resources accept an empty relative name. */
typedef enum {
    MARS_PATH_GLOBE, MARS_PATH_TEXTURES, MARS_PATH_JURISDICTION, MARS_PATH_ALMANAC,
    MARS_PATH_ASSETS, MARS_PATH_SCRIPTS, MARS_PATH_WASM, MARS_PATH_ICONS,
    MARS_PATH_WORKER, MARS_PATH_LOCALES, MARS_PATH_ZONEINFO, MARS_PATH_TEMPORARY, MARS_PATH_DOCUMENTATION,
    MARS_PATH_PRIVATE_ASSETS, MARS_PATH_PRIVATE_SCRIPTS, MARS_PATH_PRIVATE_WASM, MARS_PATH_COUNT
} mars_path_kind_t;

/** @brief Return an owned configured resource path; an explicit source root supports developer/test inputs. */
string_t *mars_path_resource(mars_path_kind_t kind, const string_t *source_root, const char *relative);

/** @brief Resolve logical packaged SQL names or explicit input paths without assuming a checkout layout. */
string_t *mars_path_source(const string_t *source_root, const char *name);

/** @brief Return an owned validated TZif path within the configured zoneinfo directory, or NULL. */
string_t *mars_path_zoneinfo(const string_t *name);

/** @brief Create a private temporary directory in the configured temporary location; caller removes and frees it. */
file_t *mars_path_temporary(void);

/** @brief Run paths list, set, unset, init or the build's seed command; return a process exit status. */
int mars_paths_command(int argc, char **argv);

#endif
