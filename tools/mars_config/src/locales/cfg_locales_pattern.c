/**
 * @file cfg_locales_pattern.c
 * @brief Exact Babel-compatible long-date pattern token boundaries.
 *
 * Doubled apostrophes are protected before quote handling. Opening a quote
 * flushes pending unquoted text; closing it joins quoted text to following
 * literals. Only the Gregorian fields supported by calendar_local are accepted.
 */
#include <stdio.h>

#include "cfg_locales_internal.h"

static bool cfg_locales_part(json_t *parts, const char *field, const string_t *literal)
{
    if (json_array_size(parts) >= CFG_LOCALES_PATTERN_PARTS) {
        fputs("Calendar pattern exceeds the 128-part limit.\n", stderr);
        return false;
    }
    string_t *name = string_new_with(field), *empty = literal ? NULL : string_new();
    json_t *pair = json_new_array();
    bool ok = name && pair && (literal || empty) && cfg_locales_push(pair, name) &&
              cfg_locales_push(pair, literal ? literal : empty) && json_array_append(parts, pair);
    json_free(pair);
    string_free(name);
    string_free(empty);
    return ok;
}

static bool cfg_locales_flush_text(json_t *parts, string_t **text)
{
    if (!string_byte_length(*text))
        return true;
    bool ok = cfg_locales_part(parts, "literal", *text);
    string_free(*text);
    *text = string_new();
    return ok && *text;
}

static bool cfg_locales_flush_field(json_t *parts, uint32_t *field, unsigned *count, const string_t *era,
                                    const string_t *suffix)
{
    if (!*field)
        return true;
    static const struct {
        uint32_t field;
        unsigned count;
        const char *name;
    } fields[] = {{'y', 1, "year"},       {'M', 1, "month"}, {'M', 3, "month_short"},
                  {'M', 4, "month_full"}, {'d', 1, "day"},   {'d', 2, "day_padded"}};
    bool ok = false;
    if (*field == 'G' && *count == 1)
        ok = era && cfg_locales_part(parts, "literal", era);
    else {
        /* Six supported field/count combinations, independent of catalogue size. */
        for (size_t i = 0; i < sizeof(fields) / sizeof(*fields); ++i)
            if (fields[i].field == *field && fields[i].count == *count) {
                ok = cfg_locales_part(parts, fields[i].name, NULL);
                if (ok && *field == 'd' && suffix && string_byte_length(suffix))
                    ok = cfg_locales_part(parts, "first_day_suffix", suffix);
                break;
            }
    }
    if (!ok)
        fprintf(stderr, "Unsupported calendar pattern field U+%04X repeated %u times.\n", (unsigned)*field, *count);
    *field = 0;
    *count = 0;
    return ok;
}

static bool cfg_locales_pattern_field(uint32_t scalar)
{
    const char fields[] = "GyYuQqMLwWdDFgEecabBhHKkmsSAzZOvVXx";
    /* Babel's fixed LDML field alphabet, not an input-sized lookup. */
    for (size_t i = 0; i + 1 < sizeof(fields); ++i)
        if (scalar == (unsigned char)fields[i])
            return true;
    return false;
}

/* Tokenise scalars without grapheme truncation or accumulated-string normalisation. */
json_t *cfg_locales_pattern(const string_t *pattern, const string_t *era, const string_t *suffix)
{
    if (!pattern || string_byte_length(pattern) > CFG_LOCALES_PATTERN_BYTES ||
        (era && string_byte_length(era) > CFG_LOCALES_AFFIX_BYTES) ||
        (suffix && string_byte_length(suffix) > CFG_LOCALES_AFFIX_BYTES)) {
        fputs("Calendar pattern exceeds 1024 bytes or an era/suffix exceeds 256 bytes.\n", stderr);
        return NULL;
    }
    json_t *parts = json_new_array();
    string_t *text = string_new(), *quoted = string_new();
    bool ok = pattern && parts && text && quoted, quote = false;
    uint32_t field = 0;
    unsigned count = 0;
    string_view_t view = string_view_all(pattern);
    string_pos_t position = 0, next = 0;
    while (ok && position < string_byte_length(pattern)) {
        uint32_t scalar = 0, following = 0;
        string_pos_t after = 0;
        ok = string_view_peek_rune_value(view, position, &scalar, &next) && scalar != 0;
        if (!ok)
            break;
        bool doubled =
            scalar == '\'' && string_view_peek_rune_value(view, next, &following, &after) && following == '\'';
        if (doubled) {
            scalar = 0;
            next = after;
        }
        if (quote) {
            if (scalar == '\'') {
                ok = cfg_locales_append(text, quoted);
                string_free(quoted);
                quoted = string_new();
                ok = ok && quoted;
                quote = false;
            } else
                ok = doubled ? !string_append_utf8_exact(quoted, "'", 1)
                             : !string_append_utf8_exact(quoted, string_c_str(pattern) + position, next - position);
        } else if (scalar == '\'') {
            ok = field ? cfg_locales_flush_field(parts, &field, &count, era, suffix)
                       : cfg_locales_flush_text(parts, &text);
            quote = true;
        } else if (cfg_locales_pattern_field(scalar)) {
            ok = cfg_locales_flush_text(parts, &text);
            if (field != scalar) {
                ok = ok && cfg_locales_flush_field(parts, &field, &count, era, suffix);
                field = scalar;
            }
            ++count;
        } else {
            ok = cfg_locales_flush_field(parts, &field, &count, era, suffix);
            ok = ok && (doubled ? !string_append_utf8_exact(text, "'", 1)
                                : !string_append_utf8_exact(text, string_c_str(pattern) + position, next - position));
        }
        position = next;
    }
    /* Babel discards an unfinished quoted buffer, retaining preceding complete tokens. */
    ok = ok &&
         (field ? cfg_locales_flush_field(parts, &field, &count, era, suffix) : cfg_locales_flush_text(parts, &text));
    string_free(text);
    string_free(quoted);
    if (!ok || !json_array_size(parts)) {
        json_free(parts);
        parts = NULL;
    }
    return parts;
}
