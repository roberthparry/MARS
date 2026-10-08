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

#include "file.h"
#include "sqlite.h"

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

/** Create an unpredictable mode-0700 staging directory beneath the supplied directory. */
string_t *cfg_database_stage(const string_t *directory);

/** Publish a staged database and configuration, restoring the previous database if configuration rename fails. */
bool cfg_database_publish(file_t *database, file_t *destination, file_t *configuration,
                          const string_t *configuration_directory, const string_t *staging, const string_t *body);

/** Remove only installer-owned staging entries, leaving an unrecovered backup intact. */
void cfg_database_cleanup(const string_t *staging);

#endif
