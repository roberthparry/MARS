#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "almanac.h"
#include "datetime.h"
#include "spice_oracle.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static bool test_almanac_suite_setup(void);
TEST_SUITE_SETUP(test_almanac_suite_setup);

static const double ARC_SECOND_DEGREES = 1.0 / 3600.0;
static const long NAVIGATION_GRADE_ARCSECONDS = 6;

static double angular_delta_degrees(double observed, double expected)
{
    double delta = fmod(observed - expected + 180.0, 360.0);

    if (delta < 0.0)
        delta += 360.0;
    return delta - 180.0;
}

static long rounded_arcsecond_error(double error_degrees)
{
    return llround(error_degrees / ARC_SECOND_DEGREES);
}

static void print_oracle_axis(const char *label,
                              double expected,
                              double got,
                              double error_degrees,
                              long rounded_arcseconds)
{
    const char *grade = (rounded_arcseconds <= NAVIGATION_GRADE_ARCSECONDS) ? "PASS" : "FAIL";

    printf("    %-8s expected = % .9f\n", label, expected);
    printf("    %-8s got      = % .9f\n", label, got);
    printf("    %-8s error    = %.2f arcsec (rounded %ld, navigation grade %s <= %ld)\n",
           label,
           error_degrees / ARC_SECOND_DEGREES,
           rounded_arcseconds,
           grade,
           NAVIGATION_GRADE_ARCSECONDS);
}

static bool configured_almanac_is_usable(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    double gha = 0.0;
    bool ok = false;

    moment = datetime_alloc();
    if (!moment)
        return false;
    if (!datetime_init_jd(moment, 2451545.0)) {
        datetime_dealloc(moment);
        return false;
    }

    almanac = almanac_open();
    if (almanac)
        ok = almanac_gha_aries(almanac, moment, &gha);

    almanac_close(almanac);
    datetime_dealloc(moment);
    return ok;
}

static bool test_almanac_suite_setup(void)
{
    if (configured_almanac_is_usable())
        return true;

    fprintf(stderr,
            "Almanac tests require a configured almanac database.\n"
            "Install it with `make install-almanac-db`.\n");
    return false;
}

static void assert_gha_aries_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    double gha = 0.0;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_jd(moment, 2451545.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_gha_aries(almanac, moment, &gha), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(fabs(gha - 280.46061837) < 0.1 * ARC_SECOND_DEGREES,
                     "J2000 mean GHA Aries matches the reference closely");

    almanac_close(almanac);
    datetime_dealloc(moment);
}

static void test_almanac_gha_aries_matches_j2000_reference(void)
{
    assert_gha_aries_case();
}

static void assert_sirius_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t entry;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2000, DT_January, 1, 12, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_lookup(almanac, "SIRIUS", moment, &entry), almanac_last_error(almanac));
    TEST_ASSERT_STR_EQ(entry.body_code, "SIRIUS");
    TEST_ASSERT_TRUE(entry.body_kind == ALMANAC_BODY_STAR, "Sirius body kind is correct");
    TEST_ASSERT_TRUE(fabs(entry.visual_magnitude - (-1.46)) < 0.01, "Sirius visual magnitude is catalogued");
    TEST_ASSERT_TRUE(isnan(entry.geocentric_distance_au), "Fixed stars do not report finite geocentric distance");

    almanac_close(almanac);
    datetime_dealloc(moment);
}

static void test_almanac_lookup_resolves_sirius(void)
{
    assert_sirius_case();
}

static void assert_snapshot_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    array_t *snapshot;
    const almanac_entry_t *first;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 27, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    snapshot = almanac_snapshot(almanac, moment);
    TEST_ASSERT_NOT_NULL(snapshot);
    TEST_ASSERT_TRUE(array_size(snapshot) >= 8u, "snapshot returns enabled bodies");
    first = array_get(snapshot, 0u);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_STR_EQ(first->body_code, "SUN");

    array_destroy(snapshot);
    almanac_close(almanac);
    datetime_dealloc(moment);
}

static void test_almanac_snapshot_returns_catalogue_order(void)
{
    assert_snapshot_case();
}

static void assert_moon_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t entry;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 27, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_lookup(almanac, "MOON", moment, &entry), almanac_last_error(almanac));
    TEST_ASSERT_STR_EQ(entry.body_code, "MOON");
    TEST_ASSERT_TRUE(entry.body_kind == ALMANAC_BODY_MOON, "Moon body kind is correct");
    TEST_ASSERT_TRUE(entry.geocentric_distance_au > 0.0, "Moon geocentric distance is reported");
    TEST_ASSERT_TRUE(entry.heliocentric_distance_au > 0.0, "Moon heliocentric distance is reported");
    TEST_ASSERT_TRUE(entry.phase_angle_degrees >= 0.0 && entry.phase_angle_degrees <= 180.0,
                     "Moon phase angle is in range");
    TEST_ASSERT_TRUE(entry.visual_magnitude == entry.visual_magnitude, "Moon visual magnitude is finite");

    almanac_close(almanac);
    datetime_dealloc(moment);
}

static void test_almanac_lookup_resolves_moon(void)
{
    assert_moon_case();
}

static void assert_jupiter_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t entry;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 27, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_lookup(almanac, "JUPITER", moment, &entry), almanac_last_error(almanac));
    TEST_ASSERT_STR_EQ(entry.body_code, "JUPITER");
    TEST_ASSERT_TRUE(entry.body_kind == ALMANAC_BODY_PLANET, "Jupiter body kind is correct");
    TEST_ASSERT_TRUE(entry.geocentric_distance_au > 0.0, "Jupiter geocentric distance is reported");
    TEST_ASSERT_TRUE(entry.heliocentric_distance_au > 0.0, "Jupiter heliocentric distance is reported");

    almanac_close(almanac);
    datetime_dealloc(moment);
}

static void test_almanac_lookup_resolves_jupiter(void)
{
    assert_jupiter_case();
}

static void assert_spice_oracle_cases(void)
{
    almanac_t *almanac;
    unsigned int i;

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);

    for (i = 0; i < ALMANAC_SPICE_ORACLE_CASE_COUNT; ++i) {
        const almanac_spice_oracle_case_t *expected = &ALMANAC_SPICE_ORACLE_CASES[i];
        datetime_t *moment;
        almanac_entry_t entry;
        double sha_error;
        double dec_error;
        long sha_error_rounded;
        long dec_error_rounded;
        bool navigation_grade;
        char message[256];

        moment = datetime_alloc();
        TEST_ASSERT_NOT_NULL(moment);
        TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment,
                                                (short)expected->year,
                                                (month_t)expected->month,
                                                (uint8_t)expected->day,
                                                (uint8_t)expected->hour,
                                                (uint8_t)expected->minute,
                                                expected->second));
        TEST_ASSERT_TRUE(almanac_lookup(almanac, expected->body_code, moment, &entry), almanac_last_error(almanac));

        sha_error = fabs(angular_delta_degrees(entry.sha_degrees, expected->sha_degrees));
        dec_error = fabs(entry.declination_degrees - expected->declination_degrees);
        sha_error_rounded = rounded_arcsecond_error(sha_error);
        dec_error_rounded = rounded_arcsecond_error(dec_error);
        navigation_grade = (sha_error_rounded <= NAVIGATION_GRADE_ARCSECONDS &&
                            dec_error_rounded <= NAVIGATION_GRADE_ARCSECONDS);
        printf("ORACLE %-7s %04d-%02d-%02d %02d:%02d:%05.2f GMT [%s]\n",
               expected->body_code,
               expected->year,
               expected->month,
               expected->day,
               expected->hour,
               expected->minute,
               expected->second,
               navigation_grade ? "PASS" : "FAIL");
        print_oracle_axis("SHA", expected->sha_degrees, entry.sha_degrees, sha_error, sha_error_rounded);
        print_oracle_axis("Dec", expected->declination_degrees, entry.declination_degrees, dec_error, dec_error_rounded);
        printf("    distance expected = %.12f AU\n", expected->geocentric_distance_au);
        printf("    distance got      = %.12f AU\n", entry.geocentric_distance_au);
        printf("    navigation grade  = %s\n", navigation_grade ? "PASS" : "FAIL");
        snprintf(message,
                 sizeof(message),
                 "%s %04d-%02d-%02d SHA error %.2f arcsec rounds to %ld arcsec, above navigation-grade limit %ld arcsec",
                 expected->body_code,
                 expected->year,
                 expected->month,
                 expected->day,
                 sha_error / ARC_SECOND_DEGREES,
                 sha_error_rounded,
                 NAVIGATION_GRADE_ARCSECONDS);
        TEST_ASSERT_TRUE(sha_error_rounded <= NAVIGATION_GRADE_ARCSECONDS, message);
        snprintf(message,
                 sizeof(message),
                 "%s %04d-%02d-%02d declination error %.2f arcsec rounds to %ld arcsec, above navigation-grade limit %ld arcsec",
                 expected->body_code,
                 expected->year,
                 expected->month,
                 expected->day,
                 dec_error / ARC_SECOND_DEGREES,
                 dec_error_rounded,
                 NAVIGATION_GRADE_ARCSECONDS);
        TEST_ASSERT_TRUE(dec_error_rounded <= NAVIGATION_GRADE_ARCSECONDS, message);

        TEST_ASSERT_TRUE(fabs(entry.geocentric_distance_au - expected->geocentric_distance_au) < 0.02,
                         "geocentric distance is broadly consistent with SPICE");
        datetime_dealloc(moment);
    }

    almanac_close(almanac);
}

static void test_almanac_matches_oracle_across_supported_range(void)
{
    assert_spice_oracle_cases();
}

static void example_almanac_shrewsbury_eclipse_watch(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t sun;
    almanac_entry_t moon;
    double sha_separation;
    double dec_separation;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_August, 12, 17, 47, 5.8));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_lookup(almanac, "SUN", moment, &sun), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_lookup(almanac, "MOON", moment, &moon), almanac_last_error(almanac));

    sha_separation = fabs(angular_delta_degrees(sun.sha_degrees, moon.sha_degrees));
    dec_separation = fabs(sun.declination_degrees - moon.declination_degrees);

    printf("Shrewsbury eclipse watch\n");
    printf("2026-08-12 17:47:05.8 GMT\n");
    printf("Greatest eclipse of the 2026-08-12 solar eclipse.\n");
    printf("Shrewsbury sees a deep partial eclipse, not totality.\n");
    printf("Sun:  SHA %.9f deg  Dec %.9f deg  Dist %.12f AU\n",
           sun.sha_degrees,
           sun.declination_degrees,
           sun.geocentric_distance_au);
    printf("Moon: SHA %.9f deg  Dec %.9f deg  Dist %.12f AU\n",
           moon.sha_degrees,
           moon.declination_degrees,
           moon.geocentric_distance_au);
    printf("Separation: SHA %.2f arcsec  Dec %.2f arcsec\n",
           sha_separation / ARC_SECOND_DEGREES,
           dec_separation / ARC_SECOND_DEGREES);

    almanac_close(almanac);
    datetime_dealloc(moment);
}

int tests_main(void)
{
    TEST_SECTION("Almanac");
    TEST_RUN_IN_GROUP(test_almanac_gha_aries_matches_j2000_reference, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_sirius, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_moon, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_jupiter, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_matches_oracle_across_supported_range, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_snapshot_returns_catalogue_order, tests, NULL);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_almanac_shrewsbury_eclipse_watch,
                                  readme_examples,
                                  "almanac,readme");
    return TEST_EXIT_CODE();
}
