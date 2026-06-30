#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "almanac.h"

typedef struct almanac_lab_options_t {
    short year;
    month_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    double second;
    double zone;
    double latitude;
    double longitude;
    almanac_body_id_t body_id;
} almanac_lab_options_t;

static bool parse_date_text(const char *text, short *year, month_t *month, uint8_t *day)
{
    datetime_t *parsed;

    if (!text || !year || !month || !day)
        return false;
    parsed = datetime_from_string(text);
    if (!parsed)
        return false;
    *year = datetime_year(parsed);
    *month = datetime_month(parsed);
    *day = datetime_day(parsed);
    datetime_dealloc(parsed);
    return true;
}

static bool parse_time_text(const char *text,
                            uint8_t *hour,
                            uint8_t *minute,
                            double *second)
{
    int parsed_hour;
    int parsed_minute;
    double parsed_second = 0.0;
    char tail[8];

    if (!text || !hour || !minute || !second)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;
    tail[0] = '\0';
    if (sscanf(text, "%d:%d:%lf%7s", &parsed_hour, &parsed_minute, &parsed_second, tail) < 2)
        return false;
    if (tail[0] != '\0')
        return false;
    if (parsed_hour < 0 || parsed_hour > 23 || parsed_minute < 0 || parsed_minute > 59)
        return false;
    if (!isfinite(parsed_second) || parsed_second < 0.0 || parsed_second >= 60.0)
        return false;
    *hour = (uint8_t)parsed_hour;
    *minute = (uint8_t)parsed_minute;
    *second = parsed_second;
    return true;
}

static bool parse_double_text(const char *text, double *out)
{
    char *end = NULL;
    double value;

    if (!text || !out)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;
    value = strtod(text, &end);
    if (end == text || !isfinite(value))
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;
    *out = value;
    return true;
}

static double normalize_degrees(double degrees)
{
    double normalized = fmod(degrees, 360.0);

    if (normalized < 0.0)
        normalized += 360.0;
    return normalized;
}

static const char *body_kind_text(almanac_body_kind_t kind)
{
    switch (kind) {
    case ALMANAC_BODY_STAR:
        return "star";
    case ALMANAC_BODY_PLANET:
        return "planet";
    case ALMANAC_BODY_SUN:
        return "sun";
    case ALMANAC_BODY_MOON:
        return "moon";
    default:
        return "body";
    }
}

static void set_default_options(almanac_lab_options_t *options)
{
    time_t now;
    struct tm utc_tm;

    if (!options)
        return;
    memset(options, 0, sizeof(*options));
    time(&now);
    utc_tm = *gmtime(&now);
    options->year = (short)(utc_tm.tm_year + 1900);
    options->month = (month_t)(utc_tm.tm_mon + 1);
    options->day = (uint8_t)utc_tm.tm_mday;
    options->hour = (uint8_t)utc_tm.tm_hour;
    options->minute = (uint8_t)utc_tm.tm_min;
    options->second = (double)utc_tm.tm_sec;
    options->zone = 0.0;
    options->latitude = 51.5074;
    options->longitude = -0.1278;
    options->body_id = ALMANAC_BODY_ID_MOON;
}

static bool key_equals(const char *got, size_t got_len, const char *want)
{
    return strlen(want) == got_len && strncmp(got, want, got_len) == 0;
}

static bool parse_options(int argc, char **argv, almanac_lab_options_t *options)
{
    int i;

    if (!options)
        return false;
    set_default_options(options);

    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        char *equals = NULL;
        size_t key_len;
        const char *value;

        if (!arg)
            continue;
        equals = strchr(arg, '=');
        if (!equals)
            return false;
        key_len = (size_t)(equals - arg);
        value = equals + 1;

        if (key_equals(arg, key_len, "date")) {
            if (!parse_date_text(value, &options->year, &options->month, &options->day))
                return false;
        } else if (key_equals(arg, key_len, "time")) {
            if (!parse_time_text(value, &options->hour, &options->minute, &options->second))
                return false;
        } else if (key_equals(arg, key_len, "zone")) {
            if (!parse_double_text(value, &options->zone))
                return false;
        } else if (key_equals(arg, key_len, "lat")) {
            if (!parse_double_text(value, &options->latitude))
                return false;
        } else if (key_equals(arg, key_len, "lon")) {
            if (!parse_double_text(value, &options->longitude))
                return false;
        } else if (key_equals(arg, key_len, "body")) {
            almanac_body_id_t body_id = almanac_body_id_from_code(value);

            if (body_id == ALMANAC_BODY_ID_UNKNOWN)
                return false;
            options->body_id = body_id;
        } else {
            return false;
        }
    }

    if (options->latitude < -90.0 || options->latitude > 90.0)
        return false;
    if (options->longitude < -180.0 || options->longitude > 180.0)
        return false;
    if (options->zone < -14.0 || options->zone > 14.0)
        return false;
    return true;
}

static void format_optional_double(char *out, size_t out_size, const char *format, double value)
{
    if (!out || out_size == 0u)
        return;
    if (!isfinite(value)) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_size, format, value);
}

static void print_snapshot_row(const char *code,
                               const char *name,
                               const char *kind,
                               double declination,
                               double right_ascension_hours,
                               double gha,
                               double sha,
                               double lha,
                               double distance_au,
                               double phase_degrees,
                               double magnitude,
                               double altitude,
                               double azimuth,
                               double semi_diameter,
                               const char *visible)
{
    char declination_text[32];
    char right_ascension_text[32];
    char gha_text[32];
    char sha_text[32];
    char lha_text[32];
    char distance_text[32];
    char phase_text[32];
    char magnitude_text[32];
    char altitude_text[32];
    char azimuth_text[32];
    char semi_diameter_text[32];

    format_optional_double(declination_text, sizeof(declination_text), "%.9f", declination);
    format_optional_double(right_ascension_text, sizeof(right_ascension_text), "%.9f", right_ascension_hours);
    format_optional_double(gha_text, sizeof(gha_text), "%.9f", gha);
    format_optional_double(sha_text, sizeof(sha_text), "%.9f", sha);
    format_optional_double(lha_text, sizeof(lha_text), "%.9f", lha);
    format_optional_double(distance_text, sizeof(distance_text), "%.12f", distance_au);
    format_optional_double(phase_text, sizeof(phase_text), "%.9f", phase_degrees);
    format_optional_double(magnitude_text, sizeof(magnitude_text), "%.9f", magnitude);
    format_optional_double(altitude_text, sizeof(altitude_text), "%.9f", altitude);
    format_optional_double(azimuth_text, sizeof(azimuth_text), "%.9f", azimuth);
    format_optional_double(semi_diameter_text, sizeof(semi_diameter_text), "%.9f", semi_diameter);

    printf("snapshot %s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
           code ? code : "",
           name ? name : "",
           kind ? kind : "",
           declination_text,
           right_ascension_text,
           gha_text,
           sha_text,
           lha_text,
           distance_text,
           phase_text,
           magnitude_text,
           altitude_text,
           azimuth_text,
           semi_diameter_text,
           visible ? visible : "");
}

int main(int argc, char **argv)
{
    almanac_lab_options_t options;
    datetime_t *moment = NULL;
    almanac_t *almanac = NULL;
    almanac_entry_t selected;
    almanac_observer_t observer;
    almanac_observables_t selected_observables;
    array_t *snapshot = NULL;
    double gha_aries = NAN;
    size_t i;

    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr,
                "usage: %s date=YYYY-MM-DD time=HH:MM[:SS] zone=0 lat=51.5074 lon=-0.1278 body=MOON\n",
                argc > 0 ? argv[0] : "almanac_lab");
        return 2;
    }

    moment = datetime_alloc();
    if (!moment ||
        !datetime_init_ymdt(moment,
                            options.year,
                            options.month,
                            options.day,
                            options.hour,
                            options.minute,
                            options.second)) {
        fprintf(stderr, "failed to create almanac moment\n");
        datetime_dealloc(moment);
        return 1;
    }

    almanac = almanac_open();
    if (!almanac) {
        fprintf(stderr, "failed to open almanac database\n");
        datetime_dealloc(moment);
        return 1;
    }

    if (!almanac_gha_aries(almanac, moment, &gha_aries)) {
        fprintf(stderr, "%s\n", almanac_last_error(almanac));
        almanac_close(almanac);
        datetime_dealloc(moment);
        return 1;
    }
    if (!almanac_lookup_body(almanac, options.body_id, moment, &selected)) {
        fprintf(stderr, "%s\n", almanac_last_error(almanac));
        almanac_close(almanac);
        datetime_dealloc(moment);
        return 1;
    }
    snapshot = almanac_snapshot(almanac, moment);
    if (!snapshot) {
        fprintf(stderr, "%s\n", almanac_last_error(almanac));
        almanac_close(almanac);
        datetime_dealloc(moment);
        return 1;
    }

    printf("date %04d-%02d-%02d\n", options.year, (int)options.month, (int)options.day);
    printf("time %02d:%02d:%04.1f\n", (int)options.hour, (int)options.minute, options.second);
    printf("zone %.2f\n", options.zone);
    printf("latitude %.6f\n", options.latitude);
    printf("longitude %.6f\n", options.longitude);
    printf("body %s\n", almanac_body_code(options.body_id));
    printf("gha_aries %.9f\n", gha_aries);
    observer.latitude_degrees = options.latitude;
    observer.longitude_degrees = options.longitude;
    observer.elevation_metres = 0.0;
    if (!almanac_observables(almanac, &selected, &observer, &selected_observables)) {
        selected_observables.altitude_degrees = NAN;
        selected_observables.azimuth_degrees = NAN;
        selected_observables.semi_diameter_degrees = NAN;
        selected_observables.above_horizon = false;
        selected_observables.visible = false;
    }
    printf("selected_name %s\n", almanac_body_display_name(selected.body_id));
    printf("selected_kind %s\n", body_kind_text(selected.body_kind));
    printf("selected_declination %.9f\n", selected.declination_degrees);
    printf("selected_right_ascension %.9f\n", selected.right_ascension_hours);
    printf("selected_gha %.9f\n", normalize_degrees(selected.gha_aries_degrees + selected.sha_degrees));
    printf("selected_sha %.9f\n", selected.sha_degrees);
    printf("selected_lha %.9f\n",
           normalize_degrees(selected.gha_aries_degrees + selected.sha_degrees - options.longitude));
    printf("selected_geo_distance %.12f\n", selected.geocentric_distance_au);
    printf("selected_helio_distance %.12f\n", selected.heliocentric_distance_au);
    printf("selected_phase %.9f\n", selected.phase_angle_degrees);
    printf("selected_visual_magnitude %.9f\n", selected.visual_magnitude);
    printf("selected_altitude %.9f\n", selected_observables.altitude_degrees);
    printf("selected_azimuth %.9f\n", selected_observables.azimuth_degrees);
    printf("selected_semi_diameter %.9f\n", selected_observables.semi_diameter_degrees);
    printf("selected_visible %s\n", selected_observables.visible ? "YES" : "NO");

    print_snapshot_row("ARIES",
                       "Aries",
                       "reference",
                       NAN,
                       NAN,
                       gha_aries,
                       0.0,
                       normalize_degrees(gha_aries - options.longitude),
                       NAN,
                       NAN,
                       NAN,
                       NAN,
                       NAN,
                       NAN,
                       "");
    for (i = 0u; i < array_size(snapshot); ++i) {
        const almanac_entry_t *entry = array_get(snapshot, i);
        almanac_observables_t observables;
        double gha;

        if (!entry)
            continue;
        if (!almanac_observables(almanac, entry, &observer, &observables)) {
            observables.altitude_degrees = NAN;
            observables.azimuth_degrees = NAN;
            observables.semi_diameter_degrees = NAN;
            observables.above_horizon = false;
            observables.visible = false;
        }
        gha = normalize_degrees(entry->gha_aries_degrees + entry->sha_degrees);
        print_snapshot_row(almanac_body_code(entry->body_id),
                           almanac_body_display_name(entry->body_id),
                           body_kind_text(entry->body_kind),
                           entry->declination_degrees,
                           entry->right_ascension_hours,
                           gha,
                           entry->sha_degrees,
                           normalize_degrees(gha - options.longitude),
                           entry->geocentric_distance_au,
                           entry->phase_angle_degrees,
                           entry->visual_magnitude,
                           observables.altitude_degrees,
                           observables.azimuth_degrees,
                           observables.semi_diameter_degrees,
                           observables.visible ? "YES" : "NO");
    }

    array_destroy(snapshot);
    almanac_close(almanac);
    datetime_dealloc(moment);
    return 0;
}
