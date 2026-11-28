/**
 * @file datetime_calendar_internal.h
 * @brief Controlled calendar-component access for jurisdiction date presentation.
 *
 * Shares native tabular conversions without exposing datetime storage or parsing rendered dates. Ordinary callers
 * should use datetime-t.h. Define MARS_SHARED_DATETIME_CALENDAR_INTERNAL_ACCESS immediately before inclusion.
 */
#ifndef MARS_SHARED_DATETIME_CALENDAR_INTERNAL_H
#define MARS_SHARED_DATETIME_CALENDAR_INTERNAL_H
#if !defined(MARS_SHARED_DATETIME_CALENDAR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_DATETIME_CALENDAR_INTERNAL_ACCESS."
#endif

#include "datetime-t.h"

/**
 * @brief Resolve civil Islamic components using the native tabular calendar.
 * @param value Borrowed civil datetime; lazy caches require synchronisation when shared.
 * @param year Output year, required.
 * @param month Output month in 1..12, required.
 * @param day Output day, required.
 * @return True on success, false for missing arguments or an invalid Julian day; transfers no ownership.
 */
bool datetime_islamic_components(const datetime_t *value, int *year, int *month, int *day);

/**
 * @brief Resolve fixed Hebrew components using the native postponement and leap-year rules.
 * @param value Borrowed civil datetime; lazy caches require synchronisation when shared.
 * @param year Output year, required.
 * @param month Output stable Tishrei-based month slot in 1..13; slot 6 is absent in common years.
 * @param day Output day, required.
 * @return True on success, false for missing arguments or an invalid date; transfers no ownership.
 */
bool datetime_hebrew_components(const datetime_t *value, int *year, int *month, int *day);
#endif
