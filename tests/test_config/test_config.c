/* test_config.c - hierarchical test configuration lifecycle and queries */

#include <stdio.h>
#include <sys/stat.h>

#include "test_config_internal.h"

static json_t *g_root = NULL;
static json_t *g_seen_root = NULL;
static int g_mode = TEST_CONFIG_MODE;
static bool g_prune_enabled = true;
static bool g_loaded = false;
static string_t *g_local_filename = NULL;

static void ensure_root_created(void)
{
    if (!g_root && g_mode != TEST_CONFIG_NONE)
        g_root = json_new_object();
}

static void ensure_seen_root_created(void)
{
    if (!g_seen_root && g_mode != TEST_CONFIG_NONE)
        g_seen_root = json_new_object();
}

static json_t *ensure_file_object(const char *file)
{
    string_t *normalised_key;
    string_t *flattened_key;
    json_t *file_object;

    if (g_mode == TEST_CONFIG_NONE)
        return NULL;

    ensure_root_created();
    if (!g_root)
        return NULL;

    if (g_mode == TEST_CONFIG_LOCAL)
        return g_root;

    normalised_key = test_config_normalise_file_path(file);
    flattened_key = test_config_flattened_file_path(file);
    if (!normalised_key || !flattened_key) {
        string_free(normalised_key);
        string_free(flattened_key);
        return NULL;
    }

    file_object = json_object_get_mutable(g_root, normalised_key);
    if (file_object && json_type(file_object) == JSON_OBJECT) {
        string_free(flattened_key);
        string_free(normalised_key);
        return file_object;
    }

    if (string_compare(normalised_key, flattened_key) != 0) {
        file_object = json_object_get_mutable(g_root, flattened_key);
        if (file_object && json_type(file_object) == JSON_OBJECT) {
            string_free(flattened_key);
            string_free(normalised_key);
            return file_object;
        }
    }

    file_object = json_new_object();
    if (file_object && test_config_json_object_set_key(g_root,
                                                       normalised_key,
                                                       file_object)) {
        json_free(file_object);
        file_object = json_object_get_mutable(g_root, normalised_key);
    } else {
        json_free(file_object);
        file_object = NULL;
    }

    string_free(flattened_key);
    string_free(normalised_key);
    return file_object;
}

static json_t *ensure_seen_file_object(const char *file)
{
    string_t *key;
    json_t *file_object;

    if (g_mode == TEST_CONFIG_NONE)
        return NULL;

    ensure_seen_root_created();
    if (!g_seen_root)
        return NULL;

    if (g_mode == TEST_CONFIG_LOCAL)
        return g_seen_root;

    key = test_config_normalise_file_path(file);
    if (!key)
        return NULL;

    file_object = json_object_get_mutable(g_seen_root, key);
    if (file_object && json_type(file_object) == JSON_OBJECT) {
        string_free(key);
        return file_object;
    }

    file_object = json_new_object();
    if (file_object && test_config_json_object_set_key(g_seen_root,
                                                       key,
                                                       file_object)) {
        json_free(file_object);
        file_object = json_object_get_mutable(g_seen_root, key);
    } else {
        json_free(file_object);
        file_object = NULL;
    }

    string_free(key);
    return file_object;
}

static void mark_seen_path(const char *file, const char *func, const char *parent)
{
    json_t *file_object;
    json_t *parent_object;

    if (g_mode == TEST_CONFIG_NONE || !file || !func)
        return;

    file_object = ensure_seen_file_object(file);
    if (!file_object)
        return;

    if (!parent || !*parent) {
        test_config_ensure_leaf(file_object, func);
        return;
    }

    parent_object = test_config_ensure_group_path(file_object, parent);
    test_config_ensure_leaf(parent_object, func);
}

static void load_json_if_needed(void)
{
    string_t *path;
    struct stat st;
    json_t *json;

    if (g_loaded)
        return;
    if (g_mode == TEST_CONFIG_NONE) {
        g_loaded = true;
        return;
    }
    if (g_mode == TEST_CONFIG_LOCAL && !g_local_filename)
        return;

    g_loaded = true;
    path = (g_mode == TEST_CONFIG_GLOBAL)
        ? test_config_compute_global_path()
        : test_config_compute_local_path(string_c_str(g_local_filename));

    if (!path)
        return;

    if (stat(string_c_str(path), &st) != 0 || st.st_size == 0) {
        string_free(path);
        return;
    }

    json = json_from_file(path);
    string_free(path);

    if (!json) {
        string_fprintf(stderr,
                       "test_config: failed to parse configuration JSON; ignoring loaded state\n");
        json_free(g_root);
        g_root = NULL;
        return;
    }

    if (!test_config_root_shape_is_supported(json, (test_config_mode_t)g_mode)) {
        string_fprintf(stderr,
                       "test_config: unsupported configuration JSON shape; ignoring loaded state\n");
        json_free(json);
        json_free(g_root);
        g_root = NULL;
        return;
    }

    json_free(g_root);
    g_root = json;
}

bool test_config_is_enabled(const string_t *file,
                            const string_t *func,
                            const string_t *parent)
{
    const char *file_text = file ? string_c_str(file) : NULL;
    const char *func_text = func ? string_c_str(func) : NULL;
    const char *parent_text = (parent && string_length(parent) > 0u)
        ? string_c_str(parent)
        : NULL;
    json_t *file_object;

    if (g_mode == TEST_CONFIG_NONE)
        return true;
    if (!file_text || !*file_text || !func_text || !*func_text)
        return true;
    if (!g_local_filename)
        g_local_filename = string_clone(file);

    load_json_if_needed();
    file_object = ensure_file_object(file_text);
    if (!file_object)
        return true;

    if (!parent_text) {
        const json_t *value;

        mark_seen_path(file_text, func_text, NULL);
        value = test_config_json_object_get_text(file_object, func_text);
        if (value)
            return test_config_value_enabled(value, true);

        test_config_ensure_leaf(file_object, func_text);
        return true;
    }

    if (!test_config_is_valid_group_name(parent_text)) {
        string_fprintf(stderr,
                       "test_config: invalid parent group path '%s' for test '%s'; use identifiers joined by '.' via test_run_subtest() or test_run_in_group()\n",
                       parent_text,
                       func_text);
        return false;
    }

    mark_seen_path(file_text, func_text, parent_text);

    {
        json_t *parent_object = NULL;
        bool parent_enabled = true;
        const json_t *child;

        if (test_config_find_group_with_effective_enabled(file_object,
                                                          parent_text,
                                                          &parent_object,
                                                          &parent_enabled) &&
            !parent_enabled)
            return false;

        if (!parent_object)
            parent_object = test_config_ensure_group_path(file_object, parent_text);
        if (!parent_object)
            return true;

        child = test_config_json_object_get_text(parent_object, func_text);
        if (child)
            return parent_enabled && test_config_value_enabled(child, true);

        test_config_ensure_leaf(parent_object, func_text);
        return true;
    }
}

bool test_config_has_key_for(const string_t *file,
                             const string_t *func,
                             const string_t *parent)
{
    const char *file_text = file ? string_c_str(file) : NULL;
    const char *func_text = func ? string_c_str(func) : NULL;
    const char *parent_text = (parent && string_length(parent) > 0u)
        ? string_c_str(parent)
        : NULL;
    json_t *file_object;

    if (g_mode == TEST_CONFIG_NONE)
        return false;
    if (!file_text || !*file_text || !func_text || !*func_text)
        return false;
    if (g_mode == TEST_CONFIG_LOCAL && !g_local_filename)
        g_local_filename = string_clone(file);

    load_json_if_needed();
    file_object = ensure_file_object(file_text);
    if (!file_object)
        return false;

    if (!parent_text)
        return test_config_json_object_get_text(file_object, func_text) != NULL;

    if (!test_config_is_valid_group_name(parent_text))
        return false;

    {
        json_t *parent_object = NULL;

        if (!test_config_find_group_with_effective_enabled(file_object,
                                                           parent_text,
                                                           &parent_object,
                                                           NULL) ||
            !parent_object)
            return false;

        return test_config_json_object_get_text(parent_object, func_text) != NULL;
    }
}

static void prune_unseen_entries(void)
{
    if (!g_prune_enabled || g_mode == TEST_CONFIG_NONE || !g_root || !g_seen_root)
        return;

    if (g_mode == TEST_CONFIG_LOCAL) {
        json_t *pruned = test_config_create_pruned_json_object(g_root,
                                                               g_seen_root,
                                                               true);

        if (pruned) {
            json_free(g_root);
            g_root = pruned;
        }
        return;
    }

    for (size_t i = 0u; i < json_object_size(g_seen_root); ++i) {
        const string_t *file_key = json_object_key_at(g_seen_root, i);
        const json_t *seen_file = json_object_value_at(g_seen_root, i);
        const json_t *actual_file;
        json_t *pruned;

        if (!file_key || !seen_file)
            continue;

        actual_file = json_object_get(g_root, file_key);
        if (!actual_file || json_type(actual_file) != JSON_OBJECT)
            continue;

        pruned = test_config_create_pruned_json_object(actual_file, seen_file, true);
        (void)test_config_json_object_set_key(g_root, file_key, pruned);
        json_free(pruned);
    }
}

void test_config_save(void)
{
    string_t *path;
    string_t *tmp;

    if (g_mode == TEST_CONFIG_NONE || !g_root)
        return;

    prune_unseen_entries();

    path = (g_mode == TEST_CONFIG_GLOBAL)
        ? test_config_compute_global_path()
        : test_config_compute_local_path(string_c_str(g_local_filename));
    tmp = path ? string_new_with(string_c_str(path)) : NULL;

    if (!tmp || string_append_cstr(tmp, ".tmp") != 0) {
        string_free(tmp);
        string_free(path);
        return;
    }

    if (json_to_file_pretty(g_root, tmp, 2) != 0) {
        string_free(tmp);
        string_free(path);
        return;
    }

    rename(string_c_str(tmp), string_c_str(path));

    string_free(tmp);
    string_free(path);
}

static void reset_state(void)
{
    json_free(g_root);
    json_free(g_seen_root);
    string_free(g_local_filename);

    g_root = NULL;
    g_seen_root = NULL;
    g_local_filename = NULL;
}

void test_config_set_prune_enabled(bool enabled)
{
    g_prune_enabled = enabled;
}

void test_config_set_mode(test_config_mode_t mode)
{
    reset_state();
    g_mode = mode;
    g_loaded = false;
    g_prune_enabled = true;
}

void test_config_shutdown(void)
{
    reset_state();
    g_mode = TEST_CONFIG_MODE;
    g_loaded = false;
}
