/**
 * @file tba_support.h
 * @brief Shared value conversion and bounded file storage for the native forecasting Lab.
 *
 * Tools-only helpers built on MARS strings, JSON and files. JSON getters borrow
 * values; readers and escaping helpers return owned objects. No shell expansion
 * or Python runtime is used. Callers report errors instead of discarding them.
 */
#ifndef TBA_SUPPORT_H
#define TBA_SUPPORT_H
#include "file.h"
#include "json.h"
#include "ustring.h"

/** @brief Borrow a member. @param object Borrowed object. @param key UTF-8 key. @return Borrowed value or NULL. */
const json_t *tba_json_get(const json_t *object, const char *key);
/** @brief Borrow scalar text. @param object Borrowed object. @param key Key. @return String/number text or empty literal. */
const char *tba_json_text(const json_t *object, const char *key);
/** @brief Read a boolean. @param object Borrowed object. @param key Key. @return False for missing/invalid values. */
bool tba_json_bool(const json_t *object, const char *key);
/** @brief Copy a member. @param object Destination. @param key Key. @param value Borrowed value. @return Success. */
bool tba_json_set(json_t *object, const char *key, const json_t *value);
/** @brief Copy string text. @param object Destination. @param key Key. @param text Borrowed text. @return Success. */
bool tba_json_string(json_t *object, const char *key, const char *text);
/** @brief Store a boolean. @param object Destination. @param key Key. @param value Boolean. @return Success. */
bool tba_json_flag(json_t *object, const char *key, bool value);
/** @brief Store a finite number. @param object Destination. @param key Key. @param value Number. @return Success. */
bool tba_json_number(json_t *object, const char *key, double value);
/** @brief Append text. @param array Destination. @param text Borrowed text. @return Success. */
bool tba_json_append(json_t *array, const char *text);
/** @brief Build a failure response. @param message Borrowed diagnostic. @return Owned JSON, or NULL. */
json_t *tba_json_error(const char *message);
/** @brief Parse a finite decimal. @param text Borrowed string. @param value Required destination. @return Valid whole input. */
bool tba_text_number(const string_t *text, double *value);
/** @brief Escape HTML text/attributes. @param text Borrowed text. @return Owned string, or NULL. */
string_t *tba_text_html(const char *text);
/**
 * @brief Open private storage in a single-threaded request worker.
 * @param file Borrowed closed file handle.
 * @param mode CREATE_NEW for exclusive creation or OPEN_OR_CREATE for a lock file.
 * @param access Requested read/write access.
 * @return Success; the caller owns the resulting open handle. Restores the process umask before returning.
 */
bool tba_file_open_private(file_t *file, file_mode_t mode, file_access_t access);
/** @brief Read a bounded regular file. @param path Borrowed path. @param limit Byte limit. @return Owned text, or NULL. */
string_t *tba_file_read(const char *path, size_t limit);
/** @brief Resolve an asset below this tool. @param name Relative asset name. @return Owned path, or NULL. */
string_t *tba_asset_path(const char *name);
/**
 * @brief Read a bounded, version-one Protobuf object with no JSON fallback.
 * @param path Borrowed regular-file path.
 * @return Owned value tree, or NULL for missing, malformed or oversized data; release with json_free.
 */
json_t *tba_file_read_message(const char *path);

/**
 * @brief Atomically publish a Protobuf object with mode 0600.
 * @param path Borrowed destination path; its parent must exist.
 * @param value Borrowed object tree, subject to the shared 4 MiB Protobuf limits.
 * @return Success. Encoding failure leaves any existing destination unchanged.
 */
bool tba_file_publish_message(const char *path, const json_t *value);

/** @brief Resolve a repository path without shell expansion. @param path Borrowed path. @return Owned absolute path. */
string_t *tba_path(const char *path);
#endif
