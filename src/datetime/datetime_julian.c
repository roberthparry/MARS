/**
 * @file datetime_julian.c
 * @brief Julian conversion, lazy civil fields and validation.
 *
 * Converts between civil dates and Julian day representations, exposes date components and validates calendar
 * ranges. Shared day and weekday queries use these conversions to avoid inconsistent calendar arithmetic.
 *
 * This is part of the datetime.h implementation. Jurisdiction holiday policy belongs to the jurisdiction module,
 * while catalogue-backed apparent sky positions belong to almanac.
 */

/* Julian-date conversion, lazy field access and civil calendar validation. */
#include <float.h>
#include <limits.h>

#include "datetime_internal.h"

/* Divide by a positive denominator using floor division for a negative numerator. */
static inline long ldivide(long numerator, long denominator) {
    return (numerator >= 0L) ? (numerator / denominator) : ((numerator - denominator + 1L) / denominator);
}

/* Convert JDN to calendar fields, applying the Gregorian reform and omitting year zero. */
static void date_julianDayNumToMDY(long julianDay, int *monthOut, int *dayOut, int *yearOut)
{
    long gregorianOffset = ldivide(100L * julianDay - 186721625L, 3652425L);
    long correctedJDN =
        (julianDay < 2299161L) ? julianDay : (julianDay + 1L + gregorianOffset - ldivide(gregorianOffset, 4L));
    long shiftedDay = correctedJDN + 1524L;
    long centuryIndex = ldivide(100L * shiftedDay - 12210L, 36525L);
    long dayOfCentury = ldivide(36525L * centuryIndex, 100L);
    long monthIndex = ldivide((shiftedDay - dayOfCentury) * 10000L, 306001L);
    *dayOut = (int)(shiftedDay - dayOfCentury - ldivide(306001L * monthIndex, 10000L));
    *monthOut = (int)((monthIndex < 14L) ? (monthIndex - 1L) : (monthIndex - 13L));
    *yearOut = (int)((*monthOut > 2) ? (centuryIndex - 4716L) : (centuryIndex - 4715L));

    // Adjust for missing year zero
    if (*yearOut <= 0)
        (*yearOut)--;
}

/* Get the year of a datetime. */
short datetime_year(const datetime_t *dttm)
{
    if (dttm == NULL)
        return SHRT_MAX;
    if (dttm->year != SHRT_MAX)
        return dttm->year;
    if (dttm->JulianDay != DBL_MAX) {
        // Convert Julian Day to calendar date and return the year.
        datetime_t *mutable_dttm = (datetime_t *)dttm; // Cast away const to store the calculated year
        mutable_dttm->JulianDayNumber = (long)(dttm->JulianDay + 0.5);
        int month, day, year;
        date_julianDayNumToMDY(mutable_dttm->JulianDayNumber, &month, &day, &year);
        mutable_dttm->year = (short)year;
        mutable_dttm->month = (month_t)month;
        mutable_dttm->day = (uint8_t)day;
        double fractional_day = dttm->JulianDay - (double)mutable_dttm->JulianDayNumber;
        double fractional_hour = fractional_day * 24.0 + 12.0; // Julian Day starts at noon
        mutable_dttm->hour = (uint8_t)fractional_hour;
        double fractional_minute = (fractional_hour - (double)mutable_dttm->hour) * 60.0;
        mutable_dttm->minute = (uint8_t)fractional_minute;
        mutable_dttm->second = (fractional_minute - (double)mutable_dttm->minute) * 60.0;
        return (short)year;
    }
    if (dttm->JulianDayNumber != LONG_MAX) {
        // Convert Julian Day Number to calendar date and return the year.
        datetime_t *mutable_dttm = (datetime_t *)dttm; // Cast away const to store the calculated year
        int month, day, year;
        date_julianDayNumToMDY(dttm->JulianDayNumber, &month, &day, &year);
        mutable_dttm->year = (short)year;
        mutable_dttm->month = (month_t)month;
        mutable_dttm->day = (uint8_t)day;
        mutable_dttm->hour = 12; // Default to noon for the time part when we only have a julian day number
        mutable_dttm->minute = 0;
        mutable_dttm->second = 0.0;
        return (short)year;
    }
    return SHRT_MAX; // This is an uninitialised state that cannot be calculated, so we return SHRT_MAX as a sentinel
                     // value.
}

/* Get the month of a datetime. */
month_t datetime_month(const datetime_t *dttm)
{
    if (dttm->year != SHRT_MAX)
        return dttm->month;
    // If we cannot calculate the year, we cannot calculate the month, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX)
        return 0;
    return dttm->month;
}

/* Get the day of a datetime. */
uint8_t datetime_day(const datetime_t *dttm)
{
    if (dttm->year != SHRT_MAX)
        return dttm->day;
    // If we cannot calculate the year, we cannot calculate the day, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX)
        return 0;
    return dttm->day;
}

/* Get the hour of a datetime. */
uint8_t datetime_hour(const datetime_t *dttm)
{
    if (dttm->year != SHRT_MAX)
        return dttm->hour;
    // If we cannot calculate the year, we cannot calculate the hour, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX)
        return 0;
    return dttm->hour;
}

/* Get the minute of a datetime. */
uint8_t datetime_minute(const datetime_t *dttm)
{
    if (dttm->year != SHRT_MAX)
        return dttm->minute;
    // If we cannot calculate the year, we cannot calculate the minute, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX)
        return 0;
    return dttm->minute;
}

/* Get the second of a datetime. */
double datetime_second(const datetime_t *dttm)
{
    if (dttm->year != SHRT_MAX)
        return dttm->second;
    // If we cannot calculate the year, we cannot calculate the second, so we return 0.0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX)
        return 0.0;
    return dttm->second;
}

/* Calculate the Julian Day Number of a given year, month and day. */
long datetime_ymd_to_jdn(short year, month_t month, uint8_t day)
{
    bool isGregorian =
        (year > 1582) || (year == 1582 && month > DT_October) || (year == 1582 && month == DT_October && day >= 15);

    int yr = year;
    int mn = month;
    int dy = day;

    if (yr < 0)
        yr++;
    if (mn <= 2) {
        yr--;
        mn += 12;
    }

    long b = 0;
    if (isGregorian) {
        long a = yr / 100;
        b = 2 - a + (a / 4);
    }

    long JulianDayNumber = ldivide(1461L * (long)yr, 4L);
    JulianDayNumber += b + (306001L * ((long)mn + 1L)) / 10000L + (long)dy + 1720995L;

    return JulianDayNumber;
}

/* Convert a datetime to a Julian Day Number. */
long datetime_jdn(const datetime_t *dttm)
{
    if (dttm->JulianDayNumber != LONG_MAX)
        return dttm->JulianDayNumber;

    if (dttm->year == SHRT_MAX) {
        // If year is not initialised, we cannot calculate the Julian Day Number, so we return LONG_MAX as a sentinel
        // value.
        return LONG_MAX;
    }

    datetime_t *mutable_dttm = (datetime_t *)dttm; // Cast away const to store the calculated Julian Day Number
    mutable_dttm->JulianDayNumber = datetime_ymd_to_jdn(dttm->year, dttm->month, dttm->day);
    return dttm->JulianDayNumber;
}

/* Convert a datetime to a Julian Day. */
double datetime_jd(const datetime_t *dttm)
{
    if (dttm->JulianDay != DBL_MAX)
        return dttm->JulianDay;

    long jdn = datetime_jdn(dttm);
    if (jdn == LONG_MAX) {
        // If we cannot calculate the Julian Day Number, we cannot calculate the Julian Day, so we return DBL_MAX as a
        // sentinel value.
        return DBL_MAX;
    }

    ((datetime_t *)dttm)->JulianDay = jdn + (dttm->hour - 12) / 24.0 + dttm->minute / 1440.0 + dttm->second / 86400.0;

    return dttm->JulianDay;
}

/* Get the weekday of a datetime. */
weekday_t datetime_weekday(const datetime_t *dttm)
{
    long jdn = datetime_jdn(dttm);
    if (jdn == LONG_MAX) {
        // If we cannot calculate the Julian Day Number, we cannot calculate the weekday, so we return 0 as a sentinel
        // value.
        return 0;
    }
    return (weekday_t)((jdn + 1) % 7 + 1);
}

/* Get a display name for a weekday. */
const char *datetime_weekday_name(weekday_t weekday)
{
    static const char *names[] = {"Unknown",   "Sunday",   "Monday", "Tuesday",
                                  "Wednesday", "Thursday", "Friday", "Saturday"};

    if (weekday < DT_Sunday || weekday > DT_Saturday)
        return names[0];
    return names[weekday];
}

/* Check if a year is a leap year. */
bool datetime_is_leap_year(short year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* Get the number of days in a month for a given year and month. */
unsigned short datetime_days_in_month(short year, month_t month)
{
    static unsigned short daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month < DT_January || month > DT_December)
        return 0;
    if (month == DT_February)
        return datetime_is_leap_year(year) ? 29 : 28;
    return daysInMonth[month];
}

/* Check whether a year/month/day triple is a valid calendar date. */
bool datetime_valid_ymd(short year, month_t month, uint8_t day)
{
    unsigned short days;

    if (year < 1 || year > 9999)
        return false;
    days = datetime_days_in_month(year, month);
    return days != 0u && day >= 1u && day <= days;
}

/* Get the number of days in the month of a datetime. */
unsigned short datetime_days_in_this_month(const datetime_t *dttm)
{
    short year = datetime_year(dttm);
    if (year == SHRT_MAX)
        return 0; // If we cannot calculate the year, we cannot calculate the number of days in the month.

    month_t month = datetime_month(dttm);
    return datetime_days_in_month(year, month);
}

/* Find the next datetime with a specific weekday after a given datetime. */
datetime_t *datetime_next_weekday(const datetime_t *dttm, weekday_t weekday)
{
    if (dttm == NULL)
        return NULL; // Invalid input

    if (dttm->year == SHRT_MAX) {
        if (datetime_year(dttm) == SHRT_MAX) {
            return NULL; // Datetime is not initialised
        }
    }

    weekday_t currentWeekday = datetime_weekday(dttm);

    int daysToAdd = (weekday - currentWeekday + 7) % 7;
    if (daysToAdd == 0)
        daysToAdd = 7; // If the target weekday is the same as the current, we want the next occurrence

    datetime_t *result = datetime_init_copy(datetime_alloc(), dttm);
    datetime_add_days(result, daysToAdd);

    return result;
}
