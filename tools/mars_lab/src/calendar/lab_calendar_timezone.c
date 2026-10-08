/**
 * @file lab_calendar_timezone.c
 * @brief Named-town zoneinfo conversion for single-threaded native Lab workers.
 *
 * Validates IANA names and their installed TZif files through the file module.
 * Each conversion saves and restores the complete TZ environment state and
 * calls tzset after both changes. This deliberately requires the Lab's prefork,
 * single-threaded worker model; it must not run alongside threads using libc
 * local-time functions. Town catalogue lookups are indexed once per worker.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "datetime.h"
#include "file.h"
#include "lab_calendar_internal.h"
#include "lab_page.h"

static string_t *lab_cal_zone_file(const string_t *zone)
{
    if (!zone || !string_byte_length(zone) || string_byte_length(zone) > 255)
        return NULL;
    string_cursor_t *cursor = string_cursor_new(zone);
    if (!cursor)
        return NULL;
    bool valid = true, component = false;
    while (!string_cursor_done(cursor)) {
        unsigned char ch;
        if (!string_cursor_peek_ascii(cursor, &ch)) {
            valid = false;
            break;
        }
        if (ch == '/') {
            if (!component) {
                valid = false;
                break;
            }
            component = false;
        } else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' ||
                   ch == '-' || ch == '+')
            component = true;
        else {
            valid = false;
            break;
        }
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    if (!valid || !component || string_starts_with(zone, "posix/") || string_starts_with(zone, "right/"))
        return NULL;
    string_t *path = string_sprintf("/usr/share/zoneinfo/%s", string_c_str(zone));
    file_t *file = file_new(path);
    file_t *resolved = file ? file_resolve(file) : NULL;
    string_t *canonical = resolved ? string_new_with(file_path(resolved)) : NULL;
    unsigned char magic[4];
    size_t read_count = 0;
    bool installed = canonical && string_starts_with(canonical, "/usr/share/zoneinfo/") && file_open_read(resolved) &&
                     file_read(resolved, magic, sizeof(magic), &read_count) && read_count == sizeof(magic) &&
                     magic[0] == 'T' && magic[1] == 'Z' && magic[2] == 'i' && magic[3] == 'f';
    file_free(resolved);
    file_free(file);
    string_free(path);
    if (!installed) {
        string_free(canonical);
        return NULL;
    }
    return canonical;
}

typedef struct {
    string_t *previous;
    bool was_set;
} timezone_scope_t;

static bool lab_cal_enter_zone(const string_t *zone, timezone_scope_t *scope)
{
    string_t *path = lab_cal_zone_file(zone);
    if (!path)
        return false;
    const char *previous = getenv("TZ");
    scope->was_set = previous != NULL;
    scope->previous = previous ? string_new_with(previous) : NULL;
    if (previous && !scope->previous) {
        string_free(path);
        return false;
    }
    string_t *setting = string_sprintf(":%s", string_c_str(path));
    bool changed = setting && setenv("TZ", string_c_str(setting), 1) == 0;
    string_free(setting);
    string_free(path);
    if (!changed) {
        string_free(scope->previous);
        scope->previous = NULL;
        return false;
    }
    tzset();
    return true;
}

static void lab_cal_leave_zone(timezone_scope_t *scope)
{
    int result = scope->was_set ? setenv("TZ", string_c_str(scope->previous), 1) : unsetenv("TZ");
    string_free(scope->previous);
    /* A failed restoration must never leave a worker serving requests in the wrong zone. */
    if (result)
        abort();
    tzset();
}

/* Convert the event's UTC instant rather than guessing its civil date's offset. */
bool lab_cal_timezone_at(const string_t *zone, double jd, struct tm *local, double *offset)
{
    if (!local || !offset || !isfinite(jd) || jd < 1721425.5 || jd > 5373484.5)
        return false;
    timezone_scope_t scope = {0};
    if (!lab_cal_enter_zone(zone, &scope))
        return false;
    time_t stamp = (time_t)llround((jd - 2440587.5) * 86400);
    struct tm converted;
    bool valid = localtime_r(&stamp, &converted) != NULL;
    double hours = valid ? converted.tm_gmtoff / 3600.0 : 0;
    lab_cal_leave_zone(&scope);
    if (!valid)
        return false;
    /* tm_zone is libc-owned storage that tzset may replace; callers need only civil fields. */
    converted.tm_zone = NULL;
    *local = converted;
    *offset = hours;
    return true;
}

/* Use local noon to avoid midnight transition ambiguities for calendar date controls. */
bool lab_cal_timezone_noon(const string_t *zone, const string_t *date, double *offset)
{
    if (!date || !offset)
        return false;
    datetime_t *day = datetime_from_string(string_c_str(date));
    if (!day)
        return false;
    struct tm civil = {.tm_year = datetime_year(day) - 1900,
                       .tm_mon = datetime_month(day) - 1,
                       .tm_mday = datetime_day(day),
                       .tm_hour = 12,
                       .tm_isdst = -1};
    const int year = civil.tm_year, month = civil.tm_mon, day_number = civil.tm_mday;
    datetime_dealloc(day);
    timezone_scope_t scope = {0};
    if (!lab_cal_enter_zone(zone, &scope))
        return false;
    time_t stamp = mktime(&civil);
    struct tm converted;
    bool valid = localtime_r(&stamp, &converted) != NULL && converted.tm_year == year && converted.tm_mon == month &&
                 converted.tm_mday == day_number;
    double hours = valid ? converted.tm_gmtoff / 3600.0 : 0;
    lab_cal_leave_zone(&scope);
    if (valid)
        *offset = hours;
    return valid;
}

static string_t *lab_cal_town_key(const char *jurisdiction, const char *name, const char *latitude,
                                  const char *longitude)
{
    double lat, lon;
    if (!lab_cal_number(latitude, &lat) || !lab_cal_number(longitude, &lon))
        return NULL;
    /* Totality actions use six decimal places; normalise catalogue spelling to the same coordinates. */
    return string_sprintf("%s|%s|%.6f|%.6f", jurisdiction, name, lat, lon);
}

static json_t *lab_cal_town_index(void)
{
    static json_t *index;
    if (index)
        return index;
    json_t *data = lab_page_jurisdictions();
    bool available = false;
    if (!json_bool_value(lab_cal_get(data, "available"), &available) || !available) {
        json_free(data);
        return NULL;
    }
    const json_t *towns = lab_cal_get(data, "towns");
    if (json_type(towns) == JSON_OBJECT) {
        index = json_new_object();
        /* One catalogue traversal constructs direct keys; request lookups never scan town arrays. */
        for (size_t i = 0; i < json_object_size(towns); ++i) {
            const string_t *jurisdiction = json_object_key_at(towns, i);
            const json_t *rows = json_object_value_at(towns, i);
            for (size_t j = 0; j < json_array_size(rows); ++j) {
                const json_t *row = json_array_get(rows, j);
                string_t *key = lab_cal_town_key(string_c_str(jurisdiction), lab_cal_text(row, "name"),
                                                 lab_cal_text(row, "latitude"), lab_cal_text(row, "longitude"));
                if (key)
                    lab_cal_set(index, string_c_str(key), lab_cal_text(row, "timezone"));
                string_free(key);
            }
        }
    }
    json_free(data);
    return index;
}

/* Honour explicit town-zone metadata; otherwise look up the browser's exact town option value. */
string_t *lab_cal_town_timezone(const json_t *options)
{
    const char *explicit_zone = lab_cal_text(options, "timezone");
    if (*explicit_zone)
        return string_new_with(explicit_zone);
    if (!*lab_cal_text(options, "town"))
        return string_new();
    json_t *index = lab_cal_town_index();
    string_t *town = string_new_with(lab_cal_text(options, "town"));
    size_t town_count = 0;
    string_t **town_parts = lab_cal_split(town, "|", &town_count);
    if (town_count < 3) {
        string_split_free(town_parts, town_count);
        string_free(town);
        return string_new();
    }
    for (size_t i = 0; i < town_count; ++i)
        string_trim(town_parts[i]);
    string_t *key = lab_cal_town_key(lab_cal_text(options, "jurisdiction"), string_c_str(town_parts[0]),
                                     string_c_str(town_parts[1]), string_c_str(town_parts[2]));
    string_t *zone = string_new_with(key ? lab_cal_text(index, string_c_str(key)) : "");
    string_free(key);
    if (!string_byte_length(zone)) {
        string_t *jurisdiction = string_new_with(lab_cal_text(options, "jurisdiction"));
        size_t count = 0;
        string_t **parts = lab_cal_split(jurisdiction, "-", &count);
        if (count > 1) {
            key = lab_cal_town_key(string_c_str(parts[0]), string_c_str(town_parts[0]), string_c_str(town_parts[1]),
                                   string_c_str(town_parts[2]));
            string_free(zone);
            zone = string_new_with(key ? lab_cal_text(index, string_c_str(key)) : "");
            string_free(key);
        }
        string_split_free(parts, count);
        string_free(jurisdiction);
    }
    string_split_free(town_parts, town_count);
    string_free(town);
    return zone;
}
