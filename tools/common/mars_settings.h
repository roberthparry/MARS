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

/** @brief Return the owned configuration database path, or NULL for invalid bootstrap configuration. */
string_t *mars_settings_database(void);

/** @brief Create the default bootstrap JSON if absent, without replacing an existing locator. */
bool mars_settings_initialise(void);

/** @brief Load stored application settings into unset process variables before starting workers or opening stores. */
bool mars_settings_load(void);

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

/** @brief Supply a process-owned key source before opening settings; NULL restores the desktop provider. */
void mars_settings_key_provider(string_t *(*provider)(bool create));

/**
 * @brief Open the encrypted configuration store, creating its parent directory only when requested.
 * @param create Whether to initialise a missing database and its parent directory.
 * @return Owned database handle, released with sqlite_close, or NULL on failure.
 * @note Isolated content workers are refused with errno set to EPERM; only their helper may decrypt settings.
 */
sqlite_t *mars_settings_open(bool create);

/** @brief Return an owned resolved resource setting from the encrypted configuration snapshot. */
string_t *mars_settings_resource(const char *name);

/** @brief Import literal assignments transactionally; never execute shell text or print values. */
bool mars_settings_import(const string_t *text, bool replace);

/** @brief Synchronise a successful legacy installer publication when the encrypted configuration is enabled. */
bool mars_settings_sync(const string_t *text);

/** @brief Run settings list, set, unset or import FILE; return a process exit status. */
int mars_settings_command(int argc, char **argv);

#endif
