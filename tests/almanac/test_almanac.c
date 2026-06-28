#include <math.h>
#include <stdio.h>

#include "almanac.h"
#include "datetime.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static bool test_almanac_suite_setup(void);
TEST_SUITE_SETUP(test_almanac_suite_setup);

static const double ARC_MINUTE_DEGREES = 1.0 / 60.0;
static const double ARC_SECOND_DEGREES = 1.0 / 3600.0;

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
    TEST_ASSERT_TRUE(fabs(entry.gha_aries_degrees - 280.456840341623) < 0.3 * ARC_MINUTE_DEGREES,
                     "Sirius GHA Aries is accurate enough for navigation");
    TEST_ASSERT_TRUE(fabs(entry.sha_degrees - 258.706271883014) < 0.3 * ARC_MINUTE_DEGREES,
                     "Sirius SHA is accurate enough for navigation");
    TEST_ASSERT_TRUE(fabs(entry.declination_degrees - (-16.717802474920)) < 0.1 * ARC_MINUTE_DEGREES,
                     "Sirius declination is accurate enough for navigation");
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
    TEST_ASSERT_TRUE(fabs(entry.gha_aries_degrees - 275.122459220820) < 0.1 * ARC_MINUTE_DEGREES,
                     "Moon GHA Aries is accurate enough for navigation");
    TEST_ASSERT_TRUE(fabs(entry.sha_degrees - 120.927866959190) < 0.6 * ARC_MINUTE_DEGREES,
                     "Moon SHA is accurate enough for navigation");
    TEST_ASSERT_TRUE(fabs(entry.declination_degrees - (-25.580814844815)) < 0.3 * ARC_MINUTE_DEGREES,
                     "Moon declination is accurate enough for navigation");
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

int tests_main(void)
{
    TEST_SECTION("Almanac");
    TEST_RUN_IN_GROUP(test_almanac_gha_aries_matches_j2000_reference, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_sirius, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_moon, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_snapshot_returns_catalogue_order, tests, NULL);
    return TEST_EXIT_CODE();
}
