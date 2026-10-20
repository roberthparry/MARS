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
 * @brief Install or verify independent shoreline storage using saved globe settings or environment overrides.
 * @param action globe-install or globe-check; reserved update/removal commands fail without changing files.
 * @return Zero on success, one on operational failure, two for an unsupported action.
 * Installation validates existing destinations without replacing them. Missing keys may be prompted for on a terminal.
 * No network download occurs.
 */
int cfg_globe_run(const char *action);

/**
 * @brief Install shoreline storage with an explicitly supplied shared key, without prompting.
 * @param key Borrowed non-empty password; never printed.
 * @return Zero on success, one on failure.
 */
int cfg_globe_install_key(const char *key);

/**
 * @brief Prompt once for a shared password and install all three databases sequentially.
 * @return Zero on success, one on failure; stops at the first failed installation.
 * An empty terminal answer retains the saved almanac key. Non-interactive calls reuse that key.
 */
int cfg_databases_run(void);

/**
 * @brief Install the selected private database using native SQLCipher.
 * @param jurisdiction True selects jurisdiction; false selects almanac.
 * @param path Optional borrowed command-line database path.
 * @param key Optional borrowed command-line password; never printed.
 * @param location Optional borrowed requested calendar location.
 * @param language Optional borrowed requested calendar language.
 * @param interactive Permit prompts when standard input is a terminal.
 * @return Zero on success, one on failure.
 * Borrowed optional arguments override environment and stored settings. Interactive
 * password prompts are used only when requested and standard input is a terminal.
 * Location and language apply only to jurisdiction installation. Existing database
 * contents survive import or calendar failure. No SQL command-line process is used.
 */
int cfg_database_run(bool jurisdiction, const char *path, const char *key, const char *location, const char *language,
                     bool interactive);

#endif
