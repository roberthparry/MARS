/**
 * @file cfg_calendar.h
 * @brief Native installer selection and population of location-specific SQL calendars.
 *
 * Selects a town and supported display locale from an encrypted staging database,
 * expands native holiday and weekend policy, and fills the calendar's daily rows.
 * The supplied connection remains caller-owned. Its imported schema and rules
 * must be committed so a separate public jurisdiction engine can read them.
 * Calls temporarily change environment settings and TZ and therefore require
 * the installer's single-threaded execution model. No database is published here.
 * Unicode catalogue normalisation requires HAVE_UNISTRING. Builds without that
 * optional facility reject non-ASCII catalogue/selection text with a diagnostic,
 * rather than silently changing accent and case equivalence.
 */
#ifndef MARS_CFG_CALENDAR_H
#define MARS_CFG_CALENDAR_H

#include <stdbool.h>

#include "sqlite.h"

/**
 * @brief Form an accent-insensitive, case-folded catalogue lookup key.
 * @param text Borrowed text to fold, decompose and trim, collapsing Unicode whitespace.
 * @param language True additionally maps hyphens to underscores for locale identifiers.
 * @return Owned key released with string_free, or NULL on failure. Non-ASCII input
 * requires HAVE_UNISTRING; this helper does not change environment or timezone state.
 */
string_t *cfg_calendar_key(const string_t *text, bool language);

/**
 * @brief Select and populate a complete native calendar in a staging database.
 * @param db Borrowed encrypted staging connection with committed imported rules.
 * @param path Borrowed staging filename used by the native jurisdiction engine.
 * @param key Borrowed encryption key; never printed or placed in a subprocess argument.
 * @param requested Optional requested town, optionally followed by a jurisdiction.
 * @param saved Optional previously selected location.
 * @param interactive Enable paginated location and language prompts.
 * @param language Optional requested language or locale.
 * @param saved_language Optional previously selected language or locale.
 * @param chosen_location Required output receiving owned canonical "town, jurisdiction" text.
 * @param chosen_language Required output receiving the owned selected locale.
 * @return True after all calendar date fields validate; false leaves outputs NULL.
 * Release successful outputs with string_free. Daily-table writes roll back on failure.
 */
bool cfg_calendar_populate(sqlite_t *db, const string_t *path, const string_t *key, const string_t *requested,
                           const string_t *saved, bool interactive, const string_t *language,
                           const string_t *saved_language, string_t **chosen_location, string_t **chosen_language);

#endif
