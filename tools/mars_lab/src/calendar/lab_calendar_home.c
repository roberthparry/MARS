/**
 * @file lab_calendar_home.c
 * @brief Current home-origin sky header, independent of worksheet dates and observers.
 *
 * Reads the installed calendar setup through the jurisdiction API. Native ephemerides
 * supply the Moon; native solar approximations and zoneinfo supply today's local solar
 * clocks. No weather, eclipse search, browser geolocation or mutable global state is used.
 */
#include <math.h>
#include <stdlib.h>
#include <sodium.h>

#include "almanac-t.h"
#include "jurisdiction-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static bool home_origin(const jurisdict_place_t *place, void *context)
{
    json_t *origin = context;
    lab_cal_set(origin, "location", place->name);
    lab_cal_set(origin, "jurisdiction", place->jurisdiction_code);
    lab_cal_set(origin, "latitude", place->latitude);
    lab_cal_set(origin, "longitude", place->longitude);
    lab_cal_set(origin, "timezone", place->timezone);
    return true;
}

static bool home_item(string_t *out, const char *label, const char *value)
{
    return !string_append_cstr(out, "<span class=\"sky-status-item\"><span class=\"sky-status-label\">") &&
        lab_cal_markup_text(out, label) &&
        !string_append_cstr(out, "</span><span class=\"sky-status-value\">") && lab_cal_markup_text(out, value) &&
        !string_append_cstr(out, "</span></span>");
}

static void home_number(json_t *object, const char *key, double number)
{
    string_t *text = string_sprintf("%.12g", number);
    lab_cal_set(object, key, string_c_str(text));
    string_free(text);
}

static json_t *home_body(almanac_t *almanac, datetime_t *moment, const char *code)
{
    json_t *row = json_new_object();
    lab_cal_set(row, "code", code);
    almanac_entry_t *entry = almanac_new_entry(almanac, code, moment);
    if (entry) {
        home_number(row, "phase", almanac_entry_phase_angle_degrees(entry));
        home_number(row, "right_ascension_hours", almanac_entry_right_ascension_hours(entry));
        home_number(row, "declination_degrees", almanac_entry_declination_degrees(entry));
        home_number(row, "gha_degrees", fmod(almanac_entry_gha_aries_degrees(entry) +
                                             almanac_entry_sha_degrees(entry), 360.0));
    }
    almanac_entry_dealloc(entry);
    return row;
}

static bool home_solar(string_t *out, const string_t *zone, const struct tm *today,
                       long jdn, double latitude, double longitude, bool rising)
{
    const char *missing = "unavailable";
    char clock[32] = "";
    for (int day = -1; day <= 1; ++day) {
        double hours = datetime_sun_time(jdn + day, latitude, longitude, rising);
        if (hours < 0 || !isfinite(hours)) {
            if (day == 0) missing = hours == -1 ? "polar night" : hours == -2 ? "midnight sun" : "unavailable";
            continue;
        }
        struct tm local = {0};
        double offset;
        if (lab_cal_timezone_at(zone, jdn + day - 0.5 + hours / 24, &local, &offset) &&
            local.tm_year == today->tm_year && local.tm_yday == today->tm_yday) {
            strftime(clock, sizeof clock, "≈%H:%M", &local);
            break;
        }
    }
    return home_item(out, rising ? "Sunrise · local" : "Sunset · local", *clock ? clock : missing);
}

static bool home_flag(string_t *out, const char *code)
{
    jurisdiction_t *j = jurisdict_open(NULL);
    string_t *name = NULL, *svg = NULL;
    bool found = jurisdict_country(j, code, &name, &svg), ok = true;
    if (found && svg && string_byte_length(svg)) {
        size_t bytes = string_byte_length(svg), capacity = sodium_base64_ENCODED_LEN(bytes, sodium_base64_VARIANT_ORIGINAL);
        char *encoded = malloc(capacity);
        ok = encoded != NULL;
        if (encoded) {
            sodium_bin2base64(encoded, capacity, (const unsigned char *)string_c_str(svg), bytes,
                              sodium_base64_VARIANT_ORIGINAL);
            ok = !string_append_cstr(out, "<img class=\"country-flag\" alt=\"") &&
                 lab_cal_markup_text(out, string_c_str(name)) &&
                 !string_append_cstr(out, "\" src=\"data:image/svg+xml;base64,") &&
                 !string_append_cstr(out, encoded) && !string_append_cstr(out, "\">");
            free(encoded);
        }
    }
    string_free(name); string_free(svg); jurisdict_close(j);
    return ok;
}

/* Render only supplied setup metadata and the current instant; worksheet state is never consulted. */
string_t *lab_cal_home_markup(const json_t *origin, time_t now)
{
    string_t *zone = string_new_with(lab_cal_text(origin, "timezone"));
    struct tm local = {0};
    double jd = 2440587.5 + (double)now / 86400;
    double offset, latitude, longitude;
    bool ok = lab_cal_number(lab_cal_text(origin, "latitude"), &latitude) && fabs(latitude) <= 90 &&
              lab_cal_number(lab_cal_text(origin, "longitude"), &longitude) && fabs(longitude) <= 180 &&
              lab_cal_timezone_at(zone, jd, &local, &offset);
    if (!ok) { string_free(zone); return NULL; }
    char date[32], local_clock[32];
    strftime(date, sizeof date, "%Y-%m-%d", &local);
    strftime(local_clock, sizeof local_clock, "%Y-%m-%d %H:%M:%S", &local);
    datetime_t *day = datetime_from_string(date), *moment = datetime_alloc();
    if (moment && !datetime_init_jd(moment, jd)) {
        datetime_dealloc(moment);
        moment = NULL;
    }
    almanac_t *almanac = moment ? almanac_open() : NULL;
    json_t *moon = home_body(almanac, moment, "MOON"), *sun = home_body(almanac, moment, "SUN");
    json_t *observer = json_new_object();
    lab_cal_set(observer, "observer_latitude", lab_cal_text(origin, "latitude"));
    lab_cal_set(observer, "observer_longitude", lab_cal_text(origin, "longitude"));
    string_t *out = string_new_with("<span class=\"sky-status-strip\"><span class=\"sky-status-events\">");
    bool moon_ready = *lab_cal_text(moon, "phase") && *lab_cal_text(sun, "phase");
    ok = out && day && (moon_ready ? lab_cal_body_symbol(out, moon, sun, observer) :
                                   home_item(out, "Moon", "unavailable")) &&
         home_solar(out, zone, &local, datetime_jdn(day), latitude, longitude, false) &&
         home_solar(out, zone, &local, datetime_jdn(day), latitude, longitude, true) &&
         !string_append_cstr(out, "</span><span class=\"sky-status-selection\">");
    int minutes = (int)lround(fabs(offset) * 60);
    string_t *clock = string_sprintf("%s GMT%c%02d:%02d", local_clock, offset < 0 ? '-' : '+', minutes / 60, minutes % 60);
    string_t *label = string_sprintf("Local time · %s", lab_cal_text(origin, "location"));
    ok = ok && clock && label && home_item(out, string_c_str(label), string_c_str(clock)) &&
         home_flag(out, lab_cal_text(origin, "jurisdiction")) && !string_append_cstr(out, "</span></span>");
    string_free(clock); string_free(label); string_free(zone);
    json_free(moon); json_free(sun); json_free(observer);
    almanac_close(almanac); datetime_dealloc(day); datetime_dealloc(moment);
    if (!ok) { string_free(out); out = NULL; }
    return out;
}

/* Return a small independent header response, including an explicit unconfigured-origin state. */
json_t *lab_cal_home_sky(void)
{
    jurisdiction_t *j = jurisdict_open(NULL);
    json_t *origin = json_new_object(), *response = json_new_object();
    bool found = jurisdict_setup_origin(j, home_origin, origin);
    jurisdict_close(j);
    string_t *html = found ? lab_cal_home_markup(origin, time(NULL)) : NULL;
    lab_cal_take(response, "ok", json_new_bool(html != NULL));
    lab_cal_set(response, "html", html ? string_c_str(html) :
        "<span class=\"sky-status-strip\">Home location unavailable — configure the calendar setup origin.</span>");
    string_free(html); json_free(origin);
    return response;
}
