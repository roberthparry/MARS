/* test_config_tree.c - hierarchical test configuration tree helpers */

#include <string.h>

#include "test_config_internal.h"

static string_t *test_config_string_from_range(const char *start, size_t len)
{
    string_t *s = string_new();

    if (!s)
        return NULL;
    if (string_append_chars(s, start, len) != 0) {
        string_free(s);
        return NULL;
    }
    return s;
}

const json_t *test_config_json_object_get_text(const json_t *object,
                                               const char *name)
{
    string_t *key;
    const json_t *value;

    key = string_new_with(name);
    if (!key)
        return NULL;
    value = json_object_get(object, key);
    string_free(key);
    return value;
}

json_t *test_config_json_object_get_text_mutable(json_t *object,
                                                 const char *name)
{
    string_t *key;
    json_t *value;

    key = string_new_with(name);
    if (!key)
        return NULL;
    value = json_object_get_mutable(object, key);
    string_free(key);
    return value;
}

void test_config_ensure_leaf(json_t *object, const char *name)
{
    json_t *leaf;

    if (!object || !name || test_config_json_object_get_text(object, name))
        return;

    leaf = json_new_bool(true);
    (void)test_config_json_object_set_literal(object, name, leaf);
    json_free(leaf);
}

static json_t *test_config_ensure_group(json_t *object, const char *name)
{
    json_t *existing;
    bool enabled = true;
    json_t *group;
    json_t *enabled_value;

    if (!object || !name)
        return NULL;

    existing = test_config_json_object_get_text_mutable(object, name);
    if (existing && json_type(existing) == JSON_OBJECT)
        return existing;

    if (existing)
        enabled = test_config_value_enabled(existing, true);

    group = json_new_object();
    if (!group)
        return NULL;

    enabled_value = json_new_bool(enabled);
    if (!enabled_value) {
        json_free(group);
        return NULL;
    }

    if (!test_config_json_object_set_literal(group, "enabled", enabled_value) ||
        !test_config_json_object_set_literal(object, name, group)) {
        json_free(enabled_value);
        json_free(group);
        return NULL;
    }

    json_free(enabled_value);
    json_free(group);
    return test_config_json_object_get_text_mutable(object, name);
}

json_t *test_config_ensure_group_path(json_t *object, const char *path)
{
    const char *start = path;
    json_t *current = object;

    if (!object || !path || !*path)
        return NULL;

    while (current && start && *start) {
        const char *dot = strchr(start, '.');
        size_t len = dot ? (size_t)(dot - start) : strlen(start);
        string_t *segment = test_config_string_from_range(start, len);

        if (!segment)
            return NULL;
        current = test_config_ensure_group(current, string_c_str(segment));
        string_free(segment);
        start = dot ? dot + 1 : NULL;
    }

    return current;
}

bool test_config_find_group_with_effective_enabled(json_t *object,
                                                   const char *path,
                                                   json_t **out_group,
                                                   bool *out_enabled)
{
    const char *start = path;
    json_t *current = object;
    bool enabled = true;

    if (!object || !path || !*path)
        return false;

    while (current && start && *start) {
        const char *dot = strchr(start, '.');
        size_t len = dot ? (size_t)(dot - start) : strlen(start);
        string_t *segment = test_config_string_from_range(start, len);
        json_t *child;

        if (!segment)
            return false;

        child = json_object_get_mutable(current, segment);
        string_free(segment);
        if (!child)
            return false;

        enabled = enabled && test_config_value_enabled(child, true);
        if (!dot) {
            if (json_type(child) != JSON_OBJECT)
                return false;
            if (out_group)
                *out_group = child;
            if (out_enabled)
                *out_enabled = enabled;
            return true;
        }

        if (json_type(child) != JSON_OBJECT)
            return false;
        current = child;
        start = dot + 1;
    }

    return false;
}

bool test_config_is_valid_group_name(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;

    if (!p || !*p)
        return false;

    while (*p) {
        if (!((*p >= 'A' && *p <= 'Z') ||
              (*p >= 'a' && *p <= 'z') ||
              (*p == '_')))
            return false;

        ++p;
        while (*p && *p != '.') {
            if (!((*p >= 'A' && *p <= 'Z') ||
                  (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') ||
                  (*p == '_')))
                return false;
            ++p;
        }

        if (*p == '.') {
            ++p;
            if (!*p)
                return false;
        }
    }

    return true;
}
