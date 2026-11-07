/**
 * @file cfg_storage.h
 * @brief Private paths, option text and destination safety for native installers.
 *
 * Installer implementation files use these helpers. They resolve the user's
 * MARS directory and verify database destinations; application callers use
 * cfg_database.h and cfg_weather.h instead. Returned objects are caller-owned.
 */
#ifndef MARS_CONFIG_STORAGE_H
#define MARS_CONFIG_STORAGE_H

#include "file-t.h"

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


#endif
