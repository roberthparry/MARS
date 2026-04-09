// test_datetime.c — full test suite for dttm_t using the new test harness

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

#include "datetime.h"

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"


/* ------------------------------------------------------------------------- */
/* TEST FUNCTIONS                                                             */
/* ------------------------------------------------------------------------- */

void test_dttm_alloc(void) {
    dttm_t *dt = dttm_alloc();
    ASSERT_NOT_NULL(dt);
    dttm_dealloc(dt);
}

void test_dttm_init_ymd(void) {
    dttm_t *dt = dttm_init_ymd(dttm_alloc(), 2024, 6, 15);
    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(dttm_year(dt), 2024);
    ASSERT_EQ_INT(dttm_month(dt), DT_June);
    ASSERT_EQ_INT(dttm_day(dt), 15);
    dttm_dealloc(dt);
}

void test_dttm_init_ymdt(void) {
    dttm_t *dt = dttm_init_ymdt(dttm_alloc(),
                                                       2024, 6, 15,
                                                       12, 30, 45.5);
    ASSERT_EQ_INT(dttm_year(dt), 2024);
    ASSERT_EQ_INT(dttm_month(dt), DT_June);
    ASSERT_EQ_INT(dttm_day(dt), 15);
    ASSERT_EQ_INT(dttm_hour(dt), 12);
    ASSERT_EQ_INT(dttm_minute(dt), 30);
    ASSERT_EQ_DOUBLE(dttm_second(dt), 45.5, 1e-9);
    dttm_dealloc(dt);
}

void test_dttm_init_copy(void) {
    dttm_t *src = dttm_init_ymdt(dttm_alloc(),
                                                        2023, 12, 31,
                                                        23, 59, 59.9);
    dttm_t *dst = dttm_init_copy(dttm_alloc(), src);

    ASSERT_TRUE(dttm_year(src)   == dttm_year(dst));
    ASSERT_TRUE(dttm_month(src)  == dttm_month(dst));
    ASSERT_TRUE(dttm_day(src)    == dttm_day(dst));
    ASSERT_TRUE(dttm_hour(src)   == dttm_hour(dst));
    ASSERT_TRUE(dttm_minute(src) == dttm_minute(dst));
    ASSERT_TRUE(fabs(dttm_second(src) - dttm_second(dst)) < 1e-9);

    dttm_dealloc(src);
    dttm_dealloc(dst);
}

void test_dttm_init_jdn(void) {
    long jdn = 2460123;
    dttm_t *dt = dttm_init_jdn(dttm_alloc(), jdn);
    ASSERT_EQ_LONG(dttm_jdn(dt), jdn);
    dttm_dealloc(dt);
}

void test_dttm_init_jd(void) {
    double jd = 2461077.369734;
    dttm_t *dt = dttm_init_jd(dttm_alloc(), jd);
    ASSERT_EQ_DOUBLE(dttm_jd(dt), jd, 1e-9);
    dttm_dealloc(dt);
}

void test_dttm_jdn_and_getJulianDay(void) {
    dttm_t *dt = dttm_init_ymdt(dttm_alloc(),
                                                       2000, 1, 1,
                                                       18, 0, 0.0);

    ASSERT_EQ_LONG(dttm_jdn(dt), 2451545);
    ASSERT_EQ_DOUBLE(dttm_jd(dt), 2451545.25, 1e-9);

    dttm_dealloc(dt);
}

void test_dttm_year_initialized(void) {
    dttm_t *dt = dttm_init_ymd(dttm_alloc(), 2022, 5, 10);
    ASSERT_EQ_INT(dttm_year(dt), 2022);
    dttm_dealloc(dt);
}

void test_dttm_init_now(void) {
    dttm_t *dt = dttm_init_now(dttm_alloc());

    ASSERT_TRUE(dttm_year(dt) > 1900 && dttm_year(dt) < 3000);
    ASSERT_TRUE(dttm_month(dt) >= 1 && dttm_month(dt) <= 12);
    ASSERT_TRUE(dttm_day(dt) >= 1 && dttm_day(dt) <= 31);
    ASSERT_TRUE(dttm_hour(dt) >= 0 && dttm_hour(dt) <= 23);
    ASSERT_TRUE(dttm_minute(dt) >= 0 && dttm_minute(dt) <= 59);
    ASSERT_TRUE(dttm_second(dt) >= 0.0 && dttm_second(dt) < 61.0);

    dttm_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* GMT CONVERSION TESTS                                                      */
/* ------------------------------------------------------------------------- */

void test_dttm_to_gmt_basic(void) {
    dttm_t *dt = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    dttm_t *result = dttm_to_gmt(dt);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result == dt);

    ASSERT_TRUE(dttm_year(dt) > 0);
    ASSERT_TRUE(dttm_month(dt) >= 1 && dttm_month(dt) <= 12);
    ASSERT_TRUE(dttm_day(dt) >= 1 && dttm_day(dt) <= 31);
    ASSERT_TRUE(dttm_hour(dt) >= 0 && dttm_hour(dt) <= 23);
    ASSERT_TRUE(dttm_minute(dt) >= 0 && dttm_minute(dt) <= 59);
    ASSERT_TRUE(dttm_second(dt) >= 0.0 && dttm_second(dt) < 61.0);

    dttm_dealloc(dt);
}

void test_dttm_to_gmt_null_pointer(void) {
    ASSERT_NULL(dttm_to_gmt(NULL));
}

void test_dttm_to_gmt_preserves_julian_values(void) {
    dttm_t *dt = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    dttm_jdn(dt);
    dttm_jd(dt);

    dttm_to_gmt(dt);

    ASSERT_TRUE(dttm_jdn(dt) != LONG_MAX);
    ASSERT_TRUE(dttm_jd(dt) != DBL_MAX);

    dttm_dealloc(dt);
}

void test_dttm_to_gmt_multiple_calls(void) {
    dttm_t *dt = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    dttm_t *copy = dttm_init_copy(dttm_alloc(), dt);

    dttm_to_gmt(copy);
    dttm_to_gmt(dt);

    ASSERT_EQ_INT(dttm_year(dt), dttm_year(copy));
    ASSERT_EQ_INT(dttm_month(dt), dttm_month(copy));
    ASSERT_EQ_INT(dttm_day(dt), dttm_day(copy));
    ASSERT_EQ_INT(dttm_minute(dt), dttm_minute(copy));
    ASSERT_EQ_DOUBLE(dttm_second(dt), dttm_second(copy), 1e-6);

    dttm_dealloc(dt);
    dttm_dealloc(copy);
}

void test_dttm_to_gmt_uninitialized(void) {
    dttm_t *dt = dttm_alloc();
    ASSERT_NOT_NULL(dttm_to_gmt(dt));
    dttm_dealloc(dt);
}

void test_dttm_to_gmt_with_julian_values(void) {
    dttm_t *dt = dttm_init_jd(dttm_alloc(), 2460428.0);

    ASSERT_NOT_NULL(dttm_to_gmt(dt));
    ASSERT_TRUE(dttm_year(dt) != SHRT_MAX);

    dttm_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* EASTER SUNDAY TESTS                                                       */
/* ------------------------------------------------------------------------- */

void test_dttm_init_easter_basic(void) {
    dttm_t *dt = dttm_init_easter(dttm_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(dttm_year(dt), 2024);
    ASSERT_EQ_INT(dttm_month(dt), DT_March);
    ASSERT_EQ_INT(dttm_day(dt), 31);
    ASSERT_EQ_INT(dttm_weekday(dt), DT_Sunday);
    ASSERT_EQ_INT(dttm_hour(dt), 0);
    ASSERT_EQ_INT(dttm_minute(dt), 0);
    ASSERT_EQ_DOUBLE(dttm_second(dt), 0.0, 1e-9);

    dttm_dealloc(dt);
}

void test_dttm_init_easter_known_dates(void) {
    struct { int year; month_t month; uint8_t day; } cases[] = {
        {2000, DT_April, 23},
        {2025, DT_April, 20},
        {2026, DT_April, 5},
    };

    for (int i = 0; i < 3; i++) {
        dttm_t *dt = dttm_init_easter(dttm_alloc(), cases[i].year);

        ASSERT_EQ_INT(dttm_year(dt), cases[i].year);
        ASSERT_EQ_INT(dttm_month(dt), cases[i].month);
        ASSERT_EQ_INT(dttm_day(dt), cases[i].day);
        ASSERT_EQ_INT(dttm_weekday(dt), DT_Sunday);

        dttm_dealloc(dt);
    }
}

void test_dttm_init_easter_boundary_years(void) {
    dttm_t *dt = dttm_init_easter(dttm_alloc(), 1);
    ASSERT_NOT_NULL(dt);
    dttm_dealloc(dt);

    dt = dttm_init_easter(dttm_alloc(), 1582);
    ASSERT_NOT_NULL(dt);
    dttm_dealloc(dt);

    dt = dttm_init_easter(dttm_alloc(), 1583);
    ASSERT_NOT_NULL(dt);
    dttm_dealloc(dt);

    dt = dttm_init_easter(dttm_alloc(), 9999);
    ASSERT_NOT_NULL(dt);
    dttm_dealloc(dt);
}

void test_dttm_init_easter_invalid_years(void) {
    dttm_t *dt = dttm_alloc();
    ASSERT_NULL(dttm_init_easter(dt, 0));
    dttm_dealloc(dt);

    dt = dttm_alloc();
    ASSERT_NULL(dttm_init_easter(dt, 10000));
    dttm_dealloc(dt);

    dt = dttm_alloc();
    ASSERT_NULL(dttm_init_easter(dt, -100));
    dttm_dealloc(dt);
}

void test_dttm_init_easter_always_sunday(void) {
    int years[] = {2000, 2010, 2020, 2024, 2025, 2026, 2050, 2100};

    for (int i = 0; i < 8; i++) {
        dttm_t *dt = dttm_init_easter(dttm_alloc(), years[i]);
        ASSERT_EQ_INT(dttm_weekday(dt), DT_Sunday);
        dttm_dealloc(dt);
    }
}

void test_dttm_init_easter_time_fields_zero(void) {
    dttm_t *dt = dttm_init_easter(dttm_alloc(), 2024);

    ASSERT_EQ_INT(dttm_hour(dt), 0);
    ASSERT_EQ_INT(dttm_minute(dt), 0);
    ASSERT_EQ_DOUBLE(dttm_second(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(dttm_jd(dt), 2460400.5, 1e-9);
    ASSERT_EQ_LONG(dttm_jdn(dt), 2460401);

    dttm_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* CHINESE NEW YEAR TESTS                                                    */
/* ------------------------------------------------------------------------- */

void test_dttm_init_chinese_new_year_basic(void) {
    dttm_t *dt = dttm_init_chinese_new_year(dttm_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(dttm_year(dt), 2024);
    ASSERT_EQ_INT(dttm_month(dt), DT_February);
    ASSERT_EQ_INT(dttm_day(dt), 10);
    ASSERT_EQ_INT(dttm_hour(dt), 12);
    ASSERT_EQ_INT(dttm_minute(dt), 0);
    ASSERT_EQ_DOUBLE(dttm_second(dt), 0.0, 1e-9);

    dttm_dealloc(dt);
}

void test_dttm_init_chinese_new_year_known_dates(void) {
    struct { int year; month_t month; uint8_t day; } cases[] = {
        {2020, DT_January, 25},
        {2021, DT_February, 12},
        {2022, DT_February, 1},
        {2023, DT_January, 22},
        {2024, DT_February, 10},
        {2025, DT_January, 29},
    };

    for (int i = 0; i < 6; i++) {
        dttm_t *dt = dttm_init_chinese_new_year(dttm_alloc(), cases[i].year);

        ASSERT_EQ_INT(dttm_year(dt), cases[i].year);
        ASSERT_EQ_INT(dttm_month(dt), cases[i].month);
        ASSERT_EQ_INT(dttm_day(dt), cases[i].day);

        dttm_dealloc(dt);
    }
}

void test_dttm_init_chinese_new_year_invalid_years(void) {
    dttm_t *dt = dttm_alloc();
    ASSERT_NULL(dttm_init_chinese_new_year(dt, 0));
    dttm_dealloc(dt);

    dt = dttm_alloc();
    ASSERT_NULL(dttm_init_chinese_new_year(dt, -50));
    dttm_dealloc(dt);

    dt = dttm_alloc();
    ASSERT_NULL(dttm_init_chinese_new_year(dt, 10000));
    dttm_dealloc(dt);
}

void test_dttm_init_chinese_new_year_time_fields_zero(void) {
    dttm_t *dt = dttm_init_chinese_new_year(dttm_alloc(), 2024);

    ASSERT_EQ_INT(dttm_hour(dt), 12);
    ASSERT_EQ_INT(dttm_minute(dt), 0);
    ASSERT_EQ_DOUBLE(dttm_second(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(dttm_jd(dt), 2460351.0, 1e-9);
    ASSERT_EQ_LONG(dttm_jdn(dt), 2460351);

    dttm_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* TIMEZONE OFFSET TESTS                                                     */
/* ------------------------------------------------------------------------- */

void test_dttm_computeTimeZoneOffset_basic(void) {
    dttm_t *dt = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    double result = dttm_tz_offset(dt);
    ASSERT_TRUE(result == 1.0);

    dttm_dealloc(dt);
}

void test_dttm_computeTimeZoneOffset_null_pointer(void) {
    ASSERT_TRUE(dttm_tz_offset(NULL) == DBL_MAX);
}

void test_dttm_computeTimeZoneOffset_uninitialized(void) {
    dttm_t *dt = dttm_alloc();
    ASSERT_TRUE(dttm_tz_offset(dt) == DBL_MAX);
    dttm_dealloc(dt);
}

/* ------------------------------------------------------------------------- */
/* JULIAN CONSISTENCY, GETTERS, COMPARISONS, DAYS-IN-MONTH                   */
/* ------------------------------------------------------------------------- */

void test_dttm_julian_roundtrip(void) {
    dttm_t *dt = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    double jd = dttm_jd(dt);
    dttm_t *copy = dttm_init_jd(dttm_alloc(), jd);
    dttm_year(copy);  // force initialization

    ASSERT_TRUE(dttm_year(copy)  == dttm_year(dt));
    ASSERT_TRUE(dttm_month(copy) == dttm_month(dt));
    ASSERT_TRUE(dttm_day(copy)   == dttm_day(dt));

    dttm_dealloc(dt);
    dttm_dealloc(copy);
}

void test_dttm_getters(void) {
    dttm_t *dt = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 14, 22, 33.5
    );

    ASSERT_EQ_INT(dttm_year(dt),   2024);
    ASSERT_EQ_INT(dttm_month(dt),  6);
    ASSERT_EQ_INT(dttm_day(dt),    15);
    ASSERT_EQ_INT(dttm_hour(dt),   14);
    ASSERT_EQ_INT(dttm_minute(dt), 22);
    ASSERT_EQ_DOUBLE(dttm_second(dt), 33.5, 1e-9);

    dttm_dealloc(dt);
}

void test_dttm_compare_equal(void) {
    dttm_t *a = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 0, 0.0
    );
    dttm_t *b = dttm_init_copy(dttm_alloc(), a);

    ASSERT_EQ_INT(dttm_compare(a, b), 0);

    dttm_dealloc(a);
    dttm_dealloc(b);
}

void test_dttm_compare_less(void) {
    dttm_t *a = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 11, 0, 0.0
    );
    dttm_t *b = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(dttm_compare(a, b) < 0);

    dttm_dealloc(a);
    dttm_dealloc(b);
}

void test_dttm_compare_greater(void) {
    dttm_t *a = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 13, 0, 0.0
    );
    dttm_t *b = dttm_init_ymdt(
        dttm_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(dttm_compare(a, b) > 0);

    dttm_dealloc(a);
    dttm_dealloc(b);
}

void test_dttm_days_in_month(void) {
    ASSERT_EQ_INT(dttm_days_in_month(2024, DT_February), 29);
    ASSERT_EQ_INT(dttm_days_in_month(2023, DT_February), 28);
    ASSERT_EQ_INT(dttm_days_in_month(2024, DT_April),    30);
    ASSERT_EQ_INT(dttm_days_in_month(2024, DT_January),  31);
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
        dttm_t *dt = dttm_init_chinese_new_year(
            dttm_alloc(),
            cases[i].year
        );

        if (!dt) {
            printf("Year %d is outside the supported range.\n", cases[i].year);
            continue;
        }

        /* Extract components */
        short y = dttm_year(dt);
        month_t m = dttm_month(dt);
        unsigned char d = dttm_day(dt);

        printf("Chinese New Year %d: %d-%02d-%02d\n",
               cases[i].year, y, (int)m, d);

        dttm_dealloc(dt);
    }
}

/* ------------------------------------------------------------------------- */
/* MAIN TEST ENTRY POINT FOR THE HARNESS                                     */
/* ------------------------------------------------------------------------- */

int tests_main(void) {

    /* Basic Julian and date initialisation tests */
    RUN_TEST(test_dttm_init_jd, NULL);
    RUN_TEST(test_dttm_jdn_and_getJulianDay, NULL);
    RUN_TEST(test_dttm_year_initialized, NULL);
    RUN_TEST(test_dttm_init_now, NULL);

    /* GMT conversion tests */
    RUN_TEST(test_dttm_to_gmt_basic, NULL);
    RUN_TEST(test_dttm_to_gmt_null_pointer, NULL);
    RUN_TEST(test_dttm_to_gmt_preserves_julian_values, NULL);
    RUN_TEST(test_dttm_to_gmt_multiple_calls, NULL);
    RUN_TEST(test_dttm_to_gmt_uninitialized, NULL);
    RUN_TEST(test_dttm_to_gmt_with_julian_values, NULL);

    /* Easter Sunday tests */
    RUN_TEST(test_dttm_init_easter_basic, NULL);
    RUN_TEST(test_dttm_init_easter_known_dates, NULL);
    RUN_TEST(test_dttm_init_easter_invalid_years, NULL);
    RUN_TEST(test_dttm_init_easter_always_sunday, NULL);
    RUN_TEST(test_dttm_init_easter_time_fields_zero, NULL);

    /* Basic allocation and initialisation tests */
    RUN_TEST(test_dttm_alloc, NULL);
    RUN_TEST(test_dttm_init_ymd, NULL);
    RUN_TEST(test_dttm_init_ymdt, NULL);
    RUN_TEST(test_dttm_init_copy, NULL);
    RUN_TEST(test_dttm_init_jdn, NULL);

    /* Chinese New Year tests */
    RUN_TEST(test_dttm_init_chinese_new_year_basic, NULL);
    RUN_TEST(test_dttm_init_chinese_new_year_known_dates, NULL);
    RUN_TEST(test_dttm_init_chinese_new_year_invalid_years, NULL);
    RUN_TEST(test_dttm_init_chinese_new_year_time_fields_zero, NULL);

    /* Timezone offset tests */
    RUN_TEST(test_dttm_computeTimeZoneOffset_basic, NULL);
    RUN_TEST(test_dttm_computeTimeZoneOffset_null_pointer, NULL);
    RUN_TEST(test_dttm_computeTimeZoneOffset_uninitialized, NULL);

    /* Julian consistency, getters, comparisons, days-in-month */
    RUN_TEST(test_dttm_julian_roundtrip, NULL);
    RUN_TEST(test_dttm_getters, NULL);
    RUN_TEST(test_dttm_compare_equal, NULL);
    RUN_TEST(test_dttm_compare_less, NULL);
    RUN_TEST(test_dttm_compare_greater, NULL);
    RUN_TEST(test_dttm_days_in_month, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    RUN_TEST(test_readme_examples, NULL);

    return 0;
}
