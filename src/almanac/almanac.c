#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "almanac.h"
#include "almanac_cartesian.h"
#include "sqlite.h"
#include "ustring.h"

typedef struct _almanac_t {
    sqlite_t *db;
    string_t *error;
    bool correction_model_loaded;
    double gmst_base_deg;
    double gmst_rate_deg_per_day;
    double gmst_quadratic_deg;
    double gmst_cubic_divisor;
    double mean_obliquity_c0_arcsec;
    double mean_obliquity_c1_arcsec;
    double mean_obliquity_c2_arcsec;
    double mean_obliquity_c3_arcsec;
    double precession_zeta_c1_deg;
    double precession_zeta_c2_deg;
    double precession_zeta_c3_deg;
    double precession_theta_c1_deg;
    double precession_theta_c2_deg;
    double precession_theta_c3_deg;
    double precession_z_c1_deg;
    double precession_z_c2_deg;
    double precession_z_c3_deg;
    double omega_c0_deg;
    double omega_c1_deg_per_century;
    double omega_c2_deg_per_century2;
    double omega_c3_century3_divisor;
    double solar_mean_longitude_c0_deg;
    double solar_mean_longitude_c1_deg_per_century;
    double lunar_mean_longitude_c0_deg;
    double lunar_mean_longitude_c1_deg_per_century;
    double moon_earth_system_mass_fraction;
    double speed_of_light_au_per_day;
    struct almanac_nutation_term_t *nutation_terms;
    size_t nutation_term_count;
} almanac_t;

typedef struct almanac_nutation_term_t {
    int multiplier_L;
    int multiplier_Lprime;
    int multiplier_omega;
    double sin_coeff_arcsec;
    double cos_coeff_arcsec;
} almanac_nutation_term_t;

typedef struct almanac_model_row_t {
    char body_code[32];
    char display_name[96];
    char body_kind[16];
    char model_kind[32];
    char brightness_model[32];
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

#define ALMANAC_CHEB_COMPONENT_COUNT 3
#define ALMANAC_FRAME_ROTATION_COMPONENT_COUNT 9
#define ALMANAC_CHEB_MAX_COEFF_COUNT 33

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

static bool almanac_lunar_geocentric_ecliptic_vector(almanac_t *almanac,
                                                     double jd,
                                                     cartesian3_t *out);
static bool almanac_lunar_geocentric_ecliptic_velocity(almanac_t *almanac,
                                                       double jd,
                                                       cartesian3_t *out_velocity_au_per_day);

static void almanac_set_error(almanac_t *almanac, const char *message)
{
    if (!almanac || !almanac->error)
        return;
    string_clear(almanac->error);
    (void)string_append_cstr(almanac->error, message ? message : "almanac error");
}

static void almanac_set_sqlite_error(almanac_t *almanac)
{
    const string_t *sqlite_error;

    if (!almanac)
        return;
    sqlite_error = almanac->db ? sqlite_last_error(almanac->db) : NULL;
    almanac_set_error(almanac, sqlite_error ? string_c_str(sqlite_error) : "almanac sqlite error");
}

static double normalize_degrees(double degrees)
{
    double value = fmod(degrees, 360.0);

    if (value < 0.0)
        value += 360.0;
    return value;
}

static double degrees_to_radians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

static double radians_to_degrees(double radians)
{
    return radians * (180.0 / M_PI);
}

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

static char *dup_c_string(const char *text)
{
    size_t len;
    char *out;

    if (!text)
        return NULL;
    len = strlen(text);
    out = malloc(len + 1u);
    if (!out)
        return NULL;
    memcpy(out, text, len + 1u);
    return out;
}

static char *dup_printf_path3(const char *a, const char *b, const char *c)
{
    size_t len_a;
    size_t len_b;
    size_t len_c;
    char *out;

    if (!a || !b || !c)
        return NULL;
    len_a = strlen(a);
    len_b = strlen(b);
    len_c = strlen(c);
    out = malloc(len_a + len_b + len_c + 1u);
    if (!out)
        return NULL;
    memcpy(out, a, len_a);
    memcpy(out + len_a, b, len_b);
    memcpy(out + len_a + len_b, c, len_c);
    out[len_a + len_b + len_c] = '\0';
    return out;
}

static char *mars_home_path(void)
{
    const char *mars_home = getenv("MARS_HOME");
    const char *home = getenv("HOME");

    if (mars_home && *mars_home)
        return dup_c_string(mars_home);
    if (home && *home)
        return dup_printf_path3(home, "/.mars", "");
    return NULL;
}

static char *almanac_default_db_path(void)
{
    char *mars_home = mars_home_path();
    char *path;

    if (!mars_home)
        return NULL;
    path = dup_printf_path3(mars_home, "/almanac/", "almanac.db");
    free(mars_home);
    return path;
}

static char *almanac_config_path(void)
{
    char *mars_home = mars_home_path();
    char *path;

    if (!mars_home)
        return NULL;
    path = dup_printf_path3(mars_home, "/config/", "almanac-db.env");
    free(mars_home);
    return path;
}

static void trim_ascii_whitespace(char *text)
{
    char *start;
    char *end;

    if (!text || *text == '\0')
        return;
    start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;
    if (start != text)
        memmove(text, start, strlen(start) + 1u);
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
}

static char *unquote_shell_value(const char *raw_value)
{
    char *value;
    size_t len;

    if (!raw_value)
        return NULL;
    value = dup_c_string(raw_value);
    if (!value)
        return NULL;
    trim_ascii_whitespace(value);
    len = strlen(value);
    if (len >= 2u &&
        ((value[0] == '"' && value[len - 1u] == '"') ||
         (value[0] == '\'' && value[len - 1u] == '\''))) {
        memmove(value, value + 1, len - 2u);
        value[len - 2u] = '\0';
    }
    return value;
}

static char *almanac_config_lookup(const char *key_name)
{
    FILE *fp;
    char *config_path;
    char line[4096];
    char *result = NULL;

    if (!key_name || *key_name == '\0')
        return NULL;

    config_path = almanac_config_path();
    if (!config_path)
        return NULL;

    fp = fopen(config_path, "r");
    free(config_path);
    if (!fp)
        return NULL;

    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        char *name;

        if (!eq)
            continue;
        *eq = '\0';
        name = line;
        trim_ascii_whitespace(name);
        if (strncmp(name, "export ", 7u) == 0) {
            name += 7;
            trim_ascii_whitespace(name);
        }
        if (*name == '\0' || *name == '#')
            continue;
        if (strcmp(name, key_name) != 0)
            continue;
        result = unquote_shell_value(eq + 1);
        break;
    }

    fclose(fp);
    return result;
}

static string_t *string_new_from_cstr(const char *text)
{
    if (!text || *text == '\0')
        return NULL;
    return string_new_with(text);
}

static bool sqlite_bind_text_cstr(sqlite_stmt_t *stmt, int index, const char *value)
{
    return sqlite_stmt_bind_text(stmt, index, value);
}

static bool sqlite_column_text_copy(sqlite_stmt_t *stmt,
                                    int column,
                                    char *out,
                                    size_t out_size)
{
    const char *text = sqlite_stmt_column_text(stmt, column);

    if (!out || out_size == 0u)
        return false;
    if (!text) {
        out[0] = '\0';
        return true;
    }
    snprintf(out, out_size, "%s", (const char *)text);
    return true;
}

static bool almanac_fetch_constant(almanac_t *almanac,
                                   const char *constant_name,
                                   double *out_value)
{
    static const char *sql =
        "SELECT constant_value "
        "FROM almanac_constant "
        "WHERE constant_name = ?1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !almanac->db || !constant_name || !out_value) {
        almanac_set_error(almanac, "invalid almanac constant lookup");
        return false;
    }

    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_text(stmt, 1, constant_name)) {
        const string_t *stmt_error = sqlite_stmt_last_error(stmt);

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind almanac constant lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "required almanac constant was not found");
        return false;
    }

    *out_value = sqlite_stmt_column_double(stmt, 0);
    sqlite_stmt_finalize(stmt);
    return true;
}

static bool almanac_load_nutation_terms(almanac_t *almanac)
{
    static const char *sql =
        "SELECT multiplier_L, multiplier_Lprime, multiplier_omega, sin_coeff_arcsec, cos_coeff_arcsec "
        "FROM almanac_nutation_term "
        "ORDER BY sort_order ASC, term_id ASC";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    almanac_nutation_term_t *terms = NULL;
    size_t count = 0u;

    if (!almanac || !almanac->db) {
        almanac_set_error(almanac, "invalid nutation model lookup");
        return false;
    }

    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        almanac_nutation_term_t *grown =
            realloc(terms, (count + 1u) * sizeof(*terms));

        if (!grown) {
            sqlite_stmt_finalize(stmt);
            free(terms);
            almanac_set_error(almanac, "failed to allocate nutation term storage");
            return false;
        }
        terms = grown;
        terms[count].multiplier_L = sqlite_stmt_column_int(stmt, 0);
        terms[count].multiplier_Lprime = sqlite_stmt_column_int(stmt, 1);
        terms[count].multiplier_omega = sqlite_stmt_column_int(stmt, 2);
        terms[count].sin_coeff_arcsec = sqlite_stmt_column_double(stmt, 3);
        terms[count].cos_coeff_arcsec = sqlite_stmt_column_double(stmt, 4);
        count++;
    }

    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        free(terms);
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (count == 0u) {
        free(terms);
        almanac_set_error(almanac, "no nutation terms are configured in the almanac database");
        return false;
    }

    almanac->nutation_terms = terms;
    almanac->nutation_term_count = count;
    return true;
}

static bool almanac_fetch_nutation_segment(almanac_t *almanac,
                                           double jd,
                                           almanac_nutation_segment_t *out)
{
    static const char *sql =
        "SELECT start_jd, end_jd, reference_jd, span_days, "
        "       dpsi_c0_rad, dpsi_c1_rad, dpsi_c2_rad, dpsi_c3_rad, "
        "       dpsi_c4_rad, dpsi_c5_rad, dpsi_c6_rad, dpsi_c7_rad, "
        "       deps_c0_rad, deps_c1_rad, deps_c2_rad, deps_c3_rad, "
        "       deps_c4_rad, deps_c5_rad, deps_c6_rad, deps_c7_rad "
        "FROM almanac_nutation_model "
        "WHERE start_jd <= ? AND end_jd > ? "
        "ORDER BY sort_order ASC, model_id ASC "
        "LIMIT 1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid nutation segment lookup");
        return false;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    sqlite_stmt_bind_double(stmt, 1, jd);
    sqlite_stmt_bind_double(stmt, 2, jd);
    rc = sqlite_stmt_step(stmt);
    if (rc == SQLITE_STEP_DONE) {
        sqlite_stmt_finalize(stmt);
        return false;
    }
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        almanac_set_sqlite_error(almanac);
        return false;
    }

    out->start_jd = sqlite_stmt_column_double(stmt, 0);
    out->end_jd = sqlite_stmt_column_double(stmt, 1);
    out->reference_jd = sqlite_stmt_column_double(stmt, 2);
    out->span_days = sqlite_stmt_column_double(stmt, 3);
    out->dpsi.c0 = sqlite_stmt_column_double(stmt, 4);
    out->dpsi.c1 = sqlite_stmt_column_double(stmt, 5);
    out->dpsi.c2 = sqlite_stmt_column_double(stmt, 6);
    out->dpsi.c3 = sqlite_stmt_column_double(stmt, 7);
    out->dpsi.c4 = sqlite_stmt_column_double(stmt, 8);
    out->dpsi.c5 = sqlite_stmt_column_double(stmt, 9);
    out->dpsi.c6 = sqlite_stmt_column_double(stmt, 10);
    out->dpsi.c7 = sqlite_stmt_column_double(stmt, 11);
    out->deps.c0 = sqlite_stmt_column_double(stmt, 12);
    out->deps.c1 = sqlite_stmt_column_double(stmt, 13);
    out->deps.c2 = sqlite_stmt_column_double(stmt, 14);
    out->deps.c3 = sqlite_stmt_column_double(stmt, 15);
    out->deps.c4 = sqlite_stmt_column_double(stmt, 16);
    out->deps.c5 = sqlite_stmt_column_double(stmt, 17);
    out->deps.c6 = sqlite_stmt_column_double(stmt, 18);
    out->deps.c7 = sqlite_stmt_column_double(stmt, 19);
    sqlite_stmt_finalize(stmt);
    return out->span_days > 0.0;
}

static bool almanac_load_correction_model(almanac_t *almanac)
{
    if (!almanac)
        return false;
    if (almanac->correction_model_loaded)
        return true;

    if (!almanac_fetch_constant(almanac, "gmst_base_deg", &almanac->gmst_base_deg) ||
        !almanac_fetch_constant(almanac, "gmst_rate_deg_per_day", &almanac->gmst_rate_deg_per_day) ||
        !almanac_fetch_constant(almanac, "gmst_quadratic_deg", &almanac->gmst_quadratic_deg) ||
        !almanac_fetch_constant(almanac, "gmst_cubic_divisor", &almanac->gmst_cubic_divisor) ||
        !almanac_fetch_constant(almanac, "mean_obliquity_c0_arcsec", &almanac->mean_obliquity_c0_arcsec) ||
        !almanac_fetch_constant(almanac, "mean_obliquity_c1_arcsec", &almanac->mean_obliquity_c1_arcsec) ||
        !almanac_fetch_constant(almanac, "mean_obliquity_c2_arcsec", &almanac->mean_obliquity_c2_arcsec) ||
        !almanac_fetch_constant(almanac, "mean_obliquity_c3_arcsec", &almanac->mean_obliquity_c3_arcsec) ||
        !almanac_fetch_constant(almanac, "precession_zeta_c1_deg", &almanac->precession_zeta_c1_deg) ||
        !almanac_fetch_constant(almanac, "precession_zeta_c2_deg", &almanac->precession_zeta_c2_deg) ||
        !almanac_fetch_constant(almanac, "precession_zeta_c3_deg", &almanac->precession_zeta_c3_deg) ||
        !almanac_fetch_constant(almanac, "precession_theta_c1_deg", &almanac->precession_theta_c1_deg) ||
        !almanac_fetch_constant(almanac, "precession_theta_c2_deg", &almanac->precession_theta_c2_deg) ||
        !almanac_fetch_constant(almanac, "precession_theta_c3_deg", &almanac->precession_theta_c3_deg) ||
        !almanac_fetch_constant(almanac, "precession_z_c1_deg", &almanac->precession_z_c1_deg) ||
        !almanac_fetch_constant(almanac, "precession_z_c2_deg", &almanac->precession_z_c2_deg) ||
        !almanac_fetch_constant(almanac, "precession_z_c3_deg", &almanac->precession_z_c3_deg) ||
        !almanac_fetch_constant(almanac, "omega_c0_deg", &almanac->omega_c0_deg) ||
        !almanac_fetch_constant(almanac, "omega_c1_deg_per_century", &almanac->omega_c1_deg_per_century) ||
        !almanac_fetch_constant(almanac, "omega_c2_deg_per_century2", &almanac->omega_c2_deg_per_century2) ||
        !almanac_fetch_constant(almanac, "omega_c3_century3_divisor", &almanac->omega_c3_century3_divisor) ||
        !almanac_fetch_constant(almanac, "solar_mean_longitude_c0_deg", &almanac->solar_mean_longitude_c0_deg) ||
        !almanac_fetch_constant(almanac, "solar_mean_longitude_c1_deg_per_century", &almanac->solar_mean_longitude_c1_deg_per_century) ||
        !almanac_fetch_constant(almanac, "lunar_mean_longitude_c0_deg", &almanac->lunar_mean_longitude_c0_deg) ||
        !almanac_fetch_constant(almanac, "lunar_mean_longitude_c1_deg_per_century", &almanac->lunar_mean_longitude_c1_deg_per_century) ||
        !almanac_fetch_constant(almanac, "moon_earth_system_mass_fraction", &almanac->moon_earth_system_mass_fraction) ||
        !almanac_fetch_constant(almanac, "speed_of_light_au_per_day", &almanac->speed_of_light_au_per_day) ||
        !almanac_load_nutation_terms(almanac)) {
        return false;
    }

    almanac->correction_model_loaded = true;
    return true;
}

static bool almanac_fetch_model(almanac_t *almanac,
                                const char *body_code,
                                almanac_model_row_t *out)
{
    static const char *sql =
        "SELECT b.body_code, b.display_name, b.body_kind, b.model_kind, b.brightness_model, b.sort_order,"
        "       b.magnitude_constant, b.magnitude_linear, b.magnitude_quadratic, b.magnitude_cubic, b.magnitude_quartic,"
        "       f.epoch_jd, f.ra_hours, f.dec_degrees, f.pm_ra_mas_per_year, f.pm_dec_mas_per_year "
        "FROM almanac_body AS b "
        "LEFT JOIN almanac_fixed_equatorial_model AS f ON f.body_code = b.body_code "
        "WHERE b.body_code = ?1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !almanac->db || !body_code || !out) {
        almanac_set_error(almanac, "invalid almanac lookup");
        return false;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_bind_text_cstr(stmt, 1, body_code)) {
        const string_t *stmt_error = sqlite_stmt_last_error(stmt);

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind almanac lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "requested almanac body was not found");
        return false;
    }

    sqlite_column_text_copy(stmt, 0, out->body_code, sizeof(out->body_code));
    sqlite_column_text_copy(stmt, 1, out->display_name, sizeof(out->display_name));
    sqlite_column_text_copy(stmt, 2, out->body_kind, sizeof(out->body_kind));
    sqlite_column_text_copy(stmt, 3, out->model_kind, sizeof(out->model_kind));
    sqlite_column_text_copy(stmt, 4, out->brightness_model, sizeof(out->brightness_model));
    out->sort_order = sqlite_stmt_column_int(stmt, 5);
    out->magnitude_constant = sqlite_stmt_column_double(stmt, 6);
    out->magnitude_linear = sqlite_stmt_column_double(stmt, 7);
    out->magnitude_quadratic = sqlite_stmt_column_double(stmt, 8);
    out->magnitude_cubic = sqlite_stmt_column_double(stmt, 9);
    out->magnitude_quartic = sqlite_stmt_column_double(stmt, 10);
    out->fixed_epoch_jd = sqlite_stmt_column_double(stmt, 11);
    out->fixed_ra_hours = sqlite_stmt_column_double(stmt, 12);
    out->fixed_dec_degrees = sqlite_stmt_column_double(stmt, 13);
    out->fixed_pm_ra_mas_per_year = sqlite_stmt_column_double(stmt, 14);
    out->fixed_pm_dec_mas_per_year = sqlite_stmt_column_double(stmt, 15);

    sqlite_stmt_finalize(stmt);
    return true;
}

static double almanac_gha_aries_for_jd(almanac_t *almanac, double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double gmst;

    if (!almanac_load_correction_model(almanac))
        return DBL_MAX;
    gmst = almanac->gmst_base_deg
         + almanac->gmst_rate_deg_per_day * (jd - 2451545.0)
         + almanac->gmst_quadratic_deg * T * T
         - (T * T * T) / almanac->gmst_cubic_divisor;

    return normalize_degrees(gmst);
}

static double almanac_mean_obliquity_radians(almanac_t *almanac, double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double arcseconds;

    if (!almanac_load_correction_model(almanac))
        return DBL_MAX;
    arcseconds = almanac->mean_obliquity_c0_arcsec
               + almanac->mean_obliquity_c1_arcsec * T
               + almanac->mean_obliquity_c2_arcsec * T * T
               + almanac->mean_obliquity_c3_arcsec * T * T * T;

    return degrees_to_radians(arcseconds / 3600.0);
}

static bool almanac_nutation_angles(almanac_t *almanac,
                                    double jd,
                                    double *delta_psi_radians,
                                    double *delta_epsilon_radians)
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

    omega = degrees_to_radians(normalize_degrees(almanac->omega_c0_deg +
                                                 almanac->omega_c1_deg_per_century * T +
                                                 almanac->omega_c2_deg_per_century2 * T * T +
                                                 (T * T * T) / almanac->omega_c3_century3_divisor));
    L = degrees_to_radians(normalize_degrees(almanac->solar_mean_longitude_c0_deg +
                                             almanac->solar_mean_longitude_c1_deg_per_century * T));
    Lprime = degrees_to_radians(normalize_degrees(almanac->lunar_mean_longitude_c0_deg +
                                                  almanac->lunar_mean_longitude_c1_deg_per_century * T));

    for (i = 0u; i < almanac->nutation_term_count; ++i) {
        const almanac_nutation_term_t *term = &almanac->nutation_terms[i];
        double argument = term->multiplier_L * L
                        + term->multiplier_Lprime * Lprime
                        + term->multiplier_omega * omega;

        dpsi_arcseconds += term->sin_coeff_arcsec * sin(argument);
        deps_arcseconds += term->cos_coeff_arcsec * cos(argument);
    }

    if (delta_psi_radians)
        *delta_psi_radians = degrees_to_radians(dpsi_arcseconds / 3600.0);
    if (delta_epsilon_radians)
        *delta_epsilon_radians = degrees_to_radians(deps_arcseconds / 3600.0);
    return true;
}

static double almanac_apparent_gha_aries_for_jd(almanac_t *almanac, double jd)
{
    double epsilon0 = almanac_mean_obliquity_radians(almanac, jd);
    double delta_psi = 0.0;
    double delta_epsilon = 0.0;
    double equation_of_equinoxes_degrees;

    if (epsilon0 == DBL_MAX || !almanac_nutation_angles(almanac, jd, &delta_psi, &delta_epsilon))
        return DBL_MAX;
    equation_of_equinoxes_degrees =
        radians_to_degrees(delta_psi * cos(epsilon0 + delta_epsilon));
    return normalize_degrees(almanac_gha_aries_for_jd(almanac, jd) + equation_of_equinoxes_degrees);
}

static bool almanac_body_kind_from_text(const char *text, almanac_body_kind_t *out)
{
    if (!text || !out)
        return false;
    if (strcmp(text, "star") == 0) {
        *out = ALMANAC_BODY_STAR;
        return true;
    }
    if (strcmp(text, "sun") == 0) {
        *out = ALMANAC_BODY_SUN;
        return true;
    }
    if (strcmp(text, "moon") == 0) {
        *out = ALMANAC_BODY_MOON;
        return true;
    }
    if (strcmp(text, "planet") == 0) {
        *out = ALMANAC_BODY_PLANET;
        return true;
    }
    return false;
}

static bool almanac_fetch_chebyshev_position_segment(almanac_t *almanac,
                                                     const char *body_code,
                                                     double jd,
                                                     almanac_chebyshev_position_segment_t *out)
{
    static const char *sql =
        "SELECT selected.start_jd, selected.end_jd, selected.segment_span_days, "
        "       selected.degree, selected.segment_index, segment.coefficient_blob "
        "FROM ( "
        "    SELECT series_id, start_jd, end_jd, segment_span_days, degree, "
        "           CASE WHEN ?2 >= end_jd THEN segment_count - 1 "
        "                ELSE CAST((?2 - start_jd) / segment_span_days AS INTEGER) "
        "           END AS segment_index "
        "    FROM almanac_chebyshev_position_series AS series "
        "    JOIN almanac_body_ref AS body_ref "
        "      ON body_ref.body_ref_id = series.body_ref_id "
        "    JOIN almanac_frame AS frame "
        "      ON frame.frame_id = series.frame_id "
        "    WHERE body_ref.body_code = ?1 AND frame.frame_code = 'ECLIPJ2000' "
        "      AND series.start_jd <= ?2 AND series.end_jd >= ?2 "
        "    ORDER BY series.start_jd ASC "
        "    LIMIT 1 "
        ") AS selected "
        "JOIN almanac_chebyshev_position_segment AS segment "
        "  ON segment.series_id = selected.series_id "
        " AND segment.segment_index = selected.segment_index";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *blob;
    size_t blob_size;
    size_t expected_size;
    int component;
    int coeff;
    int degree;
    int segment_index;
    double series_start_jd;
    double series_end_jd;
    double segment_span_days;

    if (!almanac || !almanac->db || !body_code || !out) {
        almanac_set_error(almanac, "invalid Chebyshev position segment lookup");
        return false;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_text(stmt, 1, body_code) ||
        !sqlite_stmt_bind_double(stmt, 2, jd)) {
        const string_t *stmt_error = sqlite_stmt_last_error(stmt);

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind Chebyshev lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        if (rc == SQLITE_STEP_DONE)
            almanac_set_error(almanac, "no Chebyshev position segment matched the requested date");
        else
            almanac_set_sqlite_error(almanac);
        return false;
    }

    series_start_jd = sqlite_stmt_column_double(stmt, 0);
    series_end_jd = sqlite_stmt_column_double(stmt, 1);
    segment_span_days = sqlite_stmt_column_double(stmt, 2);
    degree = sqlite_stmt_column_int(stmt, 3);
    segment_index = sqlite_stmt_column_int(stmt, 4);
    blob = sqlite_stmt_column_blob(stmt, 5);
    blob_size = sqlite_stmt_column_bytes(stmt, 5);

    if (degree < 1 || degree >= ALMANAC_CHEB_MAX_COEFF_COUNT ||
        segment_index < 0 || segment_span_days <= 0.0 || !blob) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "Chebyshev position segment is malformed");
        return false;
    }
    out->start_jd = series_start_jd + (double)segment_index * segment_span_days;
    out->end_jd = out->start_jd + segment_span_days;
    if (out->end_jd > series_end_jd)
        out->end_jd = series_end_jd;
    out->reference_jd = 0.5 * (out->start_jd + out->end_jd);
    out->radius_days = 0.5 * (out->end_jd - out->start_jd);
    if (out->radius_days <= 0.0) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "Chebyshev position segment has invalid time span");
        return false;
    }
    expected_size = (size_t)ALMANAC_CHEB_COMPONENT_COUNT * (size_t)(degree + 1) * sizeof(double);
    if (blob_size != expected_size) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "Chebyshev position coefficient blob has invalid size");
        return false;
    }

    out->degree = degree;
    for (component = 0; component < ALMANAC_CHEB_COMPONENT_COUNT; ++component) {
        for (coeff = 0; coeff <= degree; ++coeff) {
            double value;

            memcpy(&value,
                   blob + sizeof(double) * ((size_t)component * (size_t)(degree + 1) + (size_t)coeff),
                   sizeof(value));
            out->coeff[component][coeff] = value;
        }
    }
    sqlite_stmt_finalize(stmt);
    return true;
}

static bool almanac_fetch_frame_rotation_segment(almanac_t *almanac,
                                                 double jd,
                                                 almanac_frame_rotation_segment_t *out)
{
    static const char *sql =
        "SELECT selected.start_jd, selected.end_jd, selected.segment_span_days, "
        "       selected.degree, selected.segment_index, segment.coefficient_blob "
        "FROM ( "
        "    SELECT series_id, start_jd, end_jd, segment_span_days, degree, "
        "           CASE WHEN ?1 >= end_jd THEN segment_count - 1 "
        "                ELSE CAST((?1 - start_jd) / segment_span_days AS INTEGER) "
        "           END AS segment_index "
        "    FROM almanac_frame_rotation_series AS series "
        "    JOIN almanac_frame AS source_frame "
        "      ON source_frame.frame_id = series.source_frame_id "
        "    JOIN almanac_frame AS target_frame "
        "      ON target_frame.frame_id = series.target_frame_id "
        "    WHERE source_frame.frame_code = 'ECLIPJ2000' "
        "      AND target_frame.frame_code = 'TRUE_EQUATOR_DATE' "
        "      AND series.start_jd <= ?1 AND series.end_jd >= ?1 "
        "    ORDER BY series.start_jd ASC "
        "    LIMIT 1 "
        ") AS selected "
        "JOIN almanac_frame_rotation_segment AS segment "
        "  ON segment.series_id = selected.series_id "
        " AND segment.segment_index = selected.segment_index";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *blob;
    size_t blob_size;
    size_t expected_size;
    int component;
    int coeff;
    int degree;
    int segment_index;
    double series_start_jd;
    double series_end_jd;
    double segment_span_days;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid frame rotation segment lookup");
        return false;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_double(stmt, 1, jd)) {
        const string_t *stmt_error = sqlite_stmt_last_error(stmt);

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind frame rotation lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        if (rc == SQLITE_STEP_DONE)
            almanac_set_error(almanac, "no frame rotation segment matched the requested date");
        else
            almanac_set_sqlite_error(almanac);
        return false;
    }

    series_start_jd = sqlite_stmt_column_double(stmt, 0);
    series_end_jd = sqlite_stmt_column_double(stmt, 1);
    segment_span_days = sqlite_stmt_column_double(stmt, 2);
    degree = sqlite_stmt_column_int(stmt, 3);
    segment_index = sqlite_stmt_column_int(stmt, 4);
    blob = sqlite_stmt_column_blob(stmt, 5);
    blob_size = sqlite_stmt_column_bytes(stmt, 5);

    if (degree < 1 || degree >= ALMANAC_CHEB_MAX_COEFF_COUNT ||
        segment_index < 0 || segment_span_days <= 0.0 || !blob) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "frame rotation segment is malformed");
        return false;
    }
    out->start_jd = series_start_jd + (double)segment_index * segment_span_days;
    out->end_jd = out->start_jd + segment_span_days;
    if (out->end_jd > series_end_jd)
        out->end_jd = series_end_jd;
    out->reference_jd = 0.5 * (out->start_jd + out->end_jd);
    out->radius_days = 0.5 * (out->end_jd - out->start_jd);
    if (out->radius_days <= 0.0) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "frame rotation segment has invalid time span");
        return false;
    }
    expected_size = (size_t)ALMANAC_FRAME_ROTATION_COMPONENT_COUNT * (size_t)(degree + 1) * sizeof(double);
    if (blob_size != expected_size) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "frame rotation coefficient blob has invalid size");
        return false;
    }

    out->degree = degree;
    for (component = 0; component < ALMANAC_FRAME_ROTATION_COMPONENT_COUNT; ++component) {
        for (coeff = 0; coeff <= degree; ++coeff) {
            double value;

            memcpy(&value,
                   blob + sizeof(double) * ((size_t)component * (size_t)(degree + 1) + (size_t)coeff),
                   sizeof(value));
            out->coeff[component][coeff] = value;
        }
    }
    sqlite_stmt_finalize(stmt);
    return true;
}

static double almanac_chebyshev_eval(const double *coeff, int degree, double x)
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

static void almanac_eval_chebyshev_position_segment(const almanac_chebyshev_position_segment_t *segment,
                                                    double jd,
                                                    cartesian3_t *position_au,
                                                    cartesian3_t *velocity_au_per_day)
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

static void almanac_eval_frame_rotation_segment(const almanac_frame_rotation_segment_t *segment,
                                                double jd,
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

static void almanac_state_fill_equatorial(almanac_state_t *state)
{
    double radius_xy;

    if (!state)
        return;
    radius_xy = sqrt(state->geocentric_equatorial_au.x * state->geocentric_equatorial_au.x +
                     state->geocentric_equatorial_au.y * state->geocentric_equatorial_au.y);
    state->right_ascension_hours =
        normalize_degrees(radians_to_degrees(atan2(state->geocentric_equatorial_au.y,
                                                   state->geocentric_equatorial_au.x))) / 15.0;
    state->declination_degrees =
        radians_to_degrees(atan2(state->geocentric_equatorial_au.z, radius_xy));
}

static bool almanac_earth_heliocentric_state(almanac_t *almanac,
                                             double jd,
                                             cartesian3_t *position_ecliptic_au,
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

    if (!almanac_fetch_chebyshev_position_segment(almanac, "EARTH_BARYCENTER", jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment,
                                            jd,
                                            &earth_moon_barycenter,
                                            have_velocity ? &earth_moon_barycenter_velocity : NULL);
    if (!almanac_lunar_geocentric_ecliptic_vector(almanac, jd, &moon_geocentric))
        return false;
    moon_offset = cartesian_scale(&moon_geocentric, almanac->moon_earth_system_mass_fraction);
    *position_ecliptic_au = cartesian_subtract(&earth_moon_barycenter, &moon_offset);
    if (have_velocity) {
        if (!almanac_lunar_geocentric_ecliptic_velocity(almanac, jd, &moon_geocentric_velocity))
            return false;
        moon_offset = cartesian_scale(&moon_geocentric_velocity, almanac->moon_earth_system_mass_fraction);
        *velocity_ecliptic_au_per_day =
            cartesian_subtract(&earth_moon_barycenter_velocity, &moon_offset);
    }
    return true;
}

static cartesian3_t equatorial_from_ecliptic_vector(const cartesian3_t *ecliptic,
                                                    almanac_t *almanac,
                                                    double jd)
{
    return cartesian_rotate_x(ecliptic, almanac_mean_obliquity_radians(almanac, jd));
}

static bool almanac_true_equatorial_from_ecliptic(almanac_t *almanac,
                                                 const cartesian3_t *ecliptic,
                                                 double jd,
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
        out_equatorial->x = matrix[0][0] * in_x
                          + matrix[0][1] * in_y
                          + matrix[0][2] * in_z;
        out_equatorial->y = matrix[1][0] * in_x
                          + matrix[1][1] * in_y
                          + matrix[1][2] * in_z;
        out_equatorial->z = matrix[2][0] * in_x
                          + matrix[2][1] * in_y
                          + matrix[2][2] * in_z;
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

    zeta = degrees_to_radians(((almanac->precession_zeta_c3_deg * T +
                                almanac->precession_zeta_c2_deg) * T +
                               almanac->precession_zeta_c1_deg) * T);
    theta = degrees_to_radians(((almanac->precession_theta_c3_deg * T +
                                 almanac->precession_theta_c2_deg) * T +
                                almanac->precession_theta_c1_deg) * T);
    z = degrees_to_radians(((almanac->precession_z_c3_deg * T +
                             almanac->precession_z_c2_deg) * T +
                            almanac->precession_z_c1_deg) * T);

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

static double almanac_relativistic_light_time_days(const cartesian3_t *earth_heliocentric_au,
                                                   const cartesian3_t *body_heliocentric_au,
                                                   const cartesian3_t *earth_to_body_au)
{
    const double gravitational_radius_twice_au = 1.9741257129241e-08;
    const double speed_of_light_au_per_day = 173.1446326846693;
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
        return earth_body_distance / speed_of_light_au_per_day;

    numerator = earth_sun_distance + earth_body_distance + body_sun_distance;
    denominator = earth_sun_distance - earth_body_distance + body_sun_distance;
    if (denominator <= 0.0 || numerator <= denominator)
        return earth_body_distance / speed_of_light_au_per_day;

    return (earth_body_distance + gravitational_radius_twice_au * log(numerator / denominator)) /
           speed_of_light_au_per_day;
}

static bool almanac_apply_gravitational_deflection(const cartesian3_t *earth_heliocentric_equatorial_au,
                                                   const cartesian3_t *mean_equatorial_direction,
                                                   const cartesian3_t *sun_to_body_direction_equatorial,
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

    if (!earth_heliocentric_equatorial_au || !mean_equatorial_direction ||
        !sun_to_body_direction_equatorial || !out_direction) {
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
                                            const cartesian3_t *earth_heliocentric_velocity_au_per_day,
                                            cartesian3_t *out_direction)
{
    const double speed_of_light_au_per_day = 173.1446326846693;
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

    beta = cartesian_scale(earth_heliocentric_velocity_au_per_day, 1.0 / speed_of_light_au_per_day);
    projection = cartesian_dot(&direction, &beta);
    parallel_component = cartesian_scale(&direction, projection);
    transverse_beta = cartesian_subtract(&beta, &parallel_component);
    *out_direction = cartesian_add(&direction, &transverse_beta);
    return cartesian_normalize_in_place(out_direction);
}

static bool almanac_apply_nutation(almanac_t *almanac,
                                   cartesian3_t *mean_equatorial_direction,
                                   double jd)
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

static bool almanac_apply_apparent_direction_corrections(almanac_t *almanac,
                                                         const almanac_model_row_t *model,
                                                         double jd,
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

        if (strcmp(model->body_code, "SUN") != 0) {
            if (strcmp(model->body_kind, "star") == 0) {
                sun_to_body_direction = direction;
            } else {
                sun_to_body_direction = cartesian_add(&earth_heliocentric_ecliptic,
                                                      &state->geocentric_ecliptic_au);
            }
            if (!almanac_apply_gravitational_deflection(&earth_heliocentric_ecliptic,
                                                        &direction,
                                                        &sun_to_body_direction,
                                                        &direction)) {
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
    if (strcmp(model->body_code, "SUN") != 0) {
        if (strcmp(model->body_kind, "star") == 0) {
            sun_to_body_direction = direction;
        } else {
            sun_to_body_direction = cartesian_add(&earth_heliocentric_equatorial, &state->geocentric_equatorial_au);
        }
        if (!almanac_apply_gravitational_deflection(&earth_heliocentric_equatorial,
                                                    &direction,
                                                    &sun_to_body_direction,
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

static bool almanac_lunar_geocentric_ecliptic_vector(almanac_t *almanac,
                                                     double jd,
                                                     cartesian3_t *out)
{
    almanac_chebyshev_position_segment_t cheb_segment;

    if (!out) {
        almanac_set_error(almanac, "invalid lunar ecliptic vector request");
        return false;
    }

    if (!almanac_fetch_chebyshev_position_segment(almanac, "MOON", jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment, jd, out, NULL);
    return true;
}

static bool almanac_lunar_geocentric_ecliptic_velocity(almanac_t *almanac,
                                                       double jd,
                                                       cartesian3_t *out_velocity_au_per_day)
{
    almanac_chebyshev_position_segment_t cheb_segment;

    if (!out_velocity_au_per_day) {
        almanac_set_error(almanac, "invalid lunar ecliptic velocity request");
        return false;
    }

    if (!almanac_fetch_chebyshev_position_segment(almanac, "MOON", jd, &cheb_segment))
        return false;
    almanac_eval_chebyshev_position_segment(&cheb_segment, jd, NULL, out_velocity_au_per_day);
    return true;
}

static bool almanac_equatorial_from_moon(almanac_t *almanac,
                                         const almanac_model_row_t *model,
                                         double jd,
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

        if (!almanac_lunar_geocentric_ecliptic_vector(almanac,
                                                      jd - light_time_days,
                                                      &state->geocentric_ecliptic_au))
            return false;
        distance_au = cartesian_length(&state->geocentric_ecliptic_au);
        if (distance_au <= 0.0)
            return false;
        light_time_days = distance_au / almanac->speed_of_light_au_per_day;
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

static bool almanac_equatorial_from_fixed(const almanac_model_row_t *model,
                                          double jd,
                                          almanac_state_t *state)
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
    alpha0 = degrees_to_radians(model->fixed_ra_hours * 15.0 +
                                (model->fixed_pm_ra_mas_per_year * years) / 3600000.0);
    delta0 = degrees_to_radians(model->fixed_dec_degrees +
                                (model->fixed_pm_dec_mas_per_year * years) / 3600000.0);

    T = (model->fixed_epoch_jd - j2000) / 36525.0;
    t = (jd - model->fixed_epoch_jd) / 36525.0;

    zeta = degrees_to_radians(
        (((2306.2181 + 1.39656 * T - 0.000139 * T * T) * t) +
         ((0.30188 - 0.000344 * T) * t * t) +
         (0.017998 * t * t * t)) / 3600.0);
    z = degrees_to_radians(
        (((2306.2181 + 1.39656 * T - 0.000139 * T * T) * t) +
         ((1.09468 + 0.000066 * T) * t * t) +
         (0.018203 * t * t * t)) / 3600.0);
    theta = degrees_to_radians(
        (((2004.3109 - 0.85330 * T - 0.000217 * T * T) * t) -
         ((0.42665 + 0.000217 * T) * t * t) -
         (0.041833 * t * t * t)) / 3600.0);

    A = cos(delta0) * sin(alpha0 + zeta);
    B = cos(theta) * cos(delta0) * cos(alpha0 + zeta) - sin(theta) * sin(delta0);
    C = sin(theta) * cos(delta0) * cos(alpha0 + zeta) + cos(theta) * sin(delta0);

    alpha = atan2(A, B) + z;
    delta = asin(C);

    state->right_ascension_hours = normalize_degrees(radians_to_degrees(alpha)) / 15.0;
    state->declination_degrees = radians_to_degrees(delta);
    state->has_geocentric_vector = false;
    state->has_direction_vector = true;
    state->geocentric_equatorial_au.x = cos(delta) * cos(alpha);
    state->geocentric_equatorial_au.y = cos(delta) * sin(alpha);
    state->geocentric_equatorial_au.z = sin(delta);
    return true;
}

static bool almanac_equatorial_from_chebyshev(almanac_t *almanac,
                                              const almanac_model_row_t *model,
                                              double jd,
                                              almanac_state_t *state)
{
    almanac_chebyshev_position_segment_t cheb_segment;
    cartesian3_t body_helio;
    cartesian3_t earth_helio;
    cartesian3_t geocentric;
    double light_time_days = 0.0;
    int iteration;

    if (!almanac || !model || !state)
        return false;
    if (!almanac_earth_heliocentric_state(almanac, jd, &earth_helio, NULL))
        return false;

    if (strcmp(model->body_code, "SUN") == 0) {
        geocentric.x = -earth_helio.x;
        geocentric.y = -earth_helio.y;
        geocentric.z = -earth_helio.z;
    } else {
        for (iteration = 0; iteration < 3; ++iteration) {
            if (!almanac_fetch_chebyshev_position_segment(almanac,
                                                          model->body_code,
                                                          jd - light_time_days,
                                                          &cheb_segment))
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
    if (!almanac_true_equatorial_from_ecliptic(almanac,
                                               &state->geocentric_ecliptic_au,
                                               jd,
                                               &state->geocentric_equatorial_au)) {
        return false;
    }
    state->has_geocentric_vector = true;
    state->has_ecliptic_vector = true;
    state->has_direction_vector = true;
    return true;
}

static double almanac_phase_angle_degrees(const cartesian3_t *earth_to_object,
                                          const cartesian3_t *earth_to_sun)
{
    cartesian3_t object_to_earth;
    cartesian3_t object_to_sun;
    double len_earth;
    double len_sun;
    double cosine_angle;

    if (!earth_to_object || !earth_to_sun)
        return NAN;
    object_to_earth = cartesian_negate(earth_to_object);
    object_to_sun = cartesian_subtract(earth_to_sun, earth_to_object);
    len_earth = cartesian_length(&object_to_earth);
    len_sun = cartesian_length(&object_to_sun);
    if (len_earth <= 0.0 || len_sun <= 0.0)
        return NAN;
    cosine_angle = cartesian_dot(&object_to_earth, &object_to_sun) / (len_earth * len_sun);
    if (cosine_angle > 1.0)
        cosine_angle = 1.0;
    if (cosine_angle < -1.0)
        cosine_angle = -1.0;
    return radians_to_degrees(acos(cosine_angle));
}

static bool almanac_visual_magnitude(const almanac_model_row_t *model,
                                     const almanac_state_t *state,
                                     const almanac_state_t *sun_state,
                                     double *magnitude,
                                     double *phase_angle_degrees,
                                     double *heliocentric_distance_au)
{
    double delta;
    double phase_angle;
    cartesian3_t sun_to_body;
    double r;

    if (!model || !magnitude || !phase_angle_degrees || !heliocentric_distance_au)
        return false;

    *magnitude = NAN;
    *phase_angle_degrees = NAN;
    *heliocentric_distance_au = NAN;

    if (strcmp(model->brightness_model, "none") == 0)
        return true;
    if (strcmp(model->brightness_model, "catalogued") == 0) {
        *magnitude = model->magnitude_constant;
        return true;
    }
    if (!state || !state->has_geocentric_vector)
        return true;

    delta = cartesian_length(&state->geocentric_equatorial_au);
    if (delta <= 0.0)
        return true;

    if (strcmp(model->brightness_model, "sun_distance") == 0) {
        *magnitude = model->magnitude_constant + 5.0 * log10(delta);
        *heliocentric_distance_au = 0.0;
        return true;
    }

    if (!sun_state || !sun_state->has_geocentric_vector)
        return true;

    phase_angle = almanac_phase_angle_degrees(&state->geocentric_equatorial_au,
                                              &sun_state->geocentric_equatorial_au);
    sun_to_body = cartesian_subtract(&state->geocentric_equatorial_au, &sun_state->geocentric_equatorial_au);
    r = cartesian_length(&sun_to_body);
    *phase_angle_degrees = phase_angle;
    *heliocentric_distance_au = r;

    if (!(phase_angle == phase_angle) || r <= 0.0)
        return true;

    if (strcmp(model->brightness_model, "planetary_phase") == 0) {
        *magnitude = model->magnitude_constant
                   + 5.0 * log10(r * delta)
                   + model->magnitude_linear * phase_angle
                   + model->magnitude_quadratic * phase_angle * phase_angle
                   + model->magnitude_cubic * phase_angle * phase_angle * phase_angle
                   + model->magnitude_quartic * phase_angle * phase_angle * phase_angle * phase_angle;
        return true;
    }

    if (strcmp(model->brightness_model, "lunar_phase") == 0) {
        *magnitude = model->magnitude_constant
                   + model->magnitude_linear * phase_angle
                   + model->magnitude_quadratic * phase_angle * phase_angle
                   + model->magnitude_cubic * phase_angle * phase_angle * phase_angle
                   + model->magnitude_quartic * phase_angle * phase_angle * phase_angle * phase_angle
                   + 5.0 * log10(delta / (384400.0 / 149597870.7));
        return true;
    }

    return true;
}

static bool almanac_compute_entry(almanac_t *almanac,
                                  const almanac_model_row_t *model,
                                  const datetime_t *moment,
                                  almanac_entry_t *out)
{
    double civil_jd;
    double ephemeris_jd;
    almanac_state_t state;
    almanac_state_t sun_state;

    if (!almanac || !model || !moment || !out) {
        almanac_set_error(almanac, "invalid almanac computation request");
        return false;
    }

    civil_jd = datetime_jd(moment);
    ephemeris_jd = datetime_jd_tdb(moment);
    if (civil_jd == DBL_MAX || ephemeris_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian dates for almanac computation");
        return false;
    }
    memset(&state, 0, sizeof(state));
    memset(&sun_state, 0, sizeof(sun_state));
    if (strcmp(model->model_kind, "fixed_equatorial") == 0) {
        if (!almanac_equatorial_from_fixed(model, ephemeris_jd, &state)) {
            almanac_set_error(almanac, "failed to compute fixed-star position");
            return false;
        }
    } else if (strcmp(model->model_kind, "chebyshev_position") == 0) {
        if (!almanac_equatorial_from_chebyshev(almanac, model, ephemeris_jd, &state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute Chebyshev position");
            return false;
        }
    } else if (strcmp(model->model_kind, "lunar_chebyshev") == 0) {
        if (!almanac_equatorial_from_moon(almanac, model, ephemeris_jd, &state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute lunar position");
            return false;
        }
    } else {
        almanac_set_error(almanac, "unsupported almanac model kind");
        return false;
    }

    if (!state.apparent_place_complete &&
        !almanac_apply_apparent_direction_corrections(almanac, model, ephemeris_jd, &state)) {
        if (!string_length(almanac->error))
            almanac_set_error(almanac, "failed to apply apparent-place corrections");
        return false;
    }

    if (state.has_direction_vector)
        almanac_state_fill_equatorial(&state);

    memset(out, 0, sizeof(*out));
    snprintf(out->body_code, sizeof(out->body_code), "%s", model->body_code);
    snprintf(out->display_name, sizeof(out->display_name), "%s", model->display_name);
    if (!almanac_body_kind_from_text(model->body_kind, &out->body_kind)) {
        almanac_set_error(almanac, "unsupported almanac body kind");
        return false;
    }
    out->gha_aries_degrees = almanac_apparent_gha_aries_for_jd(almanac, civil_jd);
    if (out->gha_aries_degrees == DBL_MAX) {
        almanac_set_error(almanac, "failed to compute apparent GHA of Aries");
        return false;
    }
    out->right_ascension_hours = state.right_ascension_hours;
    out->sha_degrees = normalize_degrees(360.0 - (state.right_ascension_hours * 15.0));
    out->declination_degrees = state.declination_degrees;
    out->geocentric_distance_au = state.has_geocentric_vector ? cartesian_length(&state.geocentric_equatorial_au) : NAN;
    out->heliocentric_distance_au = NAN;
    out->phase_angle_degrees = NAN;
    out->visual_magnitude = NAN;

    if (strcmp(model->brightness_model, "catalogued") != 0 &&
        strcmp(model->brightness_model, "none") != 0 &&
        strcmp(model->body_code, "SUN") != 0) {
        almanac_model_row_t sun_model;

        if (!almanac_fetch_model(almanac, "SUN", &sun_model))
            return false;
        if (strcmp(sun_model.model_kind, "chebyshev_position") == 0) {
            if (!almanac_equatorial_from_chebyshev(almanac, &sun_model, ephemeris_jd, &sun_state))
                return false;
        }
        if (sun_state.has_geocentric_vector)
            almanac_state_fill_equatorial(&sun_state);
    }

    if (!almanac_visual_magnitude(model,
                                  &state,
                                  strcmp(model->body_code, "SUN") == 0 ? NULL : &sun_state,
                                  &out->visual_magnitude,
                                  &out->phase_angle_degrees,
                                  &out->heliocentric_distance_au)) {
        almanac_set_error(almanac, "failed to compute visual magnitude");
        return false;
    }
    return true;
}

almanac_t *almanac_open(void)
{
    almanac_t *almanac;
    char *path_text = NULL;
    char *key_text = NULL;
    char *configured = NULL;
    string_t *path = NULL;
    string_t *key = NULL;

    almanac = calloc(1u, sizeof(*almanac));
    if (!almanac)
        return NULL;
    almanac->error = string_new();
    if (!almanac->error) {
        free(almanac);
        return NULL;
    }

    configured = getenv("MARS_ALMANAC_DB_PATH") ? dup_c_string(getenv("MARS_ALMANAC_DB_PATH")) : NULL;
    if (!configured)
        configured = almanac_config_lookup("MARS_ALMANAC_DB_PATH");
    path_text = configured ? configured : almanac_default_db_path();

    configured = getenv("MARS_ALMANAC_DB_KEY") ? dup_c_string(getenv("MARS_ALMANAC_DB_KEY")) : NULL;
    if (!configured)
        configured = almanac_config_lookup("MARS_ALMANAC_DB_KEY");
    key_text = configured;

    path = string_new_from_cstr(path_text);
    key = string_new_from_cstr(key_text);
    if (!path || !key) {
        almanac_set_error(almanac,
                          "almanac configuration is incomplete; provide MARS_ALMANAC_DB_KEY and an almanac database");
        string_free(path);
        string_free(key);
        free(path_text);
        free(key_text);
        return almanac;
    }

    almanac->db = sqlite_open_encrypted(path, key);
    if (!almanac->db)
        almanac_set_error(almanac, "failed to open encrypted almanac database");

    string_free(path);
    string_free(key);
    free(path_text);
    free(key_text);
    return almanac;
}

void almanac_close(almanac_t *almanac)
{
    if (!almanac)
        return;
    free(almanac->nutation_terms);
    sqlite_close(almanac->db);
    string_free(almanac->error);
    free(almanac);
}

const char *almanac_last_error(const almanac_t *almanac)
{
    return (almanac && almanac->error) ? string_c_str(almanac->error) : NULL;
}

bool almanac_gha_aries(almanac_t *almanac,
                       const datetime_t *moment,
                       double *gha_aries_degrees)
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

bool almanac_lookup(almanac_t *almanac,
                    const char *body_code,
                    const datetime_t *moment,
                    almanac_entry_t *out)
{
    almanac_model_row_t model;

    if (!almanac || !moment || !out || !body_code) {
        almanac_set_error(almanac, "invalid almanac lookup request");
        return false;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return false;
    }
    if (!almanac_fetch_model(almanac, body_code, &model))
        return false;
    return almanac_compute_entry(almanac, &model, moment, out);
}

array_t *almanac_snapshot(almanac_t *almanac, const datetime_t *moment)
{
    static const char *sql =
        "SELECT body_code FROM almanac_body WHERE enabled = 1 ORDER BY sort_order ASC, body_code ASC";
    sqlite_stmt_t *stmt = NULL;
    array_t *entries = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !moment) {
        almanac_set_error(almanac, "invalid almanac snapshot request");
        return NULL;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return NULL;
    }

    entries = array_create(sizeof(almanac_entry_t), NULL, NULL);
    if (!entries) {
        almanac_set_error(almanac, "failed to allocate almanac snapshot array");
        return NULL;
    }

    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        array_destroy(entries);
        almanac_set_sqlite_error(almanac);
        return NULL;
    }

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        const char *code = sqlite_stmt_column_text(stmt, 0);
        almanac_entry_t entry;

        if (!code || !almanac_lookup(almanac, (const char *)code, moment, &entry) ||
            !array_add(entries, &entry)) {
            sqlite_stmt_finalize(stmt);
            array_destroy(entries);
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to build almanac snapshot");
            return NULL;
        }
    }

    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        array_destroy(entries);
        almanac_set_sqlite_error(almanac);
        return NULL;
    }

    return entries;
}

bool almanac_serialize(const almanac_t *almanac,
                       string_t **out_type,
                       string_t **out_encoding,
                       void **out_data,
                       size_t *out_len)
{
    static const char sentinel[] = "default";
    void *payload;

    if (!almanac || !almanac->db || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    *out_type = string_new_with("almanac_t");
    *out_encoding = string_new_with("mars/configured-engine-v1");
    if (!*out_type || !*out_encoding) {
        string_free(*out_type);
        string_free(*out_encoding);
        *out_type = NULL;
        *out_encoding = NULL;
        return false;
    }

    payload = malloc(sizeof(sentinel) - 1u);
    if (!payload) {
        string_free(*out_type);
        string_free(*out_encoding);
        *out_type = NULL;
        *out_encoding = NULL;
        return false;
    }

    memcpy(payload, sentinel, sizeof(sentinel) - 1u);
    *out_data = payload;
    *out_len = sizeof(sentinel) - 1u;
    return true;
}

almanac_t *almanac_deserialise(const void *data,
                               size_t len,
                               const string_t *type,
                               const string_t *encoding)
{
    static const char sentinel[] = "default";

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "almanac_t") != 0)
        return NULL;
    if (strcmp(string_c_str(encoding), "mars/configured-engine-v1") != 0)
        return NULL;
    if (len != sizeof(sentinel) - 1u)
        return NULL;
    if (memcmp(data, sentinel, sizeof(sentinel) - 1u) != 0)
        return NULL;

    return almanac_open();
}
