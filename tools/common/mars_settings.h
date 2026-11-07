/**
 * @file mars_settings.h
 * @brief Persistent application settings with a small JSON database locator.
 *
 * MARS_CONFIG_FILE selects the bootstrap JSON; its database member identifies mars.db.
 * Relative database paths resolve from that JSON file.
 * Stored application settings supplement, rather than replace, explicitly supplied environment values.
 * Bulk content workers may isolate settings access before starting threads, keeping the encrypted store
 * and its memory-protection policy in an owned helper process.
 */
#ifndef MARS_APP_SETTINGS_H
#define MARS_APP_SETTINGS_H

#include "string-t.h"
#include "sqlite-t.h"

/**
 * @brief Resolve the configuration database through the bootstrap JSON.
 * @return Owned absolute path, released with string_free, or NULL on invalid configuration or allocation/I/O failure.
 * @note Relative paths resolve from mars.json. Without an explicit locator, an absent default uses ~/.mars/mars.db.
 * Does not open the database or obtain its key. Call before changing the startup environment.
 */
string_t *mars_settings_database(void);

/**
 * @brief Create the default bootstrap JSON if absent, without replacing an existing locator.
 * @return True when the locator exists or is created privately; false on allocation, directory or write failure.
 * @note Does not create mars.db or retrieve a key. Use during single-threaded initialisation.
 */
bool mars_settings_initialise(void);

/**
 * @brief Load stored application settings into unset process variables.
 * @return True after a valid snapshot is applied; false on unavailable or invalid configuration, allocation,
 * transport or environment-update failure. Does not replace explicit environment values.
 * @note Call before starting workers or opening content stores. Copies all values; no release is required.
 */
bool mars_settings_load(void);

/**
 * @brief Read one application setting without changing the process environment.
 * @param[in] name Borrowed canonical setting name; not retained.
 * @return Owned value, released with string_free, or an empty string when absent.
 * Returns NULL on invalid input, unavailable configuration or allocation/transport failure.
 * Paths are resolved relative to mars.db. Explicit environment overrides are not consulted.
 * @note Use during single-threaded startup; isolation delegates decryption to the settings helper.
 */
string_t *mars_settings_read(const char *name);

/**
 * @brief Confine settings reads and automatic installer updates to an owned helper process.
 * @return True after the helper is started, or if this process already owns a usable helper; false on failure.
 * @note Call once during single-threaded startup, before opening databases or accessing the key store.
 * The helper retains SQLCipher memory protection. Replies use a private socket with a 16 MiB message limit;
 * credentials never enter temporary files or command arguments. Direct settings/paths editing commands and
 * mars_settings_open remain local APIs and must not be used by an isolated content worker. A forked child
 * cannot reuse its parent's helper. Failures never fall back to local decryption.
 */
bool mars_settings_isolate(void);

/**
 * @brief Close and reap this process's settings helper; also called automatically at normal exit.
 * @return No value; a repeated call or a call from a forked child has no effect.
 * @note Complete all settings requests first. Subsequent isolated accesses fail rather than decrypting locally.
 */
void mars_settings_isolation_close(void);

/**
 * @brief Supply a process-owned key source before opening settings.
 * @param[in] provider Callback returning an owned key string, or NULL to restore the desktop provider.
 * The callback's create parameter permits generating a missing key only for a new database.
 * @return No value; clears and wipes the previous cached key.
 * @note The provider must remain valid for later opens and inherited helper processes. Ownership of each
 * returned key transfers to settings; the cached key is wiped before release. Change providers only before workers start.
 */
void mars_settings_key_provider(string_t *(*provider)(bool create));

/**
 * @brief Open the encrypted configuration store, creating its parent directory only when requested.
 * @param create Whether to initialise a missing database and its parent directory.
 * @return Owned database handle, released with sqlite_close, or NULL on failure.
 * @note Isolated content workers are refused with errno set to EPERM; only their helper may decrypt settings.
 */
sqlite_t *mars_settings_open(bool create);

/**
 * @brief Resolve a resource path from the encrypted configuration snapshot.
 * @param[in] name Borrowed resource name; not retained.
 * @return Owned resolved path, released with string_free, or NULL when absent or on configuration/allocation failure.
 * @note Relative paths resolve from mars.db. Uses the settings helper when isolated and serialises local cache access.
 */
string_t *mars_settings_resource(const char *name);

/**
 * @brief Import a JSON object containing string-valued application settings in one transaction.
 * @param[in] text Borrowed JSON text, at most 1 MiB and 1,024 settings; never retained or printed.
 * @param replace True replaces existing values; false preserves values already stored.
 * @return True after committing every setting; false on malformed names, non-string values, unavailable
 * configuration or an SQL failure. Failure rolls back the complete update. No argument is consumed.
 * @note Use before application threads start, or through the serialised settings helper for automatic updates.
 */
bool mars_settings_import(const string_t *text, bool replace);

/**
 * @brief Persist an automatic JSON settings update through the encrypted configuration owner.
 * @param[in] text Borrowed JSON object with string values; follows mars_settings_import limits and ownership.
 * @return True after the settings transaction commits; false on validation, key-store, transport or SQL failure.
 * @note Uses the owned helper when isolation is enabled; never writes or reads an environment file.
 */
bool mars_settings_sync(const string_t *text);

/**
 * @brief Obtain stable secret material, creating it atomically only when the named setting is empty or absent.
 * @param[in] name Borrowed valid setting name ending in _KEY or _SECRET; not retained.
 * @return Owned secret string, released with string_free after wiping its bytes; NULL on invalid input,
 * unavailable key store, allocation failure or an SQL/transport error. Existing non-empty bytes are preserved.
 * @note New secrets contain 48 random bytes encoded as URL-safe Base64. Concurrent callers receive the same
 * committed secret. Uses the settings helper under isolation and never writes a plaintext secret or lock file.
 */
string_t *mars_settings_secret(const char *name);

/**
 * @brief Execute settings list, init, set, unset or import JSON_FILE.
 * @param argc Number of arguments after the settings command.
 * @param[in] argv Borrowed argument vector, valid throughout the call; not retained.
 * @return Zero on success, one on configuration/validation/I/O failure, two for invalid arguments.
 * @note Invoke before application threads start, without isolation. May create the private configuration
 * and request desktop key-store access. Listings redact credentials; no returned allocation requires release.
 */
int mars_settings_command(int argc, char **argv);

#endif
