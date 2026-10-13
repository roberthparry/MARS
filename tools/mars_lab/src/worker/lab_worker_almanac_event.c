/**
 * @file lab_worker_almanac_event.c
 * @brief Astronomical event backend for MARS Lab.
 *
 * Linked into the MARS Lab server and dispatched in an isolated calculation process.
 * Parses event requests, searches supported solar and lunar events and formats result fields for the Lab
 * interface. It delegates geometry to the native almanac engine and uses configured catalogue and location data.
 */

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "almanac-t.h"
#include "jurisdiction-t.h"
#include "sqlite-t.h"

#define MARS_SHARED_ALMANAC_INTERNAL_ACCESS
#include "internal/almanac_internal.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef enum almanac_event_lab_kind_t {
    ALMANAC_EVENT_LAB_KIND_ALL = 0,
    ALMANAC_EVENT_LAB_KIND_SOLAR,
    ALMANAC_EVENT_LAB_KIND_LUNAR
} almanac_event_lab_kind_t;

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
    almanac_event_lab_kind_t kind;
} almanac_event_lab_options_t;

#define ALMANAC_EVENT_LAB_CACHE_SCHEMA "almanac_event_lab_output_v22"
#define ALMANAC_EVENT_LAB_CACHE_PATH_ENV "MARS_LAB_OBJECT_STORE_PATH"
#define ALMANAC_EVENT_LAB_CACHE_KEY_ENV "MARS_LAB_OBJECT_STORE_KEY"
#define TOTALITY_TOWN_SCORE_LIMIT 1024u

typedef struct almanac_totality_town_t {
    char jurisdiction_id[32];
    char town_name[96];
    char timezone_name[96];
    double latitude_degrees;
    double longitude_degrees;
    double elevation_metres;
    double distance_km;
} almanac_totality_town_t;

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
    const char *kind;

    if (!options)
        return NULL;
    kind = options->kind == ALMANAC_EVENT_LAB_KIND_SOLAR   ? "solar"
           : options->kind == ALMANAC_EVENT_LAB_KIND_LUNAR ? "lunar"
                                                           : "all";
    if (snprintf(key, sizeof(key),
                 "mars_lab/%s/start=%04d-%02d-%02d/end=%04d-%02d-%02d/lat=%.9f/lon=%.9f/totality=%s/kind=%s",
                 ALMANAC_EVENT_LAB_CACHE_SCHEMA, options->start_year, (int)options->start_month,
                 (int)options->start_day, options->end_year, (int)options->end_month, (int)options->end_day,
                 options->latitude, options->longitude, options->totality_land ? "land" : "none",
                 kind) >= (int)sizeof(key)) {
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
    options->kind = ALMANAC_EVENT_LAB_KIND_ALL;
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
        } else if (key_equals(arg, key_len, "kind")) {
            if (strcmp(value, "solar") == 0)
                options->kind = ALMANAC_EVENT_LAB_KIND_SOLAR;
            else if (strcmp(value, "lunar") == 0)
                options->kind = ALMANAC_EVENT_LAB_KIND_LUNAR;
            else if (strcmp(value, "all") == 0)
                options->kind = ALMANAC_EVENT_LAB_KIND_ALL;
            else
                return false;
        } else {
            return false;
        }
    }
    return options->latitude >= -90.0 && options->latitude <= 90.0 && options->longitude >= -180.0 &&
           options->longitude <= 180.0;
}

static const char *solar_kind_text(almanac_solar_eclipse_kind_t kind)
{
    static const char *const text_by_kind[ALMANAC_SOLAR_ECLIPSE_TOTAL + 1] = {
        [ALMANAC_SOLAR_ECLIPSE_PARTIAL] = "partial",
        [ALMANAC_SOLAR_ECLIPSE_ANNULAR] = "annular",
        [ALMANAC_SOLAR_ECLIPSE_TOTAL] = "total"};

    if (kind < ALMANAC_SOLAR_ECLIPSE_PARTIAL || kind > ALMANAC_SOLAR_ECLIPSE_TOTAL)
        return "solar";
    return text_by_kind[kind];
}

static const char *lunar_kind_text(almanac_lunar_eclipse_kind_t kind)
{
    static const char *const text_by_kind[ALMANAC_LUNAR_ECLIPSE_TOTAL + 1] = {
        [ALMANAC_LUNAR_ECLIPSE_PENUMBRAL] = "penumbral",
        [ALMANAC_LUNAR_ECLIPSE_PARTIAL] = "partial",
        [ALMANAC_LUNAR_ECLIPSE_TOTAL] = "total"};

    if (kind < ALMANAC_LUNAR_ECLIPSE_PENUMBRAL || kind > ALMANAC_LUNAR_ECLIPSE_TOTAL)
        return "lunar";
    return text_by_kind[kind];
}

static double degrees_to_radians_local(double degrees)
{
    return degrees * 3.141592653589793238462643383279502884 / 180.0;
}

static double surface_distance_km(double lat1, double lon1, double lat2, double lon2)
{
    static const double EARTH_RADIUS_KM = 6371.0088;
    double phi1 = degrees_to_radians_local(lat1);
    double phi2 = degrees_to_radians_local(lat2);
    double d_phi = degrees_to_radians_local(lat2 - lat1);
    double d_lambda = degrees_to_radians_local(fmod(lon2 - lon1 + 540.0, 360.0) - 180.0);
    double sin_d_phi = sin(d_phi / 2.0);
    double sin_d_lambda = sin(d_lambda / 2.0);
    double a = sin_d_phi * sin_d_phi + cos(phi1) * cos(phi2) * sin_d_lambda * sin_d_lambda;

    if (a < 0.0)
        a = 0.0;
    if (a > 1.0)
        a = 1.0;
    return EARTH_RADIUS_KM * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

static int totality_town_compare(const void *lhs, const void *rhs)
{
    const almanac_totality_town_t *left = lhs;
    const almanac_totality_town_t *right = rhs;

    if (left->distance_km < right->distance_km)
        return -1;
    if (left->distance_km > right->distance_km)
        return 1;
    return strcmp(left->town_name, right->town_name);
}

static void copy_text_field(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0u)
        return;
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static bool append_totality_town(almanac_totality_town_t **towns, size_t *count, size_t *capacity,
                                 const almanac_totality_town_t *town)
{
    almanac_totality_town_t *next;
    size_t next_capacity;

    if (!towns || !count || !capacity || !town)
        return false;
    if (*count >= *capacity) {
        next_capacity = *capacity ? *capacity * 2u : 256u;
        next = realloc(*towns, next_capacity * sizeof(**towns));
        if (!next)
            return false;
        *towns = next;
        *capacity = next_capacity;
    }
    (*towns)[*count] = *town;
    *count += 1u;
    return true;
}

typedef struct almanac_totality_towns_t {
    const almanac_observer_t *observer;
    almanac_totality_town_t *towns;
    size_t count;
    size_t capacity;
    bool failed;
} almanac_totality_towns_t;

static bool collect_totality_town(const jurisdict_place_t *place, void *context)
{
    almanac_totality_towns_t *collection = context;
    almanac_totality_town_t town = {0};
    if (!place->jurisdiction_code || !place->name ||
        !parse_double_text(place->latitude, &town.latitude_degrees) ||
        !parse_double_text(place->longitude, &town.longitude_degrees) ||
        !parse_double_text(place->elevation, &town.elevation_metres) ||
        !isfinite(town.latitude_degrees) || !isfinite(town.longitude_degrees) || !isfinite(town.elevation_metres) ||
        fabs(town.latitude_degrees) > 90 || fabs(town.longitude_degrees) > 180 ||
        town.elevation_metres < -500 || town.elevation_metres > 9000)
        return true;
    copy_text_field(town.jurisdiction_id, sizeof(town.jurisdiction_id), place->jurisdiction_code);
    copy_text_field(town.town_name, sizeof(town.town_name), place->name);
    copy_text_field(town.timezone_name, sizeof(town.timezone_name), place->timezone);
    town.distance_km = surface_distance_km(collection->observer->latitude_degrees,
                                           collection->observer->longitude_degrees,
                                           town.latitude_degrees, town.longitude_degrees);
    collection->failed = !append_totality_town(&collection->towns, &collection->count, &collection->capacity, &town);
    return !collection->failed;
}

static bool load_totality_towns(const almanac_observer_t *observer, almanac_totality_town_t **out, size_t *count)
{
    almanac_totality_towns_t collection = {.observer = observer};
    jurisdiction_t *catalogue = jurisdict_open(NULL);
    bool ok = catalogue && jurisdict_each_town(catalogue, collect_totality_town, &collection) && !collection.failed;
    jurisdict_close(catalogue);
    *out = NULL;
    *count = 0;
    if (!ok) {
        free(collection.towns);
        return false;
    }
    /* Enumerate once, then score the bounded nearest-town shortlist in distance order. */
    if (collection.count > 1)
        qsort(collection.towns, collection.count, sizeof(*collection.towns), totality_town_compare);
    *out = collection.towns;
    *count = collection.count;
    return true;
}

static bool town_seeds_total_solar_eclipse(almanac_t *almanac, const almanac_observer_t *origin,
                                           const almanac_solar_eclipse_t *seed, const almanac_totality_town_t *town,
                                           almanac_solar_totality_location_t *out)
{
    almanac_observer_t observer;

    if (!almanac || !origin || !seed || !town || !out)
        return false;
    observer.latitude_degrees = town->latitude_degrees;
    observer.longitude_degrees = town->longitude_degrees;
    observer.elevation_metres = town->elevation_metres;
    return almanac_solar_eclipse_totality_from_seed(almanac, origin, &observer, seed, 1.0, out);
}

static bool nearest_totality_town_text(char *out, size_t out_size, almanac_t *almanac,
                                       const almanac_observer_t *observer, const almanac_solar_eclipse_t *event)
{
    almanac_totality_town_t *towns = NULL;
    size_t town_count = 0u;
    size_t score_limit;
    size_t i;
    const almanac_totality_town_t *best_town = NULL;
    almanac_solar_totality_location_t best_location;
    bool found = false;

    if (!out || out_size == 0u)
        return false;
    out[0] = '\0';
    if (!almanac || !observer || !event)
        return false;
    if (!load_totality_towns(observer, &towns, &town_count))
        return false;
    score_limit = town_count < TOTALITY_TOWN_SCORE_LIMIT ? town_count : TOTALITY_TOWN_SCORE_LIMIT;
    memset(&best_location, 0, sizeof(best_location));
    for (i = 0u; i < score_limit; ++i) {
        almanac_observer_t town_observer;
        almanac_solar_totality_location_t candidate;
        double score_degrees = NAN;

        town_observer.latitude_degrees = towns[i].latitude_degrees;
        town_observer.longitude_degrees = towns[i].longitude_degrees;
        town_observer.elevation_metres = towns[i].elevation_metres;
        if (!almanac_solar_eclipse_totality_seed_score(almanac, &town_observer, event, &score_degrees) ||
            !(score_degrees <= 0.0)) {
            continue;
        }
        memset(&candidate, 0, sizeof(candidate));
        if (!town_seeds_total_solar_eclipse(almanac, observer, event, &towns[i], &candidate))
            continue;
        best_location = candidate;
        best_town = &towns[i];
        found = true;
        break;
    }
    if (found && best_town) {
        snprintf(out, out_size, "town\t%s\t%s\t%s\t%.6f\t%.6f\t%.9f\t%.1f\t%.0f", best_town->town_name,
                 best_town->jurisdiction_id, best_town->timezone_name, best_location.latitude_degrees,
                 best_location.longitude_degrees, best_location.greatest_eclipse.jd, best_location.distance_km,
                 best_town->elevation_metres);
    }
    free(towns);
    return true;
}

static bool append_solar_events(string_t *out, almanac_t *almanac, const almanac_observer_t *observer,
                                bool totality_land, const array_t *events)
{
    size_t i;

    if (!out)
        return false;
    for (i = 0u; events && i < array_size(events); ++i) {
        const almanac_solar_eclipse_t *event = array_get(events, i);
        almanac_event_time_t greatest;
        almanac_event_time_t first;
        almanac_event_time_t fourth;
        char nearest_totality_text[512];

        if (!event)
            continue;
        (void)almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest);
        (void)almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FIRST_CONTACT, &first);
        (void)almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FOURTH_CONTACT, &fourth);
        nearest_totality_text[0] = '\0';
        if (totality_land && !nearest_totality_town_text(nearest_totality_text, sizeof(nearest_totality_text),
                                                         almanac, observer, event)) {
            fprintf(stderr, "Land-totality town catalogue is unavailable; cannot search for named locations\n");
            return false;
        }
        (void)string_append_format(out, "event Solar|Solar eclipse|%s|%.9f|%.9f|%.9f|%.6f|%.3f|%s|"
                                       "%.12f|%.12f|%.12f|0|%.9f\n",
                                   solar_kind_text(almanac_solar_eclipse_kind(event)), greatest.jd, first.jd, fourth.jd,
                                   almanac_solar_eclipse_magnitude(event),
                                   almanac_solar_eclipse_totality_percent(event), nearest_totality_text,
                                   almanac_solar_eclipse_sun_semi_diameter_degrees(event),
                                   almanac_solar_eclipse_moon_semi_diameter_degrees(event),
                                   almanac_solar_eclipse_separation_degrees(event),
                                   almanac_solar_eclipse_orientation_degrees(event));
    }
    return true;
}

static void append_lunar_events(string_t *out, const array_t *events)
{
    size_t i;

    if (!out)
        return;
    for (i = 0u; events && i < array_size(events); ++i) {
        const almanac_lunar_eclipse_t *event = array_get(events, i);
        almanac_event_time_t greatest;
        almanac_event_time_t p1;
        almanac_event_time_t p4;

        if (!event)
            continue;
        (void)almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest);
        (void)almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_P1_CONTACT, &p1);
        (void)almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_P4_CONTACT, &p4);
        (void)string_append_format(out, "event Lunar|Lunar eclipse|%s|%.9f|%.9f|%.9f|%.6f|%.3f||"
                                       "%.12f|%.12f|%.12f|%.12f|%.9f\n",
                                   lunar_kind_text(almanac_lunar_eclipse_kind(event)), greatest.jd, p1.jd, p4.jd,
                                   almanac_lunar_eclipse_umbral_magnitude(event),
                                   almanac_lunar_eclipse_totality_percent(event),
                                   almanac_lunar_eclipse_moon_semi_diameter_degrees(event),
                                   almanac_lunar_eclipse_umbral_radius_degrees(event),
                                   almanac_lunar_eclipse_opposition_error_degrees(event),
                                   almanac_lunar_eclipse_penumbral_radius_degrees(event),
                                   almanac_lunar_eclipse_orientation_degrees(event));
    }
}

/* Execute this built-in calculation mode in a fresh server child process. */
int lab_worker_almanac_event(int argc, char **argv)
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
        fprintf(stderr, "usage: %s start=YYYY-MM-DD end=YYYY-MM-DD lat=52.7073 lon=-2.7540 kind=all|solar|lunar\n",
                argc > 0 ? argv[0] : "almanac_event_lab");
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

    if (options.kind != ALMANAC_EVENT_LAB_KIND_LUNAR) {
        solar_events = almanac_find_solar_eclipses(almanac, &observer, start, end);
        if (!solar_events)
            goto done;
    }
    if (options.kind != ALMANAC_EVENT_LAB_KIND_SOLAR) {
        lunar_events = almanac_find_lunar_eclipses(almanac, &observer, start, end);
        if (!lunar_events)
            goto done;
    }

    if (!append_solar_events(output, almanac, &observer, options.totality_land, solar_events))
        goto done;
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
