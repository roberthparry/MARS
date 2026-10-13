/**
 * @file test_lab_eclipse_worker.c
 * @brief End-to-end native eclipse worker geometry and observer-orientation regression.
 *
 * Uses the configured ephemeris read-only, in an isolated child without a cache
 * or worker override. Checks the solar/lunar wire fields and resulting SVG at
 * greatest eclipse, including differing lunar orientations for two observers.
 * A disposable town catalogue exercises named totality, action transport and
 * catalogue-error reporting without modifying the installed data.
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
        "insert into jurisdiction_town_latitude values(4,'36.7202');"
        "insert into jurisdiction_town_longitude values(4,'-4.4203');"
        "insert into jurisdiction_town_elevation values(4,'22');"
        "insert into jurisdiction_town_timezone values(4,3);");
    bool ok = db && sql && sqlite_exec(db, sql);
    sqlite_close(db);
    string_free(sql);
    string_free(key);
    string_free(path);
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
              strstr(test_lab_text(test_lab_member(item, "nearest_totality_action"), "town"), "Málaga|");
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
    unsigned status = 200;
    json_t *response = fields ? lab_cal_almanac(fields, options, &status) : NULL;
    const json_t *metadata = test_lab_member(response, "almanac_presentation");
    const char *html = test_lab_text(test_lab_member(metadata, "all"), "html");
    bool ok = response && status == 200 && test_lab_ok(response, true) &&
              strstr(html, "almanac-moon-phase") && strstr(html, "Moon: 10.") &&
              strstr(html, "0.0% umbral") && strstr(html, "excludes penumbral dimming") &&
              strstr(html, "Umbral magnitude; negative means");
    if (!ok)
        string_fprintf(stderr, "Eclipse worksheet regression: %s\n", diagnostic ? string_c_str(diagnostic) : "bad markup");
    json_free(response);
    json_free(fields);
    string_free(diagnostic);
    json_free(options);
    return ok;
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
    ok = ok && eclipse_worksheet_response();
    if (ok) {
        json_t *options = test_lab_json(requests[3]);
        string_t *diagnostic = NULL;
        ok = !setenv("MARS_JURISDICTION_DB_KEY", "wrong-fixture-key", 1);
        json_t *fields = ok ? lab_cal_run("almanac_event_lab", options, 20000, &diagnostic) : NULL;
        ok = ok && !fields && diagnostic && strstr(string_c_str(diagnostic), "catalogue is unavailable");
        json_free(fields);
        string_free(diagnostic);
        json_free(options);
    }
    double difference = remainder(bearings[1] - bearings[2], 360);
    return ok && fabs(difference) > 10;
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
}
