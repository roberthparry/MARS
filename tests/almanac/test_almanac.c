#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "almanac.h"
#include "datetime.h"
#include "jurisdiction.h"
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

static datetime_t *datetime_from_event_time(const almanac_event_time_t *event_time)
{
    datetime_t *dttm = datetime_alloc();

    if (!dttm)
        return NULL;
    if (!almanac_event_time_datetime(event_time, dttm)) {
        datetime_dealloc(dttm);
        return NULL;
    }
    return dttm;
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
    almanac_entry_t *entry;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2000, DT_January, 1, 12, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    entry = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_SIRIUS, moment);
    TEST_ASSERT_TRUE(entry != NULL, almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_entry_body_id(entry) == ALMANAC_BODY_ID_SIRIUS, "Sirius body id is correct");
    TEST_ASSERT_TRUE(almanac_entry_body_kind(entry) == ALMANAC_BODY_STAR, "Sirius body kind is correct");
    TEST_ASSERT_TRUE(fabs(almanac_entry_visual_magnitude(entry) - (-1.46)) < 0.01,
                     "Sirius visual magnitude is catalogued");
    TEST_ASSERT_TRUE(isnan(almanac_entry_geocentric_distance_au(entry)),
                     "Fixed stars do not report finite geocentric distance");

    almanac_close(almanac);
    almanac_entry_dealloc(entry);
    datetime_dealloc(moment);
}

static void test_almanac_new_body_entry_resolves_sirius(void)
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
    TEST_ASSERT_TRUE(almanac_entry_body_id(first) == ALMANAC_BODY_ID_SUN, "snapshot starts with the Sun");

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
    almanac_entry_t *entry;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 27, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    entry = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_MOON, moment);
    TEST_ASSERT_TRUE(entry != NULL, almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_entry_body_id(entry) == ALMANAC_BODY_ID_MOON, "Moon body id is correct");
    TEST_ASSERT_TRUE(almanac_entry_body_kind(entry) == ALMANAC_BODY_MOON, "Moon body kind is correct");
    TEST_ASSERT_TRUE(almanac_entry_geocentric_distance_au(entry) > 0.0, "Moon geocentric distance is reported");
    TEST_ASSERT_TRUE(almanac_entry_heliocentric_distance_au(entry) > 0.0, "Moon heliocentric distance is reported");
    TEST_ASSERT_TRUE(almanac_entry_phase_angle_degrees(entry) >= 0.0 &&
                     almanac_entry_phase_angle_degrees(entry) <= 180.0,
                     "Moon phase angle is in range");
    TEST_ASSERT_TRUE(almanac_entry_visual_magnitude(entry) == almanac_entry_visual_magnitude(entry),
                     "Moon visual magnitude is finite");

    almanac_close(almanac);
    almanac_entry_dealloc(entry);
    datetime_dealloc(moment);
}

static void test_almanac_new_body_entry_resolves_moon(void)
{
    assert_moon_case();
}

static void assert_jupiter_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t *entry;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 27, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    entry = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_JUPITER, moment);
    TEST_ASSERT_TRUE(entry != NULL, almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_entry_body_id(entry) == ALMANAC_BODY_ID_JUPITER, "Jupiter body id is correct");
    TEST_ASSERT_TRUE(almanac_entry_body_kind(entry) == ALMANAC_BODY_PLANET, "Jupiter body kind is correct");
    TEST_ASSERT_TRUE(almanac_entry_geocentric_distance_au(entry) > 0.0, "Jupiter geocentric distance is reported");
    TEST_ASSERT_TRUE(almanac_entry_heliocentric_distance_au(entry) > 0.0, "Jupiter heliocentric distance is reported");

    almanac_close(almanac);
    almanac_entry_dealloc(entry);
    datetime_dealloc(moment);
}

static void test_almanac_new_body_entry_resolves_jupiter(void)
{
    assert_jupiter_case();
}

static void assert_observables_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t *sun;
    almanac_entry_t *moon;
    almanac_entry_t *sirius;
    almanac_observer_t observer = { 52.7073, -2.7540, 0.0 };
    almanac_observables_t sun_obs;
    almanac_observables_t moon_obs;
    almanac_observables_t sirius_obs;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_August, 12, 17, 47, 5.8));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    sun = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_SUN, moment);
    TEST_ASSERT_TRUE(sun != NULL, almanac_last_error(almanac));
    moon = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_MOON, moment);
    TEST_ASSERT_TRUE(moon != NULL, almanac_last_error(almanac));
    sirius = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_SIRIUS, moment);
    TEST_ASSERT_TRUE(sirius != NULL, almanac_last_error(almanac));

    TEST_ASSERT_TRUE(almanac_observables(almanac, sun, &observer, &sun_obs), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_observables(almanac, moon, &observer, &moon_obs), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_observables(almanac, sirius, &observer, &sirius_obs), almanac_last_error(almanac));

    TEST_ASSERT_TRUE(sun_obs.altitude_degrees >= -90.0 && sun_obs.altitude_degrees <= 90.0,
                     "Sun altitude is in range");
    TEST_ASSERT_TRUE(sun_obs.azimuth_degrees >= 0.0 && sun_obs.azimuth_degrees < 360.0,
                     "Sun azimuth is in range");
    TEST_ASSERT_TRUE(sun_obs.semi_diameter_degrees > 0.0,
                     "Sun semi-diameter is finite and positive");
    TEST_ASSERT_TRUE(sun_obs.visible,
                     "Sun is visible from Shrewsbury at the eclipse moment");

    TEST_ASSERT_TRUE(moon_obs.altitude_degrees >= -90.0 && moon_obs.altitude_degrees <= 90.0,
                     "Moon altitude is in range");
    TEST_ASSERT_TRUE(moon_obs.azimuth_degrees >= 0.0 && moon_obs.azimuth_degrees < 360.0,
                     "Moon azimuth is in range");
    TEST_ASSERT_TRUE(moon_obs.semi_diameter_degrees > 0.0,
                     "Moon semi-diameter is finite and positive");
    TEST_ASSERT_TRUE(moon_obs.visible,
                     "Moon is visible from Shrewsbury at the eclipse moment");

    TEST_ASSERT_TRUE(sirius_obs.altitude_degrees >= -90.0 && sirius_obs.altitude_degrees <= 90.0,
                     "Sirius altitude is in range");
    TEST_ASSERT_TRUE(sirius_obs.azimuth_degrees >= 0.0 && sirius_obs.azimuth_degrees < 360.0,
                     "Sirius azimuth is in range");
    TEST_ASSERT_TRUE(isnan(sirius_obs.semi_diameter_degrees),
                     "Stars do not report a finite semi-diameter");
    TEST_ASSERT_TRUE(!sirius_obs.visible,
                     "Sirius is not above the horizon at the eclipse moment");

    almanac_close(almanac);
    almanac_entry_dealloc(sirius);
    almanac_entry_dealloc(moon);
    almanac_entry_dealloc(sun);
    datetime_dealloc(moment);
}

static void test_almanac_observables_resolve_horizon_values(void)
{
    assert_observables_case();
}

static void assert_observables_validation_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t *sun;
    almanac_observer_t invalid_observer = { 123.0, 0.0, 0.0 };
    almanac_observables_t observables;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_August, 12, 17, 47, 5.8));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    sun = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_SUN, moment);
    TEST_ASSERT_TRUE(sun != NULL, almanac_last_error(almanac));
    TEST_ASSERT_TRUE(!almanac_observables(almanac, sun, &invalid_observer, &observables),
                     "invalid observer latitude is rejected");
    TEST_ASSERT_TRUE(strcmp(almanac_last_error(almanac), "observer latitude must be in [-90, 90]") == 0,
                     "invalid observer latitude sets a helpful error");

    almanac_close(almanac);
    almanac_entry_dealloc(sun);
    datetime_dealloc(moment);
}

static void test_almanac_observables_validate_observer_range(void)
{
    assert_observables_validation_case();
}

static void assert_geographical_position_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t *sun;
    almanac_geographical_position_t gp;
    double body_gha;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 30, 8, 14, 7.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    sun = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_SUN, moment);
    TEST_ASSERT_TRUE(sun != NULL, almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_body_geographical_position(sun, &gp),
                     "Sun geographical position is available");

    body_gha = almanac_entry_gha_aries_degrees(sun) + almanac_entry_sha_degrees(sun);
    TEST_ASSERT_TRUE(fabs(gp.latitude_degrees - almanac_entry_declination_degrees(sun)) < 1.0e-12,
                     "GP latitude is the body declination");
    TEST_ASSERT_TRUE(fabs(angular_delta_degrees(gp.longitude_degrees, -body_gha)) < 1.0e-12,
                     "GP longitude is the east-positive inverse of body GHA");
    TEST_ASSERT_TRUE(gp.longitude_degrees >= -180.0 && gp.longitude_degrees < 180.0,
                     "GP longitude uses signed east-positive degrees");

    almanac_close(almanac);
    almanac_entry_dealloc(sun);
    datetime_dealloc(moment);
}

static void test_almanac_body_geographical_position_resolves_gp(void)
{
    assert_geographical_position_case();
}

static void assert_phase_details_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t *venus;
    almanac_entry_t *jupiter;
    almanac_phase_details_t venus_phase;
    almanac_phase_details_t jupiter_phase;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 27, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    venus = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_VENUS, moment);
    TEST_ASSERT_TRUE(venus != NULL, almanac_last_error(almanac));
    jupiter = almanac_new_body_entry(almanac, ALMANAC_BODY_ID_JUPITER, moment);
    TEST_ASSERT_TRUE(jupiter != NULL, almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_phase_details(venus, &venus_phase), "Venus phase details resolve");
    TEST_ASSERT_TRUE(almanac_phase_details(jupiter, &jupiter_phase), "Jupiter phase details resolve");

    TEST_ASSERT_TRUE(venus_phase.illuminated_fraction >= 0.0 && venus_phase.illuminated_fraction <= 1.0,
                     "Venus illuminated fraction is in range");
    TEST_ASSERT_TRUE(venus_phase.phase_class != ALMANAC_PHASE_UNKNOWN,
                     "Venus phase class is known");
    TEST_ASSERT_TRUE(jupiter_phase.illuminated_fraction >= 0.0 && jupiter_phase.illuminated_fraction <= 1.0,
                     "Jupiter illuminated fraction is in range");
    TEST_ASSERT_TRUE(jupiter_phase.phase_class == ALMANAC_PHASE_GIBBOUS ||
                     jupiter_phase.phase_class == ALMANAC_PHASE_FULL,
                     "Jupiter phase class is near full as expected for an outer planet");

    almanac_close(almanac);
    almanac_entry_dealloc(jupiter);
    almanac_entry_dealloc(venus);
    datetime_dealloc(moment);
}

static void test_almanac_phase_details_resolve_planetary_phase(void)
{
    assert_phase_details_case();
}

static void assert_exact_moon_phase_case(void)
{
    datetime_t *after;
    almanac_t *almanac;
    almanac_moon_phase_event_t phase_event;

    after = datetime_alloc();
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(after, 2026, DT_August, 1, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_next_moon_phase_exact(almanac,
                                                   after,
                                                   ALMANAC_MOON_PHASE_NEW,
                                                   &phase_event),
                     almanac_last_error(almanac));
    TEST_ASSERT_TRUE(phase_event.time.valid, "next new Moon event time is valid");
    TEST_ASSERT_TRUE(phase_event.time.jd > datetime_jd(after),
                     "next new Moon occurs after the starting date");
    TEST_ASSERT_TRUE(phase_event.time.jd > 2461264.0 &&
                     phase_event.time.jd < 2461266.5,
                     "next new Moon lands near the August 2026 eclipse date");
    TEST_ASSERT_TRUE(phase_event.illuminated_fraction < 0.01,
                     "exact new Moon is nearly unilluminated");

    almanac_close(almanac);
    datetime_dealloc(after);
}

static void test_almanac_next_moon_phase_exact_finds_august_2026_new_moon(void)
{
    assert_exact_moon_phase_case();
}

static void test_almanac_next_moon_phase_exact_advances_past_previous_phase(void)
{
    datetime_t *after;
    almanac_t *almanac;
    almanac_moon_phase_event_t first;
    almanac_moon_phase_event_t next;

    after = datetime_alloc();
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_NOT_NULL(datetime_init_ymd(after, 2031, DT_October, 25));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_next_moon_phase_exact(almanac,
                                                   after,
                                                   ALMANAC_MOON_PHASE_FULL,
                                                   &first),
                     almanac_last_error(almanac));
    TEST_ASSERT_NOT_NULL(datetime_init_jd(after, first.time.jd + 0.5));
    TEST_ASSERT_TRUE(almanac_next_moon_phase_exact(almanac,
                                                   after,
                                                   ALMANAC_MOON_PHASE_FULL,
                                                   &next),
                     almanac_last_error(almanac));
    TEST_ASSERT_TRUE(next.time.jd > datetime_jd(after),
                     "next full Moon advances beyond the requested instant");
    TEST_ASSERT_TRUE(next.time.jd > first.time.jd + 25.0,
                     "successive full Moons are distinct lunations");

    almanac_close(almanac);
    datetime_dealloc(after);
}

static void assert_sunrise_sunset_case(void)
{
    datetime_t *date;
    almanac_t *almanac;
    jurisdiction_t *jurisdiction;
    almanac_sun_times_t sun_times;
    almanac_rise_set_t generic_sun_times;
    datetime_t *sunrise_time;
    datetime_t *sunset_time;
    almanac_observer_t observer = {52.7073, -2.7540, 75.0};

    date = datetime_alloc();
    TEST_ASSERT_NOT_NULL(date);
    TEST_ASSERT_NOT_NULL(datetime_init_ymd(date, 2026, DT_August, 12));

    almanac = almanac_open();
    jurisdiction = jurisdict_open("GB-ENG");
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_NOT_NULL(jurisdiction);
    TEST_ASSERT_TRUE(almanac_sunrise_sunset(almanac,
                                            jurisdiction,
                                            date,
                                            &observer,
                                            &sun_times),
                     almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_body_rise_set(almanac,
                                           jurisdiction,
                                           ALMANAC_BODY_ID_SUN,
                                           date,
                                           &observer,
                                           &generic_sun_times),
                     almanac_last_error(almanac));
    TEST_ASSERT_TRUE(fabs(generic_sun_times.rise.time.jd - sun_times.rise.time.jd) < 1e-10,
                     "generic Sun rise matches sunrise wrapper");
    TEST_ASSERT_TRUE(fabs(generic_sun_times.set.time.jd - sun_times.set.time.jd) < 1e-10,
                     "generic Sun set matches sunset wrapper");
    sunrise_time = datetime_from_event_time(&sun_times.rise.time);
    sunset_time = datetime_from_event_time(&sun_times.set.time);
    TEST_ASSERT_NOT_NULL(sunrise_time);
    TEST_ASSERT_NOT_NULL(sunset_time);

    printf("    Shrewsbury 2026-08-12 sunrise %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           datetime_hour(sunrise_time),
           datetime_minute(sunrise_time),
           datetime_second(sunrise_time),
           sun_times.rise.azimuth_degrees);
    printf("    Shrewsbury 2026-08-12 sunset  %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           datetime_hour(sunset_time),
           datetime_minute(sunset_time),
           datetime_second(sunset_time),
           sun_times.set.azimuth_degrees);

    TEST_ASSERT_TRUE(sun_times.rise.status == ALMANAC_RISE_SET_OK, "sunrise occurs locally");
    TEST_ASSERT_TRUE(sun_times.set.status == ALMANAC_RISE_SET_OK, "sunset occurs locally");
    TEST_ASSERT_TRUE(sun_times.rise.time.valid, "sunrise local time is valid");
    TEST_ASSERT_TRUE(sun_times.set.time.valid, "sunset local time is valid");
    TEST_ASSERT_TRUE(datetime_year(sunrise_time) == 2026 &&
                     datetime_month(sunrise_time) == DT_August &&
                     datetime_day(sunrise_time) == 12,
                     "sunrise is returned on the requested local date");
    TEST_ASSERT_TRUE(datetime_year(sunset_time) == 2026 &&
                     datetime_month(sunset_time) == DT_August &&
                     datetime_day(sunset_time) == 12,
                     "sunset is returned on the requested local date");
    TEST_ASSERT_TRUE(datetime_hour(sunrise_time) >= 5 &&
                     datetime_hour(sunrise_time) <= 7,
                     "sunrise local hour is plausible for Shrewsbury in August");
    TEST_ASSERT_TRUE(datetime_hour(sunset_time) >= 20 &&
                     datetime_hour(sunset_time) <= 21,
                     "sunset local hour is plausible for Shrewsbury in August");
    TEST_ASSERT_TRUE(sun_times.rise.azimuth_degrees >= 45.0 &&
                     sun_times.rise.azimuth_degrees <= 90.0,
                     "sunrise azimuth is north-east/east in August");
    TEST_ASSERT_TRUE(sun_times.set.azimuth_degrees >= 270.0 &&
                     sun_times.set.azimuth_degrees <= 315.0,
                     "sunset azimuth is west/north-west in August");

    jurisdict_close(jurisdiction);
    almanac_close(almanac);
    datetime_dealloc(sunrise_time);
    datetime_dealloc(sunset_time);
    datetime_dealloc(date);
}

static void test_almanac_sunrise_sunset_returns_local_day_times(void)
{
    assert_sunrise_sunset_case();
}

static void assert_moonrise_moonset_case(void)
{
    datetime_t *date;
    almanac_t *almanac;
    jurisdiction_t *jurisdiction;
    almanac_moon_times_t moon_times;
    datetime_t *moonrise_time;
    datetime_t *moonset_time;
    almanac_observer_t observer = {52.7073, -2.7540, 75.0};

    date = datetime_alloc();
    TEST_ASSERT_NOT_NULL(date);
    TEST_ASSERT_NOT_NULL(datetime_init_ymd(date, 2026, DT_August, 12));

    almanac = almanac_open();
    jurisdiction = jurisdict_open("GB-ENG");
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_NOT_NULL(jurisdiction);
    TEST_ASSERT_TRUE(almanac_moonrise_moonset(almanac,
                                              jurisdiction,
                                              date,
                                              &observer,
                                              &moon_times),
                     almanac_last_error(almanac));
    moonrise_time = datetime_from_event_time(&moon_times.rise.time);
    moonset_time = datetime_from_event_time(&moon_times.set.time);
    TEST_ASSERT_NOT_NULL(moonrise_time);
    TEST_ASSERT_NOT_NULL(moonset_time);

    printf("    Shrewsbury 2026-08-12 moonrise %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           datetime_hour(moonrise_time),
           datetime_minute(moonrise_time),
           datetime_second(moonrise_time),
           moon_times.rise.azimuth_degrees);
    printf("    Shrewsbury 2026-08-12 moonset  %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           datetime_hour(moonset_time),
           datetime_minute(moonset_time),
           datetime_second(moonset_time),
           moon_times.set.azimuth_degrees);

    TEST_ASSERT_TRUE(moon_times.rise.status == ALMANAC_RISE_SET_OK, "moonrise occurs locally");
    TEST_ASSERT_TRUE(moon_times.set.status == ALMANAC_RISE_SET_OK, "moonset occurs locally");
    TEST_ASSERT_TRUE(moon_times.rise.time.valid, "moonrise local time is valid");
    TEST_ASSERT_TRUE(moon_times.set.time.valid, "moonset local time is valid");
    TEST_ASSERT_TRUE(datetime_year(moonrise_time) == 2026 &&
                     datetime_month(moonrise_time) == DT_August &&
                     datetime_day(moonrise_time) == 12,
                     "moonrise is returned on the requested local date");
    TEST_ASSERT_TRUE(datetime_year(moonset_time) == 2026 &&
                     datetime_month(moonset_time) == DT_August &&
                     datetime_day(moonset_time) == 12,
                     "moonset is returned on the requested local date");
    TEST_ASSERT_TRUE(moon_times.rise.time.jd < moon_times.set.time.jd,
                     "moonrise precedes moonset near the August 2026 new Moon");
    TEST_ASSERT_TRUE(moon_times.rise.azimuth_degrees >= 45.0 &&
                     moon_times.rise.azimuth_degrees <= 120.0,
                     "moonrise azimuth is plausible near new Moon");
    TEST_ASSERT_TRUE(moon_times.set.azimuth_degrees >= 240.0 &&
                     moon_times.set.azimuth_degrees <= 315.0,
                     "moonset azimuth is plausible near new Moon");

    jurisdict_close(jurisdiction);
    almanac_close(almanac);
    datetime_dealloc(moonrise_time);
    datetime_dealloc(moonset_time);
    datetime_dealloc(date);
}

static void test_almanac_moonrise_moonset_returns_local_day_times(void)
{
    assert_moonrise_moonset_case();
}

static void assert_solar_eclipse_search_case(void)
{
    datetime_t *start;
    datetime_t *end;
    almanac_t *almanac;
    array_t *events;
    const almanac_solar_eclipse_t *event;
    almanac_event_time_t first_contact;
    almanac_event_time_t second_contact;
    almanac_event_time_t greatest_eclipse;
    almanac_event_time_t third_contact;
    almanac_event_time_t fourth_contact;
    almanac_solar_eclipse_kind_t kind;
    almanac_observer_t observer = {52.7073, -2.7540, 75.0};

    start = datetime_alloc();
    end = datetime_alloc();
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(start, 2026, DT_August, 1, 0, 0, 0.0));
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(end, 2026, DT_August, 20, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    events = almanac_find_solar_eclipses(almanac, &observer, start, end);
    TEST_ASSERT_NOT_NULL(events);
    TEST_ASSERT_TRUE(array_size(events) >= 1u, "August 2026 window includes a solar eclipse");
    event = array_get(events, 0u);
    TEST_ASSERT_NOT_NULL(event);
    kind = almanac_solar_eclipse_kind(event);
    TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FIRST_CONTACT, &first_contact),
                     "solar eclipse first contact local time is valid");
    TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest_eclipse),
                     "solar eclipse greatest local time is valid");
    TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FOURTH_CONTACT, &fourth_contact),
                     "solar eclipse fourth contact local time is valid");
    TEST_ASSERT_TRUE(kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ||
                     kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR ||
                     kind == ALMANAC_SOLAR_ECLIPSE_PARTIAL,
                     "solar eclipse kind is classified");
    TEST_ASSERT_TRUE(almanac_solar_eclipse_magnitude(event) > 0.0, "solar eclipse magnitude is positive");
    TEST_ASSERT_TRUE(almanac_solar_eclipse_totality_percent(event) > 0.0 &&
                     almanac_solar_eclipse_totality_percent(event) <= 100.0,
                     "solar eclipse totality percentage is bounded");
    TEST_ASSERT_TRUE(first_contact.jd == first_contact.jd &&
                     first_contact.jd < greatest_eclipse.jd,
                     "solar eclipse first contact precedes greatest eclipse");
    TEST_ASSERT_TRUE(fourth_contact.jd == fourth_contact.jd &&
                     fourth_contact.jd > greatest_eclipse.jd,
                     "solar eclipse fourth contact follows greatest eclipse");
    TEST_ASSERT_TRUE(first_contact.valid, "solar eclipse first contact local time is valid");
    TEST_ASSERT_TRUE(greatest_eclipse.valid, "solar eclipse greatest local time is valid");
    TEST_ASSERT_TRUE(fourth_contact.valid, "solar eclipse fourth contact local time is valid");
    if (kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ||
        kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR) {
        TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_SECOND_CONTACT, &second_contact),
                         "solar eclipse second contact local time is valid");
        TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_THIRD_CONTACT, &third_contact),
                         "solar eclipse third contact local time is valid");
        TEST_ASSERT_TRUE(second_contact.jd == second_contact.jd &&
                         second_contact.jd < greatest_eclipse.jd,
                         "solar eclipse second contact precedes greatest eclipse");
        TEST_ASSERT_TRUE(third_contact.jd == third_contact.jd &&
                         third_contact.jd > greatest_eclipse.jd,
                         "solar eclipse third contact follows greatest eclipse");
        TEST_ASSERT_TRUE(second_contact.valid, "solar eclipse second contact local time is valid");
        TEST_ASSERT_TRUE(third_contact.valid, "solar eclipse third contact local time is valid");
    }

    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
}

static void test_almanac_find_solar_eclipses_returns_august_2026_event(void)
{
    assert_solar_eclipse_search_case();
}

static void assert_lunar_eclipse_search_case(void)
{
    datetime_t *start;
    datetime_t *end;
    almanac_t *almanac;
    array_t *events;
    const almanac_lunar_eclipse_t *event;
    almanac_event_time_t p1_contact;
    almanac_event_time_t u1_contact;
    almanac_event_time_t greatest_eclipse;
    almanac_event_time_t u4_contact;
    almanac_event_time_t p4_contact;
    almanac_lunar_eclipse_kind_t kind;
    almanac_observer_t observer = {40.7128, -74.0060, 10.0};

    start = datetime_alloc();
    end = datetime_alloc();
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(start, 2025, DT_January, 1, 0, 0, 0.0));
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(end, 2025, DT_December, 31, 23, 59, 59.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    events = almanac_find_lunar_eclipses(almanac, &observer, start, end);
    TEST_ASSERT_NOT_NULL(events);
    TEST_ASSERT_TRUE(array_size(events) >= 1u, "calendar year 2025 includes at least one lunar eclipse");
    event = array_get(events, 0u);
    TEST_ASSERT_NOT_NULL(event);
    kind = almanac_lunar_eclipse_kind(event);
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_P1_CONTACT, &p1_contact),
                     "lunar eclipse P1 local time is valid");
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest_eclipse),
                     "lunar eclipse greatest local time is valid");
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_P4_CONTACT, &p4_contact),
                     "lunar eclipse P4 local time is valid");
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_penumbral_magnitude(event) > 0.0,
                     "lunar eclipse penumbral magnitude is positive");
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_totality_percent(event) >= 0.0 &&
                     almanac_lunar_eclipse_totality_percent(event) <= 100.0,
                     "lunar eclipse totality percentage is bounded");
    TEST_ASSERT_TRUE(p1_contact.jd == p1_contact.jd &&
                     p1_contact.jd < greatest_eclipse.jd,
                     "lunar eclipse P1 contact precedes greatest eclipse");
    TEST_ASSERT_TRUE(p4_contact.jd == p4_contact.jd &&
                     p4_contact.jd > greatest_eclipse.jd,
                     "lunar eclipse P4 contact follows greatest eclipse");
    TEST_ASSERT_TRUE(p1_contact.valid, "lunar eclipse P1 local time is valid");
    TEST_ASSERT_TRUE(greatest_eclipse.valid, "lunar eclipse greatest local time is valid");
    TEST_ASSERT_TRUE(p4_contact.valid, "lunar eclipse P4 local time is valid");
    if (kind == ALMANAC_LUNAR_ECLIPSE_PARTIAL ||
        kind == ALMANAC_LUNAR_ECLIPSE_TOTAL) {
        TEST_ASSERT_TRUE(almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_U1_CONTACT, &u1_contact),
                         "lunar eclipse U1 local time is valid");
        TEST_ASSERT_TRUE(almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_U4_CONTACT, &u4_contact),
                         "lunar eclipse U4 local time is valid");
        TEST_ASSERT_TRUE(u1_contact.jd == u1_contact.jd &&
                         u1_contact.jd < greatest_eclipse.jd,
                         "lunar eclipse U1 contact precedes greatest eclipse");
        TEST_ASSERT_TRUE(u4_contact.jd == u4_contact.jd &&
                         u4_contact.jd > greatest_eclipse.jd,
                         "lunar eclipse U4 contact follows greatest eclipse");
        TEST_ASSERT_TRUE(u1_contact.valid, "lunar eclipse U1 local time is valid");
        TEST_ASSERT_TRUE(u4_contact.valid, "lunar eclipse U4 local time is valid");
    }

    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
}

static void test_almanac_find_lunar_eclipses_returns_events_in_2025(void)
{
    assert_lunar_eclipse_search_case();
}

static void test_almanac_find_lunar_eclipses_matches_july_2028_partial_event(void)
{
    datetime_t *start = datetime_alloc();
    datetime_t *end = datetime_alloc();
    almanac_t *almanac;
    array_t *events;
    const almanac_lunar_eclipse_t *event;
    almanac_event_time_t greatest;
    almanac_observer_t cape_town = {-33.9258, 18.4232, 25.0};

    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(start, 2028, DT_July, 5, 0, 0, 0.0));
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(end, 2028, DT_July, 7, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    events = almanac_find_lunar_eclipses(almanac, &cape_town, start, end);
    TEST_ASSERT_NOT_NULL(events);
    TEST_ASSERT_TRUE(array_size(events) == 1u, "July 2028 window contains one visible lunar eclipse");
    event = array_get(events, 0u);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_kind(event) == ALMANAC_LUNAR_ECLIPSE_PARTIAL,
                     "July 2028 lunar eclipse is partial");
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest),
                     "July 2028 greatest eclipse is available");
    TEST_ASSERT_TRUE(greatest.jd > 2461959.263 &&
                     greatest.jd < 2461959.265,
                     "July 2028 greatest eclipse is near 18:20 GMT");
    TEST_ASSERT_TRUE(almanac_lunar_eclipse_umbral_magnitude(event) > 0.35 &&
                     almanac_lunar_eclipse_umbral_magnitude(event) < 0.45,
                     "July 2028 umbral magnitude is approximately 0.39");

    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
}

static void assert_solar_transit_search_case(void)
{
    datetime_t *start;
    datetime_t *end;
    almanac_t *almanac;
    array_t *events;
    const almanac_solar_transit_t *event;
    almanac_event_time_t first_contact;
    almanac_event_time_t second_contact;
    almanac_event_time_t greatest_transit;
    almanac_event_time_t third_contact;
    almanac_event_time_t fourth_contact;
    almanac_observer_t observer = {52.7073, -2.7540, 75.0};

    start = datetime_alloc();
    end = datetime_alloc();
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(start, 2019, DT_November, 1, 0, 0, 0.0));
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(end, 2019, DT_November, 20, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    events = almanac_find_solar_transits_for_body(almanac, ALMANAC_BODY_ID_MERCURY, &observer, start, end);
    TEST_ASSERT_NOT_NULL(events);
    TEST_ASSERT_TRUE(array_size(events) >= 1u, "November 2019 includes a Mercury transit");
    event = array_get(events, 0u);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_TRUE(almanac_solar_transit_time(event, ALMANAC_EVENT_TIME_FIRST_CONTACT, &first_contact),
                     "solar transit first contact local time is valid");
    TEST_ASSERT_TRUE(almanac_solar_transit_time(event, ALMANAC_EVENT_TIME_SECOND_CONTACT, &second_contact),
                     "solar transit second contact local time is valid");
    TEST_ASSERT_TRUE(almanac_solar_transit_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest_transit),
                     "solar transit greatest local time is valid");
    TEST_ASSERT_TRUE(almanac_solar_transit_time(event, ALMANAC_EVENT_TIME_THIRD_CONTACT, &third_contact),
                     "solar transit third contact local time is valid");
    TEST_ASSERT_TRUE(almanac_solar_transit_time(event, ALMANAC_EVENT_TIME_FOURTH_CONTACT, &fourth_contact),
                     "solar transit fourth contact local time is valid");
    TEST_ASSERT_TRUE(almanac_solar_transit_body_id(event) == ALMANAC_BODY_ID_MERCURY, "Transit body id is Mercury");
    TEST_ASSERT_TRUE(almanac_solar_transit_separation_degrees(event) <
                     almanac_solar_transit_solar_semi_diameter_degrees(event) +
                     almanac_solar_transit_planet_semi_diameter_degrees(event),
                     "Mercury transit occurs on the solar disc");
    TEST_ASSERT_TRUE(first_contact.jd == first_contact.jd &&
                     first_contact.jd < greatest_transit.jd,
                     "solar transit first contact precedes greatest transit");
    TEST_ASSERT_TRUE(second_contact.jd == second_contact.jd &&
                     second_contact.jd < greatest_transit.jd,
                     "solar transit second contact precedes greatest transit");
    TEST_ASSERT_TRUE(third_contact.jd == third_contact.jd &&
                     third_contact.jd > greatest_transit.jd,
                     "solar transit third contact follows greatest transit");
    TEST_ASSERT_TRUE(fourth_contact.jd == fourth_contact.jd &&
                     fourth_contact.jd > greatest_transit.jd,
                     "solar transit fourth contact follows greatest transit");
    TEST_ASSERT_TRUE(first_contact.valid, "solar transit first contact local time is valid");
    TEST_ASSERT_TRUE(second_contact.valid, "solar transit second contact local time is valid");
    TEST_ASSERT_TRUE(greatest_transit.valid, "solar transit greatest local time is valid");
    TEST_ASSERT_TRUE(third_contact.valid, "solar transit third contact local time is valid");
    TEST_ASSERT_TRUE(fourth_contact.valid, "solar transit fourth contact local time is valid");

    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
}

static void test_almanac_find_solar_transits_returns_mercury_event(void)
{
    assert_solar_transit_search_case();
}

static void test_almanac_find_solar_transits_returns_venus_event(void)
{
    datetime_t *start;
    datetime_t *end;
    almanac_t *almanac;
    array_t *events;
    const almanac_solar_transit_t *event;
    almanac_event_time_t greatest;
    almanac_observer_t observer = {35.6762, 139.6503, 40.0};

    start = datetime_alloc();
    end = datetime_alloc();
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(datetime_init_ymd(start, 2012, DT_June, 1));
    TEST_ASSERT_NOT_NULL(datetime_init_ymd(end, 2012, DT_June, 10));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    events = almanac_find_solar_transits_for_body(almanac,
                                                  ALMANAC_BODY_ID_VENUS,
                                                  &observer,
                                                  start,
                                                  end);
    TEST_ASSERT_NOT_NULL(events);
    TEST_ASSERT_TRUE(array_size(events) == 1u, "June 2012 includes one Venus transit");
    event = array_get(events, 0u);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_TRUE(almanac_solar_transit_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest),
                     "Venus transit greatest time is valid");
    TEST_ASSERT_TRUE(greatest.jd > 2456084.5 && greatest.jd < 2456084.7,
                     "Venus transit greatest phase is on 2012 June 6");

    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
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
        almanac_entry_t *entry;
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
        entry = almanac_new_body_entry(almanac, expected->body_id, moment);
        TEST_ASSERT_TRUE(entry != NULL, almanac_last_error(almanac));

        sha_error = fabs(angular_delta_degrees(almanac_entry_sha_degrees(entry), expected->sha_degrees));
        dec_error = fabs(almanac_entry_declination_degrees(entry) - expected->declination_degrees);
        sha_error_rounded = rounded_arcsecond_error(sha_error);
        dec_error_rounded = rounded_arcsecond_error(dec_error);
        navigation_grade = (sha_error_rounded <= NAVIGATION_GRADE_ARCSECONDS &&
                            dec_error_rounded <= NAVIGATION_GRADE_ARCSECONDS);
        printf("ORACLE %-7s %04d-%02d-%02d %02d:%02d:%05.2f local [%s]\n",
               expected->body_code,
               expected->year,
               expected->month,
               expected->day,
               expected->hour,
               expected->minute,
               expected->second,
               navigation_grade ? "PASS" : "FAIL");
        print_oracle_axis("SHA", expected->sha_degrees, almanac_entry_sha_degrees(entry), sha_error, sha_error_rounded);
        print_oracle_axis("Dec",
                          expected->declination_degrees,
                          almanac_entry_declination_degrees(entry),
                          dec_error,
                          dec_error_rounded);
        printf("    distance expected = %.12f AU\n", expected->geocentric_distance_au);
        printf("    distance got      = %.12f AU\n", almanac_entry_geocentric_distance_au(entry));
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

        TEST_ASSERT_TRUE(fabs(almanac_entry_geocentric_distance_au(entry) -
                              expected->geocentric_distance_au) < 0.02,
                         "geocentric distance is broadly consistent with SPICE");
        almanac_entry_dealloc(entry);
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
    datetime_t *start;
    datetime_t *end;
    almanac_t *almanac;
    array_t *events;
    const almanac_solar_eclipse_t *event;
    almanac_event_time_t greatest;
    almanac_event_time_t first;
    almanac_event_time_t fourth;
    datetime_t *greatest_time;
    datetime_t *first_time;
    datetime_t *fourth_time;
    almanac_solar_eclipse_kind_t event_kind;
    almanac_observer_t observer = {52.7073, -2.7540, 75.0};
    const char *kind;

    start = datetime_alloc();
    end = datetime_alloc();
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(start, 2026, DT_August, 1, 0, 0, 0.0));
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(end, 2026, DT_August, 20, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    events = almanac_find_solar_eclipses(almanac, &observer, start, end);
    TEST_ASSERT_NOT_NULL(events);

    printf("Shrewsbury eclipse watch\n");
    if (array_size(events) == 0u) {
        printf("No local solar eclipse found in August 2026.\n");
        array_destroy(events);
        almanac_close(almanac);
        datetime_dealloc(start);
        datetime_dealloc(end);
        return;
    }

    event = array_get(events, 0u);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_GREATEST, &greatest),
                     "Expected greatest eclipse time");
    TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FIRST_CONTACT, &first),
                     "Expected first contact time");
    TEST_ASSERT_TRUE(almanac_solar_eclipse_time(event, ALMANAC_EVENT_TIME_FOURTH_CONTACT, &fourth),
                     "Expected fourth contact time");
    greatest_time = datetime_from_event_time(&greatest);
    first_time = datetime_from_event_time(&first);
    fourth_time = datetime_from_event_time(&fourth);
    TEST_ASSERT_NOT_NULL(greatest_time);
    TEST_ASSERT_NOT_NULL(first_time);
    TEST_ASSERT_NOT_NULL(fourth_time);
    event_kind = almanac_solar_eclipse_kind(event);
    kind = event_kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ? "total" :
           event_kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR ? "annular" :
           "partial";
    printf("Local solar eclipse found in August 2026.\n");
    printf("Greatest local circumstance: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           datetime_year(greatest_time),
           (int)datetime_month(greatest_time),
           datetime_day(greatest_time),
           datetime_hour(greatest_time),
           datetime_minute(greatest_time),
           datetime_second(greatest_time));
    printf("Kind: %s\n", kind);
    printf("Magnitude: %.3f\n", almanac_solar_eclipse_magnitude(event));
    printf("Obscuration: %.1f%%\n", almanac_solar_eclipse_totality_percent(event));
    printf("First contact: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           datetime_year(first_time),
           (int)datetime_month(first_time),
           datetime_day(first_time),
           datetime_hour(first_time),
           datetime_minute(first_time),
           datetime_second(first_time));
    printf("Fourth contact: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           datetime_year(fourth_time),
           (int)datetime_month(fourth_time),
           datetime_day(fourth_time),
           datetime_hour(fourth_time),
           datetime_minute(fourth_time),
           datetime_second(fourth_time));

    datetime_dealloc(fourth_time);
    datetime_dealloc(first_time);
    datetime_dealloc(greatest_time);
    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
}

int tests_main(void)
{
    TEST_SECTION("Almanac");
    TEST_RUN_IN_GROUP(test_almanac_gha_aries_matches_j2000_reference, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_new_body_entry_resolves_sirius, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_new_body_entry_resolves_moon, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_new_body_entry_resolves_jupiter, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_observables_resolve_horizon_values, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_observables_validate_observer_range, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_body_geographical_position_resolves_gp, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_phase_details_resolve_planetary_phase, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_next_moon_phase_exact_finds_august_2026_new_moon, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_next_moon_phase_exact_advances_past_previous_phase, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_sunrise_sunset_returns_local_day_times, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_moonrise_moonset_returns_local_day_times, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_solar_eclipses_returns_august_2026_event, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_lunar_eclipses_returns_events_in_2025, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_lunar_eclipses_matches_july_2028_partial_event, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_solar_transits_returns_mercury_event, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_solar_transits_returns_venus_event, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_matches_oracle_across_supported_range, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_snapshot_returns_catalogue_order, tests, NULL);
    TEST_SECTION("README Output Examples");
    printf(C_BOLD C_YELLOW "Running README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_almanac_shrewsbury_eclipse_watch,
                                  readme_examples,
                                  "almanac,readme");
    return TEST_EXIT_CODE();
}
