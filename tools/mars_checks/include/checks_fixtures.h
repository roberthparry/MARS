/**
 * @file checks_fixtures.h
 * @brief Local HTTP fixture lifecycle for documented programmes.
 *
 * Starts the native tests/http/fixtures peers on loopback-only ephemeral
 * sockets. Peers are lazy and shared across the sequential README run. The
 * owner must free the collection to terminate and reap all child processes.
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

#endif
