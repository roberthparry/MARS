#include "string_internal.h"

/* Grapheme classification */

static int is_combining_mark(uint32_t cp)
{
    return (cp >= 0x0300 && cp <= 0x036F) ||
           (cp >= 0x1AB0 && cp <= 0x1AFF) ||
           (cp >= 0x1DC0 && cp <= 0x1DFF) ||
           (cp >= 0x20D0 && cp <= 0x20FF) ||
           (cp >= 0xFE20 && cp <= 0xFE2F);
}

static int is_regional_indicator(uint32_t cp)
{
    return (cp >= 0x1F1E6 && cp <= 0x1F1FF);
}

grapheme_class_t utf8_grapheme_class(uint32_t cp)
{
    if (cp == 0x000D) return GB_CR;
    if (cp == 0x000A) return GB_LF;

    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
        return GB_Control;

    if (is_combining_mark(cp))
        return GB_Extend;

    if (cp == 0x200D)
        return GB_ZWJ;

    if (is_regional_indicator(cp))
        return GB_Regional_Indicator;

    if ((cp >= 0x1F300 && cp <= 0x1FAFF) ||
        (cp >= 0x1F000 && cp <= 0x1F02F))
        return GB_Extended_Pictographic;

    return GB_Other;
}

/* Forward grapheme iterator */

size_t utf8_grapheme_next(const char *s, size_t len, size_t i)
{
    if (i >= len) return len;

    size_t adv;
    uint32_t cp = utf8_decode(s + i, len - i, &adv);
    grapheme_class_t c = utf8_grapheme_class(cp);

    size_t pos = i + adv;

    if (c == GB_CR && pos < len) {
        size_t adv2;
        uint32_t cp2 = utf8_decode(s + pos, len - pos, &adv2);
        if (utf8_grapheme_class(cp2) == GB_LF)
            return pos + adv2;
    }

    if (c == GB_Control || c == GB_CR || c == GB_LF)
        return pos;

    while (pos < len) {
        size_t adv2;
        uint32_t cp2 = utf8_decode(s + pos, len - pos, &adv2);
        grapheme_class_t c2 = utf8_grapheme_class(cp2);

        if (c2 == GB_Extend || c2 == GB_ZWJ)
            pos += adv2;
        else
            break;
    }

    if (c == GB_Regional_Indicator) {
        size_t adv2;
        uint32_t cp2 = utf8_decode(s + pos, len - pos, &adv2);
        if (utf8_grapheme_class(cp2) == GB_Regional_Indicator)
            pos += adv2;
    }

    return pos;
}

/* Backward grapheme iterator */

size_t utf8_grapheme_prev(const char *s, size_t len, size_t i)
{
    if (i == 0 || i > len) return 0;

    size_t pos = i;

    do {
        pos--;
        while (pos > 0 && ((unsigned char)s[pos] >> 6) == 0x2)
            pos--;
    } while (pos > 0 && is_combining_mark(
                utf8_decode(s + pos, len - pos, &(size_t){0})
             ));

    return pos;
}

/* Count graphemes */

size_t utf8_grapheme_count(const string_t *s)
{
    if (!s) return 0;

    size_t count = 0;
    size_t i = 0;

    while (i < s->len) {
        size_t next = utf8_grapheme_next(s->data, s->len, i);
        count++;
        i = next;
    }

    return count;
}

/* Grapheme-safe reverse */

void string_utf8_grapheme_reverse(string_t *s)
{
    if (!s || s->len <= 1) return;

    char *tmp = (char *)malloc(s->len);
    if (!tmp) return;

    size_t out = 0;
    size_t i = s->len;

    while (i > 0) {
        size_t start = utf8_grapheme_prev(s->data, s->len, i);
        size_t clen  = i - start;

        memcpy(tmp + out, s->data + start, clen);
        out += clen;

        i = start;
    }

    memcpy(s->data, tmp, s->len);
    free(tmp);
}

/* Grapheme-safe substring */

string_t *string_utf8_grapheme_substr(const string_t *s, size_t gpos, size_t glen)
{
    if (!s) return NULL;

    size_t i = 0;
    size_t g = 0;

    while (i < s->len && g < gpos) {
        i = utf8_grapheme_next(s->data, s->len, i);
        g++;
    }

    size_t start = i;

    while (i < s->len && g < gpos + glen) {
        i = utf8_grapheme_next(s->data, s->len, i);
        g++;
    }

    size_t end = i;

    string_t *out = string_new();
    if (!out) return NULL;

    if (string_reserve(out, end - start + 1) != 0) {
        string_free(out);
        return NULL;
    }

    memcpy(out->data, s->data + start, end - start);
    out->data[end - start] = '\0';
    out->len = end - start;

    return out;
}

