/**
 * @file cfg_database_internal.h
 * @brief Private SQL streaming and publication boundary for database installers.
 *
 * Only database implementation files and their regression tests use these helpers.
 * Application callers use cfg_database.h. Streams preserve source transactions;
 * publication owns no input handles and requires a closed, completed database.
 */
#ifndef MARS_CONFIG_DATABASE_INTERNAL_H
#define MARS_CONFIG_DATABASE_INTERNAL_H

#if !defined(MARS_CFG_DATABASE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_CFG_DATABASE_INTERNAL_ACCESS."
#endif

#include <stdint.h>

#include "file-t.h"
#include "sqlite-t.h"

/** Open the selected format: an empty key means plaintext; NULL is a configuration failure. */
sqlite_t *cfg_database_open(const string_t *path, const string_t *key);

/** @brief Import optional bundled country flags into the jurisdiction catalogue, preserving other tables. */
bool cfg_database_flags(sqlite_t *db, const string_t *root);

/** Record a completed managed-data migration inside its caller-owned transaction. */
bool cfg_database_revision(sqlite_t *db, const char *name, int revision);

/** Read the managed-data revision, returning zero for legacy databases without revision metadata. */
bool cfg_database_current_revision(sqlite_t *db, const char *name, int64_t *revision);

/** Refresh countries and, before revision three, shoreline; retain river data and credentials. */
bool cfg_database_globe_refresh(sqlite_t *db, const string_t *root, const string_t *staging);

/** @brief Atomically import all packaged photographic images and their verified metadata, preserving other layers. */
bool cfg_database_textures(sqlite_t *db, const string_t *root);

/** @brief Validate the complete installed photographic image set, including content digests. */
bool cfg_database_textures_valid(sqlite_t *db);

/** Shared installer-owned settings; callbacks borrow these strings and never publish them directly. */
typedef struct cfg_database_options {
    string_t *home;
    string_t *configuration_directory;
    string_t *configuration_path;
    file_t *configuration;
    string_t *path;
    string_t *expanded_path;
    string_t *key;
    string_t *requested_location;
    string_t *saved_location;
    string_t *requested_language;
    string_t *saved_language;
} cfg_database_options;

/** Database-specific policy consumed by the common plaintext or encrypted installation lifecycle. */
typedef struct cfg_database_profile {
    const char *name;
    const char *title;
    const char *default_path;
    const char *path_variable;
    const char *key_variable;
    bool (*read_options)(cfg_database_options *, const string_t *, const char *, const char *);
    bool (*import_sources)(sqlite_t *, const string_t *, const string_t *);
    bool (*populate)(sqlite_t *, const string_t *, const cfg_database_options *, bool, string_t **, string_t **);
    bool (*export_settings)(string_t *, const string_t *, const string_t *);
} cfg_database_profile;

/** Almanac-specific settings, legacy-path migration and packaged ephemeris sources. */
extern const cfg_database_profile cfg_database_almanac;

/** Jurisdiction-specific settings, packaged rules and calendar generation. */
extern const cfg_database_profile cfg_database_jurisdiction;

/** Resolve a setting in explicit, environment, saved and fallback order. */
string_t *cfg_database_setting(file_t *configuration, const char *explicit_value, const char *name,
                               const string_t *fallback);

/** Append a safely quoted shell setting to an installer-owned configuration body. */
bool cfg_database_export(string_t *body, const char *name, const string_t *value);

/** Import a packaged source, allowing absence only when optional is true. */
bool cfg_database_source(sqlite_t *db, const string_t *root, const char *name, bool optional);

/** Bounded literal INSERT state, owned by a single recursive SQL stream. */
typedef struct cfg_database_values {
    string_t *prefix;
    bool transaction;
    bool trace;
    double execution_cpu;
    size_t execution_calls;
} cfg_database_values;

/**
 * @brief Select a fixed diagnostic label without exposing arbitrary source paths.
 * @param[in] path Borrowed repository-relative path; NULL is treated as unknown.
 * @return Static allowlisted label, or "unlisted SQL source". Never caller-owned.
 */
const char *cfg_database_trace_label(const string_t *path);

/**
 * @brief Describe a packaged SQL operation without exposing arbitrary source paths.
 * @param[in] path Borrowed repository-relative path; NULL is treated as custom input.
 * @return Static operation description, or "Importing custom SQL". Never caller-owned.
 */
const char *cfg_database_progress_label(const string_t *path);

/** Execute SQL and optionally accumulate execution CPU time and call count for the source trace. */
bool cfg_database_execute(sqlite_t *db, cfg_database_values *values, const char *sql);

/** Recognise plain or OR IGNORE/REPLACE INSERT headers with optional columns and multiline whitespace. */
bool cfg_database_values_header(const string_t *sql);

/** Process a literal tuple line already appended to pending; report whether it was handled. */
bool cfg_database_values_line(sqlite_t *db, cfg_database_values *values, string_t *pending, const string_t *line,
                              bool *handled);

/** Roll back an unfinished batched INSERT and release its owned prefix. */
void cfg_database_values_free(sqlite_t *db, cfg_database_values *values);

/** Import repository-relative SQL recursively, with bounded line, statement and nesting sizes. */
bool cfg_database_import(sqlite_t *db, const string_t *root, const string_t *path);

/**
 * @brief Import the required complete, hash-verified shoreline artifact without repairs or network access.
 * @param db Borrowed writable database; an enclosing transaction is preserved.
 * @param root Borrowed repository root containing the packaged source.
 * @param staging Borrowed installer-owned directory for bounded decompression.
 * @return True on import; false on missing or invalid archive parts, hash mismatch or I/O failure.
 */
bool cfg_database_coast(sqlite_t *db, const string_t *root, const string_t *staging);

/**
 * @brief Prepare reviewed shoreline span corrections without modifying the upstream source or any database.
 * @note All handles are borrowed and closed. Output must not exist. Revision, source counts, bounds and matching
 * endpoints are checked; malformed or missing corrections fail before import. Failed output is caller-cleaned.
 */
bool cfg_database_coast_correct(file_t *input, file_t *output, file_t *corrections);

/** @brief Retire withdrawn legacy river data during jurisdiction installation without changing unrelated tables. */
bool cfg_database_rivers(sqlite_t *db, const string_t *root, const string_t *staging);

/**
 * @brief Replace river centrelines transactionally from the maintained OSM GeoJSON sequence export.
 * @param db Borrowed writable globe database; unrelated tables are preserved.
 * @param source Borrowed open plain or Zstandard stream; records are bounded to 4 MiB and 100,000 coordinates each.
 * @return True after a non-empty validated import commits; false rolls back all changes.
 * @note Installer-only: reduces stored centrelines within 8 m, fixing shared-node anchors and rejecting crossing
 * shortcuts. Full source geometry remains in the separate extract; water-area outlines are not imported.
 * Known widths below 16 m are omitted per source way; unknown widths and wider sections of the same river remain.
 * Compressed input requires one complete frame with an at-most-8-MiB window; expanded input is capped at 128 GiB.
 */
bool cfg_database_import_osm_rivers(sqlite_t *db, file_t *source);

/**
 * @brief Copy validated OSM chunks between distinct borrowed connections, replacing only river tables transactionally.
 * @note Rejects candidates or rebuilt river layers exceeding 3,800,000,000 bytes, including indexes; failures roll back.
 */
bool cfg_database_publish_rivers(sqlite_t *destination, sqlite_t *source);

/** Maximum installed river-layer footprint in bytes, including indexes; shared with bounded installer regressions. */
extern const int64_t cfg_database_river_storage_budget;

/**
 * @brief Publish with an explicitly smaller positive budget, never exceeding the approved production limit.
 * @note Enables bounded rollback regressions without allocating gigabytes. Production uses cfg_database_publish_rivers().
 */
bool cfg_database_publish_rivers_limited(sqlite_t *destination, sqlite_t *source, int64_t budget);

/** Create an unpredictable mode-0700 staging directory beneath the supplied directory. */
string_t *cfg_database_stage(const string_t *directory);

/** Publish a staged database and configuration, restoring the previous database if configuration rename fails. */
bool cfg_database_publish(file_t *database, file_t *destination, file_t *configuration,
                          const string_t *configuration_directory, const string_t *staging, const string_t *body);

/** Remove only installer-owned staging entries, leaving an unrecovered backup intact. */
void cfg_database_cleanup(const string_t *staging);

#endif
