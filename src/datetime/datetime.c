/* datetime.c - implementation of the datetime type and calendar arithmetic
 *
 * The datetime_t struct is defined here (not in the public header) so that
 * callers work entirely through the opaque pointer API.
 *
 * Uninitialised-field sentinels:
 *   year             — SHRT_MAX  (no valid Gregorian year reaches this)
 *   JulianDayNumber  — LONG_MAX
 *   JulianDay        — DBL_MAX
 * Functions that need these fields check for the sentinel before using the
 * cached value, computing and caching on first access (lazy initialisation).
 *
 * Julian Day Number (JDN) is the integer day count used as the canonical
 * internal representation for date arithmetic.  The fractional Julian Day
 * (JD) additionally encodes the time of day and is used for astronomical
 * calculations (sunrise/sunset, moon phase).
 */

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
#include "ustring.h"

/**
 * @brief the datetime type - this is the internal structure definition for the datetime type. It is not intended to be used
 *        directly by users of the datetime library, but it is needed for the implementation of the datetime functions and
 *        for testing purposes.
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

static int datetime_day_of_year(const datetime_t *dttm);
static datetime_sun_status_t datetime_sun_status_from_raw(double raw_time);

/**
 * @brief allocates enough memory for a datetime_t structure.
 * @return the start address of the allocated memory.
 */
datetime_t *datetime_alloc() {
    datetime_t *dttm = (datetime_t *)malloc(sizeof(datetime_t));
    if (dttm != NULL) {
        dttm->year = SHRT_MAX; // Mark as uninitialised
        dttm->month = 0;
        dttm->day = 0;
        dttm->hour = 0;
        dttm->minute = 0;
        dttm->second = 0.0;
        dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
        dttm->JulianDay = DBL_MAX; // Mark as uninitialised
    }
    return dttm;
}

inline void datetime_dealloc(datetime_t *dttm)
{
    if (!dttm)
        return;
    free(dttm);
}

datetime_t *datetime_init_ymd(datetime_t *dttm, short year, month_t month, uint8_t day) {
    dttm->year = year;
    dttm->month = month;
    dttm->day = day;
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    dttm->JulianDay = DBL_MAX; // Mark as uninitialised
    return dttm;
}

datetime_t *datetime_init_ymdt(datetime_t *dttm,
    short year, month_t month, uint8_t day, uint8_t hour, uint8_t minute, double second)
{
    dttm->year = year;
    dttm->month = month;
    dttm->day = day;
    dttm->hour = hour;
    dttm->minute = minute;
    dttm->second = second;
    dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    dttm->JulianDay = DBL_MAX; // Mark as uninitialised
    return dttm;
}

datetime_t *datetime_init_copy(datetime_t *dttm_dest, const datetime_t *dttm_src) {
    dttm_dest->year = dttm_src->year;
    dttm_dest->month = dttm_src->month;
    dttm_dest->day = dttm_src->day;
    dttm_dest->hour = dttm_src->hour;
    dttm_dest->minute = dttm_src->minute;
    dttm_dest->second = dttm_src->second;
    dttm_dest->JulianDayNumber = dttm_src->JulianDayNumber;
    dttm_dest->JulianDay = dttm_src->JulianDay;
    return dttm_dest;
}

datetime_t *datetime_init_jdn(datetime_t *dttm, long JulianDayNumber) {
    dttm->JulianDayNumber = JulianDayNumber;
    dttm->JulianDay = DBL_MAX; // Mark as uninitialised
    dttm->year = SHRT_MAX; // Mark as uninitialised
    dttm->month = 0;
    dttm->day = 0;
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    // The actual conversion to year, month, day, etc. will be done lazily when needed.
    return dttm;
}

datetime_t *datetime_init_jd(datetime_t *dttm, double JulianDay) {
    dttm->JulianDay = JulianDay;
    dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    dttm->year = SHRT_MAX; // Mark as uninitialised
    dttm->month = 0;
    dttm->day = 0;
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    // The actual conversion to year, month, day, etc. will be done lazily when needed.
    return dttm;
}

datetime_t *datetime_init_now(datetime_t *dttm) {
    time_t now;
	struct tm tmdate;

    time(&now);
    tmdate = *localtime(&now);
    dttm->year = (short)(tmdate.tm_year + 1900);
    dttm->month = (month_t)(tmdate.tm_mon + 1);
    dttm->day = (uint8_t)tmdate.tm_mday;
    dttm->hour = (uint8_t)tmdate.tm_hour;
    dttm->minute = (uint8_t)tmdate.tm_min;
    dttm->second = (double)tmdate.tm_sec;
    dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    dttm->JulianDay = DBL_MAX; // Mark as uninitialised
    // The actual conversion to Julian Day Number and Julian Day will be done lazily when needed

    return dttm;
}

datetime_t *datetime_from_string(const char *text)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int consumed = 0;
    datetime_t *dttm;

    if (!text)
        return NULL;

    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return NULL;

    if (sscanf(text, "%d-%d-%d %n", &year, &month, &day, &consumed) == 3) {
        if (text[consumed] != '\0')
            return NULL;
    } else if (sscanf(text, "%d/%d/%d %n", &day, &month, &year, &consumed) == 3) {
        if (text[consumed] != '\0')
            return NULL;
    } else {
        return NULL;
    }

    if (!datetime_valid_ymd((short)year, (month_t)month, (uint8_t)day))
        return NULL;

    dttm = datetime_alloc();
    if (!dttm)
        return NULL;

    return datetime_init_ymd(dttm, (short)year, (month_t)month, (uint8_t)day);
}

bool datetime_serialize(const datetime_t *dttm,
                        string_t **out_type,
                        string_t **out_encoding,
                        void **out_data,
                        size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    char buffer[128];
    int n;
    char *payload;

    if (!dttm || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    n = snprintf(buffer,
                 sizeof(buffer),
                 "%04d-%02d-%02dT%02d:%02d:%09.6f",
                 (int)datetime_year(dttm),
                 (int)datetime_month(dttm),
                 (int)datetime_day(dttm),
                 (int)datetime_hour(dttm),
                 (int)datetime_minute(dttm),
                 datetime_second(dttm));
    if (n <= 0 || (size_t)n >= sizeof(buffer))
        return false;

    payload = malloc((size_t)n);
    if (!payload)
        return false;
    memcpy(payload, buffer, (size_t)n);

    type = string_new_with("datetime_t");
    encoding = string_new_with("iso8601/local-v1");
    if (!type || !encoding) {
        free(payload);
        string_free(type);
        string_free(encoding);
        return false;
    }

    *out_type = type;
    *out_encoding = encoding;
    *out_data = payload;
    *out_len = (size_t)n;
    return true;
}

datetime_t *datetime_deserialise(const void *data,
                                 size_t len,
                                 const string_t *type,
                                 const string_t *encoding)
{
    char buffer[128];
    int year;
    int month;
    int day;
    int hour;
    int minute;
    double second;
    datetime_t *dttm;

    if (!data || len == 0u || len >= sizeof(buffer) || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "datetime_t") != 0 ||
        strcmp(string_c_str(encoding), "iso8601/local-v1") != 0)
        return NULL;

    memcpy(buffer, data, len);
    buffer[len] = '\0';
    if (sscanf(buffer,
               "%d-%d-%dT%d:%d:%lf",
               &year,
               &month,
               &day,
               &hour,
               &minute,
               &second) != 6)
        return NULL;
    if (!datetime_valid_ymd((short)year, (month_t)month, (uint8_t)day))
        return NULL;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0.0 || second >= 60.0)
        return NULL;

    dttm = datetime_alloc();
    if (!dttm)
        return NULL;
    return datetime_init_ymdt(dttm,
                              (short)year,
                              (month_t)month,
                              (uint8_t)day,
                              (uint8_t)hour,
                              (uint8_t)minute,
                              second);
}

datetime_t *datetime_init_easter(datetime_t *dttm, int year)
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

    dttm->year = (short)year;
    dttm->month = (month_t)(daysIntoYear / 31);
    dttm->day    = (uint8_t)((daysIntoYear % 31) + 1);
    dttm->hour   = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    dttm->JulianDay = DBL_MAX;
    dttm->JulianDayNumber = LONG_MAX;

    return dttm;
}

static long datetime_julian_ymd_to_jdn(int year, int month, int day)
{
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
}

datetime_t *datetime_init_orthodox_easter(datetime_t *dttm, int year)
{
    int a;
    int b;
    int c;
    int d;
    int e;
    int daysIntoYear;
    int month;
    int day;

    if (!dttm || year < 1 || year > 9999)
        return NULL;

    a = year % 4;
    b = year % 7;
    c = year % 19;
    d = (19 * c + 15) % 30;
    e = (2 * a + 4 * b - d + 34) % 7;
    daysIntoYear = d + e + 114;
    month = daysIntoYear / 31;
    day = (daysIntoYear % 31) + 1;

    datetime_init_jdn(dttm, datetime_julian_ymd_to_jdn(year, month, day));
    datetime_year(dttm);
    return dttm;
}

datetime_t *datetime_init_christmas(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;

    return datetime_init_ymd(dttm, (short)year, DT_December, 25);
}

datetime_t *datetime_init_orthodox_christmas(datetime_t *dttm, int year)
{
    if (!dttm || year < 2 || year > 9999)
        return NULL;

    datetime_init_jdn(dttm, datetime_julian_ymd_to_jdn(year - 1, DT_December, 25));
    datetime_year(dttm);
    return dttm;
}

/**
 * @brief calculates the time of the true new moon for a given lunation number lunationIndex.
 * The algorithm used is the ELP2000-85 algorithm, which is a well-known method for calculating the time of the true new moon.
 * The algorithm takes the lunation number lunationIndex as input and calculates the time of the true new moon based on a series of mathematical
 * operations. The resulting time is returned as a Julian Ephemeris Date (JDE) in days.
 * @param lunationIndex the lunation number for which to calculate the time of the true new moon. This should be a positive integer.
 * @return the time of the true new moon in days as a Julian Ephemeris Date (JDE).
 */
static double datetime_true_new_moon_tt(int lunationIndex)
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

static double datetime_true_full_moon_tt(int lunationIndex)
{
    double k = lunationIndex + 0.5;
    double T  = k / 1236.85;
    double T2 = T * T;
    double T3 = T2 * T;
    double T4 = T3 * T;
    double jdeMean = 2451550.09765 + 29.530588853 * k + 0.0001337 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;
    double sunMeanAnomaly = 2.5534 + 29.10535670 * k - 0.0000014 * T2 - 0.00000011 * T3;
    double moonMeanAnomaly = 201.5643 + 385.81693528 * k + 0.0107582 * T2 + 0.00001238 * T3 - 0.000000058 * T4;
    double moonArgumentLatitude = 160.7108 + 390.67050284 * k - 0.0016118 * T2 - 0.00000227 * T3 + 0.000000011 * T4;
    double ascendingNodeLongitude = 124.7746 - 1.56375580 * k + 0.0020691 * T2 + 0.00000215 * T3;
    const double degToRad = M_PI / 180.0;
    double E = 1 - 0.002516 * T - 0.0000074 * T2;
    double correction;

    sunMeanAnomaly *= degToRad;
    moonMeanAnomaly *= degToRad;
    moonArgumentLatitude *= degToRad;
    ascendingNodeLongitude *= degToRad;

    correction =
        -0.40614 * sin(moonMeanAnomaly)
        + 0.17302 * E * sin(sunMeanAnomaly)
        + 0.01614 * sin(2 * moonMeanAnomaly)
        + 0.01043 * sin(2 * moonArgumentLatitude)
        + 0.00734 * E * sin(moonMeanAnomaly - sunMeanAnomaly)
        - 0.00515 * E * sin(moonMeanAnomaly + sunMeanAnomaly)
        + 0.00209 * E * E * sin(2 * sunMeanAnomaly)
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

/**
 * @brief calculate the difference in seconds between Terrestrial Time (TT) and Universal Time (UT) for a given year.
 *
 * The difference between TT and UT is needed to convert between the two time standards. TT is a modern continuation of Ephemeris Time (ET),
 * and is the time standard used in astronomy. UT is the time standard used in everyday life. The difference between TT and UT is
 * caused by the Earth's slightly irregular rotation, and is usually expressed in seconds.
 *
 * The algorithm used to calculate the difference is based on the IAU SOFA series, which provides a set of polynomial expressions to
 * calculate the difference between TT and UT for a given year. The expressions are based on a fit of historical astronomical data, and are
 * accurate to within 0.1 seconds for years between 1800 and 2500.
 *
 * @param year the year for which to calculate the difference between TT and UT. This should be a positive integer between 1800 and 2500,
 *        inclusive. If the input year is outside this range, the function will return an approximate value based on the nearest valid
 *        year.
 * @return the difference in seconds between TT and UT for the given year.
 */
static double datetime_delta_t_estimate(int year)
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
static double datetime_dec_solstice_tt(int year)
{
    double Y = (year - 2000) / 1000.0;  /* millennia from J2000.0 */
    return (((-0.000000217 * Y - 0.00000084) * Y + 0.000461) * Y + 365242.74049) * Y + 2451900.05952;
}

/**
 * @brief Compute the Julian day number of the Chinese New Year for a given year.
 * This function takes a year between 1700 and 2400 and returns the Julian day number of the Chinese New Year in that year.
 * @param year The year for which the Julian day number of the Chinese New Year is to be computed.
 * @return The Julian day number of the Chinese New Year in the given year.
 */
static long datetime_chinese_new_year_jdn(int year)
{
    if (year < 1700 || year > 2400)
        return LONG_MAX;

    /* 1. December solstice of the previous year (Terrestrial Time) */
    double solsticeTerrestrialTime = datetime_dec_solstice_tt(year - 1);

    /* Convert solstice from Terrestrial Time to UTC */
    double deltaTPreviousYearDays = datetime_delta_t_estimate(year - 1) / 86400.0;
    double solsticeUTC = solsticeTerrestrialTime - deltaTPreviousYearDays;

    /* 2. Estimate lunation index for new moons around the solstice */
    int lunationIndex = (int)((solsticeUTC - 2451550.09765) / 29.530588853) - 2;

    /* 3. First new moon after the solstice */
    double firstNewMoonTerrestrial = datetime_true_new_moon_tt(lunationIndex);
    double firstNewMoonUTC = firstNewMoonTerrestrial - deltaTPreviousYearDays;

    while (firstNewMoonUTC < solsticeUTC) {
        lunationIndex++;
        firstNewMoonTerrestrial = datetime_true_new_moon_tt(lunationIndex);
        firstNewMoonUTC = firstNewMoonTerrestrial - deltaTPreviousYearDays;
    }

    /* 4. Second new moon = Chinese New Year */
    double secondNewMoonTerrestrial = datetime_true_new_moon_tt(lunationIndex + 1);
    double deltaTCurrentYearDays = datetime_delta_t_estimate(year) / 86400.0;
    double secondNewMoonUTC = secondNewMoonTerrestrial - deltaTCurrentYearDays;

    /* Convert UTC → China Standard Time (UTC+8) */
    double secondNewMoonCST = secondNewMoonUTC + (8.0 / 24.0);

    /* Round to nearest civil day in CST */
    return (long)floor(secondNewMoonCST + 0.5);
}

datetime_t *datetime_init_chinese_new_year(datetime_t *dttm, int year)
{
    // algorithm not reliable for years before 1700 or after 2400
    if (year < 1700 || year > 2400) return NULL;

    dttm->JulianDayNumber = datetime_chinese_new_year_jdn(year);
    dttm->hour   = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    dttm->JulianDay = DBL_MAX;

    datetime_year(dttm);

    return dttm;
}

static datetime_t *datetime_init_materialised_jdn(datetime_t *dttm, long jdn)
{
    if (!dttm || jdn == LONG_MAX)
        return NULL;
    datetime_init_jdn(dttm, jdn);
    datetime_year(dttm);
    return dttm;
}

static long datetime_islamic_ymd_to_jdn(int year, int month, int day)
{
    return (long)(day
        + (int)ceil(29.5 * (month - 1))
        + (year - 1) * 354
        + (3 + 11 * year) / 30
        + 1948439 - 1);
}

static long datetime_india_new_moon_jdn_in_window(int year,
                                                  month_t startMonth,
                                                  uint8_t startDay,
                                                  month_t endMonth,
                                                  uint8_t endDay);
static long datetime_local_new_moon_jdn_in_window(int year,
                                                  month_t startMonth,
                                                  uint8_t startDay,
                                                  month_t endMonth,
                                                  uint8_t endDay,
                                                  double gmtOffsetHours);

static datetime_t *datetime_init_civil_islamic_observance(datetime_t *dttm,
                                                          int gregorianYear,
                                                          int islamicMonth,
                                                          int islamicDay)
{
    int estimateIslamicYear;

    if (!dttm || gregorianYear < 1 || gregorianYear > 9999)
        return NULL;

    estimateIslamicYear = (int)floor(((gregorianYear - 622) * 33.0) / 32.0) + 1;
    for (int islamicYear = estimateIslamicYear - 2; islamicYear <= estimateIslamicYear + 2; islamicYear++) {
        long jdn;
        datetime_t probe;

        if (islamicYear < 1)
            continue;

        jdn = datetime_islamic_ymd_to_jdn(islamicYear, islamicMonth, islamicDay);
        datetime_init_jdn(&probe, jdn);
        datetime_year(&probe);
        if (probe.year == gregorianYear)
            return datetime_init_materialised_jdn(dttm, jdn);
    }

    return NULL;
}

datetime_t *datetime_init_ramadan(datetime_t *dttm, int year)
{
    return datetime_init_civil_islamic_observance(dttm, year, 9, 1);
}

datetime_t *datetime_init_eid_al_fitr(datetime_t *dttm, int year)
{
    return datetime_init_civil_islamic_observance(dttm, year, 10, 1);
}

datetime_t *datetime_init_muslim_new_year(datetime_t *dttm, int year)
{
    return datetime_init_civil_islamic_observance(dttm, year, 1, 1);
}

static long datetime_hebrew_new_year_jdn(int hebrewYear)
{
    const long hebrewEpochRd = -1373427L;
    long monthsElapsed = (235L * hebrewYear - 234L) / 19L;
    long partsElapsed = 12084L + 13753L * monthsElapsed;
    long day = 29L * monthsElapsed + partsElapsed / 25920L;

    if ((3L * (day + 1L)) % 7L < 3L)
        day++;

    return hebrewEpochRd + day + 1721425L;
}

datetime_t *datetime_init_jewish_new_year(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;

    return datetime_init_materialised_jdn(dttm, datetime_hebrew_new_year_jdn(year + 3761));
}

datetime_t *datetime_init_passover(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;

    return datetime_init_materialised_jdn(dttm, datetime_hebrew_new_year_jdn(year + 3761) - 163L);
}

static double datetime_normalise_degrees(double degrees)
{
    double out = fmod(degrees, 360.0);
    if (out < 0.0)
        out += 360.0;
    return out;
}

static double datetime_solar_ecliptic_longitude(double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double L0 = datetime_normalise_degrees(280.46646 + 36000.76983 * T + 0.0003032 * T * T);
    double M = datetime_normalise_degrees(357.52911 + 35999.05029 * T - 0.0001537 * T * T);
    double Mrad = M * M_PI / 180.0;
    double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * sin(Mrad)
             + (0.019993 - 0.000101 * T) * sin(2.0 * Mrad)
             + 0.000289 * sin(3.0 * Mrad);
    double omega = (125.04 - 1934.136 * T) * M_PI / 180.0;

    return datetime_normalise_degrees(L0 + C - 0.00569 - 0.00478 * sin(omega));
}

static long datetime_local_new_moon_jdn_for_lunation(int lunationIndex,
                                                     int year,
                                                     double gmtOffsetHours)
{
    double newMoonTT = datetime_true_new_moon_tt(lunationIndex);
    double newMoonUTC = newMoonTT - datetime_delta_t_estimate(year) / 86400.0;
    double localNewMoon = newMoonUTC + gmtOffsetHours / 24.0;

    return (long)floor(localNewMoon + 0.5);
}

static long datetime_next_local_new_moon_jdn(long afterJdn,
                                             int year,
                                             double gmtOffsetHours)
{
    int lunationIndex = (int)floor((afterJdn - 2451550.09765) / 29.530588853) - 2;
    long best = LONG_MAX;

    for (int i = 0; i < 10; i++, lunationIndex++) {
        long newMoonJdn = datetime_local_new_moon_jdn_for_lunation(lunationIndex,
                                                                   year,
                                                                   gmtOffsetHours);
        if (newMoonJdn > afterJdn && newMoonJdn < best)
            best = newMoonJdn;
    }

    return best;
}

static bool datetime_lunar_month_contains_principal_term(long startJdn,
                                                         long endJdn)
{
    double startLongitude;
    double endLongitude;
    double travelled;
    int firstPrincipalTerm;

    if (startJdn == LONG_MAX || endJdn == LONG_MAX || startJdn >= endJdn)
        return false;

    startLongitude = datetime_solar_ecliptic_longitude((double)startJdn - 0.5);
    endLongitude = datetime_solar_ecliptic_longitude((double)endJdn - 0.5);
    travelled = datetime_normalise_degrees(endLongitude - startLongitude);
    firstPrincipalTerm = (int)floor(startLongitude / 30.0) + 1;

    return firstPrincipalTerm * 30.0 <= startLongitude + travelled + 0.25;
}

static void datetime_jdn_to_julian_ymd(long jdn, int *year, int *month, int *day)
{
    long c = jdn + 32082L;
    long d = (4L * c + 3L) / 1461L;
    long e = c - (1461L * d) / 4L;
    long m = (5L * e + 2L) / 153L;

    *day = (int)(e - (153L * m + 2L) / 5L + 1L);
    *month = (int)(m + 3L - 12L * (m / 10L));
    *year = (int)(d - 4800L + m / 10L);
}

string_t *datetime_christian_calendar_date_text(const datetime_t *dttm)
{
    string_t *format;
    string_t *gregorian;
    string_t *out;
    int julianYear;
    int julianMonth;
    int julianDay;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    format = string_new_with("%yyyy-%mm-%dd");
    gregorian = format ? datetime_format_text(dttm, format) : NULL;
    string_free(format);
    if (!gregorian)
        return NULL;

    datetime_jdn_to_julian_ymd(datetime_jdn(dttm), &julianYear, &julianMonth, &julianDay);
    out = string_new();
    if (!out ||
        string_append_format(out,
                             "Gregorian %s; Julian %04d-%02d-%02d",
                             string_c_str(gregorian),
                             julianYear,
                             julianMonth,
                             julianDay) < 0) {
        string_free(gregorian);
        string_free(out);
        return NULL;
    }

    string_free(gregorian);
    return out;
}

string_t *datetime_chinese_calendar_date_text(const datetime_t *dttm)
{
    static const char *zodiac[] = {
        "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
        "Horse", "Goat", "Monkey", "Rooster", "Dog", "Pig"
    };
    long starts[16];
    int gregorianYear;
    int chineseYearStart;
    int monthCount = 1;
    int currentMonthIndex = 0;
    int leapMonthIndex = -1;
    int lunarMonth;
    int lunarDay;
    int zodiacIndex;
    long jdn;
    long cny;
    long nextCny;
    string_t *out;

    if (!dttm)
        return NULL;

    gregorianYear = datetime_year(dttm);
    jdn = datetime_jdn(dttm);
    if (gregorianYear < 1700 || gregorianYear > 2400 || jdn == LONG_MAX)
        return NULL;

    chineseYearStart = gregorianYear;
    cny = datetime_chinese_new_year_jdn(chineseYearStart);
    if (cny == LONG_MAX)
        return NULL;
    if (jdn < cny) {
        chineseYearStart--;
        cny = datetime_chinese_new_year_jdn(chineseYearStart);
    }
    nextCny = datetime_chinese_new_year_jdn(chineseYearStart + 1);
    if (cny == LONG_MAX || nextCny == LONG_MAX || jdn < cny || jdn >= nextCny)
        return NULL;

    starts[0] = cny;
    while (monthCount < 15) {
        long nextStart = datetime_next_local_new_moon_jdn(starts[monthCount - 1],
                                                          chineseYearStart,
                                                          8.0);
        if (nextStart == LONG_MAX || nextStart >= nextCny)
            break;
        starts[monthCount++] = nextStart;
    }

    if (nextCny - cny > 360L) {
        for (int i = 1; i < monthCount; i++) {
            long endJdn = (i + 1 < monthCount) ? starts[i + 1] : nextCny;
            if (!datetime_lunar_month_contains_principal_term(starts[i], endJdn)) {
                leapMonthIndex = i;
                break;
            }
        }
    }

    for (int i = 0; i < monthCount; i++) {
        long endJdn = (i + 1 < monthCount) ? starts[i + 1] : nextCny;
        if (jdn >= starts[i] && jdn < endJdn) {
            currentMonthIndex = i;
            break;
        }
    }

    lunarDay = (int)(jdn - starts[currentMonthIndex] + 1L);
    lunarMonth = currentMonthIndex + 1;
    if (leapMonthIndex >= 0) {
        if (currentMonthIndex == leapMonthIndex)
            lunarMonth = currentMonthIndex;
        else if (currentMonthIndex > leapMonthIndex)
            lunarMonth--;
    }
    zodiacIndex = (chineseYearStart - 4) % 12;
    if (zodiacIndex < 0)
        zodiacIndex += 12;

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "Year %d (%s), %smonth %d, day %d",
                             chineseYearStart + 2698,
                             zodiac[zodiacIndex],
                             currentMonthIndex == leapMonthIndex ? "leap " : "",
                             lunarMonth,
                             lunarDay) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static const char *datetime_hindu_month_name(int month)
{
    static const char *names[] = {
        NULL, "Chaitra", "Vaishakha", "Jyeshtha", "Ashadha",
        "Shravana", "Bhadrapada", "Ashvina", "Kartika",
        "Margashirsha", "Pausha", "Magha", "Phalguna"
    };

    return month >= 1 && month <= 12 ? names[month] : "Unknown";
}

string_t *datetime_hindu_calendar_date_text(const datetime_t *dttm)
{
    long starts[16];
    long jdn;
    long newYearJdn;
    long nextYearJdn;
    int gregorianYear;
    int hinduYearStart;
    int monthCount = 1;
    int monthIndex = 0;
    int month;
    int lunarDay;
    int vikramSamvatYear;
    int tithi;
    const char *paksha;
    string_t *out;

    if (!dttm)
        return NULL;

    gregorianYear = datetime_year(dttm);
    jdn = datetime_jdn(dttm);
    if (gregorianYear < 1700 || gregorianYear > 2400 || jdn == LONG_MAX)
        return NULL;

    hinduYearStart = gregorianYear;
    newYearJdn = datetime_india_new_moon_jdn_in_window(hinduYearStart,
                                                       DT_March,
                                                       15,
                                                       DT_April,
                                                       15);
    if (newYearJdn == LONG_MAX)
        return NULL;
    if (jdn < newYearJdn) {
        hinduYearStart--;
        newYearJdn = datetime_india_new_moon_jdn_in_window(hinduYearStart,
                                                           DT_March,
                                                           15,
                                                           DT_April,
                                                           15);
    }
    nextYearJdn = datetime_india_new_moon_jdn_in_window(hinduYearStart + 1,
                                                        DT_March,
                                                        15,
                                                        DT_April,
                                                        15);
    if (newYearJdn == LONG_MAX || nextYearJdn == LONG_MAX || jdn < newYearJdn || jdn >= nextYearJdn)
        return NULL;

    starts[0] = newYearJdn;
    while (monthCount < 15) {
        long nextStart = datetime_next_local_new_moon_jdn(starts[monthCount - 1],
                                                          hinduYearStart,
                                                          5.5);
        if (nextStart == LONG_MAX || nextStart >= nextYearJdn)
            break;
        starts[monthCount++] = nextStart;
    }

    for (int i = 0; i < monthCount; i++) {
        long endJdn = (i + 1 < monthCount) ? starts[i + 1] : nextYearJdn;
        if (jdn >= starts[i] && jdn < endJdn) {
            monthIndex = i;
            break;
        }
    }

    month = monthIndex % 12 + 1;
    lunarDay = (int)(jdn - starts[monthIndex] + 1L);
    if (monthIndex + 1 < monthCount) {
        double fraction = (double)(jdn - starts[monthIndex]) /
                          (double)(starts[monthIndex + 1] - starts[monthIndex]);
        tithi = (int)floor(fraction * 30.0) + 1;
    } else {
        tithi = lunarDay;
    }
    if (tithi < 1)
        tithi = 1;
    if (tithi > 30)
        tithi = 30;
    paksha = tithi <= 15 ? "Shukla" : "Krishna";
    vikramSamvatYear = hinduYearStart + 57;

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "Vikram Samvat %d, %s %s %d, lunar day %d",
                             vikramSamvatYear,
                             datetime_hindu_month_name(month),
                             paksha,
                             tithi <= 15 ? tithi : tithi - 15,
                             lunarDay) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

string_t *datetime_buddhist_calendar_date_text(const datetime_t *dttm)
{
    string_t *out;

    if (!dttm || datetime_year(dttm) == SHRT_MAX)
        return NULL;

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "B.E. %04d-%02d-%02d (Thai solar)",
                             datetime_year(dttm) + 543,
                             (int)datetime_month(dttm),
                             (int)datetime_day(dttm)) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static void datetime_jdn_to_islamic_ymd(long jdn, int *year, int *month, int *day)
{
    *year = (int)((30L * (jdn - 1948439L) + 10646L) / 10631L);
    if (*year < 1)
        *year = 1;

    *month = (int)ceil((jdn - (29L + datetime_islamic_ymd_to_jdn(*year, 1, 1))) / 29.5) + 1;
    if (*month < 1)
        *month = 1;
    if (*month > 12)
        *month = 12;

    *day = (int)(jdn - datetime_islamic_ymd_to_jdn(*year, *month, 1) + 1L);
    while (*day < 1) {
        (*month)--;
        if (*month < 1) {
            (*year)--;
            *month = 12;
        }
        *day = (int)(jdn - datetime_islamic_ymd_to_jdn(*year, *month, 1) + 1L);
    }
    while (*month < 12 && jdn >= datetime_islamic_ymd_to_jdn(*year, *month + 1, 1)) {
        (*month)++;
        *day = (int)(jdn - datetime_islamic_ymd_to_jdn(*year, *month, 1) + 1L);
    }
}

string_t *datetime_muslim_calendar_date_text(const datetime_t *dttm)
{
    static const char *monthNames[] = {
        NULL, "Muharram", "Safar", "Rabi al-awwal", "Rabi al-thani",
        "Jumada al-awwal", "Jumada al-thani", "Rajab", "Sha'ban",
        "Ramadan", "Shawwal", "Dhu al-Qadah", "Dhu al-Hijjah"
    };
    int year;
    int month;
    int day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    datetime_jdn_to_islamic_ymd(datetime_jdn(dttm), &year, &month, &day);
    out = string_new();
    if (!out ||
        string_append_format(out, "%d %s %d AH", day, monthNames[month], year) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static bool datetime_hebrew_leap_year(int year)
{
    return ((7 * year + 1) % 19) < 7;
}

static long datetime_ethiopian_ymd_to_jdn(int year, int month, int day);
static void datetime_jdn_to_ethiopian_ymd(long jdn, int *year, int *month, int *day);
static datetime_t *datetime_init_materialised_jdn(datetime_t *dttm, long jdn);

static datetime_t *datetime_init_civil_ethiopian_observance(datetime_t *dttm,
                                                            int gregorianYear,
                                                            int ethiopianMonth,
                                                            int ethiopianDay)
{
    datetime_t probe;
    int estimateEthiopianYear;

    if (!dttm || gregorianYear < 1 || gregorianYear > 9999)
        return NULL;

    estimateEthiopianYear = gregorianYear - 8;
    for (int ethiopianYear = estimateEthiopianYear - 1; ethiopianYear <= estimateEthiopianYear + 1; ethiopianYear++) {
        long jdn = datetime_ethiopian_ymd_to_jdn(ethiopianYear, ethiopianMonth, ethiopianDay);
        datetime_init_jdn(&probe, jdn);
        if (datetime_year(&probe) == gregorianYear)
            return datetime_init_materialised_jdn(dttm, jdn);
    }
    return NULL;
}

datetime_t *datetime_init_ethiopian_new_year(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 1, 1);
}

datetime_t *datetime_init_genna(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 4, 29);
}

datetime_t *datetime_init_timkat(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 5, 11);
}

datetime_t *datetime_init_meskel(datetime_t *dttm, int year)
{
    return datetime_init_civil_ethiopian_observance(dttm, year, 1, 17);
}

datetime_t *datetime_init_fasika(datetime_t *dttm, int year)
{
    return datetime_init_orthodox_easter(dttm, year);
}

static int datetime_hebrew_year_length(int year)
{
    return (int)(datetime_hebrew_new_year_jdn(year + 1) - datetime_hebrew_new_year_jdn(year));
}

static bool datetime_hebrew_cheshvan_long(int year)
{
    return datetime_hebrew_year_length(year) % 10 == 5;
}

static bool datetime_hebrew_kislev_short(int year)
{
    return datetime_hebrew_year_length(year) % 10 == 3;
}

static int datetime_hebrew_month_length(int year, int monthIndexFromTishrei)
{
    static const int commonLengths[] = { 30, 29, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29 };
    static const int leapLengths[] = { 30, 29, 30, 29, 30, 30, 29, 30, 29, 30, 29, 30, 29 };

    if (monthIndexFromTishrei == 1 && datetime_hebrew_cheshvan_long(year))
        return 30;
    if (monthIndexFromTishrei == 2 && datetime_hebrew_kislev_short(year))
        return 29;

    if (datetime_hebrew_leap_year(year))
        return leapLengths[monthIndexFromTishrei];
    return commonLengths[monthIndexFromTishrei];
}

static const char *datetime_hebrew_month_name(int year, int monthIndexFromTishrei)
{
    static const char *commonNames[] = {
        "Tishrei", "Cheshvan", "Kislev", "Tevet", "Shevat", "Adar",
        "Nisan", "Iyar", "Sivan", "Tammuz", "Av", "Elul"
    };
    static const char *leapNames[] = {
        "Tishrei", "Cheshvan", "Kislev", "Tevet", "Shevat",
        "Adar I", "Adar II", "Nisan", "Iyar", "Sivan", "Tammuz", "Av", "Elul"
    };

    if (datetime_hebrew_leap_year(year))
        return leapNames[monthIndexFromTishrei];
    return commonNames[monthIndexFromTishrei];
}

string_t *datetime_jewish_calendar_date_text(const datetime_t *dttm)
{
    long jdn;
    long newYearJdn;
    int hebrewYear;
    int dayOfYear;
    int monthIndex;
    int monthCount;
    int day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    jdn = datetime_jdn(dttm);
    hebrewYear = datetime_year(dttm) + 3760;
    while (jdn >= datetime_hebrew_new_year_jdn(hebrewYear + 1))
        hebrewYear++;
    while (jdn < datetime_hebrew_new_year_jdn(hebrewYear))
        hebrewYear--;

    newYearJdn = datetime_hebrew_new_year_jdn(hebrewYear);
    dayOfYear = (int)(jdn - newYearJdn) + 1;
    monthCount = datetime_hebrew_leap_year(hebrewYear) ? 13 : 12;
    monthIndex = 0;
    while (monthIndex < monthCount) {
        int monthLength = datetime_hebrew_month_length(hebrewYear, monthIndex);
        if (dayOfYear <= monthLength)
            break;
        dayOfYear -= monthLength;
        monthIndex++;
    }
    if (monthIndex >= monthCount)
        return NULL;
    day = dayOfYear;

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "%d %s %d AM",
                             day,
                             datetime_hebrew_month_name(hebrewYear, monthIndex),
                             hebrewYear) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static const char *datetime_cherokee_civil_month_name(month_t month)
{
    static const char *names[] = {
        NULL,
        "Cold Moon",
        "Bony Moon",
        "Windy Moon",
        "Flower Moon",
        "Planting Moon",
        "Green Corn Moon",
        "Ripe Corn Moon",
        "Fruit Moon",
        "Nut Moon",
        "Harvest Moon",
        "Trading Moon",
        "Snow Moon"
    };

    return month >= DT_January && month <= DT_December ? names[month] : "Unknown";
}

string_t *datetime_cherokee_calendar_date_text(const datetime_t *dttm)
{
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "Cherokee civil %s, day %d, year %d",
                             datetime_cherokee_civil_month_name(datetime_month(dttm)),
                             (int)datetime_day(dttm),
                             (int)datetime_year(dttm)) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static datetime_t *datetime_init_cherokee_window_new_moon(datetime_t *dttm,
                                                          int year,
                                                          month_t startMonth,
                                                          month_t endMonth)
{
    long jdn;

    /* Cherokee Nation civil centre: Tahlequah / central time approximation. */
    jdn = datetime_local_new_moon_jdn_in_window(year,
                                                startMonth,
                                                1,
                                                endMonth,
                                                (uint8_t)datetime_days_in_month((short)year, endMonth),
                                                -6.0);
    return datetime_init_materialised_jdn(dttm, jdn);
}

datetime_t *datetime_init_cherokee_new_moon_festival(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_January, DT_January);
}

datetime_t *datetime_init_cherokee_green_corn_ceremony(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_July, DT_July);
}

datetime_t *datetime_init_cherokee_ripe_corn_ceremony(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_August, DT_August);
}

datetime_t *datetime_init_cherokee_great_new_moon_festival(datetime_t *dttm, int year)
{
    return datetime_init_cherokee_window_new_moon(dttm, year, DT_September, DT_September);
}

string_t *datetime_mayan_calendar_date_text(const datetime_t *dttm)
{
    static const char *tzolkin_names[] = {
        "Imix", "Ik'", "Ak'bal", "K'an", "Chikchan",
        "Kimi", "Manik'", "Lamat", "Muluk", "Ok",
        "Chuwen", "Eb'", "B'en", "Ix", "Men",
        "K'ib'", "Kab'an", "Etz'nab'", "Kawak", "Ajaw"
    };
    static const char *haab_names[] = {
        "Pop", "Wo'", "Sip", "Sotz'", "Sek",
        "Xul", "Yaxk'in", "Mol", "Ch'en", "Yax",
        "Sak'", "Keh", "Mak", "K'ank'in", "Muwan",
        "Pax", "K'ayab", "Kumk'u", "Wayeb"
    };
    static const long mayan_epoch_jdn = 584283L;
    long jdn;
    long days;
    long baktun;
    long katun;
    long tun;
    long uinal;
    long kin;
    long tzolkin_number;
    long tzolkin_name_index;
    long haab_count;
    long haab_month_index;
    long haab_day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    jdn = datetime_jdn(dttm);
    days = jdn - mayan_epoch_jdn;
    if (days < 0)
        return NULL;

    baktun = days / 144000L;
    days %= 144000L;
    katun = days / 7200L;
    days %= 7200L;
    tun = days / 360L;
    days %= 360L;
    uinal = days / 20L;
    kin = days % 20L;

    days = jdn - mayan_epoch_jdn;
    tzolkin_number = ((days + 3L) % 13L) + 1L;
    tzolkin_name_index = (days + 19L) % 20L;
    haab_count = (days + 348L) % 365L;
    if (haab_count < 360L) {
        haab_month_index = haab_count / 20L;
        haab_day = haab_count % 20L;
    } else {
        haab_month_index = 18L;
        haab_day = haab_count - 360L;
    }

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "Long Count %ld.%ld.%ld.%ld.%ld; Tzolk'in %ld %s; Haab %ld %s",
                             baktun,
                             katun,
                             tun,
                             uinal,
                             kin,
                             tzolkin_number,
                             tzolkin_names[tzolkin_name_index],
                             haab_day,
                             haab_names[haab_month_index]) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static void datetime_mayan_haab_components(long jdn, int *monthIndex, int *day)
{
    long haabCount = (jdn - 584283L + 348L) % 365L;

    if (haabCount < 0)
        haabCount += 365L;
    if (haabCount < 360L) {
        *monthIndex = (int)(haabCount / 20L);
        *day = (int)(haabCount % 20L);
    } else {
        *monthIndex = 18;
        *day = (int)(haabCount - 360L);
    }
}

static datetime_t *datetime_init_mayan_haab_marker(datetime_t *dttm,
                                                   int gregorianYear,
                                                   int targetMonthIndex,
                                                   int targetDay)
{
    long startJdn;
    long endJdn;

    if (!dttm || gregorianYear < 1 || gregorianYear > 9999)
        return NULL;

    startJdn = datetime_ymd_to_jdn((short)gregorianYear, DT_January, 1);
    endJdn = datetime_ymd_to_jdn((short)(gregorianYear + 1), DT_January, 1);
    for (long jdn = startJdn; jdn < endJdn; jdn++) {
        int monthIndex;
        int day;

        datetime_mayan_haab_components(jdn, &monthIndex, &day);
        if (monthIndex == targetMonthIndex && day == targetDay)
            return datetime_init_materialised_jdn(dttm, jdn);
    }
    return NULL;
}

datetime_t *datetime_init_mayan_haab_new_year(datetime_t *dttm, int year)
{
    return datetime_init_mayan_haab_marker(dttm, year, 0, 0);
}

datetime_t *datetime_init_mayan_wayeb_start(datetime_t *dttm, int year)
{
    return datetime_init_mayan_haab_marker(dttm, year, 18, 0);
}

static datetime_t *datetime_init_aztec_year_start(datetime_t *dttm, int gregorianYear)
{
    return datetime_init_ymd(dttm, gregorianYear, DT_February, 23);
}

datetime_t *datetime_init_aztec_xiuhpohualli_new_year(datetime_t *dttm, int year)
{
    return datetime_init_aztec_year_start(dttm, year);
}

datetime_t *datetime_init_aztec_nemontemi_start(datetime_t *dttm, int year)
{
    if (!dttm || year < 1 || year > 9999)
        return NULL;
    return datetime_init_ymd(dttm, (short)year, DT_February, 18);
}

string_t *datetime_aztec_calendar_date_text(const datetime_t *dttm)
{
    static const char *tonalpohualli_names[] = {
        "Cipactli", "Ehecatl", "Calli", "Cuetzpalin", "Coatl",
        "Miquiztli", "Mazatl", "Tochtli", "Atl", "Itzcuintli",
        "Ozomatli", "Malinalli", "Acatl", "Ocelotl", "Cuauhtli",
        "Cozcacuauhtli", "Ollin", "Tecpatl", "Quiahuitl", "Xochitl"
    };
    static const char *xiuhpohualli_names[] = {
        "Atlcahualo", "Tlacaxipehualiztli", "Tozoztontli", "Hueytozoztli",
        "Toxcatl", "Etzalcualiztli", "Tecuilhuitontli", "Huey Tecuilhuitl",
        "Tlaxochimaco", "Xocotlhuetzi", "Ochpaniztli", "Teotleco",
        "Tepeilhuitl", "Quecholli", "Panquetzaliztli", "Atemoztli",
        "Tititl", "Izcalli", "Nemontemi"
    };
    static const char *year_bearer_names[] = {
        "Calli", "Tochtli", "Acatl", "Tecpatl"
    };
    long jdn;
    long aztec_number;
    long aztec_name_index;
    long day_index;
    long xiuh_month_index;
    long xiuh_day;
    int civilYear;
    int year_number;
    int year_bearer_index;
    datetime_t *year_start;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    jdn = datetime_jdn(dttm);
    aztec_number = ((jdn + 3L) % 13L) + 1L;
    aztec_name_index = (jdn + 13L) % 20L;

    civilYear = datetime_year(dttm);
    year_start = datetime_init_aztec_year_start(datetime_alloc(), civilYear);
    if (!year_start) {
        datetime_dealloc(year_start);
        return NULL;
    }
    if (datetime_compare(dttm, year_start) < 0) {
        civilYear--;
        datetime_init_aztec_year_start(year_start, civilYear);
    }

    day_index = datetime_jdn(dttm) - datetime_jdn(year_start);
    if (day_index < 0)
        day_index = 0;
    if (day_index < 360L) {
        xiuh_month_index = day_index / 20L;
        xiuh_day = (day_index % 20L) + 1L;
    } else {
        xiuh_month_index = 18L;
        xiuh_day = (day_index - 360L) + 1L;
    }

    year_number = ((civilYear - 2013) % 13 + 13) % 13 + 1;
    year_bearer_index = ((civilYear - 2013) % 4 + 4) % 4;

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "Tonalpohualli %ld %s; Xiuhpohualli day %ld of %s; year %d %s",
                             aztec_number,
                             tonalpohualli_names[aztec_name_index],
                             xiuh_day,
                             xiuhpohualli_names[xiuh_month_index],
                             year_number,
                             year_bearer_names[year_bearer_index]) < 0) {
        string_free(out);
        out = NULL;
    }

    datetime_dealloc(year_start);
    return out;
}

static long datetime_ethiopian_ymd_to_jdn(int year, int month, int day)
{
    return 1724221L + 365L * (long)(year - 1) + (long)((year - 1) / 4) + 30L * (long)(month - 1) + (long)day - 1L;
}

static void datetime_jdn_to_ethiopian_ymd(long jdn, int *year, int *month, int *day)
{
    int estimate;
    long year_start;

    estimate = (int)((jdn - 1724221L) / 366L) + 1;
    if (estimate < 1)
        estimate = 1;

    while (datetime_ethiopian_ymd_to_jdn(estimate + 1, 1, 1) <= jdn)
        estimate++;
    while (datetime_ethiopian_ymd_to_jdn(estimate, 1, 1) > jdn)
        estimate--;

    year_start = datetime_ethiopian_ymd_to_jdn(estimate, 1, 1);
    *year = estimate;
    *month = (int)((jdn - year_start) / 30L) + 1;
    *day = (int)(jdn - datetime_ethiopian_ymd_to_jdn(*year, *month, 1) + 1L);
}

string_t *datetime_ethiopian_calendar_date_text(const datetime_t *dttm)
{
    static const char *month_names[] = {
        NULL,
        "Meskerem", "Tikimt", "Hidar", "Tahsas", "Tir",
        "Yekatit", "Megabit", "Miyazya", "Genbot", "Sene",
        "Hamle", "Nehasse", "Pagume"
    };
    int year;
    int month;
    int day;
    string_t *out;

    if (!dttm || datetime_jdn(dttm) == LONG_MAX)
        return NULL;

    datetime_jdn_to_ethiopian_ymd(datetime_jdn(dttm), &year, &month, &day);
    if (month < 1 || month > 13)
        return NULL;

    out = string_new();
    if (!out ||
        string_append_format(out,
                             "%d %s %d EC",
                             day,
                             month_names[month],
                             year) < 0) {
        string_free(out);
        return NULL;
    }
    return out;
}

static long datetime_india_new_moon_jdn_in_window(int year,
                                                  month_t startMonth,
                                                  uint8_t startDay,
                                                  month_t endMonth,
                                                  uint8_t endDay)
{
    long windowStart;
    long windowEnd;
    int lunationIndex;

    if (year < 1700 || year > 2400)
        return LONG_MAX;

    windowStart = datetime_ymd_to_jdn((short)year, startMonth, startDay);
    windowEnd = datetime_ymd_to_jdn((short)year, endMonth, endDay);
    lunationIndex = (int)((windowStart - 2451550.09765) / 29.530588853) - 2;

    for (int i = 0; i < 8; i++, lunationIndex++) {
        double newMoonTT = datetime_true_new_moon_tt(lunationIndex);
        double newMoonUTC = newMoonTT - datetime_delta_t_estimate(year) / 86400.0;
        long indiaJdn = (long)floor(newMoonUTC + 5.5 / 24.0 + 0.5);

        if (indiaJdn >= windowStart && indiaJdn <= windowEnd)
            return indiaJdn;
    }

    return LONG_MAX;
}

static long datetime_local_new_moon_jdn_in_window(int year,
                                                  month_t startMonth,
                                                  uint8_t startDay,
                                                  month_t endMonth,
                                                  uint8_t endDay,
                                                  double gmtOffsetHours)
{
    long windowStart;
    long windowEnd;
    int lunationIndex;

    if (year < 1700 || year > 2400)
        return LONG_MAX;

    windowStart = datetime_ymd_to_jdn((short)year, startMonth, startDay);
    windowEnd = datetime_ymd_to_jdn((short)year, endMonth, endDay);
    lunationIndex = (int)((windowStart - 2451550.09765) / 29.530588853) - 2;

    for (int i = 0; i < 8; i++, lunationIndex++) {
        long localJdn = datetime_local_new_moon_jdn_for_lunation(lunationIndex,
                                                                 year,
                                                                 gmtOffsetHours);
        if (localJdn >= windowStart && localJdn <= windowEnd)
            return localJdn;
    }

    return LONG_MAX;
}

static long datetime_india_full_moon_jdn_between(int year,
                                                 long windowStart,
                                                 long windowEnd,
                                                 double *dayFraction)
{
    int lunationIndex;

    if (year < 1700 || year > 2400 || windowStart > windowEnd)
        return LONG_MAX;

    lunationIndex = (int)((windowStart - 2451550.09765) / 29.530588853) - 2;

    for (int i = 0; i < 8; i++, lunationIndex++) {
        double fullMoonTT = datetime_true_full_moon_tt(lunationIndex);
        double fullMoonUTC = fullMoonTT - datetime_delta_t_estimate(year) / 86400.0;
        double fullMoonIndia = fullMoonUTC + 5.5 / 24.0;
        double civilDay = floor(fullMoonIndia + 0.5);
        long indiaJdn = (long)civilDay;

        if (indiaJdn >= windowStart && indiaJdn <= windowEnd) {
            if (dayFraction)
                *dayFraction = fullMoonIndia + 0.5 - civilDay;
            return indiaJdn;
        }
    }

    return LONG_MAX;
}

static long datetime_last_india_full_moon_jdn_between(int year,
                                                      long windowStart,
                                                      long windowEnd)
{
    long found = LONG_MAX;
    int lunationIndex;

    if (year < 1700 || year > 2400 || windowStart > windowEnd)
        return LONG_MAX;

    lunationIndex = (int)((windowStart - 2451550.09765) / 29.530588853) - 2;

    for (int i = 0; i < 8; i++, lunationIndex++) {
        double fullMoonTT = datetime_true_full_moon_tt(lunationIndex);
        double fullMoonUTC = fullMoonTT - datetime_delta_t_estimate(year) / 86400.0;
        double fullMoonIndia = fullMoonUTC + 5.5 / 24.0;
        long indiaJdn = (long)floor(fullMoonIndia + 0.5);

        if (indiaJdn >= windowStart && indiaJdn <= windowEnd)
            found = indiaJdn;
    }

    return found;
}

datetime_t *datetime_init_diwali(datetime_t *dttm, int year)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_india_new_moon_jdn_in_window(year, DT_October, 15, DT_November, 20);
    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

datetime_t *datetime_init_holi(datetime_t *dttm, int year)
{
    double dayFraction = 0.0;
    long newYearJdn;
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    newYearJdn = datetime_india_new_moon_jdn_in_window(year, DT_March, 15, DT_April, 15);
    indiaJdn = datetime_india_full_moon_jdn_between(year, newYearJdn - 25L, newYearJdn - 8L, &dayFraction);
    if (indiaJdn != LONG_MAX && dayFraction >= 16.0 / 24.0)
        indiaJdn++;

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

datetime_t *datetime_init_hindu_new_year(datetime_t *dttm, int year)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_india_new_moon_jdn_in_window(year, DT_March, 15, DT_April, 15);
    if (indiaJdn == LONG_MAX)
        return NULL;

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

static datetime_t *datetime_init_india_full_moon_observance(datetime_t *dttm,
                                                            int year,
                                                            month_t month)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_india_full_moon_jdn_between(
        year,
        datetime_ymd_to_jdn((short)year, month, 1),
        datetime_ymd_to_jdn((short)year, month, (uint8_t)datetime_days_in_month((short)year, month)),
        NULL
    );

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

datetime_t *datetime_init_buddhist_new_year(datetime_t *dttm, int year)
{
    return datetime_init_india_full_moon_observance(dttm, year, DT_April);
}

datetime_t *datetime_init_vesak(datetime_t *dttm, int year)
{
    long indiaJdn;

    if (!dttm || year < 1700 || year > 2400)
        return NULL;

    indiaJdn = datetime_last_india_full_moon_jdn_between(
        year,
        datetime_ymd_to_jdn((short)year, DT_May, 1),
        datetime_ymd_to_jdn((short)year, DT_May, 31)
    );

    return datetime_init_materialised_jdn(dttm, indiaJdn);
}

datetime_t *datetime_init_asalha_puja(datetime_t *dttm, int year)
{
    return datetime_init_india_full_moon_observance(dttm, year, DT_July);
}

double datetime_tz_offset(const datetime_t *dttm) {
    // Get the local time and GMT time for the given datetime
    time_t t;
    struct tm local_tm, gmt_tm;

    if (dttm == NULL) return DBL_MAX;
    if (dttm->year == SHRT_MAX) {
        if (dttm->JulianDayNumber == LONG_MAX && dttm->JulianDay == DBL_MAX) {
            return DBL_MAX; // Cannot calculate timezone offset if date is not initialised
        }
        datetime_year(dttm);
    }

    // Convert datetime to struct tm
    local_tm.tm_year = dttm->year - 1900;
    local_tm.tm_mon = dttm->month - 1;
    local_tm.tm_mday = dttm->day;
    local_tm.tm_hour = dttm->hour;
    local_tm.tm_min = dttm->minute;
    local_tm.tm_sec = (int)dttm->second;
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

datetime_t *datetime_to_gmt(datetime_t *dttm)
{
    if (dttm == NULL) return NULL;

    time_t t;
    struct tm tm;

    // Convert datetime to struct tm
    tm.tm_year = dttm->year - 1900;
    tm.tm_mon = dttm->month - 1;
    tm.tm_mday = dttm->day;
    tm.tm_hour = dttm->hour;
    tm.tm_min = dttm->minute;
    tm.tm_sec = (int)dttm->second;
    tm.tm_isdst = -1; // Let mktime determine if DST is in effect

    // Convert struct tm to time_t (local time)
    t = mktime(&tm);
    if (t == -1) return NULL;

    // Convert time_t to struct tm in GMT
    struct tm gmt_tm;
    if (gmtime_r(&t, &gmt_tm) == NULL) return NULL;

    // Update the input datetime with GMT values
    dttm->year = (short)(gmt_tm.tm_year + 1900);
    dttm->month = (month_t)(gmt_tm.tm_mon + 1);
    dttm->day = (uint8_t)gmt_tm.tm_mday;
    dttm->hour = (uint8_t)gmt_tm.tm_hour;
    dttm->minute = (uint8_t)gmt_tm.tm_min;
    dttm->second = (double)gmt_tm.tm_sec;

    if (dttm->JulianDayNumber != LONG_MAX) {
        dttm->JulianDayNumber = datetime_jdn(dttm);
    }
    if (dttm->JulianDay != DBL_MAX) {
        dttm->JulianDay = datetime_jd(dttm);
    }

    return dttm;
}

/**
 * @brief Divide two long integers with truncation toward zero.
 * @param numerator The numerator.
 * @param denominator The denominator.
 * @return The truncated result of numerator / denominator.
 */
static inline long ldivide(long numerator, long denominator)
{
    return (numerator >= 0L) ? (numerator / denominator) : ((numerator - denominator + 1L) / denominator);
}

/**
 * @brief Converts a Julian Day Number into a Gregorian/Julian calendar date. Applies the Gregorian reform for JD >= 2299161
 *        and adjusts for the absence of a year zero (1 BC = year -1).
 * @param julianDay The Julian Day Number to convert.
 * @param monthOut Output pointer for month (1–12).
 * @param dayOut Output pointer for day (1–31).
 * @param yearOut Output pointer for year (no year 0).
 */
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

short datetime_year(const datetime_t *dttm) {
    if (dttm == NULL) return SHRT_MAX;
    if (dttm->year != SHRT_MAX) return dttm->year;
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
    return SHRT_MAX; // This is an uninitialised state that cannot be calculated, so we return SHRT_MAX as a sentinel value.
}

month_t datetime_month(const datetime_t *dttm) {
    if (dttm->year != SHRT_MAX) return dttm->month;
    // If we cannot calculate the year, we cannot calculate the month, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX) return 0;
    return dttm->month;
}

uint8_t datetime_day(const datetime_t *dttm) {
    if (dttm->year != SHRT_MAX) return dttm->day;
    // If we cannot calculate the year, we cannot calculate the day, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX) return 0;
    return dttm->day;
}

uint8_t datetime_hour(const datetime_t *dttm) {
    if (dttm->year != SHRT_MAX) return dttm->hour;
    // If we cannot calculate the year, we cannot calculate the hour, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX) return 0;
    return dttm->hour;
}

uint8_t datetime_minute(const datetime_t *dttm) {
    if (dttm->year != SHRT_MAX) return dttm->minute;
    // If we cannot calculate the year, we cannot calculate the minute, so we return 0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX) return 0;
    return dttm->minute;
}

double datetime_second(const datetime_t *dttm) {
    if (dttm->year != SHRT_MAX) return dttm->second;
    // If we cannot calculate the year, we cannot calculate the second, so we return 0.0 as a sentinel value.
    if (datetime_year(dttm) == SHRT_MAX) return 0.0;
    return dttm->second;
}

long datetime_ymd_to_jdn(short year, month_t month, uint8_t day) {
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

long datetime_jdn(const datetime_t *dttm) {
    if (dttm->JulianDayNumber != LONG_MAX) return dttm->JulianDayNumber;

    if (dttm->year == SHRT_MAX) {
        // If year is not initialised, we cannot calculate the Julian Day Number, so we return LONG_MAX as a sentinel value.
        return LONG_MAX;
    }

    datetime_t *mutable_dttm = (datetime_t *)dttm; // Cast away const to store the calculated Julian Day Number
    mutable_dttm->JulianDayNumber = datetime_ymd_to_jdn(dttm->year, dttm->month, dttm->day);
    return dttm->JulianDayNumber;
}

double datetime_jd(const datetime_t *dttm) {
    if (dttm->JulianDay != DBL_MAX) return dttm->JulianDay;

    long jdn = datetime_jdn(dttm);
    if (jdn == LONG_MAX) {
        // If we cannot calculate the Julian Day Number, we cannot calculate the Julian Day, so we return DBL_MAX as a sentinel value.
        return DBL_MAX;
    }

    ((datetime_t *)dttm)->JulianDay = jdn + (dttm->hour - 12) / 24.0 + dttm->minute / 1440.0 + dttm->second / 86400.0;

    return dttm->JulianDay;
}

double datetime_delta_t_seconds(int year)
{
    return datetime_delta_t_estimate(year);
}

double datetime_jd_tt(const datetime_t *dttm)
{
    double jd;
    int year;

    if (!dttm)
        return DBL_MAX;
    jd = datetime_jd(dttm);
    if (jd == DBL_MAX)
        return DBL_MAX;
    year = datetime_year(dttm);
    if (year == SHRT_MAX)
        return DBL_MAX;
    return jd + datetime_delta_t_seconds(year) / 86400.0;
}

double datetime_jd_tdb(const datetime_t *dttm)
{
    double jd_tt;
    double g_degrees;
    double g_radians;
    double correction_seconds;

    jd_tt = datetime_jd_tt(dttm);
    if (jd_tt == DBL_MAX)
        return DBL_MAX;

    g_degrees = 357.53 + 0.9856003 * (jd_tt - 2451545.0);
    g_radians = g_degrees * (M_PI / 180.0);
    correction_seconds = 0.001657 * sin(g_radians) + 0.000022 * sin(2.0 * g_radians);
    return jd_tt + correction_seconds / 86400.0;
}

weekday_t datetime_weekday(const datetime_t *dttm)
{
    long jdn = datetime_jdn(dttm);
    if (jdn == LONG_MAX) {
        // If we cannot calculate the Julian Day Number, we cannot calculate the weekday, so we return 0 as a sentinel value.
        return 0;
    }
    return (weekday_t)((jdn + 1) % 7 + 1);
}

const char *datetime_weekday_name(weekday_t weekday)
{
    static const char *names[] = {
        "Unknown",
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday"
    };

    if (weekday < DT_Sunday || weekday > DT_Saturday)
        return names[0];
    return names[weekday];
}

bool datetime_equal(const datetime_t *dttm1, const datetime_t *dttm2)
{
    if (dttm1->year == SHRT_MAX) datetime_year(dttm1); // Try to calculate the year, month, day, ... if it is not initialised
    if (dttm2->year == SHRT_MAX) datetime_year(dttm2); // Try to calculate the year, month, day, ... if it is not initialised

    return dttm1->year == dttm2->year &&
           dttm1->month == dttm2->month &&
           dttm1->day == dttm2->day &&
           dttm1->hour == dttm2->hour &&
           dttm1->minute == dttm2->minute &&
           fabs(dttm1->second - dttm2->second) < 1e-9;
}

bool datetime_lt(const datetime_t *dttm1, const datetime_t *dttm2)
{
    if (dttm1->year == SHRT_MAX) datetime_year(dttm1); // Try to calculate the year, month, day, ... if it is not initialised
    if (dttm2->year == SHRT_MAX) datetime_year(dttm2); // Try to calculate the year, month, day, ... if it is not initialised

    if (dttm1->year != dttm2->year) return dttm1->year < dttm2->year;
    if (dttm1->month != dttm2->month) return dttm1->month < dttm2->month;
    if (dttm1->day != dttm2->day) return dttm1->day < dttm2->day;
    if (dttm1->hour != dttm2->hour) return dttm1->hour < dttm2->hour;
    if (dttm1->minute != dttm2->minute) return dttm1->minute < dttm2->minute;
    return dttm1->second < dttm2->second;
}

bool datetime_le(const datetime_t *dttm1, const datetime_t *dttm2)
{
    return datetime_lt(dttm1, dttm2) || datetime_equal(dttm1, dttm2);
}

bool datetime_gt(const datetime_t *dttm1, const datetime_t *dttm2)
{
    return !datetime_le(dttm1, dttm2);
}

bool datetime_ge(const datetime_t *dttm1, const datetime_t *dttm2)
{
    return !datetime_lt(dttm1, dttm2);
}

int datetime_compare(const datetime_t *dttm1, const datetime_t *dttm2)
{
    if (datetime_lt(dttm1, dttm2)) return -1;
    if (datetime_gt(dttm1, dttm2)) return 1;
    return 0; // They are equal
}

inline bool datetime_is_leap_year(short year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

unsigned short datetime_days_in_month(short year, month_t month)
{
    static unsigned short daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month < DT_January || month > DT_December) return 0;
    if (month == DT_February) return datetime_is_leap_year(year) ? 29 : 28;
    return daysInMonth[ month ];
}

bool datetime_valid_ymd(short year, month_t month, uint8_t day)
{
    unsigned short days;

    if (year < 1 || year > 9999)
        return false;
    days = datetime_days_in_month(year, month);
    return days != 0u && day >= 1u && day <= days;
}

unsigned short datetime_days_in_this_month(const datetime_t *dttm)
{
    short year = datetime_year(dttm);
    if (year == SHRT_MAX) return 0; // If we cannot calculate the year, we cannot calculate the number of days in the month.

    month_t month = datetime_month(dttm);
    return datetime_days_in_month(year, month);
}

bool datetime_is_dst(const datetime_t *dttm)
{
    if (dttm->year == SHRT_MAX) datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised

    struct tm tmdate;
    tmdate.tm_year = dttm->year - 1900;
    tmdate.tm_mon = dttm->month - 1;
    tmdate.tm_mday = dttm->day;
    tmdate.tm_hour = dttm->hour;
    tmdate.tm_min = dttm->minute;
    tmdate.tm_sec = (int)dttm->second;
    tmdate.tm_isdst = -1; // Let mktime determine if DST is in effect

    time_t t = mktime(&tmdate);
    if (t == -1) return false; // Could not determine DST status

    return tmdate.tm_isdst > 0;
}

/* Invalidate Julian caches and recalculate from the current y/m/d/h/m/s fields. */
static void datetime_refresh_julian_caches(datetime_t *dttm)
{
    if (dttm->JulianDay != DBL_MAX || dttm->JulianDayNumber != LONG_MAX) {
        dttm->JulianDay = DBL_MAX;
        dttm->JulianDayNumber = LONG_MAX;
        datetime_jd(dttm);
    }
}

datetime_t *datetime_add_days(datetime_t *dttm, long days)
{
    if (days == 0) return dttm; // No change needed
    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add days to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;

        if (dttm->JulianDay != DBL_MAX) dttm->JulianDay += (double)days;
        if (dttm->JulianDayNumber != LONG_MAX) dttm->JulianDayNumber += days;
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

datetime_t *datetime_add_weeks(datetime_t *dttm, int weeks)
{
    return datetime_add_days(dttm, (long)weeks * 7L);
}

datetime_t *datetime_add_months(datetime_t *dttm, int months)
{
    if (months == 0) return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;
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
            if (dttm->month == DT_April || dttm->month == DT_June || dttm->month == DT_September || dttm->month == DT_November) {
                if (dttm->day > 30) {
                    dttm->day = 30;
                }
            }
        }
    }

    datetime_refresh_julian_caches(dttm);

    return dttm;
}

datetime_t *datetime_add_years(datetime_t *dttm, int years)
{
    if (years == 0) return dttm;

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;
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

datetime_t *datetime_add_hours(datetime_t *dttm, int hours)
{
    if (hours == 0) return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;
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

datetime_t *datetime_add_minutes(datetime_t *dttm, int minutes)
{
    if (minutes == 0) return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;
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

datetime_t *datetime_add_seconds(datetime_t *dttm, double seconds)
{
    if (fabs(seconds) < 1e-9) return dttm; // No change needed

    if (dttm->year == SHRT_MAX) {
        // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
        if (dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;
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

datetime_t *datetime_add_span(datetime_t *dttm, const datetime_span_t *span)
{
    if (span == NULL) return dttm;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
    if (dttm->year == SHRT_MAX && dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;

    if (span->years != 0) dttm = datetime_add_years(dttm, (int)span->years);
    if (span->months != 0) dttm = datetime_add_months(dttm, (int)span->months);
    if (span->days != 0) dttm = datetime_add_days(dttm, (long)span->days);
    if (span->hours != 0) dttm = datetime_add_hours(dttm, (int)span->hours);
    if (span->minutes != 0) dttm = datetime_add_minutes(dttm, (int)span->minutes);
    if (span->seconds != 0) dttm = datetime_add_seconds(dttm, (double)span->seconds);
    return dttm;
}

datetime_t *datetime_sub_span(datetime_t *dttm, const datetime_span_t *span)
{
    if (span == NULL) return dttm;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot add seconds to it.
    if (dttm->year == SHRT_MAX && dttm->JulianDay == DBL_MAX && dttm->JulianDayNumber == LONG_MAX) return NULL;

    if (span->years != 0) dttm = datetime_add_years(dttm, -(int)span->years);
    if (span->months != 0) dttm = datetime_add_months(dttm, -(int)span->months);
    if (span->days != 0) dttm = datetime_add_days(dttm, -(long)span->days);
    if (span->hours != 0) dttm = datetime_add_hours(dttm, -(int)span->hours);
    if (span->minutes != 0) dttm = datetime_add_minutes(dttm, -(int)span->minutes);
    if (span->seconds != 0) dttm = datetime_add_seconds(dttm, -(double)span->seconds);
    return dttm;
}

unsigned int datetime_hash(const datetime_t *dttm)
{
    if (dttm->year == SHRT_MAX) datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised

    unsigned int ms = (unsigned int)(dttm->hour * 3600000u +
                    dttm->minute * 60000u + (unsigned int)(dttm->second * 1000.0));

    unsigned int dateKey = (((unsigned int)dttm->year  << 16) |
                           ((unsigned int)dttm->month << 8) |
                           (unsigned int)dttm->day) * 0x9E3779B1u; // mix year, month, day

    unsigned int hash = dateKey ^ (ms * 0x9E3779B1u);  // mix date + time

    // MurmurHash3 finalizer (32‑bit)
    hash ^= hash >> 16;
    hash *= 0x85EBCA6Bu;
    hash ^= hash >> 13;
    hash *= 0xC2B2AE35u;
    hash ^= hash >> 16;

    return hash;
}

double datetime_duration(const datetime_t *dttm1, const datetime_t *dttm2, datetime_span_t *span)
{
    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot calculate the difference.
    if (dttm1->year == SHRT_MAX && dttm1->JulianDay == DBL_MAX && dttm1->JulianDayNumber == LONG_MAX) return DBL_MAX;

    // If we get here, it means the datetime is in an uninitialised state that cannot be calculated, so we cannot calculate the difference.
    if (dttm2->year == SHRT_MAX && dttm2->JulianDay == DBL_MAX && dttm2->JulianDayNumber == LONG_MAX) return DBL_MAX;

    double jd1 = datetime_jd(dttm1);
    double jd2 = datetime_jd(dttm2);

    if (span != NULL) {
        datetime_year(dttm1); // Try to calculate the year, month, day, ... if it is not initialised
        datetime_year(dttm2); // Try to calculate the year, month, day, ... if it is not initialised

        if (jd1 < jd2) {
            // If dttm1 is earlier than dttm2, we swap them to calculate the span as a positive duration, and we will negate the final
            // difference at the end.
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
            unsigned short daysInPrevMonth = datetime_days_in_month(year, month == DT_January ? DT_December : month - 1);
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

static char datetime_ascii_upper(char ch)
{
   return (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
}

static int datetime_cursor_peek_ascii(const string_cursor_t *cursor, char *out)
{
   unsigned char ascii = 0u;

   if (!cursor || string_cursor_done(cursor) ||
       !string_cursor_peek_ascii(cursor, &ascii))
      return 0;
   if (out)
      *out = (char)ascii;
   return 1;
}

static int datetime_cursor_peek_matches(const string_cursor_t *cursor,
                                        char lower,
                                        char upper)
{
   char ch = '\0';

   return datetime_cursor_peek_ascii(cursor, &ch) &&
          (ch == lower || ch == upper);
}

static int datetime_cursor_advance(string_cursor_t *cursor)
{
   return string_cursor_next(cursor) == 0;
}

static size_t datetime_cursor_count_run(const string_cursor_t *cursor,
                                        char lower,
                                        char upper)
{
   string_cursor_t *scan = string_cursor_clone(cursor);
   size_t count = 0u;

   if (!scan)
      return 0u;

   while (datetime_cursor_peek_matches(scan, lower, upper)) {
      count++;
      if (!datetime_cursor_advance(scan))
         break;
   }

   string_cursor_free(scan);
   return count;
}

static int datetime_cursor_second_is(const string_cursor_t *cursor, char expected)
{
   string_cursor_t *scan = string_cursor_clone(cursor);
   char ch = '\0';
   int matches = 0;

   if (!scan)
      return 0;
   if (datetime_cursor_advance(scan) &&
       datetime_cursor_peek_ascii(scan, &ch) &&
       ch == expected)
      matches = 1;

   string_cursor_free(scan);
   return matches;
}

static void datetime_cursor_skip_run(string_cursor_t *cursor, size_t count)
{
   while (count-- > 0u && !string_cursor_done(cursor))
      (void)datetime_cursor_advance(cursor);
}

static char *datetime_format_export(const string_t *text)
{
   const char *raw = string_c_str(text);
   size_t len = string_view_length(string_view_all(text)) + 1u;
   char *out = malloc(len);

   if (!out)
      return NULL;
   memcpy(out, raw, len);
   return out;
}

static void datetime_format_append_text(string_t *out,
                                        int *failed,
                                        const char *text)
{
   if (*failed)
      return;
   if (string_append_cstr(out, text) != 0)
      *failed = 1;
}

static void datetime_format_append_name(string_t *out,
                                        int *failed,
                                        const char *name,
                                        size_t max_chars,
                                        int title_case,
                                        int upper_case)
{
   string_t *text;
   string_cursor_t *cursor;
   size_t written = 0u;

   if (*failed)
      return;

   text = string_new_with(name);
   cursor = text ? string_cursor_new(text) : NULL;
   if (!text || !cursor) {
      *failed = 1;
      goto done;
   }

   while (!string_cursor_done(cursor) && written < max_chars) {
      char ascii = '\0';
      rune_t rune = string_cursor_peek(cursor);

      if (rune_to_ascii(rune, &ascii) &&
          (upper_case || (title_case && written == 0u))) {
         if (string_append_char(out, datetime_ascii_upper(ascii)) != 0) {
            *failed = 1;
            goto done;
         }
      } else if (string_append_rune(out, rune) != 0) {
         *failed = 1;
         goto done;
      }

      written++;
      if (!datetime_cursor_advance(cursor)) {
         *failed = 1;
         goto done;
      }
   }

done:
   string_cursor_free(cursor);
   string_free(text);
}

static void datetime_format_append_char(string_t *out, int *failed, char ch)
{
   if (*failed)
      return;
   if (string_append_char(out, ch) != 0)
      *failed = 1;
}

static void datetime_format_append_format(string_t *out,
                                          int *failed,
                                          const char *format,
                                          int value)
{
   if (*failed)
      return;
   if (string_append_format(out, format, value) < 0)
      *failed = 1;
}

string_t *datetime_format_text(const datetime_t *dttm,
                               const string_t *format)
{
    static const char* weekdayNames[] = { NULL,
        "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday" };
    static const char* monthNames[] = { NULL,
        "january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december" };
   string_t *formattedString;
   string_cursor_t *cursor;
   int append_failed = 0;

   if (!dttm || !format)
      return NULL;
   if (dttm->year == SHRT_MAX && datetime_year(dttm) == SHRT_MAX)
      return NULL;

   formattedString = string_new();
   cursor = formattedString ? string_cursor_new(format) : NULL;

   if (!formattedString || !cursor) {
      string_free(formattedString);
      return NULL;
   }

   while (!string_cursor_done(cursor)) {
      char marker = '\0';

      if (!datetime_cursor_peek_ascii(cursor, &marker)) {
         if (string_append_rune(formattedString, string_cursor_peek(cursor)) != 0)
            append_failed = 1;
         (void)datetime_cursor_advance(cursor);
         continue;
      }

      if (marker == '%') {
         (void)datetime_cursor_advance(cursor);
         if (string_cursor_done(cursor)) {
            datetime_format_append_char(formattedString, &append_failed, '%');
            break;
         }

         char token = '\0';
         (void)datetime_cursor_peek_ascii(cursor, &token);
         switch (token) {
            case '%':
               datetime_format_append_char(formattedString, &append_failed, '%');
               (void)datetime_cursor_advance(cursor);
               break;

            case 'd':
            case 'D':
            {
               size_t run = datetime_cursor_count_run(cursor, 'd', 'D');
               int all_caps = token == 'D' && datetime_cursor_second_is(cursor, 'D');
               switch (run) {
                  case 1:
                     datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->day);
                     datetime_cursor_skip_run(cursor, 1u);
                     break;
                  case 2:
                     datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->day);
                     datetime_cursor_skip_run(cursor, 2u);
                     break;
                  case 3:
                     datetime_format_append_name(formattedString,
                                                 &append_failed,
                                                 weekdayNames[datetime_weekday(dttm)],
                                                 3u,
                                                 token == 'D' && !all_caps,
                                                 all_caps);
                     datetime_cursor_skip_run(cursor, 3u);
                     break;
                  case 4:
                  default:
                     datetime_format_append_name(formattedString,
                                                 &append_failed,
                                                 weekdayNames[datetime_weekday(dttm)],
                                                 SIZE_MAX,
                                                 token == 'D' && !all_caps,
                                                 all_caps);
                     datetime_cursor_skip_run(cursor, 4u);
                     break;
               }
               break;
            }

            case 'o':
            case 'O':
            {
               const char *suffix = "th";

               if (!(10 < dttm->day && dttm->day < 20)) {
                  switch (dttm->day % 10) {
                     case 1: suffix = "st"; break;
                     case 2: suffix = "nd"; break;
                     case 3: suffix = "rd"; break;
                     default: suffix = "th"; break;
                  }
               }
               datetime_format_append_name(formattedString,
                                           &append_failed,
                                           suffix,
                                           SIZE_MAX,
                                           0,
                                           token == 'O');
               (void)datetime_cursor_advance(cursor);
               break;
            }

            case 'q':
            case 'Q':
            {
               const char *suffix = "ᵗʰ";

               if (!(10 < dttm->day && dttm->day < 20)) {
                  switch (dttm->day % 10) {
                     case 1: suffix = "ˢᵗ"; break;
                     case 2: suffix = "ⁿᵈ"; break;
                     case 3: suffix = "ʳᵈ"; break;
                     default: suffix = "ᵗʰ"; break;
                  }
               }
               datetime_format_append_name(formattedString,
                                           &append_failed,
                                           suffix,
                                           SIZE_MAX,
                                           0,
                                           0);
               (void)datetime_cursor_advance(cursor);
               break;
            }

            case 'm':
            case 'M':
            {
               size_t run = datetime_cursor_count_run(cursor, 'm', 'M');
               int all_caps = token == 'M' && datetime_cursor_second_is(cursor, 'M');
               switch (run) {
                  case 1:
                     datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->month);
                     datetime_cursor_skip_run(cursor, 1u);
                     break;
                  case 2:
                     datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->month);
                     datetime_cursor_skip_run(cursor, 2u);
                     break;
                  case 3:
                     datetime_format_append_name(formattedString,
                                                 &append_failed,
                                                 monthNames[dttm->month],
                                                 3u,
                                                 token == 'M' && !all_caps,
                                                 all_caps);
                     datetime_cursor_skip_run(cursor, 3u);
                     break;
                  case 4:
                  default:
                     datetime_format_append_name(formattedString,
                                                 &append_failed,
                                                 monthNames[dttm->month],
                                                 SIZE_MAX,
                                                 token == 'M' && !all_caps,
                                                 all_caps);
                     datetime_cursor_skip_run(cursor, 4u);
                     break;
               }
               break;
            }

            case 'y':
            case 'Y':
            {
               size_t run = datetime_cursor_count_run(cursor, 'y', 'Y');
               if (run < 4u) {
                  int year = dttm->year - 100 * (dttm->year / 100);
                  datetime_format_append_format(formattedString, &append_failed, "%02i", year);
                  datetime_cursor_skip_run(cursor, run);
               }
               else {
                  datetime_format_append_format(formattedString, &append_failed, "%i", dttm->year);
                  datetime_cursor_skip_run(cursor, 4u);
               }
               break;
            }

            default:
               datetime_format_append_char(formattedString, &append_failed, '%');
               break;
         }
      }
      else if (marker == '@') {
         (void)datetime_cursor_advance(cursor);
         if (string_cursor_done(cursor)) {
            datetime_format_append_char(formattedString, &append_failed, '%');
            break;
         }

         char token = '\0';
         (void)datetime_cursor_peek_ascii(cursor, &token);
         switch (token) {
            case 'h':
            case 'H':
            {
               int hour = dttm->hour;
               if (token == 'h' && dttm->hour > 12) hour -= 12;
               (void)datetime_cursor_advance(cursor);
               if (datetime_cursor_peek_matches(cursor, 'h', 'H')) {
                  datetime_format_append_format(formattedString, &append_failed, "%02i", hour);
                  (void)datetime_cursor_advance(cursor);
               }
               else {
                  datetime_format_append_format(formattedString, &append_failed, "%i", hour);
               }
               break;
            }

            case 'M':
            case 'm':
               (void)datetime_cursor_advance(cursor);
               if (datetime_cursor_peek_matches(cursor, 'm', 'M')) {
                  datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->minute);
                  (void)datetime_cursor_advance(cursor);
               }
               else {
                  datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->minute);
               }
               break;

            case 'S':
            case 's':
               (void)datetime_cursor_advance(cursor);
               if (datetime_cursor_peek_matches(cursor, 's', 'S')) {
                  datetime_format_append_format(formattedString, &append_failed, "%02i", (int)dttm->second);
                  (void)datetime_cursor_advance(cursor);
               }
               else {
                  datetime_format_append_format(formattedString, &append_failed, "%i", (int)dttm->second);
               }
               break;

            case 'P':
               if ( dttm->hour >= 12 )
                  datetime_format_append_text(formattedString, &append_failed, "PM");
               else
                  datetime_format_append_text(formattedString, &append_failed, "AM");
               (void)datetime_cursor_advance(cursor);
               break;

            case 'p':
               if ( dttm->hour >= 12 )
                  datetime_format_append_text(formattedString, &append_failed, "pm");
               else
                  datetime_format_append_text(formattedString, &append_failed, "am");
               (void)datetime_cursor_advance(cursor);
               break;

            case '@':
               datetime_format_append_char(formattedString, &append_failed, '@');
               (void)datetime_cursor_advance(cursor);
               break;

            default:
               datetime_format_append_char(formattedString, &append_failed, '@');
               break;
         }
      }
      else {
         if (marker == '^')
            (void)datetime_cursor_advance(cursor);
         else {
            datetime_format_append_char(formattedString, &append_failed, marker);
            (void)datetime_cursor_advance(cursor);
         }
      }
   }

   if (append_failed)
      goto error;

   string_cursor_free(cursor);
   return formattedString;

error:
   string_cursor_free(cursor);
   string_free(formattedString);
   return NULL;

}

char *datetime_format(const datetime_t *dttm, const char *format)
{
   string_t *format_text;
   string_t *formatted_text;
   char *result;

   if (format == NULL) return NULL;

   format_text = string_new_with(format);
   formatted_text = format_text ? datetime_format_text(dttm, format_text) : NULL;
   string_free(format_text);
   if (!formatted_text)
      return NULL;

   result = datetime_format_export(formatted_text);
   string_free(formatted_text);
   return result;
}

double datetime_sun_time(long julianDayNumber, double latitude, double longitude, bool isSunrise)
{
    const double degToRad = M_PI / 180.0;
    const double radToDeg = 180.0 / M_PI;
    const double zenith = 90.833 * degToRad;
    datetime_t *date = NULL;
    int dayOfYear;
    double gamma;
    double equationOfTime;
    double solarDeclination;
    double cosHourAngle;
    double hourAngleDegrees;
    double solarNoonMinutesUtc;
    double minutesUtc;
    double hoursUtc;

    if (!isfinite(latitude) || !isfinite(longitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0)
        return -1.0;

    date = datetime_init_jdn(datetime_alloc(), julianDayNumber);
    if (!date)
        return -1.0;

    dayOfYear = datetime_day_of_year(date);
    datetime_dealloc(date);
    if (dayOfYear <= 0)
        return -1.0;

    gamma = 2.0 * M_PI / 365.0 * ((double)dayOfYear - 1.0);

    equationOfTime = 229.18 * (
        0.000075
        + 0.001868 * cos(gamma)
        - 0.032077 * sin(gamma)
        - 0.014615 * cos(2.0 * gamma)
        - 0.040849 * sin(2.0 * gamma)
    );

    solarDeclination =
        0.006918
        - 0.399912 * cos(gamma)
        + 0.070257 * sin(gamma)
        - 0.006758 * cos(2.0 * gamma)
        + 0.000907 * sin(2.0 * gamma)
        - 0.002697 * cos(3.0 * gamma)
        + 0.001480 * sin(3.0 * gamma);

    cosHourAngle = (cos(zenith) - sin(latitude * degToRad) * sin(solarDeclination)) /
        (cos(latitude * degToRad) * cos(solarDeclination));

    if (cosHourAngle > 1.0)
        return -1.0;
    if (cosHourAngle < -1.0)
        return -2.0;

    hourAngleDegrees = acos(cosHourAngle) * radToDeg;
    solarNoonMinutesUtc = 720.0 - (4.0 * longitude) - equationOfTime;
    minutesUtc = solarNoonMinutesUtc + (isSunrise ? -4.0 * hourAngleDegrees : 4.0 * hourAngleDegrees);
    hoursUtc = minutesUtc / 60.0;

    return hoursUtc;
}

static int datetime_day_of_year(const datetime_t *dttm)
{
    short year;
    month_t month;
    uint8_t day;
    int dayOfYear = 0;

    if (!dttm)
        return 0;

    year = datetime_year(dttm);
    month = datetime_month(dttm);
    day = datetime_day(dttm);
    if (!datetime_valid_ymd(year, month, day))
        return 0;

    for (month_t m = DT_January; m < month; m++)
        dayOfYear += datetime_days_in_month(year, m);
    return dayOfYear + day;
}

double datetime_solar_declination(const datetime_t *dttm)
{
    const double radToDeg = 180.0 / M_PI;
    int dayOfYear;
    double hour;
    double gamma;
    double declination;

    if (!dttm)
        return DBL_MAX;

    dayOfYear = datetime_day_of_year(dttm);
    if (dayOfYear <= 0)
        return DBL_MAX;

    hour = (double)datetime_hour(dttm)
         + (double)datetime_minute(dttm) / 60.0
         + datetime_second(dttm) / 3600.0;
    gamma = 2.0 * M_PI / 365.0 * ((double)dayOfYear - 1.0 + (hour - 12.0) / 24.0);

    declination = 0.006918
        - 0.399912 * cos(gamma)
        + 0.070257 * sin(gamma)
        - 0.006758 * cos(2.0 * gamma)
        + 0.000907 * sin(2.0 * gamma)
        - 0.002697 * cos(3.0 * gamma)
        + 0.001480 * sin(3.0 * gamma);

    return declination * radToDeg;
}

double datetime_solar_max_altitude(const datetime_t *dttm, double latitude)
{
    double declination;

    if (!isfinite(latitude) || latitude < -90.0 || latitude > 90.0)
        return DBL_MAX;

    declination = datetime_solar_declination(dttm);
    if (declination == DBL_MAX)
        return DBL_MAX;

    return 90.0 - fabs(latitude - declination);
}

double datetime_solar_inclination(const datetime_t *dttm, double latitude)
{
    double maxAltitude;

    maxAltitude = datetime_solar_max_altitude(dttm, latitude);
    if (maxAltitude == DBL_MAX)
        return DBL_MAX;

    return 90.0 - maxAltitude;
}

/**
 * @brief set the time components of a datetime object to the sunrise or sunset time for its date and a given location.
 *        This function calculates the sunrise or sunset time for the date represented by the datetime object and a given location,
 *        and sets the time components of the datetime object accordingly. The location is specified by the latitude and longitude.
 *        The time zone offset is used to adjust the calculated time to the local time zone (inclusive of daylight saving time).
 *        If the calculated time is less than 0, it means that the sunrise/sunset occurs on the previous day, so we add 24 hours
 *        to the time and subtract one day from the datetime object. If the calculated time is greater than or equal to 24,
 *        it means that the sunrise/sunset occurs on the next day, so we subtract 24 hours from the time and add one day to the
 *        datetime object.
 * @param dttm the datetime object to set the time components of. The date components of the datetime object should already be set
 *        to the desired date.
 * @param latitude the latitude of the location to calculate the sunrise/sunset time for.
 * @param longitude the longitude of the location to calculate the sunrise/sunset time for.
 * @param timeZoneOffset the time zone offset in hours to adjust the calculated time to the local time zone (inclusive of daylight
 *        saving time). For example, if the local time zone is GMT+2, the timeZoneOffset should be 2.0. If the local time zone is
 *        GMT-5, the timeZoneOffset should be -5.0.
 * @param isSunrise a boolean value indicating whether to calculate the sunrise time (true) or the sunset time (false).
 */
static void datetime_set_sun_time(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset, bool isSunrise)
{
    long julianDayNumber = datetime_jdn(dttm);
    double time = datetime_sun_time(julianDayNumber, latitude, longitude, isSunrise);
    double minute_value;
    long rounded_minutes;
    datetime_sun_status_t status;

    datetime_year(dttm);

    status = datetime_sun_status_from_raw(time);
    if (status != DATETIME_SUN_OK) {
        dttm->hour = 0;
        dttm->minute = 0;
        dttm->second = 0.0;
        return; // Sunrise/sunset cannot be calculated for this date and location (e.g. polar night)
    }

    if (timeZoneOffset == DBL_MAX) {
        timeZoneOffset = datetime_tz_offset(dttm);
        if (timeZoneOffset == DBL_MAX) {
            timeZoneOffset = 0.0; // Fallback to GMT if time zone offset cannot be calculated
        }
    }

    time += timeZoneOffset;

    if (time < 0.0) {
        time += 24.0;
        datetime_add_days(dttm, -1);
    }
    else if (time >= 24.0) {
        time -= 24.0;
        datetime_add_days(dttm, 1);
    }

    minute_value = time * 60.0;
    rounded_minutes = lround(minute_value);
    if (rounded_minutes < 0) {
        rounded_minutes += 24L * 60L;
        datetime_add_days(dttm, -1);
    } else if (rounded_minutes >= 24L * 60L) {
        rounded_minutes -= 24L * 60L;
        datetime_add_days(dttm, 1);
    }

    // Update the time components of the datetime object
    dttm->hour = (uint8_t)(rounded_minutes / 60L);
    dttm->minute = (uint8_t)(rounded_minutes % 60L);
    dttm->second = 0.0;
}

static datetime_sun_status_t datetime_sun_status_from_raw(double raw_time)
{
    if (raw_time == -1.0)
        return DATETIME_SUN_NEVER_RISES;
    if (raw_time == -2.0)
        return DATETIME_SUN_NEVER_SETS;
    if (!isfinite(raw_time))
        return DATETIME_SUN_UNAVAILABLE;
    return DATETIME_SUN_OK;
}

/**
 * @brief initialise a datetime object with the sunrise or sunset time for a given date and location. This function calculates
 *        the sunrise or sunset time for a given date and location, and sets the time components of the datetime object accordingly.
 *        The date is specified by the Julian Day Number, and the location is specified by the latitude and longitude. The time zone
 *        offset is also taken into account to adjust the time to the local time zone (inclusive of daylight saving time). If the
 *        calculated time is less than 0, it means that the sunrise/sunset occurs on the previous day, so we add 24 hours to the
 *        time and subtract one day from the datetime object. If the calculated time is greater than or equal to 24, it means that
 *        the sunrise/sunset occurs on the next day, so we subtract 24 hours from the time and add one day to the datetime object.
 *        Finally, we update the hour, minute, and second components of the datetime object based on the calculated time.
 * @param dttm the datetime object to initialise with the sunrise or sunset time.
 * @param julianDayNumber the Julian Day Number for the date to calculate the sunrise/sunset time for.
 * @param latitude the latitude of the location to calculate the sunrise/sunset time for.
 * @param longitude the longitude of the location to calculate the sunrise/sunset time for.
 * @param timeZoneOffset the time zone offset in hours to adjust the calculated time to the local time zone (inclusive of daylight
 *        saving time). For example, if the local time zone is GMT+2, the timeZoneOffset should be 2.0. If the local time zone is
 *        GMT-5, the timeZoneOffset should be -5.0. if the timeZoneOffset is set to DBL_MAX, the function will attempt to calculate
 *        the time zone offset based on the datetime's date and the system's time zone information. Note that this may not always
 *        be accurate, especially if the datetime's date is far in the past or future, or if the system's time zone information is
 *        not up to date. Therefore, it is recommended to provide an explicit timeZoneOffset whenever possible to ensure accurate
 *        results.
 * @param isSunrise a boolean value indicating whether to calculate the sunrise time (true) or the sunset time (false).
 * @return a pointer to the initialised datetime object. If the sunrise/sunset time cannot be calculated for the given date and
 *         location (e.g. polar night), the time components of the datetime object will be set to 0, and the function will return
 *         the datetime object with the date set to the given Julian Day Number.
 */
static datetime_t *datetime_init_sun_time(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset, bool isSunrise)
{
    datetime_init_jdn(dttm, julianDayNumber);
    datetime_year(dttm); // This will fill in the year, month, day based on the new Julian Day

    datetime_set_sun_time(dttm, latitude, longitude, timeZoneOffset, isSunrise);

    return dttm;
}

static datetime_t *datetime_init_sun_time_checked(datetime_t *dttm,
                                                  long julianDayNumber,
                                                  double latitude,
                                                  double longitude,
                                                  double timeZoneOffset,
                                                  bool isSunrise,
                                                  datetime_sun_status_t *status)
{
    double raw_time;
    datetime_sun_status_t resolved_status;

    if (!dttm)
        return NULL;

    raw_time = datetime_sun_time(julianDayNumber, latitude, longitude, isSunrise);
    resolved_status = datetime_sun_status_from_raw(raw_time);
    if (status)
        *status = resolved_status;

    if (resolved_status != DATETIME_SUN_OK) {
        datetime_init_jdn(dttm, julianDayNumber);
        datetime_year(dttm);
        dttm->hour = 0;
        dttm->minute = 0;
        dttm->second = 0.0;
        return dttm;
    }

    return datetime_init_sun_time(dttm,
                                  julianDayNumber,
                                  latitude,
                                  longitude,
                                  timeZoneOffset,
                                  isSunrise);
}

static datetime_t *datetime_init_adjacent_sun_time_checked(datetime_t *dttm,
                                                           long julianDayNumber,
                                                           double latitude,
                                                           double longitude,
                                                           double timeZoneOffset,
                                                           bool isSunrise,
                                                           int direction,
                                                           datetime_sun_status_t *status)
{
    const int max_search_days = 370;
    datetime_sun_status_t seed_status;
    int seed_offset;
    int low_same_status;
    int high_same_status;
    int answer = -1;

    if (status)
        *status = DATETIME_SUN_UNAVAILABLE;

    if (!dttm || (direction != -1 && direction != 1))
        return NULL;

    seed_status = datetime_sun_status_from_raw(
        datetime_sun_time(julianDayNumber, latitude, longitude, isSunrise)
    );
    if (seed_status == DATETIME_SUN_UNAVAILABLE)
        return NULL;

    seed_offset = 1;
    {
        long candidate_jdn = julianDayNumber + (long)(direction * seed_offset);
        datetime_sun_status_t candidate_status = datetime_sun_status_from_raw(
            datetime_sun_time(candidate_jdn, latitude, longitude, isSunrise)
        );

        if (candidate_status == DATETIME_SUN_UNAVAILABLE)
            return NULL;
        if (candidate_status == DATETIME_SUN_OK) {
            if (status)
                *status = DATETIME_SUN_OK;
            return datetime_init_sun_time(dttm,
                                          candidate_jdn,
                                          latitude,
                                          longitude,
                                          timeZoneOffset,
                                          isSunrise);
        }
        seed_status = candidate_status;
    }

    low_same_status = seed_offset;
    high_same_status = seed_offset;
    for (;;) {
        long candidate_jdn = julianDayNumber + (long)(direction * high_same_status);
        double raw_time = datetime_sun_time(candidate_jdn, latitude, longitude, isSunrise);
        datetime_sun_status_t resolved_status = datetime_sun_status_from_raw(raw_time);

        if (resolved_status != seed_status)
            break;

        if (resolved_status == DATETIME_SUN_UNAVAILABLE)
            return NULL;

        if (high_same_status >= max_search_days)
            return NULL;

        low_same_status = high_same_status;
        high_same_status *= 2;
        if (high_same_status > max_search_days)
            high_same_status = max_search_days;
    }

    {
        int left = low_same_status + 1;
        int right = high_same_status;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            long candidate_jdn = julianDayNumber + (long)(direction * mid);
            double raw_time = datetime_sun_time(candidate_jdn, latitude, longitude, isSunrise);
            datetime_sun_status_t resolved_status = datetime_sun_status_from_raw(raw_time);

            if (resolved_status == DATETIME_SUN_UNAVAILABLE)
                return NULL;

            if (resolved_status != seed_status) {
                answer = mid;
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        }
    }

    if (answer < 0)
        return NULL;

    {
        long candidate_jdn = julianDayNumber + (long)(direction * answer);
        double raw_time = datetime_sun_time(candidate_jdn, latitude, longitude, isSunrise);
        datetime_sun_status_t resolved_status = datetime_sun_status_from_raw(raw_time);

        if (resolved_status != DATETIME_SUN_OK)
            return NULL;
    }

    if (status)
        *status = DATETIME_SUN_OK;
    return datetime_init_sun_time(dttm,
                                  julianDayNumber + (long)(direction * answer),
                                  latitude,
                                  longitude,
                                  timeZoneOffset,
                                  isSunrise);
}

inline datetime_t *datetime_init_sunrise(datetime_t *dttm, long julianDayNumber, double latitude, double longitude, double timeZoneOffset)
{
    return datetime_init_sun_time_checked(dttm,
                                          julianDayNumber,
                                          latitude,
                                          longitude,
                                          timeZoneOffset,
                                          true,
                                          NULL);
}

datetime_t *datetime_init_sunrise_checked(datetime_t *dttm,
                                          long julianDayNumber,
                                          double latitude,
                                          double longitude,
                                          double timeZoneOffset,
                                          datetime_sun_status_t *status)
{
    return datetime_init_sun_time_checked(dttm,
                                          julianDayNumber,
                                          latitude,
                                          longitude,
                                          timeZoneOffset,
                                          true,
                                          status);
}

datetime_t *datetime_init_previous_sunrise_checked(datetime_t *dttm,
                                                   long julianDayNumber,
                                                   double latitude,
                                                   double longitude,
                                                   double timeZoneOffset,
                                                   datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm,
                                                   julianDayNumber,
                                                   latitude,
                                                   longitude,
                                                   timeZoneOffset,
                                                   true,
                                                   -1,
                                                   status);
}

datetime_t *datetime_init_next_sunrise_checked(datetime_t *dttm,
                                               long julianDayNumber,
                                               double latitude,
                                               double longitude,
                                               double timeZoneOffset,
                                               datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm,
                                                   julianDayNumber,
                                                   latitude,
                                                   longitude,
                                                   timeZoneOffset,
                                                   true,
                                                   1,
                                                   status);
}

inline datetime_t *datetime_init_sunset(datetime_t *dttm, long julianDayNumber, double latitude, double longitude, double timeZoneOffset)
{
    return datetime_init_sun_time_checked(dttm,
                                          julianDayNumber,
                                          latitude,
                                          longitude,
                                          timeZoneOffset,
                                          false,
                                          NULL);
}

datetime_t *datetime_init_sunset_checked(datetime_t *dttm,
                                         long julianDayNumber,
                                         double latitude,
                                         double longitude,
                                         double timeZoneOffset,
                                         datetime_sun_status_t *status)
{
    return datetime_init_sun_time_checked(dttm,
                                          julianDayNumber,
                                          latitude,
                                          longitude,
                                          timeZoneOffset,
                                         false,
                                         status);
}

datetime_t *datetime_init_previous_sunset_checked(datetime_t *dttm,
                                                  long julianDayNumber,
                                                  double latitude,
                                                  double longitude,
                                                  double timeZoneOffset,
                                                  datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm,
                                                   julianDayNumber,
                                                   latitude,
                                                   longitude,
                                                   timeZoneOffset,
                                                   false,
                                                   -1,
                                                   status);
}

datetime_t *datetime_init_next_sunset_checked(datetime_t *dttm,
                                              long julianDayNumber,
                                              double latitude,
                                              double longitude,
                                              double timeZoneOffset,
                                              datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm,
                                                   julianDayNumber,
                                                   latitude,
                                                   longitude,
                                                   timeZoneOffset,
                                                   false,
                                                   1,
                                                   status);
}

inline void datetime_set_sunrise(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset)
{
    datetime_set_sun_time(dttm, latitude, longitude, timeZoneOffset, true);
}

inline void datetime_set_sunset(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset)
{
    datetime_set_sun_time(dttm, latitude, longitude, timeZoneOffset, false);
}

datetime_t *datetime_init_sunset_observance_start(datetime_t *dttm,
                                                  const datetime_t *observance_date,
                                                  double latitude,
                                                  double longitude,
                                                  double timeZoneOffset)
{
    datetime_t start_date;

    if (!dttm || !observance_date)
        return NULL;

    datetime_init_copy(&start_date, observance_date);
    if (!datetime_add_days(&start_date, -1))
        return NULL;

    return datetime_init_sunset(dttm,
                                datetime_jdn(&start_date),
                                latitude,
                                longitude,
                                timeZoneOffset);
}

// reference julian day number of a recent full moon
#define NEWMOON_JDN 2451550

// length of a synodic month
#define SYNODIC_MONTH_LENGTH 29.53058867

/**
 * @brief calculate the moon phase for a given Julian Day Number. The moon phase is calculated based on the difference between the given
 *        Julian Day Number and a known new moon date, divided by the length of a synodic month (the average time between new moons). The result is then multiplied by 8 and
 * @param julianDayNumber the Julian Day Number to calculate the moon phase for. The Julian Day Number is a continuous count of days since the beginning of the Julian Period, which is used in astronomy and other fields to represent dates. It is calculated based on the date and time, and can be used to determine the position of celestial bodies, including the moon phase.
 * @return the moon phase for the given Julian Day Number. The moon phases are typically categorized as follows:
 *         DT_NewMoon:        New Moon
 *         DT_WaxingCrescent: Waxing Crescent
 *         DT_FirstQuarter:   First Quarter
 *         DT_WaxingGibbous:  Waxing Gibbous
 *         DT_FullMoon:       Full Moon
 *         DT_WaningGibbous:  Waning Gibbous
 *         DT_LastQuarter:    Last Quarter
 *         DT_WaningCrescent: Waning Crescent
 */
static moon_phase_t datetime_moon_phase_on_jdn(long julianDayNumber)
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

moon_phase_t datetime_moon_phase(const datetime_t *dttm)
{
    long julianDayNumber = datetime_jdn(dttm);
    if (julianDayNumber == LONG_MAX) {
        return DT_NewMoon; // Default value if datetime is not initialised
    }
    return datetime_moon_phase_on_jdn(julianDayNumber);
}

const char *datetime_moon_phase_name(moon_phase_t phase)
{
    static const char *names[] = {
        "New Moon",
        "Waxing Crescent",
        "First Quarter",
        "Waxing Gibbous",
        "Full Moon",
        "Waning Gibbous",
        "Last Quarter",
        "Waning Crescent"
    };

    if (phase < DT_NewMoon || phase > DT_WaningCrescent)
        return "Unknown";
    return names[phase];
}

datetime_t *datetime_next_moon_phase(const datetime_t *dttm, moon_phase_t phase)
{
    static const double phase_fraction = 0.125; // Each moon phase corresponds to 1/8 of the synodic month

    if (dttm == NULL) return NULL; // Invalid input

    if (dttm->year == SHRT_MAX) {
        if (datetime_year(dttm) == SHRT_MAX) {
            return NULL; // Datetime is not initialised
        }
    }

    long jdn = datetime_jdn(dttm);

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
    datetime_t *result = datetime_init_copy(datetime_alloc(), dttm);
    datetime_add_days(result, daysToAdd);

    return result;
}

datetime_t *datetime_next_weekday(const datetime_t *dttm, weekday_t weekday)
{
    if (dttm == NULL) return NULL; // Invalid input

    if (dttm->year == SHRT_MAX) {
        if (datetime_year(dttm) == SHRT_MAX) {
            return NULL; // Datetime is not initialised
        }
    }

    weekday_t currentWeekday = datetime_weekday(dttm);

    int daysToAdd = (weekday - currentWeekday + 7) % 7;
    if (daysToAdd == 0) daysToAdd = 7; // If the target weekday is the same as the current, we want the next occurrence

    datetime_t *result = datetime_init_copy(datetime_alloc(), dttm);
    datetime_add_days(result, daysToAdd);

    return result;
}
