/**
 * @file test_almanac_readme.c
 * @brief Runnable README programmes for observer-relative sky positions and lunar circumstances.
 *
 * Keeps the London body table, bright-star catalogue query and Sydney Moon report synchronised with
 * docs/almanac.md. The ordinary almanac suite loads the configured ephemeris before calling these
 * examples last. Each programme releases its engine, date, entries and snapshots on every exit path;
 * the standalone Markdown runner also checks the documented numerical output.
 */
#include <math.h>
#include <stdio.h>

#include "format-t.h"
#include "almanac-t.h"
#include "array-t.h"
#include "datetime-t.h"
#include "test_harness.h"
#define MARS_TEST_ALMANAC_INTERNAL_ACCESS
#include "test_almanac_readme.h"

/* README programme: Sun, Moon, planets and named stars. */
static int almanac_readme_sky_positions(void)
{
    const almanac_observer_t observer = {
        .latitude_degrees = 51.5074, .longitude_degrees = -0.1278, .elevation_metres = 11.0
    };
    const almanac_body_id_t bodies[] = {
        ALMANAC_BODY_ID_SUN, ALMANAC_BODY_ID_MOON,
        ALMANAC_BODY_ID_MERCURY, ALMANAC_BODY_ID_VENUS, ALMANAC_BODY_ID_MARS,
        ALMANAC_BODY_ID_JUPITER, ALMANAC_BODY_ID_SATURN,
        ALMANAC_BODY_ID_SIRIUS, ALMANAC_BODY_ID_POLARIS, ALMANAC_BODY_ID_VEGA
    };
    almanac_t *almanac = almanac_open();
    datetime_t *moment = datetime_init_ymdt(datetime_alloc(), 2026, DT_January, 3, 21, 0, 0.0);
    int status = 1;
    if (!almanac || !moment)
        goto done;
    if (fmt_print("London sky at {moment:yyyy-MM-dd hh:mm:ss} UTC\n"
                  "Latitude {latitude:.4f}°, longitude {longitude:.4f}°, elevation {elevation:.0f} m\n"
                  "Body         Alt (°)   Az (°)  Centre\n", FMT(moment),
                  FMT(latitude, observer.latitude_degrees), FMT(longitude, observer.longitude_degrees),
                  FMT(elevation, observer.elevation_metres)) < 0)
        goto done;
    for (size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i) {
        almanac_entry_t *entry = almanac_new_body_entry(almanac, bodies[i], moment);
        almanac_observables_t sky;
        bool ok = entry && almanac_observables(almanac, entry, &observer, &sky);
        almanac_entry_dealloc(entry);
        if (!ok) {
            fmt_fprint(stderr, "{name}: {error}\n", FMT(name, almanac_body_display_name(bodies[i])),
                       FMT(error, almanac_last_error(almanac)));
            goto done;
        }
        if (fmt_print("{name:<10} {altitude:>8.2f} {azimuth:>8.2f}  {horizon}\n",
                      FMT(name, almanac_body_display_name(bodies[i])), FMT(altitude, sky.altitude_degrees),
                      FMT(azimuth, sky.azimuth_degrees), FMT(horizon, sky.above_horizon ? "above" : "below")) < 0)
            goto done;
    }
    status = 0;
done:
    almanac_close(almanac);
    datetime_dealloc(moment);
    return status;
}

/* Run the documented sky_positions programme with the configured catalogue. */
void example_almanac_sky_positions(void)
{
    TEST_ASSERT_INT_EQ(almanac_readme_sky_positions(), 0);
}

/* README programme: Find bright stars above the local horizon. */
static int almanac_readme_bright_stars(void)
{
    const almanac_observer_t observer = {51.5074, -0.1278, 11.0};
    almanac_t *almanac = almanac_open();
    datetime_t *moment = datetime_init_ymdt(datetime_alloc(), 2026, DT_January, 3, 21, 0, 0.0);
    array_t *catalogue = almanac && moment ? almanac_snapshot(almanac, moment) : NULL;
    int status = 1;
    if (!catalogue)
        goto done;
    if (fmt_print("London: stars of magnitude 1.0 or brighter above the horizon\n"
                  "{moment:yyyy-MM-dd hh:mm:ss} UTC\n"
                  "Star        Magnitude   Alt (°)   Az (°)\n", FMT(moment)) < 0)
        goto done;
    for (size_t i = 0; i < array_size(catalogue); ++i) {
        const almanac_entry_t *entry = array_get(catalogue, i);
        if (almanac_entry_body_kind(entry) != ALMANAC_BODY_STAR)
            continue;
        double magnitude = almanac_entry_visual_magnitude(entry);
        if (!isfinite(magnitude) || magnitude > 1.0)
            continue;
        almanac_observables_t sky;
        if (!almanac_observables(almanac, entry, &observer, &sky))
            goto done;
        if (!sky.above_horizon)
            continue;
        if (fmt_print("{name:<10} {magnitude:>9.2f} {altitude:>9.2f} {azimuth:>8.2f}\n",
                      FMT(name, almanac_body_display_name(almanac_entry_body_id(entry))), FMT(magnitude),
                      FMT(altitude, sky.altitude_degrees), FMT(azimuth, sky.azimuth_degrees)) < 0)
            goto done;
    }
    status = 0;
done:
    array_destroy(catalogue);
    datetime_dealloc(moment);
    almanac_close(almanac);
    return status;
}

/* Run the documented bright_stars programme with the configured catalogue. */
void example_almanac_bright_stars(void)
{
    TEST_ASSERT_INT_EQ(almanac_readme_bright_stars(), 0);
}

/* README programme: Moon position, phase, size and atmospheric refraction. */
static int almanac_readme_moon_details(void)
{
    const almanac_observer_t observer = {-33.8688, 151.2093, 58.0};
    almanac_t *almanac = almanac_open();
    datetime_t *moment = datetime_init_ymdt(datetime_alloc(), 2026, DT_January, 3, 12, 0, 0.0);
    almanac_entry_t *moon = almanac && moment ? almanac_new_body_entry(almanac, ALMANAC_BODY_ID_MOON, moment) : NULL;
    almanac_observables_t sky;
    almanac_phase_details_t phase;
    int status = 1;
    if (!moon || !almanac_observables(almanac, moon, &observer, &sky) || !almanac_phase_details(moon, &phase))
        goto done;
    double refraction = almanac_refraction_degrees(sky.altitude_degrees, 18.0, 1010.0);
    if (!isfinite(refraction))
        goto done;
    status = fmt_print("Moon from Sydney at {moment:yyyy-MM-dd hh:mm:ss} UTC\n"
                       "Latitude {latitude:.4f}°, longitude {longitude:.4f}°, elevation {elevation:.0f} m\n"
                       "Geometric altitude: {altitude:.3f}°; azimuth: {azimuth:.3f}°\n"
                       "Refraction at 18 °C, 1010 hPa: {refraction:.4f}°; adjusted altitude: {adjusted:.3f}°\n"
                       "Right ascension: {ra:.6f} h; declination: {declination:.6f}°\n"
                       "Geocentric distance: {distance:.0f} km; angular diameter: {diameter:.2f} arcmin\n"
                       "Illuminated: {illuminated:.2f}%; phase angle: {angle:.2f}°\n",
                       FMT(moment), FMT(latitude, observer.latitude_degrees),
                       FMT(longitude, observer.longitude_degrees), FMT(elevation, observer.elevation_metres),
                       FMT(altitude, sky.altitude_degrees), FMT(azimuth, sky.azimuth_degrees), FMT(refraction),
                       FMT(adjusted, sky.altitude_degrees + refraction),
                       FMT(ra, almanac_entry_right_ascension_hours(moon)),
                       FMT(declination, almanac_entry_declination_degrees(moon)),
                       FMT(distance, almanac_entry_geocentric_distance_au(moon) * 149597870.7),
                       FMT(diameter, sky.semi_diameter_degrees * 120.0),
                       FMT(illuminated, phase.illuminated_fraction * 100.0), FMT(angle, phase.phase_angle_degrees)) < 0;
done:
    almanac_entry_dealloc(moon);
    datetime_dealloc(moment);
    almanac_close(almanac);
    return status;
}

/* Run the documented moon_details programme with the configured catalogue. */
void example_almanac_moon_details(void)
{
    TEST_ASSERT_INT_EQ(almanac_readme_moon_details(), 0);
}
