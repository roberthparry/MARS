/**
 * @file lab_calendar_eclipses.c
 * @brief Native SVG disc-overlap previews at greatest solar and lunar eclipse.
 *
 * Consumes the event worker's angular radii, separation and observer-relative
 * orientation. Renders real circular overlaps, including annular and penumbral
 * cases, without estimating geometry from rounded magnitude or obscuration.
 * Colours are schematic: atmospheric scattering and lunar surface detail are
 * not predicted. Missing geometry never produces a guessed eclipse shape.
 */
#include <math.h>
#include <string.h>

#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static bool lab_cal_eclipse_overlap(string_t *out, double radius, double distance, const char *colour)
{
    const double disc = 10;
    if (distance >= disc + radius || radius <= 0)
        return true;
    if (radius >= distance + disc)
        return string_append_format(out, "<circle r=\"10\" fill=\"%s\"/>", colour) >= 0;
    if (disc >= distance + radius)
        return string_append_format(out, "<circle cx=\"%.9f\" r=\"%.9f\" fill=\"%s\"/>",
                                    distance, radius, colour) >= 0;
    double x = (distance * distance + disc * disc - radius * radius) / (2 * distance);
    x = fmax(-disc, fmin(disc, x));
    double y = sqrt(fmax(0, disc * disc - x * x));
    return string_append_format(out,
        "<path fill=\"%s\" d=\"M%.9f %.9f A10 10 0 %d 1 %.9f %.9f "
        "A%.9f %.9f 0 %d 1 %.9f %.9f Z\"/>", colour, x, -y, x < 0, x, y,
        radius, radius, x > distance, x, -y) >= 0;
}

/* Render only recognised eclipses with finite native geometry; event names remain independent. */
bool lab_cal_eclipse_symbol(string_t *out, const json_t *event)
{
    bool lunar = !strcmp(lab_cal_text(event, "category"), "Lunar") &&
                 !strcmp(lab_cal_text(event, "name"), "Lunar eclipse");
    bool solar = !strcmp(lab_cal_text(event, "category"), "Solar") &&
                 !strcmp(lab_cal_text(event, "name"), "Solar eclipse");
    double disc, cover, distance, penumbra = 0, orientation;
    if ((!lunar && !solar) || !lab_cal_number(lab_cal_text(event, "disc_radius"), &disc) || disc <= 0 ||
        !lab_cal_number(lab_cal_text(event, "cover_radius"), &cover) || cover < 0 ||
        !lab_cal_number(lab_cal_text(event, "centre_separation"), &distance) || distance < 0 ||
        (lunar && (!lab_cal_number(lab_cal_text(event, "penumbra_radius"), &penumbra) || penumbra < cover)))
        return true;
    cover = 10 * (cover / disc);
    distance = 10 * (distance / disc);
    penumbra = 10 * (penumbra / disc);
    if (!isfinite(cover) || !isfinite(distance) || !isfinite(penumbra) ||
        cover > 10000 || distance > 10000 || penumbra > 10000)
        return true;
    bool oriented = lab_cal_number(lab_cal_text(event, "orientation_degrees"), &orientation) &&
                    fabs(orientation) <= 180;
    const char *frame = oriented ? "local zenith up" : "orientation unavailable; schematic alignment";
    bool ok = string_append_format(out,
        "<svg class=\"almanac-eclipse-symbol\" viewBox=\"0 0 24 24\" role=\"img\" "
        "aria-label=\"%s eclipse at greatest eclipse; %s\">"
        "<title>%s eclipse at greatest eclipse; %s. Colours are schematic.</title>"
        "<g transform=\"translate(12 12) rotate(%.9f)\"><circle r=\"10\" fill=\"%s\"/>",
        lunar ? "Lunar" : "Solar", frame, lunar ? "Lunar" : "Solar", frame,
        oriented ? orientation - 90 : 0, lunar ? "#b7c3cc" : "#efc36a") >= 0;
    if (lunar)
        ok = ok && lab_cal_eclipse_overlap(out, penumbra, distance, "#747a80");
    ok = ok && lab_cal_eclipse_overlap(out, cover, distance, lunar ? "#844e48" : "#19232a") &&
         !string_append_cstr(out, "<circle r=\"10\" fill=\"none\" stroke=\"#7e8b94\" "
                                 "stroke-width=\".5\"/></g></svg>");
    return ok;
}
