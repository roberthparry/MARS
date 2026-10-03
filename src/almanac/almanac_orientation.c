/* Sidereal time, nutation, obliquity and reference-frame rotation. */
#include <float.h>
#include <math.h>

#include "almanac_engine_internal.h"

static const double ALMANAC_GMST_BASE_DEG = 280.46061837;
static const double ALMANAC_GMST_RATE_DEG_PER_DAY = 360.98564736629;
static const double ALMANAC_GMST_QUADRATIC_DEG = 0.000387933;
static const double ALMANAC_GMST_CUBIC_DIVISOR = 38710000.0;
static const double ALMANAC_MEAN_OBLIQUITY_C0_ARCSEC = 84381.448;
static const double ALMANAC_MEAN_OBLIQUITY_C1_ARCSEC = -46.8150;
static const double ALMANAC_MEAN_OBLIQUITY_C2_ARCSEC = -0.00059;
static const double ALMANAC_MEAN_OBLIQUITY_C3_ARCSEC = 0.001813;
static const double ALMANAC_PRECESSION_ZETA_C1_DEG = 0.6406161;
static const double ALMANAC_PRECESSION_ZETA_C2_DEG = 0.0000839;
static const double ALMANAC_PRECESSION_ZETA_C3_DEG = 0.0000050;
static const double ALMANAC_PRECESSION_THETA_C1_DEG = 0.5567530;
static const double ALMANAC_PRECESSION_THETA_C2_DEG = -0.0001185;
static const double ALMANAC_PRECESSION_THETA_C3_DEG = -0.0000116;
static const double ALMANAC_PRECESSION_Z_C1_DEG = 0.6406161;
static const double ALMANAC_PRECESSION_Z_C2_DEG = 0.0003041;
static const double ALMANAC_PRECESSION_Z_C3_DEG = 0.0000051;
static const double ALMANAC_OMEGA_C0_DEG = 125.04452;
static const double ALMANAC_OMEGA_C1_DEG_PER_CENTURY = -1934.136261;
static const double ALMANAC_OMEGA_C2_DEG_PER_CENTURY2 = 0.0020708;
static const double ALMANAC_OMEGA_C3_CENTURY3_DIVISOR = 450000.0;
static const double ALMANAC_SOLAR_MEAN_LONGITUDE_C0_DEG = 280.4665;
static const double ALMANAC_SOLAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY = 36000.7698;
static const double ALMANAC_LUNAR_MEAN_LONGITUDE_C0_DEG = 218.3165;
static const double ALMANAC_LUNAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY = 481267.8813;

static double almanac_cheb8_eval(const almanac_poly8_t *poly, double x)
{
    double b_kplus1 = 0.0;
    double b_kplus2 = 0.0;
    double b_k;
    const double *coeffs;
    int i;

    if (!poly)
        return 0.0;
    coeffs = &poly->c0;
    for (i = 7; i >= 1; --i) {
        b_k = 2.0 * x * b_kplus1 - b_kplus2 + coeffs[i];
        b_kplus2 = b_kplus1;
        b_kplus1 = b_k;
    }
    return x * b_kplus1 - b_kplus2 + coeffs[0];
}

static double almanac_gha_aries_for_jd(almanac_t *almanac, double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double gmst;

    if (!almanac_load_correction_model(almanac))
        return DBL_MAX;
    gmst = ALMANAC_GMST_BASE_DEG + ALMANAC_GMST_RATE_DEG_PER_DAY * (jd - 2451545.0) +
           ALMANAC_GMST_QUADRATIC_DEG * T * T - (T * T * T) / ALMANAC_GMST_CUBIC_DIVISOR;

    return almanac_normalize_degrees(gmst);
}

double almanac_mean_obliquity_radians(almanac_t *almanac, double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double arcseconds;

    if (!almanac_load_correction_model(almanac))
        return DBL_MAX;
    arcseconds = ALMANAC_MEAN_OBLIQUITY_C0_ARCSEC + ALMANAC_MEAN_OBLIQUITY_C1_ARCSEC * T +
                 ALMANAC_MEAN_OBLIQUITY_C2_ARCSEC * T * T + ALMANAC_MEAN_OBLIQUITY_C3_ARCSEC * T * T * T;

    return almanac_degrees_to_radians(arcseconds / 3600.0);
}

bool almanac_nutation_angles(almanac_t *almanac, double jd, double *delta_psi_radians, double *delta_epsilon_radians)
{
    almanac_nutation_segment_t segment;
    double T = (jd - 2451545.0) / 36525.0;
    double omega;
    double L;
    double Lprime;
    double dpsi_arcseconds = 0.0;
    double deps_arcseconds = 0.0;
    size_t i;

    if (!almanac_load_correction_model(almanac))
        return false;
    if (almanac_fetch_nutation_segment(almanac, jd, &segment)) {
        double x = (jd - segment.reference_jd) / segment.span_days;

        if (delta_psi_radians)
            *delta_psi_radians = almanac_cheb8_eval(&segment.dpsi, x);
        if (delta_epsilon_radians)
            *delta_epsilon_radians = almanac_cheb8_eval(&segment.deps, x);
        return true;
    }

    omega = almanac_degrees_to_radians(almanac_normalize_degrees(ALMANAC_OMEGA_C0_DEG + ALMANAC_OMEGA_C1_DEG_PER_CENTURY * T +
                                                 ALMANAC_OMEGA_C2_DEG_PER_CENTURY2 * T * T +
                                                 (T * T * T) / ALMANAC_OMEGA_C3_CENTURY3_DIVISOR));
    L = almanac_degrees_to_radians(
        almanac_normalize_degrees(ALMANAC_SOLAR_MEAN_LONGITUDE_C0_DEG + ALMANAC_SOLAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY * T));
    Lprime = almanac_degrees_to_radians(
        almanac_normalize_degrees(ALMANAC_LUNAR_MEAN_LONGITUDE_C0_DEG + ALMANAC_LUNAR_MEAN_LONGITUDE_C1_DEG_PER_CENTURY * T));

    for (i = 0u; i < almanac->nutation_term_count; ++i) {
        const almanac_nutation_term_t *term = &almanac->nutation_terms[i];
        double argument = term->multiplier_L * L + term->multiplier_Lprime * Lprime + term->multiplier_omega * omega;

        dpsi_arcseconds += term->sin_coeff_arcsec * sin(argument);
        deps_arcseconds += term->cos_coeff_arcsec * cos(argument);
    }

    if (delta_psi_radians)
        *delta_psi_radians = almanac_degrees_to_radians(dpsi_arcseconds / 3600.0);
    if (delta_epsilon_radians)
        *delta_epsilon_radians = almanac_degrees_to_radians(deps_arcseconds / 3600.0);
    return true;
}

double almanac_apparent_gha_aries_for_jd(almanac_t *almanac, double jd)
{
    double epsilon0 = almanac_mean_obliquity_radians(almanac, jd);
    double delta_psi = 0.0;
    double delta_epsilon = 0.0;
    double equation_of_equinoxes_degrees;

    if (epsilon0 == DBL_MAX || !almanac_nutation_angles(almanac, jd, &delta_psi, &delta_epsilon))
        return DBL_MAX;
    equation_of_equinoxes_degrees = almanac_radians_to_degrees(delta_psi * cos(epsilon0 + delta_epsilon));
    return almanac_normalize_degrees(almanac_gha_aries_for_jd(almanac, jd) + equation_of_equinoxes_degrees);
}

static void almanac_eval_frame_rotation_segment(const almanac_frame_rotation_segment_t *segment, double jd,
    double matrix[3][3])
{
    double x;
    int row;
    int column;

    if (!segment || !matrix)
        return;
    x = (jd - segment->reference_jd) / segment->radius_days;
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            int component = row * 3 + column;

            matrix[row][column] = almanac_chebyshev_eval(segment->coeff[component], segment->degree, x);
        }
    }
}

bool almanac_true_equatorial_from_ecliptic(almanac_t *almanac, const cartesian3_t *ecliptic, double jd,
    cartesian3_t *out_equatorial)
{
    almanac_frame_rotation_segment_t rotation_segment;
    double T;
    double epsilon_j2000;
    double epsilon_date;
    double cos_epsilon;
    double sin_epsilon;
    double zeta;
    double theta;
    double z;
    double delta_psi;
    double delta_epsilon;
    cartesian3_t equatorial;
    double x;
    double y;

    if (!almanac || !ecliptic || !out_equatorial)
        return false;

    if (almanac_fetch_frame_rotation_segment(almanac, jd, &rotation_segment)) {
        double matrix[3][3];
        double in_x = ecliptic->x;
        double in_y = ecliptic->y;
        double in_z = ecliptic->z;

        almanac_eval_frame_rotation_segment(&rotation_segment, jd, matrix);
        out_equatorial->x = matrix[0][0] * in_x + matrix[0][1] * in_y + matrix[0][2] * in_z;
        out_equatorial->y = matrix[1][0] * in_x + matrix[1][1] * in_y + matrix[1][2] * in_z;
        out_equatorial->z = matrix[2][0] * in_x + matrix[2][1] * in_y + matrix[2][2] * in_z;
        return true;
    }
    if (almanac->error)
        string_clear(almanac->error);

    T = (jd - 2451545.0) / 36525.0;
    epsilon_j2000 = almanac_mean_obliquity_radians(almanac, 2451545.0);
    epsilon_date = almanac_mean_obliquity_radians(almanac, jd);
    if (epsilon_j2000 == DBL_MAX || epsilon_date == DBL_MAX)
        return false;

    equatorial = cartesian_rotate_x(ecliptic, epsilon_j2000);

    zeta = almanac_degrees_to_radians(
        ((ALMANAC_PRECESSION_ZETA_C3_DEG * T + ALMANAC_PRECESSION_ZETA_C2_DEG) * T + ALMANAC_PRECESSION_ZETA_C1_DEG) *
        T);
    theta = almanac_degrees_to_radians(((ALMANAC_PRECESSION_THETA_C3_DEG * T + ALMANAC_PRECESSION_THETA_C2_DEG) * T +
                                ALMANAC_PRECESSION_THETA_C1_DEG) *
                               T);
    z = almanac_degrees_to_radians(
        ((ALMANAC_PRECESSION_Z_C3_DEG * T + ALMANAC_PRECESSION_Z_C2_DEG) * T + ALMANAC_PRECESSION_Z_C1_DEG) * T);

    equatorial = cartesian_rotate_z(&equatorial, zeta);
    equatorial = cartesian_rotate_y(&equatorial, -theta);
    equatorial = cartesian_rotate_z(&equatorial, z);

    if (!almanac_nutation_angles(almanac, jd, &delta_psi, &delta_epsilon))
        return false;

    cos_epsilon = cos(epsilon_date);
    sin_epsilon = sin(epsilon_date);
    x = equatorial.x;
    y = equatorial.y;
    equatorial.x = x - y * delta_psi * cos_epsilon - equatorial.z * delta_psi * sin_epsilon;
    equatorial.y = x * delta_psi * cos_epsilon + y - equatorial.z * delta_epsilon;
    equatorial.z = x * delta_psi * sin_epsilon + y * delta_epsilon + equatorial.z;

    *out_equatorial = equatorial;
    return true;
}

/* Compute Greenwich Hour Angle of Aries for a moment. */
bool almanac_gha_aries(almanac_t *almanac, const datetime_t *moment, double *gha_aries_degrees)
{
    if (!almanac || !moment || !gha_aries_degrees) {
        almanac_set_error(almanac, "invalid GHA Aries request");
        return false;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return false;
    }
    *gha_aries_degrees = almanac_gha_aries_for_jd(almanac, datetime_jd(moment));
    if (*gha_aries_degrees == DBL_MAX) {
        if (!string_length(almanac->error))
            almanac_set_error(almanac, "failed to compute GHA of Aries");
        return false;
    }
    return true;
}
