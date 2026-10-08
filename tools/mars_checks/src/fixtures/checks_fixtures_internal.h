/**
 * @file checks_fixtures_internal.h
 * @brief Private executable selection for fixture lifecycle regression tests.
 *
 * Allows the README tests to exercise genuine spawn failures using a disposable
 * executable path. Normal checker callers use checks_fixtures_new from the
 * public application header and its configured native fixture executable.
 */
#ifndef MARS_CHECKS_FIXTURES_INTERNAL_H
#define MARS_CHECKS_FIXTURES_INTERNAL_H

#include "checks_fixtures.h"

/**
 * @brief Allocate a fixture owner with an explicit executable path.
 * @param root Borrowed repository root, cloned into the owner.
 * @param executable Borrowed executable path, cloned into the owner.
 * @return Owned collection; release with checks_fixtures_free.
 */
checks_fixtures_t *checks_fixtures_new_with_executable(const string_t *root, const string_t *executable);

#endif
