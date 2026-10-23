/**
 * @file test_lab_almanac_presentation.c
 * @brief Native almanac visibility, clipboard and event presentation regressions.
 *
 * Uses the controlled calendar test facade with synthetic native response trees.
 * Verifies local-toggle variants, original action preservation, time fallback and
 * bounded publication. Home-sky checks additionally use native zoneinfo and available
 * configured ephemerides; no weather, network or browser is required.
 */
#include <string.h>

#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
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
         strstr(test_lab_text(all, "html"), "role=\"columnheader\">Visible</th>") &&
         !strstr(test_lab_text(visible, "html"), "role=\"columnheader\">Visible</th>") &&
         strstr(test_lab_text(visible, "html"), "data-almanac-visibility=\"visible\" aria-pressed=\"true\"") &&
         strstr(test_lab_text(all, "html"), "aria-label=\"Navigational bodies\" tabindex=\"0\"") &&
         json_bool_value(test_lab_member(all, "show_visible"), &all_column) && all_column &&
         json_bool_value(test_lab_member(visible, "show_visible"), &visible_column) && !visible_column &&
         json_bool_value(test_lab_member(json_array_get(visible_rows, 0u), "is_visible"), &shown) && shown &&
         !strcmp(test_lab_text(json_array_get(visible_rows, 0u), "visible_icon"), "✓") &&
         !strcmp(test_lab_text(json_array_get(all_rows, 1u), "visible_label"), "Not visible") &&
         !strcmp(test_lab_text(visible, "copy_text"),
                 "Fixture\nNow\nHere\n"
                 "Body filter: visible only\n\nBody | Declination | GHA | RA | Altitude | Azimuth | s.d. | Vmag.\n"
                 "A |  |  |  |  |  |  | \n\nEvents\nClass | Event | Kind | Magnitude | Obscuration | Date | "
                 "First contact | Greatest eclipse | Fourth contact | Greatest GMT | Notes | Nearest totality\n"
                 "No events found.") &&
         !strcmp(test_lab_text(json_array_get(test_lab_member(response, "all_rows"), 0u), "visible"), " yes ");
    static const char *const labelled_cells[] = {
        "data-field=\"declination\" data-label=\"Declination\"", "data-field=\"gha\" data-label=\"GHA\"",
        "data-field=\"right_ascension\" data-label=\"RA\"", "data-field=\"altitude\" data-label=\"Altitude\"",
        "data-field=\"azimuth\" data-label=\"Azimuth\"", "data-field=\"semi_diameter\" data-label=\"s.d.\"",
        "data-field=\"magnitude\" data-label=\"Vmag.\""
    };
    for (size_t i = 0; ok && i < sizeof labelled_cells / sizeof *labelled_cells; ++i)
        ok = strstr(test_lab_text(all, "html"), labelled_cells[i]) &&
             strstr(test_lab_text(visible, "html"), labelled_cells[i]);
    ok = ok && strstr(test_lab_text(all, "html"), "role=\"table\"") &&
         strstr(test_lab_text(all, "html"), "role=\"rowheader\"");
    lab_cal_set(response, "atmosphere_note", "Temperature: 12 °C; atmospheric pressure: 1005 hPa <estimated>");
    lab_cal_set(response, "altitude_note", "Altitudes account for observer elevation and atmospheric refraction.");
    ok = ok && lab_cal_almanac_presentation(response);
    const json_t *updated = almanac_metadata(response);
    static const char *const modes[] = {"all", "visible"};
    for (size_t i = 0; ok && i < 2; ++i) {
        const json_t *variant = test_lab_member(updated, modes[i]);
        ok = !strstr(test_lab_text(variant, "html"), "Location of Navigational Bodies") &&
             !strstr(test_lab_text(variant, "copy_text"), "Location of Navigational Bodies") &&
             strstr(test_lab_text(variant, "html"), "<dl class=\"almanac-summary\">") &&
             strstr(test_lab_text(variant, "html"), "<dt>GMT moment</dt><dd>Now</dd>") &&
             strstr(test_lab_text(variant, "html"), "<dt>Observer</dt><dd>Here</dd>") &&
             strstr(test_lab_text(variant, "html"), "<dt>Weather</dt><dd>Temperature:") &&
             strstr(test_lab_text(variant, "html"), "class=\"almanac-summary-corrections\"") &&
             strstr(test_lab_text(variant, "html"), "Temperature: 12 °C; atmospheric pressure: 1005 hPa &lt;estimated&gt;") &&
             strstr(test_lab_text(variant, "html"), "Altitudes account for observer elevation") &&
             strstr(test_lab_text(variant, "copy_text"), "atmospheric pressure: 1005 hPa <estimated>");
    }
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

static void test_lab_almanac_symbols(void)
{
    static const char *const codes[] = {"SUN", "MERCURY", "VENUS", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE"};
    static const char *const glyphs[] = {"☉", "☿", "♀", "♂", "♃", "♄", "⛢", "♆"};
    json_t *row = json_new_object();
    bool ok = row != NULL;
    for (size_t i = 0; ok && i < sizeof(codes) / sizeof(*codes); ++i) {
        lab_cal_set(row, "code", codes[i]);
        string_t *html = string_new(), *body = string_sprintf("data-body=\"%s\"", codes[i]);
        ok = html && lab_cal_body_symbol(html, row, NULL, NULL) && strstr(string_c_str(html), glyphs[i]) &&
             strstr(string_c_str(html), "<svg") && strstr(string_c_str(html), "<g fill=\"none\"") &&
             !strstr(string_c_str(html), "<text") && !strstr(string_c_str(html), "<image") &&
             strstr(string_c_str(html), "aria-hidden=\"true\"") && strstr(string_c_str(html), string_c_str(body));
        string_free(body);
        string_free(html);
    }
    static const char *const plain[] = {"ARIES", "", "<script>"};
    for (size_t i = 0; ok && i < sizeof(plain) / sizeof(*plain); ++i) {
        lab_cal_set(row, "code", plain[i]);
        string_t *html = string_new();
        ok = html && lab_cal_body_symbol(html, row, NULL, NULL) && !string_byte_length(html);
        string_free(html);
    }
    json_free(row);
    TEST_ASSERT_TRUE(ok, "catalogue identifiers select traditional symbols; reference and unknown bodies stay plain");
}

static void test_lab_almanac_star_sizes(void)
{
    json_t *row = test_lab_json("{\"code\":\"SIRIUS\",\"magnitude\":\"-1.5\"}");
    static const struct {
        const char *magnitude, *scale;
    } cases[] = {
        {"-1.46", "0.946000"}, {"0", "0.800000"}, {"2", "0.600000"},
        {"4", "0.400000"}, {"6", "0.300000"}, {"-100", "1.000000"},
        {"100", "0.300000"}, {"", "0.600000"}, {"nan", "0.600000"},
        {"inf", "0.600000"}, {"<script>", "0.600000"}
    };
    bool ok = row != NULL;
    for (size_t i = 0; ok && i < sizeof(cases) / sizeof(*cases); ++i) {
        lab_cal_set(row, "visual_magnitude", cases[i].magnitude);
        string_t *html = string_new(), *scale = string_sprintf("scale(%s)", cases[i].scale);
        ok = html && scale && lab_cal_body_symbol(html, row, NULL, NULL) &&
             strstr(string_c_str(html), "almanac-star-symbol") && strstr(string_c_str(html), string_c_str(scale)) &&
             !strstr(string_c_str(html), "<script>") && !strstr(string_c_str(html), "<image") &&
             (i < 7 || strstr(string_c_str(html), "unavailable; default size"));
        string_free(scale);
        string_free(html);
    }
    json_free(row);
    row = test_lab_json("{\"code\":\"POLARIS\",\"magnitude\":\"2\"}");
    string_t *html = string_new();
    ok = ok && lab_cal_body_symbol(html, row, NULL, NULL) && strstr(string_c_str(html), "scale(0.600000)");
    string_free(html);
    json_free(row);
    TEST_ASSERT_TRUE(ok, "star sizes follow unrounded magnitude, clamp extremes and label unknown brightness");
}

static void test_lab_almanac_moon_disc(void)
{
    json_t *sun = test_lab_json("{\"right_ascension_hours\":\"0\",\"declination_degrees\":\"0\"}");
    json_t *moon = test_lab_json("{\"code\":\"MOON\",\"right_ascension_hours\":\"6\","
                                 "\"declination_degrees\":\"0\"}");
    static const struct {
        const char *phase, *percentage, *arc;
    } phases[] = {
        {"180", "0.0%", "A10.000000 10 0 0 0"},
        {"120", "25.0%", "A5.000000 10 0 0 0"},
        {"90", "50.0%", "A0.000000 10 0 0 1"},
        {"60", "75.0%", "A5.000000 10 0 0 1"},
        {"0", "100.0%", "A10.000000 10 0 0 1"}
    };
    bool ok = sun && moon;
    for (size_t i = 0; ok && i < sizeof(phases) / sizeof(*phases); ++i) {
        lab_cal_set(moon, "phase", phases[i].phase);
        string_t *html = string_new();
        ok = html && lab_cal_body_symbol(html, moon, sun, NULL) && strstr(string_c_str(html), phases[i].percentage) &&
             strstr(string_c_str(html), phases[i].arc) && strstr(string_c_str(html), "rotate(0.000000 12 12)");
        string_free(html);
    }
    lab_cal_set(moon, "right_ascension_hours", "18");
    lab_cal_set(moon, "phase", "90");
    string_t *html = string_new();
    ok = ok && lab_cal_body_symbol(html, moon, sun, NULL) && strstr(string_c_str(html), "rotate(-180.000000 12 12)");
    string_free(html);
    static const char *const invalid[] = {"", "nan", "inf", "-1", "181", "<script>"};
    for (size_t i = 0; ok && i < sizeof(invalid) / sizeof(*invalid); ++i) {
        lab_cal_set(moon, "phase", invalid[i]);
        html = string_new();
        ok = html && lab_cal_body_symbol(html, moon, sun, NULL) && strstr(string_c_str(html), "☾") &&
             strstr(string_c_str(html), "<svg") && !strstr(string_c_str(html), "almanac-moon-phase");
        string_free(html);
    }
    lab_cal_set(moon, "phase", "90");
    html = string_new();
    ok = ok && lab_cal_body_symbol(html, moon, NULL, NULL) && strstr(string_c_str(html), "☾");
    string_free(html);
    json_free(moon);
    json_free(sun);

    json_t *response = test_lab_json(
        "{\"all_rows\":[{\"code\":\"SUN\",\"name\":\"Sun\",\"visible\":\"NO\","
        "\"right_ascension_hours\":\"0\",\"declination_degrees\":\"0\"},"
        "{\"code\":\"MOON\",\"name\":\"Moon\",\"visible\":\"YES\",\"phase\":\"90\","
        "\"right_ascension_hours\":\"6\",\"declination_degrees\":\"0\"}]}");
    ok = ok && response && lab_cal_almanac_presentation(response);
    const json_t *visible = test_lab_member(almanac_metadata(response), "visible");
    ok = ok && json_array_size(test_lab_member(visible, "rows")) == 1 &&
         strstr(test_lab_text(visible, "html"), "Moon: 50.0% illuminated") &&
         !strstr(test_lab_text(visible, "html"), "☉");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "lunar fraction, opposite limbs, fallbacks and filtered-out Sun remain correct");
}

static void test_lab_almanac_moon_location(void)
{
    json_t *sun = test_lab_json("{\"right_ascension_hours\":\"0\",\"declination_degrees\":\"0\"}");
    json_t *moon = test_lab_json("{\"code\":\"MOON\",\"phase\":\"120\",\"right_ascension_hours\":\"6\","
                                 "\"declination_degrees\":\"0\",\"gha_degrees\":\"0\"}");
    json_t *observer = json_new_object();
    static const struct {
        const char *latitude, *longitude, *gha, *rotation;
    } locations[] = {
        {"45", "0", "0", "0.000000"},
        {"-45", "0", "0", "180.000000"},
        {"0", "0", "60", "90.000000"},
        {"0", "0", "300", "-90.000000"},
        {"0", "60", "0", "90.000000"},
        {"0", "-60", "0", "-90.000000"},
        {"90", "0", "0", "0.000000"},
        {"-90", "0", "0", "180.000000"}
    };
    bool ok = sun && moon && observer;
    for (size_t i = 0; ok && i < sizeof(locations) / sizeof(*locations); ++i) {
        lab_cal_set(observer, "observer_latitude", locations[i].latitude);
        lab_cal_set(observer, "observer_longitude", locations[i].longitude);
        lab_cal_set(moon, "gha_degrees", locations[i].gha);
        string_t *html = string_new(), *rotation = string_sprintf("rotate(%s 12 12)", locations[i].rotation);
        ok = html && rotation && lab_cal_body_symbol(html, moon, sun, observer) &&
             strstr(string_c_str(html), string_c_str(rotation)) &&
             strstr(string_c_str(html), "25.0% illuminated; local zenith up");
        string_free(rotation);
        string_free(html);
    }
    /* At the equatorial zenith no local vertical can be assigned to the disc. */
    lab_cal_set(observer, "observer_latitude", "0");
    string_t *html = string_new();
    ok = ok && lab_cal_body_symbol(html, moon, sun, observer) &&
         strstr(string_c_str(html), "celestial north up; local orientation unavailable");
    string_free(html);
    lab_cal_set(observer, "observer_latitude", "91");
    html = string_new();
    ok = ok && lab_cal_body_symbol(html, moon, sun, observer) &&
         strstr(string_c_str(html), "local orientation unavailable") && !strstr(string_c_str(html), "nan");
    string_free(html);
    json_free(observer);
    json_free(moon);
    json_free(sun);
    TEST_ASSERT_TRUE(ok, "local lunar tilt follows hemisphere, equator, longitude and time without changing phase");
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
    json_t *rows = test_lab_json("[{\"label\":\" <label> \",\"value\":\" \",\"tooltip\":\"\\\"<script>&'\"},"
                                 "{\"label\":\" \",\"value\":\" \"},{\"value\":\" <script>&\\rμ \"}]");
    html = lab_cal_section_markup("", false, rows);
    ok = ok && html && strstr(string_c_str(html), "<details class=\"datetime-section\"><summary>Calendar</summary>") &&
         strstr(string_c_str(html), "&lt;label&gt;") && strstr(string_c_str(html), ">unavailable</span>") &&
         strstr(string_c_str(html), "title=\"&quot;&lt;script&gt;&amp;&#39;\"") &&
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

static void test_lab_country_presentation(void)
{
    json_t *response = test_lab_json("{\"observer_summary\":[{\"label\":\"Location\",\"value\":\"Britain & <UK>\","
                                    "\"flag\":\"data:image/svg+xml;base64,PHN2Zy8+\"}],\"rows\":[],\"events\":[]}");
    string_t *html = lab_cal_almanac_markup(response, test_lab_member(response, "rows"),
                                           test_lab_member(response, "events"), false);
    bool ok = html && strstr(string_c_str(html), "class=\"country-flag\" alt=\"\"") &&
              strstr(string_c_str(html), "Britain &amp; &lt;UK&gt;") && strstr(string_c_str(html), "<dt>Location</dt>") &&
              !strstr(string_c_str(html), "<dt>Country</dt>");
    string_free(html);
    json_t *cell = (json_t *)json_array_get(test_lab_member(response, "observer_summary"), 0);
    lab_cal_set(cell, "flag", "javascript:alert(1)");
    html = lab_cal_almanac_markup(response, test_lab_member(response, "rows"), test_lab_member(response, "events"), false);
    ok = ok && html && !strstr(string_c_str(html), "javascript:") && !strstr(string_c_str(html), "country-flag") &&
         strstr(string_c_str(html), "Britain &amp; &lt;UK&gt;");
    string_free(html); json_free(response);
    TEST_ASSERT_TRUE(ok, "country text remains accessible and escaped; only offline SVG image URIs are accepted");
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

static void test_lab_eclipse_previews(void)
{
    json_t *event = test_lab_json("{\"category\":\"Solar\",\"name\":\"Solar eclipse\",\"disc_radius\":\"1\","
                                  "\"cover_radius\":\"1\",\"centre_separation\":\"1\","
                                  "\"orientation_degrees\":\"45\"}");
    string_t *html = string_new();
    bool ok = event && html && lab_cal_eclipse_symbol(html, event) &&
              strstr(string_c_str(html), "r=\"10\" fill=\"#efc36a\"") &&
              strstr(string_c_str(html), "rotate(-45.000000000)") &&
              strstr(string_c_str(html), "M5.000000000 -8.660254038 A10 10") &&
              strstr(string_c_str(html), "local zenith up") && !strstr(string_c_str(html), "<image");
    string_free(html);
    lab_cal_set(event, "cover_radius", ".8");
    lab_cal_set(event, "centre_separation", ".1");
    html = string_new();
    ok = ok && lab_cal_eclipse_symbol(html, event) &&
         strstr(string_c_str(html), "cx=\"1.000000000\" r=\"8.000000000\"");
    string_free(html);
    lab_cal_set(event, "cover_radius", "1.1");
    lab_cal_set(event, "centre_separation", "0");
    html = string_new();
    ok = ok && lab_cal_eclipse_symbol(html, event) && strstr(string_c_str(html), "r=\"10\" fill=\"#19232a\"");
    string_free(html);
    lab_cal_set(event, "category", "Lunar");
    lab_cal_set(event, "name", "Lunar eclipse");
    lab_cal_set(event, "cover_radius", "2");
    lab_cal_set(event, "penumbra_radius", "3");
    lab_cal_set(event, "centre_separation", "3.5");
    lab_cal_set(event, "orientation_degrees", "-90");
    html = string_new();
    ok = ok && lab_cal_eclipse_symbol(html, event) && strstr(string_c_str(html), "#747a80") &&
         !strstr(string_c_str(html), "#844e48") && strstr(string_c_str(html), "rotate(-180.000000000)");
    string_free(html);
    lab_cal_set(event, "centre_separation", "2.5");
    html = string_new();
    ok = ok && lab_cal_eclipse_symbol(html, event) && strstr(string_c_str(html), "#747a80") &&
         strstr(string_c_str(html), "#844e48");
    string_free(html);
    lab_cal_set(event, "centre_separation", "0");
    lab_cal_set(event, "orientation_degrees", "nan");
    html = string_new();
    ok = ok && lab_cal_eclipse_symbol(html, event) && strstr(string_c_str(html), "r=\"10\" fill=\"#844e48\"") &&
         strstr(string_c_str(html), "orientation unavailable");
    string_free(html);
    static const char *const invalid[] = {"", "nan", "inf", "0", "-1", "<script>"};
    for (size_t i = 0; ok && i < sizeof(invalid) / sizeof(*invalid); ++i) {
        lab_cal_set(event, "disc_radius", invalid[i]);
        html = string_new();
        ok = html && lab_cal_eclipse_symbol(html, event) && !string_byte_length(html);
        string_free(html);
    }
    json_free(event);
    TEST_ASSERT_TRUE(ok, "solar/lunar overlap geometry, observer tilt and missing-data handling remain native SVG");
}

static void test_lab_sky_status(void)
{
    json_t *response = test_lab_json(
        "{\"fields\":{\"date\":\"2026-10-22\",\"time\":\"19:00:20.0\"},"
        "\"observer_latitude\":\"52.7\",\"observer_longitude\":\"-2.75\","
        "\"all_rows\":[{\"code\":\"MOON\",\"phase\":\"60\",\"right_ascension_hours\":\"10\","
        "\"declination_degrees\":\"5\",\"gha_degrees\":\"30\"},{\"code\":\"SUN\","
        "\"right_ascension_hours\":\"14\",\"declination_degrees\":\"-10\"}],"
        "\"observer_summary\":[{\"label\":\"Location\",\"value\":\"England & point\","
        "\"flag\":\"data:image/svg+xml;base64,PHN2Zy8+\"}]}" );
    string_t *html = lab_cal_sky_markup(response, true);
    const char *text = html ? string_c_str(html) : "";
    const char *moon = strstr(text, "almanac-moon-phase"), *set = strstr(text, "Sunset ");
    const char *rise = strstr(text, "Sunrise "), *moment = strstr(text, "2026-10-22 19:00:20 GMT");
    const char *flag = strstr(text, "class=\"country-flag\"");
    bool ok = moon && set && rise && moment && flag && moon < set && set < rise && rise < moment && moment < flag &&
              strstr(text, "England &amp; point") && strstr(text, "75.0% illuminated") &&
              strstr(text, "Selected time") && strstr(text, "sky-status-value") && !strstr(text, "19:00:20.0");
    string_free(html);
    lab_cal_set(response, "observer_latitude", "90");
    html = lab_cal_sky_markup(response, true);
    ok = ok && html && strstr(string_c_str(html), "polar night");
    string_free(html);
    lab_cal_set(response, "observer_latitude", "nan");
    html = lab_cal_sky_markup(response, true);
    ok = ok && html && strstr(string_c_str(html), "unavailable");
    string_free(html);
    json_free(response);
    response = test_lab_json("{\"fields\":{\"date\":\"2026-10-22\",\"sunset\":\"18:20\","
                             "\"sunrise\":\"07:10\",\"moon_phase\":\"Waxing Crescent\"}}");
    html = lab_cal_sky_markup(response, false);
    ok = ok && html && strstr(string_c_str(html), "🌒") && strstr(string_c_str(html), "18:20") &&
         strstr(string_c_str(html), "07:10") && !strstr(string_c_str(html), "country-flag");
    string_free(html);
    json_free(response);
    json_t *origin = test_lab_json("{\"location\":\"Home & origin\",\"jurisdiction\":\"GB-ENG\","
        "\"latitude\":\"52.7077\",\"longitude\":\"-2.7541\",\"timezone\":\"Europe/London\"}");
    html = lab_cal_home_markup(origin, (time_t)1792670400);
    ok = ok && html && strstr(string_c_str(html), "2026-10-22 13:00:00 GMT+01:00") &&
         strstr(string_c_str(html), "Home &amp; origin") && strstr(string_c_str(html), "Sunset · local") &&
         strstr(string_c_str(html), "almanac-moon-phase") &&
         strstr(string_c_str(html), "local zenith up") &&
         !strstr(string_c_str(html), "Selected time");
    string_free(html);
    html = lab_cal_home_markup(origin, (time_t)1797940800);
    ok = ok && html && strstr(string_c_str(html), "2026-12-22 12:00:00 GMT+00:00") &&
         strstr(string_c_str(html), "almanac-moon-phase");
    string_free(html);
    lab_cal_set(origin, "timezone", "missing/origin");
    html = lab_cal_home_markup(origin, (time_t)1792670400);
    ok = ok && !html;
    string_free(html);
    json_free(origin);
    TEST_ASSERT_TRUE(ok, "native sky strip preserves Moon orientation, clock order, flags, polar states and escaping");
}

/* Register bounded almanac presentation checks before README examples. */
void test_lab_almanac_presentation_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_sky_status, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_variants, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_symbols, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_star_sizes, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_moon_disc, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_moon_location, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_times, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_land_policy, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_calendar_markup_escaping, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_country_presentation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_almanac_presentation_bound, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_eclipse_previews, tests, NULL);
}
