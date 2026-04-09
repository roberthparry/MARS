#include "string_internal.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Binary search over the sorted codepoint ranges array. Returns 1 if cp falls
   within any range, 0 otherwise. */
typedef struct {
    uint32_t lo;
    uint32_t hi;
} cp_range_t;

static int range_contains(const cp_range_t *ranges, size_t n, uint32_t cp)
{
    size_t lo = 0;
    size_t hi = n;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (cp < ranges[mid].lo)
            hi = mid;
        else if (cp > ranges[mid].hi)
            lo = mid + 1;
        else
            return 1;
    }

    return 0;
}

/* Codepoint range tables */

static const cp_range_t combining_mark_ranges[] = {
    { 0x0300, 0x036F },
    { 0x1AB0, 0x1AFF },
    { 0x1DC0, 0x1DFF },
    { 0x20D0, 0x20FF },
    { 0xFE20, 0xFE2F },
};

static const cp_range_t spacing_mark_ranges[] = {
    { 0x0900, 0x0903 }, { 0x093B, 0x093C }, { 0x093E, 0x0940 },
    { 0x0949, 0x094C }, { 0x094E, 0x094F }, { 0x0982, 0x0983 },
    { 0x09BE, 0x09C0 }, { 0x09C7, 0x09C8 }, { 0x09CB, 0x09CC },
    { 0x09D7, 0x09D7 }, { 0x0A03, 0x0A03 }, { 0x0A3E, 0x0A40 },
    { 0x0A83, 0x0A83 }, { 0x0ABE, 0x0AC0 }, { 0x0AC9, 0x0AC9 },
    { 0x0ACB, 0x0ACC }, { 0x0B02, 0x0B03 }, { 0x0B3E, 0x0B3E },
    { 0x0B40, 0x0B40 }, { 0x0B47, 0x0B48 }, { 0x0B4B, 0x0B4C },
    { 0x0B57, 0x0B57 }, { 0x0BBE, 0x0BBF }, { 0x0BC1, 0x0BC2 },
    { 0x0BC6, 0x0BC8 }, { 0x0BCA, 0x0BCC }, { 0x0BD7, 0x0BD7 },
    { 0x0C01, 0x0C03 }, { 0x0C41, 0x0C44 }, { 0x0C82, 0x0C83 },
    { 0x0CBE, 0x0CBE }, { 0x0CC0, 0x0CC4 }, { 0x0CC7, 0x0CC8 },
    { 0x0CCA, 0x0CCB }, { 0x0CD5, 0x0CD6 }, { 0x0D02, 0x0D03 },
    { 0x0D3E, 0x0D40 }, { 0x0D46, 0x0D48 }, { 0x0D4A, 0x0D4C },
    { 0x0D57, 0x0D57 }, { 0x0DCF, 0x0DD1 }, { 0x0DD8, 0x0DDF },
    { 0x0DF2, 0x0DF3 }, { 0x0F3E, 0x0F3F }, { 0x0F7F, 0x0F7F },
    { 0x102B, 0x102C }, { 0x1031, 0x1031 }, { 0x1038, 0x1038 },
    { 0x103B, 0x103C }, { 0x1056, 0x1057 }, { 0x1062, 0x1062 },
    { 0x1067, 0x1068 }, { 0x1083, 0x1084 }, { 0x1087, 0x108C },
    { 0x108F, 0x108F }, { 0x109A, 0x109C }, { 0x17B6, 0x17B6 },
    { 0x17BE, 0x17C5 }, { 0x17C7, 0x17C8 }, { 0x1923, 0x1926 },
    { 0x1929, 0x192B }, { 0x1930, 0x1931 }, { 0x1933, 0x1938 },
    { 0x19B0, 0x19C0 }, { 0x19C8, 0x19C9 }, { 0x1A19, 0x1A1A },
    { 0x1A55, 0x1A55 }, { 0x1A57, 0x1A57 }, { 0x1A6D, 0x1A72 },
    { 0x1B04, 0x1B04 }, { 0x1B35, 0x1B35 }, { 0x1B3B, 0x1B3B },
    { 0x1B3D, 0x1B41 }, { 0x1B43, 0x1B44 }, { 0x1B82, 0x1B82 },
    { 0x1BA1, 0x1BA1 }, { 0x1BA6, 0x1BA7 }, { 0x1BAA, 0x1BAA },
    { 0x1BE7, 0x1BE7 }, { 0x1BEA, 0x1BEC }, { 0x1BEE, 0x1BEE },
    { 0x1BF2, 0x1BF3 }, { 0x1C24, 0x1C2B }, { 0x1C34, 0x1C35 },
    { 0x1CE1, 0x1CE1 }, { 0x1CF2, 0x1CF3 }, { 0x1CF7, 0x1CF7 },
    { 0xA823, 0xA824 }, { 0xA827, 0xA827 }, { 0xA880, 0xA881 },
    { 0xA8B4, 0xA8C3 }, { 0xA952, 0xA953 }, { 0xA983, 0xA983 },
    { 0xA9B4, 0xA9B5 }, { 0xA9BA, 0xA9BB }, { 0xA9BD, 0xA9C0 },
    { 0xAA2F, 0xAA30 }, { 0xAA33, 0xAA34 }, { 0xAA4D, 0xAA4D },
    { 0xAAEB, 0xAAEB }, { 0xAAEE, 0xAAEF }, { 0xAAF5, 0xAAF5 },
    { 0xABE3, 0xABE4 }, { 0xABE6, 0xABE7 }, { 0xABE9, 0xABEA },
    { 0xABEC, 0xABEC }, { 0x11000, 0x11000 }, { 0x11002, 0x11002 },
    { 0x11082, 0x11082 }, { 0x110B0, 0x110B2 }, { 0x110B7, 0x110B8 },
    { 0x1112C, 0x1112C }, { 0x11182, 0x11182 }, { 0x111B3, 0x111B5 },
    { 0x111BF, 0x111C0 }, { 0x1122C, 0x1122E }, { 0x11232, 0x11233 },
    { 0x11235, 0x11235 }, { 0x112E0, 0x112E2 }, { 0x11302, 0x11303 },
    { 0x1133F, 0x1133F }, { 0x11341, 0x11344 }, { 0x11347, 0x11348 },
    { 0x1134B, 0x1134D }, { 0x11362, 0x11363 }, { 0x11435, 0x11437 },
    { 0x11440, 0x11441 }, { 0x11443, 0x11445 },
};

static const cp_range_t extended_pictographic_ranges[] = {
    { 0x00A9, 0x00A9 }, { 0x00AE, 0x00AE }, { 0x203C, 0x203C },
    { 0x2049, 0x2049 }, { 0x20E3, 0x20E3 }, { 0x2122, 0x2122 },
    { 0x2139, 0x2139 }, { 0x2194, 0x2199 }, { 0x21A9, 0x21AA },
    { 0x231A, 0x231B }, { 0x2328, 0x2328 }, { 0x23CF, 0x23CF },
    { 0x23E9, 0x23F3 }, { 0x23F8, 0x23FA }, { 0x24C2, 0x24C2 },
    { 0x25AA, 0x25AB }, { 0x25B6, 0x25B6 }, { 0x25C0, 0x25C0 },
    { 0x25FB, 0x25FE }, { 0x2600, 0x2604 }, { 0x260E, 0x260E },
    { 0x2611, 0x2611 }, { 0x2614, 0x2615 }, { 0x2618, 0x2618 },
    { 0x261D, 0x261D }, { 0x2620, 0x2620 }, { 0x2622, 0x2623 },
    { 0x2626, 0x2626 }, { 0x262A, 0x262A }, { 0x262E, 0x262F },
    { 0x2638, 0x263A }, { 0x2640, 0x2640 }, { 0x2642, 0x2642 },
    { 0x2648, 0x2653 }, { 0x265F, 0x2660 }, { 0x2663, 0x2663 },
    { 0x2665, 0x2666 }, { 0x2668, 0x2668 }, { 0x267B, 0x267B },
    { 0x267E, 0x267F }, { 0x2692, 0x2697 }, { 0x2699, 0x2699 },
    { 0x269B, 0x269C }, { 0x26A0, 0x26A1 }, { 0x26AA, 0x26AB },
    { 0x26B0, 0x26B1 }, { 0x26BD, 0x26BE }, { 0x26C4, 0x26C5 },
    { 0x26CE, 0x26CF }, { 0x26D1, 0x26D1 }, { 0x26D3, 0x26D4 },
    { 0x26E9, 0x26EA }, { 0x26F0, 0x26F5 }, { 0x26F7, 0x26FA },
    { 0x26FD, 0x26FD }, { 0x2702, 0x2702 }, { 0x2705, 0x2705 },
    { 0x2708, 0x270D }, { 0x270F, 0x270F }, { 0x2712, 0x2712 },
    { 0x2714, 0x2714 }, { 0x2716, 0x2716 }, { 0x271D, 0x271D },
    { 0x2721, 0x2721 }, { 0x2728, 0x2728 }, { 0x2733, 0x2734 },
    { 0x2744, 0x2744 }, { 0x2747, 0x2747 }, { 0x274C, 0x274C },
    { 0x274E, 0x274E }, { 0x2753, 0x2755 }, { 0x2757, 0x2757 },
    { 0x2763, 0x2764 }, { 0x2795, 0x2797 }, { 0x27A1, 0x27A1 },
    { 0x27B0, 0x27B0 }, { 0x27BF, 0x27BF }, { 0x2934, 0x2935 },
    { 0x2B05, 0x2B07 }, { 0x2B1B, 0x2B1C }, { 0x2B50, 0x2B50 },
    { 0x2B55, 0x2B55 }, { 0x3030, 0x3030 }, { 0x303D, 0x303D },
    { 0x3297, 0x3297 }, { 0x3299, 0x3299 }, { 0x1F000, 0x1F02F },
    { 0x1F0CF, 0x1F0CF }, { 0x1F170, 0x1F171 }, { 0x1F17E, 0x1F17F },
    { 0x1F18E, 0x1F18E }, { 0x1F191, 0x1F19A }, { 0x1F1E0, 0x1F1FF },
    { 0x1F201, 0x1F202 }, { 0x1F21A, 0x1F21A }, { 0x1F22F, 0x1F22F },
    { 0x1F232, 0x1F23A }, { 0x1F250, 0x1F251 }, { 0x1F300, 0x1F6FF },
    { 0x1F700, 0x1F77F }, { 0x1F780, 0x1F7FF }, { 0x1F800, 0x1F8FF },
    { 0x1F900, 0x1F9FF }, { 0x1FA00, 0x1FA6F }, { 0x1FA70, 0x1FAFF },
};

static const cp_range_t regional_indicator_ranges[] = {
    { 0x1F1E6, 0x1F1FF },
};

/* Grapheme class classification */

grapheme_class_t string_grapheme_class(uint32_t cp)
{
    if (cp == 0x000D) return GB_CR;
    if (cp == 0x000A) return GB_LF;

    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
        return GB_Control;

    if (cp == 0x200D)
        return GB_ZWJ;

    if (range_contains(regional_indicator_ranges,
                       ARRAY_COUNT(regional_indicator_ranges), cp))
        return GB_Regional_Indicator;

    if (range_contains(extended_pictographic_ranges,
                       ARRAY_COUNT(extended_pictographic_ranges), cp))
        return GB_Extended_Pictographic;

    if (range_contains(combining_mark_ranges,
                       ARRAY_COUNT(combining_mark_ranges), cp))
        return GB_Extend;

    if (range_contains(spacing_mark_ranges,
                       ARRAY_COUNT(spacing_mark_ranges), cp))
        return GB_SpacingMark;

    return GB_Other;
}

/* Forward grapheme iterator */

size_t string_grapheme_next(const char *s, size_t len, size_t i)
{
    if (i >= len) return len;

    size_t adv;
    uint32_t cp = utf8_decode(s + i, len - i, &adv);
    grapheme_class_t cls = string_grapheme_class(cp);

    size_t pos = i + adv;

    /* GB3/GB4/GB5: CR+LF is one cluster; CR, LF, Control are singletons */
    if (cls == GB_CR) {
        if (pos < len) {
            size_t adv2;
            uint32_t cp2 = utf8_decode(s + pos, len - pos, &adv2);
            if (string_grapheme_class(cp2) == GB_LF)
                return pos + adv2;
        }
        return pos;
    }

    if (cls == GB_LF || cls == GB_Control)
        return pos;

    /* GB9/GB9a/GB11: consume Extend, SpacingMark, and ZWJ sequences */
    while (pos < len) {
        size_t adv2;
        uint32_t cp2 = utf8_decode(s + pos, len - pos, &adv2);
        grapheme_class_t cls2 = string_grapheme_class(cp2);

        if (cls2 == GB_Extend || cls2 == GB_SpacingMark) {
            pos += adv2;
        } else if (cls2 == GB_ZWJ) {
            pos += adv2;
            /* GB11: ZWJ followed by Extended_Pictographic is one cluster */
            if (pos < len) {
                size_t adv3;
                uint32_t cp3 = utf8_decode(s + pos, len - pos, &adv3);
                if (range_contains(extended_pictographic_ranges,
                                   ARRAY_COUNT(extended_pictographic_ranges), cp3))
                    pos += adv3;
            }
        } else {
            break;
        }
    }

    /* GB12/GB13: pair of Regional Indicators is one cluster */
    if (cls == GB_Regional_Indicator && pos < len) {
        size_t adv2;
        uint32_t cp2 = utf8_decode(s + pos, len - pos, &adv2);
        if (string_grapheme_class(cp2) == GB_Regional_Indicator)
            pos += adv2;
    }

    return pos;
}

/* Backward grapheme iterator. Scans forward from a safe anchor for
   correctness with ZWJ sequences. */

size_t string_grapheme_prev(const char *s, size_t len, size_t i)
{
    if (i == 0 || i > len) return 0;

    /* Step back to the start of the previous codepoint */
    size_t pos = i - 1;
    while (pos > 0 && ((unsigned char)s[pos] >> 6) == 0x2)
        pos--;

    /* Walk further back past any combining/ZWJ run to find a safe anchor */
    size_t anchor = pos;
    while (anchor > 0) {
        size_t prev = anchor - 1;
        while (prev > 0 && ((unsigned char)s[prev] >> 6) == 0x2)
            prev--;
        uint32_t cp = utf8_decode(s + prev, len - prev, &(size_t){0});
        grapheme_class_t cls = string_grapheme_class(cp);
        if (cls == GB_Extend   || cls == GB_ZWJ        ||
            cls == GB_SpacingMark || cls == GB_Regional_Indicator) {
            anchor = prev;
        } else {
            break;
        }
    }

    /* Scan forward from anchor to find the grapheme boundary just before i */
    size_t cur          = anchor;
    size_t prev_boundary = anchor;
    while (cur < i) {
        prev_boundary = cur;
        cur = string_grapheme_next(s, len, cur);
    }

    return prev_boundary;
}

/* Count grapheme clusters */

size_t string_grapheme_count(const string_t *s)
{
    if (!s) return 0;

    size_t count = 0;
    size_t i     = 0;

    while (i < s->len) {
        i = string_grapheme_next(s->data, s->len, i);
        count++;
    }

    return count;
}

/* Grapheme-safe reverse */

void string_grapheme_reverse(string_t *s)
{
    if (!s || s->len <= 1) return;

    size_t *starts = malloc((s->len + 1) * sizeof(size_t));
    if (!starts) return;

    size_t count = 0;
    size_t i     = 0;
    while (i < s->len) {
        starts[count++] = i;
        i = string_grapheme_next(s->data, s->len, i);
    }
    starts[count] = s->len;

    char *tmp = malloc(s->len);
    if (!tmp) { free(starts); return; }

    size_t out = 0;
    for (size_t k = count; k > 0; k--) {
        size_t clen = starts[k] - starts[k - 1];
        memcpy(tmp + out, s->data + starts[k - 1], clen);
        out += clen;
    }

    memcpy(s->data, tmp, s->len);
    free(tmp);
    free(starts);
}

/* Grapheme-safe substring */

string_t *string_grapheme_substr(const string_t *s, size_t gpos, size_t glen)
{
    if (!s) return NULL;

    size_t i = 0;
    size_t g = 0;

    while (i < s->len && g < gpos) {
        i = string_grapheme_next(s->data, s->len, i);
        g++;
    }

    size_t start = i;

    while (i < s->len && g < gpos + glen) {
        i = string_grapheme_next(s->data, s->len, i);
        g++;
    }

    size_t nbytes = i - start;

    string_t *out = string_new();
    if (!out) return NULL;

    if (nbytes == 0)
        return out;

    if (string_reserve(out, nbytes + 1) != 0) {
        string_free(out);
        return NULL;
    }

    memcpy(out->data, s->data + start, nbytes);
    out->data[nbytes] = '\0';
    out->len          = nbytes;

    return out;
}

/* Convenience: single grapheme by index */

string_t *string_grapheme_at(const string_t *s, size_t index)
{
    return string_grapheme_substr(s, index, 1);
}