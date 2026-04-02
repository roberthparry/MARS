#include <stdbool.h>

#include "string_internal.h"
#include "ustring.h"

/* Creation / destruction */

int string_reserve(string_t *s, size_t needed)
{
    if (needed <= s->cap) return 0;

    size_t new_cap = s->cap ? s->cap : 16;
    while (new_cap < needed)
        new_cap *= 2;

    char *p = realloc(s->data, new_cap);
    if (!p) return -1;

    s->data = p;
    s->cap  = new_cap;
    return 0;
}

string_t *string_new(void)
{
    string_t *s = calloc(1, sizeof(string_t));
    if (!s) return NULL;

    if (string_reserve(s, 1) != 0) {
        free(s);
        return NULL;
    }

    s->data[0] = '\0';
    return s;
}

string_t *string_new_with(const char *init)
{
    string_t *s = string_new();
    if (!s) return NULL;

    if (init && string_append_cstr(s, init) != 0) {
        string_free(s);
        return NULL;
    }
    return s;
}

string_t *string_clone(const string_t *src)
{
    if (!src)
        return NULL;

    return string_new_with(string_c_str(src));
}

void string_free(string_t *s)
{
    if (!s) return;
    free(s->data);
    free(s);
}

/* Access */

const char *string_c_str(const string_t *s)
{
    return s ? s->data : "";
}

size_t string_length(const string_t *s)
{
    return s ? s->len : 0;
}

/* Modification */

void string_clear(string_t *s)
{
    if (!s) return;
    s->len = 0;
    s->data[0] = '\0';
}

int string_append_cstr(string_t *s, const char *suffix)
{
    if (!s || !suffix) return -1;

    size_t add = strlen(suffix);
    size_t need = s->len + add + 1;

    if (string_reserve(s, need) != 0) return -1;

    memcpy(s->data + s->len, suffix, add + 1);
    s->len += add;
    return 0;
}

int string_append_chars(string_t *s, const char *buffer, size_t size)
{
    if (!s || !buffer || size == 0)
        return -1;

    size_t need = s->len + size + 1;

    /* Ensure capacity */
    if (string_reserve(s, need) != 0)
        return -1;

    /* Copy raw bytes */
    memcpy(s->data + s->len, buffer, size);

    s->len += size;
    s->data[s->len] = '\0';

    return 0;
}

int string_append_char(string_t *s, char c)
{
    if (!s) return -1;

    size_t need = s->len + 2;
    if (string_reserve(s, need) != 0) return -1;

    s->data[s->len++] = c;
    s->data[s->len] = '\0';
    return 0;
}

int string_insert(string_t *s, size_t pos, const char *text)
{
    if (!s || !text) return -1;
    if (pos > s->len) pos = s->len;

    size_t add = strlen(text);
    size_t need = s->len + add + 1;

    if (string_reserve(s, need) != 0) return -1;

    memmove(s->data + pos + add, s->data + pos, s->len - pos + 1);
    memcpy(s->data + pos, text, add);

    s->len += add;
    return 0;
}

void string_trim(string_t *s)
{
    if (!s || s->len == 0) return;

    size_t start = 0;
    while (start < s->len && (unsigned char)s->data[start] <= ' ')
        start++;

    size_t end = s->len;
    while (end > start && (unsigned char)s->data[end - 1] <= ' ')
        end--;

    size_t new_len = end - start;

    if (start > 0)
        memmove(s->data, s->data + start, new_len);

    s->data[new_len] = '\0';
    s->len = new_len;
}

/* printf-style */

int string_vprintf(string_t *s, const char *fmt, va_list ap)
{
    if (!s || !fmt) return -1;

    va_list ap2;
    va_copy(ap2, ap);

    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) return -1;

    size_t need = s->len + (size_t)n + 1;
    if (string_reserve(s, need) != 0) return -1;

    int written = vsnprintf(s->data + s->len, (size_t)n + 1, fmt, ap);
    if (written < 0) return -1;

    s->len += (size_t)written;
    return written;
}

int string_printf(string_t *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = string_vprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

int string_append_format(string_t *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = string_vprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

/* Search / compare */

ssize_t string_find(const string_t *s, const char *needle)
{
    if (!s || !needle || *needle == '\0')
        return -1;

    const char *p = strstr(s->data, needle);
    if (!p)
        return -1;

    return (ssize_t)(p - s->data);
}

int string_compare(const string_t *a, const string_t *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a->data, b->data);
}

bool string_starts_with(const string_t *s, const char *prefix)
{
    if (!s || !prefix) return 0;

    size_t plen = strlen(prefix);
    if (plen > s->len) return 0;

    return memcmp(s->data, prefix, plen) == 0;
}

bool string_ends_with(const string_t *s, const char *suffix)
{
    if (!s || !suffix) return 0;

    size_t slen = strlen(suffix);
    if (slen > s->len) return 0;

    return memcmp(s->data + (s->len - slen), suffix, slen) == 0;
}

string_t *string_substr(const string_t *s, size_t pos, size_t len)
{
    if (!s) return NULL;
    if (pos > s->len) pos = s->len;

    size_t max = s->len - pos;
    if (len > max) len = max;

    string_t *out = string_new();
    if (!out) return NULL;

    if (string_reserve(out, len + 1) != 0) {
        string_free(out);
        return NULL;
    }

    memcpy(out->data, s->data + pos, len);
    out->data[len] = '\0';
    out->len = len;

    return out;
}

void string_reverse(string_t *s)
{
    if (!s || s->len <= 1) return;

    size_t i = 0;
    size_t j = s->len - 1;

    while (i < j) {
        char tmp = s->data[i];
        s->data[i] = s->data[j];
        s->data[j] = tmp;
        i++;
        j--;
    }
}

/* Replace */

int string_replace(string_t *s, const char *search, const char *replace)
{
    if (!s || !search || !replace) return -1;
    if (*search == '\0') return 0;

    size_t search_len  = strlen(search);
    size_t replace_len = strlen(replace);

    size_t count = 0;
    const char *p = s->data;

    while ((p = strstr(p, search)) != NULL) {
        count++;
        p += search_len;
    }

    if (count == 0)
        return 0;

    size_t new_len = s->len + count * (replace_len - search_len);

    if (string_reserve(s, new_len + 1) != 0)
        return -1;

    char *temp = malloc(new_len + 1);
    if (!temp) return -1;

    char *out = temp;
    const char *in = s->data;

    while ((p = strstr(in, search)) != NULL) {
        size_t prefix_len = (size_t)(p - in);
        memcpy(out, in, prefix_len);
        out += prefix_len;

        memcpy(out, replace, replace_len);
        out += replace_len;

        in = p + search_len;
    }

    strcpy(out, in);

    memcpy(s->data, temp, new_len + 1);
    s->len = new_len;

    free(temp);
    return 0;
}

void string_to_upper(string_t *s)
{
    if (!s) return;

    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->data[i];
        if (c >= 'a' && c <= 'z')
            s->data[i] = (char)(c - ('a' - 'A'));
    }
}

void string_to_lower(string_t *s)
{
    if (!s) return;

    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->data[i];
        if (c >= 'A' && c <= 'Z')
            s->data[i] = (char)(c + ('a' - 'A'));
    }
}

unsigned long string_hash(const string_t *s)
{
    if (!s) return 0;

    const unsigned long FNV_OFFSET = 1469598103934665603UL;
    const unsigned long FNV_PRIME  = 1099511628211UL;

    unsigned long hash = FNV_OFFSET;

    for (size_t i = 0; i < s->len; i++) {
        hash ^= (unsigned char)s->data[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

/* Split / join */

string_t **string_split(const string_t *s, const char *delim, size_t *out_count)
{
    if (!s || !delim || !out_count) return NULL;

    char *copy = strdup(s->data);
    if (!copy) return NULL;

    size_t cap = 8;
    size_t count = 0;
    string_t **arr = malloc(cap * sizeof(string_t *));
    if (!arr) {
        free(copy);
        return NULL;
    }

    char *saveptr = NULL;
    char *tok = strtok_r(copy, delim, &saveptr);

    while (tok) {
        if (count == cap) {
            cap *= 2;
            string_t **tmp = realloc(arr, cap * sizeof(string_t *));
            if (!tmp) {
                string_split_free(arr, count);
                free(copy);
                return NULL;
            }
            arr = tmp;
        }

        arr[count] = string_new_with(tok);
        if (!arr[count]) {
            string_split_free(arr, count);
            free(copy);
            return NULL;
        }
        count++;

        tok = strtok_r(NULL, delim, &saveptr);
    }

    free(copy);
    *out_count = count;
    return arr;
}

void string_split_free(string_t **arr, size_t count)
{
    if (!arr) return;
    for (size_t i = 0; i < count; i++)
        string_free(arr[i]);
    free(arr);
}

string_t *string_join(string_t **arr, size_t count, const char *sep)
{
    if (!arr || count == 0) return string_new_with("");

    string_t *out = string_new();
    if (!out) return NULL;

    size_t sep_len = sep ? strlen(sep) : 0;

    for (size_t i = 0; i < count; i++) {
        string_append_cstr(out, string_c_str(arr[i]));
        if (i + 1 < count && sep_len > 0)
            string_append_cstr(out, sep);
    }

    return out;
}

