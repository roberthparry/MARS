/* test_config.c - hierarchical test configuration */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "dictionary.h"
#include "ustring.h"
#include "test_config.h"

/* Globals */

static dictionary_t *g_root  = NULL;
static int           g_mode  = TEST_CONFIG_MODE;

/* Track whether we've attempted to load JSON in this process */
static bool          g_loaded = false;

/* LOCAL mode: store the real test filename (basename only) */
static string_t     *g_local_filename = NULL;

/* string_t* key helpers */

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

/* Value types */

typedef struct _test_value {
    bool         is_node;
    bool         enabled;
    dictionary_t *content;
} test_value_t;

void test_value_clone(void *dst, const void *src)
{
    const test_value_t *s = src;
    test_value_t       *d = dst;

    d->is_node  = s->is_node;
    d->enabled  = s->enabled;
    d->content  = s->content;
}

void test_value_destroy(void *value)
{
    test_value_t *v = value;

    if (v->is_node && v->content) {
        dictionary_destroy(v->content);
        v->content = NULL;
    }
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

/* Path computation */

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

/* Root dictionary creation */

static dictionary_t *create_file_dict(void)
{
    dictionary_t *dict = dictionary_create(
        sizeof(string_t *),
        sizeof(test_value_t),
        string_key_hash,
        string_key_cmp,
        string_key_clone,
        string_key_destroy,
        test_value_clone,
        test_value_destroy
    );
    return dict;
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

/* File-level dictionary helpers */

static dictionary_t *ensure_file_dict(const char *file)
{
    ensure_root_created();

    if (g_mode == TEST_CONFIG_GLOBAL) {
        /* g_root: key = string_t*, value = dictionary_t* */

        string_t *file_key = string_new_with(file);
        if (!file_key)
            return NULL;

        dictionary_t *file_dict = NULL;

        /* Look up existing file dictionary */
        if (dictionary_get(g_root, &file_key, &file_dict) && file_dict) {
            string_free(file_key);
            return file_dict;
        }

        /* Create new file dictionary */
        file_dict = create_file_dict();
        if (!file_dict) {
            string_free(file_key);
            return NULL;
        }

        /* Store pointer to file_dict in g_root.
           dictptr_clone/dictptr_destroy handle cloning/destroying the pointed-to dictionary.
        */
        dictionary_set(g_root, &file_key, &file_dict);

        /* We own this key instance; g_root has its own clone. */
        string_free(file_key);

        return file_dict;
    }

    /* LOCAL mode: g_root *is* the file dictionary */
    return g_root;
}

/* Per-dictionary test helpers */

static bool get_test(dictionary_t *dict, const char *name, test_value_t *out)
{
    string_t *key = string_new_with(name);
    bool ok = dictionary_get(dict, &key, out);
    string_free(key);
    return ok;
}

static void set_test(dictionary_t *dict,
                     const char   *name,
                     const test_value_t *value)
{
    string_t *key = string_new_with(name);
    if (!key)
        return;

    dictionary_set(dict, &key, value);
    string_free(key);
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

/* JSON loader */

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
static bool parse_object(json_stream_t *s, dictionary_t **dict);
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

        if (!parse_object(s, &out->content))
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
static bool parse_object(json_stream_t *s, dictionary_t **dictp)
{
    dictionary_t *orig = *dictp;
    dictionary_t *work = dictionary_clone(orig);

    if (!json_expect(s, '{')) {
        dictionary_destroy(work);
        return false;
    }

    json_skip_ws(s);
    int c = json_peek(s);

    /* Empty object {} */
    if (c == '}') {
        json_get(s);
        dictionary_destroy(orig);
        *dictp = work;
        return true;
    }

    while (1) {
        string_t *key = NULL;
        if (!json_parse_string(s, &key)) {
            dictionary_destroy(work);
            return false;
        }

        if (!json_expect(s, ':')) {
            string_free(key);
            dictionary_destroy(work);
            return false;
        }

        test_value_t v;
        if (!parse_value(s, &v)) {
            string_free(key);
            dictionary_destroy(work);
            return false;
        }

        dictionary_set(work, &key, &v);
        string_free(key);

        json_skip_ws(s);
        c = json_peek(s);

        if (c == ',') {
            json_get(s);
            continue;
        }

        if (c == '}') {
            json_get(s);
            break;
        }

        dictionary_destroy(work);
        return false;
    }

    dictionary_destroy(orig);
    *dictp = work;
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

    /* LOCAL mode: root is a single dictionary */
    if (g_mode == TEST_CONFIG_LOCAL) {
        return parse_object(s, &g_root);
    }

    /* GLOBAL mode: { "file.c": { ... }, ... } */
    if (!json_expect(s, '{'))
        return false;

    json_skip_ws(s);
    int c = json_peek(s);

    /* Empty root {} */
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

        if (!parse_object(s, &file_dict)) {
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
        }

        if (c == '}') {
            json_get(s);
            break;
        }

        return false;
    }

    return true;
}

static void load_json_if_needed(void)
{
    if (g_loaded)
        return;

    /* In LOCAL mode we must know the filename before loading */
    if (g_mode == TEST_CONFIG_LOCAL && !g_local_filename)
        return;

    g_loaded = true;

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

/* JSON writer */

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

static void write_object_internal(FILE *f, dictionary_t *dict, int indent, bool is_file_level);

static void write_value(FILE *f, const test_value_t *v, int indent)
{
    if (!v->is_node) {
        fputs(v->enabled ? "true" : "false", f);
        return;
    }

    write_object_internal(f, v->content, indent, /*is_file_level=*/false);
}

/* Recursive lookup for unlimited-depth hierarchy */

static bool find_node_recursive(dictionary_t  *dict,
                                const char    *path,
                                test_value_t  *out_value,
                                dictionary_t **out_parent)
{
    const char *dot = strchr(path, '.');

    /* Extract current segment */
    char segment[256];
    if (dot) {
        size_t len = (size_t)(dot - path);
        if (len >= sizeof(segment))
            len = sizeof(segment) - 1;
        memcpy(segment, path, len);
        segment[len] = '\0';
    } else {
        strncpy(segment, path, sizeof(segment));
        segment[sizeof(segment) - 1] = '\0';
    }

    string_t *key = string_new_with(segment);
    if (!key)
        return false;

    test_value_t v;
    bool found = dictionary_get(dict, &key, &v);

    string_free(key);

    if (!found)
        return false;

    if (!dot) {
        /* Final segment */
        if (out_value)
            *out_value = v;
        if (out_parent)
            *out_parent = dict;
        return true;
    }

    /* More path: must be a node with content */
    if (!v.is_node || !v.content)
        return false;

    return find_node_recursive(v.content, dot + 1, out_value, out_parent);
}

/* Compute effective enabled state for a node by name */

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

/* Returns true if any entry in dict is a node (i.e., a group). */
static bool dictionary_has_any_group(dictionary_t *dict)
{
    size_t count = dictionary_size(dict);

    for (size_t i = 0; i < count; i++) {
        const void *val_ptr = dictionary_get_value(dict, i);
        if (!val_ptr)
            continue;

        const test_value_t *v = val_ptr;
        if (v->is_node)
            return true;
    }

    return false;
}

/* Public API */

int test_enabled(const char *file, const char *func, const char *parent)
{
    /* Always record the test file name */
    if (!g_local_filename)
        g_local_filename = string_new_with(file);

    load_json_if_needed();
    dictionary_t *file_dict = ensure_file_dict(file);

    /* -------------------- no parent: top-level test -------------------- */
    if (!parent) {

        /* If groups exist, do NOT create flat tests */
        if (dictionary_has_any_group(file_dict))
            return 1;

        test_value_t v;

        if (get_test(file_dict, func, &v))
            return v.enabled ? 1 : 0;

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
            return 0;
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
        pv.enabled = true;
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

/* Root writer (GLOBAL + LOCAL) */

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


void test_config_save(void)
{
    if (!g_root)
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
}

/* Mode control + shutdown */

static void reset_state(void)
{
    if (g_root) {
        dictionary_destroy(g_root);
        g_root = NULL;
    }

    if (g_local_filename) {
        string_free(g_local_filename);
        g_local_filename = NULL;
    }
}

void test_config_set_mode(test_config_mode_t mode)
{
    reset_state();
    g_mode = mode;
}

void test_config_shutdown(void)
{
    reset_state();
    g_mode   = TEST_CONFIG_MODE;
    g_loaded = false;
}
