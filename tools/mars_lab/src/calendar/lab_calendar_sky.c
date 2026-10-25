/**
 * @file lab_calendar_sky.c
 * @brief Compact native Sun, Moon and selected-observer status markup.
 *
 * Reuses ephemeris-oriented lunar vectors from Almanac results and existing DateTime solar fields.
 * Almanac solar clocks use the same ephemeris rise/set engine as DateTime,
 * on the selected GMT date. Polar conditions remain explicit. DateTime's categorical Moon phase
 * is a schematic when precise lunar vectors are unavailable. No browser astronomy is required.
 */
#include <math.h>
#include <string.h>

#include "datetime-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_solar.h"

static bool sky_text(string_t *out, const char *label, const char *value)
{
    return !string_append_cstr(out, "<span class=\"sky-status-item\">") &&
           !string_append_cstr(out, "<span class=\"sky-status-label\">") && lab_cal_markup_text(out, label) &&
           !string_append_cstr(out, "</span><span class=\"sky-status-value\">") && lab_cal_markup_text(out, value) &&
           !string_append_cstr(out, "</span></span>");
}

static bool sky_solar(string_t *out, const almanac_rise_set_event_t *event, const string_t *zone, bool rising)
{
    string_t *clock = lab_cal_solar_clock(event, zone);
    if (clock && event->status == ALMANAC_RISE_SET_OK) string_append_cstr(clock, " GMT");
    bool ok = clock && sky_text(out, rising ? "Sunrise " : "Sunset ", string_c_str(clock));
    string_free(clock);
    return ok;
}

static bool sky_moon(string_t *out, const json_t *response, bool almanac)
{
    const json_t *rows = lab_cal_get(response, "all_rows"), *moon = NULL, *sun = NULL;
    for (size_t i = 0; almanac && i < json_array_size(rows) && i < 256; ++i) {
        const json_t *row = json_array_get(rows, i);
        const char *code = lab_cal_text(row, "code");
        if (!strcmp(code, "MOON")) moon = row;
        if (!strcmp(code, "SUN")) sun = row;
    }
    if (moon && sun) return lab_cal_body_symbol(out, moon, sun, response);
    const char *phase = lab_cal_text(lab_cal_get(response, "fields"), "moon_phase");
    /* Eight named calendar categories are deliberately schematic, not invented precise illumination. */
    static const char *const glyphs[] = {"🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘"};
    for (unsigned i = 0; !almanac && i < 8; ++i)
        if (!strcmp(phase, datetime_moon_phase_name((moon_phase_t)i)))
            return !string_append_cstr(out, "<span class=\"sky-status-moon\" role=\"img\" aria-label=\"") &&
                   lab_cal_markup_text(out, phase) && !string_append_cstr(out, " (schematic)\">") &&
                   !string_append_cstr(out, glyphs[i]) && !string_append_cstr(out, "</span>");
    return sky_text(out, "Moon ", "unavailable");
}

static bool sky_flag(string_t *out, const json_t *response)
{
    const json_t *summary = lab_cal_get(response, "observer_summary");
    for (size_t i = 0; i < json_array_size(summary) && i < 10; ++i) {
        const json_t *cell = json_array_get(summary, i);
        const char *flag = lab_cal_text(cell, "flag");
        if (!strcmp(lab_cal_text(cell, "label"), "Location") &&
            (!strncmp(flag, "data:image/svg+xml;base64,", 26) || !strcmp(flag, "/offshore_flag.svg")))
            return !string_append_cstr(out, "<img class=\"country-flag\" alt=\"") &&
                   lab_cal_markup_text(out, lab_cal_text(cell, "value")) &&
                   !string_append_cstr(out, "\" src=\"") && lab_cal_markup_text(out, flag) &&
                   !string_append_cstr(out, "\">");
    }
    return true;
}

/* Keep the strip's ordering and all variable markup under native presentation ownership. */
string_t *lab_cal_sky_markup(const json_t *response, bool almanac)
{
    const json_t *fields = lab_cal_get(response, "fields");
    string_t *out = string_new_with("<span class=\"sky-status-template\" hidden><span class=\"sky-status-strip\">"
                                    "<span class=\"sky-status-events\">");
    bool ok = out && sky_moon(out, response, almanac);
    if (almanac) {
        datetime_t *date = datetime_from_string(lab_cal_text(fields, "date"));
        string_t *zone = string_new_with("Etc/UTC");
        almanac_observer_t observer = {.latitude_degrees = NAN, .longitude_degrees = NAN};
        bool positioned = lab_cal_number(lab_cal_text(response, "observer_latitude"), &observer.latitude_degrees) &&
                          lab_cal_number(lab_cal_text(response, "observer_longitude"), &observer.longitude_degrees);
        const char *elevation = lab_cal_text(response, "observer_elevation");
        if (*elevation) positioned = lab_cal_number(elevation, &observer.elevation_metres) && positioned;
        almanac_t *engine = positioned ? almanac_open() : NULL;
        almanac_sun_times_t solar;
        lab_cal_solar_times(engine, lab_cal_text(fields, "jurisdiction"), date, &observer, zone, &solar);
        ok = ok && sky_solar(out, &solar.set, zone, false) && sky_solar(out, &solar.rise, zone, true);
        almanac_close(engine); string_free(zone); datetime_dealloc(date);
        ok = ok && !string_append_cstr(out, "</span><span class=\"sky-status-selection\">");
        const char *time = lab_cal_text(fields, "time");
        string_t *moment = string_sprintf("%s %.*s GMT", lab_cal_text(fields, "date"), (int)strcspn(time, "."), time);
        ok = ok && moment && sky_text(out, "Selected time", string_c_str(moment)) && sky_flag(out, response);
        string_free(moment);
    } else {
        const char *set = lab_cal_text(fields, "sunset"), *rise = lab_cal_text(fields, "sunrise");
        ok = ok && sky_text(out, "Sunset ", *set ? set : "unavailable") &&
             sky_text(out, "Sunrise ", *rise ? rise : "unavailable") &&
             !string_append_cstr(out, "</span><span class=\"sky-status-selection\">") &&
             sky_text(out, "Selected date", lab_cal_text(fields, "date"));
    }
    ok = ok && !string_append_cstr(out, "</span></span></span>");
    if (!ok) { string_free(out); return NULL; }
    return out;
}
