/**
 * @file cfg_locales.h
 * @brief Native regeneration of pinned Gregorian calendar locale SQL.
 *
 * Resolves territory and language selections from the pinned Babel 2.17/CLDR 46
 * snapshot and explicit supplements, shares identical name sets and tokenises
 * long-date patterns. No Python, network or system locale database is required.
 * Returned SQL preserves the source UTF-8 bytes; callers own returned strings.
 */
#ifndef MARS_CONFIG_LOCALES_H
#define MARS_CONFIG_LOCALES_H

typedef struct _string_t string_t;

/**
 * @brief Run native pinned calendar locale generation.
 * @param argc Number of command-only arguments, excluding the executable and subcommand.
 * @param argv Borrowed argument vector supporting --check, --output PATH and --data-dir DIR.
 * @return Zero on success, one on generation or I/O failure, two for invalid arguments.
 */
int cfg_locales_run(int argc, char **argv);

/**
 * @brief Generate complete SQL without publishing or changing installed data.
 * @param root Borrowed repository root containing the country-jurisdiction SQL source.
 * @param data_dir Borrowed directory containing the versioned resolved snapshot and supplements.
 * @return Owned exact UTF-8 SQL, released with string_free, or NULL with a diagnostic on failure.
 */
string_t *cfg_locales_generate(const string_t *root, const string_t *data_dir);

#endif
