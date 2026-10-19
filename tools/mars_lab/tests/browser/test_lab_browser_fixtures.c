/**
 * @file test_lab_browser_fixtures.c
 * @brief Native calendar markup fixtures encoded for real-browser layout tests.
 *
 * Exercises the production calendar renderers and Protobuf encoder without an
 * ephemeris worker. Emits only decimal bytes into the disposable test page, so
 * test text cannot become script source. Kept separate from process/signal code
 * because the library and POSIX stack types occupy the same C namespace.
 */
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
#include "lab_wire.h"
#include "test_lab_browser_fixtures.h"
#include "test_lab_support.h"

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
        "{\"label\":\"Location\",\"value\":\"Shrewsbury, GB-ENG\"},"
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
