/* test_config.c - hierarchical test configuration */

#include "test_config.h"
#include "dictionary.h"
#include "ustring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>

/* ========================================================================= */
/*  Mode handling                                                            */
/* ========================================================================= */

#ifndef TEST_CONFIG_MODE
#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#endif

static int g_mode = TEST_CONFIG_MODE;

/* LOCAL mode: store the real test filename (basename only) */
static string_t *g_local_filename = NULL;

/* ========================================================================= */
/*  string_t* key helpers                                                    */
/* ========================================================================= */

static size_t string_key_hash(const void *key)
{
    string_t *s = *(string_t * const *)key;
    return (size_t)string_hash(s);
}

static int string_key_cmp(const void *a, const void *b)
{
    string_t *sa = *(string_t * const *)a;
    string_t *sb = *(string_t * const *)b;
    return string_compare(sa, sb);
}

static void string_key_clone(void *dst, const void *src)
{
    string_t *orig = *(string_t * const *)src;
    *(string_t **)dst = string_new_with(string_c_str(orig));
}

static void string_key_destroy(void *elem)
{
    string_free(*(string_t **)elem);
}

/* ========================================================================= */
/*  Value types                                                              */
/* ========================================================================= */

typedef struct {
    bool          is_node;   /* true = object, false = leaf */
    bool          enabled;   /* always present on nodes, always present on leaves */
    dictionary_t *content;   /* NULL for leaf, dict for node */
} test_value_t;

static void test_value_clone(void *dst, const void *src)
{
    const test_value_t *sv = src;
    test_value_t       *dv = dst;

    dv->is_node = sv->is_node;
    dv->enabled = sv->enabled;
    dv->content = sv->content; /* shallow copy; dictionary_t handles deep clone */
}

static void test_value_destroy(void *elem)
{
    test_value_t *v = elem;
    if (v->is_node && v->content)
        dictionary_destroy(v->content);
}

/* GLOBAL mode: filename -> dictionary_t* */
static void dictptr_clone(void *dst, const void *src)
{
    *(dictionary_t **)dst = *(dictionary_t * const *)src;
}

static void dictptr_destroy(void *elem)
{
    dictionary_destroy(*(dictionary_t **)elem);
}

/* ========================================================================= */
/*  Globals                                                                  */
/* ========================================================================= */

static dictionary_t *g_root  = NULL;
static bool          g_dirty = false;

/* ========================================================================= */
/*  Path computation                                                         */
/* ========================================================================= */

static string_t *compute_global_path(void)
{
    const char *file = __FILE__;
    const char *src  = strstr(file, "src/");
    string_t   *path = string_new();

    if (!src) {
        string_append_cstr(path, "tests/test_config.json");
        return path;
    }

    size_t root_len = (size_t)(src - file);
    string_append_format(path, "%.*s", (int)root_len, file);
    string_append_cstr(path, "tests/test_config.json");

    return path;
}

static string_t *compute_local_path(const char *file)
{
    const char *slash = strrchr(file, '/');
    const char *base  = slash ? slash + 1 : file;

    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);

    string_t *path = string_new();
    string_append_cstr(path, "tests/");
    string_append_format(path, "%.*s.json", (int)len, base);

    return path;
}

/* ========================================================================= */
/*  Root dictionary creation                                                 */
/* ========================================================================= */

static dictionary_t *create_file_dict(void)
{
    return dictionary_create(
        sizeof(string_t *),
        sizeof(test_value_t),
        string_key_hash,
        string_key_cmp,
        string_key_clone,
        string_key_destroy,
        test_value_clone,
        test_value_destroy
    );
}

static void ensure_root_created(void)
{
    if (g_root)
        return;

    if (g_mode == TEST_CONFIG_GLOBAL) {
        g_root = dictionary_create(
            sizeof(string_t *),
            sizeof(dictionary_t *),
            string_key_hash,
            string_key_cmp,
            string_key_clone,
            string_key_destroy,
            dictptr_clone,
            dictptr_destroy
        );
    } else {
        g_root = create_file_dict();
    }
}

/* ========================================================================= */
/*  File-level dictionary helpers                                            */
/* ========================================================================= */

static dictionary_t *ensure_file_dict(const char *file)
{
    ensure_root_created();

    if (g_mode == TEST_CONFIG_LOCAL)
        return g_root;

    string_t *key = string_new_with(file);
    dictionary_t *file_dict = NULL;

    if (dictionary_get(g_root, &key, &file_dict)) {
        string_free(key);
        return file_dict;
    }

    file_dict = create_file_dict();
    dictionary_set(g_root, &key, &file_dict);
    g_dirty = true;

    string_free(key);
    return file_dict;
}

/* ========================================================================= */
/*  Per-dictionary test helpers                                              */
/* ========================================================================= */

static bool get_test(dictionary_t *dict, const char *name, test_value_t *out)
{
    string_t *key = string_new_with(name);
    bool ok = dictionary_get(dict, &key, out);
    string_free(key);
    return ok;
}

static void set_test(dictionary_t *dict, const char *name, const test_value_t *v)
{
    string_t *key = string_new_with(name);
    dictionary_set(dict, &key, v);
    string_free(key);
    g_dirty = true;
}

static void ensure_leaf(dictionary_t *dict, const char *name, test_value_t *out)
{
    if (get_test(dict, name, out))
        return;

    out->is_node = false;
    out->enabled = true;
    out->content = NULL;
    set_test(dict, name, out);
}

/* ========================================================================= */
/*  JSON loader (rewritten for unlimited depth + "enabled" schema)           */
/* ========================================================================= */

typedef struct {
    const char *data;
    size_t      len;
    size_t      pos;
} json_stream_t;

static void json_stream_init(json_stream_t *s, const char *buf, size_t len)
{
    s->data = buf;
    s->len  = len;
    s->pos  = 0;
}

static int json_peek(json_stream_t *s)
{
    if (s->pos >= s->len)
        return EOF;
    return (unsigned char)s->data[s->pos];
}

static int json_get(json_stream_t *s)
{
    if (s->pos >= s->len)
        return EOF;
    return (unsigned char)s->data[s->pos++];
}

static void json_skip_ws(json_stream_t *s)
{
    int c;
    while ((c = json_peek(s)) != EOF) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            s->pos++;
        else
            break;
    }
}

static bool json_expect(json_stream_t *s, char ch)
{
    json_skip_ws(s);
    int c = json_get(s);
    return c == (unsigned char)ch;
}

static bool json_parse_literal(json_stream_t *s, const char *lit)
{
    json_skip_ws(s);
    size_t i = 0;
    while (lit[i]) {
        int c = json_get(s);
        if (c == EOF || c != (unsigned char)lit[i])
            return false;
        i++;
    }
    return true;
}

static bool json_parse_bool(json_stream_t *s, bool *out)
{
    json_skip_ws(s);
    int c = json_peek(s);
    if (c == 't') {
        if (!json_parse_literal(s, "true"))
            return false;
        *out = true;
        return true;
    } else if (c == 'f') {
        if (!json_parse_literal(s, "false"))
            return false;
        *out = false;
        return true;
    }
    return false;
}

static bool json_parse_string(json_stream_t *s, string_t **out)
{
    json_skip_ws(s);
    if (json_get(s) != '"')
        return false;

    string_t *str = string_new();
    int c;
    while ((c = json_get(s)) != EOF) {
        if (c == '"')
            break;
        if (c == '\\') {
            int esc = json_get(s);
            if (esc == EOF)
                break;
            c = esc;
        }
        string_append_char(str, (char)c);
    }

    if (c != '"') {
        string_free(str);
        return false;
    }

    *out = str;
    return true;
}

/* Forward declarations */
static bool parse_object(json_stream_t *s, dictionary_t *dict);
static bool parse_root(json_stream_t *s);

/* Parse a value into test_value_t:
 *
 *  - If it's a boolean → leaf test
 *  - If it's an object → node with "enabled" + children
 */
static bool parse_value(json_stream_t *s, test_value_t *out)
{
    json_skip_ws(s);
    int c = json_peek(s);

    /* leaf boolean */
    if (c == 't' || c == 'f') {
        out->is_node = false;
        out->content = NULL;
        return json_parse_bool(s, &out->enabled);
    }

    /* node object */
    if (c == '{') {
        out->is_node = true;
        out->enabled = true;            /* default */
        out->content = create_file_dict();

        if (!parse_object(s, out->content))
            return false;

        /* load enabled flag from content */
        test_value_t enabled_val;
        if (get_test(out->content, "enabled", &enabled_val)) {
            out->enabled = enabled_val.enabled;
        }

        return true;
    }

    return false;
}

/* Parse an object:
 *
 * {
 *     "enabled": true,
 *     "child1": false,
 *     "child2": { ... }
 * }
 *
 * All keys except "enabled" become children in the node's dictionary.
 */
static bool parse_object(json_stream_t *s, dictionary_t *dict)
{
    if (!json_expect(s, '{'))
        return false;

    json_skip_ws(s);
    int c = json_peek(s);
    if (c == '}') {
        json_get(s);
        return true;
    }

    while (1) {
        string_t *key = NULL;
        if (!json_parse_string(s, &key))
            return false;

        if (!json_expect(s, ':')) {
            string_free(key);
            return false;
        }

        /* Special handling for "enabled" */
        if (strcmp(string_c_str(key), "enabled") == 0) {
            bool flag = true;
            if (!json_parse_bool(s, &flag)) {
                string_free(key);
                return false;
            }

            /* Store enabled flag as a leaf */
            test_value_t v;
            v.is_node = false;
            v.enabled = flag;
            v.content = NULL;

            dictionary_set(dict, &key, &v);
            string_free(key);
        }
        else {
            /* Child node or leaf */
            test_value_t v;
            if (!parse_value(s, &v)) {
                string_free(key);
                return false;
            }

            dictionary_set(dict, &key, &v);
            string_free(key);
        }

        json_skip_ws(s);
        c = json_peek(s);
        if (c == ',') {
            json_get(s);
            continue;
        } else if (c == '}') {
            json_get(s);
            break;
        } else {
            return false;
        }
    }

    return true;
}

/* Parse the root object:
 *
 * LOCAL mode:
 *     { ...tests... }
 *
 * GLOBAL mode:
 *     { "filename": { ...tests... }, ... }
 */
static bool parse_root(json_stream_t *s)
{
    ensure_root_created();

    if (g_mode == TEST_CONFIG_LOCAL)
        return parse_object(s, g_root);

    /* GLOBAL mode */
    if (!json_expect(s, '{'))
        return false;

    json_skip_ws(s);
    int c = json_peek(s);
    if (c == '}') {
        json_get(s);
        return true;
    }

    while (1) {
        string_t *file_key = NULL;
        if (!json_parse_string(s, &file_key))
            return false;

        if (!json_expect(s, ':')) {
            string_free(file_key);
            return false;
        }

        dictionary_t *file_dict = create_file_dict();
        if (!parse_object(s, file_dict)) {
            string_free(file_key);
            dictionary_destroy(file_dict);
            return false;
        }

        dictionary_set(g_root, &file_key, &file_dict);
        string_free(file_key);

        json_skip_ws(s);
        c = json_peek(s);
        if (c == ',') {
            json_get(s);
            continue;
        } else if (c == '}') {
            json_get(s);
            break;
        } else {
            return false;
        }
    }

    return true;
}

static void load_json_if_needed(void)
{
    static bool loaded = false;
    if (loaded)
        return;
    loaded = true;

    string_t *path =
        (g_mode == TEST_CONFIG_GLOBAL)
            ? compute_global_path()
            : compute_local_path(string_c_str(g_local_filename));

    struct stat st;
    if (stat(string_c_str(path), &st) != 0 || st.st_size == 0) {
        string_free(path);
        return;
    }

    FILE *f = fopen(string_c_str(path), "rb");
    if (!f) {
        string_free(path);
        return;
    }

    char *buf = malloc((size_t)st.st_size);
    if (!buf) {
        fclose(f);
        string_free(path);
        return;
    }

    size_t n = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    string_free(path);

    if (n == 0) {
        free(buf);
        return;
    }

    json_stream_t s;
    json_stream_init(&s, buf, n);
    if (!parse_root(&s)) {
        if (g_root) {
            dictionary_destroy(g_root);
            g_root = NULL;
        }
    }

    free(buf);
}

/* ========================================================================= */
/*  JSON writer (rewritten for unlimited depth + "enabled" schema)           */
/* ========================================================================= */

static void write_indent(FILE *f, int level)
{
    for (int i = 0; i < level; i++)
        fputc(' ', f);
}

static void write_escaped_string(FILE *f, const char *s)
{
    fputc('"', f);
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c >= 0x20) {
            fputc(c, f);
        }
    }
    fputc('"', f);
}

static void write_value(FILE *f, const test_value_t *v, int indent);

/*
 * Write a node object:
 *
 * {
 *   "enabled": true,
 *   "child1": false,
 *   "child2": { ... }
 * }
 *
 * Keys are written in insertion order (dictionary order).
 */
static void write_object(FILE *f, dictionary_t *dict, int indent)
{
    size_t count = dictionary_size(dict);

    fputs("{\n", f);

    /* First write "enabled" explicitly */
    write_indent(f, indent + 2);
    write_escaped_string(f, "enabled");
    fputs(": ", f);

    /* Retrieve the stored enabled flag from the dictionary */
    test_value_t enabled_val;
    bool have_enabled = get_test(dict, "enabled", &enabled_val);

    /* If missing (should not happen), default to true */
    bool enabled_flag = have_enabled ? enabled_val.enabled : true;

    fputs(enabled_flag ? "true" : "false", f);

    /* Now write all other children */
    for (size_t i = 0; i < count; i++) {
        const void *key_ptr = dictionary_get_key(dict, i);
        if (!key_ptr)
            continue;

        string_t *key = *(string_t * const *)key_ptr;
        const char *kstr = string_c_str(key);

        /* Skip "enabled" because we already wrote it */
        if (strcmp(kstr, "enabled") == 0)
            continue;

        const void *val_ptr = dictionary_get_value(dict, i);
        if (!val_ptr)
            continue;

        const test_value_t *child = (const test_value_t *)val_ptr;

        fputs(",\n", f);
        write_indent(f, indent + 2);
        write_escaped_string(f, kstr);
        fputs(": ", f);
        write_value(f, child, indent + 2);
    }

    fputc('\n', f);
    write_indent(f, indent);
    fputc('}', f);
}

/* ========================================================================= */
/*  Write a test_value_t into JSON                                           */
/* ========================================================================= */
static void write_value(FILE *f, const test_value_t *v, int indent)
{
    if (!v->is_node) {
        /* Leaf: just write true/false */
        fputs(v->enabled ? "true" : "false", f);
        return;
    }

    /* Node: write object */
    write_object(f, v->content, indent);
}

/* ========================================================================= */
/*  Recursive lookup for unlimited-depth hierarchy                           */
/* ========================================================================= */

/*
 * find_node_recursive:
 *
 * Search the entire tree rooted at `dict` for a key named `name`.
 *
 * - If found:
 *       *out_val  = the test_value_t stored under that key
 *       *out_dict = the dictionary that directly contains that key
 *   Returns true.
 *
 * - If not found:
 *       Returns false.
 *
 * This supports unlimited nesting.
 */
static bool find_node_recursive(dictionary_t *dict,
                                const char *name,
                                test_value_t *out_val,
                                dictionary_t **out_dict)
{
    size_t count = dictionary_size(dict);

    for (size_t i = 0; i < count; i++) {
        const void *key_ptr = dictionary_get_key(dict, i);
        const void *val_ptr = dictionary_get_value(dict, i);
        if (!key_ptr || !val_ptr)
            continue;

        string_t *key = *(string_t * const *)key_ptr;
        test_value_t *val = (test_value_t *)val_ptr;

        /* Direct match */
        if (strcmp(string_c_str(key), name) == 0) {
            if (out_val)  *out_val  = *val;
            if (out_dict) *out_dict = dict;
            return true;
        }

        /* Recurse into nodes only */
        if (val->is_node && val->content) {
            if (find_node_recursive(val->content, name, out_val, out_dict))
                return true;
        }
    }

    return false;
}

/* ========================================================================= */
/*  Compute effective enabled state for a node by name                       */
/* ========================================================================= */
/*
 * effective_enabled(name) = AND of enabled flags along the path
 *                           from the root down to that node.
 *
 * Returns true if the node is found, false otherwise.
 */
/* ========================================================================= */
/*  Compute effective enabled state for a node by name                       */
/* ========================================================================= */
/*
 * effective_enabled(name) = AND of enabled flags along the path
 *                           from the root down to that node.
 *
 * File-level nodes are ALWAYS considered enabled.
 */
static bool find_effective_enabled(dictionary_t *dict,
                                   const char *name,
                                   bool ancestors_enabled,
                                   bool *out_enabled)
{
    if (!dict || !name)
        return false;

    size_t count = dictionary_size(dict);

    for (size_t i = 0; i < count; i++) {
        const void *key_ptr = dictionary_get_key(dict, i);
        const void *val_ptr = dictionary_get_value(dict, i);
        if (!key_ptr || !val_ptr)
            continue;

        string_t     *key = *(string_t * const *)key_ptr;
        test_value_t *val = (test_value_t *)val_ptr;

        /* File-level enabled is ignored — treat file as always enabled */
        bool current_enabled = ancestors_enabled && val->enabled;

        if (strcmp(string_c_str(key), name) == 0) {
            if (out_enabled)
                *out_enabled = current_enabled;
            return true;
        }

        if (val->is_node && val->content) {
            if (find_effective_enabled(val->content,
                                       name,
                                       current_enabled,
                                       out_enabled))
                return true;
        }
    }

    return false;
}

/* ========================================================================= */
/*  Public API: test_enabled (unlimited depth + parent override)             */
/* ========================================================================= */

int test_enabled(const char *file, const char *func, const char *parent)
{
    if (g_mode == TEST_CONFIG_LOCAL && !g_local_filename)
        g_local_filename = string_new_with(file);

    load_json_if_needed();
    dictionary_t *file_dict = ensure_file_dict(file);

    /* --------------------------------------------------------------
       File-level enabled is ignored — files are always enabled.
       Only groups and tests can be disabled.
       -------------------------------------------------------------- */

    /* -------------------- no parent: top-level test -------------------- */
    if (!parent) {
        test_value_t v;

        if (get_test(file_dict, func, &v))
            return v.enabled ? 1 : 0;

        /* Missing → default true and create */
        ensure_leaf(file_dict, func, &v);
        return 1;
    }

    /* -------------------- parent: check effective enabled --------------- */
    bool parent_effective_enabled = true;

    if (find_effective_enabled(file_dict,
                               parent,
                               true,
                               &parent_effective_enabled)) {
        if (!parent_effective_enabled)
            return 0;   /* parent or ancestor disabled */
    }

    /* -------------------- ensure parent node exists --------------------- */
    test_value_t pv;
    dictionary_t *parent_dict = NULL;

    if (!find_node_recursive(file_dict, parent, &pv, &parent_dict)) {
        pv.is_node = true;
        pv.enabled = true;
        pv.content = create_file_dict();
        set_test(file_dict, parent, &pv);
        parent_dict = file_dict;
    }

    if (!pv.is_node || !pv.content) {
        pv.is_node = true;
        pv.content = create_file_dict();
        if (!parent_dict)
            parent_dict = file_dict;
        set_test(parent_dict, parent, &pv);
    }

    dictionary_t *current = pv.content;

    /* -------------------- lookup child inside parent -------------------- */
    test_value_t cv;

    if (get_test(current, func, &cv))
        return cv.enabled ? 1 : 0;

    /* Missing child → default true and create */
    ensure_leaf(current, func, &cv);
    return 1;
}

/* ========================================================================= */
/*  Public API: test_config_has_key (unlimited depth)                        */
/* ========================================================================= */

bool test_config_has_key(const char *file, const char *func, const char *parent)
{
    if (g_mode == TEST_CONFIG_LOCAL && !g_local_filename)
        g_local_filename = string_new_with(file);

    load_json_if_needed();
    dictionary_t *file_dict = ensure_file_dict(file);

    /* -------------------- no parent: top-level -------------------- */
    if (!parent) {
        test_value_t v;
        return get_test(file_dict, func, &v);
    }

    /* -------------------- find parent anywhere -------------------- */
    test_value_t pv;
    dictionary_t *parent_dict = NULL;

    if (!find_node_recursive(file_dict, parent, &pv, &parent_dict))
        return false;

    if (!pv.is_node || !pv.content)
        return false;

    /* -------------------- lookup child -------------------- */
    test_value_t cv;
    return get_test(pv.content, func, &cv);
}

static void write_object_internal(FILE *f, dictionary_t *dict, int indent, bool is_file_level)
{
    size_t count = dictionary_size(dict);

    fputs("{\n", f);

    /* --------------------------------------------------------------
       Only write "enabled" for groups/tests, NOT for file-level.
       -------------------------------------------------------------- */
    if (!is_file_level) {
        write_indent(f, indent + 2);
        write_escaped_string(f, "enabled");
        fputs(": ", f);

        test_value_t enabled_val;
        bool have_enabled = get_test(dict, "enabled", &enabled_val);
        bool enabled_flag = have_enabled ? enabled_val.enabled : true;

        fputs(enabled_flag ? "true" : "false", f);
    }

    /* Write children */
    bool first_child = is_file_level;  /* file-level has no enabled field */

    for (size_t i = 0; i < count; i++) {
        const void *key_ptr = dictionary_get_key(dict, i);
        if (!key_ptr)
            continue;

        string_t *key = *(string_t * const *)key_ptr;
        const char *kstr = string_c_str(key);

        /* Skip "enabled" because we handled it above */
        if (strcmp(kstr, "enabled") == 0)
            continue;

        const void *val_ptr = dictionary_get_value(dict, i);
        if (!val_ptr)
            continue;

        const test_value_t *child = (const test_value_t *)val_ptr;

        if (!first_child)
            fputs(",\n", f);
        else
            first_child = false;

        write_indent(f, indent + 2);
        write_escaped_string(f, kstr);
        fputs(": ", f);
        write_value(f, child, indent + 2);
    }

    fputc('\n', f);
    write_indent(f, indent);
    fputc('}', f);
}

/* ========================================================================= */
/*  Root writer (GLOBAL + LOCAL)                                             */
/* ========================================================================= */

static void write_root(FILE *f)
{
    if (g_mode == TEST_CONFIG_LOCAL) {
        /* LOCAL mode: root is a single dictionary of tests */
        write_object_internal(f, g_root, 0, /*is_file_level=*/true);
        return;
    }

    /* GLOBAL mode: root is { "filename": { ...tests... }, ... } */
    if (!g_root || dictionary_size(g_root) == 0) {
        fputs("{}", f);
        return;
    }

    fputs("{\n", f);

    size_t count = dictionary_size(g_root);

    for (size_t i = 0; i < count; i++) {
        const void *key_ptr = dictionary_get_key(g_root, i);
        const void *val_ptr = dictionary_get_value(g_root, i);
        if (!key_ptr || !val_ptr)
            continue;

        string_t *file_key = *(string_t * const *)key_ptr;
        dictionary_t *file_dict = *(dictionary_t * const *)val_ptr;

        write_indent(f, 2);
        write_escaped_string(f, string_c_str(file_key));
        fputs(": ", f);

        /* File-level object: skip "enabled" */
        write_object_internal(f, file_dict, 2, /*is_file_level=*/true);

        if (i + 1 < count)
            fputs(",\n", f);
        else
            fputc('\n', f);
    }

    fputs("}\n", f);
}

/* ========================================================================= */
/*  Public API: test_config_save                                             */
/* ========================================================================= */

void test_config_save(void)
{
    if (!g_dirty || !g_root)
        return;

    string_t *path =
        (g_mode == TEST_CONFIG_GLOBAL)
            ? compute_global_path()
            : compute_local_path(string_c_str(g_local_filename));

    /* Write to a temporary file first */
    string_t *tmp = string_new_with(string_c_str(path));
    string_append_cstr(tmp, ".tmp");

    FILE *f = fopen(string_c_str(tmp), "w");
    if (!f) {
        string_free(tmp);
        string_free(path);
        return;
    }

    write_root(f);
    fclose(f);

    /* Atomic replace */
    rename(string_c_str(tmp), string_c_str(path));

    string_free(tmp);
    string_free(path);

    g_dirty = false;
}

/* ========================================================================= */
/*  Public API: test_config_set_mode                                         */
/* ========================================================================= */

void test_config_set_mode(test_config_mode_t mode)
{
    g_mode = mode;
}
