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
    bool          is_node;   /* true if this represents a node with children */
    bool          enabled;   /* enabled flag for this test/node              */
    dictionary_t *content;   /* children if is_node == true                  */
} test_value_t;

static void test_value_clone(void *dst, const void *src)
{
    const test_value_t *sv = src;
    test_value_t       *dv = dst;

    dv->is_node = sv->is_node;
    dv->enabled = sv->enabled;
    dv->content = sv->content;
}

static void test_value_destroy(void *elem)
{
    test_value_t *v = elem;
    if (v->is_node && v->content)
        dictionary_destroy(v->content);
}

/* Root dictionary: filename -> dictionary_t* */
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

static dictionary_t *g_root  = NULL;  /* filename -> dictionary_t* */
static bool          g_dirty = false; /* whether we need to save   */

/* ========================================================================= */
/*  Path computation                                                         */
/* ========================================================================= */

static string_t *compute_config_path(void)
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

/* ========================================================================= */
/*  Root dictionary creation                                                 */
/* ========================================================================= */

static void ensure_root_created(void)
{
    if (g_root)
        return;

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
}

/* ========================================================================= */
/*  File-level dictionary helpers                                            */
/* ========================================================================= */

static dictionary_t *create_test_dict(void)
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

static dictionary_t *ensure_file_dict(const char *file)
{
    ensure_root_created();

    string_t *key = string_new_with(file);
    dictionary_t *file_dict = NULL;

    if (dictionary_get(g_root, &key, &file_dict)) {
        string_free(key);
        return file_dict;
    }

    file_dict = create_test_dict();
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

static void ensure_node(dictionary_t *dict, const char *name, test_value_t *out)
{
    if (get_test(dict, name, out)) {
        if (!out->is_node) {
            out->is_node = true;
            out->content = create_test_dict();
            set_test(dict, name, out);
        } else if (!out->content) {
            out->content = create_test_dict();
            set_test(dict, name, out);
        }
        return;
    }

    out->is_node = true;
    out->enabled = true;
    out->content = create_test_dict();
    set_test(dict, name, out);
}

/* ========================================================================= */
/*  Key existence + soft removal                                             */
/* ========================================================================= */

/* Forward declaration for loader (implemented later) */
static void load_json_if_needed(void);

bool test_config_has_key(const char *file, const char *func, const char *parent)
{
    load_json_if_needed();

    dictionary_t *file_dict = ensure_file_dict(file);

    if (parent) {
        test_value_t pv;
        if (!get_test(file_dict, parent, &pv))
            return false;

        if (!pv.is_node || !pv.content)
            return false;

        test_value_t cv;
        return get_test(pv.content, func, &cv);
    }

    test_value_t v;
    return get_test(file_dict, func, &v);
}

void test_config_remove_key(const char *file, const char *func, const char *parent)
{
    load_json_if_needed();

    dictionary_t *file_dict = ensure_file_dict(file);

    if (parent) {
        test_value_t pv;
        if (!get_test(file_dict, parent, &pv))
            return;

        if (!pv.is_node || !pv.content)
            return;

        string_t *ckey = string_new_with(func);
        dictionary_remove(pv.content, &ckey);
        string_free(ckey);
        return;
    }

    string_t *key = string_new_with(func);
    dictionary_remove(file_dict, &key);
    string_free(key);
}

/* ========================================================================= */
/*  Minimal JSON tokenizer / loader (header)                                 */
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

/* ========================================================================= */
/*  Minimal JSON parser (objects + booleans only)                            */
/* ========================================================================= */

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
            /* Minimal escape handling: just store the escaped char */
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

/* Forward declaration: parse object into a dictionary_t* of test_value_t */
static bool parse_object(json_stream_t *s, dictionary_t *dict);

static bool parse_value(json_stream_t *s, test_value_t *out)
{
    json_skip_ws(s);
    int c = json_peek(s);
    if (c == '{') {
        /* Node object */
        out->is_node = true;
        out->enabled = true;
        out->content = create_test_dict();
        return parse_object(s, out->content);
    } else {
        /* Boolean leaf */
        out->is_node = false;
        out->content = NULL;
        return json_parse_bool(s, &out->enabled);
    }
}

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

        test_value_t v;
        if (!parse_value(s, &v)) {
            string_free(key);
            return false;
        }

        dictionary_set(dict, &key, &v);
        string_free(key);

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

static bool parse_root(json_stream_t *s)
{
    ensure_root_created();

    json_skip_ws(s);
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

        /* Each file maps to an object of tests */
        dictionary_t *file_dict = create_test_dict();
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

/* ========================================================================= */
/*  JSON loader                                                              */
/* ========================================================================= */

static void load_json_if_needed(void)
{
    static bool loaded = false;
    if (loaded)
        return;
    loaded = true;

    string_t *path = compute_config_path();

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
        /* On parse failure, discard everything */
        if (g_root) {
            dictionary_destroy(g_root);
            g_root = NULL;
        }
    }

    free(buf);
}

/* ========================================================================= */
/*  JSON writer (correct for your dictionary_t API)                          */
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

static void write_object(FILE *f, dictionary_t *dict, int indent)
{
    size_t count = dictionary_size(dict);
    if (count == 0) {
        fputs("{}", f);
        return;
    }

    fputs("{\n", f);

    for (size_t i = 0; i < count; i++) {

        /* Retrieve key */
        const void *key_ptr = dictionary_get_key(dict, i);
        if (!key_ptr)
            continue;

        string_t *key = *(string_t * const *)key_ptr;

        /* Retrieve value */
        const void *val_ptr = dictionary_get_value(dict, i);
        if (!val_ptr)
            continue;

        const test_value_t *val = (const test_value_t *)val_ptr;

        write_indent(f, indent + 2);
        write_escaped_string(f, string_c_str(key));
        fputs(": ", f);
        write_value(f, val, indent + 2);

        if (i + 1 < count)
            fputs(",\n", f);
        else
            fputc('\n', f);
    }

    write_indent(f, indent);
    fputc('}', f);
}

static void write_value(FILE *f, const test_value_t *v, int indent)
{
    if (v->is_node && v->content) {
        write_object(f, v->content, indent);
    } else {
        fputs(v->enabled ? "true" : "false", f);
    }
}

static void write_root(FILE *f)
{
    if (!g_root || dictionary_size(g_root) == 0) {
        fputs("{}", f);
        return;
    }

    fputs("{\n", f);

    size_t count = dictionary_size(g_root);

    for (size_t i = 0; i < count; i++) {

        /* Retrieve key */
        const void *key_ptr = dictionary_get_key(g_root, i);
        if (!key_ptr)
            continue;

        string_t *file_key = *(string_t * const *)key_ptr;

        /* Retrieve value (file_dict) */
        const void *val_ptr = dictionary_get_value(g_root, i);
        if (!val_ptr)
            continue;

        dictionary_t *file_dict = *(dictionary_t * const *)val_ptr;

        write_indent(f, 2);
        write_escaped_string(f, string_c_str(file_key));
        fputs(": ", f);
        write_object(f, file_dict, 2);

        if (i + 1 < count)
            fputs(",\n", f);
        else
            fputc('\n', f);
    }

    fputs("}\n", f);
}

/* ========================================================================= */
/*  Public API: test_enabled                                                 */
/* ========================================================================= */

int test_enabled(const char *file, const char *func, const char *parent)
{
    load_json_if_needed();

    dictionary_t *file_dict = ensure_file_dict(file);

    if (parent) {
        test_value_t pv;
        ensure_node(file_dict, parent, &pv);

        if (!pv.enabled)
            return 0;

        if (pv.is_node && pv.content) {
            test_value_t cv;
            ensure_leaf(pv.content, func, &cv);
            return cv.enabled ? 1 : 0;
        }

        /* If parent exists but is not a node, treat child as enabled by default */
        test_value_t cv;
        ensure_leaf(file_dict, func, &cv);
        return cv.enabled ? 1 : 0;
    }

    test_value_t v;
    ensure_leaf(file_dict, func, &v);
    return v.enabled ? 1 : 0;
}

/* ========================================================================= */
/*  Public API: test_config_save                                             */
/* ========================================================================= */

void test_config_save(void)
{
    if (!g_dirty || !g_root)
        return;

    if (dictionary_size(g_root) == 0)
        return;

    string_t *path = compute_config_path();
    string_t *tmp  = string_new_with(string_c_str(path));
    string_append_cstr(tmp, ".tmp");

    FILE *f = fopen(string_c_str(tmp), "w");
    if (!f) {
        string_free(tmp);
        string_free(path);
        return;
    }

    write_root(f);
    fclose(f);

    rename(string_c_str(tmp), string_c_str(path));

    string_free(tmp);
    string_free(path);

    g_dirty = false;
}
