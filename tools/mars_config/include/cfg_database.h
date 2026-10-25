/**
 * @file cfg_database.h
 * @brief Native installation of private globe, almanac and jurisdiction databases.
 *
 * The configuration executable uses this entry point to resolve saved settings,
 * import packaged SQL through SQLCipher and atomically replace a completed private
 * database. Jurisdiction installation delegates calendar selection and population
 * to cfg_calendar. Passwords are never included in installer diagnostics. Calls
 * change process-owned configuration and must be externally serialised.
 */
#ifndef MARS_CONFIG_DATABASE_H
#define MARS_CONFIG_DATABASE_H

#include <stdbool.h>

/**
 * @brief Install, verify or update independent shoreline storage using saved settings or environment overrides.
 * @param action globe-install, globe-check or globe-update; reserved removal commands fail without changing files.
 * @note Installation builds a fresh staged database and replaces the destination only after validation. Existing
 * river and custom tables are removed on success. Updating replaces packaged geography and texture tables in one
 * transaction, retaining the configuration and other layers. Both operations report import progress.
 * @return Zero on success, one on operational failure, two for an unsupported action.
 * Installation reuses the selected path and key; saved river staging credentials survive replacement.
 * Empty or absent keys select plaintext. Failed builds leave the installed database unchanged.
 * No network download occurs.
 */
int cfg_globe_run(const char *action);

/**
 * @brief Build fresh geography and photographic texture storage with an explicit key, without prompting.
 * @param key Borrowed password; empty selects plaintext, NULL is invalid. Never printed.
 * @return Zero on success, one on failure.
 */
int cfg_globe_install_key(const char *key);

/**
 * @brief Stage reduced OSM river centrelines at a new path with an 8 m error ceiling and saved key; never publish it.
 * @note The compacted candidate must not exceed 3,800,000,000 bytes, including indexes and encryption overhead.
 * Known widths below 16 m are omitted per source way, independently of the 8 m geometry simplification tolerance.
 * Accepts plain GeoJSON sequences or one Zstandard frame, streamed without an expanded intermediate file.
 * Saves source-identified phase/batch checkpoints for cfg_globe_river_resume; incomplete stages lack publication metadata.
 */
int cfg_globe_river_stage(const char *input_path, const char *output_path);

/** @brief Resume a checkpointed river candidate after verifying its source SHA-256; never publish it. */
int cfg_globe_river_resume(const char *input_path, const char *output_path);

/**
 * @brief Install a candidate in the configured storage mode, preserving unrelated tables and the saved key.
 * @note Rebuilding a river layer above 3,800,000,000 bytes rolls back replacement, even when existing free space is reused.
 */
int cfg_globe_river_install(const char *candidate_path);

/**
 * @brief Install all three databases sequentially in their individually configured storage modes.
 * @return Zero on success, one on failure; stops at the first failed installation.
 * Empty or absent keys select plaintext; non-empty saved keys retain encryption. No password prompting occurs.
 */
int cfg_databases_run(void);

/**
 * @brief Apply pending versioned migrations to existing databases sequentially, without prompting or replacing files.
 * Saved credentials and paths are reused. Missing databases are skipped; invalid keys, newer revisions and failed
 * migrations stop the operation. Each database commits its migration and revision together, preserving unrelated data.
 */
int cfg_databases_update(void);

/**
 * @brief Install missing databases and migrate existing ones sequentially, without prompting.
 * Reuses configured paths and keys; new configurations default to plaintext. Existing stores and settings are
 * preserved except for pending managed migrations. Failed installation or migration stops before later stores.
 * @return Zero on success, one on failure.
 */
int cfg_databases_ensure(void);

/**
 * @brief Export the three installed databases to plaintext and switch their saved configurations sequentially.
 * Requires stopped database users and no path/key environment overrides. Original encrypted databases and a
 * private configuration backup are retained. New files use the .plain.db suffix; existing outputs are never replaced.
 * Schema, data, user_version and application_id are preserved. Already-plaintext stores are validated and skipped.
 * @return Zero on success; one stops at the first failure, leaving later stores unchanged.
 */
int cfg_databases_plain(void);

/**
 * @brief Install the selected private database using native SQLCipher.
 * @param jurisdiction True selects jurisdiction; false selects almanac.
 * @param path Optional borrowed command-line database path.
 * @param key Optional borrowed command-line password; never printed.
 * @param location Optional borrowed requested calendar location.
 * @param language Optional borrowed requested calendar language.
 * @param interactive Permit prompts when standard input is a terminal.
 * @return Zero on success, one on failure.
 * Borrowed optional arguments override environment and stored settings.
 * Empty keys select plaintext; non-empty keys select encryption, without password prompting.
 * Location and language apply only to jurisdiction installation. Existing database
 * contents survive import or calendar failure. No SQL command-line process is used.
 */
int cfg_database_run(bool jurisdiction, const char *path, const char *key, const char *location, const char *language,
                     bool interactive);

#endif
