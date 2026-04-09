#include <stdbool.h>

#include "string_internal.h"
#include "ustring.h"

/* Grow the internal buffer to hold at least `needed` bytes. No-op if capacity
   is already sufficient. Returns 0 on success, -1 on allocation failure. */

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

/* Allocate and return a new empty string. Returns NULL on allocation failure. */

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

/* Allocate a new string pre-populated with `init`. Returns NULL on failure. */

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

/* Return a deep copy of `src`, or NULL if src is NULL or allocation fails. */

string_t *string_clone(const string_t *src)
{
    if (!src)
        return NULL;

    return string_new_with(string_c_str(src));
}

/* Free the string and its internal buffer. No-op if s is NULL. */

void string_free(string_t *s)
{
    if (!s) return;
    free(s->data);
    free(s);
}

/* Return the null-terminated C string. Returns "" if s is NULL. */

const char *string_c_str(const string_t *s)
{
    return s ? s->data : "";
}

/* Return the number of bytes in the string. Returns 0 if s is NULL. */

size_t string_length(const string_t *s)
{
    return s ? s->len : 0;
}

/* Reset the string to empty without releasing its allocated memory. */

void string_clear(string_t *s)
{
    if (!s) return;
    s->len = 0;
    s->data[0] = '\0';
}

/* Append the null-terminated C string `suffix` to s. Returns 0 on success,
   -1 if either argument is NULL or allocation fails. */

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

/* Append exactly `size` bytes from `buffer` to s (need not be null-terminated).
   Returns 0 on success, -1 on bad arguments or allocation failure. */

int string_append_chars(string_t *s, const char *buffer, size_t size)
{
    if (!s || !buffer || size == 0)
        return -1;

    size_t need = s->len + size + 1;

    if (string_reserve(s, need) != 0)
        return -1;

    memcpy(s->data + s->len, buffer, size);

    s->len += size;
    s->data[s->len] = '\0';

    return 0;
}

/* Append a single character to s. Returns 0 on success, -1 on failure. */

int string_append_char(string_t *s, char c)
{
    if (!s) return -1;

    size_t need = s->len + 2;
    if (string_reserve(s, need) != 0) return -1;

    s->data[s->len++] = c;
    s->data[s->len] = '\0';
    return 0;
}

/* Insert `text` at byte offset `pos`, shifting existing content right.
   If pos > len it is clamped to len (append). Returns 0 on success, -1 on failure. */

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

/* Strip leading and trailing ASCII whitespace (any byte <= 0x20) in-place. */

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

/* Append a printf-style formatted string using a va_list. The format is applied
   via a double-pass vsnprintf to compute the exact size before writing.
   Returns the number of characters appended, or -1 on error. */

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

/* Append a printf-style formatted string to s. Returns the number of characters
   appended, or -1 on error. */

int string_printf(string_t *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = string_vprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

/* Alias for string_printf — appends a formatted string to s. */

int string_append_format(string_t *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = string_vprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

/* Return the byte offset of the first occurrence of `needle` in s, or -1 if
   not found, needle is empty, or either argument is NULL. */

ssize_t string_find(const string_t *s, const char *needle)
{
    if (!s || !needle || *needle == '\0')
        return -1;

    const char *p = strstr(s->data, needle);
    if (!p)
        return -1;

    return (ssize_t)(p - s->data);
}

/* Lexicographically compare a and b. Returns <0, 0, or >0. NULL sorts before
   any non-NULL string. */

int string_compare(const string_t *a, const string_t *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a->data, b->data);
}

/* Return true if s begins with `prefix`. */

bool string_starts_with(const string_t *s, const char *prefix)
{
    if (!s || !prefix) return 0;

    size_t plen = strlen(prefix);
    if (plen > s->len) return 0;

    return memcmp(s->data, prefix, plen) == 0;
}

/* Return true if s ends with `suffix`. */

bool string_ends_with(const string_t *s, const char *suffix)
{
    if (!s || !suffix) return 0;

    size_t slen = strlen(suffix);
    if (slen > s->len) return 0;

    return memcmp(s->data + (s->len - slen), suffix, slen) == 0;
}

/* Return a new string containing `len` bytes starting at `pos`. Both pos and
   len are clamped to the available content. Returns NULL on failure. */

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

/* Reverse the string in-place. */

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

/* Replace every occurrence of `search` with `replace` in-place. Uses a
   two-pass approach: count occurrences first, then rebuild into a temp buffer.
   Returns 0 on success (including no matches), -1 on bad arguments or
   allocation failure. */

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

    free(s->data);
    s->data = temp;
    s->len  = new_len;
    s->cap  = new_len + 1;
    return 0;
}

/* Convert all ASCII lowercase letters to uppercase in-place. */

void string_to_upper(string_t *s)
{
    if (!s) return;

    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->data[i];
        if (c >= 'a' && c <= 'z')
            s->data[i] = (char)(c - ('a' - 'A'));
    }
}

/* Convert all ASCII uppercase letters to lowercase in-place. */

void string_to_lower(string_t *s)
{
    if (!s) return;

    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->data[i];
        if (c >= 'A' && c <= 'Z')
            s->data[i] = (char)(c + ('a' - 'A'));
    }
}

/* Compute an FNV-1a hash of the string contents. Returns 0 for NULL. */

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

/* Split s on any character in `delim`, returning a heap-allocated array of
   strings. The count is written to *out_count. The array must be freed with
   string_split_free(). Returns NULL on bad arguments or allocation failure. */

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

/* Free an array returned by string_split, including each element. */

void string_split_free(string_t **arr, size_t count)
{
    if (!arr) return;
    for (size_t i = 0; i < count; i++)
        string_free(arr[i]);
    free(arr);
}

/* Concatenate `count` strings from `arr`, inserting `sep` between each.
   Returns a new string, or NULL on allocation failure. */

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

