#ifndef _DTTM_H
#define _DTTM_H

#include <stdint.h>

/// @brief the datetime type
typedef struct _dttm_t dttm_t;

/// @brief the month type (1..12)
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

/// @brief the weekday type (1..7)
typedef enum _weekday_t {
    DT_Sunday = 1,
    DT_Monday,
    DT_Tuesday,
    DT_Wednesday,
    DT_Thursday,
    DT_Friday,
    DT_Saturday
} weekday_t;

/// @brief the moon phase type
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

/// @brief a struct to represent a span of time, e.g. for representing the difference between two datetimes, or for representing a duration
///        to add to a datetime.
typedef struct _dttm_span_t {
    unsigned short years;
    uint8_t months;
    uint8_t days;
    uint8_t hours;
    uint8_t minutes;
    double seconds;
} dttm_span_t;

dttm_t *dttm_alloc();

/// @brief deallocate a datetime structure. This function should be called to free the memory allocated for a datetime structure when it 
///        is no longer needed. It takes a pointer to the datetime structure to be deallocated and frees the memory associated with it.
///        After calling this function, the pointer to the datetime structure should not be used, as it will point to deallocated memory.
/// @param self the datetime structure to be deallocated.
void dttm_dealloc(dttm_t *self);

/// @brief initialise/set a preallocated datetime with the year, the month and the day.
/// @param self the datetime variable to be initialised.
/// @param year the year.
/// @param month the month (1..12).
/// @param day the day in the month.
/// @return the address of the initialised datetime.
dttm_t *dttm_init_ymd(dttm_t *self, short year, month_t month, uint8_t day);

/// @brief initialise/set a preallocated datetime with the year, the month, the day and the time.
/// @param self the datetime variable to be initialised.
/// @param year the year.
/// @param month the month (1..12).
/// @param day the day in the month.
/// @param hour the hour (0..23).
/// @param minute the minute (0..59).
/// @param second the second (0..59) plus fractions of a second.
/// @return the address of the initialised datetime.
dttm_t *dttm_init_ymdt(dttm_t *self, short year, month_t month, uint8_t day, uint8_t hour, uint8_t minute, double second);

/// @brief initialise/set a preallocated datetime with another datetime.
/// @param self the datetime variable to be initialised.
/// @param datetime the datetime to be initialised with.
/// @return the address of the initialised datetime.
dttm_t *dttm_init_copy(dttm_t *self, const dttm_t *datetime);

/// @brief initialise/set a preallocated datetime with a date calculated from a Julian Day Number.
/// @param self the datetime variable to be initialised.
/// @param JulianDayNumber the Julian Day Number.
/// @return the address of the initialised datetime.
dttm_t *dttm_init_jdn(dttm_t *self, long JulianDayNumber);

/// @brief initialise/set a preallocated datetime with a date calculated from a floating point Julian Day.
/// @param self the datetime variable to be initialised.
/// @param JulianDay the floating point Julian Day.
/// @return the address of the initialised datetime.
dttm_t *dttm_init_jd(dttm_t *self, double JulianDay);

/// @brief initialise/set a preallocated datetime with the current date and time.
/// @param self the datetime variable to be initialised.
/// @return the address of the initialised datetime.
dttm_t *dttm_init_now(dttm_t *self);

/// @brief calculate the date of Easter Sunday for a given year and initialise a datetime structure with that date. The algorithm 
///        used is the Anonymous Gregorian algorithm, which is a well-known method for calculating the date of Easter Sunday in the 
///        Gregorian calendar. The algorithm takes the year as input and calculates the month and day of Easter Sunday based on a 
///        series of mathematical operations. The resulting month and day are then used to initialise the datetime structure.
///        Easter Sunday is the first Sunday after the first full moon on or after 21 March, which is the vernal equinox.
/// @param self the datetime structure to initialise with the date of Easter Sunday. The year field of this structure will be set 
///        to the input year, and the month and day fields will be set to the calculated month and day of Easter Sunday. The hour,
///        minute, and second fields will be set to 0, and the Julian Day and Julian Day Number fields will be set to their 
///        uninitialised values (DBL_MAX and LONG_MAX, respectively). If the input year is less than 1 or greater than 9999, the 
///        function will return NULL to indicate an error.
/// @param year the year for which to calculate the date of Easter Sunday. This should be a positive integer between 1 and 9999, 
///        inclusive. If the input year is outside this range, the function will return NULL to indicate an error.
/// @return a pointer to the initialised datetime structure with the date of Easter Sunday, or NULL if the input year is invalid.
dttm_t *dttm_init_easter(dttm_t *self, int year);

/// @brief initialise/set a preallocated datetime with the Chinese New Year date.
/// @param self the datetime variable to be initialised.
/// @param year the year of the Chinese New Year (1700..2400).
/// @return the address of the initialised datetime, or NULL if the year is outside the valid range.
dttm_t *dttm_init_chinese_new_year(dttm_t *self, int year);

/// @brief calculate the timezone offset in hours for a given datetime. This function uses the system's time zone information to 
///        determine the offset. If the system's time zone information is not available or if the datetime is not properly 
///        initialised, the function will return DBL_MAX.
/// @param self the datetime for which to calculate the time zone offset.
/// @return the time zone offset in hours, or DBL_MAX if an error occurs.
double dttm_tz_offset(const dttm_t *self);

/// @brief convert a datetime to GMT (UTC) by subtracting the local timezone offset. This function will modify the input datetime 
///        in place and return a pointer to it. If the conversion fails for any reason (e.g. if the input datetime is not properly
///        initialised), the function will return NULL.
/// @param self the datetime to convert to GMT.
/// @return a pointer to the converted datetime, or NULL if the conversion fails.
dttm_t *dttm_to_gmt(dttm_t *self);

/// @brief get the year of a datetime. If the year is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
/// @param self the datetime to get the year from.
/// @return the year of the datetime, or SHRT_MAX if it is not initialised and cannot be calculated.
short dttm_year(const dttm_t *self);

/// @brief get the month of a datetime. If the month is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
/// @param self the datetime to get the month from.
/// @return the month of the datetime, or 0 if it is not initialised and cannot be calculated. Note that month 0 is not valid, 
///         so it can be used as a sentinel value for uninitialised month.
month_t dttm_month(const dttm_t *self);

/// @brief get the day of a datetime. If the day is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
/// @param self the datetime to get the day from.
/// @return the day of the datetime, or 0 if it is not initialised and cannot be calculated. Note that day 0 is not valid, 
///         so it can be used as a sentinel value for uninitialised day.
uint8_t dttm_day(const dttm_t *self);

/// @brief get the hour of a datetime. If the hour is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
/// @param self the datetime to get the hour from.
/// @return the hour of the datetime, or 0 if it is not initialised and cannot be calculated. Note that hour 0 is not valid, 
///         so it can be used as a sentinel value for uninitialised hour.
uint8_t dttm_hour(const dttm_t *self);

/// @brief get the minute of a datetime. If the minute is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
/// @param self the datetime to get the minute from.
/// @return the minute of the datetime, or 0 if it is not initialised and cannot be calculated. Note that minute 0 is not valid, 
///         so it can be used as a sentinel value for uninitialised minute.
uint8_t dttm_minute(const dttm_t *self);

/// @brief get the second of a datetime. If the second is not initialised, it will be calculated from the Julian Day Number or the Julian Day.
/// @param self the datetime to get the second from.
/// @return the second of the datetime, or 0.0 if it is not initialised and cannot be calculated. Note that second 0.0 is not valid, 
///         so it can be used as a sentinel value for uninitialised second.
double dttm_second(const dttm_t *self);

/// @brief calculate the Julian Day Number of a given year, month and day.
/// The Julian Day Number is a count of days since the beginning of the Julian Period, and is used as a standard
/// way of expressing dates in astronomy.
/// @param year the year
/// @param month the month (January..December)
/// @param day the day (1..31)
/// @return the Julian Day Number of the given year, month and day.
long dttm_ymd_to_jdn(short year, month_t month, uint8_t day);

/// @brief convert a datetime to a Julian Day Number.
/// @param self the datetime to convert.
/// @return the Julian Day Number of the datetime.
long dttm_jdn(const dttm_t *self);

/// @brief convert a datetime to a Julian Day.
/// @param self the datetime to convert.
/// @return the Julian Day of the datetime. 
double dttm_jd(const dttm_t *self);

/// @brief get the weekday of a datetime. If the weekday is not initialised, it will be calculated from the Julian Day Number or the 
///        Julian Day.
/// @param self the datetime to get the weekday from.
/// @return the weekday of the datetime, or 0 if it is not initialised and cannot be calculated. Note that weekday 0 is not valid, 
///         so it can be used as a sentinel value for uninitialised weekday.
///         The returned weekday is in the range [Sunday, Saturday] (1-7).
weekday_t dttm_weekday(const dttm_t *self);

/// @brief check if two datetimes are equal (i.e. represent the same point in time).
/// @param self the first datetime to compare. 
/// @param datetime the second datetime to compare.
/// @return true if the datetimes are equal, false otherwise.
bool dttm_equal(const dttm_t *dt1, const dttm_t *dt2);

/// @brief check if a datetime is less than another datetime (i.e. represents an earlier point in time).
/// @param self the first datetime to compare.
/// @param datetime the second datetime to compare.
/// @return true if self is less than datetime, false otherwise.
bool dttm_lt(const dttm_t *self, const dttm_t *datetime);

/// @brief check if a datetime is less than or equal to another datetime (i.e. represents an earlier or the same point in time).
/// @param self the first datetime to compare.
/// @param datetime the second datetime to compare.
/// @return true if self is less than or equal to datetime, false otherwise.
bool dttm_le(const dttm_t *self, const dttm_t *datetime);

/// @brief check if a datetime is greater than another datetime (i.e. represents a later point in time).
/// @param self the first datetime to compare.
/// @param datetime the second datetime to compare.
/// @return true if self is greater than datetime, false otherwise.
bool dttm_gt(const dttm_t *self, const dttm_t *datetime);

/// @brief check if a datetime is greater than or equal to another datetime (i.e. represents a later or the same point in time).
/// @param self the first datetime to compare.
/// @param datetime the second datetime to compare.
/// @return true if self is greater than or equal to datetime, false otherwise.
bool dttm_ge(const dttm_t *self, const dttm_t *datetime);

/// @brief compare two datetimes. The result is -1 if self is less than datetime, 0 if they are equal, and 1 if self is greater than datetime. 
///        This is a common pattern for comparison functions in C,
/// @param self the first datetime to compare.
/// @param datetime the second datetime to compare.
/// @return -1 if self is less than datetime, 0 if they are equal, and 1 if self is greater than datetime.
int dttm_compare(const dttm_t *self, const dttm_t *datetime);

/// @brief check if a year is a leap year.
/// @param year the year to check.
/// @return true if the year is a leap year, false otherwise.
bool dttm_is_leap_year(short year);

/// @brief get the number of days in a month for a given year and month. This is needed for adding months to a datetime,
///        because we need to know how many days are in the target month to handle edge cases like adding 1 month to January 31st.
/// @param year the year to get the number of days for (needed to handle leap years for February).
/// @param month the month to get the number of days for.
/// @return the number of days in the given month and year.
unsigned short dttm_days_in_month(short year, month_t month);

/// @brief get the number of days in the month of a datetime. If the month or year is not initialised, it will be calculated from 
///        the Julian Day Number or the Julian Day.
/// @param self the datetime to get the number of days in its month from.
/// @return the number of days in the month of the datetime, or 0 if the month or year is not initialised and cannot be calculated. 
///         Note that 0 is not a valid number of days in a month,
unsigned short dttm_days_in_this_month(const dttm_t *self);

/// @brief check if a datetime is in daylight saving time. This is a bit tricky because it depends on the local time zone and the rules 
///        for daylight saving time, which can change over time and vary by location.
/// @param self the datetime to check.
/// @return true if the datetime is in daylight saving time, false otherwise. If the datetime is in an uninitialised state that cannot 
///         be calculated, it returns false.
bool dttm_is_dst(const dttm_t *self);

/// @brief add a number of days to a datetime and return the current datetime. The original datetime is modified.
/// @param self the datetime to add days to.
/// @param days the number of days to add (can be negative).
/// @return the address of self if successful, or NULL if allocation fails.    
dttm_t *dttm_add_days(dttm_t *self, long days);

/// @brief add a number of weeks to a datetime and return the current datetime. The original datetime is modified.
/// @param self the datetime to add weeks to.
/// @param weeks the number of weeks to add (can be negative).
/// @return the address of self if successful, or NULL if allocation fails.
dttm_t *dttm_add_weeks(dttm_t *self, int weeks);

/// @brief add a number of months to a datetime and return the current datetime. The original datetime is modified. Note that adding 
///        months is more complex than adding days or weeks because months have different lengths and there are edge cases like adding
///        1 month to January 31st.
/// @param self the datetime to add months to.
/// @param months the number of months to add (can be negative).
/// @return the address of self if successful, or NULL if allocation fails.
dttm_t *dttm_add_months(dttm_t *self, int months);

/// @brief add a number of years to a datetime and return the current datetime. The original datetime is modified. Note that adding years is more complex than adding days or weeks because of edge cases like adding 1 year to February 29th on a leap year.
/// @param self the datetime to add years to.
/// @param years the number of years to add (can be negative).
/// @return the address of self if successful, or NULL if allocation fails.
dttm_t *dttm_add_years(dttm_t *self, int years);

/// @brief add a number of hours to a datetime and return the current datetime. The original datetime is modified.
/// @param self the datetime to add hours to.
/// @param hours the number of hours to add (can be negative).
/// @return the address of self if successful, or NULL if allocation fails.
dttm_t *dttm_add_hours(dttm_t *self, int hours);

/// @brief add a number of minutes to a datetime and return the current datetime. The original datetime is modified.
/// @param self the datetime to add minutes to.
/// @param minutes the number of minutes to add (can be negative).
/// @return the address of self if successful, or NULL if allocation fails.
dttm_t *dttm_add_minutes(dttm_t *self, int minutes);

/// @brief add a number of seconds to a datetime and return the current datetime. The original datetime is modified.
/// @param self the datetime to add seconds to.
/// @param seconds the number of seconds to add (can be negative).
/// @return the address of self if successful, or NULL if allocation fails.
dttm_t *dttm_add_seconds(dttm_t *self, double seconds);

/// @brief adds a dttm_span_t to a datetime.
/// @param self the datetime to be modified.
/// @param span the dttm_span_t to be added to the datetime.
/// @return the modified datetime if successful, or NULL if the datetime is in an uninitialised state that cannot be calculated.
dttm_t *dttm_add_span(dttm_t *self, const dttm_span_t *span);

/// @brief subtract a dttm_span_t from a datetime
///
/// This function subtracts a dttm_span_t from a datetime. If the datetime is in an uninitialised state that cannot be calculated, 
/// it will return NULL. 
///
/// @param self the datetime to subtract from
/// @param span the dttm_span_t to subtract
/// @return the datetime after subtracting the dttm_span_t, or NULL if the datetime is in an uninitialised state that cannot be calculated.
dttm_t *dttm_sub_span(dttm_t *self, const dttm_span_t *span);

/// @brief calculate a hash code for a datetime. This can be useful for using datetimes as keys in hash tables or for quickly 
///        comparing datetimes. The hash code is calculated based on the year, month, day, hour, minute, and second of the datetime. 
///        If the datetime is in an uninitialised state that cannot be calculated, the function will try to calculate the year, month,
///        day, hour, minute, and second from the Julian Day Number or the Julian Day before calculating the hash. Note that this 
///        means that if you have two datetimes that are in an uninitialised state but represent the same point in time (e.g. they 
///        have the same Julian Day), they will have the same hash code after this function is called.
/// @param self the datetime to calculate the hash code for.
/// @return the hash code for the datetime.
unsigned int dttm_hash(const dttm_t *self);

/// @brief calculate the duration between two datetimes in days, and optionally fill in a dttm_span_t structure with the 
///        duration in years, months, days, hours, minutes, and seconds. The duration is calculated as self - datetime, so it 
///        will be positive if self is later than datetime, negative if self is earlier than datetime, and zero if they are equal. 
///        If either datetime is in an uninitialised state that cannot be calculated, the function returns DBL_MAX to indicate that 
///        the duration cannot be calculated. If the span parameter is not NULL, it will be filled with the duration in 
///        years, months, days, hours, minutes, and seconds. The span will always represent a positive duration (i.e. years, months, 
///        days, hours, minutes, and seconds will all be non-negative), and the sign of the overall difference will be reflected in 
///        the return value of the function (positive for self > datetime, negative for self < datetime).
/// @param self the first datetime to compare.
/// @param datetime the second datetime to compare.
/// @param span an optional pointer to a dttm_span_t structure to fill with the duration in years, months, days, hours, minutes, 
///        and seconds. If NULL, the span will not be filled.
/// @return the difference between self and datetime in days, or DBL_MAX if the difference cannot be calculated due to uninitialised 
///         datetimes.
double dttm_duration(const dttm_t *self, const dttm_t *datetime, dttm_span_t *span);

/// @brief convert a datetime to a formatted string. The format string can contain the following placeholders:
///        %d   : day number - minimum digits.
///        %dd  : day number - always 2 digits.
///        %ddd : shortened day of week ( eg mon, tue, wed, ... )
///        %Ddd : Shortened day of week ( eg Mon, Tue, Wed, ... )
///        %DDD : SHORTENED day of week ( eg MON, TUE, WED, ... )
///        %dddd: full day of week ( eg monday, tuesday, ... )
///        %Dddd: Full day of week ( eg Monday, Tuesday, ... )
///        %DDDD: FULL day of week ( eg MONDAY, TUESDAY, ... )
///        %o   : cardinal for day number ( eg st, nd, th, ... )
///        %O   : cardinal for day number ( eg ST, ND, TH, ... )
///        %m   : month number - minimum digits.
///        %mm  : month number - always 2 digits.
///        %mmm : shortened month name ( eg jan, feb, ... )
///        %Mmm : Shortened month name ( eg Jan, Feb, ... )
///        %MMM : SHORTENED month name ( eg JAN, FEB, ... )
///        %mmmm: full month name ( eg january, february, ... )
///        %Mmmm: Full month name ( eg January, February, ... )
///        %MMMM: FULL month name ( eg JANUARY, FEBRUARY, ... )
///        %yy  : short year ( eg 97 )
///        %yyyy: full year ( eg 1997 )
///        @h   : hour - minimum digits.
///        @hh  : hour - always 2 digits.
///        @Hh  : hour in 24 hour clock - always 2 digits.
///        @m   : minute - minimum digits.
///        @mm  : minute - always 2 digits.
///        @s   : second - minimum digits.
///        @ss  : second - always 2 digits.
///        @p   : pm / am.
///        @P   : PM / AM.
///        %%   : '%' char.
///        @@   : '@' char.
///        ^    : void character ( eg "@h^hr" -> "9hr" and "@^@h" -> "@9" )
///        Fractions of seconds are currently not included.
/// @param self the datetime to convert to a formatted string.
/// @param format the format string to use for formatting the datetime. See the description above for the supported placeholders.
/// @return a newly allocated string containing the formatted datetime. The caller is responsible for freeing the returned string.
///         If allocation fails or the datetime is not initialised, returns NULL.
char *dttm_format(const dttm_t *self, const char *format);

/// @brief calculate the sunrise or sunset time for a given date and location. The sunrise/sunset time is calculated using the 
///        algorithm described in the NOAA Solar Calculator, which is based on the equations from the Astronomical Algorithms book
///        by Jean Meeus. The function takes the Julian Day Number for the date, the latitude and longitude of the location, and a
///        boolean indicating whether to calculate sunrise or sunset. The function returns the sunrise or sunset time in GMT as a 
///        decimal number of hours (e.g. 6.5 for 6:30 AM). If the sunrise/sunset time cannot be calculated for the given date and 
///        location (e.g. polar night), the function returns -1.0 to indicate that the sun never rises, -2.0 to indicate that the 
///        sun never sets.
/// @param julianDayNumber the Julian Day Number for the date to calculate the sunrise/sunset time for.
/// @param latitude the latitude of the location to calculate the sunrise/sunset time for.
/// @param longitude the longitude of the location to calculate the sunrise/sunset time for.
/// @param isSunrise a boolean value indicating whether to calculate the sunrise time (true) or the sunset time (false).
/// @return the sunrise or sunset time in GMT as a decimal number of hours (e.g. 6.5 for 6:30 AM). If the sunrise/sunset time
///         cannot be calculated for the given date and location (e.g. polar night), the function returns -1.0 to indicate 
///         that the sun never rises, -2.0 to indicate that the sun never sets.
double dttm_sun_time(long julianDayNumber, double latitude, double longitude, bool isSunrise);

/// @brief initialise a datetime object with the sunrise time for a given date and location. This function is a wrapper around
///        dttm_init_sun_time() that calls it with the isSunrise parameter set to true to calculate the sunrise time.
///        The date is specified by the Julian Day Number, and the location is specified by the latitude and longitude. The time zone
///        offset is also taken into account to adjust the time to the local time zone (inclusive of daylight saving time). If the
///        calculated time is less than 0, it means that the sunrise occurs on the previous day, so we add 24 hours to the time and 
///        subtract one day from the datetime object.
/// @param self the datetime object to initialise with the sunrise time.
/// @param julianDayNumber the Julian Day Number for the date to calculate the sunrise time for.
/// @param latitude the latitude of the location to calculate the sunrise time for.
/// @param longitude the longitude of the location to calculate the sunrise time for.
/// @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
/// @return a pointer to the initialised datetime object. If the sunrise time cannot be calculated for the given date and location 
///         (e.g. polar night), the time components of the datetime object will be set to 0, and the function will return the 
///         datetime object with the date set to the given Julian Day Number.
dttm_t *dttm_init_sunrise(dttm_t *self, long julianDayNumber, double latitude, double longitude, double timeZoneOffset);

/// @brief initialise a datetime object with the sunset time for a given date and location. This function is a wrapper around
///        dttm_init_sun_time() that calls it with the isSunrise parameter set to false to calculate the sunset time.
///        The date is specified by the Julian Day Number. The location is specified by the latitude and longitude. The time zone 
///        offset is also taken into account to adjust the time to the local time zone (inclusive of daylight saving time). If the
///        calculated time is less than 0, it means that the sunset occurs on the next day, so we subtract 24 hours from the time and
///        add one day to the datetime object.
/// @param self the datetime object to initialise with the sunset time.
/// @param julianDayNumber the Julian Day Number for the date to calculate the sunset time for.
/// @param latitude the latitude of the location to calculate the sunset time for.
/// @param longitude the longitude of the location to calculate the sunset time for.
/// @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
/// @return a pointer to the initialised datetime object. If the sunset time cannot be calculated for the given date and location 
///         (e.g. polar night), the time components of the datetime object will be set to 0, and the function will return the 
///         datetime object with the date set to the given Julian Day Number.
dttm_t *dttm_init_sunset(dttm_t *self, long julianDayNumber, double latitude, double longitude, double timeZoneOffset);

/// @brief set the time components of a datetime object to the sunrise time for its date and a given location. This function is a 
///        wrapper around dttm_set_sun_time() that calls it with the isSunrise parameter set to true. The location is 
///        specified by the latitude and longitude. The time zone offset is used to adjust the calculated time to the local time zone.
/// @param self the datetime object to set the time components of.
/// @param latitude the latitude of the location to calculate the sunrise time for.
/// @param longitude the longitude of the location to calculate the sunrise time for.
/// @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
void dttm_set_sunrise(dttm_t *self, double latitude, double longitude, double timeZoneOffset);

/// @brief set the time components of a datetime object to the sunset time for its date and a given location. This function is a 
///        wrapper around dttm_set_sun_time() that calls it with the isSunrise parameter set to false. The location is 
///        specified by the latitude and longitude. The time zone offset is used to adjust the calculated time to the local time zone.
/// @param self the datetime object to set the time components of.
/// @param latitude the latitude of the location to calculate the sunset time for.
/// @param longitude the longitude of the location to calculate the sunset time for.
/// @param timeZoneOffset the time zone offset in hours to adjust the calculated time to local time.
void dttm_set_sunset(dttm_t *self, double latitude, double longitude, double timeZoneOffset);

/// @brief get the moon phase for a given datetime object. This function calculates the Julian Day Number for the given datetime 
///        object, and then uses that Julian Day Number to calculate the moon phase using the dttm_moon_phase_on_jdn()
///        function. The moon phase is returned as a value of the moon_phase_t enum, which represents the different phases of the 
///        moon (e.g. New Moon, Waxing Crescent, First Quarter, etc.).
/// @param self the datetime object to get the moon phase for. The date components of the datetime object should be set to the 
///        desired date, and the time components can be set to any value (they will not affect the moon phase calculation). 
/// @return the moon phase for the given datetime object. The moon phase is returned as a value of the moon_phase_t enum, which 
///         represents the different phases of the moon (e.g. New Moon, Waxing Crescent, First Quarter, etc.). If the datetime object 
///         is not initialised (i.e. its year component is SHRT_MAX), the function will return DT_NewMoon as a default value.
moon_phase_t dttm_moon_phase(const dttm_t *self);

/// @brief find the next datetime with a specific moon phase after a given datetime. This function calculates the moon phase for 
///        the given datetime object, and then iterates forward in time until it finds a datetime with the specified moon phase. 
///        The function returns a pointer to the next datetime object with the specified moon phase. If the given datetime object 
///        is not initialised (i.e. its year component is SHRT_MAX), the function returns NULL.
/// @param self the datetime object to start the search from. The date components of the datetime object should be set to the 
///        desired starting date, and the time components can be set to any value (they will not affect the moon phase calculation).
///        If the datetime object is not initialised (i.e. its year component is SHRT_MAX), the function will return NULL.
/// @param phase the moon phase to search for. This should be a value of the moon_phase_t enum, which represents the different phases
///        of the moon (e.g. New Moon, Waxing Crescent, First Quarter, etc.).
/// @return a pointer to the next datetime object with the specified moon phase, or NULL if no such datetime exists or if the input 
///         datetime is not initialised. The caller is responsible for freeing the returned datetime object when it is no longer 
///         needed.
dttm_t *dttm_next_moon_phase(const dttm_t *self, moon_phase_t phase);

/// @brief find the next datetime with a specific weekday after a given datetime. This function calculates the weekday for the given 
///        datetime object, and then iterates forward in time until it finds a datetime with the specified weekday. The function 
///        returns a pointer to the next datetime object with the specified weekday. If the given datetime object is not initialised 
///        (i.e. its year component is SHRT_MAX), the function returns NULL.
/// @param self the datetime object to start the search from. The date components of the datetime object should be set to the 
///        desired starting date, and the time components can be set to any value (they will not affect the weekday calculation).
///        If the datetime object is not initialised (i.e. its year component is SHRT_MAX), the function will return NULL.
/// @param weekday the weekday to search for (Sunday, Monday, ..., Saturday).
/// @return a pointer to the next datetime object with the specified weekday, or NULL if no such datetime exists or if the input 
///         datetime is not initialised. The caller is responsible for freeing the returned datetime object when it is no longer 
///         needed.
dttm_t *dttm_next_weekday(const dttm_t *self, weekday_t weekday);

#endif //_DTTM_H

