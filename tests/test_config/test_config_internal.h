#ifndef TEST_CONFIG_INTERNAL_H
#define TEST_CONFIG_INTERNAL_H

#if !defined(MARS_TEST_CONFIG_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || \
     (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "test_config_internal.h is private to the test-config implementation; include test_config.h instead."
#endif

#include <stdbool.h>

#include "json.h"
#include "test_config.h"
#include "ustring.h"

string_t *test_config_compute_global_path(void);
string_t *test_config_compute_local_path(const string_t *file);
string_t *test_config_normalise_file_path(const string_t *file);
string_t *test_config_flattened_file_path(const string_t *file);

bool test_config_json_object_set_key(json_t *object,
                                     const string_t *key,
                                     const json_t *value);
bool test_config_json_object_set_literal(json_t *object,
                                         const char *key_text,
                                         const json_t *value);
const json_t *test_config_json_object_get_literal(const json_t *object,
                                                  const char *key_text);
bool test_config_json_bool_or_default(const json_t *json, bool fallback);
bool test_config_value_enabled(const json_t *value, bool fallback);
bool test_config_root_shape_is_supported(const json_t *root,
                                         test_config_mode_t mode);
json_t *test_config_create_pruned_json_object(const json_t *actual,
                                              const json_t *seen,
                                              bool file_level);

void test_config_ensure_leaf(json_t *object, const string_t *name);
json_t *test_config_ensure_group_path(json_t *object, const string_t *path);
bool test_config_find_group_with_effective_enabled(json_t *object,
                                                   const string_t *path,
                                                   json_t **out_group,
                                                   bool *out_enabled);
bool test_config_is_valid_group_name(const string_t *name);

#endif /* TEST_CONFIG_INTERNAL_H */
