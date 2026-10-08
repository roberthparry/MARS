/**
 * @file cfg_calendar_text.c
 * @brief Unicode keys and scoped environment/timezone primitives for calendar setup.
 *
 * Uses Unicode NFKD case-folding for catalogue equality and string cursors for
 * syntax. Environment changes retain unset/empty distinctions. Only validated
 * installed TZif files are accepted, and callers must restore each entered scope.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTRING
#include <unicase.h>
#include <unictype.h>
#include <uninorm.h>
#endif

#include "cfg_calendar_internal.h"
#include "file.h"

/* Copy and trim a borrowed optional string. */
string_t *cfg_calendar_value(const string_t *text)
{
    string_t *value = text ? string_clone(text) : string_new();
    if (value)
        string_trim(value);
    return value;
}

/* Compare strings through the public string interface. */
bool cfg_calendar_equal(const string_t *text, const char *literal)
{
    string_t *other = string_new_with(literal);
    bool equal = text && other && string_compare(text, other) == 0;
    string_free(other);
    return equal;
}

/* Validate text before crossing a NUL-terminated API boundary. */
bool cfg_calendar_text_valid(const string_t *text)
{
    if (!text)
        return true;
    string_view_t view = string_view_all(text);
    string_pos_t position = 0, next = 0;
    bool valid = true;
    while (position < string_byte_length(text)) {
        uint32_t value = 0;
        if (!string_view_peek_rune_value(view, position, &value, &next) || next <= position || !value) {
            valid = false;
            break;
        }
        position = next;
    }
    return valid;
}

/* Match the existing catalogue's compatibility-decomposition and case-folding keys. */
string_t *cfg_calendar_key(const string_t *text, bool language)
{
    if (!text || !cfg_calendar_text_valid(text))
        return NULL;
    if (!string_byte_length(text))
        return string_new();
#ifdef HAVE_UNISTRING
    size_t length = 0;
    uint8_t *folded =
        u8_casefold((const uint8_t *)string_c_str(text), string_byte_length(text), NULL, UNINORM_NFKD, NULL, &length);
    string_t *source = string_new(), *result = string_new();
    bool ok = source && result && folded && !string_append_utf8_exact(source, (const char *)folded, length);
    free(folded);
    string_view_t view = string_view_all(source);
    string_pos_t position = 0, next = 0;
    bool space = false;
    while (ok && position < string_byte_length(source)) {
        uint32_t value = 0;
        ok = string_view_peek_rune_value(view, position, &value, &next) && next > position;
        if (!ok)
            break;
        if (!uc_combining_class(value)) {
            if (uc_is_property_white_space(value) || (value >= 0x1c && value <= 0x1f))
                space = string_byte_length(result) != 0;
            else {
                if (space)
                    ok = string_append_char(result, ' ') == 0;
                space = false;
                if (ok)
                    ok = language && value == '-' ? string_append_char(result, '_') == 0
                                                  : string_append_rune(result, rune_from_value(value)) == 0;
            }
        }
        position = next;
    }
    string_free(source);
    if (!ok) {
        string_free(result);
        return NULL;
    }
    return result;
#else
    string_t *result = string_new();
    string_cursor_t *cursor = result ? string_cursor_new(text) : NULL;
    bool ok = cursor != NULL, space = false;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char ch;
        if (!string_cursor_peek_ascii(cursor, &ch)) {
            fputs("Unicode calendar matching requires a build with ENABLE_UNISTRING=1.\n", stderr);
            ok = false;
            break;
        }
        if (ch == ' ' || (ch >= 9 && ch <= 13) || (ch >= 0x1c && ch <= 0x1f))
            space = string_byte_length(result) != 0;
        else {
            if (space)
                ok = !string_append_char(result, ' ');
            space = false;
            if (ch >= 'A' && ch <= 'Z')
                ch += 'a' - 'A';
            if (language && ch == '-')
                ch = '_';
            ok = ok && !string_append_char(result, ch);
        }
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    if (!ok) {
        string_free(result);
        result = NULL;
    }
    return result;
#endif
}

/* Decode only the small, explicit menu-number syntax. */
bool cfg_calendar_number(const string_t *text, unsigned *number)
{
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    if (!cursor)
        return false;
    unsigned count = 0, value = 0;
    bool valid = true;
    while (!string_cursor_done(cursor)) {
        unsigned char ch;
        if (++count > 3 || !string_cursor_peek_ascii(cursor, &ch) || ch < '0' || ch > '9') {
            valid = false;
            break;
        }
        value = value * 10 + ch - '0';
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    if (valid && count)
        *number = value;
    return valid && count;
}

/* Save before mutating, so failed setup can always unwind prior successful changes. */
bool cfg_calendar_environment(cfg_calendar_environment_t *state, const char *name, const string_t *value)
{
    *state = (cfg_calendar_environment_t){.name = name};
    const char *old = getenv(name);
    state->present = old != NULL;
    state->previous = old ? string_new_with(old) : NULL;
    if ((old && !state->previous) || !value || !cfg_calendar_text_valid(value) ||
        setenv(name, string_c_str(value), 1)) {
        string_free(state->previous);
        state->previous = NULL;
        return false;
    }
    state->changed = true;
    return true;
}

/* Restore even an originally empty value, and report restoration failure. */
bool cfg_calendar_restore(cfg_calendar_environment_t *state)
{
    if (!state->changed)
        return true;
    int error = state->present ? setenv(state->name, string_c_str(state->previous), 1) : unsetenv(state->name);
    if (error)
        return false;
    string_free(state->previous);
    state->previous = NULL;
    state->changed = false;
    return true;
}

/* Verify the actual TZif file rather than accepting libc's arbitrary POSIX TZ strings. */
bool cfg_calendar_zone_enter(const string_t *name, cfg_calendar_environment_t *state)
{
    *state = (cfg_calendar_environment_t){0};
    string_cursor_t *cursor = name ? string_cursor_new(name) : NULL;
    bool valid = cursor && string_byte_length(name) && string_byte_length(name) <= 255;
    bool component = false;
    while (valid && !string_cursor_done(cursor)) {
        unsigned char ch;
        valid = string_cursor_peek_ascii(cursor, &ch);
        if (!valid)
            break;
        if (ch == '/') {
            valid = component;
            component = false;
        } else {
            valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
                    ch == '+' || ch == '-';
            component = true;
        }
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    if (!valid || !component || string_starts_with(name, "right/") || string_starts_with(name, "posix/"))
        return false;
    string_t *path = string_sprintf("/usr/share/zoneinfo/%s", string_c_str(name));
    file_t *file = path ? file_new(path) : NULL;
    file_t *resolved = file ? file_resolve(file) : NULL;
    string_t *canonical = resolved ? string_new_with(file_path(resolved)) : NULL;
    unsigned char magic[4];
    size_t count = 0;
    valid = canonical && string_starts_with(canonical, "/usr/share/zoneinfo/") && file_open_read(resolved) &&
            file_read(resolved, magic, sizeof(magic), &count) && count == sizeof(magic) && magic[0] == 'T' &&
            magic[1] == 'Z' && magic[2] == 'i' && magic[3] == 'f';
    string_t *setting = valid ? string_sprintf(":%s", string_c_str(canonical)) : NULL;
    valid = setting && cfg_calendar_environment(state, "TZ", setting);
    if (valid)
        tzset();
    string_free(setting);
    string_free(canonical);
    file_free(resolved);
    file_free(file);
    string_free(path);
    return valid;
}

/* Python's date-at-noon policy avoids midnight transitions and most ambiguous instants. */
bool cfg_calendar_noon(int year, int month, int day, double *offset)
{
    struct tm civil = {.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day, .tm_hour = 12, .tm_isdst = -1};
    errno = 0;
    time_t stamp = mktime(&civil);
    if (stamp == (time_t)-1 && errno)
        return false;
    *offset = civil.tm_gmtoff / 3600.0;
    return true;
}
