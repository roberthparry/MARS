#ifndef MARS_ALMANAC_ENGINE_INTERNAL_H
#define MARS_ALMANAC_ENGINE_INTERNAL_H

#include "almanac.h"
#include "almanac_cartesian.h"
#include "sqlite.h"
#include "ustring.h"

/* Private position state and interfaces for the almanac implementation. */
struct _almanac_entry_t {
    almanac_body_id_t body_id;
    almanac_body_kind_t body_kind;
    double moment_jd;
    double gha_aries_degrees;
    double sha_degrees;
    double declination_degrees;
    double right_ascension_hours;
    double geocentric_distance_au;
    double heliocentric_distance_au;
    double phase_angle_degrees;
    double visual_magnitude;
};

typedef struct almanac_state_t {
    cartesian3_t geocentric_equatorial_au;
    cartesian3_t geocentric_ecliptic_au;
    bool has_geocentric_vector;
    bool has_ecliptic_vector;
    bool has_direction_vector;
    bool apparent_place_complete;
    double right_ascension_hours;
    double declination_degrees;
} almanac_state_t;

double almanac_mean_obliquity_radians(almanac_t *almanac, double jd);
bool almanac_nutation_angles(almanac_t *almanac, double jd, double *delta_psi_radians, double *delta_epsilon_radians);
double almanac_apparent_gha_aries_for_jd(almanac_t *almanac, double jd);
bool almanac_true_equatorial_from_ecliptic(almanac_t *almanac, const cartesian3_t *ecliptic, double jd,
    cartesian3_t *out_equatorial);
bool almanac_state_for_body_at_jd(almanac_t *almanac, almanac_body_id_t body_id, double jd, almanac_state_t *out_state);
bool almanac_entry_fill_at_jd(almanac_t *almanac, almanac_body_id_t body_id, double jd, almanac_entry_t *out_entry);

/* Private engine state and interfaces for the almanac implementation. */
typedef struct almanac_nutation_term_t {
    int multiplier_L;
    int multiplier_Lprime;
    int multiplier_omega;
    double sin_coeff_arcsec;
    double cos_coeff_arcsec;
} almanac_nutation_term_t;

typedef enum almanac_body_ref_id_t {
    ALMANAC_BODY_REF_ID_NONE = 0,
    ALMANAC_BODY_REF_ID_SUN = 1,
    ALMANAC_BODY_REF_ID_EARTH_BARYCENTER = 2,
    ALMANAC_BODY_REF_ID_MOON = 3,
    ALMANAC_BODY_REF_ID_MERCURY = 4,
    ALMANAC_BODY_REF_ID_VENUS = 5,
    ALMANAC_BODY_REF_ID_MARS = 6,
    ALMANAC_BODY_REF_ID_JUPITER = 7,
    ALMANAC_BODY_REF_ID_SATURN = 8,
    ALMANAC_BODY_REF_ID_EARTH = 9
} almanac_body_ref_id_t;

#define ALMANAC_BODY_REF_ID_LIMIT (ALMANAC_BODY_REF_ID_EARTH + 1)

typedef enum almanac_model_kind_t {
    ALMANAC_MODEL_KIND_UNKNOWN = 0,
    ALMANAC_MODEL_KIND_FIXED_EQUATORIAL,
    ALMANAC_MODEL_KIND_CHEBYSHEV_POSITION,
    ALMANAC_MODEL_KIND_LUNAR_CHEBYSHEV
} almanac_model_kind_t;

typedef enum almanac_brightness_model_t {
    ALMANAC_BRIGHTNESS_MODEL_UNKNOWN = 0,
    ALMANAC_BRIGHTNESS_MODEL_NONE,
    ALMANAC_BRIGHTNESS_MODEL_CATALOGUED,
    ALMANAC_BRIGHTNESS_MODEL_SUN_DISTANCE,
    ALMANAC_BRIGHTNESS_MODEL_PLANETARY_PHASE,
    ALMANAC_BRIGHTNESS_MODEL_LUNAR_PHASE
} almanac_brightness_model_t;

typedef struct almanac_model_row_t {
    almanac_body_id_t body_id;
    almanac_body_kind_t body_kind;
    almanac_model_kind_t model_kind;
    almanac_brightness_model_t brightness_model;
    int sort_order;
    double magnitude_constant;
    double magnitude_linear;
    double magnitude_quadratic;
    double magnitude_cubic;
    double magnitude_quartic;
    double fixed_epoch_jd;
    double fixed_ra_hours;
    double fixed_dec_degrees;
    double fixed_pm_ra_mas_per_year;
    double fixed_pm_dec_mas_per_year;
    double orbit_epoch_jd;
    double N0;
    double N_dot;
    double i0;
    double i_dot;
    double w0;
    double w_dot;
    double a0;
    double a_dot;
    double e0;
    double e_dot;
    double M0;
    double M_dot;
} almanac_model_row_t;

#define ALMANAC_CHEB_COMPONENT_COUNT 3
#define ALMANAC_FRAME_ROTATION_COMPONENT_COUNT 9
#define ALMANAC_CHEB_MAX_COEFF_COUNT 33
#define ALMANAC_NUTATION_COEFF_COUNT 8

typedef struct almanac_chebyshev_position_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double radius_days;
    int degree;
    double coeff[ALMANAC_CHEB_COMPONENT_COUNT][ALMANAC_CHEB_MAX_COEFF_COUNT];
} almanac_chebyshev_position_segment_t;

typedef struct almanac_frame_rotation_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double radius_days;
    int degree;
    double coeff[ALMANAC_FRAME_ROTATION_COMPONENT_COUNT][ALMANAC_CHEB_MAX_COEFF_COUNT];
} almanac_frame_rotation_segment_t;

struct _almanac_t {
    sqlite_t *db;
    string_t *error;
    bool correction_model_loaded;
    almanac_nutation_term_t *nutation_terms;
    size_t nutation_term_count;
    bool chebyshev_position_segment_cached[ALMANAC_BODY_REF_ID_LIMIT];
    almanac_chebyshev_position_segment_t chebyshev_position_segment_cache[ALMANAC_BODY_REF_ID_LIMIT];
    bool frame_rotation_segment_cached;
    almanac_frame_rotation_segment_t frame_rotation_segment_cache;
    bool model_row_cached[ALMANAC_BODY_ID_COUNT];
    almanac_model_row_t model_row_cache[ALMANAC_BODY_ID_COUNT];
};

typedef struct almanac_poly8_t {
    double c0;
    double c1;
    double c2;
    double c3;
    double c4;
    double c5;
    double c6;
    double c7;
} almanac_poly8_t;

typedef struct almanac_nutation_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double span_days;
    almanac_poly8_t dpsi;
    almanac_poly8_t deps;
} almanac_nutation_segment_t;

void almanac_set_error(almanac_t *almanac, const char *message);
void almanac_set_sqlite_error(almanac_t *almanac);
bool almanac_fetch_nutation_segment(almanac_t *almanac, double jd, almanac_nutation_segment_t *out);
bool almanac_load_correction_model(almanac_t *almanac);
bool almanac_fetch_model(almanac_t *almanac, almanac_body_id_t body_id, almanac_model_row_t *out);
bool almanac_fetch_chebyshev_position_segment(almanac_t *almanac, almanac_body_ref_id_t body_ref_id, double jd,
    almanac_chebyshev_position_segment_t *out);
bool almanac_fetch_frame_rotation_segment(almanac_t *almanac, double jd, almanac_frame_rotation_segment_t *out);
double almanac_chebyshev_eval(const double *coeff, int degree, double x);
bool almanac_compute_state_for_model(almanac_t *almanac, const almanac_model_row_t *model, const datetime_t *moment,
    almanac_state_t *state);

/* Private geometry state and interfaces for the almanac implementation. */
typedef struct almanac_horizon_geometry_t {
    double altitude_degrees;
    double azimuth_degrees;
    double semi_diameter_degrees;
} almanac_horizon_geometry_t;

double almanac_normalize_degrees(double degrees);
double almanac_normalize_degrees_signed(double degrees);
double almanac_degrees_to_radians(double degrees);
double almanac_radians_to_degrees(double radians);
double almanac_clamp_unit(double value);
double almanac_disc_coverage_percent(double target_radius, double covering_radius, double centre_separation);
double almanac_body_semi_diameter_from_distance_degrees(almanac_body_id_t body_id, double distance_au);
double almanac_angular_separation_degrees(const cartesian3_t *a, const cartesian3_t *b);
double almanac_ecliptic_longitude_degrees(const cartesian3_t *vector);
bool almanac_observer_is_valid(almanac_t *almanac, const almanac_observer_t *observer);
bool almanac_topocentric_vector(almanac_t *almanac, const cartesian3_t *geocentric_equatorial_au,
    const almanac_observer_t *observer, double jd, cartesian3_t *out);
double almanac_topocentric_altitude_degrees(almanac_t *almanac, const cartesian3_t *topocentric_equatorial_au,
    const almanac_observer_t *observer, double jd);
bool almanac_body_horizon_geometry(almanac_t *almanac, almanac_body_id_t body_id, const almanac_observer_t *observer,
    double jd, almanac_horizon_geometry_t *out);

/* Private events state and interfaces for the almanac implementation. */
typedef double (*almanac_event_residual_fn)(almanac_t *almanac, double jd, void *context);
typedef double (*almanac_event_metric_fn)(almanac_t *almanac, double jd, void *context);

typedef enum almanac_contact_level_t {
    ALMANAC_CONTACT_LEVEL_OUTER = 0,
    ALMANAC_CONTACT_LEVEL_INNER,
    ALMANAC_CONTACT_LEVEL_TOTAL
} almanac_contact_level_t;

typedef struct almanac_eclipse_contact_context_t {
    almanac_body_id_t body_id;
    const almanac_observer_t *observer;
    almanac_contact_level_t contact_level;
} almanac_eclipse_contact_context_t;

double almanac_mean_moon_phase_jd(double k);
bool almanac_eclipse_candidate_near_node(double k);
bool almanac_refine_body_sun_longitude_near(almanac_t *almanac, almanac_body_id_t body_id, double target_degrees,
    double estimate_jd, double max_step_days, double *out_jd);
bool almanac_event_window_is_valid(almanac_t *almanac, const datetime_t *start, const datetime_t *end,
    double *out_start_jd, double *out_end_jd);
void almanac_event_time_from_jds(double jd, double local_jd, almanac_event_time_t *out);
void almanac_event_time_from_jd(double jd, almanac_event_time_t *out);
bool almanac_bisect_event_residual(almanac_t *almanac, almanac_event_residual_fn residual, void *context, double left_jd,
    double right_jd, double *out_jd);
double almanac_find_contact_jd(almanac_t *almanac, almanac_event_residual_fn residual, void *context, double greatest_jd,
    double direction, double max_span_days, double step_days);
double almanac_find_local_minimum_jd(almanac_t *almanac, almanac_event_metric_fn metric, void *context, double centre_jd,
    double half_span_days, double sample_step_days, int refinement_iterations);

/* Private solar eclipse state and interfaces for the almanac implementation. */
typedef struct almanac_solar_eclipse_geometry_t {
    double separation;
    double sun_sd;
    double moon_sd;
    double apparent_separation;
    double sun_altitude_degrees;
} almanac_solar_eclipse_geometry_t;

typedef struct almanac_solar_eclipse_circumstance_t {
    almanac_solar_eclipse_kind_t kind;
    double magnitude;
    double totality_percent;
    bool central;
} almanac_solar_eclipse_circumstance_t;

struct _almanac_solar_eclipse_t {
    almanac_solar_eclipse_kind_t kind;
    almanac_event_time_t first_contact;
    almanac_event_time_t second_contact;
    almanac_event_time_t greatest_eclipse;
    almanac_event_time_t third_contact;
    almanac_event_time_t fourth_contact;
    double separation_degrees;
    double magnitude;
    double totality_percent;
    double sun_semi_diameter_degrees;
    double moon_semi_diameter_degrees;
    bool central;
};

bool almanac_solar_eclipse_geometry(almanac_t *almanac, double jd, const almanac_observer_t *observer,
    almanac_solar_eclipse_geometry_t *out);
bool almanac_solar_eclipse_circumstance_from_geometry(const almanac_solar_eclipse_geometry_t *geometry,
    almanac_solar_eclipse_circumstance_t *out);
double almanac_solar_eclipse_metric(almanac_t *almanac, double jd, void *context);

#endif /* MARS_ALMANAC_ENGINE_INTERNAL_H */
