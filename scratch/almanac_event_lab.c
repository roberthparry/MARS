#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "almanac.h"
#include "sqlite.h"

typedef struct almanac_event_lab_options_t {
    short start_year;
    month_t start_month;
    uint8_t start_day;
    short end_year;
    month_t end_month;
    uint8_t end_day;
    double latitude;
    double longitude;
    bool totality_land;
} almanac_event_lab_options_t;

#define ALMANAC_EVENT_LAB_CACHE_SCHEMA "almanac_event_lab_output_v3"
#define ALMANAC_EVENT_LAB_CACHE_PATH_ENV "MARS_LAB_OBJECT_STORE_PATH"
#define ALMANAC_EVENT_LAB_CACHE_KEY_ENV "MARS_LAB_OBJECT_STORE_KEY"

static bool parse_date_text(const char *text, short *year, month_t *month, uint8_t *day)
{
    int y;
    int m;
    int d;
    char tail;

    if (!text || !year || !month || !day)
        return false;
    if (sscanf(text, "%d-%d-%d%c", &y, &m, &d, &tail) != 3)
        return false;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    if (!datetime_valid_ymd((short)y, (month_t)m, (uint8_t)d))
        return false;
    *year = (short)y;
    *month = (month_t)m;
    *day = (uint8_t)d;
    return true;
}

static bool parse_double_text(const char *text, double *out)
{
    char *end = NULL;
    double value;

    if (!text || !out)
        return false;
    value = strtod(text, &end);
    if (end == text || *end != '\0')
        return false;
    *out = value;
    return true;
}

static bool key_equals(const char *got, size_t got_len, const char *want)
{
    return strlen(want) == got_len && strncmp(got, want, got_len) == 0;
}

static sqlite_t *open_lab_object_store(void)
{
    const char *path = getenv(ALMANAC_EVENT_LAB_CACHE_PATH_ENV);
    const char *key = getenv(ALMANAC_EVENT_LAB_CACHE_KEY_ENV);
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

static string_t *cache_key_for_options(const almanac_event_lab_options_t *options)
{
    char key[256];

    if (!options)
        return NULL;
    if (snprintf(key,
                 sizeof(key),
                 "mars_lab/%s/start=%04d-%02d-%02d/end=%04d-%02d-%02d/lat=%.9f/lon=%.9f/totality=%s",
                 ALMANAC_EVENT_LAB_CACHE_SCHEMA,
                 options->start_year,
                 (int)options->start_month,
                 (int)options->start_day,
                 options->end_year,
                 (int)options->end_month,
                 (int)options->end_day,
                 options->latitude,
                 options->longitude,
                 options->totality_land ? "land" : "none") >= (int)sizeof(key)) {
        return NULL;
    }
    return string_new_with(key);
}

static string_t *load_cached_output(const almanac_event_lab_options_t *options)
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

static void store_cached_output(const almanac_event_lab_options_t *options, const string_t *output)
{
    sqlite_t *db = NULL;
    string_t *name = NULL;

    if (!output)
        return;
    name = cache_key_for_options(options);
    if (!name)
        return;
    db = open_lab_object_store();
    if (db)
        (void)sqlite_store_string(db, name, output);
    sqlite_close(db);
    string_free(name);
}

static void init_defaults(almanac_event_lab_options_t *options)
{
    memset(options, 0, sizeof(*options));
    options->start_year = 2026;
    options->start_month = DT_January;
    options->start_day = 1;
    options->end_year = 2026;
    options->end_month = DT_December;
    options->end_day = 31;
    options->latitude = 52.7073;
    options->longitude = -2.7540;
    options->totality_land = false;
}

static bool parse_options(int argc, char **argv, almanac_event_lab_options_t *options)
{
    int i;

    if (!options)
        return false;
    init_defaults(options);
    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        char *equals = arg ? strchr(arg, '=') : NULL;
        const char *value;
        size_t key_len;

        if (!equals)
            return false;
        key_len = (size_t)(equals - arg);
        value = equals + 1;
        if (key_equals(arg, key_len, "start")) {
            if (!parse_date_text(value, &options->start_year, &options->start_month, &options->start_day))
                return false;
        } else if (key_equals(arg, key_len, "end")) {
            if (!parse_date_text(value, &options->end_year, &options->end_month, &options->end_day))
                return false;
        } else if (key_equals(arg, key_len, "lat")) {
            if (!parse_double_text(value, &options->latitude))
                return false;
        } else if (key_equals(arg, key_len, "lon")) {
            if (!parse_double_text(value, &options->longitude))
                return false;
        } else if (key_equals(arg, key_len, "totality")) {
            options->totality_land = strcmp(value, "land") == 0;
        } else {
            return false;
        }
    }
    return options->latitude >= -90.0 && options->latitude <= 90.0 &&
           options->longitude >= -180.0 && options->longitude <= 180.0;
}

static const char *solar_kind_text(almanac_solar_eclipse_kind_t kind)
{
    switch (kind) {
    case ALMANAC_SOLAR_ECLIPSE_TOTAL:
        return "total";
    case ALMANAC_SOLAR_ECLIPSE_ANNULAR:
        return "annular";
    case ALMANAC_SOLAR_ECLIPSE_PARTIAL:
        return "partial";
    default:
        return "solar";
    }
}

static const char *lunar_kind_text(almanac_lunar_eclipse_kind_t kind)
{
    switch (kind) {
    case ALMANAC_LUNAR_ECLIPSE_TOTAL:
        return "total";
    case ALMANAC_LUNAR_ECLIPSE_PARTIAL:
        return "partial";
    case ALMANAC_LUNAR_ECLIPSE_PENUMBRAL:
        return "penumbral";
    default:
        return "lunar";
    }
}

static void append_solar_events(string_t *out,
                                almanac_t *almanac,
                                const almanac_observer_t *observer,
                                bool totality_land,
                                const array_t *events)
{
    size_t i;

    if (!out)
        return;
    for (i = 0u; events && i < array_size(events); ++i) {
        const almanac_solar_eclipse_t *event = array_get(events, i);
        almanac_solar_totality_location_t nearest_totality;
        char nearest_totality_text[128];

        if (!event)
            continue;
        nearest_totality_text[0] = '\0';
        if (totality_land &&
            almanac_nearest_solar_totality_land(almanac, observer, event, &nearest_totality) &&
            nearest_totality.found) {
            snprintf(nearest_totality_text,
                     sizeof(nearest_totality_text),
                     "%.6f,%.6f,%.9f,%.1f",
                     nearest_totality.latitude_degrees,
                     nearest_totality.longitude_degrees,
                     nearest_totality.greatest_eclipse_jd,
                     nearest_totality.distance_km);
        }
        (void)string_append_format(out,
                                   "event Solar|Solar eclipse|%s|%.9f|%.9f|%.9f|%.6f|%.3f|%s\n",
                                   solar_kind_text(event->kind),
                                   event->greatest_eclipse_jd,
                                   event->first_contact_jd,
                                   event->fourth_contact_jd,
                                   event->magnitude,
                                   event->totality_percent,
                                   nearest_totality_text);
    }
}

static void append_lunar_events(string_t *out, const array_t *events)
{
    size_t i;

    if (!out)
        return;
    for (i = 0u; events && i < array_size(events); ++i) {
        const almanac_lunar_eclipse_t *event = array_get(events, i);

        if (!event)
            continue;
        (void)string_append_format(out,
                                   "event Lunar|Lunar eclipse|%s|%.9f|%.9f|%.9f|%.6f|%.3f\n",
                                   lunar_kind_text(event->kind),
                                   event->greatest_eclipse_jd,
                                   event->p1_contact_jd,
                                   event->p4_contact_jd,
                                   event->umbral_magnitude,
                                   event->totality_percent);
    }
}

int main(int argc, char **argv)
{
    almanac_event_lab_options_t options;
    string_t *cached_output = NULL;
    string_t *output = NULL;
    datetime_t *start = NULL;
    datetime_t *end = NULL;
    almanac_t *almanac = NULL;
    almanac_observer_t observer;
    array_t *solar_events = NULL;
    array_t *lunar_events = NULL;
    int status = 1;

    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr, "usage: %s start=YYYY-MM-DD end=YYYY-MM-DD lat=52.7073 lon=-2.7540\n", argc > 0 ? argv[0] : "almanac_event_lab");
        return 2;
    }
    cached_output = load_cached_output(&options);
    if (cached_output) {
        fputs(string_c_str(cached_output), stdout);
        string_free(cached_output);
        return 0;
    }
    output = string_new();
    if (!output) {
        fprintf(stderr, "failed to allocate almanac event output buffer\n");
        return 1;
    }
    start = datetime_alloc();
    end = datetime_alloc();
    if (!start || !end ||
        !datetime_init_ymdt(start, options.start_year, options.start_month, options.start_day, 0, 0, 0.0) ||
        !datetime_init_ymdt(end, options.end_year, options.end_month, options.end_day, 23, 59, 59.0))
        goto done;

    observer.latitude_degrees = options.latitude;
    observer.longitude_degrees = options.longitude;
    observer.elevation_metres = 0.0;

    almanac = almanac_open();
    if (!almanac)
        goto done;

    solar_events = almanac_find_solar_eclipses(almanac, &observer, start, end);
    if (!solar_events)
        goto done;
    lunar_events = almanac_find_lunar_eclipses(almanac, &observer, start, end);
    if (!lunar_events)
        goto done;

    append_solar_events(output, almanac, &observer, options.totality_land, solar_events);
    append_lunar_events(output, lunar_events);
    store_cached_output(&options, output);
    fputs(string_c_str(output), stdout);
    status = 0;

done:
    if (status != 0 && almanac)
        fprintf(stderr, "%s\n", almanac_last_error(almanac));
    array_destroy(solar_events);
    array_destroy(lunar_events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
    string_free(output);
    return status;
}
