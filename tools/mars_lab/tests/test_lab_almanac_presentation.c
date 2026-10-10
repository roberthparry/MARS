/**
 * @file test_lab_almanac_presentation.c
 * @brief Native almanac visibility, clipboard and event presentation regressions.
 *
 * Uses the controlled calendar test facade with synthetic native response trees.
 * No ephemeris, worker, network or browser is required. Verifies local-toggle
 * variants, original action preservation, time fallback and bounded publication.
 */
#include <string.h>

#include "internal/lab_calendar_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static const json_t *almanac_metadata(const json_t *response)
{
    return test_lab_member(response, "almanac_presentation");
}

static const json_t *almanac_event(const json_t *response, size_t index)
{
    return json_array_get(test_lab_member(almanac_metadata(response), "events"), index);
}

static void test_lab_almanac_variants(void)
{
    json_t *response =
        test_lab_json("{\"worksheet_title\":\"Fixture\",\"moment_text\":\"Now\","
                      "\"observer_text\":\"Here\",\"event_title\":\"Events\",\"rows\":[{\"name\":\"ignored\"}],"
                      "\"all_rows\":[{\"name\":\" A \",\"visible\":\" yes \"},{\"code\":\"B\",\"visible\":\"NO\"}],"
                      "\"events\":[]}");
    bool ok = response && lab_cal_almanac_presentation(response);
    const json_t *all = test_lab_member(almanac_metadata(response), "all");
    const json_t *visible = test_lab_member(almanac_metadata(response), "visible");
    const json_t *all_rows = test_lab_member(all, "rows"), *visible_rows = test_lab_member(visible, "rows");
    bool all_column = false, visible_column = true, shown = false;
    ok = ok && json_array_size(all_rows) == 2u && json_array_size(visible_rows) == 1u &&
         strstr(test_lab_text(all, "html"), "<th>Visible</th>") &&
         !strstr(test_lab_text(visible, "html"), "<th>Visible</th>") &&
         strstr(test_lab_text(visible, "html"), "data-almanac-visibility=\"visible\" aria-pressed=\"true\"") &&
         strstr(test_lab_text(all, "html"), "aria-label=\"Navigational bodies\" tabindex=\"0\"") &&
         json_bool_value(test_lab_member(all, "show_visible"), &all_column) && all_column &&
         json_bool_value(test_lab_member(visible, "show_visible"), &visible_column) && !visible_column &&
         json_bool_value(test_lab_member(json_array_get(visible_rows, 0u), "is_visible"), &shown) && shown &&
         !strcmp(test_lab_text(json_array_get(visible_rows, 0u), "visible_icon"), "✓") &&
         !strcmp(test_lab_text(json_array_get(all_rows, 1u), "visible_label"), "Not visible") &&
         !strcmp(test_lab_text(visible, "copy_text"),
                 "Fixture\nNow\nHere\nLocation of Navigational Bodies; visible bodies only\n"
                 "Body filter: visible only\n\nBody | Declination | GHA | RA | Altitude | Azimuth | s.d. | Vmag.\n"
                 "A |  |  |  |  |  |  | \n\nEvents\nClass | Event | Kind | Magnitude | Obscuration | Date | "
                 "First contact | Greatest eclipse | Fourth contact | Greatest GMT | Notes | Nearest totality\n"
                 "No events found.") &&
         !strcmp(test_lab_text(json_array_get(test_lab_member(response, "all_rows"), 0u), "visible"), " yes ");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "both local visibility variants and exact clipboard text retain original rows");
}

static void test_lab_almanac_times(void)
{
    static const char *const zones[] = {"GMT+0", "GMT+1", "GMT-5", "GMT+5:30", "GMT+05:45", "GMT-03:30", "GMT"};
    bool ok = true;
    for (size_t i = 0; ok && i < sizeof(zones) / sizeof(*zones); ++i) {
        json_t *response = test_lab_json("{\"events\":[{}]}");
        json_t *events = json_new_array();
        json_t *event = json_new_object();
        string_t *clock = string_sprintf("2027-08-02 10:48:52 %s", zones[i]);
        lab_cal_set(event, "greatest", string_c_str(clock));
        lab_cal_set(event, "gmt_time", string_c_str(clock));
        ok = response && event && clock && events && json_array_append(events, event);
        lab_cal_take(response, "events", events);
        ok = ok && lab_cal_almanac_presentation(response) &&
             !strcmp(test_lab_text(almanac_event(response, 0u), "date_text"), "2027-08-02") &&
             !strcmp(test_lab_text(almanac_event(response, 0u), "greatest_text"), "10:48:52") &&
             !strcmp(test_lab_text(almanac_event(response, 0u), "gmt_text"), "10:48:52");
        json_free(event);
        json_free(response);
        string_free(clock);
    }
    json_t *response =
        test_lab_json("{\"events\":[{\"greatest\":\"Unavailable\","
                      "\"time\":\"2027-08-02T10:48:52\",\"first_contact\":\" 2027-08-01 09:00:00 GMT+1 \","
                      "\"fourth_contact\":\"2027-08-02 10:48:52 GMT+123\",\"gmt_time\":\"10:48:52 GMT\"}]}");
    ok = ok && response && lab_cal_almanac_presentation(response) &&
         !strcmp(test_lab_text(almanac_event(response, 0u), "date_text"), "2027-08-01") &&
         !strcmp(test_lab_text(almanac_event(response, 0u), "greatest_text"), "Unavailable") &&
         !strcmp(test_lab_text(almanac_event(response, 0u), "fourth_text"), "2027-08-02 10:48:52 GMT+123") &&
         !strcmp(test_lab_text(almanac_event(response, 0u), "gmt_text"), "10:48:52");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "compact times preserve supported offsets, date precedence and unavailable/malformed text");
}

static void test_lab_almanac_land_policy(void)
{
    json_t *response = test_lab_json(
        "{\"events\":["
        "{\"category\":\" Solar \",\"name\":\"Solar eclipse\",\"kind\":\"partial\",\"jd\":\"1\"},"
        "{\"category\":\"Solar\",\"name\":\"Solar eclipse\",\"kind\":\"total\"},"
        "{\"category\":\"Solar\",\"name\":\"Solar eclipse\",\"kind\":\"partial\","
        "\"nearest_totality\":\"Town\",\"nearest_totality_action\":{\"date\":\"2027-08-02\",\"town\":\"A|B\"}},"
        "{\"category\":\"Inner planet\",\"name\":\"Mercury transit\",\"time\":\"2032-11-13\"}]}");
    bool ok = response && lab_cal_almanac_presentation(response);
    for (size_t i = 0; ok && i < 4u; ++i) {
        bool land = false;
        ok = json_bool_value(test_lab_member(almanac_event(response, i), "needs_land_search"), &land) &&
             land == (i == 0u);
    }
    const json_t *action = test_lab_member(almanac_event(response, 2u), "nearest_totality_action");
    ok = ok && !strcmp(test_lab_text(action, "date"), "2027-08-02") && !strcmp(test_lab_text(action, "town"), "A|B") &&
         strstr(test_lab_text(test_lab_member(almanac_metadata(response), "all"), "html"),
                "data-almanac-land-totality=\"1\"") &&
         strstr(test_lab_text(test_lab_member(almanac_metadata(response), "all"), "html"), "data-town=\"A|B\"") &&
         !strcmp(test_lab_text(almanac_event(response, 0u), "nearest_text"),
                 "Searching for nearest location on land...") &&
         !strcmp(test_lab_text(almanac_event(response, 3u), "date_text"), "2032-11-13");
    json_free(response);
    TEST_ASSERT_TRUE(ok,
                     "native land eligibility excludes total eclipses, existing results and transits; actions survive");
}

static void test_lab_calendar_markup_escaping(void)
{
    json_t *action = test_lab_json("{\"town\":\"Málaga \\\" onclick=\\\"bad & < > '\\r\",\"date\":\"2027-08-02\"}");
    const char *unsafe = "<script>&\"'\rμ";
    string_t *html = lab_cal_totality_markup(unsafe, action);
    bool ok = html && strstr(string_c_str(html), "&lt;script&gt;&amp;&quot;&#39;&#13;μ") &&
              strstr(string_c_str(html), "data-town=\"Málaga &quot; onclick=&quot;bad &amp; &lt; &gt; &#39;&#13;\"") &&
              !strstr(string_c_str(html), "<script>");
    string_free(html);
    json_free(action);
    action = test_lab_json("{\"town\":\"  \",\"date\":\"ignored\"}");
    html = lab_cal_totality_markup("plain & text", action);
    ok = ok && html && !strcmp(string_c_str(html), "plain &amp; text");
    string_free(html);
    json_free(action);
    json_t *rows = test_lab_json("[{\"label\":\" <label> \",\"value\":\" \"},"
                                 "{\"label\":\" \",\"value\":\" \"},{\"value\":\" <script>&\\rμ \"}]");
    html = lab_cal_section_markup("", false, rows);
    ok = ok && html && strstr(string_c_str(html), "<details class=\"datetime-section\"><summary>Calendar</summary>") &&
         strstr(string_c_str(html), "&lt;label&gt;") && strstr(string_c_str(html), ">unavailable</span>") &&
         strstr(string_c_str(html), "&lt;script&gt;&amp;&#13;μ") && !strstr(string_c_str(html), "<script>");
    string_free(html);
    json_free(rows);
    rows = json_new_array();
    html = lab_cal_section_markup("Empty", true, rows);
    ok = ok && html && !string_byte_length(html);
    string_free(html);
    json_free(rows);
    TEST_ASSERT_TRUE(ok, "native markup escapes content and action attributes and retains section empty-value policy");
}

static void test_lab_almanac_presentation_bound(void)
{
    json_t *response = test_lab_json("{\"almanac_presentation\":{\"sentinel\":\"unchanged\"}}");
    json_t *rows = json_new_array(), *row = test_lab_json("{\"name\":\"Body\",\"visible\":\"YES\"}");
    bool ok = response && rows && row;
    for (size_t i = 0; ok && i < 257u; ++i)
        ok = json_array_append(rows, row);
    lab_cal_take(response, "all_rows", rows);
    ok = ok && !lab_cal_almanac_presentation(response) &&
         !strcmp(test_lab_text(almanac_metadata(response), "sentinel"), "unchanged");
    json_free(row);
    json_free(response);
    response = test_lab_json("{\"almanac_presentation\":{\"sentinel\":\"unchanged\"}}");
    json_t *events = json_new_array(), *event = test_lab_json("{\"name\":\"Event\"}");
    ok = ok && response && events && event;
    for (size_t i = 0; ok && i < 65u; ++i)
        ok = json_array_append(events, event);
    lab_cal_take(response, "events", events);
    ok = ok && !lab_cal_almanac_presentation(response) &&
         !strcmp(test_lab_text(almanac_metadata(response), "sentinel"), "unchanged");
    json_free(event);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "oversized variants fail before replacing existing presentation metadata");
}

/* Register bounded almanac presentation checks before README examples. */
void test_lab_almanac_presentation_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_almanac_variants, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_times, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_land_policy, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_calendar_markup_escaping, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_presentation_bound, tests, NULL);
}
