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
#include "json-t.h"
#include "sqlite-t.h"

/** @brief Managed geographic and photographic migrations and their in-place update boundaries. */
enum {
    CFG_GLOBE_TEXTURE_PATCH_BASE = 16, /**< First catalogue whose validated PNG rows support selective replacement. */
    CFG_GLOBE_PHOTO_REVISION = 17,     /**< Latest revision requiring a photographic catalogue update. */
    CFG_GLOBE_DATA_REVISION = 18       /**< Current catalogue, including Northern Cyprus regional flag coverage. */
};

/** Open the selected format: an empty key means plaintext; NULL is a configuration failure. */
sqlite_t *cfg_database_open(const string_t *path, const string_t *key);

/** @brief Import optional bundled country flags into the jurisdiction catalogue, preserving other tables. */
bool cfg_database_flags(sqlite_t *db, const string_t *root);

/** Record a completed managed-data migration inside its caller-owned transaction. */
bool cfg_database_revision(sqlite_t *db, const char *name, int revision);

/** Read the managed-data revision, returning zero for legacy databases without revision metadata. */
bool cfg_database_current_revision(sqlite_t *db, const char *name, int64_t *revision);

/**
 * @brief Finish an installer-owned transaction with live commit or rollback reporting.
 * @param[in] db Borrowed writable database with an active outer transaction; remains open.
 * @param[in] success True commits completed work; false requests restoration without committing.
 * @return True only after a successful commit. False preserves an earlier failure or reports
 * a failed commit, after attempting rollback. A NULL database returns false without SQL work.
 * @note Database installers use this helper serially after their stage reporters have finished.
 * Progress allocation failure does not prevent database work. No argument is retained or consumed.
 * Rollback reporting reflects its own result; callers remain responsible for handling update failure.
 */
bool cfg_database_finish_transaction(sqlite_t *db, bool success);

/**
 * @brief Execute trusted public-asset DDL without zero-overwriting discarded pages.
 * @param[in] db Borrowed writable database; serialise access for the complete call.
 * @param[in] sql Borrowed trusted SQL for public map or photographic tables only; never user input.
 * @return True after execution and restoration of the previous secure-delete setting; false on failure.
 * @note The original OFF, ON or FAST setting is restored even when the supplied SQL fails. Journaling,
 * durability and enclosing transactions remain enabled. This helper is unsuitable for private settings
 * or secrets. Arguments are neither consumed nor retained, and the database remains open.
 */
bool cfg_database_public_ddl(sqlite_t *db, const char *sql);

/**
 * @brief Prepare a new plaintext candidate containing complete river-class waterways.
 * @param[in] source Borrowed existing OSM centreline database, used only as a read source.
 * @param[in] output Borrowed path for a new candidate; existing paths are rejected.
 * @return True after a validated, committed candidate is available; false on invalid data or I/O failure.
 * @note Retains every kind-1 chunk, coordinate, anchor and spatial bound, including tributaries. Other
 * centreline classes are omitted. The source remains open and unchanged; no installed database is opened.
 * Call serially with no other transaction or attachment on source. No argument is consumed or retained.
 */
bool cfg_database_select_main_rivers(sqlite_t *source, const string_t *output);

/** Refresh versioned geography and textures using a caller-owned staging directory; retain rivers and credentials. */
bool cfg_database_globe_refresh(sqlite_t *db, const string_t *root, const string_t *staging);

/**
 * @brief Atomically synchronise packaged photographs and their provenance, preserving other layers.
 * @param[in,out] db Borrowed writable database; may already have an outer transaction and remains open.
 * @param[in] root Borrowed resource context used by the configured texture-path resolver; not retained.
 * @return True after releasing the import savepoint. False attempts rollback to the previous complete catalogue
 * when metadata, changed PNG validation, SQL work or allocation fails. No argument is consumed.
 * @note Compatible stores retain rows whose dimensions, encoded length and content digest match the manifest;
 * only new or changed PNGs are read, validated and written. Older stores receive a complete import. Unchanged
 * rows rely on their earlier content validation; cfg_database_textures_valid() performs a full integrity audit.
 * Call serially, without another writer or concurrent operations on db. An outer transaction still controls commit.
 */
bool cfg_database_textures(sqlite_t *db, const string_t *root);

/**
 * @brief Compare the prepared texture manifest with the installed snapshot without loading PNG payloads.
 * @param[in] db Borrowed readable globe database; access must be serialised for the call.
 * @param[in] root Borrowed resource-location context, resolved through the normal path configuration.
 * @return True only for byte-identical, bounded manifests. False reports changed or missing metadata,
 * invalid arguments or a read failure; the normal importer must validate a requested replacement.
 * @note Reads at most GLOBE_TEXTURE_MANIFEST_LIMIT bytes from each manifest. No argument is retained,
 * consumed or modified, and no image body is read. Callers retain ownership of both arguments.
 */
bool cfg_database_textures_current(sqlite_t *db, const string_t *root);

/**
 * @brief Import available prepared source pyramids without rewriting original photographs.
 * @param[in,out] db Borrowed writable globe connection, used serially without an enclosing transaction.
 * @param[in] root Borrowed resource context for the configured texture directory; not retained.
 * @return True when every available matching shard is already installed or imported successfully. Missing
 * optional shards retain the original-image loading path. False indicates invalid prepared data or I/O failure;
 * each previously completed source remains committed and the current failed source is rolled back.
 * @note No argument is consumed. Progress reports source counts. Repeated calls resume from completion markers.
 */
bool cfg_database_pyramids(sqlite_t *db, const string_t *root);

/** @brief Validate the complete installed photographic image set, including content digests. */
bool cfg_database_textures_valid(sqlite_t *db);

/**
 * @brief Copy every non-photographic table from the attached previous globe into an empty main database.
 * @param[in,out] db Borrowed plaintext staging connection with an active transaction and a previous attachment.
 * @return True after all table values and row counts have been preserved; false leaves publication to the caller.
 * @note The caller must hold the source writer lock. Values, rowids and virtual-table rows are copied through
 * public SQL; generated columns are recomputed. No input is retained or consumed. Use serially.
 */
bool cfg_database_copy_globe(sqlite_t *db);

/**
 * @brief Recreate preserved indexes, views, triggers and sequence state after table and image insertion.
 * @param[in,out] db Borrowed staging connection used by cfg_database_copy_globe(), with the previous attachment.
 * @return True after auxiliary schema is preserved; false on incompatible or invalid schema.
 * @note Run before committing and after all data has been copied, so triggers cannot replay during copying.
 * The connection remains caller-owned; no argument is retained or consumed.
 */
bool cfg_database_copy_globe_schema(sqlite_t *db);

/**
 * @brief Require an existing destination to have no journal, WAL or shared-memory sidecar files.
 * @param[in] destination Borrowed closed file handle; never consumed or retained.
 * @return True when all sidecars are absent; false on presence, unsafe paths or metadata errors.
 * @note Call under the destination writer guard before atomic publication. Never removes a sidecar.
 */
bool cfg_database_no_sidecars(file_t *destination);

/**
 * @brief Rebuild a plaintext globe beside its destination and atomically publish only a validated replacement.
 * @param[in,out] source Owned existing connection pointer; closed and set to NULL on entry. No active transaction
 * or statement may belong to it. The replacement uses its own writer guard on the destination.
 * @param[in] path Borrowed existing destination path; must identify a regular plaintext database.
 * @param[in] root Borrowed resource root, passed to apply without retention.
 * @param[in] staging Borrowed empty installer-owned directory on the destination filesystem.
 * @param[in] apply Borrowed migration callback operating only on the new staged connection.
 * @param[in] revision Expected final globe revision, checked under the source writer lock.
 * @param[in] force True reruns apply at the expected revision; false treats an already completed migration as success.
 * @return True after publication or an already completed migration; false preserves the installed database.
 * @note Source data, including unrelated tables and rowids, survives. Old images are not copied. Cache use is
 * bounded; SQLite journalling and full synchronisation remain enabled. Call serially with consumers closed.
 */
bool cfg_database_rebuild_globe(sqlite_t **source, const string_t *path, const string_t *root, const string_t *staging,
                               bool (*apply)(sqlite_t *, const string_t *, const string_t *), int revision, bool force);

/** Shared installer-owned settings; callbacks borrow these strings and never publish them directly. */
typedef struct cfg_database_options {
    string_t *home;                 /**< Owned MARS home, released by installer option cleanup. */
    string_t *configuration_path;   /**< Owned central mars.db path, never a per-database sidecar. */
    string_t *path;                 /**< Owned selected content-store path, retained for publication. */
    string_t *expanded_path;        /**< Owned path after home-directory expansion. */
    string_t *key;                  /**< Owned content-store credential; an empty string selects plaintext. */
    string_t *requested_location;   /**< Owned explicit calendar location, possibly empty. */
    string_t *saved_location;       /**< Owned previous calendar location from encrypted settings. */
    string_t *requested_language;   /**< Owned explicit calendar language, possibly empty. */
    string_t *saved_language;       /**< Owned previous calendar language from encrypted settings. */
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
    /**
     * @brief Add profile-specific values to the common installer's JSON transaction.
     * @param[in,out] settings Borrowed JSON object; receives copied string values and remains caller-owned.
     * @param[in] location Borrowed chosen location, valid throughout this call.
     * @param[in] language Borrowed chosen language, valid throughout this call.
     * @return True after every value is copied; false on invalid input or allocation failure.
     * @note Never publishes files or retains arguments. Called serially before common data/settings publication.
     */
    bool (*export_settings)(json_t *settings, const string_t *location, const string_t *language);
} cfg_database_profile;

/** Almanac-specific option policy and packaged ephemeris sources. */
extern const cfg_database_profile cfg_database_almanac;

/** Jurisdiction-specific settings, packaged rules and calendar generation. */
extern const cfg_database_profile cfg_database_jurisdiction;

/**
 * @brief Resolve an installer value from explicit arguments, the loaded settings environment or a default.
 * @param[in] explicit_value Optional command-line value, including an explicitly empty key; copied.
 * @param[in] name Borrowed variable name, already populated from mars.db during startup.
 * @param[in] fallback Optional borrowed default used only when neither earlier source is present.
 * @return Owned string, possibly empty; release with string_free. NULL reports allocation or configuration failure.
 * No argument is retained or consumed. Stored values come from mars.db; no per-database sidecar is read.
 */
string_t *cfg_database_setting(const char *explicit_value, const char *name, const string_t *fallback);

/**
 * @brief Copy one string setting into an installer-owned JSON update.
 * @param[in,out] body Borrowed JSON object, retained by its caller.
 * @param[in] name Borrowed setting name; copied into the object.
 * @param[in] value Borrowed value; copied without shell quoting or expansion.
 * @return True after insertion; false for invalid input or allocation failure. No argument is consumed.
 */
bool cfg_database_export(json_t *body, const char *name, const string_t *value);

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
/** @brief Parse an explicit OSM width in metres; estimates and malformed widths return false. */
bool cfg_database_river_width_metres(const json_t *properties, double *width);
/** @brief Recognise mapped open water, including lakes and lagoons; wetland alone is insufficient. */
bool cfg_database_river_open_water(const json_t *properties);

/**
 * @brief Copy validated OSM chunks between distinct borrowed connections, replacing only river tables transactionally.
 * @note Rejects candidates or rebuilt river layers exceeding 7,000,000,000 bytes, including indexes; failures roll back.
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

/**
 * @brief Publish a validated staged store and its encrypted JSON settings update.
 * @param[in,out] database Borrowed closed staged database; moved on success.
 * @param[in,out] destination Borrowed content-store destination; must be absent or a safe regular file.
 * @param[in] staging Borrowed private staging directory, retained throughout this call.
 * @param[in] body Borrowed JSON settings object, never retained or printed.
 * @return True after data replacement and settings commit; false on validation, file or settings failure.
 * Restores the previous data on a settings failure. If restoration fails, retains recovery data in staging.
 * @note Stop other database users before publication. No handle or string is consumed or freed.
 */
bool cfg_database_publish(file_t *database, file_t *destination, const string_t *staging, const string_t *body);

/** Remove only installer-owned staging entries, leaving an unrecovered backup intact. */
void cfg_database_cleanup(const string_t *staging);

#endif
