/**
 * @file test_config_internal.h
 * @brief Private configuration-tree and persistence helpers for the test harness.
 *
 * Shared by configuration implementation units and authorised white-box tests.
 * Ordinary test suites use test_config.h instead of these internal operations.
 */
#ifndef TEST_CONFIG_INTERNAL_H
#define TEST_CONFIG_INTERNAL_H

#if !defined(MARS_TEST_CONFIG_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TEST_CONFIG_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

#include "json-t.h"
#include "string-t.h"
#include "test_config.h"
#define MARS_SHARED_TEST_CONFIG_INTERNAL_ACCESS
#include "internal/test_config_internal.h"

/**
 * @brief Locate the shared selection JSON relative to the repository.
 * @return Owned string released with string_free, or NULL on allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
string_t *test_config_compute_global_path(void);
/**
 * @brief Derive a source-local JSON path by replacing the source extension.
 * @param[in] file Caller-owned source path, not consumed.
 * @return Owned string released with string_free, or NULL on allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
string_t *test_config_compute_local_path(const string_t *file);
/**
 * @brief Derive the historical flat key used when reading existing selections.
 * @param[in] file Caller-owned source path, not consumed.
 * @return Owned string released with string_free, or NULL on allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
string_t *test_config_flattened_file_path(const string_t *file);

/**
 * @brief Copy a selection value into an object using a string key.
 * @param[in] object Caller-owned destination/tree; mutations are limited to setter and ensure functions.
 * @param[in] key Caller-owned key, copied during insertion.
 * @param[in] value Caller-owned value, copied by setters; never consumed.
 * @return True for a valid/successful operation, false for invalid/missing input or allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
bool test_config_json_object_set_key(json_t *object, const string_t *key, const json_t *value);
/**
 * @brief Copy a selection value into an object using a C-string key.
 * @param[in] object Caller-owned destination/tree; mutations are limited to setter and ensure functions.
 * @param[in] key_text Caller-owned NUL-terminated key, valid during the call.
 * @param[in] value Caller-owned value, copied by setters; never consumed.
 * @return True for a valid/successful operation, false for invalid/missing input or allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
bool test_config_json_object_set_literal(json_t *object, const char *key_text, const json_t *value);
/**
 * @brief Find a selection value using a C-string key.
 * @param[in] object Caller-owned destination/tree; mutations are limited to setter and ensure functions.
 * @param[in] key_text Caller-owned NUL-terminated key, valid during the call.
 * @return Tree-owned child, valid until tree mutation/release; do not free it. NULL means absent/invalid or allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
const json_t *test_config_json_object_get_literal(const json_t *object, const char *key_text);
/**
 * @brief Read a Boolean selection or return the supplied fallback.
 * @param[in] json Caller-owned JSON value, or NULL.
 * @param[in] fallback State to use when the value is absent or unsupported.
 * @return The Boolean value, or fallback for absent/non-Boolean input.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
bool test_config_json_bool_or_default(const json_t *json, bool fallback);
/**
 * @brief Resolve a leaf or group's enabled state.
 * @param[in] value Caller-owned value, copied by setters; never consumed.
 * @param[in] fallback State to use when the value is absent or unsupported.
 * @return Effective enabled state; an object without an enabled member defaults to true.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
bool test_config_value_enabled(const json_t *value, bool fallback);
/**
 * @brief Validate the complete configuration tree for the selected scope.
 * @param[in] root Caller-owned immutable JSON root.
 * @param[in] mode GLOBAL or LOCAL shape; NONE is unsupported.
 * @return True for a valid/successful operation, false for invalid/missing input or allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
bool test_config_root_shape_is_supported(const json_t *root, test_config_mode_t mode);
/**
 * @brief Copy active selection keys into a pruned configuration tree.
 * @param[in] got Caller-owned previous configuration, not modified.
 * @param[in] seen Caller-owned discovered-case tree, not modified.
 * @param[in] file_level True applies source-level handling rather than nested-group handling.
 * @return Owned JSON tree released with json_free, or NULL on allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
json_t *test_config_create_pruned_json_object(const json_t *got, const json_t *seen, bool file_level);

/**
 * @brief Materialise a missing enabled leaf without replacing an existing selection.
 * @param[in] object Caller-owned destination/tree; mutations are limited to setter and ensure functions.
 * @param[in] name Caller-owned candidate name, copied when inserted.
 * @return No value; invalid input or allocation failure does not replace existing selections.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
void test_config_ensure_leaf(json_t *object, const string_t *name);
/**
 * @brief Materialise a dotted group path while preserving existing enabled states.
 * @param[in] object Caller-owned destination/tree; mutations are limited to setter and ensure functions.
 * @param[in] path Caller-owned dotted group path, valid during the call.
 * @return Tree-owned child, valid until tree mutation/release; do not free it. NULL means absent/invalid or allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
json_t *test_config_ensure_group_path(json_t *object, const string_t *path);
/**
 * @brief Resolve an existing group and its inherited enabled state.
 * @param[in] object Caller-owned destination/tree; mutations are limited to setter and ensure functions.
 * @param[in] path Caller-owned dotted group path, valid during the call.
 * @param[out] out_group Optional output receiving a tree-owned child, valid until tree mutation/release; do not free it.
 * @param[out] out_enabled Optional output receiving the inherited enabled state.
 * @return True for a valid/successful operation, false for invalid/missing input or allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
bool test_config_find_group_with_effective_enabled(json_t *object, const string_t *path, json_t **out_group,
                                                   bool *out_enabled);
/**
 * @brief Validate a non-empty identifier or dotted identifier path.
 * @param[in] name Caller-owned candidate name, copied when inserted.
 * @return True for a valid/successful operation, false for invalid/missing input or allocation failure.
 * @note Private sequential harness facility; serialise access with mutations of the same tree.
 */
bool test_config_is_valid_group_name(const string_t *name);

#endif /* TEST_CONFIG_INTERNAL_H */
