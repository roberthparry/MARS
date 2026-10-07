/**
 * @file datetime_internal.h
 * @brief Private datetime representation and civil-date helpers.
 *
 * Defines civil fields and lazy Julian caches and declares internal initialisation and conversion helpers.
 * Implementation units must keep cache sentinels and civil values consistent; callers outside the module use
 * opaque datetime_t handles.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef MARS_DATETIME_INTERNAL_H
#define MARS_DATETIME_INTERNAL_H

#include <stddef.h>

#include "datetime.h"

/*
 * Private datetime state; callers outside this module use the opaque public API.
 * SHRT_MAX marks an unset year, LONG_MAX an unset JDN and DBL_MAX an unset JD.
 * Julian caches and civil fields are materialised lazily by the accessors.
 * JDN records the civil day; the fractional JD also carries the time of day.
 */
typedef struct _datetime_t {
    short year;
    month_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    double second;
    long JulianDayNumber;
    double JulianDay;
} datetime_t;

/* Initialise from JDN and materialise the civil fields for observance constructors. */
datetime_t *datetime_init_materialised_jdn(datetime_t *dttm, long jdn);

#endif
