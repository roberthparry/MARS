/**
 * @file almanac_totality_screen.c
 * @brief Bounded, event-local candidate screening for named land-totality searches.
 *
 * Resolves apparent Sun/Moon vectors and Earth rotation once per two-minute sample,
 * then reuses Earth-fixed vectors for every catalogue observer. A broad 0.1-degree
 * screen precedes a chord-interpolated separation check with a 0.001-degree margin. This is a
 * sampled shortlist, not a proof of totality or an exhaustive land search: callers
 * must refine the local maximum with the native ephemeris and check the shoreline.
 * Storage is about 17 KiB per event, independent of the number of towns, and is
 * released after the search. No interpolated position is presented as a result.
 */
#include <float.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_ALMANAC_INTERNAL_ACCESS
#include "almanac_engine_internal.h"
#define MARS_ALMANAC_INTERNAL_ACCESS
#include "almanac_internal.h"

enum { ALMANAC_TOTALITY_SAMPLES = 361 };

typedef struct _almanac_totality_screen_t {
    cartesian3_t sun[ALMANAC_TOTALITY_SAMPLES];
    cartesian3_t moon[ALMANAC_TOTALITY_SAMPLES];
    double cosine_squared;
} almanac_totality_screen_t;

/* Prepare fixed-size screening data without modifying or retaining the catalogue. */
almanac_totality_screen_t *almanac_totality_screen_new(almanac_t *almanac, const almanac_solar_eclipse_t *eclipse)
{
    if (!almanac || !eclipse || !isfinite(eclipse->greatest_eclipse.jd))
        return NULL;
    almanac_totality_screen_t *screen = calloc(1, sizeof(*screen));
    if (!screen)
        return NULL;
    double cosine = cos(almanac_degrees_to_radians(0.1));
    screen->cosine_squared = cosine * cosine;
    for (unsigned i = 0; i < ALMANAC_TOTALITY_SAMPLES; ++i) {
        double jd = eclipse->greatest_eclipse.jd - 0.25 + i / 720.0;
        almanac_state_t sun, moon;
        if (!computation_step_current("almanac.totality.screen") ||
            !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_SUN, jd, &sun) ||
            !almanac_state_for_body_at_jd(almanac, ALMANAC_BODY_ID_MOON, jd, &moon)) {
            free(screen);
            return NULL;
        }
        double gha = almanac_apparent_gha_aries_for_jd(almanac, jd);
        if (!isfinite(gha) || gha == DBL_MAX) {
            free(screen);
            return NULL;
        }
        double rotation = -almanac_degrees_to_radians(gha);
        screen->sun[i] = cartesian_rotate_z(&sun.geocentric_equatorial_au, rotation);
        screen->moon[i] = cartesian_rotate_z(&moon.geocentric_equatorial_au, rotation);
    }
    return screen;
}

/* Release all event-local samples. */
void almanac_totality_screen_free(almanac_totality_screen_t *screen)
{
    free(screen);
}

static bool screen_near_totality(const almanac_totality_screen_t *screen, const cartesian3_t *position)
{
    cartesian3_t previous = {0};
    double previous_radius = 0;
    for (unsigned i = 0; i < ALMANAC_TOTALITY_SAMPLES; ++i) {
        cartesian3_t sun = cartesian_subtract(&screen->sun[i], position);
        cartesian3_t moon = cartesian_subtract(&screen->moon[i], position);
        double sun_distance = sqrt(cartesian_dot(&sun, &sun));
        double moon_distance = sqrt(cartesian_dot(&moon, &moon));
        double radius = almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_MOON, moon_distance) -
                        almanac_body_semi_diameter_from_distance_degrees(ALMANAC_BODY_ID_SUN, sun_distance);
        cartesian3_t difference = {moon.x / moon_distance - sun.x / sun_distance,
                                  moon.y / moon_distance - sun.y / sun_distance,
                                  moon.z / moon_distance - sun.z / sun_distance};
        if (i) {
            cartesian3_t step = cartesian_subtract(&difference, &previous);
            double length_squared = cartesian_dot(&step, &step);
            double fraction = length_squared > 0 ? -cartesian_dot(&previous, &step) / length_squared : 0;
            fraction = fmax(0, fmin(1, fraction));
            cartesian3_t closest = {previous.x + fraction * step.x, previous.y + fraction * step.y,
                                   previous.z + fraction * step.z};
            /* Interpolation only rejects clear misses. Exact ephemeris refinement supplies every result. */
            double margin = almanac_degrees_to_radians(fmax(radius, previous_radius) + 0.001);
            if (margin > 0 && cartesian_dot(&closest, &closest) <= margin * margin)
                return true;
        }
        previous = difference;
        previous_radius = radius;
    }
    return false;
}

/* Scan the small fixed time grid, not the town catalogue, using squared dot products instead of acos. */
bool almanac_totality_screen_candidate(almanac_t *almanac, const almanac_totality_screen_t *screen,
                                       const almanac_observer_t *observer, bool *possible)
{
    if (possible)
        *possible = false;
    if (!screen || !possible || !computation_step_current("almanac.totality.screen"))
        return false;
    cartesian3_t position;
    if (!almanac_observer_position_au(almanac, observer, 0, &position))
        return false;
    for (unsigned i = 0; i < ALMANAC_TOTALITY_SAMPLES; ++i) {
        cartesian3_t sun = cartesian_subtract(&screen->sun[i], &position);
        cartesian3_t moon = cartesian_subtract(&screen->moon[i], &position);
        double dot = cartesian_dot(&sun, &moon);
        double norm = cartesian_dot(&sun, &sun) * cartesian_dot(&moon, &moon);
        if (norm > 0 && dot > 0 && dot * dot >= screen->cosine_squared * norm) {
            *possible = screen_near_totality(screen, &position);
            break;
        }
    }
    return true;
}
