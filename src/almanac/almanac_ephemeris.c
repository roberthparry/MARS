/* Chebyshev states and apparent-place corrections. */
#include <float.h>
#include <math.h>
#include <string.h>

#include "almanac_engine_internal.h"

static const almanac_body_ref_id_t ALMANAC_BODY_REF_IDS[ALMANAC_BODY_ID_COUNT] = {
    [ALMANAC_BODY_ID_SUN]     = ALMANAC_BODY_REF_ID_SUN,
    [ALMANAC_BODY_ID_MOON]    = ALMANAC_BODY_REF_ID_MOON,
    [ALMANAC_BODY_ID_MERCURY] = ALMANAC_BODY_REF_ID_MERCURY,
    [ALMANAC_BODY_ID_VENUS]   = ALMANAC_BODY_REF_ID_VENUS,
    [ALMANAC_BODY_ID_MARS]    = ALMANAC_BODY_REF_ID_MARS,
    [ALMANAC_BODY_ID_JUPITER] = ALMANAC_BODY_REF_ID_JUPITER,
    [ALMANAC_BODY_ID_SATURN]  = ALMANAC_BODY_REF_ID_SATURN
};

static const double ALMANAC_MOON_EARTH_SYSTEM_MASS_FRACTION = 0.01215058426954;
static const double ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY = 173.1446326846693;

static bool almanac_lunar_geocentric_ecliptic_vector(almanac_t *almanac, double jd, cartesian3_t *out);
static bool almanac_lunar_geocentric_ecliptic_velocity(almanac_t *almanac, double jd,
    cartesian3_t *out_velocity_au_per_day);

double almanac_chebyshev_eval(const double *coeff, int degree, double x)
{
    double b_kplus1 = 0.0;
    double b_kplus2 = 0.0;
    double b_k;
    int i;

    for (i = degree; i >= 1; --i) {
        b_k = 2.0 * x * b_kplus1 - b_kplus2 + coeff[i];
        b_kplus2 = b_kplus1;
        b_kplus1 = b_k;
    }
    return x * b_kplus1 - b_kplus2 + coeff[0];
}

static double almanac_chebyshev_derivative_eval(const double *coeff, int degree, double x)
{
    double u_minus2 = 1.0;
    double value = degree >= 1 ? coeff[1] : 0.0;
    double u_minus1 = 2.0 * x;
    int n;

    for (n = 2; n <= degree; ++n) {
        double u;

        if (n == 2) {
            u = u_minus1;
        } else {
            u = 2.0 * x * u_minus1 - u_minus2;
            u_minus2 = u_minus1;
            u_minus1 = u;
        }
        value += (double)n * coeff[n] * u;
    }
    return value;
}

static void almanac_eval_chebyshev_position_segment(const almanac_chebyshev_position_segment_t *segment, double jd,
    cartesian3_t *position_au, cartesian3_t *velocity_au_per_day)
{
    double x;

    if (!segment)
        return;
    x = (jd - segment->reference_jd) / segment->radius_days;
    if (position_au) {
        position_au->x = almanac_chebyshev_eval(segment->coeff[0], segment->degree, x);
        position_au->y = almanac_chebyshev_eval(segment->coeff[1], segment->degree, x);
        position_au->z = almanac_chebyshev_eval(segment->coeff[2], segment->degree, x);
    }
    if (velocity_au_per_day) {
        velocity_au_per_day->x =
            almanac_chebyshev_derivative_eval(segment->coeff[0], segment->degree, x) / segment->radius_days;
        velocity_au_per_day->y =
            almanac_chebyshev_derivative_eval(segment->coeff[1], segment->degree, x) / segment->radius_days;
        velocity_au_per_day->z =
            almanac_chebyshev_derivative_eval(segment->coeff[2], segment->degree, x) / segment->radius_days;
    }
}

static void almanac_state_fill_equatorial(almanac_state_t *state)
{
    double radius_xy;

    if (!state)
        return;
    radius_xy = sqrt(state->geocentric_equatorial_au.x * state->geocentric_equatorial_au.x +
                     state->geocentric_equatorial_au.y * state->geocentric_equatorial_au.y);
    state->right_ascension_hours = almanac_normalize_degrees(almanac_radians_to_degrees(
                                       atan2(state->geocentric_equatorial_au.y, state->geocentric_equatorial_au.x))) /
                                   15.0;
    state->declination_degrees = almanac_radians_to_degrees(atan2(state->geocentric_equatorial_au.z, radius_xy));
}

static bool almanac_earth_heliocentric_state(almanac_t *almanac, double jd, cartesian3_t *position_ecliptic_au,
    cartesian3_t *velocity_ecliptic_au_per_day)
{
    almanac_chebyshev_position_segment_t cheb_segment;
    cartesian3_t earth_moon_barycenter;
    cartesian3_t earth_moon_barycenter_velocity;
    cartesian3_t moon_geocentric;
    cartesian3_t moon_geocentric_velocity;
    cartesian3_t moon_offset;
    cartesian3_t position_storage;
    bool have_velocity = velocity_ecliptic_au_per_day != NULL;

    if (!almanac || (!position_ecliptic_au && !velocity_ecliptic_au_per_day)) {
        almanac_set_error(almanac, "invalid Earth-state request");
        return false;
    }
    if (!position_ecliptic_au)
        position_ecliptic_au = &position_storage;

    if (!almanac_load_correction_model(almanac))
        return false;

    if (!almanac_fetch_chebyshev_position_segment(almanac, ALMANAC_BODY_REF_ID_EARTH_BARYCENTER, jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment, jd, &earth_moon_barycenter,
                                            have_velocity ? &earth_moon_barycenter_velocity : NULL);
    if (!almanac_lunar_geocentric_ecliptic_vector(almanac, jd, &moon_geocentric))
        return false;
    moon_offset = cartesian_scale(&moon_geocentric, ALMANAC_MOON_EARTH_SYSTEM_MASS_FRACTION);
    *position_ecliptic_au = cartesian_subtract(&earth_moon_barycenter, &moon_offset);
    if (have_velocity) {
        if (!almanac_lunar_geocentric_ecliptic_velocity(almanac, jd, &moon_geocentric_velocity))
            return false;
        moon_offset = cartesian_scale(&moon_geocentric_velocity, ALMANAC_MOON_EARTH_SYSTEM_MASS_FRACTION);
        *velocity_ecliptic_au_per_day = cartesian_subtract(&earth_moon_barycenter_velocity, &moon_offset);
    }
    return true;
}

static cartesian3_t equatorial_from_ecliptic_vector(const cartesian3_t *ecliptic, almanac_t *almanac, double jd)
{
    return cartesian_rotate_x(ecliptic, almanac_mean_obliquity_radians(almanac, jd));
}

static double almanac_relativistic_light_time_days(const cartesian3_t *earth_heliocentric_au,
    const cartesian3_t *body_heliocentric_au, const cartesian3_t *earth_to_body_au)
{
    const double gravitational_radius_twice_au = 1.9741257129241e-08;
    double earth_sun_distance;
    double body_sun_distance;
    double earth_body_distance;
    double numerator;
    double denominator;

    if (!earth_heliocentric_au || !body_heliocentric_au || !earth_to_body_au)
        return 0.0;

    earth_sun_distance = cartesian_length(earth_heliocentric_au);
    body_sun_distance = cartesian_length(body_heliocentric_au);
    earth_body_distance = cartesian_length(earth_to_body_au);
    if (earth_sun_distance <= 0.0 || body_sun_distance <= 0.0 || earth_body_distance <= 0.0)
        return earth_body_distance / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;

    numerator = earth_sun_distance + earth_body_distance + body_sun_distance;
    denominator = earth_sun_distance - earth_body_distance + body_sun_distance;
    if (denominator <= 0.0 || numerator <= denominator)
        return earth_body_distance / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;

    return (earth_body_distance + gravitational_radius_twice_au * log(numerator / denominator)) /
           ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;
}

static bool almanac_apply_gravitational_deflection(const cartesian3_t *earth_heliocentric_equatorial_au,
    const cartesian3_t *mean_equatorial_direction, const cartesian3_t *sun_to_body_direction_equatorial,
    cartesian3_t *out_direction)
{
    const double gravitational_radius_twice_au = 1.9741257129241e-08;
    cartesian3_t earth_direction;
    cartesian3_t p_hat;
    cartesian3_t q_hat;
    cartesian3_t first_cross;
    cartesian3_t second_cross;
    double earth_distance;
    double denominator;
    cartesian3_t correction;

    if (!earth_heliocentric_equatorial_au || !mean_equatorial_direction || !sun_to_body_direction_equatorial ||
        !out_direction) {
        return false;
    }

    earth_distance = cartesian_length(earth_heliocentric_equatorial_au);
    if (earth_distance <= 0.0)
        return false;

    earth_direction = cartesian_scale(earth_heliocentric_equatorial_au, 1.0 / earth_distance);
    p_hat = *mean_equatorial_direction;
    q_hat = *sun_to_body_direction_equatorial;
    if (!cartesian_normalize_in_place(&p_hat) || !cartesian_normalize_in_place(&q_hat))
        return false;

    denominator = 1.0 + cartesian_dot(&q_hat, &earth_direction);
    if (fabs(denominator) < 1.0e-12) {
        *out_direction = p_hat;
        return true;
    }

    first_cross = cartesian_cross(&q_hat, &earth_direction);
    second_cross = cartesian_cross(&first_cross, &p_hat);
    correction = cartesian_scale(&second_cross, (earth_distance * gravitational_radius_twice_au) / denominator);
    *out_direction = cartesian_add(&p_hat, &correction);
    return cartesian_normalize_in_place(out_direction);
}

static bool almanac_apply_annual_aberration(const cartesian3_t *mean_equatorial_direction,
    const cartesian3_t *earth_heliocentric_velocity_au_per_day, cartesian3_t *out_direction)
{
    cartesian3_t beta;
    cartesian3_t direction;
    cartesian3_t parallel_component;
    cartesian3_t transverse_beta;
    double projection;

    if (!mean_equatorial_direction || !earth_heliocentric_velocity_au_per_day || !out_direction)
        return false;

    direction = *mean_equatorial_direction;
    if (!cartesian_normalize_in_place(&direction))
        return false;

    beta = cartesian_scale(earth_heliocentric_velocity_au_per_day, 1.0 / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY);
    projection = cartesian_dot(&direction, &beta);
    parallel_component = cartesian_scale(&direction, projection);
    transverse_beta = cartesian_subtract(&beta, &parallel_component);
    *out_direction = cartesian_add(&direction, &transverse_beta);
    return cartesian_normalize_in_place(out_direction);
}

static bool almanac_apply_nutation(almanac_t *almanac, cartesian3_t *mean_equatorial_direction, double jd)
{
    double epsilon0;
    double delta_psi;
    double delta_epsilon;
    cartesian3_t ecliptic;
    cartesian3_t nutated;

    if (!mean_equatorial_direction)
        return false;

    epsilon0 = almanac_mean_obliquity_radians(almanac, jd);
    if (epsilon0 == DBL_MAX || !almanac_nutation_angles(almanac, jd, &delta_psi, &delta_epsilon))
        return false;
    ecliptic = cartesian_rotate_x(mean_equatorial_direction, -epsilon0);
    ecliptic = cartesian_rotate_z(&ecliptic, delta_psi);
    nutated = cartesian_rotate_x(&ecliptic, epsilon0 + delta_epsilon);
    *mean_equatorial_direction = nutated;
    return cartesian_normalize_in_place(mean_equatorial_direction);
}

static bool almanac_apply_apparent_direction_corrections(almanac_t *almanac, const almanac_model_row_t *model, double jd,
    almanac_state_t *state)
{
    cartesian3_t earth_velocity_ecliptic;
    cartesian3_t earth_velocity_equatorial;
    cartesian3_t earth_heliocentric_ecliptic;
    cartesian3_t earth_heliocentric_equatorial;
    cartesian3_t sun_to_body_direction;
    cartesian3_t direction;
    double distance;

    if (!almanac || !model || !state || !state->has_direction_vector)
        return false;
    if (!almanac_load_correction_model(almanac))
        return false;
    if (!almanac_earth_heliocentric_state(almanac, jd, &earth_heliocentric_ecliptic, &earth_velocity_ecliptic))
        return false;

    if (state->has_ecliptic_vector) {
        direction = state->geocentric_ecliptic_au;
        distance = cartesian_length(&direction);
        if (distance <= 0.0)
            return false;
        if (!cartesian_normalize_in_place(&direction))
            return false;

        if (model->body_id != ALMANAC_BODY_ID_SUN) {
            if (model->body_kind == ALMANAC_BODY_STAR) {
                sun_to_body_direction = direction;
            } else {
                sun_to_body_direction = cartesian_add(&earth_heliocentric_ecliptic, &state->geocentric_ecliptic_au);
            }
            if (!almanac_apply_gravitational_deflection(&earth_heliocentric_ecliptic, &direction,
                                                        &sun_to_body_direction, &direction)) {
                return false;
            }
        }
        if (!almanac_apply_annual_aberration(&direction, &earth_velocity_ecliptic, &direction))
            return false;
        if (!almanac_true_equatorial_from_ecliptic(almanac, &direction, jd, &direction))
            return false;

        state->geocentric_equatorial_au = cartesian_scale(&direction, distance);
        return true;
    }

    earth_heliocentric_equatorial = equatorial_from_ecliptic_vector(&earth_heliocentric_ecliptic, almanac, jd);
    earth_velocity_equatorial = equatorial_from_ecliptic_vector(&earth_velocity_ecliptic, almanac, jd);
    direction = state->geocentric_equatorial_au;
    if (model->body_id != ALMANAC_BODY_ID_SUN) {
        if (model->body_kind == ALMANAC_BODY_STAR) {
            sun_to_body_direction = direction;
        } else {
            sun_to_body_direction = cartesian_add(&earth_heliocentric_equatorial, &state->geocentric_equatorial_au);
        }
        if (!almanac_apply_gravitational_deflection(&earth_heliocentric_equatorial, &direction, &sun_to_body_direction,
                                                    &direction)) {
            return false;
        }
    }
    if (!almanac_apply_annual_aberration(&direction, &earth_velocity_equatorial, &direction))
        return false;
    if (!almanac_apply_nutation(almanac, &direction, jd))
        return false;

    if (state->has_geocentric_vector) {
        double distance = cartesian_length(&state->geocentric_equatorial_au);
        state->geocentric_equatorial_au = cartesian_scale(&direction, distance);
    } else {
        state->geocentric_equatorial_au = direction;
    }
    return true;
}

static bool almanac_lunar_geocentric_ecliptic_vector(almanac_t *almanac, double jd, cartesian3_t *out)
{
    almanac_chebyshev_position_segment_t cheb_segment;

    if (!out) {
        almanac_set_error(almanac, "invalid lunar ecliptic vector request");
        return false;
    }

    if (!almanac_fetch_chebyshev_position_segment(almanac, ALMANAC_BODY_REF_ID_MOON, jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment, jd, out, NULL);
    return true;
}

static bool almanac_lunar_geocentric_ecliptic_velocity(almanac_t *almanac, double jd,
    cartesian3_t *out_velocity_au_per_day)
{
    almanac_chebyshev_position_segment_t cheb_segment;

    if (!out_velocity_au_per_day) {
        almanac_set_error(almanac, "invalid lunar ecliptic velocity request");
        return false;
    }

    if (!almanac_fetch_chebyshev_position_segment(almanac, ALMANAC_BODY_REF_ID_MOON, jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment, jd, NULL, out_velocity_au_per_day);
    return true;
}

static bool almanac_equatorial_from_moon(almanac_t *almanac, const almanac_model_row_t *model, double jd,
    almanac_state_t *state)
{
    cartesian3_t earth_velocity_ecliptic;
    double light_time_days = 0.0;
    int iteration;

    if (!almanac || !model || !state)
        return false;
    if (!almanac_load_correction_model(almanac))
        return false;

    for (iteration = 0; iteration < 3; ++iteration) {
        double distance_au;

        if (!almanac_lunar_geocentric_ecliptic_vector(almanac, jd - light_time_days, &state->geocentric_ecliptic_au))
            return false;
        distance_au = cartesian_length(&state->geocentric_ecliptic_au);
        if (distance_au <= 0.0)
            return false;
        light_time_days = distance_au / ALMANAC_SPEED_OF_LIGHT_AU_PER_DAY;
    }
    if (!almanac_earth_heliocentric_state(almanac, jd, NULL, &earth_velocity_ecliptic))
        return false;
    state->geocentric_ecliptic_au.x -= earth_velocity_ecliptic.x * light_time_days;
    state->geocentric_ecliptic_au.y -= earth_velocity_ecliptic.y * light_time_days;
    state->geocentric_ecliptic_au.z -= earth_velocity_ecliptic.z * light_time_days;
    state->has_geocentric_vector = true;
    state->has_ecliptic_vector = true;
    state->has_direction_vector = true;
    return true;
}

static bool almanac_equatorial_from_fixed(const almanac_model_row_t *model, double jd, almanac_state_t *state)
{
    const double j2000 = 2451545.0;
    double alpha0;
    double delta0;
    double years;
    double T;
    double t;
    double zeta;
    double z;
    double theta;
    double A;
    double B;
    double C;
    double alpha;
    double delta;

    if (!model || !state)
        return false;

    years = (jd - model->fixed_epoch_jd) / 365.25;
    alpha0 = almanac_degrees_to_radians(model->fixed_ra_hours * 15.0 + (model->fixed_pm_ra_mas_per_year * years) / 3600000.0);
    delta0 = almanac_degrees_to_radians(model->fixed_dec_degrees + (model->fixed_pm_dec_mas_per_year * years) / 3600000.0);

    T = (model->fixed_epoch_jd - j2000) / 36525.0;
    t = (jd - model->fixed_epoch_jd) / 36525.0;

    zeta = almanac_degrees_to_radians((((2306.2181 + 1.39656 * T - 0.000139 * T * T) * t) + ((0.30188 - 0.000344 * T) * t * t) +
                               (0.017998 * t * t * t)) /
                              3600.0);
    z = almanac_degrees_to_radians((((2306.2181 + 1.39656 * T - 0.000139 * T * T) * t) + ((1.09468 + 0.000066 * T) * t * t) +
                            (0.018203 * t * t * t)) /
                           3600.0);
    theta = almanac_degrees_to_radians((((2004.3109 - 0.85330 * T - 0.000217 * T * T) * t) -
                                ((0.42665 + 0.000217 * T) * t * t) - (0.041833 * t * t * t)) /
                               3600.0);

    A = cos(delta0) * sin(alpha0 + zeta);
    B = cos(theta) * cos(delta0) * cos(alpha0 + zeta) - sin(theta) * sin(delta0);
    C = sin(theta) * cos(delta0) * cos(alpha0 + zeta) + cos(theta) * sin(delta0);

    alpha = atan2(A, B) + z;
    delta = asin(C);

    state->right_ascension_hours = almanac_normalize_degrees(almanac_radians_to_degrees(alpha)) / 15.0;
    state->declination_degrees = almanac_radians_to_degrees(delta);
    state->has_geocentric_vector = false;
    state->has_direction_vector = true;
    state->geocentric_equatorial_au.x = cos(delta) * cos(alpha);
    state->geocentric_equatorial_au.y = cos(delta) * sin(alpha);
    state->geocentric_equatorial_au.z = sin(delta);
    return true;
}

static bool almanac_equatorial_from_chebyshev(almanac_t *almanac, const almanac_model_row_t *model, double jd,
    almanac_state_t *state)
{
    almanac_chebyshev_position_segment_t cheb_segment;
    cartesian3_t body_helio;
    cartesian3_t earth_helio;
    cartesian3_t geocentric;
    almanac_body_ref_id_t body_ref_id;
    double light_time_days = 0.0;
    int iteration;

    if (!almanac || !model || !state)
        return false;
    if (!almanac_earth_heliocentric_state(almanac, jd, &earth_helio, NULL))
        return false;

    if (model->body_id == ALMANAC_BODY_ID_SUN) {
        geocentric.x = -earth_helio.x;
        geocentric.y = -earth_helio.y;
        geocentric.z = -earth_helio.z;
    } else {
        body_ref_id = ALMANAC_BODY_REF_IDS[model->body_id];
        if (body_ref_id == ALMANAC_BODY_REF_ID_NONE) {
            almanac_set_error(almanac, "requested body does not have a Chebyshev body reference");
            return false;
        }
        for (iteration = 0; iteration < 3; ++iteration) {
            if (!almanac_fetch_chebyshev_position_segment(almanac, body_ref_id, jd - light_time_days, &cheb_segment))
                return false;
            almanac_eval_chebyshev_position_segment(&cheb_segment, jd - light_time_days, &body_helio, NULL);
            geocentric.x = body_helio.x - earth_helio.x;
            geocentric.y = body_helio.y - earth_helio.y;
            geocentric.z = body_helio.z - earth_helio.z;
            if (!almanac_load_correction_model(almanac))
                return false;
            light_time_days = almanac_relativistic_light_time_days(&earth_helio, &body_helio, &geocentric);
        }
    }

    state->geocentric_ecliptic_au = geocentric;
    if (!almanac_true_equatorial_from_ecliptic(almanac, &state->geocentric_ecliptic_au, jd,
                                               &state->geocentric_equatorial_au)) {
        return false;
    }
    state->has_geocentric_vector = true;
    state->has_ecliptic_vector = true;
    state->has_direction_vector = true;
    return true;
}

bool almanac_compute_state_for_model(almanac_t *almanac, const almanac_model_row_t *model, const datetime_t *moment,
    almanac_state_t *state)
{
    double ephemeris_jd;

    if (!almanac || !model || !moment || !state) {
        almanac_set_error(almanac, "invalid almanac computation request");
        return false;
    }

    ephemeris_jd = datetime_jd_tdb(moment);
    if (ephemeris_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian dates for almanac computation");
        return false;
    }
    memset(state, 0, sizeof(*state));
    if (model->model_kind == ALMANAC_MODEL_KIND_FIXED_EQUATORIAL) {
        if (!almanac_equatorial_from_fixed(model, ephemeris_jd, state)) {
            almanac_set_error(almanac, "failed to compute fixed-star position");
            return false;
        }
    } else if (model->model_kind == ALMANAC_MODEL_KIND_CHEBYSHEV_POSITION) {
        if (!almanac_equatorial_from_chebyshev(almanac, model, ephemeris_jd, state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute Chebyshev position");
            return false;
        }
    } else if (model->model_kind == ALMANAC_MODEL_KIND_LUNAR_CHEBYSHEV) {
        if (!almanac_equatorial_from_moon(almanac, model, ephemeris_jd, state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute lunar position");
            return false;
        }
    } else {
        almanac_set_error(almanac, "unsupported almanac model kind");
        return false;
    }

    if (!state->apparent_place_complete &&
        !almanac_apply_apparent_direction_corrections(almanac, model, ephemeris_jd, state)) {
        if (!string_length(almanac->error))
            almanac_set_error(almanac, "failed to apply apparent-place corrections");
        return false;
    }

    if (state->has_direction_vector)
        almanac_state_fill_equatorial(state);
    return true;
}

bool almanac_state_for_body_at_jd(almanac_t *almanac, almanac_body_id_t body_id, double jd, almanac_state_t *out_state)
{
    datetime_t *moment;
    almanac_model_row_t model;
    bool ok;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !out_state) {
        almanac_set_error(almanac, "invalid almanac state request");
        return false;
    }
    moment = datetime_alloc();
    if (!moment) {
        almanac_set_error(almanac, "failed to allocate datetime for almanac state request");
        return false;
    }
    if (!datetime_init_jd(moment, jd)) {
        datetime_dealloc(moment);
        almanac_set_error(almanac, "failed to initialise datetime for almanac state request");
        return false;
    }
    ok = almanac_fetch_model(almanac, body_id, &model) &&
         almanac_compute_state_for_model(almanac, &model, moment, out_state);
    datetime_dealloc(moment);
    return ok;
}
