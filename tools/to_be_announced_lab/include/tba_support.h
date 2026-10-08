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
/** @brief Escape JSON for a script data block. @param value Borrowed JSON. @return Owned string, or NULL. */
string_t *tba_text_script(const json_t *value);
/** @brief Read a bounded regular file. @param path Borrowed path. @param limit Byte limit. @return Owned text, or NULL. */
string_t *tba_file_read(const char *path, size_t limit);
/** @brief Resolve an asset below this tool. @param name Relative asset name. @return Owned path, or NULL. */
string_t *tba_asset_path(const char *name);
/** @brief Atomically publish text, mode 0600. @param path Destination. @param text Borrowed text. @return Success. */
bool tba_file_publish(const char *path, const string_t *text);
/** @brief Resolve a repository path without shell expansion. @param path Borrowed path. @return Owned absolute path. */
string_t *tba_path(const char *path);
#endif
