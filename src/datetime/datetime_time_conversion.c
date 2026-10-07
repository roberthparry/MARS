/**
 * @file datetime_time_conversion.c
 * @brief Host timezone and astronomical time-scale conversion.
 *
 * Converts local time to GMT and derives Delta T, terrestrial time and barycentric time values. This unit joins
 * civil time handling to the time scales required by calendar astronomy.
 *
 * This is part of the datetime.h implementation. Jurisdiction holiday policy belongs to the jurisdiction module,
 * while catalogue-backed apparent sky positions belong to almanac.
 */

/* Host timezone conversion and astronomical time scales. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "datetime_internal.h"
#include "datetime_astronomy_internal.h"

/* Estimate TT minus UT in seconds using the existing piecewise year polynomials. */
double datetime_delta_t_estimate(int year)
{
    double offsetYears; /* Years offset from reference epoch for this segment */
    double deltaT;      /* Resulting ΔT in seconds */

    if (year < 1800) {
        offsetYears = year - 1700;
        deltaT = (((-0.0000000851788756 * offsetYears + 0.00013336) * offsetYears - 0.0059285) * offsetYears + 0.1603) *
                     offsetYears +
                 8.83;

    } else if (year < 1860) {
        offsetYears = year - 1800;
        deltaT = (((((0.000000000875 * offsetYears - 0.0000001699) * offsetYears + 0.0000121272) * offsetYears -
                    0.00037436) *
                       offsetYears +
                   0.0041116) *
                      offsetYears +
                  0.0068612) *
                     offsetYears +
                 13.72;

    } else if (year < 1900) {
        offsetYears = year - 1860;
        deltaT =
            ((((0.0000042886428 * offsetYears - 0.0004473624) * offsetYears + 0.01680668) * offsetYears - 0.251754) *
                 offsetYears +
             0.5737) *
                offsetYears +
            7.62;

    } else if (year < 1920) {
        offsetYears = year - 1900;
        deltaT =
            (((-0.000197 * offsetYears + 0.0061966) * offsetYears - 0.0598939) * offsetYears + 1.494119) * offsetYears -
            2.79;

    } else if (year < 1941) {
        offsetYears = year - 1920;
        deltaT = ((0.0020936 * offsetYears - 0.076100) * offsetYears + 0.84493) * offsetYears + 21.20;

    } else if (year < 1961) {
        offsetYears = year - 1950;
        deltaT =
            ((0.000392618767177 * offsetYears - 0.004291845493562231) * offsetYears + 0.407) * offsetYears + 29.107;

    } else if (year < 1986) {
        offsetYears = year - 1975;
        deltaT =
            ((-0.00139275766016713 * offsetYears - 0.00384615384615385) * offsetYears + 1.067) * offsetYears + 45.45;

    } else if (year < 2005) {
        offsetYears = year - 2000;
        deltaT = ((((0.00002373599 * offsetYears + 0.000651814) * offsetYears + 0.0017275) * offsetYears - 0.060374) *
                      offsetYears +
                  0.3345) *
                     offsetYears +
                 63.86;

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

/* Calculate the timezone offset in hours for a given datetime. */
double datetime_tz_offset(const datetime_t *dttm)
{
    // Get the local time and GMT time for the given datetime
    time_t t;
    struct tm local_tm, gmt_tm;

    if (dttm == NULL)
        return DBL_MAX;
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
    if (t == -1)
        return DBL_MAX;

    // Convert time_t to struct tm in GMT
    struct tm *gmtm = gmtime(&t);
    if (gmtm == NULL)
        return DBL_MAX;
    gmt_tm = *gmtm;

    // Calculate the timezone offset in hours
    double offset_hours = (local_tm.tm_hour - gmt_tm.tm_hour) + (local_tm.tm_min - gmt_tm.tm_min) / 60.0 +
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

/* Convert a datetime to GMT (UTC) by subtracting the local timezone offset. */
datetime_t *datetime_to_gmt(datetime_t *dttm)
{
    if (dttm == NULL)
        return NULL;

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
    if (t == -1)
        return NULL;

    // Convert time_t to struct tm in GMT
    struct tm gmt_tm;
    if (gmtime_r(&t, &gmt_tm) == NULL)
        return NULL;

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

/* Estimate Delta T, the difference TT - UT, in seconds for a given year. */
double datetime_delta_t_seconds(int year)
{
    return datetime_delta_t_estimate(year);
}

/* Convert a civil datetime to Julian Date on the Terrestrial Time scale. */
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

/* Convert a civil datetime to Julian Date on the Barycentric Dynamical Time scale. */
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

/* Check if a datetime is in daylight saving time. */
bool datetime_is_dst(const datetime_t *dttm)
{
    if (dttm->year == SHRT_MAX)
        datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised

    struct tm tmdate;
    tmdate.tm_year = dttm->year - 1900;
    tmdate.tm_mon = dttm->month - 1;
    tmdate.tm_mday = dttm->day;
    tmdate.tm_hour = dttm->hour;
    tmdate.tm_min = dttm->minute;
    tmdate.tm_sec = (int)dttm->second;
    tmdate.tm_isdst = -1; // Let mktime determine if DST is in effect

    time_t t = mktime(&tmdate);
    if (t == -1)
        return false; // Could not determine DST status

    return tmdate.tm_isdst > 0;
}
