/**
 * @file datetime_solar.c
 * @brief Solar coordinates, solstices and daily horizon events.
 *
 * Estimates solar longitude and sunrise or sunset, including adjacent-day and observance-boundary helpers. These
 * routines support civil calendars and should not be confused with the almanac engine's catalogue-backed
 * calculations.
 *
 * This is part of the datetime.h implementation. Jurisdiction holiday policy belongs to the jurisdiction module,
 * while catalogue-backed apparent sky positions belong to almanac.
 */

/* Solar position estimates, solstices and sunrise/sunset calculations. */
#include <float.h>
#include <limits.h>
#include <math.h>

#include "datetime_internal.h"
#include "datetime_astronomy_internal.h"

static int datetime_day_of_year(const datetime_t *dttm);
static datetime_sun_status_t datetime_sun_status_from_raw(double raw_time);

/* Estimate the December solstice in Terrestrial Time. */
double datetime_dec_solstice_tt(int year)
{
    double Y = (year - 2000) / 1000.0; /* millennia from J2000.0 */
    return (((-0.000000217 * Y - 0.00000084) * Y + 0.000461) * Y + 365242.74049) * Y + 2451900.05952;
}

double datetime_normalise_degrees(double degrees)
{
    double out = fmod(degrees, 360.0);
    if (out < 0.0)
        out += 360.0;
    return out;
}

double datetime_solar_ecliptic_longitude(double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double L0 = datetime_normalise_degrees(280.46646 + 36000.76983 * T + 0.0003032 * T * T);
    double M = datetime_normalise_degrees(357.52911 + 35999.05029 * T - 0.0001537 * T * T);
    double Mrad = M * M_PI / 180.0;
    double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * sin(Mrad) + (0.019993 - 0.000101 * T) * sin(2.0 * Mrad) +
               0.000289 * sin(3.0 * Mrad);
    double omega = (125.04 - 1934.136 * T) * M_PI / 180.0;

    return datetime_normalise_degrees(L0 + C - 0.00569 - 0.00478 * sin(omega));
}

/* Calculate sunrise or sunset in GMT hours, retaining the polar-event sentinel values. */
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

    if (!isfinite(latitude) || !isfinite(longitude) || latitude < -90.0 || latitude > 90.0 || longitude < -180.0 ||
        longitude > 180.0)
        return -1.0;

    date = datetime_init_jdn(datetime_alloc(), julianDayNumber);
    if (!date)
        return -1.0;

    dayOfYear = datetime_day_of_year(date);
    datetime_dealloc(date);
    if (dayOfYear <= 0)
        return -1.0;

    gamma = 2.0 * M_PI / 365.0 * ((double)dayOfYear - 1.0);

    equationOfTime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma) - 0.014615 * cos(2.0 * gamma) -
                               0.040849 * sin(2.0 * gamma));

    solarDeclination = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma) - 0.006758 * cos(2.0 * gamma) +
                       0.000907 * sin(2.0 * gamma) - 0.002697 * cos(3.0 * gamma) + 0.001480 * sin(3.0 * gamma);

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

/* Approximate the solar declination for a datetime. */
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

    hour = (double)datetime_hour(dttm) + (double)datetime_minute(dttm) / 60.0 + datetime_second(dttm) / 3600.0;
    gamma = 2.0 * M_PI / 365.0 * ((double)dayOfYear - 1.0 + (hour - 12.0) / 24.0);

    declination = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma) - 0.006758 * cos(2.0 * gamma) +
                  0.000907 * sin(2.0 * gamma) - 0.002697 * cos(3.0 * gamma) + 0.001480 * sin(3.0 * gamma);

    return declination * radToDeg;
}

/* Approximate the Sun's maximum altitude on a date at a latitude. */
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

/* Approximate the Sun's solar-noon inclination from the local vertical. */
double datetime_solar_inclination(const datetime_t *dttm, double latitude)
{
    double maxAltitude;

    maxAltitude = datetime_solar_max_altitude(dttm, latitude);
    if (maxAltitude == DBL_MAX)
        return DBL_MAX;

    return 90.0 - maxAltitude;
}

/* Set the local solar-event time, adjusting the civil date when midnight is crossed. */
static void datetime_set_sun_time(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset,
    bool isSunrise)
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
    } else if (time >= 24.0) {
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

/* Initialise the date before applying the local solar-event time. */
static datetime_t *datetime_init_sun_time(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset, bool isSunrise)
{
    datetime_init_jdn(dttm, julianDayNumber);
    datetime_year(dttm); // This will fill in the year, month, day based on the new Julian Day

    datetime_set_sun_time(dttm, latitude, longitude, timeZoneOffset, isSunrise);

    return dttm;
}

static datetime_t *datetime_init_sun_time_checked(datetime_t *dttm, long julianDayNumber, double latitude,
    double longitude, double timeZoneOffset, bool isSunrise, datetime_sun_status_t *status)
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

    return datetime_init_sun_time(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, isSunrise);
}

static datetime_t *datetime_init_adjacent_sun_time_checked(datetime_t *dttm, long julianDayNumber, double latitude,
    double longitude, double timeZoneOffset, bool isSunrise, int direction, datetime_sun_status_t *status)
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

    seed_status = datetime_sun_status_from_raw(datetime_sun_time(julianDayNumber, latitude, longitude, isSunrise));
    if (seed_status == DATETIME_SUN_UNAVAILABLE)
        return NULL;

    seed_offset = 1;
    {
        long candidate_jdn = julianDayNumber + (long)(direction * seed_offset);
        datetime_sun_status_t candidate_status =
            datetime_sun_status_from_raw(datetime_sun_time(candidate_jdn, latitude, longitude, isSunrise));

        if (candidate_status == DATETIME_SUN_UNAVAILABLE)
            return NULL;
        if (candidate_status == DATETIME_SUN_OK) {
            if (status)
                *status = DATETIME_SUN_OK;
            return datetime_init_sun_time(dttm, candidate_jdn, latitude, longitude, timeZoneOffset, isSunrise);
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
    return datetime_init_sun_time(dttm, julianDayNumber + (long)(direction * answer), latitude, longitude,
                                  timeZoneOffset, isSunrise);
}

/* Initialise a datetime object with the sunrise time for a given date and location. */
datetime_t *datetime_init_sunrise(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset)
{
    return datetime_init_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, true, NULL);
}

/* Initialise a datetime object with sunrise and return the calculation status. */
datetime_t *datetime_init_sunrise_checked(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset, datetime_sun_status_t *status)
{
    return datetime_init_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, true, status);
}

/* Initialise the previous sunrise before a given civil date. */
datetime_t *datetime_init_previous_sunrise_checked(datetime_t *dttm, long julianDayNumber, double latitude,
    double longitude, double timeZoneOffset, datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, true, -1,
                                                   status);
}

/* Initialise the next sunrise after a given civil date. */
datetime_t *datetime_init_next_sunrise_checked(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset, datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, true, 1,
                                                   status);
}

/* Initialise a datetime object with the sunset time for a given date and location. */
datetime_t *datetime_init_sunset(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset)
{
    return datetime_init_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, false, NULL);
}

/* Initialise a datetime object with sunset and return the calculation status. */
datetime_t *datetime_init_sunset_checked(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset, datetime_sun_status_t *status)
{
    return datetime_init_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, false, status);
}

/* Initialise the previous sunset before a given civil date. */
datetime_t *datetime_init_previous_sunset_checked(datetime_t *dttm, long julianDayNumber, double latitude,
    double longitude, double timeZoneOffset, datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, false,
                                                   -1, status);
}

/* Initialise the next sunset after a given civil date. */
datetime_t *datetime_init_next_sunset_checked(datetime_t *dttm, long julianDayNumber, double latitude, double longitude,
    double timeZoneOffset, datetime_sun_status_t *status)
{
    return datetime_init_adjacent_sun_time_checked(dttm, julianDayNumber, latitude, longitude, timeZoneOffset, false, 1,
                                                   status);
}

/* Set the time components of a datetime object to the sunrise time for its date and a given location. */
void datetime_set_sunrise(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset)
{
    datetime_set_sun_time(dttm, latitude, longitude, timeZoneOffset, true);
}

/* Set the time components of a datetime object to the sunset time for its date and a given location. */
void datetime_set_sunset(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset)
{
    datetime_set_sun_time(dttm, latitude, longitude, timeZoneOffset, false);
}

/* Initialise a datetime with the sunset start instant for a sunset-to-sunset calendar date. */
datetime_t *datetime_init_sunset_observance_start(datetime_t *dttm, const datetime_t *observance_date, double latitude,
    double longitude, double timeZoneOffset)
{
    datetime_t start_date;

    if (!dttm || !observance_date)
        return NULL;

    datetime_init_copy(&start_date, observance_date);
    if (!datetime_add_days(&start_date, -1))
        return NULL;

    return datetime_init_sunset(dttm, datetime_jdn(&start_date), latitude, longitude, timeZoneOffset);
}
