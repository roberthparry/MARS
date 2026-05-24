// test_datetime.c — full test suite for datetime_t using the new test harness

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

#include "datetime.h"

#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static int datetime_validity_equal(const void *actual, const void *expected, void *ctx);
static void datetime_validity_format(const void *value, char *buf, size_t buf_size, void *ctx);
static bool test_datetime_suite_setup(void);

static const test_validity_contract_t datetime_exact_contract =
    TEST_VALIDITY_CONTRACT("datetime-exact",
                           datetime_validity_equal,
                           datetime_validity_format,
                           NULL);

TEST_SUITE_SETUP(test_datetime_suite_setup);

#define TEST_ASSERT_DATETIME_EQ(actual_ptr, expected_ptr) \
    do { \
        const datetime_t *test_datetime_actual__ = (actual_ptr); \
        const datetime_t *test_datetime_expected__ = (expected_ptr); \
        TEST_ASSERT_VALID_NAMED("datetime-exact", \
                                &test_datetime_actual__, \
                                &test_datetime_expected__); \
    } while (0)

static int datetime_validity_equal(const void *actual, const void *expected, void *ctx)
{
    const datetime_t *const *got = (const datetime_t *const *)actual;
    const datetime_t *const *want = (const datetime_t *const *)expected;

    (void)ctx;
    return datetime_year(*got) == datetime_year(*want)
        && datetime_month(*got) == datetime_month(*want)
        && datetime_day(*got) == datetime_day(*want)
        && datetime_hour(*got) == datetime_hour(*want)
        && datetime_minute(*got) == datetime_minute(*want)
        && fabs(datetime_second(*got) - datetime_second(*want)) < 1e-4;
}

static void datetime_validity_format(const void *value, char *buf, size_t buf_size, void *ctx)
{
    const datetime_t *const *dt = (const datetime_t *const *)value;

    (void)ctx;
    if (!buf || buf_size == 0)
        return;

    snprintf(buf, buf_size,
             "%04d-%02d-%02d %02d:%02d:%09.6f",
             datetime_year(*dt),
             (int)datetime_month(*dt),
             (int)datetime_day(*dt),
             datetime_hour(*dt),
             datetime_minute(*dt),
             datetime_second(*dt));
}

static bool test_datetime_suite_setup(void)
{
    test_register_validity_checker("datetime-exact", &datetime_exact_contract);
    return TEST_REQUIRE_VALIDITY_CHECKER("datetime-exact");
}


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

    TEST_ASSERT_DATETIME_EQ(src, dst);

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

void test_datetime_year_initialised(void) {
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

void test_datetime_to_gmt_uninitialised(void) {
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

void test_dttm_computeTimeZoneOffset_uninitialised(void) {
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
    datetime_year(copy);  // force initialisation

    TEST_ASSERT_DATETIME_EQ(copy, dt);

    datetime_dealloc(dt);
    datetime_dealloc(copy);
}

void test_datetime_getters(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, 6, 15, 14, 22, 33.5
    );

    ASSERT_EQ_INT(datetime_year(dt), 2024);
    ASSERT_EQ_INT(datetime_month(dt), 6);
    ASSERT_EQ_INT(datetime_day(dt), 15);
    ASSERT_EQ_INT(datetime_hour(dt), 14);
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

static void example_chinese_new_years(void) {
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
    TEST_RUN_CASE(test_datetime_init_jd, NULL);
    TEST_RUN_CASE(test_datetime_jdn_and_getJulianDay, NULL);
    TEST_RUN_CASE(test_datetime_year_initialised, NULL);
    TEST_RUN_CASE(test_datetime_init_now, NULL);

    /* GMT conversion tests */
    TEST_RUN_CASE(test_datetime_to_gmt_basic, NULL);
    TEST_RUN_CASE(test_datetime_to_gmt_null_pointer, NULL);
    TEST_RUN_CASE(test_datetime_to_gmt_preserves_julian_values, NULL);
    TEST_RUN_CASE(test_datetime_to_gmt_multiple_calls, NULL);
    TEST_RUN_CASE(test_datetime_to_gmt_uninitialised, NULL);
    TEST_RUN_CASE(test_datetime_to_gmt_with_julian_values, NULL);

    /* Easter Sunday tests */
    TEST_RUN_CASE(test_datetime_init_easter_basic, NULL);
    TEST_RUN_CASE(test_datetime_init_easter_known_dates, NULL);
    TEST_RUN_CASE(test_datetime_init_easter_invalid_years, NULL);
    TEST_RUN_CASE(test_datetime_init_easter_always_sunday, NULL);
    TEST_RUN_CASE(test_datetime_init_easter_time_fields_zero, NULL);

    /* Basic allocation and initialisation tests */
    TEST_RUN_CASE(test_datetime_alloc, NULL);
    TEST_RUN_CASE(test_datetime_init_ymd, NULL);
    TEST_RUN_CASE(test_datetime_init_ymdt, NULL);
    TEST_RUN_CASE(test_datetime_init_copy, NULL);
    TEST_RUN_CASE(test_datetime_init_jdn, NULL);

    /* Chinese New Year tests */
    TEST_RUN_CASE(test_datetime_init_chinese_new_year_basic, NULL);
    TEST_RUN_CASE(test_datetime_init_chinese_new_year_known_dates, NULL);
    TEST_RUN_CASE(test_datetime_init_chinese_new_year_invalid_years, NULL);
    TEST_RUN_CASE(test_datetime_init_chinese_new_year_time_fields_zero, NULL);

    /* Timezone offset tests */
    TEST_RUN_CASE(test_dttm_computeTimeZoneOffset_basic, NULL);
    TEST_RUN_CASE(test_dttm_computeTimeZoneOffset_null_pointer, NULL);
    TEST_RUN_CASE(test_dttm_computeTimeZoneOffset_uninitialised, NULL);

    /* Julian consistency, getters, comparisons, days-in-month */
    TEST_RUN_CASE(test_dttm_julian_roundtrip, NULL);
    TEST_RUN_CASE(test_datetime_getters, NULL);
    TEST_RUN_CASE(test_datetime_compare_equal, NULL);
    TEST_RUN_CASE(test_datetime_compare_less, NULL);
    TEST_RUN_CASE(test_datetime_compare_greater, NULL);
    TEST_RUN_CASE(test_datetime_days_in_month, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_TAGS(example_chinese_new_years, "datetime,readme,output");

    return TESTS_EXIT_CODE();
}
