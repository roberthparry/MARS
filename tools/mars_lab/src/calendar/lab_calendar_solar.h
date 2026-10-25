/**
 * @file lab_calendar_solar.h
 * @brief Private ephemeris solar-clock helpers for calendar sky renderers.
 *
 * Shares civil-date matching and missing-event presentation within the calendar
 * adapter and its authorised native tests. External callers use lab_calendar.h;
 * general astronomical consumers use the public almanac API.
 */
#ifndef MARS_LAB_CALENDAR_SOLAR_H
#define MARS_LAB_CALENDAR_SOLAR_H

#if !defined(MARS_LAB_CALENDAR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_CALENDAR_INTERNAL_ACCESS."
#endif

#include "almanac-t.h"

/** @brief Find ephemeris solar events on the supplied date in an IANA zone, without an approximate fallback. */
bool lab_cal_solar_times(almanac_t *almanac, const char *jurisdiction, const datetime_t *date,
                         const almanac_observer_t *observer, const string_t *zone, almanac_sun_times_t *times);

/** @brief Return an owned local HH:MM clock rounded to the nearest minute, or an explicit missing-event status. */
string_t *lab_cal_solar_clock(const almanac_rise_set_event_t *event, const string_t *zone);

#endif
