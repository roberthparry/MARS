#include <stdbool.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "almanac.h"
#define MARS_ALMANAC_INTERNAL_ACCESS
#include "almanac/almanac_internal.h"
#include "sqlite.h"

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

#define ALMANAC_EVENT_LAB_CACHE_SCHEMA "almanac_event_lab_output_v19"
#define ALMANAC_EVENT_LAB_CACHE_PATH_ENV "MARS_LAB_OBJECT_STORE_PATH"
#define ALMANAC_EVENT_LAB_CACHE_KEY_ENV "MARS_LAB_OBJECT_STORE_KEY"
#define JURISDICTION_DB_PATH_ENV "MARS_JURISDICTION_DB_PATH"
#define JURISDICTION_DB_KEY_ENV "MARS_JURISDICTION_DB_KEY"
#define LEGACY_HOLIDAY_DB_PATH_ENV "MARS_HOLIDAY_DB_PATH"
#define LEGACY_HOLIDAY_DB_KEY_ENV "MARS_HOLIDAY_DB_KEY"
#define TOTALITY_TOWN_SCORE_LIMIT 256u
#define TOTALITY_TOWN_REFINE_LIMIT 16u
#define TOTALITY_TOWN_DEFAULT_SEED_PRIORITY 1000

typedef struct almanac_totality_town_t {
    char jurisdiction_id[32];
    char town_name[96];
    char timezone_name[96];
    double latitude_degrees;
    double longitude_degrees;
    double elevation_metres;
    double distance_km;
    double path_distance_km;
    double seed_score_degrees;
    int seed_priority;
    bool seed_score_valid;
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

static bool parse_int_text(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (!text || !out)
        return false;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 0 || value > 1000000L)
        return false;
    *out = (int)value;
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

static const char *first_nonempty_env(const char *primary, const char *fallback)
{
    const char *value = getenv(primary);

    if (value && *value)
        return value;
    value = getenv(fallback);
    return value && *value ? value : NULL;
}

static sqlite_t *open_jurisdiction_db(void)
{
    const char *path = first_nonempty_env(JURISDICTION_DB_PATH_ENV, LEGACY_HOLIDAY_DB_PATH_ENV);
    const char *key = first_nonempty_env(JURISDICTION_DB_KEY_ENV, LEGACY_HOLIDAY_DB_KEY_ENV);
    string_t *path_text = NULL;
    string_t *key_text = NULL;
    sqlite_t *db = NULL;

    if (!path || !key)
        return NULL;
    path_text = string_new_with(path);
    key_text = string_new_with(key);
    if (!path_text || !key_text)
        goto done;
    db = sqlite_open_encrypted(path_text, key_text);

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
    kind = options->kind == ALMANAC_EVENT_LAB_KIND_SOLAR
        ? "solar"
        : options->kind == ALMANAC_EVENT_LAB_KIND_LUNAR ? "lunar" : "all";
    if (snprintf(key,
                 sizeof(key),
                 "mars_lab/%s/start=%04d-%02d-%02d/end=%04d-%02d-%02d/lat=%.9f/lon=%.9f/totality=%s/kind=%s",
                 ALMANAC_EVENT_LAB_CACHE_SCHEMA,
                 options->start_year,
                 (int)options->start_month,
                 (int)options->start_day,
                 options->end_year,
                 (int)options->end_month,
                 (int)options->end_day,
                 options->latitude,
                 options->longitude,
                 options->totality_land ? "land" : "none",
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
    return options->latitude >= -90.0 && options->latitude <= 90.0 &&
           options->longitude >= -180.0 && options->longitude <= 180.0;
}

static const char *solar_kind_text(almanac_solar_eclipse_kind_t kind)
{
    static const char *const text_by_kind[ALMANAC_SOLAR_ECLIPSE_TOTAL + 1] = {
        [ALMANAC_SOLAR_ECLIPSE_PARTIAL] = "partial",
        [ALMANAC_SOLAR_ECLIPSE_ANNULAR] = "annular",
        [ALMANAC_SOLAR_ECLIPSE_TOTAL] = "total"
    };

    if (kind < ALMANAC_SOLAR_ECLIPSE_PARTIAL || kind > ALMANAC_SOLAR_ECLIPSE_TOTAL)
        return "solar";
    return text_by_kind[kind];
}

static const char *lunar_kind_text(almanac_lunar_eclipse_kind_t kind)
{
    static const char *const text_by_kind[ALMANAC_LUNAR_ECLIPSE_TOTAL + 1] = {
        [ALMANAC_LUNAR_ECLIPSE_PENUMBRAL] = "penumbral",
        [ALMANAC_LUNAR_ECLIPSE_PARTIAL] = "partial",
        [ALMANAC_LUNAR_ECLIPSE_TOTAL] = "total"
    };

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

    if (left->seed_priority < right->seed_priority)
        return -1;
    if (left->seed_priority > right->seed_priority)
        return 1;
    if (left->path_distance_km < right->path_distance_km)
        return -1;
    if (left->path_distance_km > right->path_distance_km)
        return 1;
    if (left->distance_km < right->distance_km)
        return -1;
    if (left->distance_km > right->distance_km)
        return 1;
    return strcmp(left->town_name, right->town_name);
}

static int totality_town_seed_score_compare(const void *lhs, const void *rhs)
{
    const almanac_totality_town_t *left = lhs;
    const almanac_totality_town_t *right = rhs;

    if (left->seed_priority < right->seed_priority)
        return -1;
    if (left->seed_priority > right->seed_priority)
        return 1;
    if (left->seed_score_valid && !right->seed_score_valid)
        return -1;
    if (!left->seed_score_valid && right->seed_score_valid)
        return 1;
    if (left->seed_score_valid && right->seed_score_valid) {
        if (left->seed_score_degrees < right->seed_score_degrees)
            return -1;
        if (left->seed_score_degrees > right->seed_score_degrees)
            return 1;
    }
    return totality_town_compare(lhs, rhs);
}

static int totality_town_seed_priority_fallback(const char *jurisdiction_id, const char *town_name)
{
    static const struct {
        const char *jurisdiction_id;
        const char *town_name;
        int seed_priority;
    } seeds[] = {
        {"ES", "Oviedo", 0},
        {"ES", "Madrid", 1},
        {"ES", "Santander", 2},
        {"ES", "Bilbao", 3},
        {"ES", "Burgos", 4},
        {"ES", "Zaragoza", 5},
        {"ES", "Valencia", 6},
        {"GI", "Gibraltar", 7},
        {"MA", "Tangier", 8},
        {"MA", "Rabat", 9},
        {"TN", "Tunis", 10},
        {"LY", "Tripoli", 11},
        {"EG", "Alexandria", 12},
        {"EG", "Cairo", 13},
        {"IS", "Reykjav\303\255k", 14},
        {"GL", "Nuuk", 15},
        {"PT", "Lisbon", 16},
        {"FR", "Paris", 17}
    };
    size_t i;

    if (!jurisdiction_id || !town_name)
        return TOTALITY_TOWN_DEFAULT_SEED_PRIORITY;
    for (i = 0u; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        if (strcmp(jurisdiction_id, seeds[i].jurisdiction_id) == 0 &&
            strcmp(town_name, seeds[i].town_name) == 0) {
            return seeds[i].seed_priority;
        }
    }
    return TOTALITY_TOWN_DEFAULT_SEED_PRIORITY;
}

static void copy_text_field(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0u)
        return;
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static bool append_totality_town(almanac_totality_town_t **towns,
                                 size_t *count,
                                 size_t *capacity,
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

static almanac_totality_town_t *load_totality_towns(const almanac_observer_t *observer,
                                                    const almanac_solar_totality_location_t *path_reference,
                                                    size_t *out_count)
{
    static const char *sql_with_seed_priority =
        "select town_jurisdiction.jurisdiction_id, "
        "       town_name.town_name, "
        "       town_latitude.latitude, "
        "       town_longitude.longitude, "
        "       coalesce(timezone_canonical.canonical_timezone_name, timezone_code.timezone_name, ''), "
        "       coalesce(town_elevation.elevation_metres, 0), "
        "       coalesce(totality_seed.totality_seed_priority, 1000) "
        "from jurisdiction_town_entity as town "
        "join jurisdiction_town_jurisdiction_id as town_jurisdiction "
        "  on town_jurisdiction.jurisdiction_town_id = town.jurisdiction_town_id "
        "join jurisdiction_town_name as town_name "
        "  on town_name.jurisdiction_town_id = town.jurisdiction_town_id "
        "join jurisdiction_town_latitude as town_latitude "
        "  on town_latitude.jurisdiction_town_id = town.jurisdiction_town_id "
        "join jurisdiction_town_longitude as town_longitude "
        "  on town_longitude.jurisdiction_town_id = town.jurisdiction_town_id "
        "left join jurisdiction_town_elevation as town_elevation "
        "  on town_elevation.jurisdiction_town_id = town.jurisdiction_town_id "
        "left join jurisdiction_town_timezone as town_timezone "
        "  on town_timezone.jurisdiction_town_id = town.jurisdiction_town_id "
        "left join timezone_code "
        "  on timezone_code.timezone_code = town_timezone.timezone_code "
        "left join timezone_canonical "
        "  on timezone_canonical.timezone_name = timezone_code.timezone_name "
        "left join jurisdiction_town_totality_seed_priority as totality_seed "
        "  on totality_seed.jurisdiction_town_id = town.jurisdiction_town_id";
    static const char *sql_without_seed_priority =
        "select town_jurisdiction.jurisdiction_id, "
        "       town_name.town_name, "
        "       town_latitude.latitude, "
        "       town_longitude.longitude, "
        "       coalesce(timezone_canonical.canonical_timezone_name, timezone_code.timezone_name, ''), "
        "       coalesce(town_elevation.elevation_metres, 0) "
        "from jurisdiction_town_entity as town "
        "join jurisdiction_town_jurisdiction_id as town_jurisdiction "
        "  on town_jurisdiction.jurisdiction_town_id = town.jurisdiction_town_id "
        "join jurisdiction_town_name as town_name "
        "  on town_name.jurisdiction_town_id = town.jurisdiction_town_id "
        "join jurisdiction_town_latitude as town_latitude "
        "  on town_latitude.jurisdiction_town_id = town.jurisdiction_town_id "
        "join jurisdiction_town_longitude as town_longitude "
        "  on town_longitude.jurisdiction_town_id = town.jurisdiction_town_id "
        "left join jurisdiction_town_elevation as town_elevation "
        "  on town_elevation.jurisdiction_town_id = town.jurisdiction_town_id "
        "left join jurisdiction_town_timezone as town_timezone "
        "  on town_timezone.jurisdiction_town_id = town.jurisdiction_town_id "
        "left join timezone_code "
        "  on timezone_code.timezone_code = town_timezone.timezone_code "
        "left join timezone_canonical "
        "  on timezone_canonical.timezone_name = timezone_code.timezone_name";
    sqlite_t *db = NULL;
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    almanac_totality_town_t *towns = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    bool has_seed_priority = true;

    if (out_count)
        *out_count = 0u;
    if (!observer || !out_count)
        return NULL;
    db = open_jurisdiction_db();
    if (!db)
        return NULL;
    stmt = sqlite_stmt_prepare(db, sql_with_seed_priority);
    if (!stmt) {
        has_seed_priority = false;
        stmt = sqlite_stmt_prepare(db, sql_without_seed_priority);
        if (!stmt)
            goto done;
    }
    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        almanac_totality_town_t town;
        const char *jurisdiction_id = sqlite_stmt_column_text(stmt, 0);
        const char *town_name = sqlite_stmt_column_text(stmt, 1);
        const char *latitude_text = sqlite_stmt_column_text(stmt, 2);
        const char *longitude_text = sqlite_stmt_column_text(stmt, 3);
        const char *timezone_name = sqlite_stmt_column_text(stmt, 4);
        const char *elevation_text = sqlite_stmt_column_text(stmt, 5);
        const char *priority_text = has_seed_priority ? sqlite_stmt_column_text(stmt, 6) : NULL;
        double latitude;
        double longitude;
        double elevation = 0.0;
        int seed_priority = totality_town_seed_priority_fallback(jurisdiction_id, town_name);

        if (!jurisdiction_id || !town_name || !latitude_text || !longitude_text)
            continue;
        if (!parse_double_text(latitude_text, &latitude) ||
            !parse_double_text(longitude_text, &longitude) ||
            (elevation_text && !parse_double_text(elevation_text, &elevation)) ||
            latitude < -90.0 || latitude > 90.0 ||
            longitude < -180.0 || longitude > 180.0 ||
            elevation < -500.0 || elevation > 9000.0) {
            continue;
        }
        memset(&town, 0, sizeof(town));
        copy_text_field(town.jurisdiction_id, sizeof(town.jurisdiction_id), jurisdiction_id);
        copy_text_field(town.town_name, sizeof(town.town_name), town_name);
        copy_text_field(town.timezone_name, sizeof(town.timezone_name), timezone_name);
        town.latitude_degrees = latitude;
        town.longitude_degrees = longitude;
        town.elevation_metres = elevation;
        if (priority_text)
            (void)parse_int_text(priority_text, &seed_priority);
        town.seed_priority = seed_priority;
        town.seed_score_degrees = NAN;
        town.seed_score_valid = false;
        town.distance_km = surface_distance_km(observer->latitude_degrees,
                                               observer->longitude_degrees,
                                               latitude,
                                               longitude);
        if (path_reference && path_reference->found) {
            town.path_distance_km = surface_distance_km(path_reference->latitude_degrees,
                                                        path_reference->longitude_degrees,
                                                        latitude,
                                                        longitude);
        } else {
            town.path_distance_km = town.distance_km;
        }
        if (!append_totality_town(&towns, &count, &capacity, &town))
            goto done;
    }
    qsort(towns, count, sizeof(*towns), totality_town_compare);
    *out_count = count;

done:
    sqlite_stmt_finalize(stmt);
    sqlite_close(db);
    return towns;
}

static bool town_seeds_total_solar_eclipse(almanac_t *almanac,
                                           const almanac_observer_t *origin,
                                           const almanac_solar_eclipse_t *seed,
                                           const almanac_totality_town_t *town,
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

static bool nearest_totality_town_text(char *out,
                                       size_t out_size,
                                       almanac_t *almanac,
                                       const almanac_observer_t *observer,
                                       const almanac_solar_eclipse_t *event,
                                       const almanac_solar_totality_location_t *path_reference)
{
    almanac_totality_town_t *towns = NULL;
    size_t town_count = 0u;
    size_t score_limit;
    size_t refine_count = 0u;
    size_t i;
    const almanac_totality_town_t *best_town = NULL;
    almanac_solar_totality_location_t best_location;
    bool found = false;

    if (!out || out_size == 0u)
        return false;
    out[0] = '\0';
    if (!almanac || !observer || !event)
        return false;
    towns = load_totality_towns(observer, path_reference, &town_count);
    if (!towns || town_count == 0u)
        return false;
    score_limit = town_count < TOTALITY_TOWN_SCORE_LIMIT ? town_count : TOTALITY_TOWN_SCORE_LIMIT;
    for (i = 0u; i < score_limit; ++i) {
        almanac_observer_t town_observer;
        double score_degrees = NAN;

        town_observer.latitude_degrees = towns[i].latitude_degrees;
        town_observer.longitude_degrees = towns[i].longitude_degrees;
        town_observer.elevation_metres = towns[i].elevation_metres;
        towns[i].seed_score_valid =
            almanac_solar_eclipse_totality_seed_score(almanac,
                                                      &town_observer,
                                                      event,
                                                      &score_degrees) &&
            score_degrees == score_degrees &&
            score_degrees <= 1.0;
        towns[i].seed_score_degrees = score_degrees;
    }
    qsort(towns, score_limit, sizeof(*towns), totality_town_seed_score_compare);

    memset(&best_location, 0, sizeof(best_location));
    for (i = 0u; i < score_limit && refine_count < TOTALITY_TOWN_REFINE_LIMIT; ++i) {
        almanac_solar_totality_location_t candidate;

        memset(&candidate, 0, sizeof(candidate));
        if (!towns[i].seed_score_valid)
            break;
        if (found && towns[i].distance_km > best_location.distance_km + 50.0)
            break;
        refine_count += 1u;
        if (!town_seeds_total_solar_eclipse(almanac, observer, event, &towns[i], &candidate))
            continue;
        if (!found || candidate.distance_km < best_location.distance_km) {
            best_location = candidate;
            best_town = &towns[i];
            found = true;
        }
    }
    if (found && best_town) {
        snprintf(out,
                 out_size,
                 "town\t%s\t%s\t%s\t%.6f\t%.6f\t%.9f\t%.1f\t%.0f",
                 best_town->town_name,
                 best_town->jurisdiction_id,
                 best_town->timezone_name,
                 best_location.latitude_degrees,
                 best_location.longitude_degrees,
                 best_location.greatest_eclipse.jd,
                 best_location.distance_km,
                 best_town->elevation_metres);
    }
    free(towns);
    return found;
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
        almanac_event_time_t greatest;
        almanac_event_time_t first;
        almanac_event_time_t fourth;
        char nearest_totality_text[512];
        bool have_named_totality = false;

        if (!event)
            continue;
        (void)almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest);
        (void)almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FIRST_CONTACT, &first);
        (void)almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FOURTH_CONTACT, &fourth);
        nearest_totality_text[0] = '\0';
        if (totality_land) {
            have_named_totality = nearest_totality_town_text(nearest_totality_text,
                                                             sizeof(nearest_totality_text),
                                                             almanac,
                                                             observer,
                                                             event,
                                                             NULL);
        }
        if (totality_land && have_named_totality) {
            /* The town search emits an already named land location. */
        }
        (void)string_append_format(out,
                                   "event Solar|Solar eclipse|%s|%.9f|%.9f|%.9f|%.6f|%.3f|%s\n",
                                   solar_kind_text(almanac_solar_eclipse_kind(event)),
                                   greatest.jd,
                                   first.jd,
                                   fourth.jd,
                                   almanac_solar_eclipse_magnitude(event),
                                   almanac_solar_eclipse_totality_percent(event),
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
        almanac_event_time_t greatest;
        almanac_event_time_t p1;
        almanac_event_time_t p4;

        if (!event)
            continue;
        (void)almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest);
        (void)almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_P1_CONTACT, &p1);
        (void)almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_P4_CONTACT, &p4);
        (void)string_append_format(out,
                                   "event Lunar|Lunar eclipse|%s|%.9f|%.9f|%.9f|%.6f|%.3f\n",
                                   lunar_kind_text(almanac_lunar_eclipse_kind(event)),
                                   greatest.jd,
                                   p1.jd,
                                   p4.jd,
                                   almanac_lunar_eclipse_umbral_magnitude(event),
                                   almanac_lunar_eclipse_totality_percent(event));
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
        fprintf(stderr,
                "usage: %s start=YYYY-MM-DD end=YYYY-MM-DD lat=52.7073 lon=-2.7540 kind=all|solar|lunar\n",
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
