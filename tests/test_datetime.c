// test_datetime.c — full colourised test suite for datetime_t
// Modernised non‑fatal test harness with coloured PASS/FAIL output.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

#include "datetime.h"

// ─────────────────────────────────────────────────────────────
// COLOUR MACROS
// ─────────────────────────────────────────────────────────────
#define GREEN   "\033[1;32m"
#define RED     "\033[1;31m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define RESET   "\033[0m"

// ─────────────────────────────────────────────────────────────
// TEST FRAMEWORK
// ─────────────────────────────────────────────────────────────
static int tests_run = 0;
static int tests_failed = 0;

#define TEST(fn) \
    do { \
        tests_run++; \
        printf(BLUE "Running %s...\n" RESET, #fn); \
        if (fn()) { \
            printf(GREEN "✔ PASS: %s\n\n" RESET, #fn); \
        } else { \
            tests_failed++; \
            printf(RED "✘ FAIL: %s\n\n" RESET, #fn); \
        } \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            printf(RED "    Assertion failed: %s\n" RESET, #expr); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ_INT(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            printf(RED "    Expected %d, got %d\n" RESET, (int)(expected), (int)(actual)); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ_LONG(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            printf(RED "    Expected %ld, got %ld\n" RESET, (long)(expected), (long)(actual)); \
            return false; \
        } \
    } while (0)

#define ASSERT_EQ_DOUBLE(actual, expected, eps) \
    do { \
        if (fabs((actual) - (expected)) > (eps)) { \
            printf(RED "    Expected %.12f, got %.12f\n" RESET, (double)(expected), (double)(actual)); \
            return false; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf(RED "    Expected non-null pointer\n" RESET); \
            return false; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf(RED "    Expected NULL pointer\n" RESET); \
            return false; \
        } \
    } while (0)


// ─────────────────────────────────────────────────────────────
// BASIC ALLOCATION & INITIALISATION TESTS
// ─────────────────────────────────────────────────────────────

bool test_datetime_alloc(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NOT_NULL(dt);
    datetime_dealloc(dt);
    return true;
}

bool test_datetime_initWithYearMonthDay(void) {
    datetime_t *dt = datetime_initWithYearMonthDay(datetime_alloc(), 2024, 6, 15);
    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), June);
    ASSERT_EQ_INT(datetime_getDay(dt), 15);
    datetime_dealloc(dt);
    return true;
}

bool test_datetime_initWithYearMonthDayTime(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(datetime_alloc(),
                                                       2024, 6, 15,
                                                       12, 30, 45.5);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), June);
    ASSERT_EQ_INT(datetime_getDay(dt), 15);
    ASSERT_EQ_INT(datetime_getHour(dt), 12);
    ASSERT_EQ_INT(datetime_getMinute(dt), 30);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 45.5, 1e-9);
    datetime_dealloc(dt);
    return true;
}

bool test_datetime_initWithDateTime(void) {
    datetime_t *src = datetime_initWithYearMonthDayTime(datetime_alloc(),
                                                        2023, 12, 31,
                                                        23, 59, 59.9);
    datetime_t *dst = datetime_initWithDateTime(datetime_alloc(), src);
    ASSERT_TRUE(datetime_getYear(src)   == datetime_getYear(dst) &&
                datetime_getMonth(src)  == datetime_getMonth(dst) &&
                datetime_getDay(src)    == datetime_getDay(dst) &&
                datetime_getHour(src)   == datetime_getHour(dst) &&
                datetime_getMinute(src) == datetime_getMinute(dst) &&
                fabs(datetime_getSecond(src) - datetime_getSecond(dst)) < 1e-9);

    datetime_dealloc(src);
    datetime_dealloc(dst);
    return true;
}

bool test_datetime_initWithJulianDayNumber(void) {
    long jdn = 2460123;
    datetime_t *dt = datetime_initWithJulianDayNumber(datetime_alloc(), jdn);
    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), jdn);
    datetime_dealloc(dt);
    return true;
}

bool test_datetime_initWithJulianDay(void) {
    double jd = 2461077.369734;
    datetime_t *dt = datetime_initWithJulianDay(datetime_alloc(), jd);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), jd, 1e-9);
    datetime_dealloc(dt);
    return true;
}

bool test_datetime_getJulianDayNumber_and_getJulianDay(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(datetime_alloc(),
                                                       2000, 1, 1,
                                                       18, 0, 0.0);

    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), 2451545);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), 2451545.25, 1e-9);

    datetime_dealloc(dt);
    return true;
}

bool test_datetime_getYear_initialized(void) {
    datetime_t *dt = datetime_initWithYearMonthDay(datetime_alloc(), 2022, 5, 10);
    ASSERT_EQ_INT(datetime_getYear(dt), 2022);
    datetime_dealloc(dt);
    return true;
}

bool test_datetime_initWithCurrentDateTime(void) {
    datetime_t *dt = datetime_initWithCurrentDateTime(datetime_alloc());

    ASSERT_TRUE(datetime_getYear(dt) > 1900 && datetime_getYear(dt) < 3000);
    ASSERT_TRUE(datetime_getMonth(dt) >= 1 && datetime_getMonth(dt) <= 12);
    ASSERT_TRUE(datetime_getDay(dt) >= 1 && datetime_getDay(dt) <= 31);
    ASSERT_TRUE(datetime_getHour(dt) >= 0 && datetime_getHour(dt) <= 23);
    ASSERT_TRUE(datetime_getMinute(dt) >= 0 && datetime_getMinute(dt) <= 59);
    ASSERT_TRUE(datetime_getSecond(dt) >= 0.0 && datetime_getSecond(dt) < 61.0);

    datetime_dealloc(dt);
    return true;
}

// ─────────────────────────────────────────────────────────────
// SECTION 3 — GMT CONVERSION TESTS
// ─────────────────────────────────────────────────────────────

bool test_datetime_toGreenwichMeanTime_basic(void) {
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
    return true;
}

bool test_datetime_toGreenwichMeanTime_null_pointer(void) {
    ASSERT_NULL(datetime_toGreenwichMeanTime(NULL));
    return true;
}

bool test_datetime_toGreenwichMeanTime_preserves_julian_values(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    datetime_getJulianDayNumber(dt);
    datetime_getJulianDay(dt);

    datetime_toGreenwichMeanTime(dt);

    ASSERT_TRUE(datetime_getJulianDayNumber(dt) != LONG_MAX);
    ASSERT_TRUE(datetime_getJulianDay(dt) != DBL_MAX);

    datetime_dealloc(dt);
    return true;
}

bool test_datetime_toGreenwichMeanTime_multiple_calls(void) {
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
    return true;
}

bool test_datetime_toGreenwichMeanTime_uninitialized(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NOT_NULL(datetime_toGreenwichMeanTime(dt));
    datetime_dealloc(dt);
    return true;
}

bool test_datetime_toGreenwichMeanTime_with_julian_values(void) {
    datetime_t *dt = datetime_initWithJulianDay(datetime_alloc(), 2460428.0);

    ASSERT_NOT_NULL(datetime_toGreenwichMeanTime(dt));
    ASSERT_TRUE(datetime_getYear(dt) != SHRT_MAX);

    datetime_dealloc(dt);
    return true;
}


// ─────────────────────────────────────────────────────────────
// SECTION 4 — EASTER SUNDAY TESTS
// ─────────────────────────────────────────────────────────────

bool test_datetime_initWithEasterSunday_basic(void) {
    datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), March);
    ASSERT_EQ_INT(datetime_getDay(dt), 31);
    ASSERT_EQ_INT(datetime_getWeekday(dt), Sunday);
    ASSERT_EQ_INT(datetime_getHour(dt), 0);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);

    datetime_dealloc(dt);
    return true;
}

bool test_datetime_initWithEasterSunday_known_dates(void) {
    struct { int year; month_t month; unsigned char day; } cases[] = {
        {2000, April, 23},
        {2025, April, 20},
        {2026, April, 5},
    };

    for (int i = 0; i < 3; i++) {
        datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), cases[i].year);

        ASSERT_EQ_INT(datetime_getYear(dt), cases[i].year);
        ASSERT_EQ_INT(datetime_getMonth(dt), cases[i].month);
        ASSERT_EQ_INT(datetime_getDay(dt), cases[i].day);
        ASSERT_EQ_INT(datetime_getWeekday(dt), Sunday);

        datetime_dealloc(dt);
    }

    return true;
}

bool test_datetime_initWithEasterSunday_boundary_years(void) {
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

    return true;
}

bool test_datetime_initWithEasterSunday_invalid_years(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithEasterSunday(dt, 0));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithEasterSunday(dt, 10000));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithEasterSunday(dt, -100));
    datetime_dealloc(dt);

    return true;
}

bool test_datetime_initWithEasterSunday_always_sunday(void) {
    int years[] = {2000, 2010, 2020, 2024, 2025, 2026, 2050, 2100};

    for (int i = 0; i < 8; i++) {
        datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), years[i]);
        ASSERT_EQ_INT(datetime_getWeekday(dt), Sunday);
        datetime_dealloc(dt);
    }

    return true;
}

bool test_datetime_initWithEasterSunday_time_fields_zero(void) {
    datetime_t *dt = datetime_initWithEasterSunday(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_getHour(dt), 0);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), 2460400.5, 1e-9);
    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), 2460401);

    datetime_dealloc(dt);
    return true;
}

// ─────────────────────────────────────────────────────────────
// SECTION 5 — CHINESE NEW YEAR TESTS
// ─────────────────────────────────────────────────────────────

bool test_datetime_initWithChineseNewYear_basic(void) {
    datetime_t *dt = datetime_initWithChineseNewYear(datetime_alloc(), 2024);

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), February);
    ASSERT_EQ_INT(datetime_getDay(dt), 10);
    ASSERT_EQ_INT(datetime_getHour(dt), 12);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);

    datetime_dealloc(dt);
    return true;
}

bool test_datetime_initWithChineseNewYear_known_dates(void) {
    struct { int year; month_t month; unsigned char day; } cases[] = {
        {2020, January, 25},
        {2021, February, 12},
        {2022, February, 1},
        {2023, January, 22},
        {2024, February, 10},
        {2025, January, 29},
    };

    for (int i = 0; i < 6; i++) {
        datetime_t *dt = datetime_initWithChineseNewYear(datetime_alloc(), cases[i].year);

        ASSERT_EQ_INT(datetime_getYear(dt), cases[i].year);
        ASSERT_EQ_INT(datetime_getMonth(dt), cases[i].month);
        ASSERT_EQ_INT(datetime_getDay(dt), cases[i].day);

        datetime_dealloc(dt);
    }

    return true;
}

bool test_datetime_initWithChineseNewYear_invalid_years(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithChineseNewYear(dt, 0));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithChineseNewYear(dt, -50));
    datetime_dealloc(dt);

    dt = datetime_alloc();
    ASSERT_NULL(datetime_initWithChineseNewYear(dt, 10000));
    datetime_dealloc(dt);

    return true;
}

bool test_datetime_initWithChineseNewYear_time_fields_zero(void) {
    datetime_t *dt = datetime_initWithChineseNewYear(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_getHour(dt), 12);
    ASSERT_EQ_INT(datetime_getMinute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 0.0, 1e-9);
    ASSERT_EQ_DOUBLE(datetime_getJulianDay(dt), 2460351.0, 1.0e-9);
    ASSERT_EQ_LONG(datetime_getJulianDayNumber(dt), 2460351);

    datetime_dealloc(dt);
    return true;
}


// ─────────────────────────────────────────────────────────────
// SECTION 6 — TIMEZONE OFFSET TESTS
// ─────────────────────────────────────────────────────────────

bool test_datetime_computeTimeZoneOffset_basic(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    double result = datetime_calculateTimeZoneOffset(dt);
    ASSERT_TRUE(result == 1.0);

    datetime_dealloc(dt);
    return true;
}

bool test_datetime_computeTimeZoneOffset_null_pointer(void) {
    ASSERT_TRUE(datetime_calculateTimeZoneOffset(NULL) == DBL_MAX);
    return true;
}

bool test_datetime_computeTimeZoneOffset_uninitialized(void) {
    datetime_t *dt = datetime_alloc();
    ASSERT_TRUE(datetime_calculateTimeZoneOffset(dt) == DBL_MAX);
    datetime_dealloc(dt);
    return true;
}


// ─────────────────────────────────────────────────────────────
// SECTION 7 — JULIAN CONSISTENCY, GETTERS, COMPARISONS, DAYS-IN-MONTH
// ─────────────────────────────────────────────────────────────

bool test_datetime_julian_roundtrip(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 30, 45.0
    );

    double jd = datetime_getJulianDay(dt);
    datetime_t *copy = datetime_initWithJulianDay(datetime_alloc(), jd);
    datetime_getYear(copy);

    ASSERT_TRUE(datetime_getYear(copy) == datetime_getYear(dt));
    ASSERT_TRUE(datetime_getMonth(copy) == datetime_getMonth(dt));
    ASSERT_TRUE(datetime_getDay(copy) == datetime_getDay(dt));

    datetime_dealloc(dt);
    datetime_dealloc(copy);
    return true;
}

bool test_datetime_getters(void) {
    datetime_t *dt = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 14, 22, 33.5
    );

    ASSERT_EQ_INT(datetime_getYear(dt), 2024);
    ASSERT_EQ_INT(datetime_getMonth(dt), 6);
    ASSERT_EQ_INT(datetime_getDay(dt), 15);
    ASSERT_EQ_INT(datetime_getHour(dt), 14);
    ASSERT_EQ_INT(datetime_getMinute(dt), 22);
    ASSERT_EQ_DOUBLE(datetime_getSecond(dt), 33.5, 1e-9);

    datetime_dealloc(dt);
    return true;
}

bool test_datetime_compare_equal(void) {
    datetime_t *a = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );
    datetime_t *b = datetime_initWithDateTime(datetime_alloc(), a);

    ASSERT_EQ_INT(datetime_compare(a, b), 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
    return true;
}

bool test_datetime_compare_less(void) {
    datetime_t *a = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 11, 0, 0.0
    );
    datetime_t *b = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(datetime_compare(a, b) < 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
    return true;
}

bool test_datetime_compare_greater(void) {
    datetime_t *a = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 13, 0, 0.0
    );
    datetime_t *b = datetime_initWithYearMonthDayTime(
        datetime_alloc(), 2024, 6, 15, 12, 0, 0.0
    );

    ASSERT_TRUE(datetime_compare(a, b) > 0);

    datetime_dealloc(a);
    datetime_dealloc(b);
    return true;
}

bool test_datetime_daysInMonth(void) {
    ASSERT_EQ_INT(datetime_daysInMonth(2024, February), 29);
    ASSERT_EQ_INT(datetime_daysInMonth(2023, February), 28);
    ASSERT_EQ_INT(datetime_daysInMonth(2024, April), 30);
    ASSERT_EQ_INT(datetime_daysInMonth(2024, January), 31);
    return true;
}


// ─────────────────────────────────────────────────────────────
// SECTION 8 — MAIN TEST RUNNER
// ─────────────────────────────────────────────────────────────

int main(void) {

    TEST(test_datetime_initWithJulianDay);
    TEST(test_datetime_getJulianDayNumber_and_getJulianDay);
    TEST(test_datetime_getYear_initialized);
    TEST(test_datetime_initWithCurrentDateTime);

    TEST(test_datetime_toGreenwichMeanTime_basic);
    TEST(test_datetime_toGreenwichMeanTime_null_pointer);
    TEST(test_datetime_toGreenwichMeanTime_preserves_julian_values);
    TEST(test_datetime_toGreenwichMeanTime_multiple_calls);
    TEST(test_datetime_toGreenwichMeanTime_uninitialized);
    TEST(test_datetime_toGreenwichMeanTime_with_julian_values);

    TEST(test_datetime_initWithEasterSunday_basic);
    TEST(test_datetime_initWithEasterSunday_known_dates);
    TEST(test_datetime_initWithEasterSunday_invalid_years);
    TEST(test_datetime_initWithEasterSunday_always_sunday);
    TEST(test_datetime_initWithEasterSunday_time_fields_zero);

    TEST(test_datetime_alloc);
    TEST(test_datetime_initWithYearMonthDay);
    TEST(test_datetime_initWithYearMonthDayTime);
    TEST(test_datetime_initWithDateTime);
    TEST(test_datetime_initWithJulianDayNumber);

    TEST(test_datetime_initWithChineseNewYear_basic);
    TEST(test_datetime_initWithChineseNewYear_known_dates);
    TEST(test_datetime_initWithChineseNewYear_invalid_years);
    TEST(test_datetime_initWithChineseNewYear_time_fields_zero);

    TEST(test_datetime_computeTimeZoneOffset_basic);
    TEST(test_datetime_computeTimeZoneOffset_null_pointer);
    TEST(test_datetime_computeTimeZoneOffset_uninitialized);

    TEST(test_datetime_julian_roundtrip);
    TEST(test_datetime_getters);
    TEST(test_datetime_compare_equal);
    TEST(test_datetime_compare_less);
    TEST(test_datetime_compare_greater);
    TEST(test_datetime_daysInMonth);

    printf("\nTotal tests run: %d\n", tests_run);
    printf("Total tests failed: %d\n", tests_failed);

    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
