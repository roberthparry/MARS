// test_datetime.c — full test suite for datetime_t using the new test harness

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

#include "datetime.h"
#include "ustring.h"

#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static int datetime_validity_equal(const void *actual, const void *expected, void *ctx);
static int datetime_validity_format(const void *value, string_t *out, void *ctx);
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

static int datetime_validity_format(const void *value, string_t *out, void *ctx)
{
    const datetime_t *const *dt = (const datetime_t *const *)value;

    (void)ctx;
    if (!out)
        return -1;

    return string_append_format(out,
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

void test_datetime_dealloc_null_is_safe(void) {
    datetime_dealloc(NULL);
    ASSERT_TRUE(true);
}

void test_datetime_from_string_valid_iso_date(void) {
    datetime_t *dt = datetime_from_string("2026-06-25");

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_year(dt), 2026);
    ASSERT_EQ_INT(datetime_month(dt), DT_June);
    ASSERT_EQ_INT(datetime_day(dt), 25);
    ASSERT_EQ_INT(datetime_hour(dt), 0);
    ASSERT_EQ_INT(datetime_minute(dt), 0);
    ASSERT_EQ_DOUBLE(datetime_second(dt), 0.0, 1e-9);

    datetime_dealloc(dt);
}

void test_datetime_from_string_rejects_invalid_iso_date(void) {
    ASSERT_NULL(datetime_from_string("2026-02-30"));
    ASSERT_NULL(datetime_from_string("2026/06/25"));
    ASSERT_NULL(datetime_from_string(NULL));
}

void test_datetime_from_string_accepts_dmy_slash_date(void) {
    datetime_t *dt = datetime_from_string("25/06/2026");

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_year(dt), 2026);
    ASSERT_EQ_INT(datetime_month(dt), DT_June);
    ASSERT_EQ_INT(datetime_day(dt), 25);

    datetime_dealloc(dt);
}

void test_datetime_from_string_ignores_outer_whitespace(void) {
    datetime_t *dt = datetime_from_string("  2026-06-25  ");

    ASSERT_NOT_NULL(dt);
    ASSERT_EQ_INT(datetime_year(dt), 2026);
    ASSERT_EQ_INT(datetime_month(dt), DT_June);
    ASSERT_EQ_INT(datetime_day(dt), 25);

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

void test_datetime_valid_ymd(void) {
    ASSERT_TRUE(datetime_valid_ymd(2024, DT_February, 29));
    ASSERT_FALSE(datetime_valid_ymd(2023, DT_February, 29));
    ASSERT_FALSE(datetime_valid_ymd(2024, DT_April, 31));
    ASSERT_FALSE(datetime_valid_ymd(0, DT_January, 1));
}

void test_datetime_display_names(void) {
    TEST_ASSERT_STR_EQ(datetime_weekday_name(DT_Sunday), "Sunday");
    TEST_ASSERT_STR_EQ(datetime_weekday_name(DT_Saturday), "Saturday");
    TEST_ASSERT_STR_EQ(datetime_weekday_name((weekday_t)0), "Unknown");
    TEST_ASSERT_STR_EQ(datetime_moon_phase_name(DT_NewMoon), "New Moon");
    TEST_ASSERT_STR_EQ(datetime_moon_phase_name(DT_WaningCrescent), "Waning Crescent");
    TEST_ASSERT_STR_EQ(datetime_moon_phase_name((moon_phase_t)99), "Unknown");
}

void test_datetime_solar_position_helpers(void) {
    datetime_t *solstice = datetime_init_ymdt(
        datetime_alloc(), 2024, DT_June, 21, 12, 0, 0.0
    );
    double declination = datetime_solar_declination(solstice);
    double maxAltitude = datetime_solar_max_altitude(solstice, 51.5074);
    double inclination = datetime_solar_inclination(solstice, 51.5074);

    ASSERT_EQ_DOUBLE(declination, 23.45, 0.35);
    ASSERT_EQ_DOUBLE(maxAltitude, 61.94, 0.35);
    ASSERT_EQ_DOUBLE(inclination, 28.06, 0.35);

    datetime_dealloc(solstice);
}

void test_datetime_orthodox_easter_known_date(void) {
    datetime_t *dt = datetime_init_orthodox_easter(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_year(dt), 2024);
    ASSERT_EQ_INT(datetime_month(dt), DT_May);
    ASSERT_EQ_INT(datetime_day(dt), 5);
    ASSERT_EQ_INT(datetime_weekday(dt), DT_Sunday);

    datetime_dealloc(dt);
}

void test_datetime_christmas_known_dates(void) {
    datetime_t *christmas = datetime_init_christmas(datetime_alloc(), 2026);
    datetime_t *orthodox = datetime_init_orthodox_christmas(datetime_alloc(), 2026);

    ASSERT_EQ_INT(datetime_year(christmas), 2026);
    ASSERT_EQ_INT(datetime_month(christmas), DT_December);
    ASSERT_EQ_INT(datetime_day(christmas), 25);

    ASSERT_EQ_INT(datetime_year(orthodox), 2026);
    ASSERT_EQ_INT(datetime_month(orthodox), DT_January);
    ASSERT_EQ_INT(datetime_day(orthodox), 7);

    datetime_dealloc(christmas);
    datetime_dealloc(orthodox);
}

void test_datetime_jewish_new_year_known_date(void) {
    datetime_t *dt = datetime_init_jewish_new_year(datetime_alloc(), 2024);

    ASSERT_EQ_INT(datetime_year(dt), 2024);
    ASSERT_EQ_INT(datetime_month(dt), DT_October);
    ASSERT_EQ_INT(datetime_day(dt), 3);

    datetime_dealloc(dt);
}

void test_datetime_eid_al_fitr_known_date(void) {
    datetime_t *dt = datetime_init_eid_al_fitr(datetime_alloc(), 2026);

    ASSERT_EQ_INT(datetime_year(dt), 2026);
    ASSERT_EQ_INT(datetime_month(dt), DT_March);
    ASSERT_EQ_INT(datetime_day(dt), 19);

    datetime_dealloc(dt);
}

void test_datetime_passover_known_date(void) {
    datetime_t *dt = datetime_init_passover(datetime_alloc(), 2026);

    ASSERT_EQ_INT(datetime_year(dt), 2026);
    ASSERT_EQ_INT(datetime_month(dt), DT_April);
    ASSERT_EQ_INT(datetime_day(dt), 2);

    datetime_dealloc(dt);
}

void test_datetime_hindu_observance_known_dates(void) {
    datetime_t *holi = datetime_init_holi(datetime_alloc(), 2024);
    datetime_t *new_year = datetime_init_hindu_new_year(datetime_alloc(), 2026);

    ASSERT_EQ_INT(datetime_year(holi), 2024);
    ASSERT_EQ_INT(datetime_month(holi), DT_March);
    ASSERT_EQ_INT(datetime_day(holi), 25);

    ASSERT_EQ_INT(datetime_year(new_year), 2026);
    ASSERT_EQ_INT(datetime_month(new_year), DT_March);
    ASSERT_EQ_INT(datetime_day(new_year), 19);

    datetime_dealloc(holi);
    datetime_dealloc(new_year);
}

void test_datetime_buddhist_observance_known_dates(void) {
    datetime_t *new_year = datetime_init_buddhist_new_year(datetime_alloc(), 2026);
    datetime_t *vesak = datetime_init_vesak(datetime_alloc(), 2026);
    datetime_t *asalha = datetime_init_asalha_puja(datetime_alloc(), 2026);

    ASSERT_EQ_INT(datetime_year(new_year), 2026);
    ASSERT_EQ_INT(datetime_month(new_year), DT_April);
    ASSERT_EQ_INT(datetime_day(new_year), 2);

    ASSERT_EQ_INT(datetime_year(vesak), 2026);
    ASSERT_EQ_INT(datetime_month(vesak), DT_May);
    ASSERT_EQ_INT(datetime_day(vesak), 31);

    ASSERT_EQ_INT(datetime_year(asalha), 2026);
    ASSERT_EQ_INT(datetime_month(asalha), DT_July);
    ASSERT_EQ_INT(datetime_day(asalha), 29);

    datetime_dealloc(new_year);
    datetime_dealloc(vesak);
    datetime_dealloc(asalha);
}

void test_datetime_calendar_date_texts_known_dates(void) {
    datetime_t *june = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 21);
    datetime_t *rosh = datetime_init_ymd(datetime_alloc(), 2024, DT_October, 3);
    datetime_t *ramadan = datetime_init_ymd(datetime_alloc(), 2026, DT_February, 17);
    datetime_t *ethiopian_new_year = datetime_init_ymd(datetime_alloc(), 2023, DT_September, 11);
    string_t *christian = datetime_christian_calendar_date_text(june);
    string_t *chinese = datetime_chinese_calendar_date_text(ramadan);
    string_t *hindu = datetime_hindu_calendar_date_text(june);
    string_t *buddhist = datetime_buddhist_calendar_date_text(june);
    string_t *muslim = datetime_muslim_calendar_date_text(ramadan);
    string_t *jewish = datetime_jewish_calendar_date_text(rosh);
    string_t *cherokee = datetime_cherokee_calendar_date_text(june);
    string_t *mayan = datetime_mayan_calendar_date_text(june);
    string_t *aztec = datetime_aztec_calendar_date_text(june);
    string_t *ethiopian = datetime_ethiopian_calendar_date_text(ethiopian_new_year);

    ASSERT_NOT_NULL(christian);
    ASSERT_NOT_NULL(chinese);
    ASSERT_NOT_NULL(hindu);
    ASSERT_NOT_NULL(buddhist);
    ASSERT_NOT_NULL(muslim);
    ASSERT_NOT_NULL(jewish);
    ASSERT_NOT_NULL(cherokee);
    ASSERT_NOT_NULL(mayan);
    ASSERT_NOT_NULL(aztec);
    ASSERT_NOT_NULL(ethiopian);

    TEST_ASSERT_STR_EQ(string_c_str(christian), "Gregorian 2026-06-21; Julian 2026-06-08");
    TEST_ASSERT_STR_EQ(string_c_str(chinese), "Year 4724 (Horse), month 1, day 1");
    TEST_ASSERT_STR_EQ(string_c_str(hindu), "Vikram Samvat 2083, Ashadha Shukla 7, lunar day 7");
    TEST_ASSERT_STR_EQ(string_c_str(buddhist), "B.E. 2569-06-21 (Thai solar)");
    TEST_ASSERT_STR_EQ(string_c_str(muslim), "1 Ramadan 1447 AH");
    TEST_ASSERT_STR_EQ(string_c_str(jewish), "1 Tishrei 5785 AM");
    TEST_ASSERT_STR_EQ(string_c_str(cherokee), "Cherokee civil Green Corn Moon, day 21, year 2026");
    TEST_ASSERT_STR_EQ(string_c_str(mayan), "Long Count 13.0.13.12.10; Tzolk'in 7 Ok; Haab 3 Sek");
    TEST_ASSERT_STR_EQ(string_c_str(aztec), "Tonalpohualli 5 Mazatl; Xiuhpohualli day 19 of Etzalcualiztli; year 1 Tochtli");
    TEST_ASSERT_STR_EQ(string_c_str(ethiopian), "1 Meskerem 2016 EC");

    string_free(christian);
    string_free(chinese);
    string_free(hindu);
    string_free(buddhist);
    string_free(muslim);
    string_free(jewish);
    string_free(cherokee);
    string_free(mayan);
    string_free(aztec);
    string_free(ethiopian);
    datetime_dealloc(june);
    datetime_dealloc(rosh);
    datetime_dealloc(ramadan);
    datetime_dealloc(ethiopian_new_year);
}

void test_datetime_additional_calendar_observances_known_dates(void) {
    datetime_t *enkutatash = datetime_init_ethiopian_new_year(datetime_alloc(), 2026);
    datetime_t *genna = datetime_init_genna(datetime_alloc(), 2026);
    datetime_t *timkat = datetime_init_timkat(datetime_alloc(), 2026);
    datetime_t *meskel = datetime_init_meskel(datetime_alloc(), 2026);
    datetime_t *fasika = datetime_init_fasika(datetime_alloc(), 2026);
    datetime_t *cherokee_new_moon_festival = datetime_init_cherokee_new_moon_festival(datetime_alloc(), 2026);
    datetime_t *cherokee_green_corn_ceremony = datetime_init_cherokee_green_corn_ceremony(datetime_alloc(), 2026);
    datetime_t *cherokee_ripe_corn_ceremony = datetime_init_cherokee_ripe_corn_ceremony(datetime_alloc(), 2026);
    datetime_t *cherokee_great_new_moon_festival = datetime_init_cherokee_great_new_moon_festival(datetime_alloc(), 2026);
    datetime_t *haab_new_year = datetime_init_mayan_haab_new_year(datetime_alloc(), 2026);
    datetime_t *wayeb = datetime_init_mayan_wayeb_start(datetime_alloc(), 2026);
    datetime_t *xiuh_new_year = datetime_init_aztec_xiuhpohualli_new_year(datetime_alloc(), 2026);
    datetime_t *nemontemi = datetime_init_aztec_nemontemi_start(datetime_alloc(), 2026);

    ASSERT_NOT_NULL(enkutatash);
    ASSERT_NOT_NULL(genna);
    ASSERT_NOT_NULL(timkat);
    ASSERT_NOT_NULL(meskel);
    ASSERT_NOT_NULL(fasika);
    ASSERT_NOT_NULL(cherokee_new_moon_festival);
    ASSERT_NOT_NULL(cherokee_green_corn_ceremony);
    ASSERT_NOT_NULL(cherokee_ripe_corn_ceremony);
    ASSERT_NOT_NULL(cherokee_great_new_moon_festival);
    ASSERT_NOT_NULL(haab_new_year);
    ASSERT_NOT_NULL(wayeb);
    ASSERT_NOT_NULL(xiuh_new_year);
    ASSERT_NOT_NULL(nemontemi);

    ASSERT_EQ_INT(datetime_year(enkutatash), 2026);
    ASSERT_EQ_INT(datetime_month(enkutatash), DT_September);
    ASSERT_EQ_INT(datetime_day(enkutatash), 11);

    ASSERT_EQ_INT(datetime_month(genna), DT_January);
    ASSERT_EQ_INT(datetime_day(genna), 7);

    ASSERT_EQ_INT(datetime_month(timkat), DT_January);
    ASSERT_EQ_INT(datetime_day(timkat), 19);

    ASSERT_EQ_INT(datetime_month(meskel), DT_September);
    ASSERT_EQ_INT(datetime_day(meskel), 27);

    ASSERT_EQ_INT(datetime_month(fasika), DT_April);
    ASSERT_EQ_INT(datetime_day(fasika), 12);

    ASSERT_EQ_INT(datetime_month(cherokee_new_moon_festival), DT_January);
    ASSERT_EQ_INT(datetime_day(cherokee_new_moon_festival), 18);

    ASSERT_EQ_INT(datetime_month(cherokee_green_corn_ceremony), DT_July);
    ASSERT_EQ_INT(datetime_day(cherokee_green_corn_ceremony), 14);

    ASSERT_EQ_INT(datetime_month(cherokee_ripe_corn_ceremony), DT_August);
    ASSERT_EQ_INT(datetime_day(cherokee_ripe_corn_ceremony), 12);

    ASSERT_EQ_INT(datetime_month(cherokee_great_new_moon_festival), DT_September);
    ASSERT_EQ_INT(datetime_day(cherokee_great_new_moon_festival), 10);

    ASSERT_EQ_INT(datetime_month(haab_new_year), DT_March);
    ASSERT_EQ_INT(datetime_day(haab_new_year), 30);

    ASSERT_EQ_INT(datetime_month(wayeb), DT_March);
    ASSERT_EQ_INT(datetime_day(wayeb), 25);

    ASSERT_EQ_INT(datetime_month(xiuh_new_year), DT_February);
    ASSERT_EQ_INT(datetime_day(xiuh_new_year), 23);

    ASSERT_EQ_INT(datetime_month(nemontemi), DT_February);
    ASSERT_EQ_INT(datetime_day(nemontemi), 18);

    datetime_dealloc(enkutatash);
    datetime_dealloc(genna);
    datetime_dealloc(timkat);
    datetime_dealloc(meskel);
    datetime_dealloc(fasika);
    datetime_dealloc(cherokee_new_moon_festival);
    datetime_dealloc(cherokee_green_corn_ceremony);
    datetime_dealloc(cherokee_ripe_corn_ceremony);
    datetime_dealloc(cherokee_great_new_moon_festival);
    datetime_dealloc(haab_new_year);
    datetime_dealloc(wayeb);
    datetime_dealloc(xiuh_new_year);
    datetime_dealloc(nemontemi);
}

void test_datetime_sunset_observance_start_gmt(void) {
    datetime_t *rosh = datetime_init_jewish_new_year(datetime_alloc(), 2024);
    datetime_t *start = datetime_init_sunset_observance_start(
        datetime_alloc(), rosh, 51.5074, -0.1278, 0.0
    );

    ASSERT_EQ_INT(datetime_year(start), 2024);
    ASSERT_EQ_INT(datetime_month(start), DT_October);
    ASSERT_EQ_INT(datetime_day(start), 2);
    ASSERT_TRUE(datetime_hour(start) >= 16 && datetime_hour(start) <= 19);

    datetime_dealloc(rosh);
    datetime_dealloc(start);
}

void test_datetime_sunset_rolls_into_following_civil_day_when_needed(void) {
    datetime_t *date = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 23);
    datetime_t *sunset;

    ASSERT_NOT_NULL(date);
    sunset = datetime_init_sunset(datetime_alloc(),
                                  datetime_jdn(date),
                                  64.1466,
                                  -21.9426,
                                  0.0);

    ASSERT_NOT_NULL(sunset);
    ASSERT_EQ_INT(datetime_year(sunset), 2026);
    ASSERT_EQ_INT(datetime_month(sunset), DT_June);
    ASSERT_EQ_INT(datetime_day(sunset), 24);
    ASSERT_EQ_INT(datetime_hour(sunset), 0);
    ASSERT_TRUE(datetime_minute(sunset) <= 10);

    datetime_dealloc(sunset);
    datetime_dealloc(date);
}

void test_datetime_svalbard_boundary_sunrise_keeps_after_midnight_local_time(void) {
    datetime_t *date = datetime_init_ymd(datetime_alloc(), 2026, DT_April, 19);
    datetime_t *sunrise;
    datetime_sun_status_t status = DATETIME_SUN_UNAVAILABLE;

    ASSERT_NOT_NULL(date);
    sunrise = datetime_init_sunrise_checked(datetime_alloc(),
                                            datetime_jdn(date),
                                            78.2232,
                                            15.6469,
                                            2.0,
                                            &status);
    ASSERT_NOT_NULL(sunrise);
    ASSERT_EQ_INT(status, DATETIME_SUN_OK);
    ASSERT_EQ_INT(datetime_year(sunrise), 2026);
    ASSERT_EQ_INT(datetime_month(sunrise), DT_April);
    ASSERT_EQ_INT(datetime_day(sunrise), 19);
    ASSERT_EQ_INT(datetime_hour(sunrise), 1);
    ASSERT_TRUE(datetime_minute(sunrise) >= 10 && datetime_minute(sunrise) <= 25);

    datetime_dealloc(sunrise);
    datetime_dealloc(date);
}

void test_datetime_adjacent_sun_events_resolve_neighbouring_days(void) {
    datetime_t *date = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 23);
    datetime_t *previous_sunset;
    datetime_t *next_sunrise;
    datetime_sun_status_t status = DATETIME_SUN_UNAVAILABLE;

    ASSERT_NOT_NULL(date);
    previous_sunset = datetime_init_previous_sunset_checked(datetime_alloc(),
                                                            datetime_jdn(date),
                                                            51.5074,
                                                            -0.1278,
                                                            1.0,
                                                            &status);
    ASSERT_NOT_NULL(previous_sunset);
    ASSERT_EQ_INT(status, DATETIME_SUN_OK);
    ASSERT_TRUE(datetime_compare(previous_sunset, date) < 0);

    next_sunrise = datetime_init_next_sunrise_checked(datetime_alloc(),
                                                      datetime_jdn(date),
                                                      51.5074,
                                                      -0.1278,
                                                      1.0,
                                                      &status);
    ASSERT_NOT_NULL(next_sunrise);
    ASSERT_EQ_INT(status, DATETIME_SUN_OK);
    ASSERT_TRUE(datetime_compare(next_sunrise, date) > 0);

    datetime_dealloc(next_sunrise);
    datetime_dealloc(previous_sunset);
    datetime_dealloc(date);
}

void test_datetime_format_uses_string_builder(void) {
    datetime_t *dt = datetime_init_ymdt(
        datetime_alloc(), 2024, DT_June, 15, 13, 5, 9.0
    );
    char *formatted = datetime_format(dt,
                                      "%Dddd %d%o %Mmmm %yyyy @hh:@mm:@ss @p %% @@");

    ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_STR_EQ(formatted,
                       "Saturday 15th June 2024 01:05:09 pm % @");

    free(formatted);
    datetime_dealloc(dt);
}

void test_datetime_format_superscript_ordinal_suffix(void) {
    datetime_t *dt = datetime_init_ymd(
        datetime_alloc(), 2026, DT_April, 1
    );
    char *formatted = datetime_format(dt, "%Dddd %d%q %Mmmm %yyyy");

    ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_STR_EQ(formatted, "Wednesday 1ˢᵗ April 2026");

    free(formatted);
    datetime_dealloc(dt);
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
    TEST_RUN_IN_GROUP(test_datetime_init_jd, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_jdn_and_getJulianDay, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_year_initialised, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_now, tests, NULL);

    /* GMT conversion tests */
    TEST_RUN_IN_GROUP(test_datetime_to_gmt_basic, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_to_gmt_null_pointer, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_to_gmt_preserves_julian_values, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_to_gmt_multiple_calls, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_to_gmt_uninitialised, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_to_gmt_with_julian_values, tests, NULL);

    /* Easter Sunday tests */
    TEST_RUN_IN_GROUP(test_datetime_init_easter_basic, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_easter_known_dates, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_easter_invalid_years, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_easter_always_sunday, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_easter_time_fields_zero, tests, NULL);

    /* Basic allocation and initialisation tests */
    TEST_RUN_IN_GROUP(test_datetime_alloc, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_dealloc_null_is_safe, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_from_string_valid_iso_date, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_from_string_rejects_invalid_iso_date, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_from_string_accepts_dmy_slash_date, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_from_string_ignores_outer_whitespace, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_ymd, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_ymdt, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_copy, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_jdn, tests, NULL);

    /* Chinese New Year tests */
    TEST_RUN_IN_GROUP(test_datetime_init_chinese_new_year_basic, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_chinese_new_year_known_dates, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_chinese_new_year_invalid_years, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_init_chinese_new_year_time_fields_zero, tests, NULL);

    /* Timezone offset tests */
    TEST_RUN_IN_GROUP(test_dttm_computeTimeZoneOffset_basic, tests, NULL);
    TEST_RUN_IN_GROUP(test_dttm_computeTimeZoneOffset_null_pointer, tests, NULL);
    TEST_RUN_IN_GROUP(test_dttm_computeTimeZoneOffset_uninitialised, tests, NULL);

    /* Julian consistency, getters, comparisons, days-in-month */
    TEST_RUN_IN_GROUP(test_dttm_julian_roundtrip, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_getters, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_compare_equal, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_compare_less, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_compare_greater, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_days_in_month, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_valid_ymd, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_display_names, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_solar_position_helpers, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_orthodox_easter_known_date, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_christmas_known_dates, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_jewish_new_year_known_date, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_eid_al_fitr_known_date, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_passover_known_date, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_hindu_observance_known_dates, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_buddhist_observance_known_dates, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_calendar_date_texts_known_dates, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_additional_calendar_observances_known_dates, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_sunset_observance_start_gmt, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_sunset_rolls_into_following_civil_day_when_needed, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_svalbard_boundary_sunrise_keeps_after_midnight_local_time, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_adjacent_sun_events_resolve_neighbouring_days, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_format_uses_string_builder, tests, NULL);
    TEST_RUN_IN_GROUP(test_datetime_format_superscript_ordinal_suffix, tests, NULL);

    printf(C_YELLOW "\nRunning README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_chinese_new_years, readme_examples,
                                  "datetime,readme,output");

    return TESTS_EXIT_CODE();
}
