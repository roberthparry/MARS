/**
 * @file almanac_geometry.c
 * @brief Angular conventions and observer-relative horizon geometry.
 *
 * Normalises angles and calculates topocentric vectors, altitude and horizontal coordinates. Use these helpers to
 * connect celestial states to a specified observer rather than duplicate coordinate conversions in individual
 * event finders.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Angular helpers and observer-relative horizon geometry. */
#include <float.h>
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"

double almanac_normalize_degrees(double degrees)
{
    double value = fmod(degrees, 360.0);

    if (value < 0.0)
        value += 360.0;
    return value;
}

double almanac_normalize_degrees_signed(double degrees)
{
    double value = fmod(degrees + 180.0, 360.0);

    if (value < 0.0)
        value += 360.0;
    return value - 180.0;
}

double almanac_degrees_to_radians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

double almanac_radians_to_degrees(double radians)
{
    return radians * (180.0 / M_PI);
}

double almanac_clamp_unit(double value)
{
    if (value < -1.0)
        return -1.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

double almanac_disc_coverage_percent(double target_radius, double covering_radius, double centre_separation)
{
    double d;
    double target_area;
    double overlap_area;
    double term;

    if (!(target_radius > 0.0) || !(centre_separation >= 0.0))
        return NAN;
    if (!(covering_radius > 0.0))
        return 0.0;

    d = fabs(centre_separation);
    target_area = M_PI * target_radius * target_radius;
    if (d >= target_radius + covering_radius)
        return 0.0;
    if (d <= fabs(target_radius - covering_radius)) {
        overlap_area = M_PI * fmin(target_radius, covering_radius) * fmin(target_radius, covering_radius);
        return 100.0 * overlap_area / target_area;
    }

    term = (-d + target_radius + covering_radius) * (d + target_radius - covering_radius) *
           (d - target_radius + covering_radius) * (d + target_radius + covering_radius);
    overlap_area = target_radius * target_radius *
                       acos(almanac_clamp_unit((d * d + target_radius * target_radius - covering_radius * covering_radius) /
                                       (2.0 * d * target_radius))) +
                   covering_radius * covering_radius *
                       acos(almanac_clamp_unit((d * d + covering_radius * covering_radius - target_radius * target_radius) /
                                       (2.0 * d * covering_radius))) -
                   0.5 * sqrt(fmax(0.0, term));

    return 100.0 * overlap_area / target_area;
}

static double normalize_radians_positive(double radians)
{
    double value = fmod(radians, 2.0 * M_PI);

    if (value < 0.0)
        value += 2.0 * M_PI;
    return value;
}

static bool almanac_body_id_radius_au(almanac_body_id_t body_id, double *out_radius_au)
{
    static const double AU_PER_KM = 1.0 / 149597870.7;
    static const double radius_km_by_body_id[ALMANAC_BODY_ID_COUNT] = {
        [ALMANAC_BODY_ID_UNKNOWN] = NAN,
        [ALMANAC_BODY_ID_SUN]     = 695700.0,
        [ALMANAC_BODY_ID_MOON]    = 1737.4,
        [ALMANAC_BODY_ID_MERCURY] = 2439.7,
        [ALMANAC_BODY_ID_VENUS]   = 6051.8,
        [ALMANAC_BODY_ID_MARS]    = 3389.5,
        [ALMANAC_BODY_ID_JUPITER] = 69911.0,
        [ALMANAC_BODY_ID_SATURN]  = 58232.0,
        [ALMANAC_BODY_ID_URANUS]  = 25362.0,
        [ALMANAC_BODY_ID_NEPTUNE] = 24622.0
    };
    double radius_km;

    if (!out_radius_au)
        return false;

    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return false;

    radius_km = radius_km_by_body_id[body_id];
    if (!(radius_km > 0.0))
        return false;
    *out_radius_au = radius_km * AU_PER_KM;
    return true;
}

static bool almanac_body_radius_au(const almanac_entry_t *body, double *out_radius_au)
{
    if (!body)
        return false;
    return almanac_body_id_radius_au(body->body_id, out_radius_au);
}

double almanac_body_semi_diameter_from_distance_degrees(almanac_body_id_t body_id, double distance_au)
{
    double radius_au;

    if (!(distance_au > 0.0))
        return NAN;
    if (!almanac_body_id_radius_au(body_id, &radius_au) || !(radius_au > 0.0))
        return NAN;
    if (distance_au < radius_au)
        return NAN;
    return almanac_radians_to_degrees(asin(almanac_clamp_unit(radius_au / distance_au)));
}

double almanac_angular_separation_degrees(const cartesian3_t *a, const cartesian3_t *b)
{
    double len_a;
    double len_b;
    double cosine_angle;

    if (!a || !b)
        return NAN;
    len_a = cartesian_length(a);
    len_b = cartesian_length(b);
    if (len_a <= 0.0 || len_b <= 0.0)
        return NAN;
    cosine_angle = cartesian_dot(a, b) / (len_a * len_b);
    return almanac_radians_to_degrees(acos(almanac_clamp_unit(cosine_angle)));
}

double almanac_ecliptic_longitude_degrees(const cartesian3_t *vector)
{
    if (!vector)
        return NAN;
    return almanac_normalize_degrees(almanac_radians_to_degrees(atan2(vector->y, vector->x)));
}

bool almanac_observer_is_valid(almanac_t *almanac, const almanac_observer_t *observer)
{
    if (!observer) {
        almanac_set_error(almanac, "invalid almanac observer");
        return false;
    }
    if (!isfinite(observer->latitude_degrees) || !isfinite(observer->longitude_degrees) ||
        !isfinite(observer->elevation_metres)) {
        almanac_set_error(almanac, "observer coordinates must be finite");
        return false;
    }
    if (observer->latitude_degrees < -90.0 || observer->latitude_degrees > 90.0) {
        almanac_set_error(almanac, "observer latitude must be in [-90, 90]");
        return false;
    }
    if (observer->longitude_degrees < -360.0 || observer->longitude_degrees > 360.0) {
        almanac_set_error(almanac, "observer longitude must be in [-360, 360]");
        return false;
    }
    return true;
}

/* Derive observer-relative observables from one computed almanac entry. */
bool almanac_observables(almanac_t *almanac, const almanac_entry_t *body, const almanac_observer_t *observer,
    almanac_observables_t *out)
{
    double gha_body_degrees;
    double lha_radians;
    double latitude_radians;
    double declination_radians;
    double sin_altitude;
    double altitude_radians;
    double azimuth_radians;
    double radius_au;
    double semi_diameter_radians = NAN;
    almanac_horizon_geometry_t geometry;

    if (!almanac || !body || !observer || !out) {
        almanac_set_error(almanac, "invalid almanac observables request");
        return false;
    }
    if (!almanac_observer_is_valid(almanac, observer))
        return false;

    if (body->moment_jd > 0.0 && isfinite(body->moment_jd) && almanac_body_id_radius_au(body->body_id, &radius_au) &&
        almanac_body_horizon_geometry(almanac, body->body_id, observer, body->moment_jd, &geometry)) {
        memset(out, 0, sizeof(*out));
        out->altitude_degrees = geometry.altitude_degrees;
        out->azimuth_degrees = geometry.azimuth_degrees;
        out->semi_diameter_degrees = geometry.semi_diameter_degrees;
        out->above_horizon = out->altitude_degrees > 0.0;
        out->visible =
            out->altitude_degrees +
                (out->semi_diameter_degrees == out->semi_diameter_degrees ? out->semi_diameter_degrees : 0.0) >
            0.0;
        return true;
    }

    gha_body_degrees = almanac_normalize_degrees(body->gha_aries_degrees + body->sha_degrees);
    lha_radians = almanac_degrees_to_radians(almanac_normalize_degrees(gha_body_degrees + observer->longitude_degrees));
    latitude_radians = almanac_degrees_to_radians(observer->latitude_degrees);
    declination_radians = almanac_degrees_to_radians(body->declination_degrees);

    sin_altitude = sin(latitude_radians) * sin(declination_radians) +
                   cos(latitude_radians) * cos(declination_radians) * cos(lha_radians);
    altitude_radians = asin(almanac_clamp_unit(sin_altitude));
    azimuth_radians = atan2(sin(lha_radians), cos(lha_radians) * sin(latitude_radians) -
                                                  tan(declination_radians) * cos(latitude_radians));
    azimuth_radians = normalize_radians_positive(azimuth_radians + M_PI);

    if (body->geocentric_distance_au > 0.0 && almanac_body_radius_au(body, &radius_au) && radius_au > 0.0 &&
        body->geocentric_distance_au >= radius_au) {
        semi_diameter_radians = asin(almanac_clamp_unit(radius_au / body->geocentric_distance_au));
    }

    memset(out, 0, sizeof(*out));
    out->altitude_degrees = almanac_radians_to_degrees(altitude_radians);
    out->azimuth_degrees = almanac_radians_to_degrees(azimuth_radians);
    out->semi_diameter_degrees =
        semi_diameter_radians == semi_diameter_radians ? almanac_radians_to_degrees(semi_diameter_radians) : NAN;
    out->above_horizon = out->altitude_degrees > 0.0;
    out->visible = out->altitude_degrees +
                       (out->semi_diameter_degrees == out->semi_diameter_degrees ? out->semi_diameter_degrees : 0.0) >
                   0.0;
    return true;
}

static bool almanac_observer_equatorial_position_au(almanac_t *almanac, const almanac_observer_t *observer, double jd,
    cartesian3_t *out)
{
    static const double earth_equatorial_radius_au = 6378.137 / 149597870.7;
    static const double earth_flattening = 1.0 / 298.257223563;
    static const double au_per_metre = 1.0 / 149597870700.0;
    double latitude;
    double lst;
    double sin_latitude;
    double cos_latitude;
    double eccentricity2;
    double prime_vertical;
    double rho_xy;
    double rho_z;
    double gha_aries;

    if (!almanac || !observer || !out)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;
    gha_aries = almanac_apparent_gha_aries_for_jd(almanac, jd);
    if (gha_aries == DBL_MAX)
        return false;

    latitude = almanac_degrees_to_radians(observer->latitude_degrees);
    lst = almanac_degrees_to_radians(almanac_normalize_degrees(gha_aries + observer->longitude_degrees));
    sincos(latitude, &sin_latitude, &cos_latitude);
    eccentricity2 = earth_flattening * (2.0 - earth_flattening);
    prime_vertical = earth_equatorial_radius_au / sqrt(1.0 - eccentricity2 * sin_latitude * sin_latitude);
    rho_xy = (prime_vertical + observer->elevation_metres * au_per_metre) * cos_latitude;
    rho_z = ((1.0 - eccentricity2) * prime_vertical + observer->elevation_metres * au_per_metre) * sin_latitude;

    out->x = rho_xy * cos(lst);
    out->y = rho_xy * sin(lst);
    out->z = rho_z;
    return true;
}

bool almanac_topocentric_vector(almanac_t *almanac, const cartesian3_t *geocentric_equatorial_au,
    const almanac_observer_t *observer, double jd, cartesian3_t *out)
{
    cartesian3_t observer_position;

    if (!geocentric_equatorial_au || !out)
        return false;
    if (!almanac_observer_equatorial_position_au(almanac, observer, jd, &observer_position))
        return false;
    *out = cartesian_subtract(geocentric_equatorial_au, &observer_position);
    return true;
}

double almanac_topocentric_altitude_degrees(almanac_t *almanac, const cartesian3_t *topocentric_equatorial_au,
    const almanac_observer_t *observer, double jd)
{
    double gha_aries;
    double latitude;
    double lst;
    double sin_latitude;
    double cos_latitude;
    cartesian3_t zenith;
    cartesian3_t direction;

    if (!almanac || !topocentric_equatorial_au || !observer)
        return NAN;
    if (!almanac_observer_is_valid(almanac, observer))
        return NAN;
    gha_aries = almanac_apparent_gha_aries_for_jd(almanac, jd);
    if (gha_aries == DBL_MAX)
        return NAN;

    direction = *topocentric_equatorial_au;
    if (!cartesian_normalize_in_place(&direction))
        return NAN;
    latitude = almanac_degrees_to_radians(observer->latitude_degrees);
    lst = almanac_degrees_to_radians(almanac_normalize_degrees(gha_aries + observer->longitude_degrees));
    sincos(latitude, &sin_latitude, &cos_latitude);
    zenith.x = cos_latitude * cos(lst);
    zenith.y = cos_latitude * sin(lst);
    zenith.z = sin_latitude;

    return almanac_radians_to_degrees(asin(almanac_clamp_unit(cartesian_dot(&direction, &zenith))));
}

static bool almanac_topocentric_horizontal_degrees(almanac_t *almanac, const cartesian3_t *topocentric_equatorial_au,
    const almanac_observer_t *observer, double jd, double *out_altitude_degrees, double *out_azimuth_degrees)
{
    double gha_aries;
    double latitude;
    double lst;
    double sin_latitude;
    double cos_latitude;
    cartesian3_t direction;
    cartesian3_t zenith;
    cartesian3_t north;
    cartesian3_t east;
    double altitude;
    double azimuth;

    if (!almanac || !topocentric_equatorial_au || !observer || !out_altitude_degrees || !out_azimuth_degrees)
        return false;
    if (!almanac_observer_is_valid(almanac, observer))
        return false;

    gha_aries = almanac_apparent_gha_aries_for_jd(almanac, jd);
    if (gha_aries == DBL_MAX)
        return false;

    direction = *topocentric_equatorial_au;
    if (!cartesian_normalize_in_place(&direction))
        return false;

    latitude = almanac_degrees_to_radians(observer->latitude_degrees);
    lst = almanac_degrees_to_radians(almanac_normalize_degrees(gha_aries + observer->longitude_degrees));
    sincos(latitude, &sin_latitude, &cos_latitude);

    zenith.x = cos_latitude * cos(lst);
    zenith.y = cos_latitude * sin(lst);
    zenith.z = sin_latitude;
    north.x = -sin_latitude * cos(lst);
    north.y = -sin_latitude * sin(lst);
    north.z = cos_latitude;
    east.x = -sin(lst);
    east.y = cos(lst);
    east.z = 0.0;

    altitude = asin(almanac_clamp_unit(cartesian_dot(&direction, &zenith)));
    azimuth = atan2(cartesian_dot(&direction, &east), cartesian_dot(&direction, &north));

    *out_altitude_degrees = almanac_radians_to_degrees(altitude);
    *out_azimuth_degrees = almanac_radians_to_degrees(normalize_radians_positive(azimuth));
    return true;
}

bool almanac_body_horizon_geometry(almanac_t *almanac, almanac_body_id_t body_id, const almanac_observer_t *observer,
    double jd, almanac_horizon_geometry_t *out)
{
    almanac_state_t body_state;
    cartesian3_t body_topocentric;
    double body_distance;
    double radius_au;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !observer || !out) {
        return false;
    }
    if (!almanac_body_id_radius_au(body_id, &radius_au))
        return false;
    if (!almanac_state_for_body_at_jd(almanac, body_id, jd, &body_state))
        return false;

    if (!almanac_topocentric_vector(almanac, &body_state.geocentric_equatorial_au, observer, jd, &body_topocentric)) {
        return false;
    }

    body_distance = cartesian_length(&body_topocentric);
    if (!(body_distance > radius_au))
        return false;
    if (!almanac_topocentric_horizontal_degrees(almanac, &body_topocentric, observer, jd, &out->altitude_degrees,
                                                &out->azimuth_degrees)) {
        return false;
    }
    out->semi_diameter_degrees = almanac_radians_to_degrees(asin(almanac_clamp_unit(radius_au / body_distance)));
    return true;
}
