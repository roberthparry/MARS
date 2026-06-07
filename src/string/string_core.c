/* string_core.c - memory management, mutation, and formatting for string_t
 *
 * Implements the lifecycle and core mutation operations for the dynamic UTF-8
 * string type defined in ustring.h:
 *
 *   Allocation/free  — string_new, string_new_with, string_clone, string_free
 *   Capacity         — string_reserve (internal), the doubling growth policy
 *   Append           — string_append_cstr, string_append_char,
 *                      string_append_format
 *   Insert/remove    — string_insert, string_remove
 *   Accessors        — string_c_str, string_length, string_is_empty
 *
 * All functions that accept or produce C strings assume valid UTF-8.
 * No transcoding is performed; bytes are copied as-is.
 */

#include <stdbool.h>
#include <limits.h>

#ifdef HAVE_UNISTRING
#include <unicase.h>
#endif

#include <ctype.h>
#include <stdio.h>

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
    free(s->scratch);
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
    return suffix
        ? string_append_view(s, string_view_from_chars(suffix, strlen(suffix)))
        : -1;
}

int string_append_string(string_t *s, const string_t *suffix)
{
    return suffix
        ? string_append_view(s, string_view_all(suffix))
        : -1;
}

int string_append_view(string_t *s, string_view_t suffix)
{
    const char *data = string_view_data(suffix);
    size_t add = string_view_length(suffix);
    size_t need;

    if (!s || (!data && add > 0u))
        return -1;
    if (add == 0u)
        return 0;

    need = s->len + add + 1u;
    if (string_reserve(s, need) != 0)
        return -1;

    memcpy(s->data + s->len, data, add);
    s->len += add;
    s->data[s->len] = '\0';
    return string_normalise_storage(s);
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

    return string_normalise_storage(s);
}

/* Append a single character to s. Returns 0 on success, -1 on failure. */

int string_append_char(string_t *s, char c)
{
    if (!s) return -1;

    size_t need = s->len + 2;
    if (string_reserve(s, need) != 0) return -1;

    s->data[s->len++] = c;
    s->data[s->len] = '\0';
    return string_normalise_storage(s);
}

/* Insert `text` at byte offset `pos`, shifting existing content right.
   If pos > len it is clamped to len (append). Returns 0 on success, -1 on failure. */

int string_insert(string_t *s, size_t pos, const char *text)
{
    return (s && text)
        ? string_insert_view(s,
                             string_character_byte_offset(s, pos),
                             string_view_from_chars(text, strlen(text)))
        : -1;
}

int string_insert_string(string_t *s, size_t pos, const string_t *text)
{
    return (s && text)
        ? string_insert_view(s,
                             string_character_byte_offset(s, pos),
                             string_view_all(text))
        : -1;
}

int string_insert_view(string_t *s, size_t pos, string_view_t text)
{
    const char *data = string_view_data(text);
    size_t add = string_view_length(text);
    size_t need;

    if (!s || (!data && add > 0u))
        return -1;
    if (pos > s->len)
        pos = s->len;
    if (add == 0u)
        return 0;

    need = s->len + add + 1u;
    if (string_reserve(s, need) != 0)
        return -1;

    memmove(s->data + pos + add, s->data + pos, s->len - pos + 1);
    memcpy(s->data + pos, data, add);

    s->len += add;
    return string_normalise_storage(s);
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
    (void)string_normalise_storage(s);
}

static bool string_format_has_string_module_specifier(const char *fmt)
{
    const char *p = fmt;

    while (p && *p) {
        if (*p++ != '%')
            continue;
        if (*p == '%') {
            p++;
            continue;
        }
        while (*p && strchr("-+ #0", *p))
            p++;
        if (*p == '*')
            p++;
        else
            while (isdigit((unsigned char)*p))
                p++;
        if (*p == '.') {
            p++;
            if (*p == '*')
                p++;
            else
                while (isdigit((unsigned char)*p))
                    p++;
        }
        if (p[0] == 'h' && p[1] == 'h')
            p += 2;
        else if (p[0] == 'l' && p[1] == 'l')
            p += 2;
        else if (*p && strchr("hljztL", *p))
            p++;
        if (*p == 'S' || *p == 'W' || *p == 'R')
            return true;
        if (*p)
            p++;
    }

    return false;
}

static int string_append_printf_piece(string_t *s,
                                      const char *fmt,
                                      ...)
{
    int n;
    int written;
    size_t need;
    va_list ap;
    va_list ap2;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) {
        va_end(ap);
        return -1;
    }

    need = s->len + (size_t)n + 1u;
    if (string_reserve(s, need) != 0) {
        va_end(ap);
        return -1;
    }

    written = vsnprintf(s->data + s->len, (size_t)n + 1u, fmt, ap);
    va_end(ap);
    if (written < 0)
        return -1;

    s->len += (size_t)written;
    return written;
}

static int string_format_append_literal(string_t *s,
                                        const char *start,
                                        size_t len)
{
    if (len == 0u)
        return 0;
    return string_append_chars(s, start, len) == 0 ? (int)len : -1;
}

static int string_format_spec_has_integer_conversion(char conv)
{
    return conv == 'd' || conv == 'i' || conv == 'u' ||
           conv == 'o' || conv == 'x' || conv == 'X';
}

static int string_format_spec_has_float_conversion(char conv)
{
    return conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' ||
           conv == 'g' || conv == 'G' || conv == 'a' || conv == 'A';
}

static int string_format_append_standard_conversion(string_t *s,
                                                    const char *spec_start,
                                                    size_t spec_len,
                                                    char conv,
                                                    const char *length,
                                                    int width_from_arg,
                                                    int precision_from_arg,
                                                    va_list ap)
{
    char spec[128];
    int width = 0;
    int precision = 0;

    if (spec_len >= sizeof(spec))
        return -1;
    memcpy(spec, spec_start, spec_len);
    spec[spec_len] = '\0';

    if (width_from_arg)
        width = va_arg(ap, int);
    if (precision_from_arg)
        precision = va_arg(ap, int);

#define STRING_APPEND_FORMATTED(value) \
    do { \
        if (width_from_arg && precision_from_arg) \
            return string_append_printf_piece(s, spec, width, precision, (value)); \
        if (width_from_arg) \
            return string_append_printf_piece(s, spec, width, (value)); \
        if (precision_from_arg) \
            return string_append_printf_piece(s, spec, precision, (value)); \
        return string_append_printf_piece(s, spec, (value)); \
    } while (0)

    if (conv == 's') {
        const char *value = va_arg(ap, const char *);
        STRING_APPEND_FORMATTED(value);
    }
    if (conv == 'c') {
        int value = va_arg(ap, int);
        STRING_APPEND_FORMATTED(value);
    }
    if (conv == 'p') {
        void *value = va_arg(ap, void *);
        STRING_APPEND_FORMATTED(value);
    }
    if (string_format_spec_has_integer_conversion(conv)) {
        if (strcmp(length, "hh") == 0 || strcmp(length, "h") == 0 ||
            length[0] == '\0') {
            if (conv == 'd' || conv == 'i') {
                int value = va_arg(ap, int);
                STRING_APPEND_FORMATTED(value);
            } else {
                unsigned int value = va_arg(ap, unsigned int);
                STRING_APPEND_FORMATTED(value);
            }
        } else if (strcmp(length, "l") == 0) {
            if (conv == 'd' || conv == 'i') {
                long value = va_arg(ap, long);
                STRING_APPEND_FORMATTED(value);
            } else {
                unsigned long value = va_arg(ap, unsigned long);
                STRING_APPEND_FORMATTED(value);
            }
        } else if (strcmp(length, "ll") == 0) {
            if (conv == 'd' || conv == 'i') {
                long long value = va_arg(ap, long long);
                STRING_APPEND_FORMATTED(value);
            } else {
                unsigned long long value = va_arg(ap, unsigned long long);
                STRING_APPEND_FORMATTED(value);
            }
        } else if (strcmp(length, "z") == 0) {
            if (conv == 'd' || conv == 'i') {
                ptrdiff_t value = va_arg(ap, ptrdiff_t);
                STRING_APPEND_FORMATTED(value);
            } else {
                size_t value = va_arg(ap, size_t);
                STRING_APPEND_FORMATTED(value);
            }
        } else if (strcmp(length, "t") == 0) {
            ptrdiff_t value = va_arg(ap, ptrdiff_t);
            STRING_APPEND_FORMATTED(value);
        } else if (strcmp(length, "j") == 0) {
            if (conv == 'd' || conv == 'i') {
                intmax_t value = va_arg(ap, intmax_t);
                STRING_APPEND_FORMATTED(value);
            } else {
                uintmax_t value = va_arg(ap, uintmax_t);
                STRING_APPEND_FORMATTED(value);
            }
        }
        return -1;
    }
    if (string_format_spec_has_float_conversion(conv)) {
        if (strcmp(length, "L") == 0) {
            long double value = va_arg(ap, long double);
            STRING_APPEND_FORMATTED(value);
        } else {
            double value = va_arg(ap, double);
            STRING_APPEND_FORMATTED(value);
        }
    }

#undef STRING_APPEND_FORMATTED

    return -1;
}

static int string_append_vformat_with_string_module_objects(string_t *s,
                                                            const char *fmt,
                                                            va_list ap)
{
    const char *literal = fmt;
    const char *p = fmt;
    int total = 0;

    while (*p) {
        const char *spec_start;
        char length[3] = { 0, 0, 0 };
        char conv;
        int width_from_arg = 0;
        int precision_from_arg = 0;
        int appended;

        if (*p != '%') {
            p++;
            continue;
        }

        appended = string_format_append_literal(s, literal, (size_t)(p - literal));
        if (appended < 0)
            return -1;
        total += appended;

        spec_start = p++;
        if (*p == '%') {
            if (string_append_char(s, '%') != 0)
                return -1;
            total++;
            p++;
            literal = p;
            continue;
        }

        while (*p && strchr("-+ #0", *p))
            p++;
        if (*p == '*') {
            width_from_arg = 1;
            p++;
        } else {
            while (isdigit((unsigned char)*p))
                p++;
        }
        if (*p == '.') {
            p++;
            if (*p == '*') {
                precision_from_arg = 1;
                p++;
            } else {
                while (isdigit((unsigned char)*p))
                    p++;
            }
        }
        if (p[0] == 'h' && p[1] == 'h') {
            length[0] = 'h';
            length[1] = 'h';
            p += 2;
        } else if (p[0] == 'l' && p[1] == 'l') {
            length[0] = 'l';
            length[1] = 'l';
            p += 2;
        } else if (*p && strchr("hljztL", *p)) {
            length[0] = *p++;
        }

        conv = *p;
        if (!conv)
            return -1;
        p++;

        if (conv == 'S') {
            const string_t *value;

            if (width_from_arg)
                (void)va_arg(ap, int);
            if (precision_from_arg)
                (void)va_arg(ap, int);
            if (length[0] != '\0')
                return -1;
            value = va_arg(ap, const string_t *);
            appended = value
                ? string_append_string(s, value)
                : string_append_cstr(s, "(null)");
            if (appended != 0)
                return -1;
            total += value ? (int)string_length(value) : 6;
        } else if (conv == 'W') {
            string_view_t value;
            size_t before;

            if (width_from_arg)
                (void)va_arg(ap, int);
            if (precision_from_arg)
                (void)va_arg(ap, int);
            if (length[0] != '\0')
                return -1;
            value = va_arg(ap, string_view_t);
            before = string_length(s);
            if (string_append_view(s, value) != 0)
                return -1;
            total += (int)(string_length(s) - before);
        } else if (conv == 'R') {
            rune_t value;
            size_t before;

            if (width_from_arg)
                (void)va_arg(ap, int);
            if (precision_from_arg)
                (void)va_arg(ap, int);
            if (length[0] != '\0')
                return -1;
            value = va_arg(ap, rune_t);
            if (rune_is_empty(value))
                appended = 0;
            else {
                before = string_length(s);
                if (string_append_rune(s, value) != 0)
                    return -1;
                appended = (int)(string_length(s) - before);
            }
            total += appended;
        } else if (conv == 'n') {
            return -1;
        } else {
            appended = string_format_append_standard_conversion(
                s,
                spec_start,
                (size_t)(p - spec_start),
                conv,
                length,
                width_from_arg,
                precision_from_arg,
                ap);
            if (appended < 0)
                return -1;
            total += appended;
        }

        literal = p;
    }

    {
        int appended = string_format_append_literal(s, literal, (size_t)(p - literal));
        if (appended < 0)
            return -1;
        total += appended;
    }

    if (string_normalise_storage(s) != 0)
        return -1;
    return total;
}

/* Append a printf-style formatted string using a va_list. The format is applied
   via a double-pass vsnprintf to compute the exact size before writing.
   The string module also owns %S, %W, and %R: they append const string_t *,
   string_view_t, and rune_t arguments respectively.
   Returns the number of characters appended, or -1 on error. */

int string_append_vformat(string_t *s, const char *fmt, va_list ap)
{
    if (!s || !fmt) return -1;

    if (string_format_has_string_module_specifier(fmt)) {
        va_list ap2;
        int result;

        va_copy(ap2, ap);
        result = string_append_vformat_with_string_module_objects(s, fmt, ap2);
        va_end(ap2);
        return result;
    }

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
    if (string_normalise_storage(s) != 0)
        return -1;
    return written;
}

/* Append a printf-style formatted string to s. */

int string_append_format(string_t *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = string_append_vformat(s, fmt, ap);
    va_end(ap);
    return r;
}

string_t *string_vsprintf(const char *fmt, va_list ap)
{
    string_t *out;
    va_list ap2;

    if (!fmt)
        return NULL;

    out = string_new();
    if (!out)
        return NULL;

    va_copy(ap2, ap);
    if (string_append_vformat(out, fmt, ap2) < 0) {
        va_end(ap2);
        string_free(out);
        return NULL;
    }
    va_end(ap2);
    return out;
}

string_t *string_sprintf(const char *fmt, ...)
{
    string_t *out;
    va_list ap;

    va_start(ap, fmt);
    out = string_vsprintf(fmt, ap);
    va_end(ap);
    return out;
}

int string_printf(const char *fmt, ...)
{
    string_t *out;
    size_t len;
    va_list ap;

    va_start(ap, fmt);
    out = string_vsprintf(fmt, ap);
    va_end(ap);
    if (!out)
        return -1;

    len = string_length(out);
    if (len > (size_t)INT_MAX) {
        string_free(out);
        return -1;
    }

    if (fputs(string_c_str(out), stdout) == EOF) {
        string_free(out);
        return -1;
    }

    string_free(out);
    return (int)len;
}

/* Return the byte offset of the first occurrence of `needle` in s, or -1 if
   not found, needle is empty, or either argument is NULL. */

static const char *string_find_bytes(const char *haystack,
                                     size_t haystack_len,
                                     const char *needle,
                                     size_t needle_len)
{
    if (!haystack || !needle || needle_len == 0u || needle_len > haystack_len)
        return NULL;

    for (size_t i = 0u; i <= haystack_len - needle_len; ++i) {
        if (memcmp(haystack + i, needle, needle_len) == 0)
            return haystack + i;
    }

    return NULL;
}

string_offset_t string_find(const string_t *s, const char *needle)
{
    return needle
        ? string_find_view(s, string_view_from_chars(needle, strlen(needle)))
        : -1;
}

string_offset_t string_find_string(const string_t *s, const string_t *needle)
{
    return needle
        ? string_find_view(s, string_view_all(needle))
        : -1;
}

string_offset_t string_find_view(const string_t *s, string_view_t needle)
{
    const char *needle_data = string_view_data(needle);
    size_t needle_len = string_view_length(needle);
    const char *p;

    if (!s || !needle_data || needle_len == 0u)
        return -1;

    p = string_find_bytes(s->data, s->len, needle_data, needle_len);
    if (!p)
        return -1;

    return (string_offset_t)(p - s->data);
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
    return prefix
        ? string_starts_with_view(s, string_view_from_chars(prefix, strlen(prefix)))
        : false;
}

bool string_starts_with_string(const string_t *s, const string_t *prefix)
{
    return prefix
        ? string_starts_with_view(s, string_view_all(prefix))
        : false;
}

bool string_starts_with_view(const string_t *s, string_view_t prefix)
{
    const char *prefix_data = string_view_data(prefix);
    size_t prefix_len = string_view_length(prefix);

    if (!s || !prefix_data || prefix_len > s->len)
        return false;

    return memcmp(s->data, prefix_data, prefix_len) == 0;
}

/* Return true if s ends with `suffix`. */

bool string_ends_with(const string_t *s, const char *suffix)
{
    return suffix
        ? string_ends_with_view(s, string_view_from_chars(suffix, strlen(suffix)))
        : false;
}

bool string_ends_with_string(const string_t *s, const string_t *suffix)
{
    return suffix
        ? string_ends_with_view(s, string_view_all(suffix))
        : false;
}

bool string_ends_with_view(const string_t *s, string_view_t suffix)
{
    const char *suffix_data = string_view_data(suffix);
    size_t suffix_len = string_view_length(suffix);

    if (!s || !suffix_data || suffix_len > s->len)
        return false;

    return memcmp(s->data + (s->len - suffix_len),
                  suffix_data,
                  suffix_len) == 0;
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

/* Reverse the string in-place by user-visible characters. */

void string_reverse(string_t *s)
{
    string_grapheme_reverse(s);
}

/* Replace every occurrence of `search` with `replace` in-place. Uses a
   two-pass approach: count occurrences first, then rebuild into a temp buffer.
   Returns 0 on success (including no matches), -1 on bad arguments or
   allocation failure. */

int string_replace(string_t *s, const char *search, const char *replace)
{
    return (search && replace)
        ? string_replace_view(s,
                              string_view_from_chars(search, strlen(search)),
                              string_view_from_chars(replace, strlen(replace)))
        : -1;
}

int string_replace_string(string_t *s,
                          const string_t *search,
                          const string_t *replace)
{
    return (search && replace)
        ? string_replace_view(s, string_view_all(search), string_view_all(replace))
        : -1;
}

int string_replace_view(string_t *s, string_view_t search, string_view_t replace)
{
    const char *search_data = string_view_data(search);
    const char *replace_data = string_view_data(replace);
    size_t search_len = string_view_length(search);
    size_t replace_len = string_view_length(replace);
    size_t count = 0;
    const char *p;
    size_t new_len;
    char *temp;
    char *out;
    const char *in;

    if (!s || !search_data || (!replace_data && replace_len > 0u))
        return -1;
    if (search_len == 0u)
        return 0;

    p = s->data;
    while ((p = string_find_bytes(p, (size_t)((s->data + s->len) - p),
                                  search_data, search_len)) != NULL) {
        count++;
        p += search_len;
    }

    if (count == 0)
        return 0;

    new_len = replace_len >= search_len
        ? s->len + count * (replace_len - search_len)
        : s->len - count * (search_len - replace_len);

    temp = malloc(new_len + 1);
    if (!temp) return -1;

    out = temp;
    in = s->data;

    while ((p = string_find_bytes(in, (size_t)((s->data + s->len) - in),
                                  search_data, search_len)) != NULL) {
        size_t prefix_len = (size_t)(p - in);
        memcpy(out, in, prefix_len);
        out += prefix_len;

        if (replace_len > 0u)
            memcpy(out, replace_data, replace_len);
        out += replace_len;

        in = p + search_len;
    }

    memcpy(out, in, (size_t)((s->data + s->len) - in));
    out += (size_t)((s->data + s->len) - in);
    *out = '\0';

    free(s->data);
    s->data = temp;
    s->len  = new_len;
    s->cap  = new_len + 1;
    return string_normalise_storage(s);
}

static int string_map_case(string_t *s, int to_upper)
{
    if (!s)
        return -1;

#ifdef HAVE_UNISTRING
    size_t out_len = 0u;
    uint8_t *mapped = to_upper
        ? u8_toupper((const uint8_t *)s->data, s->len, NULL, NULL, NULL, &out_len)
        : u8_tolower((const uint8_t *)s->data, s->len, NULL, NULL, NULL, &out_len);

    if (!mapped)
        return -1;

    if (string_reserve(s, out_len + 1u) != 0) {
        free(mapped);
        return -1;
    }

    memcpy(s->data, mapped, out_len);
    s->data[out_len] = '\0';
    s->len = out_len;
    free(mapped);
    return string_normalise_storage(s);
#else
    for (size_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->data[i];
        if (to_upper) {
            if (c >= 'a' && c <= 'z')
                s->data[i] = (char)(c - ('a' - 'A'));
        } else {
            if (c >= 'A' && c <= 'Z')
                s->data[i] = (char)(c + ('a' - 'A'));
        }
    }
    return 0;
#endif
}

/* Convert text to uppercase in-place. */

void string_to_upper(string_t *s)
{
    (void)string_map_case(s, 1);
}

/* Convert text to lowercase in-place. */

void string_to_lower(string_t *s)
{
    (void)string_map_case(s, 0);
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

string_t **string_split_string(const string_t *s,
                               const string_t *delim,
                               size_t *out_count)
{
    return delim ? string_split_by_view(s, string_view_all(delim), out_count)
                 : NULL;
}

string_t **string_split_by_view(const string_t *s,
                                string_view_t delim,
                                size_t *out_count)
{
    const char *delim_data = string_view_data(delim);
    size_t delim_len = string_view_length(delim);
    const char *start;
    const char *end;
    size_t cap = 8u;
    size_t count = 0u;
    string_t **arr;

    if (!s || !out_count || !delim_data || delim_len == 0u)
        return NULL;

    arr = malloc(cap * sizeof(*arr));
    if (!arr)
        return NULL;

    start = s->data;
    while ((end = string_find_bytes(start,
                                    (size_t)((s->data + s->len) - start),
                                    delim_data,
                                    delim_len)) != NULL) {
        if (count == cap) {
            string_t **tmp;
            cap *= 2u;
            tmp = realloc(arr, cap * sizeof(*arr));
            if (!tmp) {
                string_split_free(arr, count);
                return NULL;
            }
            arr = tmp;
        }

        arr[count] = string_new();
        if (!arr[count]) {
            string_split_free(arr, count);
            return NULL;
        }
        if (string_append_view(arr[count],
                               string_view_from_region(start, end)) != 0) {
            string_split_free(arr, count + 1u);
            return NULL;
        }
        count++;
        start = end + delim_len;
    }

    if (count == cap) {
        string_t **tmp = realloc(arr, cap * 2u * sizeof(*arr));
        if (!tmp) {
            string_split_free(arr, count);
            return NULL;
        }
        arr = tmp;
    }

    arr[count] = string_new();
    if (!arr[count]) {
        string_split_free(arr, count);
        return NULL;
    }
    if (string_append_view(arr[count],
                           string_view_from_region(start, s->data + s->len)) != 0) {
        string_split_free(arr, count + 1u);
        return NULL;
    }
    count++;

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
    return sep
        ? string_join_with_view(arr,
                                count,
                                string_view_from_chars(sep, strlen(sep)))
        : string_join_with_view(arr,
                                count,
                                string_view_from_chars(NULL, 0u));
}

string_t *string_join_string(string_t **arr,
                             size_t count,
                             const string_t *sep)
{
    return sep ? string_join_with_view(arr, count, string_view_all(sep))
               : string_join_with_view(arr, count, string_view_from_chars(NULL, 0u));
}

string_t *string_join_with_view(string_t **arr,
                                size_t count,
                                string_view_t sep)
{
    size_t sep_len = string_view_length(sep);
    if (!arr || count == 0) return string_new_with("");

    string_t *out = string_new();
    if (!out) return NULL;

    for (size_t i = 0; i < count; i++) {
        if (string_append_view(out, string_view_all(arr[i])) != 0) {
            string_free(out);
            return NULL;
        }
        if (i + 1 < count && sep_len > 0)
            if (string_append_view(out, sep) != 0) {
                string_free(out);
                return NULL;
            }
    }

    return out;
}
