#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "almanac.h"
#include "sqlite.h"

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
    bool cache_only;
    bool cache_put;
} almanac_lab_options_t;

#define ALMANAC_LAB_CACHE_SCHEMA "almanac_lab_output_v6"
#define ALMANAC_LAB_CACHE_PATH_ENV "MARS_LAB_OBJECT_STORE_PATH"
#define ALMANAC_LAB_CACHE_KEY_ENV "MARS_LAB_OBJECT_STORE_KEY"

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

static sqlite_t *open_lab_object_store(void)
{
    const char *path = getenv(ALMANAC_LAB_CACHE_PATH_ENV);
    const char *key = getenv(ALMANAC_LAB_CACHE_KEY_ENV);
    string_t *path_text = NULL;
    string_t *key_text = NULL;
    sqlite_t *db = NULL;

    if (!path || !*path || !key || !*key)
        return NULL;
    path_text = string_new_with(path);
    key_text = string_new_with(key);
    if (!path_text || !key_text)
        goto done;
    db = sqlite_open_encrypted(path_text, key_text);
    if (!db) {
        (void)remove(path);
        db = sqlite_open_encrypted(path_text, key_text);
    }
    if (db)
        (void)chmod(path, S_IRUSR | S_IWUSR);
    if (db && !sqlite_init_object_store(db)) {
        sqlite_close(db);
        db = NULL;
    }

done:
    string_free(key_text);
    string_free(path_text);
    return db;
}

static string_t *cache_key_for_options(const almanac_lab_options_t *options)
{
    char key[256];

    if (!options)
        return NULL;
    if (snprintf(key,
                 sizeof(key),
                 "mars_lab/%s/date=%04d-%02d-%02d/time=%02d:%02d:%010.6f/zone=%.9f/lat=%.9f/lon=%.9f/body=%d",
                 ALMANAC_LAB_CACHE_SCHEMA,
                 options->year,
                 (int)options->month,
                 (int)options->day,
                 (int)options->hour,
                 (int)options->minute,
                 options->second,
                 options->zone,
                 options->latitude,
                 options->longitude,
                 (int)options->body_id) >= (int)sizeof(key)) {
        return NULL;
    }
    return string_new_with(key);
}

static string_t *load_cached_output(const almanac_lab_options_t *options)
{
    sqlite_t *db = NULL;
    string_t *name = NULL;
    string_t *output = NULL;

    name = cache_key_for_options(options);
    if (!name)
        return NULL;
    db = open_lab_object_store();
    if (!db)
        goto done;
    if (!sqlite_load_string(db, name, &output))
        output = NULL;

done:
    sqlite_close(db);
    string_free(name);
    return output;
}

static bool store_cached_output(const almanac_lab_options_t *options, const string_t *output)
{
    sqlite_t *db = NULL;
    string_t *name = NULL;
    bool stored = false;

    if (!output)
        return false;
    name = cache_key_for_options(options);
    if (!name)
        return false;
    db = open_lab_object_store();
    if (db) {
        stored = sqlite_store_string(db, name, output);
        if (!stored) {
            const string_t *error = sqlite_last_error(db);

            fprintf(stderr, "failed to store almanac cache object: %s\n", error ? string_c_str(error) : "unknown sqlite error");
        }
    } else {
        fprintf(stderr, "failed to open almanac object store\n");
    }
    sqlite_close(db);
    string_free(name);
    return stored;
}

static string_t *read_standard_input(void)
{
    char buffer[4096];
    string_t *input = string_new();

    if (!input)
        return NULL;
    for (;;) {
        size_t got = fread(buffer, 1u, sizeof(buffer), stdin);

        if (got > 0u && string_append_chars(input, buffer, got) != 0) {
            string_free(input);
            return NULL;
        }
        if (got < sizeof(buffer)) {
            if (ferror(stdin)) {
                string_free(input);
                return NULL;
            }
            break;
        }
    }
    return input;
}

static bool parse_boolean_text(const char *text, bool *out)
{
    if (!text || !out)
        return false;
    if (strcmp(text, "1") == 0 || strcmp(text, "true") == 0 || strcmp(text, "yes") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(text, "0") == 0 || strcmp(text, "false") == 0 || strcmp(text, "no") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static const char *body_kind_text(almanac_body_kind_t kind)
{
    static const char *const text_by_kind[ALMANAC_BODY_MOON + 1] = {
        [ALMANAC_BODY_STAR] = "star",
        [ALMANAC_BODY_PLANET] = "planet",
        [ALMANAC_BODY_SUN] = "sun",
        [ALMANAC_BODY_MOON] = "moon"
    };

    if (kind < ALMANAC_BODY_STAR || kind > ALMANAC_BODY_MOON)
        return "body";
    return text_by_kind[kind];
}

static double angular_separation_degrees(const almanac_entry_t *a,
                                         const almanac_entry_t *b)
{
    double ra_a;
    double ra_b;
    double dec_a;
    double dec_b;
    double cos_separation;

    if (!a || !b)
        return NAN;
    ra_a = almanac_entry_right_ascension_hours(a) * (M_PI / 12.0);
    ra_b = almanac_entry_right_ascension_hours(b) * (M_PI / 12.0);
    dec_a = almanac_entry_declination_degrees(a) * (M_PI / 180.0);
    dec_b = almanac_entry_declination_degrees(b) * (M_PI / 180.0);
    if (!isfinite(ra_a) || !isfinite(ra_b) || !isfinite(dec_a) || !isfinite(dec_b))
        return NAN;
    cos_separation = sin(dec_a) * sin(dec_b) +
                     cos(dec_a) * cos(dec_b) * cos(ra_a - ra_b);
    if (cos_separation < -1.0)
        cos_separation = -1.0;
    else if (cos_separation > 1.0)
        cos_separation = 1.0;
    return acos(cos_separation) * (180.0 / M_PI);
}

static bool visually_visible(const almanac_entry_t *entry,
                             const almanac_observables_t *observables,
                             double sun_altitude_degrees,
                             const almanac_entry_t *sun_entry,
                             bool local_solar_eclipse_in_progress)
{
    static const double CIVIL_TWILIGHT_SUN_ALTITUDE_DEGREES = -6.0;
    static const double NAUTICAL_TWILIGHT_SUN_ALTITUDE_DEGREES = -12.0;
    double solar_separation;
    double magnitude;
    almanac_body_id_t body_id;
    almanac_body_kind_t body_kind;

    if (!entry || !observables || !observables->visible)
        return false;
    body_id = almanac_entry_body_id(entry);
    body_kind = almanac_entry_body_kind(entry);
    if (body_kind == ALMANAC_BODY_SUN)
        return true;
    if (body_id == ALMANAC_BODY_ID_MOON && local_solar_eclipse_in_progress)
        return true;

    solar_separation = angular_separation_degrees(entry, sun_entry);
    magnitude = almanac_entry_visual_magnitude(entry);

    if (!isfinite(sun_altitude_degrees))
        return body_kind == ALMANAC_BODY_MOON;

    if (sun_altitude_degrees > 0.0) {
        if (body_kind == ALMANAC_BODY_MOON) {
            return observables->altitude_degrees > 5.0 &&
                   (!isfinite(solar_separation) || solar_separation >= 12.0);
        }
        if (body_id == ALMANAC_BODY_ID_VENUS) {
            return isfinite(magnitude) && magnitude <= -3.5 &&
                   observables->altitude_degrees >= 10.0 &&
                   isfinite(solar_separation) && solar_separation >= 20.0;
        }
        return false;
    }

    if (sun_altitude_degrees > CIVIL_TWILIGHT_SUN_ALTITUDE_DEGREES) {
        if (body_kind == ALMANAC_BODY_MOON)
            return true;
        if (body_kind == ALMANAC_BODY_PLANET)
            return isfinite(magnitude) && magnitude <= -1.0 &&
                   observables->altitude_degrees >= 3.0;
        return false;
    }

    if (sun_altitude_degrees > NAUTICAL_TWILIGHT_SUN_ALTITUDE_DEGREES) {
        if (body_kind == ALMANAC_BODY_MOON)
            return true;
        if (body_kind == ALMANAC_BODY_PLANET)
            return !isfinite(magnitude) || magnitude <= 2.0;
        if (body_kind == ALMANAC_BODY_STAR)
            return isfinite(magnitude) && magnitude <= 1.5 &&
                   observables->altitude_degrees >= 5.0;
        return false;
    }

    if (body_kind == ALMANAC_BODY_MOON)
        return true;
    if (body_kind == ALMANAC_BODY_PLANET)
        return !isfinite(magnitude) || magnitude <= 6.0;
    if (body_kind == ALMANAC_BODY_STAR)
        return !isfinite(magnitude) || magnitude <= 6.5;
    return true;
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
    options->cache_only = false;
    options->cache_put = false;
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
        } else if (key_equals(arg, key_len, "cache_only")) {
            if (!parse_boolean_text(value, &options->cache_only))
                return false;
        } else if (key_equals(arg, key_len, "cache_put")) {
            if (!parse_boolean_text(value, &options->cache_put))
                return false;
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

static void format_unsigned_angle(char *out, size_t out_size, double degrees)
{
    double total_minutes;
    int whole_degrees;
    double minutes;

    if (!out || out_size == 0u)
        return;
    if (!isfinite(degrees)) {
        out[0] = '\0';
        return;
    }

    total_minutes = round(normalize_degrees(degrees) * 6000.0) / 100.0;
    whole_degrees = (int)(total_minutes / 60.0);
    minutes = total_minutes - (double)(whole_degrees * 60);
    if (minutes >= 60.0) {
        whole_degrees = (whole_degrees + 1) % 360;
        minutes = 0.0;
    }

    snprintf(out, out_size, "%03d\xC2\xB0 %05.2f", whole_degrees, minutes);
}

static void format_signed_angle(char *out, size_t out_size, double degrees)
{
    double total_minutes;
    int whole_degrees;
    double minutes;
    const char *sign;

    if (!out || out_size == 0u)
        return;
    if (!isfinite(degrees)) {
        out[0] = '\0';
        return;
    }

    sign = degrees < 0.0 ? "-" : "";
    total_minutes = round(fabs(degrees) * 6000.0) / 100.0;
    whole_degrees = (int)(total_minutes / 60.0);
    minutes = total_minutes - (double)(whole_degrees * 60);
    if (minutes >= 60.0) {
        ++whole_degrees;
        minutes = 0.0;
    }

    snprintf(out, out_size, "%s%03d\xC2\xB0 %05.2f", sign, whole_degrees, minutes);
}

static void format_declination_angle(char *out, size_t out_size, double degrees)
{
    double total_minutes;
    int whole_degrees;
    double minutes;
    const char *hemisphere;

    if (!out || out_size == 0u)
        return;
    if (!isfinite(degrees)) {
        out[0] = '\0';
        return;
    }

    hemisphere = degrees < 0.0 ? "S" : "N";
    total_minutes = round(fabs(degrees) * 6000.0) / 100.0;
    whole_degrees = (int)(total_minutes / 60.0);
    minutes = total_minutes - (double)(whole_degrees * 60);
    if (minutes >= 60.0) {
        ++whole_degrees;
        minutes = 0.0;
    }

    snprintf(out, out_size, "%s %03d\xC2\xB0 %05.2f", hemisphere, whole_degrees, minutes);
}

static void format_right_ascension(char *out, size_t out_size, double hours)
{
    format_unsigned_angle(out, out_size, hours * 15.0);
}

static void format_semi_diameter(char *out, size_t out_size, double degrees)
{
    if (!out || out_size == 0u)
        return;
    if (!isfinite(degrees)) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_size, "%05.2f", degrees * 60.0);
}

static void append_snapshot_row(string_t *out,
                                const char *code,
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
    char declination_display[32];
    char right_ascension_display[32];
    char gha_display[32];
    char altitude_display[32];
    char azimuth_display[32];
    char semi_diameter_display[32];

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
    format_declination_angle(declination_display, sizeof(declination_display), declination);
    format_right_ascension(right_ascension_display, sizeof(right_ascension_display), right_ascension_hours);
    format_unsigned_angle(gha_display, sizeof(gha_display), gha);
    format_signed_angle(altitude_display, sizeof(altitude_display), altitude);
    format_unsigned_angle(azimuth_display, sizeof(azimuth_display), azimuth);
    format_semi_diameter(semi_diameter_display, sizeof(semi_diameter_display), semi_diameter);

    if (!out)
        return;
    (void)string_append_format(out,
                               "snapshot %s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n",
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
                               visible ? visible : "",
                               declination_display,
                               right_ascension_display,
                               gha_display,
                               altitude_display,
                               azimuth_display,
                               semi_diameter_display);
}

int main(int argc, char **argv)
{
    almanac_lab_options_t options;
    string_t *cached_output = NULL;
    string_t *output = NULL;
    datetime_t *moment = NULL;
    almanac_t *almanac = NULL;
    almanac_entry_t *selected = NULL;
    almanac_observer_t observer;
    almanac_observables_t selected_observables;
    almanac_geographical_position_t selected_gp;
    almanac_observables_t sun_observables;
    array_t *snapshot = NULL;
    double gha_aries = NAN;
    double sun_altitude_degrees = NAN;
    const almanac_entry_t *sun_entry = NULL;
    bool local_solar_eclipse_in_progress = false;
    size_t i;

    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr,
                "usage: %s date=YYYY-MM-DD time=HH:MM[:SS] zone=0 lat=51.5074 lon=-0.1278 body=MOON\n",
                argc > 0 ? argv[0] : "almanac_lab");
        return 2;
    }

    if (options.cache_put) {
        string_t *input = read_standard_input();

        if (!input) {
            fprintf(stderr, "failed to read almanac cache input\n");
            return 1;
        }
        if (!store_cached_output(&options, input)) {
            fprintf(stderr, "failed to store almanac cache input\n");
            string_free(input);
            return 1;
        }
        string_free(input);
        return 0;
    }

    cached_output = load_cached_output(&options);
    if (cached_output) {
        fputs(string_c_str(cached_output), stdout);
        string_free(cached_output);
        return 0;
    }
    if (options.cache_only)
        return 3;

    output = string_new();
    if (!output) {
        fprintf(stderr, "failed to allocate almanac output buffer\n");
        return 1;
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
        string_free(output);
        datetime_dealloc(moment);
        return 1;
    }

    almanac = almanac_open();
    if (!almanac) {
        fprintf(stderr, "failed to open almanac database\n");
        string_free(output);
        datetime_dealloc(moment);
        return 1;
    }
    if (!almanac_gha_aries(almanac, moment, &gha_aries)) {
        fprintf(stderr, "%s\n", almanac_last_error(almanac));
        almanac_close(almanac);
        string_free(output);
        datetime_dealloc(moment);
        return 1;
    }
    selected = almanac_new_body_entry(almanac, options.body_id, moment);
    if (!selected) {
        fprintf(stderr, "%s\n", almanac_last_error(almanac));
        almanac_close(almanac);
        string_free(output);
        datetime_dealloc(moment);
        return 1;
    }
    snapshot = almanac_snapshot(almanac, moment);
    if (!snapshot) {
        fprintf(stderr, "%s\n", almanac_last_error(almanac));
        almanac_entry_dealloc(selected);
        almanac_close(almanac);
        string_free(output);
        datetime_dealloc(moment);
        return 1;
    }

    (void)string_append_format(output, "date %04d-%02d-%02d\n", options.year, (int)options.month, (int)options.day);
    (void)string_append_format(output, "time %02d:%02d:%04.1f\n", (int)options.hour, (int)options.minute, options.second);
    (void)string_append_format(output, "zone %.2f\n", options.zone);
    (void)string_append_format(output, "latitude %.6f\n", options.latitude);
    (void)string_append_format(output, "longitude %.6f\n", options.longitude);
    (void)string_append_format(output, "body %s\n", almanac_body_code(options.body_id));
    (void)string_append_format(output, "gha_aries %.9f\n", gha_aries);
    observer.latitude_degrees = options.latitude;
    observer.longitude_degrees = options.longitude;
    observer.elevation_metres = 0.0;
    sun_observables.altitude_degrees = NAN;
    sun_observables.azimuth_degrees = NAN;
    sun_observables.semi_diameter_degrees = NAN;
    sun_observables.above_horizon = false;
    sun_observables.visible = false;
    for (i = 0u; i < array_size(snapshot); ++i) {
        const almanac_entry_t *entry = array_get(snapshot, i);

        if (!entry || almanac_entry_body_id(entry) != ALMANAC_BODY_ID_SUN)
            continue;
        sun_entry = entry;
        if (almanac_observables(almanac, entry, &observer, &sun_observables))
            sun_altitude_degrees = sun_observables.altitude_degrees;
        break;
    }
    local_solar_eclipse_in_progress = almanac_solar_eclipse_in_progress(almanac, &observer, moment);
    if (!almanac_observables(almanac, selected, &observer, &selected_observables)) {
        selected_observables.altitude_degrees = NAN;
        selected_observables.azimuth_degrees = NAN;
        selected_observables.semi_diameter_degrees = NAN;
        selected_observables.above_horizon = false;
        selected_observables.visible = false;
    }
    if (!almanac_body_geographical_position(selected, &selected_gp)) {
        selected_gp.latitude_degrees = NAN;
        selected_gp.longitude_degrees = NAN;
    }
    (void)string_append_format(output, "selected_name %s\n", almanac_body_display_name(almanac_entry_body_id(selected)));
    (void)string_append_format(output, "selected_kind %s\n", body_kind_text(almanac_entry_body_kind(selected)));
    (void)string_append_format(output, "selected_declination %.9f\n", almanac_entry_declination_degrees(selected));
    (void)string_append_format(output, "selected_right_ascension %.9f\n", almanac_entry_right_ascension_hours(selected));
    (void)string_append_format(output,
                               "selected_gha %.9f\n",
                               normalize_degrees(almanac_entry_gha_aries_degrees(selected) +
                                                 almanac_entry_sha_degrees(selected)));
    (void)string_append_format(output, "selected_sha %.9f\n", almanac_entry_sha_degrees(selected));
    (void)string_append_format(output,
                               "selected_lha %.9f\n",
                               normalize_degrees(almanac_entry_gha_aries_degrees(selected) +
                                                 almanac_entry_sha_degrees(selected) -
                                                 options.longitude));
    (void)string_append_format(output,
                               "selected_geo_distance %.12f\n",
                               almanac_entry_geocentric_distance_au(selected));
    (void)string_append_format(output,
                               "selected_helio_distance %.12f\n",
                               almanac_entry_heliocentric_distance_au(selected));
    (void)string_append_format(output, "selected_phase %.9f\n", almanac_entry_phase_angle_degrees(selected));
    (void)string_append_format(output, "selected_visual_magnitude %.9f\n", almanac_entry_visual_magnitude(selected));
    (void)string_append_format(output, "selected_altitude %.9f\n", selected_observables.altitude_degrees);
    (void)string_append_format(output, "selected_azimuth %.9f\n", selected_observables.azimuth_degrees);
    (void)string_append_format(output, "selected_semi_diameter %.9f\n", selected_observables.semi_diameter_degrees);
    (void)string_append_format(output, "selected_gp_latitude %.9f\n", selected_gp.latitude_degrees);
    (void)string_append_format(output, "selected_gp_longitude %.9f\n", selected_gp.longitude_degrees);
    (void)string_append_format(output,
                               "selected_visible %s\n",
                               visually_visible(selected,
                                                &selected_observables,
                                                sun_altitude_degrees,
                                                sun_entry,
                                                local_solar_eclipse_in_progress) ? "YES" : "NO");

    append_snapshot_row(output,
                        "ARIES",
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
        bool visible;

        if (!entry)
            continue;
        if (!almanac_observables(almanac, entry, &observer, &observables)) {
            observables.altitude_degrees = NAN;
            observables.azimuth_degrees = NAN;
            observables.semi_diameter_degrees = NAN;
            observables.above_horizon = false;
            observables.visible = false;
        }
        gha = normalize_degrees(almanac_entry_gha_aries_degrees(entry) + almanac_entry_sha_degrees(entry));
        visible = visually_visible(entry,
                                   &observables,
                                   sun_altitude_degrees,
                                   sun_entry,
                                   local_solar_eclipse_in_progress);
        append_snapshot_row(output,
                            almanac_body_code(almanac_entry_body_id(entry)),
                            almanac_body_display_name(almanac_entry_body_id(entry)),
                            body_kind_text(almanac_entry_body_kind(entry)),
                            almanac_entry_declination_degrees(entry),
                            almanac_entry_right_ascension_hours(entry),
                            gha,
                            almanac_entry_sha_degrees(entry),
                            normalize_degrees(gha - options.longitude),
                            almanac_entry_geocentric_distance_au(entry),
                            almanac_entry_phase_angle_degrees(entry),
                            almanac_entry_visual_magnitude(entry),
                            observables.altitude_degrees,
                            observables.azimuth_degrees,
                            observables.semi_diameter_degrees,
                            visible ? "YES" : "NO");
    }

    (void)store_cached_output(&options, output);
    fputs(string_c_str(output), stdout);
    string_free(output);
    array_destroy(snapshot);
    almanac_entry_dealloc(selected);
    almanac_close(almanac);
    datetime_dealloc(moment);
    return 0;
}
