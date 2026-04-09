#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "datetime.h"

/// @brief the datetime type - this is the internal structure definition for the datetime type. It is not intended to be used 
///        directly by users of the datetime library, but it is needed for the implementation of the datetime functions and 
///        for testing purposes.
typedef struct _dttm_t {
    short year;
    month_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    double second;
    long JulianDayNumber;
    double JulianDay;
} dttm_t;

/// @brief allocates enough memory for a dttm_t structure.
/// @return the start address of the allocated memory.
dttm_t *dttm_alloc() {
    dttm_t *self = (dttm_t *)malloc(sizeof(dttm_t));
    if (self != NULL) {
        self->year = SHRT_MAX; // Mark as uninitialised
        self->month = 0;
        self->day = 0;
        self->hour = 0;
        self->minute = 0;
        self->second = 0.0;
        self->JulianDayNumber = LONG_MAX; // Mark as uninitialised
        self->JulianDay = DBL_MAX; // Mark as uninitialised
    }
    return self;
}

inline void dttm_dealloc(dttm_t *self)
{
    free(self);
}

dttm_t *dttm_init_ymd(dttm_t *self, short year, month_t month, uint8_t day) {
    self->year = year;
    self->month = month;
    self->day = day;
    self->hour = 0;
    self->minute = 0;
    self->second = 0.0;
    self->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    self->JulianDay = DBL_MAX; // Mark as uninitialised
    return self;
}

dttm_t *dttm_init_ymdt(dttm_t *self,
    short year, month_t month, uint8_t day, uint8_t hour, uint8_t minute, double second)
{
    self->year = year;
    self->month = month;
    self->day = day;
    self->hour = hour;
    self->minute = minute;
    self->second = second;
    self->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    self->JulianDay = DBL_MAX; // Mark as uninitialised
    return self;
}

dttm_t *dttm_init_copy(dttm_t *self, const dttm_t *datetime) {
    self->year = datetime->year;
    self->month = datetime->month;
    self->day = datetime->day;
    self->hour = datetime->hour;
    self->minute = datetime->minute;
    self->second = datetime->second;
    self->JulianDayNumber = datetime->JulianDayNumber;
    self->JulianDay = datetime->JulianDay;
    return self;
}

dttm_t *dttm_init_jdn(dttm_t *self, long JulianDayNumber) {
    self->JulianDayNumber = JulianDayNumber;
    self->JulianDay = DBL_MAX; // Mark as uninitialised
    self->year = SHRT_MAX; // Mark as uninitialised
    self->month = 0;
    self->day = 0;
    self->hour = 0;
    self->minute = 0;
    self->second = 0.0;
    // The actual conversion to year, month, day, etc. will be done lazily when needed.
    return self;
}

dttm_t *dttm_init_jd(dttm_t *self, double JulianDay) {
    self->JulianDay = JulianDay;
    self->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    self->year = SHRT_MAX; // Mark as uninitialised
    self->month = 0;
    self->day = 0;
    self->hour = 0;
    self->minute = 0;
    self->second = 0.0;
    // The actual conversion to year, month, day, etc. will be done lazily when needed.
    return self;
}

dttm_t *dttm_init_now(dttm_t *self) {
    time_t now;
	struct tm tmdate;

    time(&now);
    tmdate = *localtime(&now);
    self->year = (short)(tmdate.tm_year + 1900);
    self->month = (month_t)(tmdate.tm_mon + 1);
    self->day = (uint8_t)tmdate.tm_mday;
    self->hour = (uint8_t)tmdate.tm_hour;
    self->minute = (uint8_t)tmdate.tm_min;
    self->second = (double)tmdate.tm_sec;
    self->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    self->JulianDay = DBL_MAX; // Mark as uninitialised
    // The actual conversion to Julian Day Number and Julian Day will be done lazily when needed

    return self;
}

dttm_t *dttm_init_easter(dttm_t *self, int year)
{
    if (year < 1 || year > 9999) return NULL;

    int goldenNumber = year % 19;
    int daysIntoYear;

    if (year < 1583) {
        int posIn4YearLeapCycle = year % 4;
        int weekdayCycle = year % 7;
        int paschalFullMoon = (19 * goldenNumber + 15) % 30;
        int weekdayOffset = (2 * posIn4YearLeapCycle + 4 * weekdayCycle - paschalFullMoon + 34) % 7;
        daysIntoYear = paschalFullMoon + weekdayOffset + 114;
    } else {
        int century = year / 100;
        int yearInCentury = year % 100;
        int centuryLeapCorrections = century / 4;
        int priorLeapRemainder = century % 4;
        int gregorianCorrection = (century + 8) / 25;
        int leapSkipAdjustment = (century - gregorianCorrection + 1) / 3;
        int epact = (19 * goldenNumber + century - centuryLeapCorrections - leapSkipAdjustment + 15) % 30;
        int yearLeapCorrections = yearInCentury / 4;
        int leapOffset = yearInCentury % 4;
        int daysFromFullMoonToSunday = (32 + 2 * priorLeapRemainder + 2 * yearLeapCorrections - epact - leapOffset) % 7;
        int easterMonthAdjust = (goldenNumber + 11 * epact + 22 * daysFromFullMoonToSunday) / 451;
        daysIntoYear = epact + daysFromFullMoonToSunday - 7 * easterMonthAdjust + 114;
    }

    self->year = (short)year;
    self->month = (month_t)(daysIntoYear / 31);
    self->day    = (uint8_t)((daysIntoYear % 31) + 1);
    self->hour   = 0;
    self->minute = 0;
    self->second = 0.0;
    self->JulianDay = DBL_MAX;
    self->JulianDayNumber = LONG_MAX;

    return self;
}

/// @brief calculates the time of the true new moon for a given lunation number lunationIndex.
/// The algorithm used is the ELP2000-85 algorithm, which is a well-known method for calculating the time of the true new moon.
/// The algorithm takes the lunation number lunationIndex as input and calculates the time of the true new moon based on a series of mathematical 
/// operations. The resulting time is returned as a Julian Ephemeris Date (JDE) in days.
/// @param lunationIndex the lunation number for which to calculate the time of the true new moon. This should be a positive integer.
/// @return the time of the true new moon in days as a Julian Ephemeris Date (JDE).
static double dttm_true_new_moon_tt(int lunationIndex)
{
    /* Time in Julian centuries from J2000 */
    double T  = lunationIndex / 1236.85;
    double T2 = T * T;
    double T3 = T2 * T;
    double T4 = T3 * T;

    /* Mean new moon (JDE) */
    double jdeMean = 2451550.09765 + 29.530588853 * lunationIndex + 0.0001337 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;

    /* Sun's mean anomaly (degrees) */
    double sunMeanAnomaly = 2.5534 + 29.10535670 * lunationIndex - 0.0000014 * T2 - 0.00000011 * T3;

    /* Moon's mean anomaly (degrees) */
    double moonMeanAnomaly = 201.5643 + 385.81693528 * lunationIndex + 0.0107582 * T2 + 0.00001238 * T3 - 0.000000058 * T4;

    /* Moon's argument of latitude (degrees) */
    double moonArgumentLatitude = 160.7108 + 390.67050284 * lunationIndex - 0.0016118 * T2 - 0.00000227 * T3 + 0.000000011 * T4;

    /* Longitude of ascending node (degrees) */
    double ascendingNodeLongitude = 124.7746 - 1.56375580 * lunationIndex + 0.0020691 * T2 + 0.00000215 * T3;

    /* Convert to radians */
    const double degToRad = M_PI / 180.0;
    sunMeanAnomaly        *= degToRad;
    moonMeanAnomaly       *= degToRad;
    moonArgumentLatitude  *= degToRad;
    ascendingNodeLongitude*= degToRad;

    /* Eccentricity correction factor */
    double E = 1 - 0.002516 * T - 0.0000074 * T2;

    /* Periodic correction terms (Meeus Table 49.A) */
    double correction =
        -0.40720 * sin(moonMeanAnomaly)
        + 0.17241 * E * sin(sunMeanAnomaly)
        + 0.01608 * sin(2 * moonMeanAnomaly)
        + 0.01039 * sin(2 * moonArgumentLatitude)
        + 0.00739 * E * sin(moonMeanAnomaly - sunMeanAnomaly)
        - 0.00514 * E * sin(moonMeanAnomaly + sunMeanAnomaly)
        + 0.00208 * E * E * sin(2 * sunMeanAnomaly)
        - 0.00111 * sin(moonMeanAnomaly - 2 * moonArgumentLatitude)
        - 0.00057 * sin(moonMeanAnomaly + 2 * moonArgumentLatitude)
        + 0.00056 * E * sin(2 * moonMeanAnomaly + sunMeanAnomaly)
        - 0.00042 * sin(3 * moonMeanAnomaly)
        + 0.00042 * E * sin(sunMeanAnomaly + 2 * moonArgumentLatitude)
        + 0.00038 * E * sin(sunMeanAnomaly - 2 * moonArgumentLatitude)
        - 0.00024 * E * sin(2 * moonMeanAnomaly - sunMeanAnomaly)
        - 0.00017 * sin(ascendingNodeLongitude)
        - 0.00007 * sin(moonMeanAnomaly + 2 * sunMeanAnomaly)
        + 0.00004 * sin(2 * moonMeanAnomaly - 2 * moonArgumentLatitude)
        + 0.00004 * sin(3 * sunMeanAnomaly)
        + 0.00003 * sin(moonMeanAnomaly + sunMeanAnomaly - 2 * moonArgumentLatitude)
        + 0.00003 * sin(2 * moonMeanAnomaly + 2 * moonArgumentLatitude)
        - 0.00003 * sin(moonMeanAnomaly - sunMeanAnomaly + 2 * moonArgumentLatitude)
        - 0.00002 * sin(moonMeanAnomaly - sunMeanAnomaly - 2 * moonArgumentLatitude)
        - 0.00002 * sin(3 * moonMeanAnomaly + sunMeanAnomaly)
        + 0.00002 * sin(4 * moonMeanAnomaly);

    return jdeMean + correction;
}

/// @brief calculate the difference in seconds between Terrestrial Time (TT) and Universal Time (UT) for a given year.
///
/// The difference between TT and UT is needed to convert between the two time standards. TT is a modern continuation of Ephemeris Time (ET), 
/// and is the time standard used in astronomy. UT is the time standard used in everyday life. The difference between TT and UT is 
/// caused by the Earth's slightly irregular rotation, and is usually expressed in seconds.
///
/// The algorithm used to calculate the difference is based on the IAU SOFA series, which provides a set of polynomial expressions to 
/// calculate the difference between TT and UT for a given year. The expressions are based on a fit of historical astronomical data, and are 
/// accurate to within 0.1 seconds for years between 1800 and 2500.
///
/// @param year the year for which to calculate the difference between TT and UT. This should be a positive integer between 1800 and 2500, 
///        inclusive. If the input year is outside this range, the function will return an approximate value based on the nearest valid 
///        year.
/// @return the difference in seconds between TT and UT for the given year.
static double dttm_delta_t(int year)
{
    double offsetYears;   /* Years offset from reference epoch for this segment */
    double deltaT;        /* Resulting ΔT in seconds */

    if (year < 1800) {
        offsetYears = year - 1700;
        deltaT = (((-0.0000000851788756 * offsetYears + 0.00013336) * offsetYears - 0.0059285) * offsetYears + 0.1603) * offsetYears + 8.83;

    } else if (year < 1860) {
        offsetYears = year - 1800;
        deltaT = (((((0.000000000875 * offsetYears - 0.0000001699) * offsetYears + 0.0000121272) * offsetYears - 0.00037436) * offsetYears
               + 0.0041116) * offsetYears + 0.0068612) * offsetYears + 13.72;

    } else if (year < 1900) {
        offsetYears = year - 1860;
        deltaT = ((((0.0000042886428 * offsetYears - 0.0004473624) * offsetYears + 0.01680668) * offsetYears - 0.251754) * offsetYears
               + 0.5737) * offsetYears + 7.62;

    } else if (year < 1920) {
        offsetYears = year - 1900;
        deltaT = (((-0.000197 * offsetYears + 0.0061966) * offsetYears - 0.0598939) * offsetYears + 1.494119) * offsetYears - 2.79;

    } else if (year < 1941) {
        offsetYears = year - 1920;
        deltaT = ((0.0020936 * offsetYears - 0.076100) * offsetYears + 0.84493) * offsetYears + 21.20;

    } else if (year < 1961) {
        offsetYears = year - 1950;
        deltaT = ((0.000392618767177 * offsetYears - 0.004291845493562231) * offsetYears + 0.407) * offsetYears + 29.107;

    } else if (year < 1986) {
        offsetYears = year - 1975;
        deltaT = ((-0.00139275766016713 * offsetYears - 0.00384615384615385) * offsetYears + 1.067) * offsetYears + 45.45;

    } else if (year < 2005) {
        offsetYears = year - 2000;
        deltaT = ((((0.00002373599 * offsetYears + 0.000651814) * offsetYears + 0.0017275) * offsetYears - 0.060374) * offsetYears
               + 0.3345) * offsetYears + 63.86;

    } else if (year < 2050) {
        offsetYears = year - 2000;
        deltaT = (0.005589 * offsetYears + 0.32217) * offsetYears + 62.92;

    } else if (year < 2150) {
        double centuriesFrom1820 = (year - 1820) / 100.0;
        deltaT = -20.0 + 32.0 * centuriesFrom1820 * centuriesFrom1820 - 0.5628 * (2150 - year);

    } else {
        double centuriesFrom1820 = (year - 1820) / 100.0;
        deltaT = -20.0 + 32.0 * centuriesFrom1820 * centuriesFrom1820;
    }

    return deltaT;
}

/* Compute the TT of the December solstice for a given year (Meeus polynomial). */
static double dttm_dec_solstice_tt(int year)
{
    double Y = (year - 2000) / 1000.0;  /* millennia from J2000.0 */
    return (((-0.000000217 * Y - 0.00000084) * Y + 0.000461) * Y + 365242.74049) * Y + 2451900.05952;
}

/// @brief Compute the Julian day number of the Chinese New Year for a given year.
/// This function takes a year between 1700 and 2400 and returns the Julian day number of the Chinese New Year in that year.
/// @param year The year for which the Julian day number of the Chinese New Year is to be computed.
/// @return The Julian day number of the Chinese New Year in the given year.
static long dttm_chinese_new_year_jdn(int year)
{
    if (year < 1700 || year > 2400)
        return LONG_MAX;

    /* 1. December solstice of the previous year (Terrestrial Time) */
    double solsticeTerrestrialTime = dttm_dec_solstice_tt(year - 1);

    /* Convert solstice from Terrestrial Time to UTC */
    double deltaTPreviousYearDays = dttm_delta_t(year - 1) / 86400.0;
    double solsticeUTC = solsticeTerrestrialTime - deltaTPreviousYearDays;

    /* 2. Estimate lunation index for new moons around the solstice */
    int lunationIndex = (int)((solsticeUTC - 2451550.09765) / 29.530588853) - 2;

    /* 3. First new moon after the solstice */
    double firstNewMoonTerrestrial = dttm_true_new_moon_tt(lunationIndex);
    double firstNewMoonUTC = firstNewMoonTerrestrial - deltaTPreviousYearDays;

    while (firstNewMoonUTC < solsticeUTC) {
        lunationIndex++;
        firstNewMoonTerrestrial = dttm_true_new_moon_tt(lunationIndex);
        firstNewMoonUTC = firstNewMoonTerrestrial - deltaTPreviousYearDays;
    }

    /* 4. Second new moon = Chinese New Year */
    double secondNewMoonTerrestrial = dttm_true_new_moon_tt(lunationIndex + 1);
    double deltaTCurrentYearDays = dttm_delta_t(year) / 86400.0;
    double secondNewMoonUTC = secondNewMoonTerrestrial - deltaTCurrentYearDays;

    /* Convert UTC → China Standard Time (UTC+8) */
    double secondNewMoonCST = secondNewMoonUTC + (8.0 / 24.0);

    /* Round to nearest civil day in CST */
    return (long)floor(secondNewMoonCST + 0.5);
}

dttm_t *dttm_init_chinese_new_year(dttm_t *self, int year)
{
    // algorithm not reliable for years before 1700 or after 2400
    if (year < 1700 || year > 2400) return NULL;

    self->JulianDayNumber = dttm_chinese_new_year_jdn(year);
    self->hour   = 0;
    self->minute = 0;
    self->second = 0.0;
    self->JulianDay = DBL_MAX;

    dttm_year(self);

    return self;
}

double dttm_tz_offset(const dttm_t *self) {
    // Get the local time and GMT time for the given datetime
    time_t t;
    struct tm local_tm, gmt_tm;

    if (self == NULL) return DBL_MAX;
    if (self->year == SHRT_MAX) {
        if (self->JulianDayNumber == LONG_MAX && self->JulianDay == DBL_MAX) {
            return DBL_MAX; // Cannot calculate timezone offset if date is not initialised
        }
        dttm_year(self);
    }

    // Convert datetime to struct tm
    local_tm.tm_year = self->year - 1900;
    local_tm.tm_mon = self->month - 1;
    local_tm.tm_mday = self->day;
    local_tm.tm_hour = self->hour;
    local_tm.tm_min = self->minute;
    local_tm.tm_sec = (int)self->second;
    local_tm.tm_isdst = -1; // Let mktime determine if DST is in effect

    // Convert struct tm to time_t (local time)
    t = mktime(&local_tm);
    if (t == -1) return DBL_MAX;

    // Convert time_t to struct tm in GMT
    struct tm *gmtm = gmtime(&t);
    if (gmtm == NULL) return DBL_MAX;
    gmt_tm = *gmtm;

    // Calculate the timezone offset in hours
    double offset_hours = (local_tm.tm_hour - gmt_tm.tm_hour) + 
                          (local_tm.tm_min - gmt_tm.tm_min) / 60.0 + 
                          (local_tm.tm_sec - gmt_tm.tm_sec) / 3600.0;

    // Adjust for day difference if necessary
    if (local_tm.tm_yday != gmt_tm.tm_yday) {
        if (local_tm.tm_yday > gmt_tm.tm_yday) {
            offset_hours += 24.0; // Local time is ahead of GMT
        } else {
            offset_hours -= 24.0; // Local time is behind GMT
        }
    }

    return offset_hours;
}

dttm_t *dttm_to_gmt(dttm_t *self)
{
    if (self == NULL) return NULL;

    time_t t;
    struct tm tm;

    // Convert datetime to struct tm
    tm.tm_year = self->year - 1900;
    tm.tm_mon = self->month - 1;
    tm.tm_mday = self->day;
    tm.tm_hour = self->hour;
    tm.tm_min = self->minute;
    tm.tm_sec = (int)self->second;
    tm.tm_isdst = -1; // Let mktime determine if DST is in effect

    // Convert struct tm to time_t (local time)
    t = mktime(&tm);
    if (t == -1) return NULL;

    // Convert time_t to struct tm in GMT
    struct tm *gmtm = gmtime(&t);
    if (gmtm == NULL) return NULL;

    // Update the input datetime with GMT values
    self->year = (short)(gmtm->tm_year + 1900);
    self->month = (month_t)(gmtm->tm_mon + 1);
    self->day = (uint8_t)gmtm->tm_mday;
    self->hour = (uint8_t)gmtm->tm_hour;
    self->minute = (uint8_t)gmtm->tm_min;
    self->second = (double)gmtm->tm_sec;
    
    if (self->JulianDayNumber != LONG_MAX) {
        // If Julian Day Number is initialised, we need to update it as well
        self->JulianDayNumber = dttm_jdn(self);
    }
    if (self->JulianDay != DBL_MAX) {
        // If Julian Day is initialised, we need to update it as well
        self->JulianDay = dttm_jd(self);
    }

    return self;
}

/// @brief Divide two long integers with truncation toward zero.
/// @param numerator The numerator.
/// @param denominator The denominator.
/// @return The truncated result of numerator / denominator.
static inline long ldivide(long numerator, long denominator)
{
    return (numerator >= 0L) ? (numerator / denominator) : ((numerator - denominator + 1L) / denominator);
}

/// @brief Converts a Julian Day Number into a Gregorian/Julian calendar date. Applies the Gregorian reform for JD >= 2299161 
///        and adjusts for the absence of a year zero (1 BC = year -1).
/// @param julianDay The Julian Day Number to convert.
/// @param monthOut Output pointer for month (1–12).
/// @param dayOut Output pointer for day (1–31).
/// @param yearOut Output pointer for year (no year 0).
static void date_julianDayNumToMDY(long julianDay, int *monthOut, int *dayOut, int *yearOut)
{
    long gregorianOffset = ldivide(100L * julianDay - 186721625L, 3652425L);
    long correctedJDN = (julianDay < 2299161L) ? julianDay : (julianDay + 1L + gregorianOffset - ldivide(gregorianOffset, 4L));
    long shiftedDay = correctedJDN + 1524L;
    long centuryIndex = ldivide(100L * shiftedDay - 12210L, 36525L);
    long dayOfCentury = ldivide(36525L * centuryIndex, 100L);
    long monthIndex = ldivide((shiftedDay - dayOfCentury) * 10000L, 306001L);
    *dayOut = (int)(shiftedDay - dayOfCentury - ldivide(306001L * monthIndex, 10000L));
    *monthOut = (int)((monthIndex < 14L) ? (monthIndex - 1L) : (monthIndex - 13L));
    *yearOut = (int)((*monthOut > 2) ? (centuryIndex - 4716L) : (centuryIndex - 4715L));

    // Adjust for missing year zero
    if (*yearOut <= 0) (*yearOut)--;
}

short dttm_year(const dttm_t *self) {
    if (self == NULL) return SHRT_MAX;
    if (self->year != SHRT_MAX) return self->year;
    if (self->JulianDay != DBL_MAX) {
        // Convert Julian Day to calendar date and return the year.
        dttm_t *mutable_self = (dttm_t *)self; // Cast away const to store the calculated year
        mutable_self->JulianDayNumber = (long)(self->JulianDay + 0.5);
        int month, day, year;
        date_julianDayNumToMDY(self->JulianDayNumber, &month, &day, &year);
        mutable_self->year = (short)year;
        mutable_self->month = (month_t)month;
        mutable_self->day = (uint8_t)day;
        double fractional_day = self->JulianDay - (double)mutable_self->JulianDayNumber;
        double fractional_hour = fractional_day * 24.0 + 12.0; // Julian Day starts at noon
        mutable_self->hour = (uint8_t)fractional_hour;
        double fractional_minute = (fractional_hour - (double)mutable_self->hour) * 60.0;
        mutable_self->minute = (uint8_t)fractional_minute;
        mutable_self->second = (fractional_minute - (double)mutable_self->minute) * 60.0;
        return (short)year;
    }
    if (self->JulianDayNumber != LONG_MAX) {
        // Convert Julian Day Number to calendar date and return the year.
        int month, day, year;
        date_julianDayNumToMDY(self->JulianDayNumber, &month, &day, &year);
        dttm_t *mutable_self = (dttm_t *)self; // Cast away const to store the calculated year
        mutable_self->year = (short)year;
        mutable_self->month = (month_t)month;
        mutable_self->day = (uint8_t)day;
        mutable_self->hour = 12; // Default to noon for the time part when we only have a julian day number
        mutable_self->minute = 0;
        mutable_self->second = 0.0;
        return (short)year;
    }
    return SHRT_MAX; // This is an uninitialised state that cannot be calculated, so we return SHRT_MAX as a sentinel value.
}

month_t dttm_month(const dttm_t *self) {
    if (self->year != SHRT_MAX) return self->month;
    // If we cannot calculate the year, we cannot calculate the month, so we return 0 as a sentinel value.
    if (dttm_year(self) == SHRT_MAX) return 0;
    return self->month;
}

uint8_t dttm_day(const dttm_t *self) {
    if (self->year != SHRT_MAX) return self->day;
    // If we cannot calculate the year, we cannot calculate the day, so we return 0 as a sentinel value.
    if (dttm_year(self) == SHRT_MAX) return 0;
    return self->day;
}

uint8_t dttm_hour(const dttm_t *self) {
    if (self->year != SHRT_MAX) return self->hour;
    // If we cannot calculate the year, we cannot calculate the hour, so we return 0 as a sentinel value.
    if (dttm_year(self) == SHRT_MAX) return 0;
    return self->hour;
}

uint8_t dttm_minute(const dttm_t *self) {
    if (self->year != SHRT_MAX) return self->minute;
    // If we cannot calculate the year, we cannot calculate the minute, so we return 0 as a sentinel value.
    if (dttm_year(self) == SHRT_MAX) return 0;
    return self->minute;
}

double dttm_second(const dttm_t *self) {
    if (self->year != SHRT_MAX) return self->second;
    // If we cannot calculate the year, we cannot calculate the second, so we return 0.0 as a sentinel value.
    if (dttm_year(self) == SHRT_MAX) return 0.0;
    return self->second;
}

long dttm_ymd_to_jdn(short year, month_t month, uint8_t day) {
    bool isGregorian = (year > 1582) ||
                       (year == 1582 && month > DT_October) || 
                       (year == 1582 && month == DT_October && day >= 15);

    int yr = year;
    int mn = month;
    int dy = day;
    
    if (yr < 0) yr++;
    if (mn <= 2) { yr--; mn += 12; }

    long b = 0;
    if (isGregorian) {
        long a = yr / 100;
        b = 2 - a + (a/4);
    }

    long JulianDayNumber = ldivide(1461L * (long) yr, 4L);
    JulianDayNumber += b + ( 306001L * ((long) mn + 1L) ) / 10000L + (long) dy + 1720995L;

    return JulianDayNumber;
}

long dttm_jdn(const dttm_t *self) {
    if (self->JulianDayNumber != LONG_MAX) return self->JulianDayNumber;

    if (self->year == SHRT_MAX) {
        // If year is not initialised, we cannot calculate the Julian Day Number, so we return LONG_MAX as a sentinel value.
        return LONG_MAX;
    }

    dttm_t *mutable_self = (dttm_t *)self; // Cast away const to store the calculated Julian Day Number
    mutable_self->JulianDayNumber = dttm_ymd_to_jdn(self->year, self->month, self->day);
    return self->JulianDayNumber;
}

double dttm_jd(const dttm_t *self) {
    if (self->JulianDay != DBL_MAX) return self->JulianDay;

    long jdn = dttm_jdn(self);
    if (jdn == LONG_MAX) {
        // If we cannot calculate the Julian Day Number, we cannot calculate the Julian Day, so we return DBL_MAX as a sentinel value.
        return DBL_MAX;
    }

    ((dttm_t *)self)->JulianDay = jdn + (self->hour - 12) / 24.0 + self->minute / 1440.0 + self->second / 86400.0;

    return self->JulianDay;
}

weekday_t dttm_weekday(const dttm_t *self)
{
    long jdn = dttm_jdn(self);
    if (jdn == LONG_MAX) {
        // If we cannot calculate the Julian Day Number, we cannot calculate the weekday, so we return 0 as a sentinel value.
        return 0;
    }
    return (weekday_t)((jdn + 1) % 7 + 1); 
}

bool dttm_equal(const dttm_t *self, const dttm_t *datetime)
{
    if (self->year == SHRT_MAX) dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
    if (datetime->year == SHRT_MAX) dttm_year(datetime); // Try to calculate the year, month, day, ... if it is not initialised
    
    return self->year == datetime->year &&
           self->month == datetime->month &&
           self->day == datetime->day &&
           self->hour == datetime->hour &&
           self->minute == datetime->minute &&
           fabs(self->second - datetime->second) < 1e-9;
}

bool dttm_lt(const dttm_t *self, const dttm_t *datetime)
{
    if (self->year == SHRT_MAX) dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
    if (datetime->year == SHRT_MAX) dttm_year(datetime); // Try to calculate the year, month, day, ... if it is not initialised

    if (self->year != datetime->year) return self->year < datetime->year;
    if (self->month != datetime->month) return self->month < datetime->month;
    if (self->day != datetime->day) return self->day < datetime->day;
    if (self->hour != datetime->hour) return self->hour < datetime->hour;
    if (self->minute != datetime->minute) return self->minute < datetime->minute;
    return self->second < datetime->second;
}

bool dttm_le(const dttm_t *self, const dttm_t *datetime)
{
    return dttm_lt(self, datetime) || dttm_equal(self, datetime);
}

bool dttm_gt(const dttm_t *self, const dttm_t *datetime)
{
    return !dttm_le(self, datetime);
}

bool dttm_ge(const dttm_t *self, const dttm_t *datetime)
{
    return !dttm_lt(self, datetime);
}

int dttm_compare(const dttm_t *self, const dttm_t *datetime)
{
    if (dttm_lt(self, datetime)) return -1;
    if (dttm_gt(self, datetime)) return 1;
    return 0; // They are equal
}

inline bool dttm_is_leap_year(short year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

unsigned short dttm_days_in_month(short year, month_t month)
{
    static unsigned short daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month < DT_January || month > DT_December) return 0;
    if (month == DT_February) return dttm_is_leap_year(year) ? 29 : 28;
    return daysInMonth[ month ];
}

unsigned short dttm_days_in_this_month(const dttm_t *self)
{
    short year = dttm_year(self);
    if (year == SHRT_MAX) return 0; // If we cannot calculate the year, we cannot calculate the number of days in the month.

    month_t month = dttm_month(self);
    return dttm_days_in_month(year, month);
}

bool dttm_is_dst(const dttm_t *self)
{
    if (self->year == SHRT_MAX) dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised

    struct tm tmdate;
    tmdate.tm_year = self->year - 1900;
    tmdate.tm_mon = self->month - 1;
    tmdate.tm_mday = self->day;
    tmdate.tm_hour = self->hour;
    tmdate.tm_min = self->minute;
    tmdate.tm_sec = (int)self->second;
    tmdate.tm_isdst = -1; // Let mktime determine if DST is in effect

    time_t t = mktime(&tmdate);
    if (t == -1) return false; // Could not determine DST status

    return tmdate.tm_isdst > 0;
}   

/* Invalidate Julian caches and recalculate from the current y/m/d/h/m/s fields. */
static void dttm_refresh_julian_caches(dttm_t *self)
{
    if (self->JulianDay != DBL_MAX || self->JulianDayNumber != LONG_MAX) {
        self->JulianDay = DBL_MAX;
        self->JulianDayNumber = LONG_MAX;
        dttm_jd(self);
    }
}

dttm_t *dttm_add_days(dttm_t *self, long days)
{
    if (days == 0) return self; // No change needed
    if (self->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add days to it.
        if (self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL;

        if (self->JulianDay != DBL_MAX) self->JulianDay += (double)days;
        if (self->JulianDayNumber != LONG_MAX) self->JulianDayNumber += days;
        return self;
    }

    self->JulianDay = DBL_MAX;
    self->JulianDayNumber = LONG_MAX;

    uint8_t hour = self->hour;
    uint8_t minute = self->minute;
    double second = self->second;
    double jdn = dttm_jd(self);
    dttm_init_jd(self, jdn + (double)days);
    dttm_year(self); // This will fill in the year, month, day based on the new Julian Day
    
    self->hour = hour;
    self->minute = minute;
    self->second = second;

    return self;
}

dttm_t *dttm_add_weeks(dttm_t *self, int weeks)
{
    return dttm_add_days(self, (long)weeks * 7L);
}

dttm_t *dttm_add_months(dttm_t *self, int months)
{
    if (months == 0) return self; // No change needed
    
    if (self->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL; 
        dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
    } 

    int years = months / 12;
    int remainingMonths = months % 12;
    if (years != 0) {
        self->year += years;
    }
    if (remainingMonths != 0) {
        self->month += remainingMonths;
        if (self->month > 12) {
            self->year++;
            self->month -= 12;
        } else if (self->month < 1) {
            self->year--;
            self->month += 12;
        }
    }

    if (self->day >= 29) {
        if (self->month == DT_February) {
            // Handle February separately because of leap years
            int maxDay = dttm_is_leap_year(self->year) ? 29 : 28;
            if (self->day > maxDay) {
                self->day = (uint8_t)maxDay;
            }
        } else {
            // Handle months with 30 days
            if (self->month == DT_April || self->month == DT_June || self->month == DT_September || self->month == DT_November) {
                if (self->day > 30) {
                    self->day = 30;
                }
            }
        }
    }

    dttm_refresh_julian_caches(self);

    return self;
}

dttm_t *dttm_add_years(dttm_t *self, int years)
{
    if (years == 0) return self; // No change needed
    
    if (self->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL; 
        dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
    } 

    self->year += years;
    if (self->month == DT_February && self->day == 29 && !dttm_is_leap_year(self->year)) {
        // If we are on February 29 and the new year is not a leap year, we need to adjust the day to February 28
        self->day = 28;
    }

    dttm_refresh_julian_caches(self);
    return self;
}

dttm_t *dttm_add_hours(dttm_t *self, int hours)
{
    if (hours == 0) return self; // No change needed

    if (self->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL; 
        dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
    } 

    int daysToAdd = hours / 24;
    int remainingHours = hours % 24;

    int hour = self->hour;
    hour += remainingHours;
    if (hour >= 24) {
        hour -= 24;
        daysToAdd++;
    } else if (hour < 0) {
        hour += 24;
        daysToAdd--;
    }

    self->hour = hour;
    if (daysToAdd != 0) {
        dttm_add_days(self, daysToAdd);
    }

    dttm_refresh_julian_caches(self);

    return self;
}

dttm_t *dttm_add_minutes(dttm_t *self, int minutes)
{
    if (minutes == 0) return self; // No change needed

    if (self->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL; 
        dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
    } 

    int hoursToAdd = minutes / 60;
    int remainingMinutes = minutes % 60;

    int minute = self->minute;
    minute += remainingMinutes;
    if (minute >= 60) {
        minute -= 60;
        hoursToAdd++;
    } else if (minute < 0) {
        minute += 60;
        hoursToAdd--;
    }
    self->minute = minute;

    if (hoursToAdd != 0) {
        dttm_add_hours(self, hoursToAdd);
    }

    dttm_refresh_julian_caches(self);

    return self;
}

dttm_t *dttm_add_seconds(dttm_t *self, double seconds)
{
    if (fabs(seconds) < 1e-9) return self; // No change needed

    if (self->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL;
        dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
    } 

    int minutesToAdd = (int)(seconds / 60.0);
    double remainingSeconds = seconds - (double)(minutesToAdd * 60);

    self->second += remainingSeconds;
    if (self->second >= 60.0) {
        self->second -= 60.0;
        dttm_add_minutes(self, 1);
    } else if (self->second < 0.0) {
        self->second += 60.0;
        dttm_add_minutes(self, -1);
    }

    dttm_refresh_julian_caches(self);

    return self;
}

dttm_t *dttm_add_span(dttm_t *self, const dttm_span_t *span)
{
    if (span == NULL) return self;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
    if (self->year == SHRT_MAX && self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL;

    if (span->years != 0) self = dttm_add_years(self, (int)span->years);
    if (span->months != 0) self = dttm_add_months(self, (int)span->months);
    if (span->days != 0) self = dttm_add_days(self, (long)span->days);
    if (span->hours != 0) self = dttm_add_hours(self, (int)span->hours);
    if (span->minutes != 0) self = dttm_add_minutes(self, (int)span->minutes);
    if (span->seconds != 0) self = dttm_add_seconds(self, (double)span->seconds);
    return self;
}

dttm_t *dttm_sub_span(dttm_t *self, const dttm_span_t *span)
{
    if (span == NULL) return self;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
    if (self->year == SHRT_MAX && self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return NULL;

    if (span->years != 0) self = dttm_add_years(self, -(int)span->years);
    if (span->months != 0) self = dttm_add_months(self, -(int)span->months);
    if (span->days != 0) self = dttm_add_days(self, -(long)span->days);
    if (span->hours != 0) self = dttm_add_hours(self, -(int)span->hours);
    if (span->minutes != 0) self = dttm_add_minutes(self, -(int)span->minutes);
    if (span->seconds != 0) self = dttm_add_seconds(self, -(double)span->seconds);
    return self;
}

unsigned int dttm_hash(const dttm_t *self)
{
    if (self->year == SHRT_MAX) dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised

    unsigned int ms = (unsigned int)(self->hour * 3600000u +
                    self->minute * 60000u + (unsigned int)(self->second * 1000.0));

    unsigned int dateKey = (((unsigned int)self->year  << 16) | 
                           ((unsigned int)self->month << 8) |
                           (unsigned int)self->day) * 0x9E3779B1u; // mix year, month, day

    unsigned int hash = dateKey ^ (ms * 0x9E3779B1u);  // mix date + time

    // MurmurHash3 finalizer (32‑bit)
    hash ^= hash >> 16;
    hash *= 0x85EBCA6Bu;
    hash ^= hash >> 13;
    hash *= 0xC2B2AE35u;
    hash ^= hash >> 16;

    return hash;
}

double dttm_duration(const dttm_t *self, const dttm_t *datetime, dttm_span_t *span)
{
    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot calculate the difference.
    if (self->year == SHRT_MAX && self->JulianDay == DBL_MAX && self->JulianDayNumber == LONG_MAX) return DBL_MAX;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot calculate the difference.
    if (datetime->year == SHRT_MAX && datetime->JulianDay == DBL_MAX && datetime->JulianDayNumber == LONG_MAX) return DBL_MAX;

    double jd1 = dttm_jd(self);
    double jd2 = dttm_jd(datetime);

    if (span != NULL) {
        dttm_year(self); // Try to calculate the year, month, day, ... if it is not initialised
        dttm_year(datetime); // Try to calculate the year, month, day, ... if it is not initialised

        if (jd1 < jd2) {
            // If self is earlier than datetime, we swap them to calculate the span as a positive duration, and we will negate the final 
            // difference at the end.
            const dttm_t *temp = self;
            self = datetime;
            datetime = temp;
        }

        int years = self->year - datetime->year;
        int months = self->month - datetime->month;
        int days = self->day - datetime->day;
        int hours = self->hour - datetime->hour;
        int minutes = self->minute - datetime->minute;
        double seconds = self->second - datetime->second;

        // Normalize the span so that each component is within its normal range
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
            int month = datetime->month;
            int year = datetime->year;
            unsigned short daysInPrevMonth = dttm_days_in_month(year, month == DT_January ? DT_December : month - 1);
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

/* Internal utility functions for string handling */

static void str_to_upper(char *s)
{
    for (; *s; s++)
        *s = (char)toupper((unsigned char)*s);
}

typedef char *string_t;

#define STR_CHUNK 32  /* allocation granularity for string_t growth */

/* Return the stored length of a string_t (bytes before the data pointer). */
static inline size_t string_length(const string_t string) {
    return *(size_t *)&string[ -sizeof(size_t) ];
}

/* Return the stored capacity of a string_t. */
static inline size_t string_capacity(const string_t string) {
    return *(size_t *)&string[ -sizeof(size_t) - sizeof(size_t) ];
}

size_t size_max(size_t a, size_t b) {
    return (a < b) ? b : a;
}

/* Allocate a new string_t initialized with value. */
static string_t string_create(const char *value) {
    size_t len = strlen(value);
    size_t capacity = size_max(STR_CHUNK, STR_CHUNK*((len + STR_CHUNK)/STR_CHUNK));
    char *string = (char *)malloc(sizeof(size_t) + sizeof(size_t) + capacity);
    *(size_t *)string = capacity;
    string += sizeof(size_t);
    *(size_t *)string = len;
    string += sizeof(size_t);
    memcpy(string, value, len + 1);
    return (string_t)string;
}

/* Return a deep copy of string. */
[[maybe_unused]] static string_t string_clone(const string_t string) {
    size_t len = string_length(string);
    size_t capacity = string_capacity(string);
    char *clone = (char *)malloc(sizeof(size_t) + sizeof(size_t) + capacity);
    *(size_t *)clone = capacity;
    clone += sizeof(size_t);
    *(size_t *)clone = len;
    clone += sizeof(size_t);
    memcpy(clone, string, len + 1);
    return (string_t)clone;
}

/* Grow string_t to accommodate newlen bytes, doubling in STR_CHUNK increments. */
static string_t string_realloc(string_t string, size_t newlen)
{
   size_t capacity = string_capacity(string);

   if (newlen >= capacity) {
      capacity = STR_CHUNK*((newlen + STR_CHUNK)/STR_CHUNK);
      string = (char *)realloc(&string[ -sizeof(size_t) - sizeof(size_t) ], sizeof(size_t) + sizeof(size_t) + capacity);
      *(size_t *)string = capacity;
      string += sizeof(size_t) + sizeof(size_t);
   }
   *(size_t *)&string[ -sizeof(size_t) ] = newlen;
   return (string_t)string;
}

/* Free a string_t. Use free() instead after string_finalize(). */
[[maybe_unused]] static void string_destroy(string_t string) {
   if (string != NULL) {
      free(&string[ -sizeof(size_t) - sizeof(size_t) ]);
   }
}

/* Append null-terminated str to string. May reallocate; returns new pointer. */
string_t string_append(string_t string, const char *str)
{
   size_t len = string_length(string);
   size_t newlen = len + strlen(str);
   string = string_realloc(string, newlen);
   memcpy(string + len, str, newlen - len + 1);
   return (string_t)string;
}

/* Append a single character to string. May reallocate; returns new pointer. */
static string_t string_append_char(string_t string, char chr)
{
   size_t len = string_length(string);
   size_t newlen = len + 1;
   string = string_realloc(string, newlen);
   string[ len ] = chr;
   string[ len + 1 ] = '\0';
   return (string_t)string;
}

/* Strip the length/capacity header and return a plain malloc'd C string. */
static char *string_finalize(string_t string)
{
   size_t len = string_length(string);
   return (char *)realloc(memmove(&string[ -sizeof(size_t) - sizeof(size_t) ], string, len + 1), len + 1);
}

char *dttm_format(const dttm_t *self, const char *format)
{
    static char* weekdayNames[] = { NULL,
        "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday" };
    static char* monthNames[] = { NULL,
        "january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december" };
   
    if (format == NULL) return NULL;
    if (self->year == SHRT_MAX) {
        if (dttm_year(self) == SHRT_MAX) {
            return NULL;
        }
    }

   string_t formattedString = string_create("");
   
   char  buffer[ 32 ];
   
   char *formatPtr = (char *)format;
   
   while ( *formatPtr ) {
      if ( *formatPtr == '%' ) {
         formatPtr ++;
         if ( *formatPtr == '\0' ) {
            formattedString = string_append_char(formattedString, '%');
            break;
         }
         
         switch ( *formatPtr ) {
            case '%':
               formattedString = string_append_char(formattedString, '%');
               formatPtr ++;
               break;
               
            case 'd':
            case 'D':
               switch ( strspn( formatPtr, "Dd" ) ) {
                  case 1:
                     sprintf(buffer, "%i", (int) self->day);
                     formattedString = string_append(formattedString, buffer);
                     formatPtr ++;
                     break;
                  case 2:
                     sprintf(buffer, "%02i", (int) self->day);
                     formattedString = string_append(formattedString, buffer);
                     formatPtr += 2;
                     break;
                  case 3:
                     strncpy(buffer, weekdayNames[ dttm_weekday(self) ], 3);
                     buffer[3] = '\0';
                     if (*formatPtr == 'D') {
                        if ( formatPtr[ 1 ] == 'D' )
                           str_to_upper(buffer);
                        else
                           buffer[ 0 ] = (char)toupper((int) buffer[ 0 ]);
                     }
                     formattedString = string_append(formattedString, buffer);
                     formatPtr += 3;
                     break;
                  case 4:
                  default:
                     strcpy( buffer, weekdayNames[ dttm_weekday(self) ] );
                     if (*formatPtr == 'D') {
                        if (formatPtr[ 1 ] == 'D')
                           str_to_upper(buffer);
                        else
                           buffer[ 0 ] = (char)toupper((int) buffer[ 0 ]);
                     }
                     formattedString = string_append(formattedString, buffer);
                     formatPtr += 4;
                     break;
               }
               break;
               
            case 'o':
            case 'O':
               if ( 10 < self->day && self->day < 20 ) {
                  strcpy(buffer, "th");
               } else {
                  switch (self->day % 10) {
                     case 1:
                        strcpy(buffer, "st");
                        break;
                     case 2:
                        strcpy(buffer, "nd");
                        break;
                     case 3:
                        strcpy(buffer, "rd");
                        break;
                     default:
                        strcpy(buffer, "th");
                        break;
                  }

               }
               if (*formatPtr == 'O')
                  str_to_upper(buffer);
               formattedString = string_append(formattedString, buffer);
               formatPtr ++;
               break;
               
            case 'm':
            case 'M':
               switch ( strspn(formatPtr, "Mm")) {
                  case 1:
                     sprintf(buffer, "%i", (int)self->month);
                     formattedString = string_append(formattedString, buffer);
                     formatPtr ++;
                     break;
                  case 2:
                     sprintf( buffer, "%02i", (int) self->month );
                     formattedString = string_append(formattedString, buffer);
                     formatPtr += 2;
                     break;
                  case 3:
                     strncpy(buffer, monthNames[ self->month ], 3);
                     buffer[3] = '\0';
                     if (*formatPtr == 'M') {
                        if (formatPtr[ 1 ] == 'M')
                           str_to_upper(buffer);
                        else
                           buffer[ 0 ] = (char)toupper((int)buffer[ 0 ]);
                     }
                     formattedString = string_append(formattedString, buffer);
                     formatPtr += 3;
                     break;
                  case 4:
                  default:
                     strcpy(buffer, monthNames[ self->month ] );
                     if ( *formatPtr == 'M' ) {
                        if ( formatPtr[ 1 ] == 'M' )
                           str_to_upper( buffer );
                        else
                           buffer[ 0 ] = (char) toupper( (int) buffer[ 0 ] );
                     }
                     formattedString = string_append(formattedString, buffer);
                     formatPtr += 4;
                     break;
               }
               break;
            
            case 'y':
            case 'Y':
            {
               int Y_count = (int) strspn( formatPtr, "Yy" );
               if (Y_count < 4) {
                  int year = self->year - 100 * (self->year / 100);
                  sprintf(buffer, "%02i", year);
                  formattedString = string_append(formattedString, buffer);
                  formatPtr += Y_count;
               }
               else {
                  sprintf(buffer, "%i", self->year);
                  formattedString = string_append(formattedString, buffer);
                  formatPtr += 4;
               }
               break;
            }
            
            default:
               formattedString = string_append_char(formattedString, '%');
               break;
         }
      }
      else if ( *formatPtr == '@' ) {
         formatPtr ++;
         if ( *formatPtr == '\0' ) {
            formattedString = string_append_char(formattedString, '%');
            break;
         }
         
         switch (*formatPtr) {
            case 'h':
            case 'H':
            {
               int hour = self->hour;
               if (*formatPtr == 'h' && self->hour > 12) hour -= 12;
               formatPtr ++;
               if (toupper(*formatPtr) == 'H') {
                  sprintf(buffer, "%02i", hour);
                  formattedString = string_append(formattedString, buffer);
                  formatPtr ++;
               }
               else {
                  sprintf( buffer, "%i", hour);
                  formattedString = string_append(formattedString, buffer);
               }
               break;
            }
            
            case 'M':
            case 'm':
               formatPtr ++;
               if (toupper(*formatPtr) == 'M') {
                  sprintf(buffer, "%02i", (int)self->minute);
                  formattedString = string_append(formattedString, buffer);
                  formatPtr ++;
               }
               else {
                  sprintf(buffer, "%i", (int)self->minute);
                  formattedString = string_append(formattedString, buffer);
               }
               break;
               
            case 'S':
            case 's':
               formatPtr ++;
               if (toupper(*formatPtr) == 'S') {
                  sprintf( buffer, "%02i", (int)self->second);
                  formattedString = string_append(formattedString, buffer);
                  formatPtr ++;
               }
               else {
                  sprintf( buffer, "%i", (int)self->second);
                  formattedString = string_append(formattedString, buffer);
               }
               break;
            
            case 'P':
               if ( self->hour >= 12 )
                  formattedString = string_append(formattedString, "PM");
               else
                  formattedString = string_append(formattedString, "AM");
               formatPtr ++;
               break;
               
            case 'p':
               if ( self->hour >= 12 )
                  formattedString = string_append(formattedString, "pm");
               else
                  formattedString = string_append(formattedString, "am");
               formatPtr ++;
               break;
            
            case '@':
               formattedString = string_append_char(formattedString, '@');
               formatPtr ++;
               break;
         
            default:
               formattedString = string_append_char(formattedString, '@');
               break;
         }
      }
      else {
         if ( *formatPtr == '^' )
            formatPtr ++;
         else {
            formattedString = string_append_char(formattedString, *formatPtr);
            formatPtr ++;
         }
      }
   }

   return string_finalize(formattedString);
}

double dttm_sun_time(long julianDayNumber, double latitude, double longitude, bool isSunrise)
{
    const double degToRad = M_PI / 180.0;
    const double radToDeg = 180.0 / M_PI;
    const double zenith = 90.833 * degToRad;   // official sunrise/sunset zenith

    double longitudeHour = longitude / 15.0;

    // Step 1: Approximate solar time
    double approxSolarTime = julianDayNumber + ((isSunrise ? 6.0 : 18.0) - longitudeHour) / 24.0;

    // Step 2: Sun's mean anomaly
    double solarMeanAnomaly = (0.9856 * (approxSolarTime - 2451545.0)) - 3.289;

    // Step 3: Sun's true longitude
    double sunsTrueLongitude = solarMeanAnomaly
        + 1.916 * sin(solarMeanAnomaly * degToRad)
        + 0.020 * sin(2.0 * solarMeanAnomaly * degToRad)
        + 282.634;

    sunsTrueLongitude = fmod(sunsTrueLongitude, 360.0);
    if (sunsTrueLongitude < 0.0) sunsTrueLongitude += 360.0;

    // Step 4: Sun's right ascension
    double sunsRightAscension = radToDeg * atan(0.91764 * tan(sunsTrueLongitude * degToRad));

    sunsRightAscension = fmod(sunsRightAscension, 360.0);
    if (sunsRightAscension < 0.0) sunsRightAscension += 360.0;

    // Adjust RA to same quadrant as true longitude
    double L_quadrant = floor(sunsTrueLongitude / 90.0) * 90.0;
    double RA_quadrant = floor(sunsRightAscension / 90.0) * 90.0;
    sunsRightAscension += (L_quadrant - RA_quadrant);

    sunsRightAscension /= 15.0;  // convert degrees → hours

    // Step 5: Sun's declination
    double sinDeclination = 0.39782 * sin(sunsTrueLongitude * degToRad);
    double cosDeclination = cos(asin(sinDeclination));

    // Step 6: Sun's local hour angle
    double cosHourAngle = (cos(zenith) - (sinDeclination * sin(latitude * degToRad))) / (cosDeclination * cos(latitude * degToRad));

    if (cosHourAngle > 1.0) return -1.0;    // Sun never rises
    if (cosHourAngle < -1.0) return -2.0;   // Sun never sets

    double hourAngle = isSunrise ? 360.0 - radToDeg * acos(cosHourAngle) : radToDeg * acos(cosHourAngle);
    hourAngle /= 15.0;

    // Step 7: Local mean time
    double localMeanTime = hourAngle + sunsRightAscension - (0.06571 * (approxSolarTime - 2451545.0)) - 6.622;

    // Step 8: Convert to GMT
    double gmtTime = localMeanTime - longitudeHour;

    // Normalize to [0, 24)
    gmtTime = fmod(gmtTime, 24.0);
    if (gmtTime < 0.0) gmtTime += 24.0;

    return gmtTime;
}

/// @brief set the time components of a datetime object to the sunrise or sunset time for its date and a given location. 
///        This function calculates the sunrise or sunset time for the date represented by the datetime object and a given location,
///        and sets the time components of the datetime object accordingly. The location is specified by the latitude and longitude. 
///        The time zone offset is used to adjust the calculated time to the local time zone (inclusive of daylight saving time). 
///        If the calculated time is less than 0, it means that the sunrise/sunset occurs on the previous day, so we add 24 hours 
///        to the time and subtract one day from the datetime object. If the calculated time is greater than or equal to 24, 
///        it means that the sunrise/sunset occurs on the next day, so we subtract 24 hours from the time and add one day to the 
///        datetime object.
/// @param self the datetime object to set the time components of. The date components of the datetime object should already be set 
///        to the desired date.
/// @param latitude the latitude of the location to calculate the sunrise/sunset time for.
/// @param longitude the longitude of the location to calculate the sunrise/sunset time for.
/// @param timeZoneOffset the time zone offset in hours to adjust the calculated time to the local time zone (inclusive of daylight 
///        saving time). For example, if the local time zone is GMT+2, the timeZoneOffset should be 2.0. If the local time zone is 
///        GMT-5, the timeZoneOffset should be -5.0.
/// @param isSunrise a boolean value indicating whether to calculate the sunrise time (true) or the sunset time (false).
static void dttm_set_sun_time(dttm_t *self, double latitude, double longitude, double timeZoneOffset, bool isSunrise)
{
    long julianDayNumber = dttm_jdn(self);
    dttm_year(self);

    double time = dttm_sun_time(julianDayNumber, latitude, longitude, isSunrise);
    if (time < 0.0) {
        self->hour = 0;
        return; // Sunrise/sunset cannot be calculated for this date and location (e.g. polar night)
    }

    if (timeZoneOffset == DBL_MAX) {
        timeZoneOffset = dttm_tz_offset(self);
        if (timeZoneOffset == DBL_MAX) {
            timeZoneOffset = 0.0; // Fallback to GMT if time zone offset cannot be calculated
        }
    }

    time += timeZoneOffset;

    if (time < 0.0) {
        time += 24.0;
        dttm_add_days(self, -1);
    }
    else if (time >= 24.0) {
        time -= 24.0;
        dttm_add_days(self, 1);
    }

    // Update the time components of the datetime object
    self->hour = (uint8_t)time;
    self->minute = (uint8_t)((time - self->hour) * 60.0);
    self->second = 0.0;
}

/// @brief initialise a datetime object with the sunrise or sunset time for a given date and location. This function calculates 
///        the sunrise or sunset time for a given date and location, and sets the time components of the datetime object accordingly.
///        The date is specified by the Julian Day Number, and the location is specified by the latitude and longitude. The time zone
///        offset is also taken into account to adjust the time to the local time zone (inclusive of daylight saving time). If the
///        calculated time is less than 0, it means that the sunrise/sunset occurs on the previous day, so we add 24 hours to the 
///        time and subtract one day from the datetime object. If the calculated time is greater than or equal to 24, it means that
///        the sunrise/sunset occurs on the next day, so we subtract 24 hours from the time and add one day to the datetime object.
///        Finally, we update the hour, minute, and second components of the datetime object based on the calculated time.
/// @param self the datetime object to initialise with the sunrise or sunset time.
/// @param julianDayNumber the Julian Day Number for the date to calculate the sunrise/sunset time for.
/// @param latitude the latitude of the location to calculate the sunrise/sunset time for.
/// @param longitude the longitude of the location to calculate the sunrise/sunset time for.
/// @param timeZoneOffset the time zone offset in hours to adjust the calculated time to the local time zone (inclusive of daylight 
///        saving time). For example, if the local time zone is GMT+2, the timeZoneOffset should be 2.0. If the local time zone is 
///        GMT-5, the timeZoneOffset should be -5.0. if the timeZoneOffset is set to DBL_MAX, the function will attempt to calculate 
///        the time zone offset based on the datetime's date and the system's time zone information. Note that this may not always 
///        be accurate, especially if the datetime's date is far in the past or future, or if the system's time zone information is 
///        not up to date. Therefore, it is recommended to provide an explicit timeZoneOffset whenever possible to ensure accurate 
///        results.
/// @param isSunrise a boolean value indicating whether to calculate the sunrise time (true) or the sunset time (false).
/// @return a pointer to the initialised datetime object. If the sunrise/sunset time cannot be calculated for the given date and 
///         location (e.g. polar night), the time components of the datetime object will be set to 0, and the function will return 
///         the datetime object with the date set to the given Julian Day Number.
static dttm_t *dttm_init_sun_time(dttm_t *self, long julianDayNumber, double latitude, double longitude, 
    double timeZoneOffset, bool isSunrise)
{
    dttm_init_jdn(self, julianDayNumber);
    dttm_year(self); // This will fill in the year, month, day based on the new Julian Day

    dttm_set_sun_time(self, latitude, longitude, timeZoneOffset, isSunrise);

    return self;
}

inline dttm_t *dttm_init_sunrise(dttm_t *self, long julianDayNumber, double latitude, double longitude, double timeZoneOffset)
{
    return dttm_init_sun_time(self, julianDayNumber, latitude, longitude, timeZoneOffset, true);
}

inline dttm_t *dttm_init_sunset(dttm_t *self, long julianDayNumber, double latitude, double longitude, double timeZoneOffset)
{
    return dttm_init_sun_time(self, julianDayNumber, latitude, longitude, timeZoneOffset, false);
}

inline void dttm_set_sunrise(dttm_t *self, double latitude, double longitude, double timeZoneOffset)
{
    dttm_set_sun_time(self, latitude, longitude, timeZoneOffset, true);
}

inline void dttm_set_sunset(dttm_t *self, double latitude, double longitude, double timeZoneOffset)
{
    dttm_set_sun_time(self, latitude, longitude, timeZoneOffset, false);
}

// reference julian day number of a recent full moon
#define NEWMOON_JDN 2451550

// length of a synodic month
#define SYNODIC_MONTH_LENGTH 29.53058867

/// @brief calculate the moon phase for a given Julian Day Number. The moon phase is calculated based on the difference between the given
///        Julian Day Number and a known new moon date, divided by the length of a synodic month (the average time between new moons). The result is then multiplied by 8 and
/// @param julianDayNumber the Julian Day Number to calculate the moon phase for. The Julian Day Number is a continuous count of days since the beginning of the Julian Period, which is used in astronomy and other fields to represent dates. It is calculated based on the date and time, and can be used to determine the position of celestial bodies, including the moon phase. 
/// @return the moon phase for the given Julian Day Number. The moon phases are typically categorized as follows:
///         DT_NewMoon:        New Moon
///         DT_WaxingCrescent: Waxing Crescent
///         DT_FirstQuarter:   First Quarter
///         DT_WaxingGibbous:  Waxing Gibbous
///         DT_FullMoon:       Full Moon
///         DT_WaningGibbous:  Waning Gibbous
///         DT_LastQuarter:    Last Quarter
///         DT_WaningCrescent: Waning Crescent
static moon_phase_t dttm_moon_phase_on_jdn(long julianDayNumber)
{
    // The moon phase is calculated based on the difference between the given Julian Day Number and a known new moon date,
    // divided by the length of a synodic month (the average time between new moons).
    // The result is then multiplied by 8 and rounded to get an integer value representing the moon phase.
    // The moon phases are typically categorized as follows:
    // 0: New Moon
    // 1: Waxing Crescent
    // 2: First Quarter
    // 3: Waxing Gibbous
    // 4: Full Moon
    // 5: Waning Gibbous
    // 6: Last Quarter
    // 7: Waning Crescent

    double moonPhase = fmod((julianDayNumber - NEWMOON_JDN) / SYNODIC_MONTH_LENGTH, 1.0);
    if (moonPhase < 0) {
        moonPhase += 1.0; // Ensure moonPhase is in the range [0, 1)
    }

    return (moon_phase_t)(int)(moonPhase * 8 + 0.5) % 8; // Round to nearest integer and wrap around using modulo
}

moon_phase_t dttm_moon_phase(const dttm_t *self)
{
    long julianDayNumber = dttm_jdn(self);
    if (julianDayNumber == LONG_MAX) {
        return DT_NewMoon; // Default value if datetime is not initialised
    }
    return dttm_moon_phase_on_jdn(julianDayNumber);
}

dttm_t *dttm_next_moon_phase(const dttm_t *self, moon_phase_t phase)
{
    static const double phase_fraction = 0.125; // Each moon phase corresponds to 1/8 of the synodic month

    if (self == NULL) return NULL; // Invalid input

    if (self->year == SHRT_MAX) {
        if (dttm_year(self) == SHRT_MAX) {
            return NULL; // Datetime is not initialised
        }
    }

    long jdn = dttm_jdn(self);

    // Current phase fraction (0 = New Moon, 0.5 = Full Moon, etc.)
    double currentPhase = fmod((jdn - NEWMOON_JDN) / SYNODIC_MONTH_LENGTH, 1.0);
    if (currentPhase < 0.0)
        currentPhase += 1.0;

    // Target phase fraction
    double targetPhase = (double)phase * phase_fraction;

    // Compute how far ahead the next target phase is
    double delta = targetPhase - currentPhase;

    delta = fmod(delta + 1.0, 1.0);
    if (delta <= 0.03386) delta += 1.0;
    
    // Convert phase fraction difference → days
    double daysToAdd = delta * SYNODIC_MONTH_LENGTH;

    // Create new datetime and add fractional days
    dttm_t *result = dttm_init_copy(dttm_alloc(), self);
    dttm_add_days(result, daysToAdd);

    return result;
}

dttm_t *dttm_next_weekday(const dttm_t *self, weekday_t weekday)
{
    if (self == NULL) return NULL; // Invalid input

    if (self->year == SHRT_MAX) {
        if (dttm_year(self) == SHRT_MAX) {
            return NULL; // Datetime is not initialised
        }
    }

    weekday_t currentWeekday = dttm_weekday(self);

    int daysToAdd = (weekday - currentWeekday + 7) % 7;
    if (daysToAdd == 0) daysToAdd = 7; // If the target weekday is the same as the current, we want the next occurrence

    dttm_t *result = dttm_init_copy(dttm_alloc(), self);
    dttm_add_days(result, daysToAdd);

    return result;
}