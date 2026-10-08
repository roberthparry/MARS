/**
 * @file cfg_storage.h
 * @brief Private paths, literal settings and atomic publication for native installers.
 *
 * Installer implementation files use these helpers. They resolve the user's
 * MARS directory and protect secret configuration files; application callers use
 * cfg_weather.h instead. Returned objects are caller-owned.
 */
#ifndef MARS_CONFIG_STORAGE_H
#define MARS_CONFIG_STORAGE_H

#include "file.h"

/**
 * @brief Resolve MARS_HOME, including ~/ expansion, or the user's ~/.mars directory.
 * @return Owned path string, or NULL on failure; release with string_free.
 */
string_t *cfg_storage_home(void);

/**
 * @brief Create a private directory without following symbolic-link components.
 * @param[in] path Borrowed directory path.
 * @param[in] tighten_existing Require current-user ownership and set mode 0700 when true.
 * @return True on success, false with errno on failure.
 */
bool cfg_storage_private_directory(const string_t *path, bool tighten_existing);

/**
 * @brief Check that a destination is absent or a user-owned, single-link regular file.
 * @param[in,out] file Borrowed closed file handle.
 * @param[out] exists Required destination for whether the pathname exists.
 * @return True for an acceptable destination; false on error or unsafe metadata.
 */
bool cfg_storage_safe_regular(file_t *file, bool *exists);

/**
 * @brief Read at most 1 MiB of UTF-8 configuration, requiring a safe regular file.
 * @param[in,out] file Borrowed closed handle; an existing file is made private (0600).
 * @return Owned text, empty if absent, or NULL on failure; release with string_free.
 */
string_t *cfg_storage_read_configuration(file_t *file);

/**
 * @brief Expand ~/ or ~ in a path without shell evaluation.
 * @param[in] path Borrowed path string.
 * @return Owned expanded copy, or NULL on error.
 */
string_t *cfg_storage_expand_path(const string_t *path);

/**
 * @brief Return the directory containing a path.
 * @param[in] path Borrowed path string.
 * @return Owned parent path, or NULL on allocation failure.
 */
string_t *cfg_storage_parent(const string_t *path);

/**
 * @brief Copy a C string and trim outer whitespace.
 * @param[in] text Optional borrowed text; NULL produces an empty string.
 * @return Owned string, or NULL on allocation failure.
 */
string_t *cfg_storage_value(const char *text);

/**
 * @brief Read a literal environment assignment without executing substitutions.
 * @param[in,out] file Borrowed closed configuration handle; missing files are allowed.
 * @param[in] name Borrowed assignment name to find.
 * @return Owned value, empty when absent, or NULL for malformed/conflicting assignments or I/O failure.
 */
string_t *cfg_storage_setting(file_t *file, const char *name);

/**
 * @brief Quote a single-line value for a literal shell export.
 * @param[in] value Borrowed text; NUL, CR and LF are rejected.
 * @return Owned quoted text, or NULL on invalid input/allocation failure.
 */
string_t *cfg_storage_quote(const string_t *value);

/**
 * @brief Publish a complete private configuration file atomically.
 * @param[in,out] destination Borrowed closed destination handle; only safe regular files may be replaced.
 * @param[in] directory Borrowed parent directory, created privately if needed.
 * @param[in] body Borrowed complete UTF-8 text to write.
 * @return True after synced publication, false on error; no partial file is published.
 */
bool cfg_storage_publish(file_t *destination, const string_t *directory, const string_t *body);

#endif
