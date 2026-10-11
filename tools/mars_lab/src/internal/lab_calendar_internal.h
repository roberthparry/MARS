/**
 * @file lab_calendar_internal.h
 * @brief Controlled white-box test access to native Lab calendar helpers.
 *
 * Only calendar regression tests should use this façade. Application modules
 * must use lab_calendar.h; the implementation details belong to calendar/.
 */
#ifndef MARS_LAB_CALENDAR_TEST_INTERNAL_H
#define MARS_LAB_CALENDAR_TEST_INTERNAL_H

#if !defined(MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS."
#endif
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "../calendar/lab_calendar_internal.h"
#endif
