#ifdef HAVE_UNISTRING
    #include <unistr.h>
    #include <uninorm.h>
#endif

#include "string_internal.h"
#include "ustring.h"

/* Views */

string_view_t string_view(const string_t *s, size_t pos, size_t len)
{
    string_view_t v = {0};

    if (!s || pos > s->len)
        return v;

    size_t max = s->len - pos;
    if (len > max) len = max;

    v.data = s->data + pos;
    v.len  = len;

    return v;
}

int string_view_equals(const string_view_t *v, const char *cstr)
{
    if (!v || !cstr) return 0;

    size_t n = strlen(cstr);
    if (n != v->len) return 0;

    return memcmp(v->data, cstr, n) == 0;
}

string_t *string_from_view(const string_view_t *v)
{
    if (!v) return NULL;

    string_t *s = string_new();
    if (!s) return NULL;

    if (string_reserve(s, v->len + 1) != 0) {
        string_free(s);
        return NULL;
    }

    memcpy(s->data, v->data, v->len);
    s->data[v->len] = '\0';
    s->len = v->len;

    return s;
}

/* Zero-copy split into views */

static int views_grow(string_view_t **views, size_t *cap)
{
    size_t new_cap = *cap * 2;
    string_view_t *tmp = realloc(*views, new_cap * sizeof(string_view_t));
    if (!tmp) { free(*views); *views = NULL; return -1; }
    *views = tmp;
    *cap = new_cap;
    return 0;
}

string_view_t *string_split_view(const string_t *s, const char *delim, size_t *out_count)
{
    if (!s || !delim || !out_count) return NULL;

    size_t cap = 8;
    size_t count = 0;
    size_t delim_len = strlen(delim);

    string_view_t *views = malloc(cap * sizeof(string_view_t));
    if (!views) return NULL;

    const char *start = s->data;
    const char *end;

    while ((end = strstr(start, delim)) != NULL) {
        if (count == cap && views_grow(&views, &cap) != 0) return NULL;
        views[count].data = start;
        views[count].len  = (size_t)(end - start);
        count++;
        start = end + delim_len;
    }

    if (count == cap && views_grow(&views, &cap) != 0) return NULL;
    views[count].data = start;
    views[count].len  = s->len - (size_t)(start - s->data);
    count++;

    *out_count = count;
    return views;
}

void string_split_view_free(string_view_t *views)
{
    free(views);
}

/* Fixed-capacity buffer */

void string_buffer_init(string_buffer_t *b, char *storage, size_t capacity)
{
    b->data = storage;
    b->len  = 0;
    b->cap  = capacity;
    if (capacity > 0)
        b->data[0] = '\0';
}

int string_buffer_append(string_buffer_t *b, const char *text)
{
    size_t add = strlen(text);
    if (b->len + add + 1 > b->cap)
        return -1;

    memcpy(b->data + b->len, text, add + 1);
    b->len += add;
    return 0;
}

int string_buffer_append_char(string_buffer_t *b, char c)
{
    if (b->len + 2 > b->cap)
        return -1;

    b->data[b->len++] = c;
    b->data[b->len] = '\0';
    return 0;
}

/* Normalization hook (stub) */

static int utf8_normalize_external(const char *in, size_t in_len,
                                   char **out, size_t *out_len,
                                   string_norm_form_t form)
{
#ifndef HAVE_UNISTRING
    // Fallback: no-op normalization
    char *copy = malloc(in_len + 1);
    if (!copy) return -1;
    memcpy(copy, in, in_len);
    copy[in_len] = '\0';
    *out = copy;
    *out_len = in_len;
    return 0;
#else
    uninorm_t nf;

    switch (form) {
        case STRING_NORM_NFC:  nf = UNINORM_NFC;  break;
        case STRING_NORM_NFD:  nf = UNINORM_NFD;  break;
        case STRING_NORM_NFKC: nf = UNINORM_NFKC; break;
        case STRING_NORM_NFKD: nf = UNINORM_NFKD; break;
        default: return -1;
    }

    size_t outsize = 0;

    uint8_t *norm = u8_normalize(nf, (const uint8_t *)in, in_len, NULL, &outsize);

    if (!norm)
        return -1;

    *out = (char *)norm;
    *out_len = outsize;
    return 0;
#endif
}


/* Normalize s in place to the given Unicode normalization form.
   Returns 0 on success, -1 on error or unsupported form. */
int string_normalize(string_t *s, string_norm_form_t form)
{
    if (!s) return -1;

    char *norm = NULL;
    size_t norm_len = 0;

    if (utf8_normalize_external(s->data, s->len, &norm, &norm_len, form) != 0)
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
