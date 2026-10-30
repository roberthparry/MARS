/**
 * @file lab_calendar_timezone.c
 * @brief Observer catalogue recovery and zoneinfo conversion for native Lab workers.
 *
 * Validates IANA names and their installed TZif files through the file module.
 * Each conversion saves and restores the complete TZ environment state and
 * calls tzset after both changes. This deliberately requires the Lab's prefork,
 * single-threaded worker model; it must not run alongside threads using libc
 * local-time functions. Indexed database lookups recover one exact reference
 * position without building a world catalogue in each worker. Saved town identity
 * and catalogue heights are recovered without replacing explicitly manual heights.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datetime-t.h"
#include "file-t.h"
#include "jurisdiction-t.h"
#include "mars_paths.h"
#include "lab_forms.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

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
    string_t *canonical = mars_path_zoneinfo(zone);
    if (!canonical) return NULL;
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

static bool lab_cal_reference_place(const jurisdict_place_t *place, void *context)
{
    json_t **result = context, *row = json_new_object();
    if (!row) return false;
    lab_cal_set(row, "name", place->name);
    lab_cal_set(row, "latitude", place->latitude);
    lab_cal_set(row, "longitude", place->longitude);
    lab_cal_set(row, "elevation", place->elevation);
    lab_cal_set(row, "timezone", place->timezone);
    lab_cal_set(row, "jurisdiction", place->jurisdiction_code);
    if (lab_forms_town_presentation(row)) *result = row;
    else json_free(row);
    return false;
}

static json_t *lab_cal_reference_at(const char *latitude, const char *longitude)
{
    double lat, lon;
    if (!lab_cal_number(latitude, &lat) || !lab_cal_number(longitude, &lon)) return NULL;
    jurisdiction_t *engine = jurisdict_open(NULL);
    json_t *result = NULL;
    if (!jurisdict_place_at(engine, lat, lon, lab_cal_reference_place, &result)) {
        json_free(result);
        result = NULL;
    }
    jurisdict_close(engine);
    return result;
}

/* Repair legacy named locations by exact catalogue coordinates; never infer terrain from a nearby town. */
void lab_cal_resolve_observer(json_t *options)
{
    const char *mode = lab_cal_text(options, "elevation_mode");
    if (!*mode) return; /* Older API callers retain their explicitly supplied height. */
    json_t *town = lab_cal_reference_at(lab_cal_text(options, "lat"), lab_cal_text(options, "lon"));
    if (town) {
        lab_cal_set(options, "town", lab_cal_text(town, "value"));
        const char *selected = lab_cal_text(options, "jurisdiction"), *code = lab_cal_text(town, "jurisdiction");
        if (!(strlen(code) == 2 && !strncmp(selected, code, 2) && selected[2] == '-'))
            lab_cal_set(options, "jurisdiction", code);
        lab_cal_set(options, "timezone", lab_cal_text(town, "timezone"));
        if (strcmp(mode, "manual") && *lab_cal_text(town, "elevation")) {
            lab_cal_set(options, "elevation", lab_cal_text(town, "elevation"));
            lab_cal_set(options, "elevation_mode", "catalogue");
        }
    } else if (strcmp(mode, "manual")) {
        lab_cal_set(options, "elevation_mode", "retained");
    }
    json_free(town);
}

/* Honour explicit town-zone metadata; otherwise look up the browser's exact town option value. */
string_t *lab_cal_town_timezone(const json_t *options)
{
    const char *explicit_zone = lab_cal_text(options, "timezone");
    if (*explicit_zone)
        return string_new_with(explicit_zone);
    if (!*lab_cal_text(options, "town"))
        return string_new();
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
    json_t *place = lab_cal_reference_at(string_c_str(town_parts[1]), string_c_str(town_parts[2]));
    const char *selected = lab_cal_text(options, "jurisdiction"), *code = lab_cal_text(place, "jurisdiction");
    bool matches = !strcmp(selected, code) || (strlen(code) == 2 && !strncmp(selected, code, 2) && selected[2] == '-');
    string_t *zone = string_new_with(matches ? lab_cal_text(place, "timezone") : "");
    json_free(place);
    string_split_free(town_parts, town_count);
    string_free(town);
    return zone;
}

/* Recompute only offsets known to be derived, using the exact saved UTC instant. */
void lab_cal_resolve_observer_zone(json_t *options)
{
    lab_cal_set(options, "zone_note", "");
    if (strcmp(lab_cal_text(options, "zone_mode"), "auto")) return;
    datetime_t *moment = datetime_from_string(lab_cal_text(options, "date"));
    unsigned hour = 0, minute = 0, second = 0;
    int parts = sscanf(lab_cal_text(options, "time"), "%u:%u:%u", &hour, &minute, &second);
    if (!moment || parts < 2 || hour > 23 || minute > 59 || second > 59) {
        datetime_dealloc(moment);
        return;
    }
    double jd = datetime_jdn(moment) - 0.5 + (hour * 3600 + minute * 60 + second) / 86400.0;
    datetime_init_jd(moment, jd);
    string_t *zone = lab_cal_town_timezone(options);
    struct tm local;
    double offset = 0;
    bool resolved = lab_cal_timezone_at(zone, jd, &local, &offset);
    if (!resolved) {
        const char *code = lab_cal_observer_jurisdiction(options);
        jurisdiction_t *engine = *code ? jurisdict_open(code) : NULL;
        /* Antarctica has station-specific civil times, not a continent-wide McMurdo zone. */
        resolved = !strcmp(code, "AQ") || (engine && jurisdict_default_gmt_offset(engine, moment, &offset));
        if (!strcmp(code, "AQ")) lab_cal_set(options, "zone_note", "UTC; station timezone unknown");
        jurisdict_close(engine);
    }
    if (resolved && isfinite(offset) && offset >= -14 && offset <= 14) {
        string_t *text = string_sprintf("%.10g", offset);
        lab_cal_set(options, "zone", string_c_str(text));
        string_free(text);
    }
    string_free(zone);
    datetime_dealloc(moment);
}
