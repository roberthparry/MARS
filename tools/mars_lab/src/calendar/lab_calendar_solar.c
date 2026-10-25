/**
 * @file lab_calendar_solar.c
 * @brief Ephemeris solar clocks shared by native sky strips.
 *
 * Reuses the DateTime rise/set engine, including topocentric position, apparent
 * solar radius, standard horizon refraction and observer elevation. Jurisdiction
 * rules define search days; event instants are then matched to the requested IANA
 * civil date, so a town need not share its jurisdiction's representative zone.
 * Adjacent search days cover UTC/local date boundaries. No approximate fallback
 * is published when ephemerides or jurisdiction data are unavailable.
 */
#include <math.h>

#include "jurisdiction-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_solar.h"

/* Share the accurate native solver while retaining the caller's civil-day definition. */
bool lab_cal_solar_times(almanac_t *almanac, const char *code, const datetime_t *date,
                         const almanac_observer_t *observer, const string_t *zone, almanac_sun_times_t *times)
{
    if (!times) return false;
    *times = (almanac_sun_times_t){.rise.status = ALMANAC_RISE_SET_UNAVAILABLE,
                                  .set.status = ALMANAC_RISE_SET_UNAVAILABLE};
    if (!almanac || !date || !observer || !zone) return false;
    jurisdiction_t *jurisdiction = jurisdict_open(code);
    datetime_t *candidate = datetime_alloc();
    bool evaluated = false;
    static const int days[] = {0, -1, 1};
    for (unsigned i = 0; jurisdiction && candidate && i < 3; ++i) {
        almanac_sun_times_t found;
        if (!datetime_init_jdn(candidate, datetime_jdn(date) + days[i]) ||
            !almanac_sunrise_sunset(almanac, jurisdiction, candidate, observer, &found)) continue;
        evaluated = true;
        const almanac_rise_set_event_t *source[] = {&found.rise, &found.set};
        almanac_rise_set_event_t *target[] = {&times->rise, &times->set};
        for (unsigned event = 0; event < 2; ++event) {
            if (target[event]->status == ALMANAC_RISE_SET_OK) continue;
            if (i == 0) target[event]->status = source[event]->status == ALMANAC_RISE_SET_OK ?
                ALMANAC_RISE_SET_NOT_ON_DATE : source[event]->status;
            struct tm local = {0};
            double offset;
            if (source[event]->status == ALMANAC_RISE_SET_OK &&
                lab_cal_timezone_at(zone, source[event]->time.jd, &local, &offset) &&
                local.tm_year + 1900 == datetime_year(date) && local.tm_mon + 1 == (int)datetime_month(date) &&
                local.tm_mday == datetime_day(date)) *target[event] = *source[event];
        }
        if (times->rise.status == ALMANAC_RISE_SET_OK && times->set.status == ALMANAC_RISE_SET_OK) break;
    }
    datetime_dealloc(candidate); jurisdict_close(jurisdiction);
    return evaluated;
}

/* Format only computed event instants; missing data and polar states stay explicit. */
string_t *lab_cal_solar_clock(const almanac_rise_set_event_t *event, const string_t *zone)
{
    static const char *const states[] = {
        [ALMANAC_RISE_SET_OK] = "unavailable",
        [ALMANAC_RISE_SET_NOT_ON_DATE] = "not on this date",
        [ALMANAC_RISE_SET_NEVER_RISES] = "polar night",
        [ALMANAC_RISE_SET_NEVER_SETS] = "midnight sun",
        [ALMANAC_RISE_SET_UNAVAILABLE] = "unavailable"
    };
    if (!event || event->status < ALMANAC_RISE_SET_OK || event->status > ALMANAC_RISE_SET_UNAVAILABLE)
        return string_new_with("unavailable");
    struct tm local = {0};
    double offset;
    if (event->status != ALMANAC_RISE_SET_OK || !isfinite(event->time.jd) ||
        !lab_cal_timezone_at(zone, floor((event->time.jd - 2440587.5) * 1440 + 0.5) / 1440 + 2440587.5,
                             &local, &offset)) return string_new_with(states[event->status]);
    char clock[16];
    return strftime(clock, sizeof clock, "%H:%M", &local) ? string_new_with(clock) : NULL;
}
