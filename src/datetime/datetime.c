/* Datetime lifetime, construction, parsing, serialisation and comparison. */
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "datetime_internal.h"
#include "ustring.h"

/* Allocate an uninitialised datetime object. */
datetime_t *datetime_alloc()
{
    datetime_t *dttm = (datetime_t *)malloc(sizeof(datetime_t));
    if (dttm != NULL) {
        dttm->year = SHRT_MAX; // Mark as uninitialised
        dttm->month = 0;
        dttm->day = 0;
        dttm->hour = 0;
        dttm->minute = 0;
        dttm->second = 0.0;
        dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
        dttm->JulianDay = DBL_MAX;        // Mark as uninitialised
    }
    return dttm;
}

/* Release a datetime object; a null pointer requires no action. */
void datetime_dealloc(datetime_t *dttm)
{
    if (!dttm)
        return;
    free(dttm);
}

/* Initialise a datetime with the year, the month and the day. */
datetime_t *datetime_init_ymd(datetime_t *dttm, short year, month_t month, uint8_t day)
{
    dttm->year = year;
    dttm->month = month;
    dttm->day = day;
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    dttm->JulianDay = DBL_MAX;        // Mark as uninitialised
    return dttm;
}

/* Initialise a datetime with the year, the month, the day and the time. */
datetime_t *datetime_init_ymdt(datetime_t *dttm, short year, month_t month, uint8_t day, uint8_t hour, uint8_t minute,
    double second)
{
    dttm->year = year;
    dttm->month = month;
    dttm->day = day;
    dttm->hour = hour;
    dttm->minute = minute;
    dttm->second = second;
    dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    dttm->JulianDay = DBL_MAX;        // Mark as uninitialised
    return dttm;
}

/* Initialise a datetime with another datetime. */
datetime_t *datetime_init_copy(datetime_t *dttm_dest, const datetime_t *dttm_src)
{
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

/* Initialise a datetime with a date calculated from a Julian Day Number. */
datetime_t *datetime_init_jdn(datetime_t *dttm, long JulianDayNumber)
{
    dttm->JulianDayNumber = JulianDayNumber;
    dttm->JulianDay = DBL_MAX; // Mark as uninitialised
    dttm->year = SHRT_MAX;     // Mark as uninitialised
    dttm->month = 0;
    dttm->day = 0;
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    // The actual conversion to year, month, day, etc. will be done lazily when needed.
    return dttm;
}

/* Initialise a datetime with a date calculated from a floating point Julian Day. */
datetime_t *datetime_init_jd(datetime_t *dttm, double JulianDay)
{
    dttm->JulianDay = JulianDay;
    dttm->JulianDayNumber = LONG_MAX; // Mark as uninitialised
    dttm->year = SHRT_MAX;            // Mark as uninitialised
    dttm->month = 0;
    dttm->day = 0;
    dttm->hour = 0;
    dttm->minute = 0;
    dttm->second = 0.0;
    // The actual conversion to year, month, day, etc. will be done lazily when needed.
    return dttm;
}

/* Initialise a datetime with the current date and time. */
datetime_t *datetime_init_now(datetime_t *dttm)
{
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
    dttm->JulianDay = DBL_MAX;        // Mark as uninitialised
    // The actual conversion to Julian Day Number and Julian Day will be done lazily when needed

    return dttm;
}

/* Parse a supported civil date into a newly allocated datetime. */
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

/* Serialise a datetime into a SQLite-ready payload. */
bool datetime_serialize(const datetime_t *dttm, string_t **out_type, string_t **out_encoding, void **out_data,
    size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    char buffer[128];
    int n;
    char *payload;

    if (!dttm || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    n = snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%09.6f", (int)datetime_year(dttm),
                 (int)datetime_month(dttm), (int)datetime_day(dttm), (int)datetime_hour(dttm),
                 (int)datetime_minute(dttm), datetime_second(dttm));
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

/* Reconstruct a datetime from a serialised payload. */
datetime_t *datetime_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)
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
    if (strcmp(string_c_str(type), "datetime_t") != 0 || strcmp(string_c_str(encoding), "iso8601/local-v1") != 0)
        return NULL;

    memcpy(buffer, data, len);
    buffer[len] = '\0';
    if (sscanf(buffer, "%d-%d-%dT%d:%d:%lf", &year, &month, &day, &hour, &minute, &second) != 6)
        return NULL;
    if (!datetime_valid_ymd((short)year, (month_t)month, (uint8_t)day))
        return NULL;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0.0 || second >= 60.0)
        return NULL;

    dttm = datetime_alloc();
    if (!dttm)
        return NULL;
    return datetime_init_ymdt(dttm, (short)year, (month_t)month, (uint8_t)day, (uint8_t)hour, (uint8_t)minute, second);
}

datetime_t *datetime_init_materialised_jdn(datetime_t *dttm, long jdn)
{
    if (!dttm || jdn == LONG_MAX)
        return NULL;
    datetime_init_jdn(dttm, jdn);
    datetime_year(dttm);
    return dttm;
}

/* Compare datetime fields for equality, allowing the existing tolerance for seconds. */
bool datetime_equal(const datetime_t *dttm1, const datetime_t *dttm2)
{
    if (dttm1->year == SHRT_MAX)
        datetime_year(dttm1); // Try to calculate the year, month, day, ... if it is not initialised
    if (dttm2->year == SHRT_MAX)
        datetime_year(dttm2); // Try to calculate the year, month, day, ... if it is not initialised

    return dttm1->year == dttm2->year && dttm1->month == dttm2->month && dttm1->day == dttm2->day &&
           dttm1->hour == dttm2->hour && dttm1->minute == dttm2->minute && fabs(dttm1->second - dttm2->second) < 1e-9;
}

/* Report whether the first datetime precedes the second. */
bool datetime_lt(const datetime_t *dttm1, const datetime_t *dttm2)
{
    if (dttm1->year == SHRT_MAX)
        datetime_year(dttm1); // Try to calculate the year, month, day, ... if it is not initialised
    if (dttm2->year == SHRT_MAX)
        datetime_year(dttm2); // Try to calculate the year, month, day, ... if it is not initialised

    if (dttm1->year != dttm2->year)
        return dttm1->year < dttm2->year;
    if (dttm1->month != dttm2->month)
        return dttm1->month < dttm2->month;
    if (dttm1->day != dttm2->day)
        return dttm1->day < dttm2->day;
    if (dttm1->hour != dttm2->hour)
        return dttm1->hour < dttm2->hour;
    if (dttm1->minute != dttm2->minute)
        return dttm1->minute < dttm2->minute;
    return dttm1->second < dttm2->second;
}

/* Report whether the first datetime precedes or equals the second. */
bool datetime_le(const datetime_t *dttm1, const datetime_t *dttm2)
{
    return datetime_lt(dttm1, dttm2) || datetime_equal(dttm1, dttm2);
}

/* Report whether the first datetime follows the second. */
bool datetime_gt(const datetime_t *dttm1, const datetime_t *dttm2)
{
    return !datetime_le(dttm1, dttm2);
}

/* Report whether the first datetime follows or equals the second. */
bool datetime_ge(const datetime_t *dttm1, const datetime_t *dttm2)
{
    return !datetime_lt(dttm1, dttm2);
}

/* Compare two datetimes. */
int datetime_compare(const datetime_t *dttm1, const datetime_t *dttm2)
{
    if (datetime_lt(dttm1, dttm2))
        return -1;
    if (datetime_gt(dttm1, dttm2))
        return 1;
    return 0; // They are equal
}

/* Calculate a hash code for a datetime. */
unsigned int datetime_hash(const datetime_t *dttm)
{
    if (dttm->year == SHRT_MAX)
        datetime_year(dttm); // Try to calculate the year, month, day, ... if it is not initialised

    unsigned int ms =
        (unsigned int)(dttm->hour * 3600000u + dttm->minute * 60000u + (unsigned int)(dttm->second * 1000.0));

    unsigned int dateKey =
        (((unsigned int)dttm->year << 16) | ((unsigned int)dttm->month << 8) | (unsigned int)dttm->day) *
        0x9E3779B1u; // mix year, month, day

    unsigned int hash = dateKey ^ (ms * 0x9E3779B1u); // mix date + time

    // MurmurHash3 finalizer (32‑bit)
    hash ^= hash >> 16;
    hash *= 0x85EBCA6Bu;
    hash ^= hash >> 13;
    hash *= 0xC2B2AE35u;
    hash ^= hash >> 16;

    return hash;
}
