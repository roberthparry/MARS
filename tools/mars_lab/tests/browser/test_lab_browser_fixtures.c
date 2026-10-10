/**
 * @file test_lab_browser_fixtures.c
 * @brief Native calendar markup fixtures encoded for real-browser layout tests.
 *
 * Exercises the production calendar renderers and Protobuf encoder without an
 * ephemeris worker. Emits only decimal bytes into the disposable test page, so
 * test text cannot become script source. Kept separate from process/signal code
 * because the library and POSIX stack types occupy the same C namespace.
 */
#include "internal/lab_calendar_internal.h"
#include "lab_wire.h"
#include "test_lab_browser_fixtures.h"
#include "test_lab_support.h"

/* Append native, binary fixtures for consumption after WebAssembly startup. */
bool lab_browser_almanac_fixture(string_t *script)
{
    json_t *fixture = test_lab_json(
        "{\"visibility\":\"all\",\"worksheet_title\":\"Layout regression\","
        "\"event_title\":\"Upcoming eclipses and inner planetary transits\","
        "\"all_rows\":[{\"name\":\"Sun\",\"visible\":\"YES\"},{\"name\":\"Moon\",\"visible\":\"NO\"}],"
        "\"events\":[{\"category\":\"Solar\",\"name\":\"Solar eclipse\",\"kind\":\"partial\",\"magnitude\":\"0.482\","
        "\"obscuration\":\"37.9%\",\"first_contact\":\"2027-08-02 09:06:00 GMT+1\","
        "\"greatest\":\"2027-08-02 10:08:00 GMT+1\",\"fourth_contact\":\"2027-08-02 11:13:00 GMT+1\","
        "\"gmt_time\":\"2027-08-02 09:08:00 GMT+0\","
        "\"nearest_totality\":\"Málaga, ES; 36.7204, -4.4203; 2027-08-02 10:48:52 GMT+2; 1782 km from observer\","
        "\"nearest_totality_action\":{\"date\":\"2027-08-02\",\"time\":\"10:48:52\",\"zone\":\"2\","
        "\"jurisdiction\":\"ES\",\"town\":\"Málaga|36.7204|-4.4203|0\",\"latitude\":\"36.7204\","
        "\"longitude\":\"-4.4203\",\"elevation\":\"0\"}}]}");
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
