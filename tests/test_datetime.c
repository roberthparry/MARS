// test_datetime.c — full test suite for datetime_t using the new test harness

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

#include "datetime.h"

/* ------------------------------------------------------------------------- */
/* TEST FUNCTIONS                                                             */
/* ------------------------------------------------------------------------- */

void test_datetime_alloc(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);
}

void test_datetime_initWithYearMonthDay(void) {
    datetime_t *dt = datetime_initWithYearMonthDay(datetime_alloc(), 2024, 6, 15);
    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), DT_June);
    ASSERT_EQ_INT(datetime_getDay(dt), 15);
    datetime_dealloc(dt);
}

void test_datetime_initWithYearMonthDayTime(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(datetime_alloc(),
                                                       2024, 6, 15,
                                                       12, 30, 45.5);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), DT_June);
    ASSERT_EQ_INT(datetime_getDay(dt), 15);
    ASSERT_EQ_INT(datetime_getHour(dt), 12);
    ASSERT_EQ_INT(datetime_getMinute(dt), 30);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 45.5, 1e-9);
    datetime_dealloc(dt);
}

void test_datetime_initWithDateTime(void) {
    datetime_t *src = datetime_initWithYearMonthDayTime(datetime_alloc(),
                                                        2023, 12, 31,
                                                        23, 59, 59.9);
    datetime_t *dst = datetime_initWithDateTime(datetime_alloc(), src);

    ASSERT_TRUE(datetime_getYear(src)   == datetime_getYear(dst));
    ASSERT_TRUE(datetime_getMonth(src)  == datetime_getMonth(dst));
    ASSERT_TRUE(datetime_getDay(src)    == datetime_getDay(dst));
    ASSERT_TRUE(datetime_getHour(src)   == datetime_getHour(dst));
    ASSERT_TRUE(datetime_getMinute(src) == datetime_getMinute(dst));
    ASSERT_TRUE(fabs(datetime_getSecond(src) - datetime_getSecond(dst)) < 1e-9);

    datetime_dealloc(src);
    datetime_dealloc(dst);
}

void test_datetime_initWithJulianDayNumber(void) {
    long jdn = 2460123;
    datetime_t *dt = datetime_initWithJulianDayNumber(datetime_alloc(), jdn);
    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), jdn);
    datetime_dealloc(dt);
}

void test_datetime_initWithJulianDay(void) {
    double jd = 2461077.369734;
    datetime_t *dt = datetime_initWithJulianDay(datetime_alloc(), jd);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), jd, 1e-9);
    datetime_dealloc(dt);
}

void test_datetime_getJulianDayNumber_and_getJulianDay(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(datetime_alloc(),
                                                       2000, 1, 1,
                                                       18, 0, 0.0);

    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), 2451545);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), 2451545.25, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_getYear_initialized(void) {
    datetime_t *dt = datetime_initWithYearMonthDay(datetime_alloc(), 2022, 5, 10);
    ASSERT_EQ_INT(datetime_getYear(dt), 2022);
    datetime_dealloc(dt);
}

void test_datetime_initWithCurrentDateTime(void) {
    datetime_t *dt = datetime_initWithCurrentDateTime(datetime_alloc());

    ASSERT_TRUE(datetime_getYear(dt) > 1900 && datetime_getYear(dt) < 3000);
    ASSERT_TRUE(datetime_getMonth(dt) >= 1 && datetime_getMonth(dt) <= 12);
    ASSERT_TRUE(datetime_getDay(dt) >= 1 && datetime_getDay(dt) <= 31);
    ASSERT_TRUE(datetime_getHour(dt) >= 0 && datetime_getHour(dt) <= 23);
    ASSERT_TRUE(datetime_getMinute(dt) >= 0 && datetime_getMinute(dt) <= 59);
    ASSERT_TRUE(datetime_getSecond(dt) >= 0.0 && datetime_getSecond(dt) < 61.0);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* GMT CONVERSION TESTS                                                      */
/* ------------------------------------------------------------------------- */

void test_datetime_toGreenwichMeanTime_basic(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    datetime_t *result = datetime_toGreenwichMeanTime(dt);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result == dt);

    ASSERT_TRUE(datetime_getYear(dt) > 0);
    ASSERT_TRUE(datetime_getMonth(dt) >= 1 && datetime_getMonth(dt) <= 12);
    ASSERT_TRUE(datetime_getDay(dt) >= 1 && datetime_getDay(dt) <= 31);
    ASSERT_TRUE(datetime_getHour(dt) >= 0 && datetime_getHour(dt) <= 23);
    ASSERT_TRUE(datetime_getMinute(dt) >= 0 && datetime_getMinute(dt) <= 59);
    ASSERT_TRUE(datetime_getSecond(dt) >= 0.0 && datetime_getSecond(dt) < 61.0);

    datetime_dealloc(dt);
}

void test_datetime_toGreenwichMeanTime_null_pointer(void) {
    ASSERT_NULL(datetime_toGreenwichMeanTime(NULL));
}

void test_datetime_toGreenwichMeanTime_preserves_julian_values(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    datetime_getJulianDayNumber(dt);
    datetime_getJulianDay(dt);

    datetime_toGreenwichMeanTime(dt);

    ASSERT_TRUE(datetime_getJulianDayNumber(dt) != LONG_MAX);
    ASSERT_TRUE(datetime_getJulianDay(dt) != DBL_MAX);

    datetime_dealloc(dt);
}

void test_datetime_toGreenwichMeanTime_multiple_calls(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    datetime_t *copy = datetime_initWithDateTime(datetime_alloc(), dt);

    datetime_toGreenwichMeanTime(copy);
    datetime_toGreenwichMeanTime(dt);

    ASSERT_EQ_INT(datetime_getYear(dt), datetime_getYear(copy));
    ASSERT_EQ_INT(datetime_getMonth(dt), datetime_getMonth(copy));
    ASSERT_EQ_INT(datetime_getDay(dt), datetime_getDay(copy));
    ASSERT_EQ_INT(datetime_getMinute(dt), datetime_getMinute(copy));
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), datetime_getSecond(copy), 1e-6);

    datetime_dealloc(dt);
    datetime_dealloc(copy);
}

void test_datetime_toGreenwichMeanTime_uninitialized(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NOT_NULL(datetime_toGreenwichMeanTime(dt));
    datetime_dealloc(dt);
}

void test_datetime_toGreenwichMeanTime_with_julian_values(void) {
    datetime_t *dt = datetime_initWithJulianDay(datetime_alloc(), 2460428.0);

    ASSERT_NOT_NULL(datetime_toGreenwichMeanTime(dt));
    ASSERT_TRUE(datetime_getYear(dt) != SHRT_MAX);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* EASTER SUNDAY TESTS                                                       */
/* ------------------------------------------------------------------------- */

void test_datetime_initWithEasterSunday_basic(void) {
    datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), DT_March);
    ASSERT_EQ_INT(datetime_getDay(dt), 31);
    ASSERT_EQ_INT(datetime_getWeekday(dt), DT_Sunday);
    ASSERT_EQ_INT(datetime_getHour(dt), 0);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_initWithEasterSunday_known_dates(void) {
    struct { int year; month_t month; unsigned char day; } cases[] = {
        {2000, DT_April, 23},
        {2025, DT_April, 20},
        {2026, DT_April, 5},
    };

    for (int i = 0; i < 3; i++) {
        datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), cases[i].year);

        ASSERT_EQ_INT(datetime_getYear(dt), cases[i].year);
        ASSERT_EQ_INT(datetime_getMonth(dt), cases[i].month);
        ASSERT_EQ_INT(datetime_getDay(dt), cases[i].day);
        ASSERT_EQ_INT(datetime_getWeekday(dt), DT_Sunday);

        datetime_dealloc(dt);
    }
}

void test_datetime_initWithEasterSunday_boundary_years(void) {
    datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), 1);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);

    dt = datetime_initWithEasterSunday(datetime_alloc(), 1582);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);

    dt = datetime_initWithEasterSunday(datetime_alloc(), 1583);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);

    dt = datetime_initWithEasterSunday(datetime_alloc(), 9999);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);
}

void test_datetime_initWithEasterSunday_invalid_years(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithEasterSunday(dt, 0));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithEasterSunday(dt, 10000));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithEasterSunday(dt, -100));
    datetime_dealloc(dt);
}

void test_datetime_initWithEasterSunday_always_sunday(void) {
    int years[] = {2000, 2010, 2020, 2024, 2025, 2026, 2050, 2100};

    for (int i = 0; i < 8; i++) {
        datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), years[i]);
        ASSERT_EQ_INT(datetime_getWeekday(dt), DT_Sunday);
        datetime_dealloc(dt);
    }
}

void test_datetime_initWithEasterSunday_time_fields_zero(void) {
    datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_getHour(dt), 0);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), 2460400.5, 1e-9);
    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), 2460401);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* CHINESE NEW YEAR TESTS                                                    */
/* ------------------------------------------------------------------------- */

void test_datetime_initWithChineseNewYear_basic(void) {
    datetime_t *dt = datetime_initWithChineseNewYear(datetime_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), DT_February);
    ASSERT_EQ_INT(datetime_getDay(dt), 10);
    ASSERT_EQ_INT(datetime_getHour(dt), 12);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_initWithChineseNewYear_known_dates(void) {
    struct { int year; month_t month; unsigned char day; } cases[] = {
        {2020, DT_January, 25},
        {2021, DT_February, 12},
        {2022, DT_February, 1},
        {2023, DT_January, 22},
        {2024, DT_February, 10},
        {2025, DT_January, 29},
    };

    for (int i = 0; i < 6; i++) {
        datetime_t *dt = datetime_initWithChineseNewYear(datetime_alloc(), cases[i].year);

        ASSERT_EQ_INT(datetime_getYear(dt), cases[i].year);
        ASSERT_EQ_INT(datetime_getMonth(dt), cases[i].month);
        ASSERT_EQ_INT(datetime_getDay(dt), cases[i].day);

        datetime_dealloc(dt);
    }
}

void test_datetime_initWithChineseNewYear_invalid_years(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithChineseNewYear(dt, 0));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithChineseNewYear(dt, -50));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithChineseNewYear(dt, 10000));
    datetime_dealloc(dt);
}

void test_datetime_initWithChineseNewYear_time_fields_zero(void) {
    datetime_t *dt = datetime_initWithChineseNewYear(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_getHour(dt), 12);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), 2460351.0, 1e-9);
    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), 2460351);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* TIMEZONE OFFSET TESTS                                                     */
/* ------------------------------------------------------------------------- */

void test_datetime_computeTimeZoneOffset_basic(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    double result = datetime_calculateTimeZoneOffset(dt);
    ASSERT_TRUE(result == 1.0);

    datetime_dealloc(dt);
}

void test_datetime_computeTimeZoneOffset_null_pointer(void) {
    ASSERT_TRUE(datetime_calculateTimeZoneOffset(NULL) == DBL_MAX);
}

void test_datetime_computeTimeZoneOffset_uninitialized(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_TRUE(datetime_calculateTimeZoneOffset(dt) == DBL_MAX);
    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* JULIAN CONSISTENCY, GETTERS, COMPARISONS, DAYS-IN-MONTH                   */
/* ------------------------------------------------------------------------- */

void test_datetime_julian_roundtrip(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    double jd = datetime_getJulianDay(dt);
    datetime_t *copy = datetime_initWithJulianDay(datetime_alloc(), jd);
    datetime_getYear(copy);  // force initialization

    ASSERT_TRUE(datetime_getYear(copy)  == datetime_getYear(dt));
    ASSERT_TRUE(datetime_getMonth(copy) == datetime_getMonth(dt));
    ASSERT_TRUE(datetime_getDay(copy)   == datetime_getDay(dt));

    datetime_dealloc(dt);
    datetime_dealloc(copy);
}

void test_datetime_getters(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 14, 22, 33.5
    );

    ASSERT_EQ_INT(datetime_getYear(dt),   2024);
    ASSERT_EQ_INT(datetime_getMonth(dt),  6);
    ASSERT_EQ_INT(datetime_getDay(dt),    15);
    ASSERT_EQ_INT(datetime_getHour(dt),   14);
    ASSERT_EQ_INT(datetime_getMinute(dt), 22);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 33.5, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_compare_equal(void) {
    datetime_t *a = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );
    datetime_t *b = datetime_initWithDateTime(datetime_alloc(), a);

    ASSERT_EQ_INT(datetime_compare(a, b), 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
}

void test_datetime_compare_less(void) {
    datetime_t *a = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 11, 0, 0.0
    );
    datetime_t *b = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(datetime_compare(a, b) < 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
}

void test_datetime_compare_greater(void) {
    datetime_t *a = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 13, 0, 0.0
    );
    datetime_t *b = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(datetime_compare(a, b) > 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
}

void test_datetime_daysInMonth(void) {
    ASSERT_EQ_INT(datetime_daysInMonth(2024, DT_February), 29);
    ASSERT_EQ_INT(datetime_daysInMonth(2023, DT_February), 28);
    ASSERT_EQ_INT(datetime_daysInMonth(2024, DT_April),    30);
    ASSERT_EQ_INT(datetime_daysInMonth(2024, DT_January),  31);
}

/* ------------------------------------------------------------------------- */
/* MAIN TEST ENTRY POINT FOR THE HARNESS                                     */
/* ------------------------------------------------------------------------- */

int tests_main(void) {

    /* Basic Julian and date initialisation tests */
    RUN_TEST(test_datetime_initWithJulianDay, NULL);
    RUN_TEST(test_datetime_getJulianDayNumber_and_getJulianDay, NULL);
    RUN_TEST(test_datetime_getYear_initialized, NULL);
    RUN_TEST(test_datetime_initWithCurrentDateTime, NULL);

    /* GMT conversion tests */
    RUN_TEST(test_datetime_toGreenwichMeanTime_basic, NULL);
    RUN_TEST(test_datetime_toGreenwichMeanTime_null_pointer, NULL);
    RUN_TEST(test_datetime_toGreenwichMeanTime_preserves_julian_values, NULL);
    RUN_TEST(test_datetime_toGreenwichMeanTime_multiple_calls, NULL);
    RUN_TEST(test_datetime_toGreenwichMeanTime_uninitialized, NULL);
    RUN_TEST(test_datetime_toGreenwichMeanTime_with_julian_values, NULL);

    /* Easter Sunday tests */
    RUN_TEST(test_datetime_initWithEasterSunday_basic, NULL);
    RUN_TEST(test_datetime_initWithEasterSunday_known_dates, NULL);
    RUN_TEST(test_datetime_initWithEasterSunday_invalid_years, NULL);
    RUN_TEST(test_datetime_initWithEasterSunday_always_sunday, NULL);
    RUN_TEST(test_datetime_initWithEasterSunday_time_fields_zero, NULL);

    /* Basic allocation and initialisation tests */
    RUN_TEST(test_datetime_alloc, NULL);
    RUN_TEST(test_datetime_initWithYearMonthDay, NULL);
    RUN_TEST(test_datetime_initWithYearMonthDayTime, NULL);
    RUN_TEST(test_datetime_initWithDateTime, NULL);
    RUN_TEST(test_datetime_initWithJulianDayNumber, NULL);

    /* Chinese New Year tests */
    RUN_TEST(test_datetime_initWithChineseNewYear_basic, NULL);
    RUN_TEST(test_datetime_initWithChineseNewYear_known_dates, NULL);
    RUN_TEST(test_datetime_initWithChineseNewYear_invalid_years, NULL);
    RUN_TEST(test_datetime_initWithChineseNewYear_time_fields_zero, NULL);

    /* Timezone offset tests */
    RUN_TEST(test_datetime_computeTimeZoneOffset_basic, NULL);
    RUN_TEST(test_datetime_computeTimeZoneOffset_null_pointer, NULL);
    RUN_TEST(test_datetime_computeTimeZoneOffset_uninitialized, NULL);

    /* Julian consistency, getters, comparisons, days-in-month */
    RUN_TEST(test_datetime_julian_roundtrip, NULL);
    RUN_TEST(test_datetime_getters, NULL);
    RUN_TEST(test_datetime_compare_equal, NULL);
    RUN_TEST(test_datetime_compare_less, NULL);
    RUN_TEST(test_datetime_compare_greater, NULL);
    RUN_TEST(test_datetime_daysInMonth, NULL);

    return 0;
}
