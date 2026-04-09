#include "string_internal.h"

/* UTF‑8 codepoint utilities */

static size_t utf8_char_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}

size_t utf8_next(const char *s, size_t len, size_t i)
{
    if (i >= len) return len;
    return i + utf8_char_len((unsigned char)s[i]);
}

size_t string_utf8_prev(const char *s, size_t len, size_t i)
{
    if (i == 0 || i > len) return 0;

    size_t j = i - 1;
    while (j > 0 && ((unsigned char)s[j] >> 6) == 0x2)
        j--;

    return j;
}

size_t string_utf8_length(const string_t *s)
{
    if (!s) return 0;

    size_t count = 0;
    size_t i = 0;

    while (i < s->len) {
        i += utf8_char_len((unsigned char)s->data[i]);
        count++;
    }

    return count;
}

void string_utf8_reverse(string_t *s)
{
    if (!s || s->len <= 1) return;

    char *tmp = malloc(s->len);
    if (!tmp) return;

    size_t out = 0;
    size_t i = s->len;

    while (i > 0) {
        size_t start = string_utf8_prev(s->data, s->len, i);
        size_t clen  = i - start;

        memcpy(tmp + out, s->data + start, clen);
        out += clen;

        i = start;
    }

    memcpy(s->data, tmp, s->len);
    free(tmp);
}

void string_utf8_to_upper(string_t *s)
{
    if (!s) return;

    size_t i = 0;
    while (i < s->len) {
        size_t clen = utf8_char_len((unsigned char)s->data[i]);

        if (clen == 1) {
            unsigned char c = s->data[i];
            if (c >= 'a' && c <= 'z')
                s->data[i] = (char)(c - 32);
        }

        i += clen;
    }
}

void string_utf8_to_lower(string_t *s)
{
    if (!s) return;

    size_t i = 0;
    while (i < s->len) {
        size_t clen = utf8_char_len((unsigned char)s->data[i]);

        if (clen == 1) {
            unsigned char c = s->data[i];
            if (c >= 'A' && c <= 'Z')
                s->data[i] = (char)(c + 32);
        }

        i += clen;
    }
}

uint32_t utf8_decode(const char *s, size_t len, size_t *adv)
{
    if (len == 0) { *adv = 0; return 0; }

    unsigned char c = s[0];
    size_t clen = utf8_char_len(c);

    if (clen > len) { *adv = 1; return c; }

    *adv = clen;
    switch (clen) {
    case 2: return ((c & 0x1F) << 6)  |  (s[1] & 0x3F);
    case 3: return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6)  |  (s[2] & 0x3F);
    case 4: return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    default: return c;
    }
}