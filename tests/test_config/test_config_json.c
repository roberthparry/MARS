/* test_config_json.c - JSON helpers for test configuration */

#define MARS_TEST_CONFIG_INTERNAL_ACCESS
#include "test_config_internal.h"

bool test_config_json_object_set_key(json_t *object,
                                     const string_t *key,
                                     const json_t *value)
{
    if (!object || !key || !value)
        return false;

    return json_object_set(object, key, value);
}

bool test_config_json_object_set_literal(json_t *object,
                                         const char *key_text,
                                         const json_t *value)
{
    string_t *key;
    bool ok;

    if (!key_text)
        return false;

    key = string_new_with(key_text);
    if (!key)
        return false;

    ok = test_config_json_object_set_key(object, key, value);
    string_free(key);
    return ok;
}

const json_t *test_config_json_object_get_literal(const json_t *object,
                                                  const char *key_text)
{
    string_t *key;
    const json_t *value;

    if (!object || !key_text)
        return NULL;

    key = string_new_with(key_text);
    if (!key)
        return NULL;

    value = json_object_get(object, key);
    string_free(key);
    return value;
}

bool test_config_json_bool_or_default(const json_t *json, bool fallback)
{
    bool enabled;

    if (json && json_type(json) == JSON_BOOL && json_bool_value(json, &enabled))
        return enabled;
    return fallback;
}

bool test_config_value_enabled(const json_t *value, bool fallback)
{
    const json_t *enabled;

    if (!value)
        return fallback;
    if (json_type(value) == JSON_BOOL)
        return test_config_json_bool_or_default(value, fallback);
    if (json_type(value) != JSON_OBJECT)
        return fallback;

    enabled = test_config_json_object_get_literal(value, "enabled");
    return test_config_json_bool_or_default(enabled, true);
}

static bool test_config_json_shape_is_supported(const json_t *json, bool file_level)
{
    size_t count;

    if (!json || json_type(json) != JSON_OBJECT)
        return false;

    count = json_object_size(json);
    for (size_t i = 0u; i < count; ++i) {
        const string_t *key = json_object_key_at(json, i);
        const json_t *value = json_object_value_at(json, i);

        if (!key || !value)
            return false;
        if (!file_level &&
            string_view_equals_literal(string_view_all(key), "enabled")) {
            if (json_type(value) != JSON_BOOL)
                return false;
            continue;
        }
        if (json_type(value) == JSON_BOOL)
            continue;
        if (json_type(value) == JSON_OBJECT &&
            test_config_json_shape_is_supported(value, false))
            continue;
        return false;
    }

    return true;
}

bool test_config_root_shape_is_supported(const json_t *root,
                                         test_config_mode_t mode)
{
    if (!root || json_type(root) != JSON_OBJECT)
        return false;

    if (mode == TEST_CONFIG_LOCAL)
        return test_config_json_shape_is_supported(root, true);
    if (mode != TEST_CONFIG_GLOBAL)
        return false;

    for (size_t i = 0u; i < json_object_size(root); ++i) {
        const json_t *file_object = json_object_value_at(root, i);

        if (!test_config_json_shape_is_supported(file_object, true))
            return false;
    }

    return true;
}

json_t *test_config_create_pruned_json_object(const json_t *actual,
                                              const json_t *seen,
                                              bool file_level)
{
    json_t *pruned;

    if (!seen || json_type(seen) != JSON_OBJECT)
        return json_new_object();

    pruned = json_new_object();
    if (!pruned)
        return NULL;

    if (!file_level) {
        bool enabled = test_config_value_enabled(actual, true);
        json_t *enabled_value = json_new_bool(enabled);

        if (!enabled_value)
            goto fail;
        if (!test_config_json_object_set_literal(pruned, "enabled", enabled_value)) {
            json_free(enabled_value);
            goto fail;
        }
        json_free(enabled_value);
    }

    for (size_t i = 0u; i < json_object_size(seen); ++i) {
        const string_t *key = json_object_key_at(seen, i);
        const json_t *seen_value = json_object_value_at(seen, i);
        const json_t *actual_value;
        json_t *preserved;

        if (!key || !seen_value ||
            string_view_equals_literal(string_view_all(key), "enabled"))
            continue;

        actual_value = actual && json_type(actual) == JSON_OBJECT
            ? json_object_get(actual, key)
            : NULL;

        if (json_type(seen_value) == JSON_OBJECT) {
            preserved = test_config_create_pruned_json_object(
                (actual_value && json_type(actual_value) == JSON_OBJECT)
                    ? actual_value
                    : NULL,
                seen_value,
                false);
        } else {
            bool enabled = actual_value
                ? test_config_value_enabled(
                    actual_value,
                    test_config_json_bool_or_default(seen_value, true))
                : test_config_json_bool_or_default(seen_value, true);

            preserved = json_new_bool(enabled);
        }

        if (!test_config_json_object_set_key(pruned, key, preserved)) {
            json_free(preserved);
            goto fail;
        }
        json_free(preserved);
    }

    return pruned;

fail:
    json_free(pruned);
    return NULL;
}
