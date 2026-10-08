/**
 * @file lab_runtime.h
 * @brief Private pre-fork cache configuration for the native MARS Lab launcher.
 *
 * Resolves the existing Lab object-store path and secret, retaining Python-era
 * configuration. Missing secrets are generated once under an advisory lock and
 * published atomically. The cache database itself is never opened or replaced.
 * Call only in the single-threaded parent before starting request workers.
 */
#ifndef MARS_LAB_RUNTIME_H
#define MARS_LAB_RUNTIME_H

#include <stdbool.h>

/**
 * @brief Prepare the inherited object-store environment without logging secrets.
 *
 * A non-empty MARS_LAB_OBJECT_STORE_KEY overrides the key in
 * MARS_HOME/config/mars-lab.env. MARS_HOME defaults to ~/.mars. A missing key is
 * generated with libsodium and stored in a mode-0600 configuration file under a
 * mode-0700 directory, preserving unrelated configuration lines. Existing keys
 * are never rotated. Conflicting duplicate keys, unreadable or oversized files,
 * symlinks and unsafe configuration ownership cause failure, not regeneration.
 *
 * MARS_LAB_OBJECT_STORE_PATH takes precedence over MARS_LAB_CACHE_FILE; relative
 * CACHE_FILE values are resolved below MARS_HOME/lab, whose default cache is
 * mars_lab_object_store.sqlite3. Leading ~/ is expanded without a shell. Missing
 * cache parents are created privately; existing explicitly configured shared
 * parents are not chmodded. Success exports MARS_LAB_OBJECT_STORE_PATH and KEY
 * for child processes. The function changes environment and temporarily umask,
 * so it must not run concurrently with other threads. Separate processes are
 * serialised by a stable configuration lock file that is never unlinked.
 * @return True on success; false with errno set. No secret is printed.
 */
bool lab_runtime_prepare(void);

#endif
