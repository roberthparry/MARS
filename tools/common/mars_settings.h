/**
 * @file mars_settings.h
 * @brief Persistent application settings with a small JSON database locator.
 *
 * MARS_CONFIG_FILE selects the bootstrap JSON; its database member identifies mars.db.
 * Relative database paths resolve from that JSON file.
 * Stored application settings supplement, rather than replace, explicitly supplied environment values.
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

/** @brief Supply a process-owned key source before opening settings; NULL restores the desktop provider. */
void mars_settings_key_provider(string_t *(*provider)(bool create));

/** @brief Open the encrypted configuration store, creating its parent directory only when requested. */
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
