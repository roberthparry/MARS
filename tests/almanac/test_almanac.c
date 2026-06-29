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
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_SIRIUS, moment, &entry), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(entry.body_id == ALMANAC_BODY_ID_SIRIUS, "Sirius body id is correct");
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
    TEST_ASSERT_TRUE(first->body_id == ALMANAC_BODY_ID_SUN, "snapshot starts with the Sun");

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
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_MOON, moment, &entry), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(entry.body_id == ALMANAC_BODY_ID_MOON, "Moon body id is correct");
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
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_JUPITER, moment, &entry), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(entry.body_id == ALMANAC_BODY_ID_JUPITER, "Jupiter body id is correct");
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

static void assert_observables_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t sun;
    almanac_entry_t moon;
    almanac_entry_t sirius;
    almanac_observer_t observer = { 52.7073, -2.7540, 0.0 };
    almanac_observables_t sun_obs;
    almanac_observables_t moon_obs;
    almanac_observables_t sirius_obs;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_August, 12, 17, 47, 5.8));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_SUN, moment, &sun), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_MOON, moment, &moon), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_SIRIUS, moment, &sirius), almanac_last_error(almanac));

    TEST_ASSERT_TRUE(almanac_observables(almanac, &sun, &observer, &sun_obs), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_observables(almanac, &moon, &observer, &moon_obs), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_observables(almanac, &sirius, &observer, &sirius_obs), almanac_last_error(almanac));

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
    almanac_entry_t sun;
    almanac_observer_t invalid_observer = { 123.0, 0.0, 0.0 };
    almanac_observables_t observables;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_August, 12, 17, 47, 5.8));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_SUN, moment, &sun), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(!almanac_observables(almanac, &sun, &invalid_observer, &observables),
                     "invalid observer latitude is rejected");
    TEST_ASSERT_TRUE(strcmp(almanac_last_error(almanac), "observer latitude must be in [-90, 90]") == 0,
                     "invalid observer latitude sets a helpful error");

    almanac_close(almanac);
    datetime_dealloc(moment);
}

static void test_almanac_observables_validate_observer_range(void)
{
    assert_observables_validation_case();
}

static void assert_phase_details_case(void)
{
    datetime_t *moment;
    almanac_t *almanac;
    almanac_entry_t venus;
    almanac_entry_t jupiter;
    almanac_phase_details_t venus_phase;
    almanac_phase_details_t jupiter_phase;

    moment = datetime_alloc();
    TEST_ASSERT_NOT_NULL(moment);
    TEST_ASSERT_NOT_NULL(datetime_init_ymdt(moment, 2026, DT_June, 27, 0, 0, 0.0));

    almanac = almanac_open();
    TEST_ASSERT_NOT_NULL(almanac);
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_VENUS, moment, &venus), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_lookup_body(almanac, ALMANAC_BODY_ID_JUPITER, moment, &jupiter), almanac_last_error(almanac));
    TEST_ASSERT_TRUE(almanac_phase_details(&venus, &venus_phase), "Venus phase details resolve");
    TEST_ASSERT_TRUE(almanac_phase_details(&jupiter, &jupiter_phase), "Jupiter phase details resolve");

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
    TEST_ASSERT_TRUE(phase_event.moment_jd > datetime_jd(after),
                     "next new Moon occurs after the starting date");
    TEST_ASSERT_TRUE(phase_event.moment_jd > 2461264.0 &&
                     phase_event.moment_jd < 2461266.5,
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

static void assert_sunrise_sunset_case(void)
{
    datetime_t *date;
    almanac_t *almanac;
    jurisdiction_t *jurisdiction;
    almanac_sun_times_t sun_times;
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

    printf("    Shrewsbury 2026-08-12 sunrise %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           sun_times.sunrise.local_time.hour,
           sun_times.sunrise.local_time.minute,
           sun_times.sunrise.local_time.second,
           sun_times.sunrise.azimuth_degrees);
    printf("    Shrewsbury 2026-08-12 sunset  %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           sun_times.sunset.local_time.hour,
           sun_times.sunset.local_time.minute,
           sun_times.sunset.local_time.second,
           sun_times.sunset.azimuth_degrees);

    TEST_ASSERT_TRUE(sun_times.sunrise.status == ALMANAC_RISE_SET_OK, "sunrise occurs locally");
    TEST_ASSERT_TRUE(sun_times.sunset.status == ALMANAC_RISE_SET_OK, "sunset occurs locally");
    TEST_ASSERT_TRUE(sun_times.sunrise.local_time.valid, "sunrise local time is valid");
    TEST_ASSERT_TRUE(sun_times.sunset.local_time.valid, "sunset local time is valid");
    TEST_ASSERT_TRUE(sun_times.sunrise.local_time.year == 2026 &&
                     sun_times.sunrise.local_time.month == DT_August &&
                     sun_times.sunrise.local_time.day == 12,
                     "sunrise is returned on the requested local date");
    TEST_ASSERT_TRUE(sun_times.sunset.local_time.year == 2026 &&
                     sun_times.sunset.local_time.month == DT_August &&
                     sun_times.sunset.local_time.day == 12,
                     "sunset is returned on the requested local date");
    TEST_ASSERT_TRUE(sun_times.sunrise.local_time.hour >= 5 &&
                     sun_times.sunrise.local_time.hour <= 7,
                     "sunrise local hour is plausible for Shrewsbury in August");
    TEST_ASSERT_TRUE(sun_times.sunset.local_time.hour >= 20 &&
                     sun_times.sunset.local_time.hour <= 21,
                     "sunset local hour is plausible for Shrewsbury in August");
    TEST_ASSERT_TRUE(sun_times.sunrise.azimuth_degrees >= 45.0 &&
                     sun_times.sunrise.azimuth_degrees <= 90.0,
                     "sunrise azimuth is north-east/east in August");
    TEST_ASSERT_TRUE(sun_times.sunset.azimuth_degrees >= 270.0 &&
                     sun_times.sunset.azimuth_degrees <= 315.0,
                     "sunset azimuth is west/north-west in August");

    jurisdict_close(jurisdiction);
    almanac_close(almanac);
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

    printf("    Shrewsbury 2026-08-12 moonrise %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           moon_times.moonrise.local_time.hour,
           moon_times.moonrise.local_time.minute,
           moon_times.moonrise.local_time.second,
           moon_times.moonrise.azimuth_degrees);
    printf("    Shrewsbury 2026-08-12 moonset  %02u:%02u:%04.1f local, azimuth %.1f deg\n",
           moon_times.moonset.local_time.hour,
           moon_times.moonset.local_time.minute,
           moon_times.moonset.local_time.second,
           moon_times.moonset.azimuth_degrees);

    TEST_ASSERT_TRUE(moon_times.moonrise.status == ALMANAC_RISE_SET_OK, "moonrise occurs locally");
    TEST_ASSERT_TRUE(moon_times.moonset.status == ALMANAC_RISE_SET_OK, "moonset occurs locally");
    TEST_ASSERT_TRUE(moon_times.moonrise.local_time.valid, "moonrise local time is valid");
    TEST_ASSERT_TRUE(moon_times.moonset.local_time.valid, "moonset local time is valid");
    TEST_ASSERT_TRUE(moon_times.moonrise.local_time.year == 2026 &&
                     moon_times.moonrise.local_time.month == DT_August &&
                     moon_times.moonrise.local_time.day == 12,
                     "moonrise is returned on the requested local date");
    TEST_ASSERT_TRUE(moon_times.moonset.local_time.year == 2026 &&
                     moon_times.moonset.local_time.month == DT_August &&
                     moon_times.moonset.local_time.day == 12,
                     "moonset is returned on the requested local date");
    TEST_ASSERT_TRUE(moon_times.moonrise.jd < moon_times.moonset.jd,
                     "moonrise precedes moonset near the August 2026 new Moon");
    TEST_ASSERT_TRUE(moon_times.moonrise.azimuth_degrees >= 45.0 &&
                     moon_times.moonrise.azimuth_degrees <= 120.0,
                     "moonrise azimuth is plausible near new Moon");
    TEST_ASSERT_TRUE(moon_times.moonset.azimuth_degrees >= 240.0 &&
                     moon_times.moonset.azimuth_degrees <= 315.0,
                     "moonset azimuth is plausible near new Moon");

    jurisdict_close(jurisdiction);
    almanac_close(almanac);
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
    TEST_ASSERT_TRUE(event->kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ||
                     event->kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR ||
                     event->kind == ALMANAC_SOLAR_ECLIPSE_PARTIAL,
                     "solar eclipse kind is classified");
    TEST_ASSERT_TRUE(event->magnitude > 0.0, "solar eclipse magnitude is positive");
    TEST_ASSERT_TRUE(event->totality_percent > 0.0 &&
                     event->totality_percent <= 100.0,
                     "solar eclipse totality percentage is bounded");
    TEST_ASSERT_TRUE(event->first_contact_jd == event->first_contact_jd &&
                     event->first_contact_jd < event->greatest_eclipse_jd,
                     "solar eclipse first contact precedes greatest eclipse");
    TEST_ASSERT_TRUE(event->fourth_contact_jd == event->fourth_contact_jd &&
                     event->fourth_contact_jd > event->greatest_eclipse_jd,
                     "solar eclipse fourth contact follows greatest eclipse");
    TEST_ASSERT_TRUE(event->first_contact_time.valid, "solar eclipse first contact local time is valid");
    TEST_ASSERT_TRUE(event->greatest_eclipse_time.valid, "solar eclipse greatest local time is valid");
    TEST_ASSERT_TRUE(event->fourth_contact_time.valid, "solar eclipse fourth contact local time is valid");
    if (event->kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ||
        event->kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR) {
        TEST_ASSERT_TRUE(event->second_contact_jd == event->second_contact_jd &&
                         event->second_contact_jd < event->greatest_eclipse_jd,
                         "solar eclipse second contact precedes greatest eclipse");
        TEST_ASSERT_TRUE(event->third_contact_jd == event->third_contact_jd &&
                         event->third_contact_jd > event->greatest_eclipse_jd,
                         "solar eclipse third contact follows greatest eclipse");
        TEST_ASSERT_TRUE(event->second_contact_time.valid, "solar eclipse second contact local time is valid");
        TEST_ASSERT_TRUE(event->third_contact_time.valid, "solar eclipse third contact local time is valid");
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
    TEST_ASSERT_TRUE(event->penumbral_magnitude > 0.0, "lunar eclipse penumbral magnitude is positive");
    TEST_ASSERT_TRUE(event->totality_percent >= 0.0 &&
                     event->totality_percent <= 100.0,
                     "lunar eclipse totality percentage is bounded");
    TEST_ASSERT_TRUE(event->p1_contact_jd == event->p1_contact_jd &&
                     event->p1_contact_jd < event->greatest_eclipse_jd,
                     "lunar eclipse P1 contact precedes greatest eclipse");
    TEST_ASSERT_TRUE(event->p4_contact_jd == event->p4_contact_jd &&
                     event->p4_contact_jd > event->greatest_eclipse_jd,
                     "lunar eclipse P4 contact follows greatest eclipse");
    TEST_ASSERT_TRUE(event->p1_contact_time.valid, "lunar eclipse P1 local time is valid");
    TEST_ASSERT_TRUE(event->greatest_eclipse_time.valid, "lunar eclipse greatest local time is valid");
    TEST_ASSERT_TRUE(event->p4_contact_time.valid, "lunar eclipse P4 local time is valid");
    if (event->kind == ALMANAC_LUNAR_ECLIPSE_PARTIAL ||
        event->kind == ALMANAC_LUNAR_ECLIPSE_TOTAL) {
        TEST_ASSERT_TRUE(event->u1_contact_jd == event->u1_contact_jd &&
                         event->u1_contact_jd < event->greatest_eclipse_jd,
                         "lunar eclipse U1 contact precedes greatest eclipse");
        TEST_ASSERT_TRUE(event->u4_contact_jd == event->u4_contact_jd &&
                         event->u4_contact_jd > event->greatest_eclipse_jd,
                         "lunar eclipse U4 contact follows greatest eclipse");
        TEST_ASSERT_TRUE(event->u1_contact_time.valid, "lunar eclipse U1 local time is valid");
        TEST_ASSERT_TRUE(event->u4_contact_time.valid, "lunar eclipse U4 local time is valid");
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

static void assert_solar_transit_search_case(void)
{
    datetime_t *start;
    datetime_t *end;
    almanac_t *almanac;
    array_t *events;
    const almanac_solar_transit_t *event;
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
    TEST_ASSERT_TRUE(event->body_id == ALMANAC_BODY_ID_MERCURY, "Transit body id is Mercury");
    TEST_ASSERT_TRUE(event->separation_degrees < event->solar_semi_diameter_degrees + event->planet_semi_diameter_degrees,
                     "Mercury transit occurs on the solar disc");
    TEST_ASSERT_TRUE(event->first_contact_jd == event->first_contact_jd &&
                     event->first_contact_jd < event->greatest_transit_jd,
                     "solar transit first contact precedes greatest transit");
    TEST_ASSERT_TRUE(event->second_contact_jd == event->second_contact_jd &&
                     event->second_contact_jd < event->greatest_transit_jd,
                     "solar transit second contact precedes greatest transit");
    TEST_ASSERT_TRUE(event->third_contact_jd == event->third_contact_jd &&
                     event->third_contact_jd > event->greatest_transit_jd,
                     "solar transit third contact follows greatest transit");
    TEST_ASSERT_TRUE(event->fourth_contact_jd == event->fourth_contact_jd &&
                     event->fourth_contact_jd > event->greatest_transit_jd,
                     "solar transit fourth contact follows greatest transit");
    TEST_ASSERT_TRUE(event->first_contact_time.valid, "solar transit first contact local time is valid");
    TEST_ASSERT_TRUE(event->second_contact_time.valid, "solar transit second contact local time is valid");
    TEST_ASSERT_TRUE(event->greatest_transit_time.valid, "solar transit greatest local time is valid");
    TEST_ASSERT_TRUE(event->third_contact_time.valid, "solar transit third contact local time is valid");
    TEST_ASSERT_TRUE(event->fourth_contact_time.valid, "solar transit fourth contact local time is valid");

    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
}

static void test_almanac_find_solar_transits_returns_mercury_event(void)
{
    assert_solar_transit_search_case();
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
        TEST_ASSERT_TRUE(almanac_lookup_body(almanac, expected->body_id, moment, &entry), almanac_last_error(almanac));

        sha_error = fabs(angular_delta_degrees(entry.sha_degrees, expected->sha_degrees));
        dec_error = fabs(entry.declination_degrees - expected->declination_degrees);
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
    datetime_t *start;
    datetime_t *end;
    almanac_t *almanac;
    array_t *events;
    const almanac_solar_eclipse_t *event;
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
    kind = event->kind == ALMANAC_SOLAR_ECLIPSE_TOTAL ? "total" :
           event->kind == ALMANAC_SOLAR_ECLIPSE_ANNULAR ? "annular" :
           "partial";
    printf("Local solar eclipse found in August 2026.\n");
    printf("Greatest local circumstance: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           event->greatest_eclipse_time.year,
           (int)event->greatest_eclipse_time.month,
           event->greatest_eclipse_time.day,
           event->greatest_eclipse_time.hour,
           event->greatest_eclipse_time.minute,
           event->greatest_eclipse_time.second);
    printf("Kind: %s\n", kind);
    printf("Magnitude: %.3f\n", event->magnitude);
    printf("Obscuration: %.1f%%\n", event->totality_percent);
    printf("First contact: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           event->first_contact_time.year,
           (int)event->first_contact_time.month,
           event->first_contact_time.day,
           event->first_contact_time.hour,
           event->first_contact_time.minute,
           event->first_contact_time.second);
    printf("Fourth contact: %04d-%02d-%02d %02u:%02u:%04.1f local time\n",
           event->fourth_contact_time.year,
           (int)event->fourth_contact_time.month,
           event->fourth_contact_time.day,
           event->fourth_contact_time.hour,
           event->fourth_contact_time.minute,
           event->fourth_contact_time.second);

    array_destroy(events);
    almanac_close(almanac);
    datetime_dealloc(start);
    datetime_dealloc(end);
}

int tests_main(void)
{
    TEST_SECTION("Almanac");
    TEST_RUN_IN_GROUP(test_almanac_gha_aries_matches_j2000_reference, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_sirius, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_moon, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_lookup_resolves_jupiter, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_observables_resolve_horizon_values, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_observables_validate_observer_range, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_phase_details_resolve_planetary_phase, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_next_moon_phase_exact_finds_august_2026_new_moon, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_sunrise_sunset_returns_local_day_times, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_moonrise_moonset_returns_local_day_times, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_solar_eclipses_returns_august_2026_event, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_lunar_eclipses_returns_events_in_2025, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_find_solar_transits_returns_mercury_event, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_matches_oracle_across_supported_range, tests, NULL);
    TEST_RUN_IN_GROUP(test_almanac_snapshot_returns_catalogue_order, tests, NULL);
    TEST_SECTION("README Output Examples");
    printf(C_BOLD C_YELLOW "Running README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_almanac_shrewsbury_eclipse_watch,
                                  readme_examples,
                                  "almanac,readme");
    return TEST_EXIT_CODE();
}
