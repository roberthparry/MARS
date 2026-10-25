/**
 * @file test_lab_browser_fixtures.c
 * @brief Native calendar markup fixtures encoded for real-browser layout tests.
 *
 * Exercises the production calendar renderers and Protobuf encoder without an
 * ephemeris worker. Emits only decimal bytes into the disposable test page, so
 * test text cannot become script source. Kept separate from process/signal code
 * because the library and POSIX stack types occupy the same C namespace.
 */
#include <stdlib.h>
#include <string.h>

#include "jurisdiction-t.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
#include "lab_wire.h"
#include "test_lab_support.h"
#include "test_lab_browser_fixtures.h"

static json_t *lab_browser_flag_fixture(void)
{
    string_t *path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = sqlite_open_encrypted(path, key);
    file_t *source = file_new_cstr("packaging/jurisdiction-db/flag_regions.sql");
    string_t *sql = file_read_all_text(source);
    bool ok = db && sql && sqlite_exec(db, sql) && sqlite_exec_cstr(db,
        "create table jurisdiction_country_flag(iso_country_code text primary key,svg text not null);"
        "create table calendar_local_settings(singleton integer primary key,jurisdiction text,location text,"
        "latitude real,longitude real,timezone text);"
        "insert into calendar_local_settings values(1,'GB-ENG','Shrewsbury',52.7077,-2.7541,'Europe/London');"
        "insert into jurisdiction values('FR',null,'country','France'),('ES',null,'country','Spain'),"
        "('IT',null,'country','Italy'),('DE',null,'country','Germany'),('FI',null,'country','Finland'),"
        "('PL',null,'country','Poland'),('MN',null,'country','Mongolia')");
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db, "insert into jurisdiction_country_flag values(?1,?2)") : NULL;
    const char *codes[] = {"GB-CON", "GB-ENG", "GB", "FR", "ES", "IT", "DE", "FI", "PL", "MN"};
    const char *files[] = {"gb-con", "gb-eng", "gb", "fr", "es", "it", "de", "fi", "pl", "mn"};
    for (unsigned i = 0; ok && i < sizeof codes / sizeof *codes; ++i) {
        string_t *filename = string_sprintf("packaging/jurisdiction-db/flags/%s.svg", files[i]);
        file_t *image = file_new(filename);
        string_t *svg = file_read_all_text(image);
        ok = insert && svg && sqlite_stmt_bind_text(insert, 1, codes[i]) &&
             sqlite_stmt_bind_text(insert, 2, string_c_str(svg)) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
        sqlite_stmt_reset(insert); string_free(svg); file_free(image); string_free(filename);
    }
    sqlite_stmt_finalize(insert); sqlite_close(db); string_free(path); string_free(key); string_free(sql); file_free(source);
    ok = ok && test_lab_offshore_database();
    const struct { const char *country, *parent; } parents[] = {
        {"GB-ENG", "GB"}, {"GB-WLS", "GB"}, {"GB-SCT", "GB"}, {"GB-CON", "GB"}, {"GB-NIR", "GB"},
        {"IM", "GB"}, {"JE", "GB"}, {"GG", "GB"}, {"FR", "FR"}, {"ES", "ES"}, {"IT", "IT"},
        {"DE", "DE"}, {"FI", "FI"}, {"PL", "PL"}, {"MN", "MN"}, {"", ""}, {"ZZ-SEA", ""}
    };
    for (unsigned i = 0; ok && i < sizeof parents / sizeof *parents; ++i) {
        json_t *options = json_new_object(), *request = json_new_object();
        lab_cal_set(options, "country_code", parents[i].country);
        lab_cal_set(options, "jurisdiction", "RU");
        string_t *actual = lab_cal_jurisdiction_flag(options);
        if (*parents[i].parent) {
            lab_cal_set(request, "country_code", parents[i].parent);
            unsigned status = 200;
            json_t *expected = lab_cal_country(request, &status);
            ok = actual && string_byte_length(actual) && status == 200 &&
                 !strcmp(string_c_str(actual), lab_cal_text(expected, "country_flag"));
            json_free(expected);
        } else ok = actual && !string_byte_length(actual);
        string_free(actual); json_free(options); json_free(request);
    }
    jurisdiction_t *j = ok ? jurisdict_open(NULL) : NULL;
    string_t *name = NULL, *svg = NULL;
    ok = ok && jurisdict_country(j, "GB-CON", &name, &svg) && svg;
    json_t *result = ok ? json_new_string(svg) : NULL;
    string_free(name); string_free(svg); jurisdict_close(j);
    return result;
}

/* Append native, binary fixtures for consumption after WebAssembly startup. */
bool lab_browser_almanac_fixture(string_t *script)
{
    json_t *fixture = test_lab_json(
        "{\"visibility\":\"all\",\"worksheet_title\":\"Layout regression\","
        "\"observer_latitude\":\"-45\",\"observer_longitude\":\"0\","
        "\"event_title\":\"Upcoming eclipses and inner planetary transits\","
        "\"all_rows\":[{\"code\":\"SUN\",\"name\":\"Sun\",\"visible\":\"YES\","
        "\"right_ascension_hours\":\"0\",\"declination_degrees\":\"0\","
        "\"declination\":\"S 008° 00.12\",\"gha\":\"108° 33.91\",\"right_ascension\":\"198° 53.79\","
        "\"altitude\":\"-015° 54.62\",\"azimuth\":\"277° 42.61\",\"semi_diameter\":\"16.02\",\"magnitude\":\"-26.7\"},"
        "{\"code\":\"MOON\",\"name\":\"Moon\",\"visible\":\"NO\",\"phase\":\"90\","
        "\"right_ascension_hours\":\"6\",\"declination_degrees\":\"0\",\"gha_degrees\":\"0\"},"
        "{\"code\":\"SIRIUS\",\"name\":\"Sirius\",\"visible\":\"NO\",\"visual_magnitude\":\"-1.46\"},"
        "{\"code\":\"POLARIS\",\"name\":\"Polaris\",\"visible\":\"NO\",\"visual_magnitude\":\"2\"}],"
        "\"events\":[{\"category\":\"Solar\",\"name\":\"Solar eclipse\",\"kind\":\"partial\",\"magnitude\":\"0.482\","
        "\"disc_radius\":\"1\",\"cover_radius\":\"1\",\"centre_separation\":\"1\",\"orientation_degrees\":\"45\","
        "\"obscuration\":\"37.9%\",\"first_contact\":\"2027-08-02 09:06:00 GMT+1\","
        "\"greatest\":\"2027-08-02 10:08:00 GMT+1\",\"fourth_contact\":\"2027-08-02 11:13:00 GMT+1\","
        "\"gmt_time\":\"2027-08-02 09:08:00 GMT+0\","
        "\"nearest_totality\":\"Málaga, ES; 36.7204, -4.4203; 2027-08-02 10:48:52 GMT+2; 1782 km from observer\","
        "\"nearest_totality_action\":{\"date\":\"2027-08-02\",\"time\":\"10:48:52\",\"zone\":\"2\","
        "\"jurisdiction\":\"ES\",\"town\":\"Málaga|36.7204|-4.4203|0\",\"latitude\":\"36.7204\","
        "\"longitude\":\"-4.4203\",\"elevation\":\"0\"}}]}");
    json_t *summary = test_lab_json(
        "[{\"label\":\"GMT moment\",\"value\":\"2026-10-18 18:10:39\"},"
        "{\"label\":\"Location\",\"value\":\"Shrewsbury, GB-ENG\","
        "\"flag\":\"data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciLz4=\"},"
        "{\"label\":\"Zone\",\"value\":\"1.00\"},"
        "{\"label\":\"Latitude\",\"value\":\"52° 42.46′ N\"},"
        "{\"label\":\"Longitude\",\"value\":\"2° 45.25′ W\"},"
        "{\"label\":\"Observer altitude\",\"value\":\"50 m · Land\"},"
        "{\"label\":\"Temperature\",\"value\":\"8.3 °C\"},"
        "{\"label\":\"Pressure\",\"value\":\"1032 hPa\"},"
        "{\"label\":\"Weather source\",\"value\":\"WeatherAPI temperature; estimated pressure\"}]");
    lab_cal_take(fixture, "observer_summary", summary);
    lab_cal_set(fixture, "altitude_note", "Altitudes corrected for observer height and refraction. "
                                         "No horizon dip. No refraction below −0.575°.");
    json_t *pending = test_lab_json(
        "{\"visibility\":\"all\",\"events\":[{\"category\":\"Solar\","
        "\"name\":\"Solar eclipse\",\"kind\":\"partial\",\"jd\":\"2461619.9\",\"first_contact\":\"Unavailable\"}]}");
    json_t *fixtures = json_new_array();
    json_t *sections = json_new_array();
    json_t *rows = test_lab_json("[{\"label\":\" <img src=x> \",\"value\":\"quoted & \\\"text\\\"\\rμ\"},"
                                 "{\"label\":\"Absent\",\"value\":\" \"},{\"label\":\" \",\"value\":\" \"}]");
    lab_cal_section(sections, "<script> & calendar", false, rows);
    lab_cal_section(sections, "Open", true, rows);
    bool ok = fixture && pending && fixtures && lab_cal_almanac_presentation(fixture) &&
              lab_cal_almanac_presentation(pending) && json_array_append(fixtures, fixture) &&
              json_array_append(fixtures, pending) && rows && sections && json_array_append(fixtures, sections);
    json_t *flag = lab_browser_flag_fixture();
    ok = ok && flag && json_array_append(fixtures, flag);
    json_free(flag);
    json_t *destination = json_clone(fixture);
    json_t *destination_summary = test_lab_json(
        "[{\"label\":\"Location\",\"value\":\"Santiago, Chile\"},"
        "{\"label\":\"Latitude\",\"value\":\"33° 26.93′ S\"},"
        "{\"label\":\"Longitude\",\"value\":\"70° 40.16′ W\"}]");
    lab_cal_take(destination, "observer_summary", destination_summary);
    lab_cal_set(destination, "observer_latitude", "-33.4489");
    lab_cal_set(destination, "observer_longitude", "-70.6693");
    ok = ok && destination && lab_cal_almanac_presentation(destination) && json_array_append(fixtures, destination);
    json_free(destination);
    json_free(rows);
    json_free(sections);
    json_t *envelope = json_new_object();
    lab_cal_take(envelope, "fixtures", fixtures);
    array_t *bytes = ok && envelope ? lab_wire_encode(envelope) : NULL;
    ok = bytes && !string_append_cstr(script, "\nwindow.labAlmanacFixtures = () => labWire.decode(new Uint8Array([");
    for (size_t i = 0; ok && i < array_size(bytes); ++i)
        ok = string_append_format(script, "%s%u", i ? "," : "", *(const unsigned char *)array_get(bytes, i)) >= 0;
    ok = ok && !string_append_cstr(script, "]).buffer).fixtures;\n");
    array_destroy(bytes);
    json_free(envelope);
    json_free(pending);
    json_free(fixture);
    return ok;
}
