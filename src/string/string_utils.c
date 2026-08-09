/* string_utils.c - views, search, split/join, replace, and normalisation
 *
 * Higher-level string utilities built on top of the core string_t type:
 *
 *   Views        — string_view, string_view_equals_view, string_from_view
 *   Search       — string_find, string_find_last, string_contains
 *   Comparison   — string_compare, string_equals, string_starts_with,
 *                  string_ends_with
 *   Split/join   — string_split_views (zero-copy), string_split,
 *                  string_join, string_join_views
 *   Replace      — string_replace_first, string_replace_all
 *   Normalisation — string_normalise and related Unicode helpers.
 *                   (requires libunistring; no-op stubs otherwise)
 *
 * When compiled with -DHAVE_UNISTRING, the Unicode normalisation functions
 * delegate to GNU libunistring (u8_normalize).  Without it they are defined
 * as pass-through clones so callers compile unchanged.
 */

#ifdef HAVE_UNISTRING
#include <uninorm.h>
#include <unistr.h>
#endif

#include <ctype.h>
#include <stdarg.h>

#define MARS_STRING_INTERNAL_ACCESS
#include "string_internal.h"
#include "ustring.h"

/* Views */

static const char *string_view_stored_data(const string_view_t *view)
{
    return view ? (const char *)view->_opaque[0] : NULL;
}

static size_t string_view_stored_len(const string_view_t *view)
{
    return view ? (size_t)view->_opaque[1] : 0u;
}

static void string_view_store(string_view_t *view, const char *data, size_t len)
{
    if (!view)
        return;
    view->_opaque[0] = (uintptr_t)data;
    view->_opaque[1] = data ? (uintptr_t)len : 0u;
}

static string_view_t string_view_make(const char *data, size_t len)
{
    string_view_t view;

    string_view_store(&view, data, len);
    return view;
}

string_view_t string_view_from_chars(const char *data, size_t len)
{
    return string_view_make(data, len);
}

const char *string_view_data(string_view_t view)
{
    return string_view_stored_data(&view);
}

size_t string_view_length(string_view_t view)
{
    return string_view_stored_len(&view);
}

int string_view_is_empty(string_view_t view)
{
    return !string_view_data(view) || string_view_length(view) == 0u;
}

const char *string_view_end(string_view_t view)
{
    const char *data = string_view_data(view);

    return data ? data + string_view_length(view) : NULL;
}

string_view_t string_view_slice(string_view_t view, size_t pos, size_t len)
{
    size_t view_len = string_view_length(view);
    const char *data = string_view_data(view);
    size_t max;

    if (!data || pos > view_len)
        return string_view_from_chars(NULL, 0u);

    max = view_len - pos;
    if (len > max)
        len = max;

    return string_view_from_chars(data + pos, len);
}

int string_view_equals_view(string_view_t a, string_view_t b)
{
    return string_view_length(a) == string_view_length(b) &&
           (string_view_length(a) == 0u ||
            memcmp(string_view_data(a), string_view_data(b), string_view_length(a)) == 0);
}

string_view_t string_view_empty(void)
{
    return string_view_make(NULL, 0u);
}

static bool string_view_peek_storage(string_view_t view, size_t pos, unsigned char *out)
{
    const char *data = string_view_data(view);

    if (!data || pos >= string_view_length(view))
        return false;
    if (out)
        *out = (unsigned char)data[pos];
    return true;
}

bool string_view_peek_ascii(string_view_t view, string_pos_t pos, unsigned char *out)
{
    unsigned char c = 0u;

    if (!string_view_peek_storage(view, pos, &c) || c >= 0x80u)
        return false;
    if (out)
        *out = c;
    return true;
}

bool string_view_peek_rune_value(string_view_t view, string_pos_t pos, uint32_t *out, string_pos_t *next_pos_out)
{
    const char *data = string_view_data(view);
    size_t width = 0u;
    uint32_t cp;

    if (!data || pos >= string_view_length(view))
        return false;

    cp = utf8_decode(data + pos, string_view_length(view) - pos, &width);
    if (width == 0u)
        return false;

    if (out)
        *out = cp;
    if (next_pos_out)
        *next_pos_out = pos + width;
    return true;
}

static bool string_rune_value_is_space(uint32_t cp)
{
    return (cp >= 0x09u && cp <= 0x0du) || cp == 0x20u || cp == 0x85u || cp == 0xa0u || cp == 0x1680u ||
           (cp >= 0x2000u && cp <= 0x200au) || cp == 0x2028u || cp == 0x2029u || cp == 0x202fu || cp == 0x205fu ||
           cp == 0x3000u;
}

string_view_t string_view_trim(string_view_t view)
{
    size_t start = 0u;
    size_t end = string_view_length(view);
    uint32_t cp = 0u;
    string_pos_t next = 0u;

    while (start < end && string_view_peek_rune_value(view, start, &cp, &next) && string_rune_value_is_space(cp)) {
        start = next;
    }

    while (end > start) {
        size_t prev = end;
        size_t scan = start;

        while (scan < end && string_view_peek_rune_value(view, scan, &cp, &next) && next <= end) {
            prev = scan;
            scan = next;
        }
        if (prev >= end || !string_view_peek_rune_value(view, prev, &cp, &next) || !string_rune_value_is_space(cp)) {
            break;
        }
        end = prev;
    }

    return string_view_slice(view, start, end - start);
}

bool string_view_equals_literal(string_view_t view, const char *literal)
{
    size_t len;

    if (!literal)
        return false;

    len = strlen(literal);
    return string_view_length(view) == len && (len == 0u || memcmp(string_view_data(view), literal, len) == 0);
}

bool string_view_starts_with_view(string_view_t view, string_view_t literal, bool case_insensitive)
{
    size_t i = 0u;
    size_t literal_len = string_view_length(literal);
    unsigned char got = 0u;
    unsigned char want = 0u;

    if (literal_len > string_view_length(view))
        return false;

    while (i < literal_len) {
        if (!string_view_peek_storage(view, i, &got) || !string_view_peek_storage(literal, i, &want))
            return false;
        if (case_insensitive) {
            got = (unsigned char)tolower(got);
            want = (unsigned char)tolower(want);
        }
        if (got != want)
            return false;
        i++;
    }

    return true;
}

bool string_view_starts_with(string_view_t view, const string_t *literal, bool case_insensitive)
{
    return literal && string_view_starts_with_view(view, string_view_all(literal), case_insensitive);
}

string_view_t string_view(const string_t *s, size_t pos, size_t len)
{
    if (!s || pos > s->len)
        return string_view_from_chars(NULL, 0u);

    size_t max = s->len - pos;
    if (len > max)
        len = max;

    return string_view_from_chars(s->data + pos, len);
}

string_view_t string_view_all(const string_t *s)
{
    return string_view(s, 0u, string_encoded_len(s));
}

/* Cursor parsing */

static rune_t string_cursor_rune_empty(void)
{
    rune_t rune = {{0u, 0u, 0u}};

    return rune;
}

static rune_t string_cursor_rune_from_range(const string_t *s, size_t offset, size_t length)
{
    rune_t rune = {{(uintptr_t)s, (uintptr_t)offset, (uintptr_t)length}};

    return rune;
}

string_cursor_t *string_cursor_new(const string_t *s)
{
    string_cursor_t *cursor;

    if (!s)
        return NULL;

    cursor = malloc(sizeof(*cursor));
    if (!cursor)
        return NULL;

    cursor->source = s;
    cursor->owned_source = NULL;
    cursor->pos = 0u;
    return cursor;
}

string_cursor_t *string_cursor_new_view(string_view_t view)
{
    string_t *source = string_from_view(&view);
    string_cursor_t *cursor;

    if (!source)
        return NULL;

    cursor = string_cursor_new(source);
    if (!cursor) {
        string_free(source);
        return NULL;
    }

    cursor->owned_source = source;
    return cursor;
}

string_cursor_t *string_cursor_clone(const string_cursor_t *cursor)
{
    string_cursor_t *clone;
    string_t *owned_clone = NULL;

    if (!cursor || !cursor->source)
        return NULL;

    if (cursor->owned_source) {
        owned_clone = string_clone(cursor->owned_source);
        if (!owned_clone)
            return NULL;
    }

    clone = string_cursor_new(owned_clone ? owned_clone : cursor->source);
    if (!clone) {
        string_free(owned_clone);
        return NULL;
    }

    clone->owned_source = owned_clone;
    clone->pos = cursor->pos;
    return clone;
}

void string_cursor_free(string_cursor_t *cursor)
{
    if (cursor)
        string_free(cursor->owned_source);
    free(cursor);
}

bool string_cursor_done(const string_cursor_t *cursor)
{
    return !cursor || !cursor->source || cursor->pos >= cursor->source->len;
}

string_pos_t string_cursor_position(const string_cursor_t *cursor)
{
    return cursor ? cursor->pos : 0u;
}

string_pos_t string_cursor_end_position(const string_cursor_t *cursor)
{
    return (cursor && cursor->source) ? cursor->source->len : 0u;
}

int string_cursor_seek(string_cursor_t *cursor, string_pos_t pos)
{
    if (!cursor || !cursor->source || pos > cursor->source->len)
        return -1;

    cursor->pos = pos;
    return 0;
}

rune_t string_cursor_peek(const string_cursor_t *cursor)
{
    size_t end;

    if (string_cursor_done(cursor))
        return string_cursor_rune_empty();

    end = string_grapheme_next(cursor->source->data, cursor->source->len, cursor->pos);
    if (end <= cursor->pos)
        return string_cursor_rune_empty();

    return string_cursor_rune_from_range(cursor->source, cursor->pos, end - cursor->pos);
}

int string_cursor_next(string_cursor_t *cursor)
{
    size_t next;

    if (string_cursor_done(cursor))
        return -1;

    if ((unsigned char)cursor->source->data[cursor->pos] < 0x80u) {
        ++cursor->pos;
        return 0;
    }

    next = string_grapheme_next(cursor->source->data, cursor->source->len, cursor->pos);
    if (next <= cursor->pos || next > cursor->source->len)
        return -1;

    cursor->pos = next;
    return 0;
}

bool string_cursor_match(const string_cursor_t *cursor, const char *literal)
{
    size_t len;

    if (!cursor || !cursor->source || !literal)
        return false;

    len = strlen(literal);
    if (len > cursor->source->len - cursor->pos)
        return false;

    return memcmp(cursor->source->data + cursor->pos, literal, len) == 0;
}

bool string_cursor_consume(string_cursor_t *cursor, const char *literal)
{
    size_t len;

    if (!string_cursor_match(cursor, literal))
        return false;

    len = strlen(literal);
    cursor->pos += len;
    return true;
}

bool string_cursor_peek_ascii(const string_cursor_t *cursor, unsigned char *out)
{
    char c = '\0';

    if (!cursor || !cursor->source || cursor->pos >= cursor->source->len)
        return false;
    if ((unsigned char)cursor->source->data[cursor->pos] < 0x80u) {
        if (out)
            *out = (unsigned char)cursor->source->data[cursor->pos];
        return true;
    }

    if (!rune_to_ascii(string_cursor_peek(cursor), &c))
        return false;
    if (out)
        *out = (unsigned char)c;
    return true;
}

bool string_cursor_peek_ascii_at(const string_cursor_t *cursor, string_pos_t pos, unsigned char *out)
{
    char c = '\0';

    if (!cursor || !cursor->source || pos >= cursor->source->len)
        return false;
    if ((unsigned char)cursor->source->data[pos] < 0x80u) {
        if (out)
            *out = (unsigned char)cursor->source->data[pos];
        return true;
    }

    if (!rune_to_ascii(string_cursor_peek_at(cursor, pos), &c))
        return false;
    if (out)
        *out = (unsigned char)c;
    return true;
}

bool string_cursor_skip(string_cursor_t *cursor, string_pos_t span)
{
    if (!cursor || !cursor->source || span > cursor->source->len - cursor->pos)
        return false;

    cursor->pos += span;
    return true;
}

void string_cursor_skip_spaces(string_cursor_t *cursor)
{
    uint32_t cp = 0u;
    rune_t rune;

    while (!string_cursor_done(cursor)) {
        rune = string_cursor_peek(cursor);
        cp = rune_value(rune);
        if (!string_rune_value_is_space(cp))
            break;
        if (string_cursor_next(cursor) != 0)
            break;
    }
}

string_t *string_cursor_slice_between(string_pos_t start, string_pos_t end, const string_cursor_t *cursor)
{
    string_t *out;

    if (!cursor || !cursor->source || start > end || end > cursor->source->len)
        return NULL;

    out = string_new();
    if (!out)
        return NULL;

    if (string_reserve(out, end - start + 1u) != 0) {
        string_free(out);
        return NULL;
    }

    if (end > start)
        memcpy(out->data, cursor->source->data + start, end - start);
    out->data[end - start] = '\0';
    out->len = end - start;
    return out;
}

string_t *string_cursor_extract(string_pos_t start, const string_cursor_t *cursor)
{
    return cursor ? string_cursor_slice_between(start, cursor->pos, cursor) : NULL;
}

int string_cursor_append_slice_between(string_t *out, string_pos_t start, string_pos_t end,
                                       const string_cursor_t *cursor)
{
    string_t *copy;
    int rc;

    if (!out || !cursor)
        return -1;

    copy = string_cursor_slice_between(start, end, cursor);
    if (!copy)
        return -1;

    rc = string_append_string(out, copy);
    string_free(copy);
    return rc;
}

string_view_t string_cursor_view_between(string_pos_t start, string_pos_t end, const string_cursor_t *cursor)
{
    if (!cursor || !cursor->source || start > end || end > cursor->source->len)
        return string_view_empty();

    return string_view(cursor->source, start, end - start);
}

string_view_t string_cursor_view_extract(string_pos_t start, const string_cursor_t *cursor)
{
    return cursor ? string_cursor_view_between(start, cursor->pos, cursor) : string_view_empty();
}

rune_t string_cursor_peek_at(const string_cursor_t *cursor, string_pos_t pos)
{
    string_cursor_t tmp;

    if (!cursor || !cursor->source || pos > cursor->source->len)
        return string_cursor_rune_empty();

    tmp = *cursor;
    tmp.owned_source = NULL;
    tmp.pos = pos;
    return string_cursor_peek(&tmp);
}

bool string_cursor_match_at(const string_cursor_t *cursor, string_pos_t pos, const char *literal)
{
    string_cursor_t tmp;

    if (!cursor || !literal || pos > string_cursor_end_position(cursor))
        return false;

    tmp = *cursor;
    tmp.owned_source = NULL;
    tmp.pos = pos;
    return string_cursor_match(&tmp, literal);
}

string_t *string_from_view(const string_view_t *v)
{
    const char *data;
    size_t len;
    if (!v)
        return NULL;

    data = string_view_data(*v);
    len = string_view_length(*v);
    if (!data && len > 0u)
        return NULL;

    string_t *s = string_new();
    if (!s)
        return NULL;

    if (string_reserve(s, len + 1u) != 0) {
        string_free(s);
        return NULL;
    }

    if (len > 0u)
        memcpy(s->data, data, len);
    s->data[len] = '\0';
    s->len = len;

    return s;
}

/* Zero-copy split into views */

static int views_grow(string_view_t **views, size_t *cap)
{
    size_t new_cap = *cap * 2;
    string_view_t *tmp = realloc(*views, new_cap * sizeof(string_view_t));
    if (!tmp) {
        free(*views);
        *views = NULL;
        return -1;
    }
    *views = tmp;
    *cap = new_cap;
    return 0;
}

static bool string_utils_find_bytes_index(const char *haystack, size_t haystack_len, const char *needle,
                                          size_t needle_len, size_t start, size_t *index_out)
{
    if (!haystack || !needle || needle_len == 0u || needle_len > haystack_len)
        return false;
    if (start > haystack_len - needle_len)
        return false;

    for (size_t i = start; i <= haystack_len - needle_len; i++) {
        if (memcmp(&haystack[i], needle, needle_len) == 0) {
            if (index_out)
                *index_out = i;
            return true;
        }
    }
    return false;
}

string_view_t *string_split_view_by_view(const string_t *s, string_view_t delim, size_t *out_count)
{
    const char *delim_data = string_view_data(delim);
    size_t delim_len = string_view_length(delim);
    string_view_t source;
    size_t start = 0u;
    size_t match = 0u;

    size_t cap = 8;
    size_t count = 0;

    string_view_t *views = malloc(cap * sizeof(string_view_t));
    if (!views)
        return NULL;

    if (!s || !out_count || !delim_data || delim_len == 0u) {
        free(views);
        return NULL;
    }

    source = string_view_all(s);
    while (string_utils_find_bytes_index(s->data, s->len, delim_data, delim_len, start, &match)) {
        if (count == cap && views_grow(&views, &cap) != 0)
            return NULL;
        views[count] = string_view_slice(source, start, match - start);
        count++;
        start = match + delim_len;
    }

    if (count == cap && views_grow(&views, &cap) != 0)
        return NULL;
    views[count] = string_view_slice(source, start, s->len - start);
    count++;

    *out_count = count;
    return views;
}

void string_split_view_free(string_view_t *views)
{
    free(views);
}

/* Fixed-capacity buffer */

static char *string_buffer_data(string_buffer_t *b)
{
    return b ? (char *)b->_opaque_storage[0] : NULL;
}

static const char *string_buffer_const_data(const string_buffer_t *b)
{
    return b ? (const char *)b->_opaque_storage[0] : "";
}

static size_t string_buffer_len(const string_buffer_t *b)
{
    return b ? (size_t)b->_opaque_storage[1] : 0u;
}

static size_t string_buffer_cap(const string_buffer_t *b)
{
    return b ? (size_t)b->_opaque_storage[2] : 0u;
}

static void string_buffer_set_len(string_buffer_t *b, size_t len)
{
    if (b)
        b->_opaque_storage[1] = (uintptr_t)len;
}

void string_buffer_init(string_buffer_t *b, char *storage, size_t capacity)
{
    if (!b)
        return;
    b->_opaque_storage[0] = (uintptr_t)storage;
    b->_opaque_storage[1] = 0u;
    b->_opaque_storage[2] = (uintptr_t)capacity;
    if (storage && capacity > 0u)
        storage[0] = '\0';
}

int string_buffer_append(string_buffer_t *b, const char *text)
{
    char *data = string_buffer_data(b);
    size_t len = string_buffer_len(b);
    size_t cap = string_buffer_cap(b);
    size_t add;

    if (!data || !text)
        return -1;

    add = strlen(text);
    if (len + add + 1u > cap)
        return -1;

    memcpy(data + len, text, add + 1u);
    string_buffer_set_len(b, len + add);
    return 0;
}

int string_buffer_append_string(string_buffer_t *b, const string_t *text)
{
    return text ? string_buffer_append(b, string_c_str(text)) : -1;
}

int string_buffer_append_char(string_buffer_t *b, char c)
{
    char *data = string_buffer_data(b);
    size_t len = string_buffer_len(b);
    size_t cap = string_buffer_cap(b);

    if (!data || len + 2u > cap)
        return -1;

    data[len++] = c;
    data[len] = '\0';
    string_buffer_set_len(b, len);
    return 0;
}

const char *string_buffer_c_str(const string_buffer_t *b)
{
    return string_buffer_const_data(b);
}

size_t string_buffer_length(const string_buffer_t *b)
{
    return string_buffer_len(b);
}

size_t string_buffer_capacity(const string_buffer_t *b)
{
    return string_buffer_cap(b);
}

/* Builder API */

string_builder_t *string_builder_new(void)
{
    return string_new();
}

void string_builder_free(string_builder_t *b)
{
    string_free(b);
}

int string_builder_append(string_builder_t *b, const char *s)
{
    return string_append_cstr(b, s);
}

int string_builder_append_string(string_builder_t *b, const string_t *s)
{
    return string_append_string(b, s);
}

int string_builder_append_view(string_builder_t *b, string_view_t s)
{
    return string_append_view(b, s);
}

int string_builder_append_char(string_builder_t *b, char c)
{
    return string_append_char(b, c);
}

int string_builder_format(string_builder_t *b, const char *fmt, ...)
{
    int r;
    va_list ap;

    va_start(ap, fmt);
    r = string_append_vformat(b, fmt, ap);
    va_end(ap);
    return r;
}

/* Normalisation hook (stub) */

static int utf8_normalise_external(const char *in, size_t in_len, char **out, size_t *out_len, string_norm_form_t form)
{
#ifndef HAVE_UNISTRING
    // Fallback: no-op normalisation
    char *copy = malloc(in_len + 1);
    if (!copy)
        return -1;
    memcpy(copy, in, in_len);
    copy[in_len] = '\0';
    *out = copy;
    *out_len = in_len;
    return 0;
#else
    static const uninorm_t normal_forms[STRING_NORM_COUNT] = {[STRING_NORM_NFC] = UNINORM_NFC,
                                                              [STRING_NORM_NFD] = UNINORM_NFD,
                                                              [STRING_NORM_NFKC] = UNINORM_NFKC,
                                                              [STRING_NORM_NFKD] = UNINORM_NFKD};

    if ((size_t)form >= STRING_NORM_COUNT)
        return -1;

    size_t outsize = 0;

    uint8_t *norm = u8_normalize(normal_forms[form], (const uint8_t *)in, in_len, NULL, &outsize);

    if (!norm)
        return -1;

    *out = (char *)norm;
    *out_len = outsize;
    return 0;
#endif
}

/* Normalise s in place to the given Unicode normalisation form.
   Returns 0 on success, -1 on error or unsupported form. */
int string_normalise(string_t *s, string_norm_form_t form)
{
    if (!s)
        return -1;

    char *norm = NULL;
    size_t norm_len = 0;

    if (utf8_normalise_external(s->data, s->len, &norm, &norm_len, form) != 0)
        return -1;

    if (string_reserve(s, norm_len + 1) != 0) {
        free(norm);
        return -1;
    }

    memcpy(s->data, norm, norm_len);
    s->data[norm_len] = '\0';
    s->len = norm_len;

    free(norm);
    return 0;
}

int string_normalise_storage(string_t *s)
{
    return string_normalise(s, STRING_NORM_NFC);
}
