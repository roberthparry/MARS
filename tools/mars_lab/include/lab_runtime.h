/**
 * @file lab_runtime.h
 * @brief Private pre-fork cache configuration for the native MARS Lab launcher.
 *
 * Resolves the existing Lab object-store path and database-owned secret. Missing
 * secrets are generated once in an encrypted settings transaction. The cache
 * database itself is never opened or replaced by this facility.
 * Call only in the single-threaded parent before starting request workers.
 */
#ifndef MARS_LAB_RUNTIME_H
#define MARS_LAB_RUNTIME_H

#include <stdbool.h>

/**
 * @brief Prepare the inherited object-store environment without logging secrets.
 *
 * A non-empty MARS_LAB_OBJECT_STORE_KEY is an explicit override. Otherwise the
 * encrypted configuration supplies the existing key or atomically stores a new
 * libsodium-generated key. Existing keys are never rotated. Configuration or
 * desktop key-store failure returns false rather than replacing a stored key.
 * MARS_LAB_OBJECT_STORE_PATH is normally loaded from mars.db. Leading ~/ is expanded without a shell. Missing
 * cache parents are created privately; existing explicitly configured shared
 * parents are not chmodded. Success exports MARS_LAB_OBJECT_STORE_PATH and KEY
 * for child processes. The function changes process environment, so it must not
 * run concurrently with other threads. Separate processes serialise secret
 * creation through the settings database. No plaintext sidecar or lock file is used.
 * @return True on success; false on path, allocation, configuration or I/O failure.
 * No returned allocation requires release and no secret is printed.
 */
bool lab_runtime_prepare(void);

#endif
