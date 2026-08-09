/* test_config_tree.c - hierarchical test configuration tree helpers */

#define MARS_TEST_CONFIG_INTERNAL_ACCESS
#include "test_config_internal.h"

static bool test_config_ascii_is_alpha_or_underscore(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static bool test_config_ascii_is_name_char(char ch)
{
    return test_config_ascii_is_alpha_or_underscore(ch) || (ch >= '0' && ch <= '9');
}

static bool test_config_cursor_peek_ascii(const string_cursor_t *cursor, char *out)
{
    unsigned char ch = 0u;

    if (!string_cursor_peek_ascii(cursor, &ch))
        return false;
    if (out)
        *out = (char)ch;
    return true;
}

static bool test_config_cursor_at_dot(const string_cursor_t *cursor)
{
    char ch = '\0';

    return test_config_cursor_peek_ascii(cursor, &ch) && ch == '.';
}

static string_t *test_config_cursor_next_segment(string_cursor_t *cursor)
{
    string_pos_t start;

    if (!cursor || string_cursor_done(cursor))
        return NULL;

    start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor) && !test_config_cursor_at_dot(cursor))
        (void)string_cursor_next(cursor);

    return string_cursor_extract(start, cursor);
}

void test_config_ensure_leaf(json_t *object, const string_t *name)
{
    json_t *leaf;

    if (!object || !name || json_object_get(object, name))
        return;

    leaf = json_new_bool(true);
    (void)test_config_json_object_set_key(object, name, leaf);
    json_free(leaf);
}

static json_t *test_config_ensure_group(json_t *object, const string_t *name)
{
    json_t *existing;
    bool enabled = true;
    json_t *group;
    json_t *enabled_value;

    if (!object || !name)
        return NULL;

    existing = json_object_get_mutable(object, name);
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
        !test_config_json_object_set_key(object, name, group)) {
        json_free(enabled_value);
        json_free(group);
        return NULL;
    }

    json_free(enabled_value);
    json_free(group);
    return json_object_get_mutable(object, name);
}

json_t *test_config_ensure_group_path(json_t *object, const string_t *path)
{
    string_cursor_t *cursor;
    json_t *current = object;

    if (!object || !path || string_length(path) == 0u)
        return NULL;

    cursor = string_cursor_new(path);
    if (!cursor)
        return NULL;

    while (current && !string_cursor_done(cursor)) {
        string_t *segment = test_config_cursor_next_segment(cursor);

        if (!segment || string_length(segment) == 0u) {
            string_free(segment);
            string_cursor_free(cursor);
            return NULL;
        }

        current = test_config_ensure_group(current, segment);
        string_free(segment);

        if (test_config_cursor_at_dot(cursor))
            (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return current;
}

bool test_config_find_group_with_effective_enabled(json_t *object, const string_t *path, json_t **out_group,
                                                   bool *out_enabled)
{
    string_cursor_t *cursor;
    json_t *current = object;
    bool enabled = true;

    if (!object || !path || string_length(path) == 0u)
        return false;

    cursor = string_cursor_new(path);
    if (!cursor)
        return false;

    while (current && !string_cursor_done(cursor)) {
        string_t *segment = test_config_cursor_next_segment(cursor);
        bool has_more;
        json_t *child;

        if (!segment || string_length(segment) == 0u) {
            string_free(segment);
            string_cursor_free(cursor);
            return false;
        }

        child = json_object_get_mutable(current, segment);
        string_free(segment);
        if (!child) {
            string_cursor_free(cursor);
            return false;
        }

        enabled = enabled && test_config_value_enabled(child, true);
        has_more = test_config_cursor_at_dot(cursor);
        if (!has_more) {
            if (json_type(child) != JSON_OBJECT) {
                string_cursor_free(cursor);
                return false;
            }
            if (out_group)
                *out_group = child;
            if (out_enabled)
                *out_enabled = enabled;
            string_cursor_free(cursor);
            return true;
        }

        if (json_type(child) != JSON_OBJECT) {
            string_cursor_free(cursor);
            return false;
        }
        current = child;
        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return false;
}

bool test_config_is_valid_group_name(const string_t *name)
{
    string_cursor_t *cursor;
    bool expecting_segment_start = true;
    bool saw_segment = false;

    if (!name || string_length(name) == 0u)
        return false;

    cursor = string_cursor_new(name);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        char ch = '\0';

        if (!test_config_cursor_peek_ascii(cursor, &ch)) {
            string_cursor_free(cursor);
            return false;
        }

        if (expecting_segment_start) {
            if (!test_config_ascii_is_alpha_or_underscore(ch)) {
                string_cursor_free(cursor);
                return false;
            }
            expecting_segment_start = false;
            saw_segment = true;
        } else if (ch == '.') {
            expecting_segment_start = true;
        } else if (!test_config_ascii_is_name_char(ch)) {
            string_cursor_free(cursor);
            return false;
        }

        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return saw_segment && !expecting_segment_start;
}
