/**
 * @file checks_fixtures.h
 * @brief Local protocol fixtures and installed-resource settings for documented programmes.
 *
 * Starts the native tests/http/fixtures peers on loopback-only ephemeral
 * sockets. Peers are lazy and shared across the sequential README run. The
 * owner must free the collection to terminate and reap all child processes.
 * Installed-resource examples inherit the saved application settings through an isolated helper;
 * disposable globe-reader examples remain independent of installed data.
 */
#ifndef MARS_CHECKS_FIXTURES_H
#define MARS_CHECKS_FIXTURES_H

#include "checks_support.h"

/** Opaque owner of the three optional local protocol peers. */
typedef struct checks_fixtures checks_fixtures_t;

/**
 * @brief Allocate an empty fixture owner, borrowing no caller storage.
 * @param root Borrowed repository root path.
 * @return Owned result; release with the corresponding free function. NULL denotes an inspection error where
 * documented.
 */
checks_fixtures_t *checks_fixtures_new(const string_t *root);

/**
 * @brief Stop and reap all peers, close listeners and free the owner.
 * @param fixtures Owned object to release; NULL is accepted.
 * @return No value.
 */
void checks_fixtures_free(checks_fixtures_t *fixtures);

/**
 * @brief Append the same protocol and file-example arguments as the original runner.
 * @param fixtures Borrowed fixture owner, except when ownership is explicitly released.
 * @param path Borrowed path; ownership is unchanged.
 * @param code Borrowed programme source.
 * @param arguments Borrowed argument collection; each element is one literal argument.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_fixtures_arguments(checks_fixtures_t *fixtures, const string_t *path, const string_t *code,
                               checks_strings_t *arguments);

/**
 * @brief Assemble a README programme command with disposable storage and local protocol fixtures as needed.
 * @param[in,out] fixtures Borrowed owner of lazily started protocol peers; release with checks_fixtures_free.
 * @param[in] path Borrowed Markdown path identifying the example; not retained.
 * @param[in] code Borrowed programme source used to select the required fixtures; not retained.
 * @param[in] binary Borrowed executable path, copied into the command.
 * @param[in] working Borrowed private working directory, already created by the caller.
 * @param[in,out] arguments Borrowed empty collection receiving owned command words; no shell is used.
 * @return True when preparation succeeds; false on storage or protocol-fixture failure. Release the command
 * collection on either path and remove the working directory after the child exits, including failure paths.
 * @note Globe reader identity examples receive an unencrypted temporary SQLite database through child-only
 * environment assignments. Examples opening installed almanac, globe or jurisdiction data lazily load the saved
 * settings once into the process environment, preserving explicit overrides, including empty keys. The encrypted
 * settings store is read in an isolated helper; credentials never enter command arguments. No database is created
 * or installed by this preparation. Call sequentially before execution and before starting application threads.
 */
bool checks_fixtures_programme(checks_fixtures_t *fixtures, const string_t *path, const string_t *code,
                              const string_t *binary, const string_t *working, checks_strings_t *arguments);

#endif
