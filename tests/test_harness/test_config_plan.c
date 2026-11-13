/**
 * @file test_config_plan.c
 * @brief Count selected module cases from an immutable JSON configuration snapshot.
 *
 * Builds hash indexes for parent paths and disabled groups across translation units. Counting visits each
 * configured entry once after indexing; no registration function, case fixture or SQLite connection is invoked.
 * Missing configuration leaves progress indeterminate until a later run has learnt the module's cases.
 */
#include <stdint.h>
#include <string.h>

#include "string-t.h"
#include "array-t.h"
#include "test_config.h"
#define MARS_TEST_CONFIG_INTERNAL_ACCESS
#include "test_config_internal.h"

typedef struct {
    string_t *path;
    bool enabled;
} test_plan_entry_t;

static void test_plan_entry_free(void *value)
{
    string_free(((test_plan_entry_t *)value)->path);
}

static bool test_plan_mark(json_t *index, const string_t *path)
{
    json_t *yes = json_new_bool(true);
    bool ok = yes && json_object_set(index, path, yes);
    json_free(yes);
    return ok;
}

static bool test_plan_collect(const json_t *node, const string_t *source, const string_t *path, bool enabled,
                             test_config_selection_fn selection, array_t *entries, json_t *parents, unsigned depth)
{
    if (depth > 128 || !node || (json_type(node) != JSON_OBJECT && json_type(node) != JSON_BOOL)) return false;
    bool effective = enabled && test_config_value_enabled(node, true);
    if (path) {
        test_plan_entry_t entry = {.path = string_clone(path),
            .enabled = selection ? selection(source, path, effective) : effective};
        if (!entry.path || !array_add(entries, &entry)) { string_free(entry.path); return false; }
    }
    if (json_type(node) != JSON_OBJECT) return true;
    for (size_t i = 0; i < json_object_size(node); ++i) {
        const string_t *key = json_object_key_at(node, i);
        if (string_view_equals_literal(string_view_all(key), "enabled")) continue;
        string_t *child = path ? string_sprintf("%S.%S", path, key) : string_clone(key);
        bool ok = child && (!path || test_plan_mark(parents, path)) &&
            test_plan_collect(json_object_value_at(node, i), source, child, effective, selection,
                              entries, parents, depth + 1);
        string_free(child);
        if (!ok) return false;
    }
    return true;
}

static bool test_plan_blocked(const json_t *blocked, const string_t *path, bool *valid)
{
    const char *text = string_c_str(path);
    for (const char *dot = strchr(text, '.'); dot; dot = strchr(dot + 1, '.')) {
        string_t *parent = string_new();
        bool copied = parent && !string_append_utf8_exact(parent, text, (size_t)(dot - text));
        if (!copied) *valid = false;
        bool excluded = !copied || json_object_get(blocked, parent) != NULL;
        string_free(parent);
        if (excluded) return true;
    }
    return false;
}

/* Count semantic leaves rather than treating group wrappers as independently completed tests. */
bool test_config_count_enabled(const json_t *configuration, const string_t *suite_source, test_config_mode_t mode,
                               test_config_selection_fn selection, size_t *count)
{
    if (count) *count = 0;
    if (!configuration || json_type(configuration) != JSON_OBJECT || !suite_source || !count ||
        (mode != TEST_CONFIG_GLOBAL && mode != TEST_CONFIG_LOCAL)) return false;
    string_t *source = test_config_normalise_file_path(suite_source);
    const char *name = source ? string_c_str(source) : NULL;
    const char *slash = name ? strrchr(name, '/') : NULL;
    size_t prefix = slash ? (size_t)(slash - name) + 1 : 0;
    array_t *entries = array_create(sizeof(test_plan_entry_t), NULL, test_plan_entry_free);
    json_t *parents = json_new_object(), *blocked = json_new_object();
    bool present = false, ok = source && entries && parents && blocked;
    if (ok && mode == TEST_CONFIG_LOCAL) {
        present = true;
        ok = test_plan_collect(configuration, source, NULL, true, selection, entries, parents, 0);
    } else {
        for (size_t i = 0; ok && i < json_object_size(configuration); ++i) {
            const string_t *file = json_object_key_at(configuration, i);
            const char *text = string_c_str(file);
            if (!prefix || strncmp(text, name, prefix) || strchr(text + prefix, '/')) continue;
            present = true;
            const json_t *node = json_object_value_at(configuration, i);
            ok = json_type(node) == JSON_OBJECT && test_plan_collect(node, file, NULL, true,
                                   selection, entries, parents, 0);
        }
    }
    for (size_t i = 0; ok && i < array_size(entries); ++i) {
        const test_plan_entry_t *entry = array_get(entries, i);
        if (!entry->enabled && strchr(string_c_str(entry->path), '.') && json_object_get(parents, entry->path))
            ok = test_plan_mark(blocked, entry->path);
    }
    size_t total = 0;
    for (size_t i = 0; ok && i < array_size(entries); ++i) {
        const test_plan_entry_t *entry = array_get(entries, i);
        if (entry->enabled && !json_object_get(parents, entry->path) &&
            !test_plan_blocked(blocked, entry->path, &ok) && ok) {
            if (total == SIZE_MAX) ok = false;
            else ++total;
        }
    }
    if (ok && present) *count = total;
    array_destroy(entries);
    json_free(blocked); json_free(parents); string_free(source);
    return ok && present;
}
