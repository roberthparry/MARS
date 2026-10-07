/**
 * @file datetime_arithmetic.c
 * @brief Calendar arithmetic, spans and elapsed durations.
 *
 * Adds civil units and time intervals, refreshes cached Julian values and computes differences. This is the
 * implementation boundary for changing a date without leaving its civil fields and caches inconsistent.
 *
 * This is part of the datetime.h implementation. Jurisdiction holiday policy belongs to the jurisdiction module,
 * while catalogue-backed apparent sky positions belong to almanac.
 */

/* Calendar arithmetic, spans and durations. */
#include <float.h>
#include <limits.h>
#include <math.h>

#include "datetime_internal.h"

/* Invalidate Julian caches and recalculate from the current civil fields. */
static void datetime_refresh_julian_caches(datetime_t *dttm)
{
    if (dttm->JulianDay != DBL_MAX || dttm->JulianDayNumber != LONG_MAX) {
        dttm->JulianDay = DBL_MAX;
        dttm->JulianDayNumber = LONG_MAX;
        datetime_jd(dttm);
    }
}

/* Add a number of days to a datetime and return the current datetime. */
datetime_t *datetime_add_days(datetime_t *dttm, long days)
{
    if (days == 0)
        return dttm; // No change needed
    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
        // add days to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
            return NULL;

        if (dttm->JulianDay != DBL_MAX)
            dttm->JulianDay += (double)days;
        if (dttm->JulianDayNumber != LONG_MAX)
            dttm->JulianDayNumber += days;
        return dttm;
    }

    dttm->JulianDay = DBL_MAX;
    dttm->JulianDayNumber = LONG_MAX;

    uint8_t hour = dttm->hour;
    uint8_t minute = dttm->minute;
    double second = dttm->second;
    double jdn = datetime_jd(dttm);
    datetime_init_jd(dttm, jdn + (double)days);
    datetime_year(dttm); // This will fill in the year, month, day based on the new Julian Day

    dttm->hour = hour;
    dttm->minute = minute;
    dttm->second = second;

    return dttm;
}

/* Add a number of weeks to a datetime and return the current datetime. */
datetime_t *datetime_add_weeks(datetime_t *dttm, int weeks)
{
    return datetime_add_days(dttm, (long)weeks * 7L);
}

/* Add a number of months to a datetime and return the current datetime. */
datetime_t *datetime_add_months(datetime_t *dttm, int months)
{
    if (months == 0)
        return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
        // add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
            return NULL;
        datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised
    }

    int years = months / 12;
    int remainingMonths = months % 12;
    if (years != 0) {
        dttm->year += years;
    }
    if (remainingMonths != 0) {
        dttm->month += remainingMonths;
        if (dttm->month > 12) {
            dttm->year++;
            dttm->month -= 12;
        } else if (dttm->month < 1) {
            dttm->year--;
            dttm->month += 12;
        }
    }

    if (dttm->day >= 29) {
        if (dttm->month == DT_February) {
            // Handle February separately because of leap years
            int maxDay = datetime_is_leap_year(dttm->year) ? 29 : 28;
            if (dttm->day > maxDay) {
                dttm->day = (uint8_t)maxDay;
            }
        } else {
            // Handle months with 30 days
            if (dttm->month == DT_April || dttm->month == DT_June || dttm->month == DT_September ||
                dttm->month == DT_November) {
                if (dttm->day > 30) {
                    dttm->day = 30;
                }
            }
        }
    }

    datetime_refresh_julian_caches(dttm);

    return dttm;
}

/* Add a number of years to a datetime and return the current datetime. */
datetime_t *datetime_add_years(datetime_t *dttm, int years)
{
    if (years == 0)
        return dttm;

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
        // add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
            return NULL;
        datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised
    }

    dttm->year += years;
    if (dttm->month == DT_February && dttm->day == 29 && !datetime_is_leap_year(dttm->year)) {
        // If we are on February 29 and the new year is not a leap year, we need to adjust the day to February 28
        dttm->day = 28;
    }

    datetime_refresh_julian_caches(dttm);
    return dttm;
}

/* Add a number of hours to a datetime and return the current datetime. */
datetime_t *datetime_add_hours(datetime_t *dttm, int hours)
{
    if (hours == 0)
        return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
        // add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
            return NULL;
        datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised
    }

    int daysToAdd = hours / 24;
    int remainingHours = hours % 24;

    int hour = dttm->hour;
    hour += remainingHours;
    if (hour >= 24) {
        hour -= 24;
        daysToAdd++;
    } else if (hour < 0) {
        hour += 24;
        daysToAdd--;
    }

    dttm->hour = hour;
    if (daysToAdd != 0) {
        datetime_add_days(dttm, daysToAdd);
    }

    datetime_refresh_julian_caches(dttm);

    return dttm;
}

/* Add a number of minutes to a datetime and return the current datetime. */
datetime_t *datetime_add_minutes(datetime_t *dttm, int minutes)
{
    if (minutes == 0)
        return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
        // add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
            return NULL;
        datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised
    }

    int hoursToAdd = minutes / 60;
    int remainingMinutes = minutes % 60;

    int minute = dttm->minute;
    minute += remainingMinutes;
    if (minute >= 60) {
        minute -= 60;
        hoursToAdd++;
    } else if (minute < 0) {
        minute += 60;
        hoursToAdd--;
    }
    dttm->minute = minute;

    if (hoursToAdd != 0) {
        datetime_add_hours(dttm, hoursToAdd);
    }

    datetime_refresh_julian_caches(dttm);

    return dttm;
}

/* Add a number of seconds to a datetime and return the current datetime. */
datetime_t *datetime_add_seconds(datetime_t *dttm, double seconds)
{
    if (fabs(seconds) < 1e-9)
        return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
        // add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
            return NULL;
        datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised
    }

    int minutesToAdd = (int)(seconds / 60.0);
    double remainingSeconds = seconds - (double)(minutesToAdd * 60);

    dttm->second += remainingSeconds;
    if (dttm->second >= 60.0) {
        dttm->second -= 60.0;
        datetime_add_minutes(dttm, 1);
    } else if (dttm->second < 0.0) {
        dttm->second += 60.0;
        datetime_add_minutes(dttm, -1);
    }

    datetime_refresh_julian_caches(dttm);

    return dttm;
}

/* Add each component of a calendar span to the datetime. */
datetime_t *datetime_add_span(datetime_t *dttm, const datetime_span_t *span)
{
    if (span == NULL)
        return dttm;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add
    // seconds to it.
    if (dttm->year == SHRT_MAX && dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
        return NULL;

    if (span->years != 0)
        dttm = datetime_add_years(dttm, (int)span->years);
    if (span->months != 0)
        dttm = datetime_add_months(dttm, (int)span->months);
    if (span->days != 0)
        dttm = datetime_add_days(dttm, (long)span->days);
    if (span->hours != 0)
        dttm = datetime_add_hours(dttm, (int)span->hours);
    if (span->minutes != 0)
        dttm = datetime_add_minutes(dttm, (int)span->minutes);
    if (span->seconds != 0)
        dttm = datetime_add_seconds(dttm, (double)span->seconds);
    return dttm;
}

/* Subtract each component of a calendar span from the datetime. */
datetime_t *datetime_sub_span(datetime_t *dttm, const datetime_span_t *span)
{
    if (span == NULL)
        return dttm;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add
    // seconds to it.
    if (dttm->year == SHRT_MAX && dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX)
        return NULL;

    if (span->years != 0)
        dttm = datetime_add_years(dttm, -(int)span->years);
    if (span->months != 0)
        dttm = datetime_add_months(dttm, -(int)span->months);
    if (span->days != 0)
        dttm = datetime_add_days(dttm, -(long)span->days);
    if (span->hours != 0)
        dttm = datetime_add_hours(dttm, -(int)span->hours);
    if (span->minutes != 0)
        dttm = datetime_add_minutes(dttm, -(int)span->minutes);
    if (span->seconds != 0)
        dttm = datetime_add_seconds(dttm, -(double)span->seconds);
    return dttm;
}

/* Return the difference in days and optionally fill a calendar span. */
double datetime_duration(const datetime_t *dttm1, const datetime_t *dttm2, datetime_span_t *span)
{
    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
    // calculate the difference.
    if (dttm1->year == SHRT_MAX && dttm1->JulianDay == DBL_MAX && dttm1->JulianDayNumber == LONG_MAX)
        return DBL_MAX;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot
    // calculate the difference.
    if (dttm2->year == SHRT_MAX && dttm2->JulianDay == DBL_MAX && dttm2->JulianDayNumber == LONG_MAX)
        return DBL_MAX;

    double jd1 = datetime_jd(dttm1);
    double jd2 = datetime_jd(dttm2);

    if (span != NULL) {
        datetime_year(dttm1); // Try to calculate the year, month, day, ... if it is not initialised
        datetime_year(dttm2); // Try to calculate the year, month, day, ... if it is not initialised

        if (jd1 < jd2) {
            // If dttm1 is earlier than dttm2, we swap them to calculate the span as a positive duration, and we will
            // negate the final difference at the end.
            const datetime_t *temp = dttm1;
            dttm1 = dttm2;
            dttm2 = temp;
        }

        int years = dttm1->year - dttm2->year;
        int months = dttm1->month - dttm2->month;
        int days = dttm1->day - dttm2->day;
        int hours = dttm1->hour - dttm2->hour;
        int minutes = dttm1->minute - dttm2->minute;
        double seconds = dttm1->second - dttm2->second;

        // Normalise the span so that each component is within its normal range
        if (seconds < 0) {
            seconds += 60.0;
            minutes--;
        }
        if (minutes < 0) {
            minutes += 60;
            hours--;
        }
        if (hours < 0) {
            hours += 24;
            days--;
        }
        if (days < 0) {
            int month = dttm1->month;
            int year = dttm1->year;
            unsigned short daysInPrevMonth =
                datetime_days_in_month(year, month == DT_January ? DT_December : month - 1);
            days += daysInPrevMonth;
            months--;
        }
        if (months < 0) {
            months += 12;
            years--;
        }

        span->years = (unsigned short)years;
        span->months = (uint8_t)months;
        span->days = (uint8_t)days;
        span->hours = (uint8_t)hours;
        span->minutes = (uint8_t)minutes;
        span->seconds = seconds;
    }

    return jd1 - jd2;
}
