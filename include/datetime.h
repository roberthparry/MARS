#ifndef _DATETIME_H
#define _DATETIME_H

/**
 * @file datetime.h
 * @brief Gregorian calendar datetime type with astronomical calculations.
 *
 * Provides an opaque datetime_t type supporting:
 *   • Construction from year/month/day and optional time-of-day components
 *   • Arithmetic via datetime_span_t (add/subtract durations)
 *   • Conversion to/from Unix timestamps and Julian Day Numbers
 *   • Calendar queries: leap year, day-of-week, day-of-year, week number
 *   • Astronomical calculations: sunrise/sunset times, moon phase
 *   • Formatting and parsing
 *
 * All datetime_t values are heap-allocated; callers must call datetime_free()
 * exactly once for each handle returned by a constructor.
 */

#include <stdint.h>

/**
 * @brief the datetime type
 */
typedef struct _datetime_t datetime_t;
typedef struct _string_t string_t;

/**
 * @brief the month type (1..12)
 */
typedef enum _month_t {
    DT_January = 1,
    DT_February,
    DT_March,
    DT_April,
    DT_May,
    DT_June,
    DT_July,
    DT_August,
    DT_September,
    DT_October,
    DT_November,
    DT_December
} month_t;

/**
 * @brief the weekday type (1..7)
 */
typedef enum _weekday_t {
    DT_Sunday = 1,
    DT_Monday,
    DT_Tuesday,
    DT_Wednesday,
    DT_Thursday,
    DT_Friday,
    DT_Saturday
} weekday_t;

/**
 * @brief the moon phase type
 */
typedef enum _moon_phase_t {
    DT_NewMoon = 0,
    DT_WaxingCrescent,
    DT_FirstQuarter,
    DT_WaxingGibbous,
    DT_FullMoon,
    DT_WaningGibbous,
    DT_LastQuarter,
    DT_WaningCrescent
} moon_phase_t;

/**
 * @brief a struct to represent a span of time, e.g. for representing the difference between two datetimes, or for representing a duration
 *        to add to a datetime.
 */
typedef struct _datetime_span_t {
    unsigned short years;
    uint8_t months;
    uint8_t days;
    uint8_t hours;
    uint8_t minutes;
    double seconds;
} datetime_span_t;

#define DATETIME_BANK_HOLIDAY_MAX 512

typedef struct _datetime_holiday_t {
    const char *name;
    short year;
    month_t month;
    uint8_t day;
} datetime_holiday_t;

typedef struct _datetime_holiday_list_t {
    unsigned short count;
    datetime_holiday_t items[DATETIME_BANK_HOLIDAY_MAX];
} datetime_holiday_list_t;

datetime_t *datetime_alloc();

/**
 * @brief deallocate a datetime structure. This function should be called to free the memory allocated for a datetime structure when it
 *        is no longer needed. It takes a pointer to the datetime structure to be deallocated and frees the memory associated with it.
 *        After calling this function, the pointer to the datetime structure should not be used, as it will point to deallocated memory.
 * @param dttm the datetime structure to be deallocated.
 */
void datetime_dealloc(datetime_t *dttm);

/**
 * @brief initialise/set a preallocated datetime with the year, the month and the day.
 * @param dttm the datetime variable to be initialised.
 * @param year the year.
 * @param month the month (1..12).
 * @param day the day in the month.
 * @return the address of the initialised datetime.
 */
datetime_t *datetime_init_ymd(datetime_t *dttm, short year, month_t month, uint8_t day);

/**
 * @brief initialise/set a preallocated datetime with the year, the month, the day and the time.
 * @param dttm the datetime variable to be initialised.
 * @param year the year.
 * @param month the month (1..12).
 * @param day the day in the month.
 * @param hour the hour (0..23).
 * @param minute the minute (0..59).
 * @param second the second (0..59) plus fractions of a second.
 * @return the address of the initialised datetime.
 */
datetime_t *datetime_init_ymdt(datetime_t *dttm, short year, month_t month, uint8_t day, uint8_t hour, uint8_t minute, double second);

/**
 * @brief initialise/set a preallocated datetime with another datetime.
 * @param dttm_dest the datetime variable to be initialised.
 * @param dttm_src the datetime to be initialised with.
 * @return the address of the initialised datetime.
 */
datetime_t *datetime_init_copy(datetime_t *dttm_dest, const datetime_t *dttm_src);

/**
 * @brief initialise/set a preallocated datetime with a date calculated from a Julian Day Number.
 * @param dttm the datetime variable to be initialised.
 * @param JulianDayNumber the Julian Day Number.
 * @return the address of the initialised datetime.
 */
datetime_t *datetime_init_jdn(datetime_t *dttm, long JulianDayNumber);

/**
 * @brief initialise/set a preallocated datetime with a date calculated from a floating point Julian Day.
 * @param dttm the datetime variable to be initialised.
 * @param JulianDay the floating point Julian Day.
 * @return the address of the initialised datetime.
 */
datetime_t *datetime_init_jd(datetime_t *dttm, double JulianDay);

/**
 * @brief initialise/set a preallocated datetime with the current date and time.
 * @param dttm the datetime variable to be initialised.
 * @return the address of the initialised datetime.
 */
datetime_t *datetime_init_now(datetime_t *dttm);

/**
 * @brief calculate the date of Easter Sunday for a given year and initialise a datetime structure with that date. The algorithm
 *        used is the Anonymous Gregorian algorithm, which is a well-known method for calculating the date of Easter Sunday in the
 *        Gregorian calendar. The algorithm takes the year as input and calculates the month and day of Easter Sunday based on a
 *        series of mathematical operations. The resulting month and day are then used to initialise the datetime structure.
 *        Easter Sunday is the first Sunday after the first full moon on or after 21 March, which is the vernal equinox.
 * @param dttm the datetime structure to initialise with the date of Easter Sunday. The year field of this structure will be set
 *        to the input year, and the month and day fields will be set to the calculated month and day of Easter Sunday. The hour,
 *        minute, and second fields will be set to 0, and the Julian Day and Julian Day Number fields will be set to their
 *        uninitialised values (DBL_MAX and LONG_MAX, respectively). If the input year is less than 1 or greater than 9999, the
 *        function will return NULL to indicate an error.
 * @param year the year for which to calculate the date of Easter Sunday. This should be a positive integer between 1 and 9999,
 *        inclusive. If the input year is outside this range, the function will return NULL to indicate an error.
 * @return a pointer to the initialised datetime structure with the date of Easter Sunday, or NULL if the input year is invalid.
 */
datetime_t *datetime_init_easter(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with Orthodox Easter Sunday.
 *
 * This is the Julian-calendar Pascha date converted to the Gregorian calendar,
 * used by Russian and Greek Orthodox churches for modern dates.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_orthodox_easter(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with Christmas Day in the Gregorian calendar.
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_christmas(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with Orthodox Christmas Day observed in a Gregorian civil year.
 *
 * This is Julian-calendar 25 December from the previous Julian year, converted
 * to the Gregorian calendar date on which Orthodox Christmas is observed in the
 * requested civil year.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian civil year of the observance.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_orthodox_christmas(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with the Chinese New Year date.
 * @param dttm the datetime variable to be initialised.
 * @param year the year of the Chinese New Year (1700..2400).
 * @return the address of the initialised datetime, or NULL if the year is outside the valid range.
 */
datetime_t *datetime_init_chinese_new_year(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with estimated Diwali.
 *
 * The calculation uses the astronomical new moon in the usual October/November
 * window for India. It is suitable for calendar assistance; local religious
 * observance can differ by region.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_diwali(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with estimated Holi.
 *
 * The calculation uses the India civil date around the Phalguna full moon.
 * It is suitable for calendar assistance; local religious observance can
 * differ by region and tithi rules.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_holi(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with estimated Hindu lunar New Year.
 *
 * This uses the Chaitra lunar new year convention, observed as Ugadi/Gudi
 * Padwa in several regions. It is suitable for calendar assistance; local
 * religious observance can differ by region and tithi rules.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_hindu_new_year(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with estimated Theravada Buddhist New Year.
 *
 * This uses the first April full moon convention. Buddhist New Year varies by
 * tradition and country, so the date is intended for calendar assistance.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_buddhist_new_year(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with estimated Vesak, or Buddha Day.
 *
 * This uses the May full moon convention. Local observance can differ by
 * tradition and country.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_vesak(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with estimated Asalha Puja, or Dharma Day.
 *
 * This uses the July full moon convention. Local observance can differ by
 * tradition and country.
 *
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_asalha_puja(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with the first day of Ramadan in the civil Islamic calendar.
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year in which to search.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_ramadan(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with Eid al-Fitr in the civil Islamic calendar.
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year in which to search.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_eid_al_fitr(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with Muslim New Year in the civil Islamic calendar.
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year in which to search.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_muslim_new_year(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with Rosh Hashanah, the Jewish New Year.
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_jewish_new_year(datetime_t *dttm, int year);

/**
 * @brief initialise/set a preallocated datetime with Passover, Nisan 15 in the Jewish calendar.
 * @param dttm the datetime variable to be initialised.
 * @param year the Gregorian year of the Passover observance.
 * @return the address of the initialised datetime, or NULL if the year is invalid.
 */
datetime_t *datetime_init_passover(datetime_t *dttm, int year);

/**
 * @brief format the selected date in the Christian civil calendar systems.
 *
 * The result uses datetime_format_text() internally and includes both the
 * Gregorian civil date and the corresponding Julian calendar date.
 *
 * @param dttm the datetime to format.
 * @return owned text, or NULL if the date is invalid.
 */
string_t *datetime_christian_calendar_date_text(const datetime_t *dttm);

/**
 * @brief format the selected date in the Chinese lunisolar calendar.
 *
 * The result contains the Chinese year, zodiac animal, lunar month, leap-month
 * marker where applicable, and lunar day. The calculation is astronomical and
 * intended for calendar assistance.
 *
 * @param dttm the datetime to format.
 * @return owned text, or NULL if the date is outside the supported range.
 */
string_t *datetime_chinese_calendar_date_text(const datetime_t *dttm);

/**
 * @brief format the selected date in an Indian Hindu lunisolar calendar style.
 *
 * The result contains a Vikram Samvat year, lunar month, paksha, tithi, and
 * lunar day. The calculation uses India-local lunar events and is intended for
 * calendar assistance because regional rules vary.
 *
 * @param dttm the datetime to format.
 * @return owned text, or NULL if the date is outside the supported range.
 */
string_t *datetime_hindu_calendar_date_text(const datetime_t *dttm);

/**
 * @brief format the selected date in the Thai solar Buddhist Era.
 *
 * @param dttm the datetime to format.
 * @return owned text, or NULL if the date is invalid.
 */
string_t *datetime_buddhist_calendar_date_text(const datetime_t *dttm);

/**
 * @brief format the selected date in the civil Islamic calendar.
 *
 * @param dttm the datetime to format.
 * @return owned text, or NULL if the date is invalid.
 */
string_t *datetime_muslim_calendar_date_text(const datetime_t *dttm);

/**
 * @brief format the selected date in the Jewish calendar.
 *
 * @param dttm the datetime to format.
 * @return owned text, or NULL if the date is invalid.
 */
string_t *datetime_jewish_calendar_date_text(const datetime_t *dttm);

/**
 * @brief list English bank holidays for a Gregorian year.
 * @param year the Gregorian year.
 * @param out output list to fill.
 * @return true on success, false if the year/list is invalid.
 */
bool datetime_english_bank_holidays(int year, datetime_holiday_list_t *out);

/**
 * @brief list English bank holidays whose dates fall within an inclusive date range.
 * @param start the first date in the range.
 * @param end the final date in the range.
 * @param out output list to fill, sorted by date.
 * @return true on success, false if the dates/list are invalid or the output list would overflow.
 */
bool datetime_english_bank_holidays_between(const datetime_t *start, const datetime_t *end, datetime_holiday_list_t *out);

/**
 * @brief calculate the timezone offset in hours for a given datetime. This function uses the system's time zone information to
 *        determine the offset. If the system's time zone information is not available or if the datetime is not properly
 *        initialised, the function will return DBL_MAX.
 * @param dttm the datetime for which to calculate the time zone offset.
 * @return the time zone offset in hours, or DBL_MAX if an error occurs.
 */
double datetime_tz_offset(const datetime_t *dttm);

/**
 * @brief convert a datetime to GMT (UTC) by subtracting the local timezone offset. This function will modify the input datetime
 *        in place and return a pointer to it. If the conversion fails for any reason (e.g. if the input datetime is not properly
 *        initialised), the function will return NULL.
 * @param dttm the datetime to convert to GMT.
 * @return a pointer to the converted datetime, or NULL if the conversion fails.
 */
datetime_t *datetime_to_gmt(datetime_t *dttm);

/**
 * @brief get the year of a datetime. If the year is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
 * @param dttm the datetime to get the year from.
 * @return the year of the datetime, or SHRT_MAX if it is not initialised and cannot be calculated.
 */
short datetime_year(const datetime_t *dttm);

/**
 * @brief get the month of a datetime. If the month is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
 * @param dttm the datetime to get the month from.
 * @return the month of the datetime, or 0 if it is not initialised and cannot be calculated. Note that month 0 is not valid,
 *         so it can be used as a sentinel value for uninitialised month.
 */
month_t datetime_month(const datetime_t *dttm);

/**
 * @brief get the day of a datetime. If the day is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
 * @param dttm the datetime to get the day from.
 * @return the day of the datetime, or 0 if it is not initialised and cannot be calculated. Note that day 0 is not valid,
 *         so it can be used as a sentinel value for uninitialised day.
 */
uint8_t datetime_day(const datetime_t *dttm);

/**
 * @brief get the hour of a datetime. If the hour is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
 * @param dttm the datetime to get the hour from.
 * @return the hour of the datetime, or 0 if it is not initialised and cannot be calculated. Note that hour 0 is not valid,
 *         so it can be used as a sentinel value for uninitialised hour.
 */
uint8_t datetime_hour(const datetime_t *dttm);

/**
 * @brief get the minute of a datetime. If the minute is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
 * @param dttm the datetime to get the minute from.
 * @return the minute of the datetime, or 0 if it is not initialised and cannot be calculated. Note that minute 0 is not valid,
 *         so it can be used as a sentinel value for uninitialised minute.
 */
uint8_t datetime_minute(const datetime_t *dttm);

/**
 * @brief get the second of a datetime. If the second is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
 * @param dttm the datetime to get the second from.
 * @return the second of the datetime, or 0.0 if it is not initialised and cannot be calculated. Note that second 0.0 is not valid,
 *         so it can be used as a sentinel value for uninitialised second.
 */
double datetime_second(const datetime_t *dttm);

/**
 * @brief calculate the Julian Day Number of a given year, month and day.
 * The Julian Day Number is a count of days since the beginning of the Julian Period, and is used as a standard
 * way of expressing dates in astronomy.
 * @param year the year
 * @param month the month (January..December)
 * @param day the day (1..31)
 * @return the Julian Day Number of the given year, month and day.
 */
long datetime_ymd_to_jdn(short year, month_t month, uint8_t day);

/**
 * @brief convert a datetime to a Julian Day Number.
 * @param dttm the datetime to convert.
 * @return the Julian Day Number of the datetime.
 */
long datetime_jdn(const datetime_t *dttm);

/**
 * @brief convert a datetime to a Julian Day.
 * @param dttm the datetime to convert.
 * @return the Julian Day of the datetime.
 */
double datetime_jd(const datetime_t *dttm);

/**
 * @brief get the weekday of a datetime. If the weekday is not initialised, it will be calculated from the Julian Day Number or the
 *        Julian Day.
 * @param dttm the datetime to get the weekday from.
 * @return the weekday of the datetime, or 0 if it is not initialised and cannot be calculated. Note that weekday 0 is not valid,
 *         so it can be used as a sentinel value for uninitialised weekday.
 *         The returned weekday is in the range [Sunday, Saturday] (1-7).
 */
weekday_t datetime_weekday(const datetime_t *dttm);

/**
 * @brief get a display name for a weekday.
 * @param weekday the weekday value.
 * @return a constant string such as "Sunday", or "Unknown" for invalid values.
 */
const char *datetime_weekday_name(weekday_t weekday);

/**
 * @brief check if two datetimes are equal (i.e. represent the same point in time).
 * @param dttm1 the first datetime to compare.
 * @param dttm2 the second datetime to compare.
 * @return true if the datetimes are equal, false otherwise.
 */
bool datetime_equal(const datetime_t *dttm1, const datetime_t *dttm2);

/**
 * @brief check if a datetime is less than another datetime (i.e. represents an earlier point in time).
 * @param dttm1 the first datetime to compare.
 * @param dttm2 the second datetime to compare.
 * @return true if dttm1 is less than dttm2, false otherwise.
 */
bool datetime_lt(const datetime_t *dttm1, const datetime_t *dttm2);

/**
 * @brief check if a datetime is less than or equal to another datetime (i.e. represents an earlier or the same point in time).
 * @param dttm1 the first datetime to compare.
 * @param dttm2 the second datetime to compare.
 * @return true if dttm1 is less than or equal to dttm2, false otherwise.
 */
bool datetime_le(const datetime_t *dttm1, const datetime_t *dttm2);

/**
 * @brief check if a datetime is greater than another datetime (i.e. represents a later point in time).
 * @param dttm1 the first datetime to compare.
 * @param dttm2 the second datetime to compare.
 * @return true if dttm1 is greater than dttm2, false otherwise.
 */
bool datetime_gt(const datetime_t *dttm1, const datetime_t *dttm2);

/**
 * @brief check if a datetime is greater than or equal to another datetime (i.e. represents a later or the same point in time).
 * @param dttm1 the first datetime to compare.
 * @param dttm2 the second datetime to compare.
 * @return true if dttm1 is greater than or equal to dttm2, false otherwise.
 */
bool datetime_ge(const datetime_t *dttm1, const datetime_t *dttm2);

/**
 * @brief compare two datetimes. The result is -1 if dttm1 is less than dttm2, 0 if they are equal, and 1 if dttm1 is greater than dttm2.
 *        This is a common pattern for comparison functions in C,
 * @param dttm1 the first datetime to compare.
 * @param dttm2 the second datetime to compare.
 * @return -1 if dttm1 is less than dttm2, 0 if they are equal, and 1 if dttm1 is greater than dttm2.
 */
int datetime_compare(const datetime_t *dttm1, const datetime_t *dttm2);

/**
 * @brief check if a year is a leap year.
 * @param year the year to check.
 * @return true if the year is a leap year, false otherwise.
 */
bool datetime_is_leap_year(short year);

/**
 * @brief check whether a year/month/day triple is a valid calendar date.
 * @param year the year to check.
 * @param month the month to check.
 * @param day the day to check.
 * @return true if the date is valid, false otherwise.
 */
bool datetime_valid_ymd(short year, month_t month, uint8_t day);

/**
 * @brief get the number of days in a month for a given year and month. This is needed for adding months to a datetime,
 *        because we need to know how many days are in the target month to handle edge cases like adding 1 month to January 31st.
 * @param year the year to get the number of days for (needed to handle leap years for February).
 * @param month the month to get the number of days for.
 * @return the number of days in the given month and year.
 */
unsigned short datetime_days_in_month(short year, month_t month);

/**
 * @brief get the number of days in the month of a datetime. If the month or year is not initialised, it will be calculated from
 *        the Julian Day Number or the Julian Day.
 * @param dttm the datetime to get the number of days in its month from.
 * @return the number of days in the month of the datetime, or 0 if the month or year is not initialised and cannot be calculated.
 *         Note that 0 is not a valid number of days in a month,
 */
unsigned short datetime_days_in_this_month(const datetime_t *dttm);

/**
 * @brief check if a datetime is in daylight saving time. This is a bit tricky because it depends on the local time zone and the rules
 *        for daylight saving time, which can change over time and vary by location.
 * @param dttm the datetime to check.
 * @return true if the datetime is in daylight saving time, false otherwise. If the datetime is in an uninitialised state that cannot
 *         be calculated, it returns false.
 */
bool datetime_is_dst(const datetime_t *dttm);

/**
 * @brief add a number of days to a datetime and return the current datetime. The original datetime is modified.
 * @param dttm the datetime to add days to.
 * @param days the number of days to add (can be negative).
 * @return the address of dttm if successful, or NULL if allocation fails.
 */
datetime_t *datetime_add_days(datetime_t *dttm, long days);

/**
 * @brief add a number of weeks to a datetime and return the current datetime. The original datetime is modified.
 * @param dttm the datetime to add weeks to.
 * @param weeks the number of weeks to add (can be negative).
 * @return the address of dttm if successful, or NULL if allocation fails.
 */
datetime_t *datetime_add_weeks(datetime_t *dttm, int weeks);

/**
 * @brief add a number of months to a datetime and return the current datetime. The original datetime is modified. Note that adding
 *        months is more complex than adding days or weeks because months have different lengths and there are edge cases like adding
 *        1 month to January 31st.
 * @param dttm the datetime to add months to.
 * @param months the number of months to add (can be negative).
 * @return the address of dttm if successful, or NULL if allocation fails.
 */
datetime_t *datetime_add_months(datetime_t *dttm, int months);

/**
 * @brief add a number of years to a datetime and return the current datetime. The original datetime is modified. Note that adding years is more complex than adding days or weeks because of edge cases like adding 1 year to February 29th on a leap year.
 * @param dttm the datetime to add years to.
 * @param years the number of years to add (can be negative).
 * @return the address of dttm if successful, or NULL if allocation fails.
 */
datetime_t *datetime_add_years(datetime_t *dttm, int years);

/**
 * @brief add a number of hours to a datetime and return the current datetime. The original datetime is modified.
 * @param dttm the datetime to add hours to.
 * @param hours the number of hours to add (can be negative).
 * @return the address of dttm if successful, or NULL if allocation fails.
 */
datetime_t *datetime_add_hours(datetime_t *dttm, int hours);

/**
 * @brief add a number of minutes to a datetime and return the current datetime. The original datetime is modified.
 * @param dttm the datetime to add minutes to.
 * @param minutes the number of minutes to add (can be negative).
 * @return the address of dttm if successful, or NULL if allocation fails.
 */
datetime_t *datetime_add_minutes(datetime_t *dttm, int minutes);

/**
 * @brief add a number of seconds to a datetime and return the current datetime. The original datetime is modified.
 * @param dttm the datetime to add seconds to.
 * @param seconds the number of seconds to add (can be negative).
 * @return the address of dttm if successful, or NULL if allocation fails.
 */
datetime_t *datetime_add_seconds(datetime_t *dttm, double seconds);

/**
 * @brief adds a datetime_span_t to a datetime.
 * @param dttm the datetime to be modified.
 * @param span the datetime_span_t to be added to the datetime.
 * @return the modified datetime if successful, or NULL if the datetime is in an uninitialised state that cannot be calculated.
 */
datetime_t *datetime_add_span(datetime_t *dttm, const datetime_span_t *span);

/**
 * @brief subtract a datetime_span_t from a datetime
 *
 * This function subtracts a datetime_span_t from a datetime. If the datetime is in an uninitialised state that cannot be calculated,
 * it will return NULL.
 *
 * @param dttm the datetime to subtract from
 * @param span the datetime_span_t to subtract
 * @return the datetime after subtracting the datetime_span_t, or NULL if the datetime is in an uninitialised state that cannot be calculated.
 */
datetime_t *datetime_sub_span(datetime_t *dttm, const datetime_span_t *span);

/**
 * @brief calculate a hash code for a datetime. This can be useful for using datetimes as keys in hash tables or for quickly
 *        comparing datetimes. The hash code is calculated based on the year, month, day, hour, minute, and second of the datetime.
 *        If the datetime is in an uninitialised state that cannot be calculated, the function will try to calculate the year, month,
 *        day, hour, minute, and second from the Julian Day Number or the Julian Day before calculating the hash. Note that this
 *        means that if you have two datetimes that are in an uninitialised state but represent the same point in time (e.g. they
 *        have the same Julian Day), they will have the same hash code after this function is called.
 * @param dttm the datetime to calculate the hash code for.
 * @return the hash code for the datetime.
 */
unsigned int datetime_hash(const datetime_t *dttm);

/**
 * @brief calculate the duration between two datetimes in days, and optionally fill in a datetime_span_t structure with the
 *        duration in years, months, days, hours, minutes, and seconds. The duration is calculated as dttm1 - dttm2, so it
 *        will be positive if dttm1 is later than dttm2, negative if dttm1 is earlier than dttm2, and zero if they are equal.
 *        If either datetime is in an uninitialised state that cannot be calculated, the function returns DBL_MAX to indicate that
 *        the duration cannot be calculated. If the span parameter is not NULL, it will be filled with the duration in
 *        years, months, days, hours, minutes, and seconds. The span will always represent a positive duration (i.e. years, months,
 *        days, hours, minutes, and seconds will all be non-negative), and the sign of the overall difference will be reflected in
 *        the return value of the function (positive for dttm1 > dttm2, negative for dttm1 < dttm2).
 * @param dttm1 the first datetime to compare.
 * @param dttm2 the second datetime to compare.
 * @param span an optional pointer to a datetime_span_t structure to fill with the duration in years, months, days, hours, minutes,
 *        and seconds. If NULL, the span will not be filled.
 * @return the difference between dttm1 and dttm2 in days, or DBL_MAX if the difference cannot be calculated due to uninitialised
 *         datetimes.
 */
double datetime_duration(const datetime_t *dttm1, const datetime_t *dttm2, datetime_span_t *span);

/**
 * @brief convert a datetime to a formatted string. The format string can contain the following placeholders:
 *        %d   : day number - minimum digits.
 *        %dd  : day number - always 2 digits.
 *        %ddd : shortened day of week ( eg mon, tue, wed, ... )
 *        %Ddd : Shortened day of week ( eg Mon, Tue, Wed, ... )
 *        %DDD : SHORTENED day of week ( eg MON, TUE, WED, ... )
 *        %dddd: full day of week ( eg monday, tuesday, ... )
 *        %Dddd: Full day of week ( eg Monday, Tuesday, ... )
 *        %DDDD: FULL day of week ( eg MONDAY, TUESDAY, ... )
 *        %o   : cardinal for day number ( eg st, nd, th, ... )
 *        %O   : cardinal for day number ( eg ST, ND, TH, ... )
 *        %m   : month number - minimum digits.
 *        %mm  : month number - always 2 digits.
 *        %mmm : shortened month name ( eg jan, feb, ... )
 *        %Mmm : Shortened month name ( eg Jan, Feb, ... )
 *        %MMM : SHORTENED month name ( eg JAN, FEB, ... )
 *        %mmmm: full month name ( eg january, february, ... )
 *        %Mmmm: Full month name ( eg January, February, ... )
 *        %MMMM: FULL month name ( eg JANUARY, FEBRUARY, ... )
 *        %yy  : short year ( eg 97 )
 *        %yyyy: full year ( eg 1997 )
 *        @h   : hour - minimum digits.
 *        @hh  : hour - always 2 digits.
 *        @Hh  : hour in 24 hour clock - always 2 digits.
 *        @m   : minute - minimum digits.
 *        @mm  : minute - always 2 digits.
 *        @s   : second - minimum digits.
 *        @ss  : second - always 2 digits.
 *        @p   : pm / am.
 *        @P   : PM / AM.
 *        %%   : '%' char.
 *        @@   : '@' char.
 *        ^    : void character ( eg "@h^hr" -> "9hr" and "@^@h" -> "@9" )
 *        Fractions of seconds are currently not included.
 * @param dttm the datetime to convert to a formatted string.
 * @param format the format string to use for formatting the datetime. See the description above for the supported placeholders.
 * @return a newly allocated C string containing the formatted datetime. The caller is responsible for freeing the returned string.
 *         If allocation fails or the datetime is not initialised, returns NULL.
 */
char *datetime_format(const datetime_t *dttm, const char *format);

/**
 * @brief Format a datetime using a string object.
 *
 * This is the string_t-based counterpart to datetime_format(). The returned
 * string must be released with string_free().
 */
string_t *datetime_format_text(const datetime_t *dttm, const string_t *format);

/**
 * @brief calculate the sunrise or sunset time for a given date and location. The sunrise/sunset time is calculated using the
 *        algorithm described in the NOAA Solar Calculator, which is based on the equations from the Astronomical Algorithms book
 *        by Jean Meeus. The function takes the Julian Day Number for the date, the latitude and longitude of the location, and a
 *        boolean indicating whether to calculate sunrise or sunset. The function returns the sunrise or sunset time in GMT as a
 *        decimal number of hours (e.g. 6.5 for 6:30 AM). If the sunrise/sunset time cannot be calculated for the given date and
 *        location (e.g. polar night), the function returns -1.0 to indicate that the sun never rises, -2.0 to indicate that the
 *        sun never sets.
 * @param julianDayNumber the Julian Day Number for the date to calculate the sunrise/sunset time for.
 * @param latitude the latitude of the location to calculate the sunrise/sunset time for.
 * @param longitude the longitude of the location to calculate the sunrise/sunset time for.
 * @param isSunrise a boolean value indicating whether to calculate the sunrise time (true) or the sunset time (false).
 * @return the sunrise or sunset time in GMT as a decimal number of hours (e.g. 6.5 for 6:30 AM). If the sunrise/sunset time
 *         cannot be calculated for the given date and location (e.g. polar night), the function returns -1.0 to indicate
 *         that the sun never rises, -2.0 to indicate that the sun never sets.
 */
double datetime_sun_time(long julianDayNumber, double latitude, double longitude, bool isSunrise);

/**
 * @brief approximate the solar declination for a datetime.
 *
 * The returned value is in degrees, positive north of the celestial equator.
 * The calculation is the NOAA fractional-year approximation, suitable for
 * calendar and daylight calculations rather than precision ephemerides.
 *
 * @param dttm the datetime to evaluate.
 * @return declination in degrees, or DBL_MAX if it cannot be calculated.
 */
double datetime_solar_declination(const datetime_t *dttm);

/**
 * @brief approximate the Sun's maximum altitude on a date at a latitude.
 * @param dttm the date to evaluate.
 * @param latitude the observer latitude in degrees.
 * @return maximum altitude in degrees above the horizon, or DBL_MAX on error.
 */
double datetime_solar_max_altitude(const datetime_t *dttm, double latitude);

/**
 * @brief approximate the Sun's solar-noon inclination from the local vertical.
 * @param dttm the date to evaluate.
 * @param latitude the observer latitude in degrees.
 * @return solar-noon inclination in degrees, or DBL_MAX on error.
 */
double datetime_solar_inclination(const datetime_t *dttm, double latitude);

/**
 * @brief initialise a datetime object with the sunrise time for a given date and location. This function is a wrapper around
 *        datetime_init_sun_time() that calls it with the isSunrise parameter set to true to calculate the sunrise time.
 *        The date is specified by the Julian Day Number, and the location is specified by the latitude and longitude. The time zone
 *        offset is also taken into account to adjust the time to the local time zone (inclusive of daylight saving time). If the
 *        calculated time is less than 0, it means that the sunrise occurs on the previous day, so we add 24 hours to the time and
 *        subtract one day from the datetime object.
 * @param dttm the datetime object to initialise with the sunrise time.
 * @param julianDayNumber the Julian Day Number for the date to calculate the sunrise time for.
 * @param latitude the latitude of the location to calculate the sunrise time for.
 * @param longitude the longitude of the location to calculate the sunrise time for.
 * @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
 * @return a pointer to the initialised datetime object. If the sunrise time cannot be calculated for the given date and location
 *         (e.g. polar night), the time components of the datetime object will be set to 0, and the function will return the
 *         datetime object with the date set to the given Julian Day Number.
 */
datetime_t *datetime_init_sunrise(datetime_t *dttm, long julianDayNumber, double latitude, double longitude, double timeZoneOffset);

/**
 * @brief initialise a datetime object with the sunset time for a given date and location. This function is a wrapper around
 *        datetime_init_sun_time() that calls it with the isSunrise parameter set to false to calculate the sunset time.
 *        The date is specified by the Julian Day Number. The location is specified by the latitude and longitude. The time zone
 *        offset is also taken into account to adjust the time to the local time zone (inclusive of daylight saving time). If the
 *        calculated time is less than 0, it means that the sunset occurs on the next day, so we subtract 24 hours from the time and
 *        add one day to the datetime object.
 * @param dttm the datetime object to initialise with the sunset time.
 * @param julianDayNumber the Julian Day Number for the date to calculate the sunset time for.
 * @param latitude the latitude of the location to calculate the sunset time for.
 * @param longitude the longitude of the location to calculate the sunset time for.
 * @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
 * @return a pointer to the initialised datetime object. If the sunset time cannot be calculated for the given date and location
 *         (e.g. polar night), the time components of the datetime object will be set to 0, and the function will return the
 *         datetime object with the date set to the given Julian Day Number.
 */
datetime_t *datetime_init_sunset(datetime_t *dttm, long julianDayNumber, double latitude, double longitude, double timeZoneOffset);

/**
 * @brief set the time components of a datetime object to the sunrise time for its date and a given location. This function is a
 *        wrapper around datetime_set_sun_time() that calls it with the isSunrise parameter set to true. The location is
 *        specified by the latitude and longitude. The time zone offset is used to adjust the calculated time to the local time zone.
 * @param dttm the datetime object to set the time components of.
 * @param latitude the latitude of the location to calculate the sunrise time for.
 * @param longitude the longitude of the location to calculate the sunrise time for.
 * @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
 */
void datetime_set_sunrise(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset);

/**
 * @brief set the time components of a datetime object to the sunset time for its date and a given location. This function is a
 *        wrapper around datetime_set_sun_time() that calls it with the isSunrise parameter set to false. The location is
 *        specified by the latitude and longitude. The time zone offset is used to adjust the calculated time to the local time zone.
 * @param dttm the datetime object to set the time components of.
 * @param latitude the latitude of the location to calculate the sunset time for.
 * @param longitude the longitude of the location to calculate the sunset time for.
 * @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
 */
void datetime_set_sunset(datetime_t *dttm, double latitude, double longitude, double timeZoneOffset);

/**
 * @brief initialise a datetime with the sunset start instant for a sunset-to-sunset calendar date.
 *
 * Some religious calendars treat a date as beginning at sunset on the preceding
 * civil day. This helper takes the civil observance date and returns sunset on
 * the preceding civil day at the requested location.
 *
 * @param dttm the datetime object to initialise.
 * @param observance_date the civil date of the observance.
 * @param latitude observer latitude in degrees.
 * @param longitude observer longitude in degrees.
 * @param timeZoneOffset offset from GMT in hours for the returned clock time; use 0 for GMT.
 * @return a pointer to the initialised datetime, or NULL on invalid input.
 */
datetime_t *datetime_init_sunset_observance_start(datetime_t *dttm,
                                                  const datetime_t *observance_date,
                                                  double latitude,
                                                  double longitude,
                                                  double timeZoneOffset);

/**
 * @brief get the moon phase for a given datetime object. This function calculates the Julian Day Number for the given datetime
 *        object, and then uses that Julian Day Number to calculate the moon phase using the datetime_moon_phase_on_jdn()
 *        function. The moon phase is returned as a value of the moon_phase_t enum, which represents the different phases of the
 *        moon (e.g. New Moon, Waxing Crescent, First Quarter, etc.).
 * @param dttm the datetime object to get the moon phase for. The date components of the datetime object should be set to the
 *        desired date, and the time components can be set to any value (they will not affect the moon phase calculation).
 * @return the moon phase for the given datetime object. The moon phase is returned as a value of the moon_phase_t enum, which
 *         represents the different phases of the moon (e.g. New Moon, Waxing Crescent, First Quarter, etc.). If the datetime object
 *         is not initialised (i.e. its year component is SHRT_MAX), the function will return DT_NewMoon as a default value.
 */
moon_phase_t datetime_moon_phase(const datetime_t *dttm);

/**
 * @brief get a display name for a moon phase.
 * @param phase the moon phase value.
 * @return a constant string such as "Waxing Crescent", or "Unknown" for invalid values.
 */
const char *datetime_moon_phase_name(moon_phase_t phase);

/**
 * @brief find the next datetime with a specific moon phase after a given datetime. This function calculates the moon phase for
 *        the given datetime object, and then iterates forward in time until it finds a datetime with the specified moon phase.
 *        The function returns a pointer to the next datetime object with the specified moon phase. If the given datetime object
 *        is not initialised (i.e. its year component is SHRT_MAX), the function returns NULL.
 * @param dttm the datetime object to start the search from. The date components of the datetime object should be set to the
 *        desired starting date, and the time components can be set to any value (they will not affect the moon phase calculation).
 *        If the datetime object is not initialised (i.e. its year component is SHRT_MAX), the function will return NULL.
 * @param phase the moon phase to search for. This should be a value of the moon_phase_t enum, which represents the different phases
 *        of the moon (e.g. New Moon, Waxing Crescent, First Quarter, etc.).
 * @return a pointer to the next datetime object with the specified moon phase, or NULL if no such datetime exists or if the input
 *         datetime is not initialised. The caller is responsible for freeing the returned datetime object when it is no longer
 *         needed.
 */
datetime_t *datetime_next_moon_phase(const datetime_t *dttm, moon_phase_t phase);

/**
 * @brief find the next datetime with a specific weekday after a given datetime. This function calculates the weekday for the given
 *        datetime object, and then iterates forward in time until it finds a datetime with the specified weekday. The function
 *        returns a pointer to the next datetime object with the specified weekday. If the given datetime object is not initialised
 *        (i.e. its year component is SHRT_MAX), the function returns NULL.
 * @param dttm the datetime object to start the search from. The date components of the datetime object should be set to the
 *        desired starting date, and the time components can be set to any value (they will not affect the weekday calculation).
 *        If the datetime object is not initialised (i.e. its year component is SHRT_MAX), the function will return NULL.
 * @param weekday the weekday to search for (Sunday, Monday, ..., Saturday).
 * @return a pointer to the next datetime object with the specified weekday, or NULL if no such datetime exists or if the input
 *         datetime is not initialised. The caller is responsible for freeing the returned datetime object when it is no longer
 *         needed.
 */
datetime_t *datetime_next_weekday(const datetime_t *dttm, weekday_t weekday);

#endif //_DATETIME_H
