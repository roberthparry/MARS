// test_datetime.c — full test suite for datetime_t using the new test harness

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

#include "datetime.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"


/* ------------------------------------------------------------------------- */
/* TEST FUNCTIONS                                                             */
/* ------------------------------------------------------------------------- */

void test_datetime_alloc(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);
}

void test_datetime_init_ymd(void) {
    datetime_t *dt = datetime_init_ymd(datetime_alloc(), 2024, 6, 15);
    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_year(dt), 2024);
    ASSERT_EQ_INT(datetime_month(dt), DT_June);
    ASSERT_EQ_INT(datetime_day(dt), 15);
    datetime_dealloc(dt);
}

void test_datetime_init_ymdt(void) {
    datetime_t *dt = datetime_init_ymdt(datetime_alloc(),
                                                       2024, 6, 15,
                                                       12, 30, 45.5);
    ASSERT_EQ_INT(datetime_year(dt), 2024);
    ASSERT_EQ_INT(datetime_month(dt), DT_June);
    ASSERT_EQ_INT(datetime_day(dt), 15);
    ASSERT_EQ_INT(datetime_hour(dt), 12);
    ASSERT_EQ_INT(datetime_minute(dt), 30);
    ASSERT_EQ_DOUBLE(datetime_second(dt), 45.5, 1e-9);
    datetime_dealloc(dt);
}

void test_datetime_init_copy(void) {
    datetime_t *src = datetime_init_ymdt(datetime_alloc(),
                                                        2023, 12, 31,
                                                        23, 59, 59.9);
    datetime_t *dst = datetime_init_copy(datetime_alloc(), src);

    ASSERT_TRUE(datetime_year(src)   == datetime_year(dst));
    ASSERT_TRUE(datetime_month(src)  == datetime_month(dst));
    ASSERT_TRUE(datetime_day(src)    == datetime_day(dst));
    ASSERT_TRUE(datetime_hour(src)   == datetime_hour(dst));
    ASSERT_TRUE(datetime_minute(src) == datetime_minute(dst));
    ASSERT_TRUE(fabs(datetime_second(src) - datetime_second(dst)) < 1e-9);

    datetime_dealloc(src);
    datetime_dealloc(dst);
}

void test_datetime_init_jdn(void) {
    long jdn = 2460123;
    datetime_t *dt = datetime_init_jdn(datetime_alloc(), jdn);
    ASSERT_EQ_LONG(datetime_jdn(dt), jdn);
    datetime_dealloc(dt);
}

void test_datetime_init_jd(void) {
    double jd = 2461077.369734;
    datetime_t *dt = datetime_init_jd(datetime_alloc(), jd);
    ASSERT_EQ_DOUBLE(datetime_jd(dt), jd, 1e-9);
    datetime_dealloc(dt);
}

void test_datetime_jdn_and_getJulianDay(void) {
    datetime_t *dt = datetime_init_ymdt(datetime_alloc(),
                                                       2000, 1, 1,
                                                       18, 0, 0.0);

    ASSERT_EQ_LONG(datetime_jdn(dt), 2451545);
    ASSERT_EQ_DOUBLE(datetime_jd(dt), 2451545.25, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_year_initialized(void) {
    datetime_t *dt = datetime_init_ymd(datetime_alloc(), 2022, 5, 10);
    ASSERT_EQ_INT(datetime_year(dt), 2022);
    datetime_dealloc(dt);
}

void test_datetime_init_now(void) {
    datetime_t *dt = datetime_init_now(datetime_alloc());

    ASSERT_TRUE(datetime_year(dt) > 1900 && datetime_year(dt) < 3000);
    ASSERT_TRUE(datetime_month(dt) >= 1 && datetime_month(dt) <= 12);
    ASSERT_TRUE(datetime_day(dt) >= 1 && datetime_day(dt) <= 31);
    ASSERT_TRUE(datetime_hour(dt) >= 0 && datetime_hour(dt) <= 23);
    ASSERT_TRUE(datetime_minute(dt) >= 0 && datetime_minute(dt) <= 59);
    ASSERT_TRUE(datetime_second(dt) >= 0.0 && datetime_second(dt) < 61.0);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* GMT CONVERSION TESTS                                                      */
/* ------------------------------------------------------------------------- */

void test_datetime_to_gmt_basic(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    datetime_t *result = datetime_to_gmt(dt);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result == dt);

    ASSERT_TRUE(datetime_year(dt) > 0);
    ASSERT_TRUE(datetime_month(dt) >= 1 && datetime_month(dt) <= 12);
    ASSERT_TRUE(datetime_day(dt) >= 1 && datetime_day(dt) <= 31);
    ASSERT_TRUE(datetime_hour(dt) >= 0 && datetime_hour(dt) <= 23);
    ASSERT_TRUE(datetime_minute(dt) >= 0 && datetime_minute(dt) <= 59);
    ASSERT_TRUE(datetime_second(dt) >= 0.0 && datetime_second(dt) < 61.0);

    datetime_dealloc(dt);
}

void test_datetime_to_gmt_null_pointer(void) {
    ASSERT_NULL(datetime_to_gmt(NULL));
}

void test_datetime_to_gmt_preserves_julian_values(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    datetime_jdn(dt);
    datetime_jd(dt);

    datetime_to_gmt(dt);

    ASSERT_TRUE(datetime_jdn(dt) != LONG_MAX);
    ASSERT_TRUE(datetime_jd(dt) != DBL_MAX);

    datetime_dealloc(dt);
}

void test_datetime_to_gmt_multiple_calls(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    datetime_t *copy = datetime_init_copy(datetime_alloc(), dt);

    datetime_to_gmt(copy);
    datetime_to_gmt(dt);

    ASSERT_EQ_INT(datetime_year(dt), datetime_year(copy));
    ASSERT_EQ_INT(datetime_month(dt), datetime_month(copy));
    ASSERT_EQ_INT(datetime_day(dt), datetime_day(copy));
    ASSERT_EQ_INT(datetime_minute(dt), datetime_minute(copy));
    ASSERT_EQ_DOUBLE(datetime_second(dt), datetime_second(copy), 1e-6);

    datetime_dealloc(dt);
    datetime_dealloc(copy);
}

void test_datetime_to_gmt_uninitialized(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NOT_NULL(datetime_to_gmt(dt));
    datetime_dealloc(dt);
}

void test_datetime_to_gmt_with_julian_values(void) {
    datetime_t *dt = datetime_init_jd(datetime_alloc(), 2460428.0);

    ASSERT_NOT_NULL(datetime_to_gmt(dt));
    ASSERT_TRUE(datetime_year(dt) != SHRT_MAX);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* EASTER SUNDAY TESTS                                                       */
/* ------------------------------------------------------------------------- */

void test_datetime_init_easter_basic(void) {
    datetime_t *dt = datetime_init_easter(datetime_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_year(dt), 2024);
    ASSERT_EQ_INT(datetime_month(dt), DT_March);
    ASSERT_EQ_INT(datetime_day(dt), 31);
    ASSERT_EQ_INT(datetime_weekday(dt), DT_Sunday);
    ASSERT_EQ_INT(datetime_hour(dt), 0);
    ASSERT_EQ_INT(datetime_minute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_second(dt), 0.0, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_init_easter_known_dates(void) {
    struct { int year; month_t month; uint8_t day; } cases[] = {
        {2000, DT_April, 23},
        {2025, DT_April, 20},
        {2026, DT_April, 5},
    };

    for (int i = 0; i < 3; i++) {
        datetime_t *dt = datetime_init_easter(datetime_alloc(), cases[i].year);

        ASSERT_EQ_INT(datetime_year(dt), cases[i].year);
        ASSERT_EQ_INT(datetime_month(dt), cases[i].month);
        ASSERT_EQ_INT(datetime_day(dt), cases[i].day);
        ASSERT_EQ_INT(datetime_weekday(dt), DT_Sunday);

        datetime_dealloc(dt);
    }
}

void test_datetime_init_easter_boundary_years(void) {
    datetime_t *dt = datetime_init_easter(datetime_alloc(), 1);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);

    dt = datetime_init_easter(datetime_alloc(), 1582);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);

    dt = datetime_init_easter(datetime_alloc(), 1583);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);

    dt = datetime_init_easter(datetime_alloc(), 9999);
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);
}

void test_datetime_init_easter_invalid_years(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NULL(datetime_init_easter(dt, 0));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_init_easter(dt, 10000));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_init_easter(dt, -100));
    datetime_dealloc(dt);
}

void test_datetime_init_easter_always_sunday(void) {
    int years[] = {2000, 2010, 2020, 2024, 2025, 2026, 2050, 2100};

    for (int i = 0; i < 8; i++) {
        datetime_t *dt = datetime_init_easter(datetime_alloc(), years[i]);
        ASSERT_EQ_INT(datetime_weekday(dt), DT_Sunday);
        datetime_dealloc(dt);
    }
}

void test_datetime_init_easter_time_fields_zero(void) {
    datetime_t *dt = datetime_init_easter(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_hour(dt), 0);
    ASSERT_EQ_INT(datetime_minute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_second(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(datetime_jd(dt), 2460400.5, 1e-9);
    ASSERT_EQ_LONG(datetime_jdn(dt), 2460401);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* CHINESE NEW YEAR TESTS                                                    */
/* ------------------------------------------------------------------------- */

void test_datetime_init_chinese_new_year_basic(void) {
    datetime_t *dt = datetime_init_chinese_new_year(datetime_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_year(dt), 2024);
    ASSERT_EQ_INT(datetime_month(dt), DT_February);
    ASSERT_EQ_INT(datetime_day(dt), 10);
    ASSERT_EQ_INT(datetime_hour(dt), 12);
    ASSERT_EQ_INT(datetime_minute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_second(dt), 0.0, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_init_chinese_new_year_known_dates(void) {
    struct { int year; month_t month; uint8_t day; } cases[] = {
        {2020, DT_January, 25},
        {2021, DT_February, 12},
        {2022, DT_February, 1},
        {2023, DT_January, 22},
        {2024, DT_February, 10},
        {2025, DT_January, 29},
    };

    for (int i = 0; i < 6; i++) {
        datetime_t *dt = datetime_init_chinese_new_year(datetime_alloc(), cases[i].year);

        ASSERT_EQ_INT(datetime_year(dt), cases[i].year);
        ASSERT_EQ_INT(datetime_month(dt), cases[i].month);
        ASSERT_EQ_INT(datetime_day(dt), cases[i].day);

        datetime_dealloc(dt);
    }
}

void test_datetime_init_chinese_new_year_invalid_years(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NULL(datetime_init_chinese_new_year(dt, 0));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_init_chinese_new_year(dt, -50));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_init_chinese_new_year(dt, 10000));
    datetime_dealloc(dt);
}

void test_datetime_init_chinese_new_year_time_fields_zero(void) {
    datetime_t *dt = datetime_init_chinese_new_year(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_hour(dt), 12);
    ASSERT_EQ_INT(datetime_minute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_second(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(datetime_jd(dt), 2460351.0, 1e-9);
    ASSERT_EQ_LONG(datetime_jdn(dt), 2460351);

    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* TIMEZONE OFFSET TESTS                                                     */
/* ------------------------------------------------------------------------- */

void test_dttm_computeTimeZoneOffset_basic(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    double result = datetime_tz_offset(dt);
    ASSERT_TRUE(result == 1.0);

    datetime_dealloc(dt);
}

void test_dttm_computeTimeZoneOffset_null_pointer(void) {
    ASSERT_TRUE(datetime_tz_offset(NULL) == DBL_MAX);
}

void test_dttm_computeTimeZoneOffset_uninitialized(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_TRUE(datetime_tz_offset(dt) == DBL_MAX);
    datetime_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* JULIAN CONSISTENCY, GETTERS, COMPARISONS, DAYS-IN-MONTH                   */
/* ------------------------------------------------------------------------- */

void test_dttm_julian_roundtrip(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    double jd = datetime_jd(dt);
    datetime_t *copy = datetime_init_jd(datetime_alloc(), jd);
    datetime_year(copy);  // force initialization

    ASSERT_TRUE(datetime_year(copy)  == datetime_year(dt));
    ASSERT_TRUE(datetime_month(copy) == datetime_month(dt));
    ASSERT_TRUE(datetime_day(copy)   == datetime_day(dt));

    datetime_dealloc(dt);
    datetime_dealloc(copy);
}

void test_datetime_getters(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 14, 22, 33.5
    );

    ASSERT_EQ_INT(datetime_year(dt),   2024);
    ASSERT_EQ_INT(datetime_month(dt),  6);
    ASSERT_EQ_INT(datetime_day(dt),    15);
    ASSERT_EQ_INT(datetime_hour(dt),   14);
    ASSERT_EQ_INT(datetime_minute(dt), 22);
    ASSERT_EQ_DOUBLE(datetime_second(dt), 33.5, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_compare_equal(void) {
    datetime_t *a = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );
    datetime_t *b = datetime_init_copy(datetime_alloc(), a);

    ASSERT_EQ_INT(datetime_compare(a, b), 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
}

void test_datetime_compare_less(void) {
    datetime_t *a = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 11, 0, 0.0
    );
    datetime_t *b = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(datetime_compare(a, b) < 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
}

void test_datetime_compare_greater(void) {
    datetime_t *a = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 13, 0, 0.0
    );
    datetime_t *b = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(datetime_compare(a, b) > 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
}

void test_datetime_days_in_month(void) {
    ASSERT_EQ_INT(datetime_days_in_month(2024, DT_February), 29);
    ASSERT_EQ_INT(datetime_days_in_month(2023, DT_February), 28);
    ASSERT_EQ_INT(datetime_days_in_month(2024, DT_April),    30);
    ASSERT_EQ_INT(datetime_days_in_month(2024, DT_January),  31);
}

void test_readme_examples(void) {
    struct {
        int year;
        month_t month;
        unsigned char day;
    } cases[] = {
        {2020, DT_January, 25},
        {2021, DT_February, 12},
        {2022, DT_February, 1},
        {2023, DT_January, 22},
        {2024, DT_February, 10},
        {2025, DT_January, 29},
    };

    for (int i = 0; i < 6; i++) {
        /* Compute Chinese New Year for the given year */
        datetime_t *dt = datetime_init_chinese_new_year(
            datetime_alloc(),
            cases[i].year
        );

        if (!dt) {
            printf("Year %d is outside the supported range.\n", cases[i].year);
            continue;
        }

        /* Extract components */
        short y = datetime_year(dt);
        month_t m = datetime_month(dt);
        unsigned char d = datetime_day(dt);

        printf("Chinese New Year %d: %d-%02d-%02d\n",
               cases[i].year, y, (int)m, d);

        datetime_dealloc(dt);
    }
}

/* ------------------------------------------------------------------------- */
/* MAIN TEST ENTRY POINT FOR THE HARNESS                                     */
/* ------------------------------------------------------------------------- */

int tests_main(void) {

    /* Basic Julian and date initialisation tests */
    RUN_TEST_CASE(test_datetime_init_jd);
    RUN_TEST_CASE(test_datetime_jdn_and_getJulianDay);
    RUN_TEST_CASE(test_datetime_year_initialized);
    RUN_TEST_CASE(test_datetime_init_now);

    /* GMT conversion tests */
    RUN_TEST_CASE(test_datetime_to_gmt_basic);
    RUN_TEST_CASE(test_datetime_to_gmt_null_pointer);
    RUN_TEST_CASE(test_datetime_to_gmt_preserves_julian_values);
    RUN_TEST_CASE(test_datetime_to_gmt_multiple_calls);
    RUN_TEST_CASE(test_datetime_to_gmt_uninitialized);
    RUN_TEST_CASE(test_datetime_to_gmt_with_julian_values);

    /* Easter Sunday tests */
    RUN_TEST_CASE(test_datetime_init_easter_basic);
    RUN_TEST_CASE(test_datetime_init_easter_known_dates);
    RUN_TEST_CASE(test_datetime_init_easter_invalid_years);
    RUN_TEST_CASE(test_datetime_init_easter_always_sunday);
    RUN_TEST_CASE(test_datetime_init_easter_time_fields_zero);

    /* Basic allocation and initialisation tests */
    RUN_TEST_CASE(test_datetime_alloc);
    RUN_TEST_CASE(test_datetime_init_ymd);
    RUN_TEST_CASE(test_datetime_init_ymdt);
    RUN_TEST_CASE(test_datetime_init_copy);
    RUN_TEST_CASE(test_datetime_init_jdn);

    /* Chinese New Year tests */
    RUN_TEST_CASE(test_datetime_init_chinese_new_year_basic);
    RUN_TEST_CASE(test_datetime_init_chinese_new_year_known_dates);
    RUN_TEST_CASE(test_datetime_init_chinese_new_year_invalid_years);
    RUN_TEST_CASE(test_datetime_init_chinese_new_year_time_fields_zero);

    /* Timezone offset tests */
    RUN_TEST_CASE(test_dttm_computeTimeZoneOffset_basic);
    RUN_TEST_CASE(test_dttm_computeTimeZoneOffset_null_pointer);
    RUN_TEST_CASE(test_dttm_computeTimeZoneOffset_uninitialized);

    /* Julian consistency, getters, comparisons, days-in-month */
    RUN_TEST_CASE(test_dttm_julian_roundtrip);
    RUN_TEST_CASE(test_datetime_getters);
    RUN_TEST_CASE(test_datetime_compare_equal);
    RUN_TEST_CASE(test_datetime_compare_less);
    RUN_TEST_CASE(test_datetime_compare_greater);
    RUN_TEST_CASE(test_datetime_days_in_month);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    RUN_TEST_CASE(test_readme_examples);

    return TESTS_EXIT_CODE();
}
