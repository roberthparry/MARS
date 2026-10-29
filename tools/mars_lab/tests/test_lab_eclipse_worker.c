/**
 * @file test_lab_eclipse_worker.c
 * @brief End-to-end native eclipse worker geometry and observer-orientation regression.
 *
 * Uses the configured ephemeris read-only, in an isolated child without a cache
 * or worker override. Checks the solar/lunar wire fields and resulting SVG at
 * greatest eclipse, including differing lunar orientations for two observers.
 * A disposable town catalogue exercises named totality, action transport and
 * catalogue-error reporting without modifying the installed data. A 50,000-town
 * fixture mixes distant misses and dense near misses to check the worker budget.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite-t.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static string_t *ephemeris_home;

static bool eclipse_town_catalogue(const char *directory)
{
    if (!test_lab_catalogue_database(directory))
        return false;
    string_t *path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_t *sql = string_new_with(
        "insert into jurisdiction values('ES',null,'country','Spain');"
        "insert into timezone_code values('Europe/Madrid',3);"
        "insert into jurisdiction_town values(4,'ES','Málaga');"
        /* Use an inland Málaga site; the former port-side coordinate is within the uncertain coastal band. */
        "insert into jurisdiction_town_latitude values(4,'36.7300');"
        "insert into jurisdiction_town_longitude values(4,'-4.4300');"
        "insert into jurisdiction_town_elevation values(4,'22');"
        "insert into jurisdiction_town_timezone values(4,3);");
    bool ok = db && sql && sqlite_exec(db, sql);
    sqlite_close(db);
    string_free(sql);
    string_free(key);
    string_free(path);
    return ok;
}

static bool eclipse_totality_action_roundtrip(const json_t *action)
{
    json_t *options = json_new_object();
    static const char *const worker_keys[] = {"date", "time", "zone", "elevation"};
    for (size_t i = 0; i < sizeof(worker_keys) / sizeof(*worker_keys); ++i)
        lab_cal_set(options, worker_keys[i], test_lab_text(action, worker_keys[i]));
    lab_cal_set(options, "lat", test_lab_text(action, "latitude"));
    lab_cal_set(options, "lon", test_lab_text(action, "longitude"));
    lab_cal_set(options, "body", "SUN");
    string_t *diagnostic = NULL;
    json_t *fields = lab_cal_run("almanac_lab", options, 20000, &diagnostic);
    lab_cal_set(options, "visibility", "all");
    lab_cal_set(options, "timezone", "Europe/Madrid");
    lab_cal_set(options, "jurisdiction", test_lab_text(action, "jurisdiction"));
    lab_cal_set(options, "town", test_lab_text(action, "town"));
    unsigned status = 200;
    json_t *worksheet = fields ? lab_cal_almanac(fields, options, &status) : NULL;
    const json_t *events = test_lab_member(worksheet, "events");
    bool found = false;
    for (size_t i = 0; i < json_array_size(events); ++i) {
        const json_t *event = json_array_get(events, i);
        if (!strcmp(test_lab_text(event, "category"), "Solar") &&
            !strncmp(test_lab_text(event, "gmt_time"), "2027-08-02", 10)) {
            found = !strcmp(test_lab_text(event, "kind"), "total") &&
                    !strcmp(test_lab_text(event, "obscuration"), "100.0%");
        }
    }
    bool ok = status == 200 && found && !strcmp(test_lab_text(action, "date"), "2027-08-02") &&
              !strncmp(test_lab_text(action, "time"), "08:", 3) &&
              !strcmp(test_lab_text(action, "zone"), "2.00");
    if (!ok)
        string_fprintf(stderr, "Totality action round trip: %s; time=%s; eclipse retained=%d\n",
                       diagnostic ? string_c_str(diagnostic) : "unexpected worksheet",
                       test_lab_text(action, "time"), found);
    json_free(worksheet);
    json_free(fields);
    string_free(diagnostic);
    json_free(options);
    return ok;
}

static bool eclipse_totality_response(const char *jd, const json_t *request)
{
    json_t *payload = test_lab_json("{\"event_year\":\"2027\",\"events\":[]}");
    json_t *entry = json_new_object(), *options = json_clone(request);
    lab_cal_set(entry, "jd", jd);
    json_array_append((json_t *)test_lab_member(payload, "events"), entry);
    lab_cal_set(options, "date", "2026-10-13");
    lab_cal_set(options, "zone", "0");
    lab_cal_set(options, "jurisdiction", "GB-ENG");
    lab_cal_set(options, "timezone", "Europe/London");
    unsigned status = 200;
    json_t *response = lab_cal_totality(payload, options, &status);
    const json_t *items = test_lab_member(response, "items"), *item = json_array_get(items, 0);
    bool ok = status == 200 && test_lab_ok(response, true) && json_array_size(items) == 1 &&
              !strcmp(test_lab_text(item, "jd"), jd) && strstr(test_lab_text(item, "nearest_totality"), "Málaga") &&
              strstr(test_lab_text(item, "nearest_totality"), "GMT+2") &&
              strstr(test_lab_text(item, "html"), "data-almanac-use-totality") &&
              strstr(test_lab_text(test_lab_member(item, "nearest_totality_action"), "town"), "Málaga|") &&
              eclipse_totality_action_roundtrip(test_lab_member(item, "nearest_totality_action"));
    json_free(response);
    json_free(options);
    json_free(entry);
    json_free(payload);
    return ok;
}

static bool eclipse_worksheet_response(void)
{
    json_t *options = test_lab_json(
        "{\"date\":\"2026-10-13\",\"time\":\"20:00:00\",\"zone\":\"1\",\"lat\":\"52.7073\",\"lon\":\"-2.7540\","
        "\"body\":\"MOON\"}");
    string_t *diagnostic = NULL;
    json_t *fields = lab_cal_run("almanac_lab", options, 20000, &diagnostic);
    lab_cal_set(options, "jurisdiction", "GB-ENG");
    lab_cal_set(options, "timezone", "Europe/London");
    lab_cal_set(options, "visibility", "all");
    lab_cal_set(options, "elevation", "2");
    lab_cal_set(options, "sea_horizon", "true");
    lab_cal_set(options, "atmosphere_note", "Temperature: 10 °C; atmospheric pressure: 1013 hPa");
    unsigned status = 200;
    json_t *response = fields ? lab_cal_almanac(fields, options, &status) : NULL;
    const json_t *metadata = test_lab_member(response, "almanac_presentation");
    const char *html = test_lab_text(test_lab_member(metadata, "all"), "html");
    bool ok = response && status == 200 && test_lab_ok(response, true) &&
              strstr(html, "almanac-moon-phase") && strstr(html, "Moon: 10.") &&
              strstr(html, "0.0% umbral") && strstr(html, "excludes penumbral dimming") &&
              strstr(html, "Umbral magnitude; negative means") &&
              strstr(html, "<dt>Observer altitude</dt><dd>2 m · Sea</dd>") &&
              strstr(html, "height, horizon dip and refraction");
    if (!ok)
        string_fprintf(stderr, "Eclipse worksheet regression: %s\n", diagnostic ? string_c_str(diagnostic) : "bad markup");
    json_free(response);
    lab_cal_set(options, "lat", "-12.999999");
    lab_cal_set(options, "lon", "179.999999");
    response = fields ? lab_cal_almanac(fields, options, &status) : NULL;
    const json_t *summary = test_lab_member(response, "observer_summary");
    ok = json_array_size(summary) == 9 &&
         !strcmp(test_lab_text(json_array_get(summary, 3), "value"), "13° 00.00′ S") &&
         !strcmp(test_lab_text(json_array_get(summary, 4), "value"), "180° 00.00′ E") && ok;
    json_free(response);
    lab_cal_set(options, "defer_events", "true");
    response = fields ? lab_cal_almanac(fields, options, &status) : NULL;
    const json_t *preview = test_lab_member(test_lab_member(response, "almanac_presentation"), "all");
    ok = ok && test_lab_ok(response, true) && json_array_size(test_lab_member(response, "observer_summary")) == 9 &&
         json_array_size(test_lab_member(response, "all_rows")) > 0 &&
         json_array_size(test_lab_member(response, "events")) == 0 &&
         !strcmp(test_lab_text(response, "events_notice"), "Calculating upcoming events…") &&
         strstr(test_lab_text(preview, "html"), "Calculating upcoming events…") &&
         strstr(test_lab_text(preview, "copy_text"), "Calculating upcoming events…") &&
         !strstr(test_lab_text(preview, "copy_text"), "No events found.");
    json_free(response);
    json_free(fields);
    string_free(diagnostic);
    json_free(options);
    return ok;
}

static bool almanac_observer_worker(void)
{
    json_t *options = test_lab_json(
        "{\"date\":\"2026-08-12\",\"time\":\"17:47:05\",\"lat\":\"52.7073\",\"lon\":\"-2.7540\",\"body\":\"MOON\"}");
    double sea_altitude = 0, raised_altitude = 0, refracted_altitude = 0, horizon_altitude = 0;
    bool ok = true;
    for (unsigned i = 0; i < 4; ++i) {
        if (i == 1)
            lab_cal_set(options, "elevation", "3000");
        if (i == 2) {
            lab_cal_set(options, "temperature_c", "5");
            lab_cal_set(options, "pressure_hpa", "700");
        }
        if (i == 3)
            lab_cal_set(options, "sea_horizon", "true");
        string_t *diagnostic = NULL;
        json_t *fields = lab_cal_run("almanac_lab", options, 20000, &diagnostic);
        double *altitudes[] = {&sea_altitude, &raised_altitude, &refracted_altitude, &horizon_altitude};
        double *altitude = altitudes[i];
        ok = lab_cal_number(lab_cal_text(fields, "selected_altitude"), altitude) && ok;
        if (i == 2) {
            double geometric = 0;
            ok = lab_cal_number(lab_cal_text(fields, "selected_geometric_altitude"), &geometric) &&
                 fabs(geometric - raised_altitude) < 1e-8 && ok;
        }
        json_free(fields);
        string_free(diagnostic);
    }
    json_free(options);
    double dip = acos(6371008.8 / (6371008.8 + 3000)) * 180 / M_PI;
    return ok && fabs(sea_altitude - raised_altitude) > .0001 && refracted_altitude > raised_altitude &&
           fabs(horizon_altitude - refracted_altitude - dip) < 1e-8;
}

static bool almanac_location_modes(void)
{
    /* Historical date predates weather coverage, keeping this integration check offline. */
    json_t *payload = test_lab_json(
        "{\"date\":\"2000-01-01\",\"time\":\"12:00:00\",\"jurisdiction\":\"GB-ENG\","
        "\"latitude\":\"52.707700\",\"longitude\":\"-2.754100\",\"elevation\":\"2\",\"town\":\"Fixture\"}");
    string_t *route = string_new_with("/almanac-eval");
    double altitudes[3] = {0};
    bool ok = true;
    for (unsigned i = 0; i < 3; ++i) {
        if (i == 1)
            lab_cal_set(payload, "town", "");
        if (i == 2) {
            lab_cal_set(payload, "latitude", "0");
            lab_cal_set(payload, "longitude", "0");
        }
        unsigned status = 0;
        json_t *response = lab_cal_request(route, payload, &status);
        ok = status == 200 &&
             lab_cal_number(lab_cal_text(test_lab_member(response, "fields"), "selected_altitude"), &altitudes[i]) &&
             !strcmp(lab_cal_text(response, "observer_surface"), i == 2 ? "Sea" : "Land") &&
             strstr(lab_cal_text(response, "altitude_note"), i == 2 ? "Sea mode." : "No horizon dip.") && ok;
        const json_t *summary = test_lab_member(response, "observer_summary");
        ok = json_array_size(summary) >= 9 && json_array_size(summary) <= 10 &&
             !strcmp(test_lab_text(json_array_get(summary, 3), "value"), i == 2 ? "0° 00.00′ N" : "52° 42.46′ N") &&
             !strcmp(test_lab_text(json_array_get(summary, 4), "value"), i == 2 ? "0° 00.00′ E" : "2° 45.25′ W") &&
             !strcmp(test_lab_text(json_array_get(summary, 8), "value"),
                     "Rough seasonal assumption\nestimated pressure (standard atmosphere)") &&
             !strcmp(lab_cal_text(response, "observer_latitude"), i == 2 ? "0" : "52.707700") && ok;
        json_free(response);
    }
    string_free(route);
    route = string_new_with("/almanac-weather");
    unsigned status = 0;
    json_t *refresh = lab_cal_request(route, payload, &status);
    double refreshed_altitude = 0;
    bool background = false;
    ok = status == 200 && json_bool_value(test_lab_member(refresh, "weather_refresh"), &background) && background &&
         lab_cal_number(lab_cal_text(test_lab_member(refresh, "fields"), "selected_altitude"), &refreshed_altitude) &&
         fabs(refreshed_altitude - altitudes[2]) < 1e-8 && ok;
    json_free(refresh);
    json_free(payload);
    string_free(route);
    return ok && fabs(altitudes[1] - altitudes[0]) < 1e-8;
}

static bool eclipse_worker_fixture(const char *directory)
{
    bool ok = !(ephemeris_home ? setenv("MARS_HOME", string_c_str(ephemeris_home), 1) : unsetenv("MARS_HOME")) &&
              !unsetenv("MARS_LAB_OBJECT_STORE_PATH") && !unsetenv("MARS_LAB_OBJECT_STORE_KEY") &&
              !unsetenv("MARS_LAB_ALMANAC_EVENT_BINARY") && !unsetenv("MARS_LAB_ALMANAC_BINARY") &&
              eclipse_town_catalogue(directory);
    static const char *const requests[] = {
        "{\"start\":\"2026-08-10\",\"end\":\"2026-08-15\",\"lat\":\"51.5074\",\"lon\":\"-0.1278\",\"kind\":\"solar\"}",
        "{\"start\":\"2025-03-13\",\"end\":\"2025-03-15\",\"lat\":\"40.7128\",\"lon\":\"-74.006\",\"kind\":\"lunar\"}",
        "{\"start\":\"2025-03-13\",\"end\":\"2025-03-15\",\"lat\":\"-33.4489\",\"lon\":\"-70.6693\",\"kind\":\"lunar\"}",
        "{\"start\":\"2027-08-01\",\"end\":\"2027-08-04\",\"lat\":\"51.5074\",\"lon\":\"-0.1278\","
        "\"kind\":\"solar\",\"totality\":\"land\"}",
        "{\"start\":\"2027-08-01\",\"end\":\"2027-08-04\",\"lat\":\"52.7073\",\"lon\":\"-2.7540\","
        "\"kind\":\"solar\",\"totality\":\"land\"}",
        "{\"start\":\"2027-02-20\",\"end\":\"2027-02-21\",\"lat\":\"52.7073\",\"lon\":\"-2.7540\",\"kind\":\"lunar\"}"
    };
    static const char *const geometry[] = {
        "disc_radius", "cover_radius", "centre_separation", "penumbra_radius", "orientation_degrees"
    };
    double bearings[sizeof(requests) / sizeof(*requests)] = {0};
    for (size_t i = 0; ok && i < sizeof(requests) / sizeof(*requests); ++i) {
        json_t *options = test_lab_json(requests[i]);
        string_t *diagnostic = NULL;
        json_t *fields = lab_cal_run("almanac_event_lab", options, 20000, &diagnostic);
        string_t *events = string_new_with(lab_cal_text(fields, "event"));
        size_t line_count = 0, column_count = 0;
        string_t **lines = lab_cal_split(events, "\n", &line_count);
        string_t **columns = line_count ? lab_cal_split(lines[0], "|", &column_count) : NULL;
        json_t *event = json_new_object();
        string_t *svg = string_new();
        ok = fields && column_count == 14 && event && svg;
        if (ok) {
            lab_cal_set(event, "category", string_c_str(columns[0]));
            lab_cal_set(event, "name", string_c_str(columns[1]));
            for (size_t j = 0; j < 5; ++j)
                lab_cal_set(event, geometry[j], string_c_str(columns[9 + j]));
            ok = lab_cal_number(string_c_str(columns[13]), &bearings[i]) && fabs(bearings[i]) <= 180 &&
                 lab_cal_eclipse_symbol(svg, event) && strstr(string_c_str(svg), "local zenith up") &&
                 strstr(string_c_str(svg), "almanac-eclipse-symbol");
            if (ok && (i == 3 || i == 4)) {
                double magnitude, percent, greatest, first, last;
                ok = lab_cal_number(string_c_str(columns[6]), &magnitude) &&
                     lab_cal_number(string_c_str(columns[7]), &percent) &&
                     fabs(magnitude - (i == 3 ? .517820 : .482344)) < .0001 &&
                     fabs(percent - (i == 3 ? 41.903 : 37.901)) < .01 &&
                     lab_cal_number(string_c_str(columns[3]), &greatest) &&
                     lab_cal_number(string_c_str(columns[4]), &first) &&
                     lab_cal_number(string_c_str(columns[5]), &last) &&
                     first < greatest && greatest < last && (last - first) * 24 > 1.8 &&
                     strstr(string_c_str(columns[8]), "town\tMálaga\tES\tEurope/Madrid\t") &&
                     eclipse_totality_response(string_c_str(columns[3]), options);
            }
            if (ok && i == 5) {
                double magnitude, percent;
                ok = !strcmp(string_c_str(columns[2]), "penumbral") &&
                     lab_cal_number(string_c_str(columns[6]), &magnitude) && magnitude < 0 &&
                     lab_cal_number(string_c_str(columns[7]), &percent) && percent == 0 &&
                     strstr(string_c_str(svg), "#747a80") && !strstr(string_c_str(svg), "#844e48");
            }
        }
        if (!ok)
            string_fprintf(stderr, "Eclipse worker regression %zu: %s\n", i,
                           diagnostic ? string_c_str(diagnostic) : "bad geometry");
        string_free(svg);
        json_free(event);
        string_split_free(columns, column_count);
        string_split_free(lines, line_count);
        string_free(events);
        string_free(diagnostic);
        json_free(fields);
        json_free(options);
    }
    ok = ok && almanac_observer_worker() && almanac_location_modes() && eclipse_worksheet_response();
    if (ok) {
        json_t *options = test_lab_json(requests[3]);
        string_t *diagnostic = NULL;
        ok = !setenv("MARS_JURISDICTION_DB_KEY", "wrong-fixture-key", 1);
        json_t *fields = ok ? lab_cal_run("almanac_event_lab", options, 20000, &diagnostic) : NULL;
        ok = ok && !fields && diagnostic && strstr(string_c_str(diagnostic), "location data are unavailable");
        json_free(fields);
        string_free(diagnostic);
        json_free(options);
    }
    double difference = remainder(bearings[1] - bearings[2], 360);
    return ok && fabs(difference) > 10;
}

static bool eclipse_catalogue_scale_fixture(const char *directory)
{
    bool ok = !(ephemeris_home ? setenv("MARS_HOME", string_c_str(ephemeris_home), 1) : unsetenv("MARS_HOME")) &&
              !unsetenv("MARS_LAB_OBJECT_STORE_PATH") && !unsetenv("MARS_LAB_OBJECT_STORE_KEY") &&
              !unsetenv("MARS_LAB_ALMANAC_EVENT_BINARY") && eclipse_town_catalogue(directory);
    string_t *path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = ok && path && key ? sqlite_open_encrypted(path, key) : NULL;
    ok = db && sqlite_exec_cstr(db,
        "BEGIN; INSERT INTO jurisdiction VALUES('IS',null,'country','Iceland');"
        "INSERT INTO timezone_code VALUES('Atlantic/Reykjavik',4);"
        "WITH RECURSIVE ids(id) AS (VALUES(5) UNION ALL SELECT id+1 FROM ids WHERE id<50000) "
        "INSERT INTO jurisdiction_town SELECT id,'IS','Nearby fixture '||id FROM ids;"
        "INSERT INTO jurisdiction_town_latitude SELECT jurisdiction_town_id,"
        "CASE WHEN jurisdiction_town_id%2=0 THEN '64.3218' ELSE '41.1496' END "
        "FROM jurisdiction_town WHERE jurisdiction_town_id>=5;"
        "INSERT INTO jurisdiction_town_longitude SELECT jurisdiction_town_id,"
        "CASE WHEN jurisdiction_town_id%2=0 THEN '-22.0749' ELSE '-8.6109' END "
        "FROM jurisdiction_town WHERE jurisdiction_town_id>=5;"
        "INSERT INTO jurisdiction_town_elevation SELECT jurisdiction_town_id,'8' "
        "FROM jurisdiction_town WHERE jurisdiction_town_id>=5;"
        "INSERT INTO jurisdiction_town_timezone SELECT jurisdiction_town_id,4 "
        "FROM jurisdiction_town WHERE jurisdiction_town_id>=5; COMMIT;") && ok;
    sqlite_close(db);
    string_free(path);
    string_free(key);
    json_t *options = test_lab_json(
        "{\"start\":\"2027-08-01\",\"end\":\"2027-08-04\",\"lat\":\"64.3218\",\"lon\":\"-22.0749\","
        "\"kind\":\"solar\",\"totality\":\"land\"}");
    string_t *diagnostic = NULL;
    json_t *fields = ok ? lab_cal_run("almanac_event_lab", options, 20000, &diagnostic) : NULL;
    ok = fields && strstr(lab_cal_text(fields, "event"), "town\tMálaga\tES\tEurope/Madrid\t") && ok;
    if (!ok)
        string_fprintf(stderr, "Large-catalogue land search: %s\n",
                       diagnostic ? string_c_str(diagnostic) : "missing validated town");
    json_free(fields);
    string_free(diagnostic);
    json_free(options);
    return ok;
}

static void test_lab_totality_large_catalogue(void)
{
    const char *configured_home = getenv("MARS_HOME");
    ephemeris_home = configured_home ? string_new_with(configured_home) : NULL;
    bool ok = (!configured_home || ephemeris_home) && test_lab_isolated(eclipse_catalogue_scale_fixture);
    string_free(ephemeris_home);
    ephemeris_home = NULL;
    TEST_ASSERT_TRUE(ok, "Akranes finds verified land totality beyond 1,024 nearer towns within the existing worker timeout");
}

static void test_lab_eclipse_worker_geometry(void)
{
    const char *configured_home = getenv("MARS_HOME");
    ephemeris_home = configured_home ? string_new_with(configured_home) : NULL;
    bool ok = (!configured_home || ephemeris_home) && test_lab_isolated(eclipse_worker_fixture);
    string_free(ephemeris_home);
    ephemeris_home = NULL;
    TEST_ASSERT_TRUE(ok,
                     "real eclipse workers supply preview geometry and observer-dependent lunar tilt");
}

/* Register the live worker check with the sequential native suite. */
void test_lab_eclipse_worker_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_eclipse_worker_geometry, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_totality_large_catalogue, tests, NULL);
}
