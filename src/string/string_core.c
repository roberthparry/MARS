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
 * Most C-string boundary helpers accept UTF-8. string_new_wide() is the
 * explicit wide-string boundary and converts wchar_t input into UTF-8.
 */

#include <stdbool.h>
#include <limits.h>

#ifdef HAVE_UNISTRING
#include <unicase.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <wchar.h>

#define MARS_STRING_INTERNAL_ACCESS
#include "string_internal.h"
#include "ustring.h"

static int string_append_utf8_scalar(string_t *s, uint32_t value)
{
    char bytes[4];
    size_t count;

    if (!s)
        return -1;

    if (value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu))
        value = 0xFFFDu;

    if (value <= 0x7Fu) {
        bytes[0] = (char)value;
        count = 1u;
    } else if (value <= 0x7FFu) {
        bytes[0] = (char)(0xC0u | (value >> 6));
        bytes[1] = (char)(0x80u | (value & 0x3Fu));
        count = 2u;
    } else if (value <= 0xFFFFu) {
        bytes[0] = (char)(0xE0u | (value >> 12));
        bytes[1] = (char)(0x80u | ((value >> 6) & 0x3Fu));
        bytes[2] = (char)(0x80u | (value & 0x3Fu));
        count = 3u;
    } else {
        bytes[0] = (char)(0xF0u | (value >> 18));
        bytes[1] = (char)(0x80u | ((value >> 12) & 0x3Fu));
        bytes[2] = (char)(0x80u | ((value >> 6) & 0x3Fu));
        bytes[3] = (char)(0x80u | (value & 0x3Fu));
        count = 4u;
    }

    return string_append_chars(s, bytes, count);
}

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

string_t *string_new_wide(const wchar_t *init)
{
    string_t *s;
    size_t i;

    if (!init)
        return NULL;

    s = string_new();
    if (!s)
        return NULL;

    for (i = 0u; init[i] != L'\0'; ++i) {
        uint32_t value = (uint32_t)init[i];

        if (sizeof(wchar_t) == 2u &&
            value >= 0xD800u && value <= 0xDBFFu) {
            uint32_t next = (uint32_t)init[i + 1u];

            if (next >= 0xDC00u && next <= 0xDFFFu) {
                value = 0x10000u +
                        (((value - 0xD800u) << 10) | (next - 0xDC00u));
                ++i;
            } else {
                value = 0xFFFDu;
            }
        } else if (sizeof(wchar_t) == 2u &&
                   value >= 0xDC00u && value <= 0xDFFFu) {
            value = 0xFFFDu;
        }

        if (string_append_utf8_scalar(s, value) != 0) {
            string_free(s);
            return NULL;
        }
    }

    return s;
}

/* Return a deep copy of `src`, or NULL if src is NULL or allocation fails. */

string_t *string_clone(const string_t *src)
{
    string_t *clone;

    if (!src)
        return NULL;

    clone = string_new();
    if (!clone)
        return NULL;

    if (string_append_string(clone, src) != 0) {
        string_free(clone);
        return NULL;
    }

    return clone;
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

/* Return the encoded length of the string. Returns 0 if s is NULL. */

size_t string_encoded_len(const string_t *s)
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

void string_trim(string_t *s)
{
    string_view_t trimmed_view;
    string_t *trimmed;
    char *old_data;
    char *old_scratch;

    if (!s || s->len == 0)
        return;

    trimmed_view = string_view_trim(string_view_all(s));
    trimmed = string_from_view(&trimmed_view);
    if (!trimmed)
        return;

    old_data = s->data;
    old_scratch = s->scratch;

    s->data = trimmed->data;
    s->len = trimmed->len;
    s->cap = trimmed->cap;
    s->scratch = trimmed->scratch;
    s->scratch_cap = trimmed->scratch_cap;

    trimmed->data = old_data;
    trimmed->len = 0u;
    trimmed->cap = old_data ? 1u : 0u;
    trimmed->scratch = old_scratch;
    trimmed->scratch_cap = 0u;
    string_free(trimmed);
}

#define STRING_FORMAT_CONV_BASE 'A'
#define STRING_FORMAT_CONV_LAST 'z'
#define STRING_FORMAT_CONV_RANGE \
    ((unsigned int)((unsigned char)STRING_FORMAT_CONV_LAST - \
                    (unsigned char)STRING_FORMAT_CONV_BASE))

/* Bits are indexed from 'A'; the punctuation gap between 'Z' and 'a' is zero. */
static const uint64_t STRING_FORMAT_STANDARD_MASK = UINT64_C(0x94e17d00800071); /* AEFGXacdefginopsux */
static const uint64_t STRING_FORMAT_INTEGER_MASK  = UINT64_C(0x90410800800000); /* Xdioux */
static const uint64_t STRING_FORMAT_FLOAT_MASK    = UINT64_C(0x00007100000071); /* AEFGaefg */

static bool string_format_char_in_mask(char conv, uint64_t mask)
{
    unsigned int bit = (unsigned int)((unsigned char)conv -
                                      (unsigned char)STRING_FORMAT_CONV_BASE);

    if (bit > STRING_FORMAT_CONV_RANGE)
        return false;

    return ((mask >> bit) & UINT64_C(1)) != 0u;
}

static int string_format_char_is_standard_conversion(char conv)
{
    return string_format_char_in_mask(conv, STRING_FORMAT_STANDARD_MASK);
}

static bool string_format_cursor_peek_ascii(const string_cursor_t *cursor,
                                            unsigned char *out)
{
    return cursor && string_cursor_peek_ascii(cursor, out);
}

static bool string_format_cursor_peek_char(const string_cursor_t *cursor,
                                           char expected)
{
    unsigned char actual = 0u;

    return string_format_cursor_peek_ascii(cursor, &actual) &&
           actual == (unsigned char)expected;
}

static bool string_format_cursor_consume_char(string_cursor_t *cursor,
                                              char expected)
{
    if (!string_format_cursor_peek_char(cursor, expected))
        return false;
    return string_cursor_next(cursor) == 0;
}

static bool string_format_cursor_peek_ascii_at(const string_cursor_t *cursor,
                                               string_pos_t pos,
                                               unsigned char *out)
{
    return cursor && string_cursor_peek_ascii_at(cursor, pos, out);
}

static char string_format_cursor_ascii_at(const string_cursor_t *cursor,
                                          string_pos_t pos)
{
    unsigned char value = 0u;

    return string_format_cursor_peek_ascii_at(cursor, pos, &value)
        ? (char)value
        : '\0';
}

static bool string_format_cursor_skip(string_cursor_t *cursor,
                                      string_pos_t span)
{
    return cursor && string_cursor_skip(cursor, span);
}

static bool string_format_has_string_module_specifier(const char *fmt)
{
    string_t *format;
    string_cursor_t *cursor;
    bool found = false;

    if (!fmt)
        return false;

    format = string_new_with(fmt);
    if (!format)
        return true;

    cursor = string_cursor_new(format);
    if (!cursor) {
        string_free(format);
        return true;
    }

    while (!string_cursor_done(cursor)) {
        unsigned char ch = 0u;

        if (!string_format_cursor_peek_ascii(cursor, &ch))
            goto advance;
        if (ch != '%')
            goto advance;
        if (string_cursor_next(cursor) != 0)
            break;
        if (string_format_cursor_consume_char(cursor, '%'))
            continue;

        while (string_format_cursor_peek_ascii(cursor, &ch) &&
               strchr("-+ #0", (int)ch)) {
            if (string_cursor_next(cursor) != 0)
                goto done;
        }
        if (string_format_cursor_consume_char(cursor, '*')) {
            /* Width is supplied by the argument list. */
        } else {
            while (string_format_cursor_peek_ascii(cursor, &ch) &&
                   isdigit((unsigned char)ch)) {
                if (string_cursor_next(cursor) != 0)
                    goto done;
            }
        }
        if (string_format_cursor_consume_char(cursor, '.')) {
            if (string_format_cursor_consume_char(cursor, '*')) {
                /* Precision is supplied by the argument list. */
            } else {
                while (string_format_cursor_peek_ascii(cursor, &ch) &&
                       isdigit((unsigned char)ch)) {
                    if (string_cursor_next(cursor) != 0)
                        goto done;
                }
            }
        }
        {
            string_pos_t pos = string_cursor_position(cursor);
            char c0 = string_format_cursor_ascii_at(cursor, pos);
            char c1 = string_format_cursor_ascii_at(cursor, pos + 1u);
            char c2 = string_format_cursor_ascii_at(cursor, pos + 2u);

            if (c0 == 'h' && c1 == 'h' &&
                string_format_char_is_standard_conversion(c2)) {
                if (!string_format_cursor_skip(cursor, 2u))
                    goto done;
            } else if (c0 == 'l' && c1 == 'l' &&
                       string_format_char_is_standard_conversion(c2)) {
                if (!string_format_cursor_skip(cursor, 2u))
                    goto done;
            } else if (c0 != '\0' && strchr("hljztL", c0) &&
                       string_format_char_is_standard_conversion(c1)) {
                if (string_cursor_next(cursor) != 0)
                    goto done;
            }
        }
        if (string_format_cursor_peek_ascii(cursor, &ch) &&
            (ch == 'S' || ch == 'W' || ch == 'R')) {
            found = true;
            goto done;
        }

advance:
        if (string_cursor_next(cursor) != 0)
            break;
    }

done:
    string_cursor_free(cursor);
    string_free(format);
    return found;
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

static int string_format_append_cursor_literal(string_t *s,
                                               string_pos_t start,
                                               string_pos_t end,
                                               const string_cursor_t *cursor)
{
    size_t before;

    if (start == end)
        return 0;
    before = string_encoded_len(s);
    if (string_cursor_append_slice_between(s, start, end, cursor) != 0)
        return -1;
    return (int)(string_encoded_len(s) - before);
}

static int string_format_spec_has_integer_conversion(char conv)
{
    return string_format_char_in_mask(conv, STRING_FORMAT_INTEGER_MASK);
}

static int string_format_spec_has_float_conversion(char conv)
{
    return string_format_char_in_mask(conv, STRING_FORMAT_FLOAT_MASK);
}

static int string_format_append_standard_conversion(string_t *s,
                                                    const string_t *spec_text,
                                                    char conv,
                                                    const char *length,
                                                    int width_from_arg,
                                                    int precision_from_arg,
                                                    va_list ap)
{
    const char *spec;
    int width = 0;
    int precision = 0;

    if (!spec_text)
        return -1;
    spec = string_c_str(spec_text);

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

static int string_append_vformat_with_string_module_objects(
    string_t *s,
    const char *fmt,
    va_list ap,
    string_format_callback_t callback,
    void *callback_user)
{
    string_t *format = NULL;
    string_cursor_t *cursor = NULL;
    string_pos_t literal_start;
    int total = 0;
    int result = -1;

    format = string_new_with(fmt);
    if (!format)
        return -1;
    cursor = string_cursor_new(format);
    if (!cursor)
        goto done;

    literal_start = string_cursor_position(cursor);

    while (!string_cursor_done(cursor)) {
        string_pos_t spec_start;
        char length[3] = { 0, 0, 0 };
        char conv;
        unsigned char ch = 0u;
        int width_from_arg = 0;
        int precision_from_arg = 0;
        int appended;
        string_format_spec_t spec = {
            .conversion = '\0',
            .trailing_modifier = '\0',
            .flag_left = false,
            .flag_sign = false,
            .flag_space = false,
            .flag_alternate = false,
            .flag_zero = false,
            .width = 0,
            .precision = -1,
            .width_from_argument = false,
            .precision_from_argument = false,
            .length = { 0, 0, 0 }
        };

        if (!string_format_cursor_peek_ascii(cursor, &ch) || ch != '%') {
            if (string_cursor_next(cursor) != 0)
                goto done;
            continue;
        }

        appended = string_format_append_cursor_literal(
            s,
            literal_start,
            string_cursor_position(cursor),
            cursor);
        if (appended < 0)
            goto done;
        total += appended;

        spec_start = string_cursor_position(cursor);
        if (string_cursor_next(cursor) != 0)
            goto done;
        if (string_format_cursor_consume_char(cursor, '%')) {
            if (string_append_char(s, '%') != 0)
                goto done;
            total++;
            literal_start = string_cursor_position(cursor);
            continue;
        }

        while (string_format_cursor_peek_ascii(cursor, &ch) &&
               strchr("-+ #0", (int)ch)) {
            if (ch == '-')
                spec.flag_left = true;
            else if (ch == '+')
                spec.flag_sign = true;
            else if (ch == ' ')
                spec.flag_space = true;
            else if (ch == '#')
                spec.flag_alternate = true;
            else if (ch == '0')
                spec.flag_zero = true;
            if (string_cursor_next(cursor) != 0)
                goto done;
        }
        if (string_format_cursor_consume_char(cursor, '*')) {
            width_from_arg = 1;
            spec.width_from_argument = true;
        } else {
            while (string_format_cursor_peek_ascii(cursor, &ch) &&
                   isdigit((unsigned char)ch)) {
                spec.width = spec.width * 10 + (int)(ch - '0');
                if (string_cursor_next(cursor) != 0)
                    goto done;
            }
        }
        if (string_format_cursor_consume_char(cursor, '.')) {
            if (string_format_cursor_consume_char(cursor, '*')) {
                precision_from_arg = 1;
                spec.precision_from_argument = true;
            } else {
                spec.precision = 0;
                while (string_format_cursor_peek_ascii(cursor, &ch) &&
                       isdigit((unsigned char)ch)) {
                    spec.precision = spec.precision * 10 + (int)(ch - '0');
                    if (string_cursor_next(cursor) != 0)
                        goto done;
                }
            }
        }
        {
            string_pos_t pos = string_cursor_position(cursor);
            char c0 = string_format_cursor_ascii_at(cursor, pos);
            char c1 = string_format_cursor_ascii_at(cursor, pos + 1u);
            char c2 = string_format_cursor_ascii_at(cursor, pos + 2u);

            if (c0 == 'h' && c1 == 'h' &&
                string_format_char_is_standard_conversion(c2)) {
                length[0] = 'h';
                length[1] = 'h';
                spec.length[0] = 'h';
                spec.length[1] = 'h';
                if (!string_format_cursor_skip(cursor, 2u))
                    goto done;
            } else if (c0 == 'l' && c1 == 'l' &&
                       string_format_char_is_standard_conversion(c2)) {
                length[0] = 'l';
                length[1] = 'l';
                spec.length[0] = 'l';
                spec.length[1] = 'l';
                if (!string_format_cursor_skip(cursor, 2u))
                    goto done;
            } else if (c0 != '\0' && strchr("hljztL", c0) &&
                       string_format_char_is_standard_conversion(c1)) {
                length[0] = c0;
                spec.length[0] = length[0];
                if (string_cursor_next(cursor) != 0)
                    goto done;
            }
        }

        if (!string_format_cursor_peek_ascii(cursor, &ch))
            goto done;
        conv = (char)ch;
        spec.conversion = conv;
        if (string_cursor_next(cursor) != 0)
            goto done;
        if (callback &&
            !string_format_char_is_standard_conversion(conv) &&
            string_format_cursor_peek_ascii(cursor, &ch) &&
            isalpha((unsigned char)ch)) {
            spec.trailing_modifier = (char)ch;
        }

        if (callback && conv != 'S' && conv != 'W' && conv != 'R') {
            size_t before = string_encoded_len(s);
            string_format_result_t handled = callback(s, &spec, ap, callback_user);

            if (handled == STRING_FORMAT_ERROR)
                goto done;
            if (handled == STRING_FORMAT_HANDLED ||
                handled == STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER) {
                total += (int)(string_encoded_len(s) - before);
                if (handled == STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER &&
                    spec.trailing_modifier != '\0') {
                    if (string_cursor_next(cursor) != 0)
                        goto done;
                }
                literal_start = string_cursor_position(cursor);
                continue;
            }
        }

        if (conv == 'S') {
            const string_t *value;

            if (width_from_arg)
                (void)va_arg(ap, int);
            if (precision_from_arg)
                (void)va_arg(ap, int);
            if (length[0] != '\0')
                goto done;
            value = va_arg(ap, const string_t *);
            appended = value
                ? string_append_string(s, value)
                : string_append_cstr(s, "(null)");
            if (appended != 0)
                goto done;
            total += value ? (int)string_encoded_len(value) : 6;
        } else if (conv == 'W') {
            string_view_t value;
            size_t before;

            if (width_from_arg)
                (void)va_arg(ap, int);
            if (precision_from_arg)
                (void)va_arg(ap, int);
            if (length[0] != '\0')
                goto done;
            value = va_arg(ap, string_view_t);
            before = string_encoded_len(s);
            if (string_append_view(s, value) != 0)
                goto done;
            total += (int)(string_encoded_len(s) - before);
        } else if (conv == 'R') {
            rune_t value;
            size_t before;

            if (width_from_arg)
                (void)va_arg(ap, int);
            if (precision_from_arg)
                (void)va_arg(ap, int);
            if (length[0] != '\0')
                goto done;
            value = va_arg(ap, rune_t);
            if (rune_is_empty(value))
                appended = 0;
            else {
                before = string_encoded_len(s);
                if (string_append_rune(s, value) != 0)
                    goto done;
                appended = (int)(string_encoded_len(s) - before);
            }
            total += appended;
        } else if (conv == 'n') {
            goto done;
        } else {
            string_t *standard_spec = string_cursor_slice_between(
                spec_start,
                string_cursor_position(cursor),
                cursor);

            appended = string_format_append_standard_conversion(
                s,
                standard_spec,
                conv,
                length,
                width_from_arg,
                precision_from_arg,
                ap);
            string_free(standard_spec);
            if (appended < 0)
                goto done;
            total += appended;
        }

        literal_start = string_cursor_position(cursor);
    }

    {
        int appended = string_format_append_cursor_literal(
            s,
            literal_start,
            string_cursor_position(cursor),
            cursor);
        if (appended < 0)
            goto done;
        total += appended;
    }

    if (string_normalise_storage(s) != 0)
        goto done;
    result = total;

done:
    string_cursor_free(cursor);
    string_free(format);
    return result;
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
        result = string_append_vformat_with_string_module_objects(
            s, fmt, ap2, NULL, NULL);
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

int string_append_vformat_with_callback(string_t *s,
                                        const char *fmt,
                                        va_list ap,
                                        string_format_callback_t callback,
                                        void *user)
{
    va_list ap2;
    int result;

    if (!s || !fmt)
        return -1;
    if (!callback)
        return string_append_vformat(s, fmt, ap);

    va_copy(ap2, ap);
    result = string_append_vformat_with_string_module_objects(
        s, fmt, ap2, callback, user);
    va_end(ap2);
    return result;
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

string_t *string_vsprintf_with_callback(const char *fmt,
                                        va_list ap,
                                        string_format_callback_t callback,
                                        void *user)
{
    string_t *out;
    va_list ap2;

    if (!fmt)
        return NULL;
    if (!callback)
        return string_vsprintf(fmt, ap);

    out = string_new();
    if (!out)
        return NULL;

    va_copy(ap2, ap);
    if (string_append_vformat_with_callback(out, fmt, ap2, callback, user) < 0) {
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

static int string_vfprintf_stream(FILE *stream, const char *fmt, va_list ap)
{
    string_t *out;
    size_t len;

    out = string_vsprintf(fmt, ap);
    if (!out)
        return -1;

    len = string_encoded_len(out);
    if (len > (size_t)INT_MAX) {
        string_free(out);
        return -1;
    }

    if (fwrite(string_c_str(out), 1u, len, stream) != len) {
        string_free(out);
        return -1;
    }

    string_free(out);
    return (int)len;
}

int string_printf(const char *fmt, ...)
{
    int written;
    va_list ap;

    va_start(ap, fmt);
    written = string_vfprintf_stream(stdout, fmt, ap);
    va_end(ap);
    return written;
}

int string_fprintf(FILE *stream, const char *fmt, ...)
{
    int written;
    va_list ap;

    if (!stream)
        return -1;

    va_start(ap, fmt);
    written = string_vfprintf_stream(stream, fmt, ap);
    va_end(ap);
    return written;
}

/* Return the byte offset of the first occurrence of `needle` in s, or -1 if
   not found, needle is empty, or either argument is NULL. */

static bool string_find_bytes_index(const char *haystack,
                                    size_t haystack_len,
                                    const char *needle,
                                    size_t needle_len,
                                    size_t start,
                                    size_t *index_out)
{
    if (!haystack || !needle || needle_len == 0u || needle_len > haystack_len)
        return false;
    if (start > haystack_len - needle_len)
        return false;

    for (size_t i = start; i <= haystack_len - needle_len; ++i) {
        if (memcmp(&haystack[i], needle, needle_len) == 0) {
            if (index_out)
                *index_out = i;
            return true;
        }
    }

    return false;
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
    size_t index = 0u;

    if (!s || !needle_data || needle_len == 0u)
        return -1;

    if (!string_find_bytes_index(s->data,
                                 s->len,
                                 needle_data,
                                 needle_len,
                                 0u,
                                 &index))
        return -1;

    return (string_offset_t)index;
}

/* Lexicographically compare a and b. Returns <0, 0, or >0. NULL sorts before
   any non-NULL string. */

int string_compare(const string_t *a, const string_t *b)
{
    size_t min_len;
    int cmp;

    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;

    min_len = a->len < b->len ? a->len : b->len;
    cmp = min_len > 0u ? memcmp(a->data, b->data, min_len) : 0;
    if (cmp != 0)
        return cmp;
    if (a->len < b->len)
        return -1;
    if (a->len > b->len)
        return 1;
    return 0;
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
    size_t pos = 0u;
    size_t match = 0u;
    size_t new_len;
    char *temp;
    size_t out_pos = 0u;
    size_t in_pos = 0u;

    if (!s || !search_data || (!replace_data && replace_len > 0u))
        return -1;
    if (search_len == 0u)
        return 0;

    while (string_find_bytes_index(s->data,
                                   s->len,
                                   search_data,
                                   search_len,
                                   pos,
                                   &match)) {
        count++;
        pos = match + search_len;
    }

    if (count == 0)
        return 0;

    new_len = replace_len >= search_len
        ? s->len + count * (replace_len - search_len)
        : s->len - count * (search_len - replace_len);

    temp = malloc(new_len + 1);
    if (!temp) return -1;

    while (string_find_bytes_index(s->data,
                                   s->len,
                                   search_data,
                                   search_len,
                                   in_pos,
                                   &match)) {
        size_t prefix_len = match - in_pos;

        memcpy(&temp[out_pos], &s->data[in_pos], prefix_len);
        out_pos += prefix_len;

        if (replace_len > 0u)
            memcpy(&temp[out_pos], replace_data, replace_len);
        out_pos += replace_len;

        in_pos = match + search_len;
    }

    memcpy(&temp[out_pos], &s->data[in_pos], s->len - in_pos);
    out_pos += s->len - in_pos;
    temp[out_pos] = '\0';

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
    string_view_t source;
    size_t start = 0u;
    size_t match = 0u;
    size_t cap = 8u;
    size_t count = 0u;
    string_t **arr;

    if (!s || !out_count || !delim_data || delim_len == 0u)
        return NULL;

    arr = malloc(cap * sizeof(*arr));
    if (!arr)
        return NULL;

    source = string_view_all(s);
    while (string_find_bytes_index(s->data,
                                   s->len,
                                   delim_data,
                                   delim_len,
                                   start,
                                   &match)) {
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

        {
            string_view_t segment = string_view_slice(source,
                                                      start,
                                                      match - start);

            arr[count] = string_from_view(&segment);
        }
        if (!arr[count]) {
            string_split_free(arr, count);
            return NULL;
        }
        count++;
        start = match + delim_len;
    }

    if (count == cap) {
        string_t **tmp = realloc(arr, cap * 2u * sizeof(*arr));
        if (!tmp) {
            string_split_free(arr, count);
            return NULL;
        }
        arr = tmp;
    }

    {
        string_view_t segment = string_view_slice(source,
                                                  start,
                                                  s->len - start);

        arr[count] = string_from_view(&segment);
        if (!arr[count]) {
            string_split_free(arr, count);
            return NULL;
        }
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
