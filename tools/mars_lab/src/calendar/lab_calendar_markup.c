/**
 * @file lab_calendar_markup.c
 * @brief Escaped native almanac worksheet markup for the browser's DOM bridge.
 *
 * Renders both visibility variants from prepared calendar metadata, retaining
 * accessible tables, compact clocks and exact action attributes. All variable
 * text passes through string_t escaping; only fixed markup is trusted. These
 * private helpers neither calculate astronomy nor access browser state.
 */
#include <string.h>

#include "string-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static bool lab_cal_markup_text(string_t *out, const char *value)
{
    string_t *text = string_new_with(value);
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    bool ok = cursor != NULL;
    while (ok && !string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        unsigned ch = rune_value(rune);
        const char *entity = ch == '&'    ? "&amp;"
                             : ch == '<'  ? "&lt;"
                             : ch == '>'  ? "&gt;"
                             : ch == '"'  ? "&quot;"
                             : ch == '\'' ? "&#39;"
                             : ch == '\r' ? "&#13;"
                                          : NULL;
        ok = !(entity ? string_append_cstr(out, entity) : string_append_rune(out, rune));
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    string_free(text);
    return ok;
}

static bool lab_cal_markup_flag(const json_t *object, const char *key)
{
    bool value = false;
    json_bool_value(lab_cal_get(object, key), &value);
    return value;
}

/* Keep section layout and empty-value policy in the native calendar module. */
string_t *lab_cal_section_markup(const char *title, bool open, const json_t *rows)
{
    string_t *out = string_new();
    if (!out || !json_array_size(rows))
        return out;
    bool ok = string_append_format(out, "<details class=\"datetime-section\"%s><summary>", open ? " open" : "") >= 0 &&
              lab_cal_markup_text(out, *title ? title : "Calendar") &&
              !string_append_cstr(out, "</summary><div class=\"datetime-section-rows\">");
    for (size_t i = 0; ok && i < json_array_size(rows); ++i) {
        const json_t *row = json_array_get(rows, i);
        string_t *label = string_new_with(lab_cal_text(row, "label")),
                 *value = string_new_with(lab_cal_text(row, "value"));
        ok = label && value;
        if (ok) {
            string_trim(label);
            string_trim(value);
            if (string_byte_length(label) || string_byte_length(value))
                ok = !string_append_cstr(out, "<div class=\"datetime-row\"><span class=\"datetime-row-label\">") &&
                     lab_cal_markup_text(out, string_c_str(label)) &&
                     !string_append_cstr(out, "</span><span class=\"datetime-row-value\">") &&
                     lab_cal_markup_text(out, string_byte_length(value) ? string_c_str(value) : "unavailable") &&
                     !string_append_cstr(out, "</span></div>");
        }
        string_free(label);
        string_free(value);
    }
    ok = ok && !string_append_cstr(out, "</div></details>");
    if (!ok) {
        string_free(out);
        return NULL;
    }
    return out;
}

/* Build an escaped text/action fragment shared by initial and deferred results. */
string_t *lab_cal_totality_markup(const char *text, const json_t *action)
{
    string_t *out = string_new(), *town = string_new_with(lab_cal_text(action, "town"));
    if (town)
        string_trim(town);
    bool button = town && string_byte_length(town);
    bool ok = out && town && (!button || !string_append_cstr(out, "<span class=\"almanac-totality-action\"><span>")) &&
              lab_cal_markup_text(out, text);
    if (button) {
        ok = ok && !string_append_cstr(out, "</span><button type=\"button\" class=\"almanac-use-totality\" "
                                            "data-almanac-use-totality=\"1\"");
        static const char *const fields[] = {"date", "time",     "zone",      "jurisdiction",
                                             "town", "latitude", "longitude", "elevation"};
        for (size_t i = 0; ok && i < sizeof fields / sizeof *fields; ++i)
            ok = string_append_format(out, " data-%s=\"", fields[i]) >= 0 &&
                 lab_cal_markup_text(out, lab_cal_text(action, fields[i])) && !string_append_char(out, '"');
        ok = ok && !string_append_cstr(out, ">Use</button></span>");
    }
    string_free(town);
    if (!ok) {
        string_free(out);
        return NULL;
    }
    return out;
}

static bool lab_cal_markup_bodies(string_t *out, const json_t *rows, const json_t *source,
                                  const json_t *observer, bool visible)
{
    /* One bounded pass finds the orientation reference for this worksheet. */
    const json_t *sun = NULL;
    for (size_t i = 0; i < json_array_size(source) && i < 256u; ++i) {
        const json_t *row = json_array_get(source, i);
        if (!strcmp(lab_cal_text(row, "code"), "SUN")) {
            sun = row;
            break;
        }
    }
    static const struct {
        const char *field, *label;
    } fields[] = {{"declination", "Declination"}, {"gha", "GHA"}, {"right_ascension", "RA"},
                   {"altitude", "Altitude"}, {"azimuth", "Azimuth"}, {"semi_diameter", "s.d."},
                   {"magnitude", "Vmag."}};
    bool ok =
        !string_append_cstr(
            out, "<div class=\"almanac-table-scroll\" role=\"region\" "
                 "aria-label=\"Navigational bodies\" tabindex=\"0\"><table class=\"almanac-grid-table almanac-body-table\" "
                 "role=\"table\"><thead role=\"rowgroup\"><tr role=\"row\">"
                 "<th scope=\"col\" role=\"columnheader\">Body</th>");
    for (size_t j = 0; ok && j < sizeof fields / sizeof *fields; ++j)
        ok = string_append_format(out, "<th scope=\"col\" role=\"columnheader\">%s</th>", fields[j].label) >= 0;
    ok = ok && (visible || !string_append_cstr(out, "<th scope=\"col\" role=\"columnheader\">Visible</th>")) &&
         !string_append_cstr(out, "</tr></thead><tbody role=\"rowgroup\">");
    for (size_t i = 0; ok && i < json_array_size(rows); ++i) {
        const json_t *row = json_array_get(rows, i);
        bool reference = !strcmp(lab_cal_text(row, "kind"), "reference");
        const char *name = lab_cal_text(row, "name");
        if (!*name)
            name = lab_cal_text(row, "code");
        ok = string_append_format(out, "<tr role=\"row\" class=\"%s\"><td role=\"rowheader\" class=\"%s\">",
                                  reference ? "reference" : "",
                                  reference ? "reference-name" : "body-name") >= 0 &&
             lab_cal_body_symbol(out, row, sun, observer) && lab_cal_markup_text(out, name) &&
             !string_append_cstr(out, "</td>");
        for (size_t j = 0; ok && j < sizeof fields / sizeof *fields; ++j)
            ok = string_append_format(out, "<td role=\"cell\" class=\"number\" data-field=\"%s\" data-label=\"%s\">",
                                      fields[j].field, fields[j].label) >= 0 &&
                 lab_cal_markup_text(out, lab_cal_text(row, fields[j].field)) && !string_append_cstr(out, "</td>");
        if (!visible)
            ok = ok &&
                 string_append_format(out, "<td role=\"cell\" data-label=\"Visible\" class=\"visible-cell %s\" title=\"",
                                      lab_cal_markup_flag(row, "is_visible") ? "yes" : "no") >= 0 &&
                 lab_cal_markup_text(out, lab_cal_text(row, "visible_label")) &&
                 !string_append_cstr(out, "\"><span class=\"almanac-visible-icon\" role=\"img\" aria-label=\"") &&
                 lab_cal_markup_text(out, lab_cal_text(row, "visible_label")) && !string_append_cstr(out, "\">") &&
                 lab_cal_markup_text(out, lab_cal_text(row, "visible_icon")) &&
                 !string_append_cstr(out, "</span></td>");
        ok = ok && !string_append_cstr(out, "</tr>");
    }
    if (!json_array_size(rows))
        ok = ok && string_append_format(
                       out, "<tr role=\"row\"><td role=\"cell\" colspan=\"%u\">"
                            "No bodies found for the current visibility filter.</td></tr>",
                       visible ? 8u : 9u) >= 0;
    return ok && !string_append_cstr(out, "</tbody></table></div>");
}

static bool lab_cal_markup_events(string_t *out, const json_t *events)
{
    static const struct {
        const char *field, *label, *css, *title;
    } cells[] = {{"category", "Class", "", NULL},
                 {"name", "Event", "body-name", NULL},
                 {"kind", "Kind", "event-kind", NULL},
                 {"magnitude", "Magnitude", "number event-measure", NULL},
                 {"obscuration", "Obscuration", "number event-measure", NULL},
                 {"date_text", "Date", "number event-date", NULL},
                 {"first_text", "First", "number event-time", "first_contact"},
                 {"greatest_text", "Greatest", "number event-time", "greatest"},
                 {"fourth_text", "Fourth", "number event-time", "fourth_contact"},
                 {"gmt_text", "GMT", "number event-gmt", "gmt_time"}};
    bool ok = !string_append_cstr(
        out,
        "<div class=\"almanac-table-scroll\" role=\"region\" "
        "aria-label=\"Upcoming astronomical events\" tabindex=\"0\"><table class=\"almanac-grid-table "
        "almanac-event-table\">"
        "<thead><tr><th class=\"event-class\">Class</th><th class=\"event-name\">Event</th><th "
        "class=\"event-kind\">Kind</th>"
        "<th class=\"event-measure\" title=\"Magnitude\">Mag.</th><th class=\"event-measure\" "
        "title=\"Obscuration\">Obsc.</th>"
        "<th class=\"event-date\">Date</th><th class=\"event-time\">First</th><th class=\"event-time\">Greatest</th>"
        "<th class=\"event-time\">Fourth</th><th class=\"event-gmt\" title=\"Greatest GMT\">GMT</th>"
        "<th class=\"event-totality\">Nearest Totality</th></tr></thead><tbody>");
    for (size_t i = 0; ok && i < json_array_size(events); ++i) {
        const json_t *event = json_array_get(events, i);
        ok = !string_append_cstr(out, "<tr data-almanac-event-jd=\"") &&
             lab_cal_markup_text(out, lab_cal_text(event, "jd")) && !string_append_cstr(out, "\">");
        for (size_t j = 0; ok && j < sizeof cells / sizeof *cells; ++j) {
            ok = string_append_format(out, "<td class=\"%s\" data-label=\"%s\"", cells[j].css, cells[j].label) >= 0;
            if (j == 3u || j == 4u) {
                bool lunar = !strcmp(lab_cal_text(event, "category"), "Lunar");
                const char *measure = j == 3u
                    ? (lunar ? "Umbral magnitude; negative means the Moon misses the dark umbra"
                             : "Fraction of the Sun's diameter covered at greatest eclipse")
                    : (lunar ? "Moon area inside the dark umbra; excludes penumbral dimming"
                             : "Sun area covered by the Moon at greatest eclipse");
                ok = ok && !string_append_cstr(out, " title=\"") && lab_cal_markup_text(out, measure) &&
                     !string_append_char(out, '"');
            }
            if (cells[j].title) {
                const char *title = lab_cal_text(event, cells[j].title);
                if (j == 7u && !*title)
                    title = lab_cal_text(event, "time");
                ok = ok && !string_append_cstr(out, " title=\"") && lab_cal_markup_text(out, title) &&
                     !string_append_char(out, '"');
            }
            ok = ok && !string_append_char(out, '>');
            if (j == 1u)
                ok = ok && !string_append_cstr(out, "<span class=\"almanac-event-name\">") &&
                     lab_cal_eclipse_symbol(out, event) &&
                     !string_append_cstr(out, "<span class=\"almanac-event-label\">");
            ok = ok && lab_cal_markup_text(out, lab_cal_text(event, cells[j].field)) &&
                 (j != 1u || !string_append_cstr(out, "</span></span>")) &&
                 !string_append_cstr(out, "</td>");
        }
        ok = ok && !string_append_cstr(out, "<td class=\"event-details\" data-label=\"Nearest totality\"");
        if (lab_cal_markup_flag(event, "needs_land_search"))
            ok = ok && !string_append_cstr(out, " data-almanac-land-totality=\"") &&
                 lab_cal_markup_text(out, lab_cal_text(event, "jd")) && !string_append_char(out, '"');
        string_t *totality =
            lab_cal_totality_markup(lab_cal_text(event, "nearest_text"), lab_cal_get(event, "nearest_totality_action"));
        ok = ok && totality && !string_append_char(out, '>') && !string_append_string(out, totality) &&
             !string_append_cstr(out, "</td></tr>");
        string_free(totality);
    }
    if (!json_array_size(events))
        ok =
            ok && !string_append_cstr(out, "<tr><td colspan=\"11\">No eclipses or Mercury/Venus transits found in this "
                                           "one-year window.</td></tr>");
    return ok && !string_append_cstr(out, "</tbody></table></div>");
}

/* Publish only complete markup; callers own and release the returned string. */
string_t *lab_cal_almanac_markup(const json_t *response, const json_t *rows, const json_t *events, bool visible)
{
    if (json_array_size(rows) > 256u || json_array_size(events) > 64u)
        return NULL;
    string_t *out = string_new();
    const char *title = lab_cal_text(response, "worksheet_title");
    if (!*title)
        title = "AstroNav Navigation Almanac";
    bool ok =
        out &&
        !string_append_cstr(out, "<div class=\"almanac-sheet\"><div class=\"almanac-sheet-header\">"
                                 "<div class=\"almanac-sheet-title\">") &&
        lab_cal_markup_text(out, title) && !string_append_cstr(out, "</div><div>") &&
        lab_cal_markup_text(out, lab_cal_text(response, "moment_text")) && !string_append_cstr(out, "</div><div>") &&
        lab_cal_markup_text(out, lab_cal_text(response, "observer_text")) &&
        string_append_format(out, "</div><div>Location of Navigational Bodies; %s</div>",
                             visible ? "visible bodies only" : "all bodies shown") >= 0 &&
        !string_append_cstr(
            out,
            "<div class=\"almanac-sheet-toolbar\" aria-label=\"Body list filter\">"
            "<span class=\"almanac-sheet-toolbar-label\">Body list</span><span class=\"almanac-visibility-toggle\" "
            "role=\"group\" aria-label=\"Body list filter\">");
    static const char *const modes[] = {"all", "visible"}, *const labels[] = {"All", "Visible"};
    for (unsigned i = 0; ok && i < 2u; ++i) {
        bool active = visible == (i == 1u);
        ok = string_append_format(out,
                                  "<button type=\"button\" class=\"%s\" data-almanac-visibility=\"%s\" "
                                  "aria-pressed=\"%s\">%s</button>",
                                  active ? "active" : "", modes[i], active ? "true" : "false", labels[i]) >= 0;
    }
    const json_t *source = lab_cal_get(response, "all_rows");
    if (!source)
        source = lab_cal_get(response, "rows");
    ok = ok && !string_append_cstr(out, "</span></div></div>") &&
         lab_cal_markup_bodies(out, rows, source ? source : rows, response, visible) &&
         !string_append_cstr(out, "<div class=\"almanac-events-title\">") &&
         lab_cal_markup_text(out, lab_cal_text(response, "event_title")) && !string_append_cstr(out, "</div>") &&
         lab_cal_markup_events(out, events) && !string_append_cstr(out, "</div>");
    if (!ok) {
        string_free(out);
        return NULL;
    }
    return out;
}
