/**
 * @file lab_calendar_symbols.c
 * @brief Native SVG astronomical symbols and illuminated lunar discs for the Almanac table.
 *
 * Uses catalogue identifiers for direct vector symbol lookup, without font or bitmap dependencies. The phase angle
 * controls the terminator; apparent equatorial coordinates orient the bright
 * limb towards the Sun. The observer's parallactic angle turns the disc into
 * the local horizon frame, with the zenith upwards, continuously across both
 * hemispheres and the equator. This schematic omits lunar terrain and refraction.
 * Missing coordinates retain the ordinary Moon symbol rather than inventing a phase.
 * Catalogue stars use a bounded, schematic magnitude-to-size mapping within a
 * fixed icon slot; larger stars indicate greater apparent brightness, not angular diameter.
 */
#include <math.h>

#include "almanac-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static bool lab_cal_moon_vertical(const json_t *row, const json_t *observer, double dec, double *angle)
{
    double latitude, longitude, gha;
    if (!lab_cal_number(lab_cal_text(observer, "observer_latitude"), &latitude) || fabs(latitude) > 90 ||
        !lab_cal_number(lab_cal_text(observer, "observer_longitude"), &longitude) || fabs(longitude) > 180 ||
        !lab_cal_number(lab_cal_text(row, "gha_degrees"), &gha) || gha < 0 || gha >= 360)
        return false;
    double radians = acos(-1.0) / 180.0;
    double phi = latitude * radians, hour_angle = (gha + longitude) * radians;
    double east = cos(phi) * sin(hour_angle);
    double north = sin(phi) * cos(dec) - cos(phi) * sin(dec) * cos(hour_angle);
    /* At the zenith/nadir the local vertical direction on the disc is undefined. */
    if (hypot(east, north) < 1e-12)
        return false;
    *angle = atan2(east, north);
    return true;
}

static bool lab_cal_moon_symbol(string_t *out, const json_t *row, const json_t *sun, const json_t *observer)
{
    double phase, ra, dec, sun_ra, sun_dec;
    if (!lab_cal_number(lab_cal_text(row, "phase"), &phase) || phase < 0 || phase > 180 ||
        !lab_cal_number(lab_cal_text(row, "right_ascension_hours"), &ra) ||
        !lab_cal_number(lab_cal_text(row, "declination_degrees"), &dec) ||
        !lab_cal_number(lab_cal_text(sun, "right_ascension_hours"), &sun_ra) ||
        !lab_cal_number(lab_cal_text(sun, "declination_degrees"), &sun_dec) ||
        ra < 0 || ra >= 24 || sun_ra < 0 || sun_ra >= 24 || fabs(dec) > 90 || fabs(sun_dec) > 90)
        return !string_append_cstr(out, "<svg class=\"almanac-body-symbol\" data-body=\"MOON\" "
                                       "viewBox=\"0 0 24 26\" aria-hidden=\"true\"><title>☾</title>"
                                       "<path d=\"M17 2 A11 11 0 1 0 17 24 A12 12 0 0 1 17 2 Z\" "
                                       "fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.8\"/></svg>");

    const double radians = acos(-1.0) / 180.0;
    double cosine = cos(phase * radians), illuminated = (1 + cosine) / 2;
    double delta = (sun_ra - ra) * 15 * radians;
    double angle = atan2(cos(sun_dec * radians) * sin(delta),
                         sin(sun_dec * radians) * cos(dec * radians) -
                         cos(sun_dec * radians) * sin(dec * radians) * cos(delta));
    double rotation = -90 - angle / radians;
    double vertical;
    bool local = lab_cal_moon_vertical(row, observer, dec * radians, &vertical);
    if (local)
        rotation += vertical / radians;
    const char *orientation = local ? "local zenith up" : "celestial north up; local orientation unavailable";
    /* The outer semicircle and elliptical terminator enclose the lit fraction. */
    return string_append_format(
               out, "<svg class=\"almanac-body-symbol almanac-moon-phase\" data-body=\"MOON\" viewBox=\"0 0 24 24\" "
                    "role=\"img\" aria-label=\"Moon: %.1f%% illuminated; %s\">"
                    "<title>Moon: %.1f%% illuminated; %s</title>"
                    "<circle class=\"almanac-moon-shadow\" cx=\"12\" cy=\"12\" r=\"10\" fill=\"#41494f\"/>"
                    "<path fill=\"currentColor\" transform=\"rotate(%.6f 12 12)\" "
                    "d=\"M12 2 A10 10 0 0 1 12 22 A%.6f 10 0 0 %d 12 2 Z\"/>"
                    "<circle class=\"almanac-moon-rim\" cx=\"12\" cy=\"12\" r=\"10\" fill=\"none\" stroke=\"#7e8b94\" "
                    "stroke-width=\".7\"/></svg>",
               illuminated * 100, orientation, illuminated * 100, orientation, rotation,
               fabs(cosine) * 10, cosine >= 0 ? 1 : 0) >= 0;
}

static bool lab_cal_star_symbol(string_t *out, const json_t *row, almanac_body_id_t id)
{
    const char *value = lab_cal_text(row, "visual_magnitude");
    if (!lab_cal_get(row, "visual_magnitude"))
        value = lab_cal_text(row, "magnitude");
    double magnitude;
    bool known = lab_cal_number(value, &magnitude);
    /* Compress the magnitude range for legibility, keeping table columns aligned. */
    double radius = known ? fmax(3, fmin(10, 8 - magnitude)) : 6;
    string_t *label = known ? string_sprintf("Star: visual magnitude %.2f; larger symbols mean brighter stars", magnitude)
                            : string_new_with("Star: visual magnitude unavailable; default size");
    bool ok = label && string_append_format(
        out, "<svg class=\"almanac-body-symbol almanac-star-symbol\" data-body=\"%s\" "
             "viewBox=\"0 0 24 26\" role=\"img\" aria-label=\"%s\"><title>%s</title>"
             "<path fill=\"currentColor\" transform=\"translate(12 13) scale(%.6f)\" "
             "d=\"M0 -10 L2.35 -3.24 L9.51 -3.09 L3.80 1.24 L5.88 8.09 L0 4 "
             "L-5.88 8.09 L-3.80 1.24 L-9.51 -3.09 L-2.35 -3.24 Z\"/></svg>",
        almanac_body_code(id), string_c_str(label), string_c_str(label), radius / 10) >= 0;
    string_free(label);
    return ok;
}

/* Keep glyph selection native and independent of translated display names. */
bool lab_cal_body_symbol(string_t *out, const json_t *row, const json_t *sun, const json_t *observer)
{
    static const char sun_shape[] =
        "<circle cx='12' cy='13' r='10'/><circle cx='12' cy='13' r='2' fill='currentColor'/>";
    static const char mercury_shape[] =
        "<circle cx='12' cy='11' r='5'/><path d='M7 2 A5 5 0 0 0 17 2 M12 16V24 M8 20H16'/>";
    static const char venus_shape[] = "<circle cx='12' cy='8' r='6'/><path d='M12 14V24 M7 20H17'/>";
    static const char mars_shape[] = "<circle cx='9' cy='16' r='7'/><path d='M14 11L22 3 M16 3H22V9'/>";
    static const char jupiter_shape[] = "<path d='M4 8C4 1 13 1 13 8C13 13 8 17 3 19H21 M17 2V24'/>";
    static const char saturn_shape[] = "<path d='M8 2V24 M4 4H12 M8 13C15 7 20 12 15 18L12 22Q11 24 14 24'/>";
    static const char uranus_shape[] =
        "<circle cx='12' cy='17' r='7'/><path d='M12 10V2 M9 5L12 2L15 5'/>"
        "<circle cx='12' cy='17' r='1' fill='currentColor'/>";
    static const char neptune_shape[] =
        "<path d='M12 2V24 M7 21H17 M4 5V12Q4 17 12 17Q20 17 20 12V5 "
        "M1 7L4 2L7 7 M9 6L12 1L15 6 M17 7L20 2L23 7'/>";
    static const struct {
        const char *symbol, *geometry;
    } symbols[ALMANAC_BODY_ID_NEPTUNE + 1] = {
        [ALMANAC_BODY_ID_SUN]     = {"☉", sun_shape},
        [ALMANAC_BODY_ID_MERCURY] = {"☿", mercury_shape},
        [ALMANAC_BODY_ID_VENUS]   = {"♀", venus_shape},
        [ALMANAC_BODY_ID_MARS]    = {"♂", mars_shape},
        [ALMANAC_BODY_ID_JUPITER] = {"♃", jupiter_shape},
        [ALMANAC_BODY_ID_SATURN]  = {"♄", saturn_shape},
        [ALMANAC_BODY_ID_URANUS]  = {"⛢", uranus_shape},
        [ALMANAC_BODY_ID_NEPTUNE] = {"♆", neptune_shape}
    };
    almanac_body_id_t id = almanac_body_id_from_code(lab_cal_text(row, "code"));
    if (id == ALMANAC_BODY_ID_MOON)
        return lab_cal_moon_symbol(out, row, sun, observer);
    if (id >= ALMANAC_BODY_ID_ACAMAR && id < ALMANAC_BODY_ID_COUNT)
        return lab_cal_star_symbol(out, row, id);
    if (id < ALMANAC_BODY_ID_SUN || id > ALMANAC_BODY_ID_NEPTUNE || !symbols[id].geometry)
        return true;
    return string_append_format(out, "<svg class=\"almanac-body-symbol\" data-body=\"%s\" "
                                    "viewBox=\"0 0 24 26\" aria-hidden=\"true\"><title>%s</title>"
                                    "<g fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.8\" "
                                    "stroke-linecap=\"round\" stroke-linejoin=\"round\">%s</g></svg>",
                                almanac_body_code(id), symbols[id].symbol, symbols[id].geometry) >= 0;
}
