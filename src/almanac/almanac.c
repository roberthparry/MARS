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
    double omega_c0_deg;
    double omega_c1_deg_per_century;
    double omega_c2_deg_per_century2;
    double omega_c3_century3_divisor;
    double solar_mean_longitude_c0_deg;
    double solar_mean_longitude_c1_deg_per_century;
    double lunar_mean_longitude_c0_deg;
    double lunar_mean_longitude_c1_deg_per_century;
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
    bool has_geocentric_vector;
    bool has_direction_vector;
    double right_ascension_hours;
    double declination_degrees;
} almanac_state_t;

typedef struct almanac_orbital_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double span_days;
    double Omega_c0;
    double Omega_c1;
    double Omega_c2;
    double i_c0;
    double i_c1;
    double i_c2;
    double varpi_c0;
    double varpi_c1;
    double varpi_c2;
    double a_c0;
    double a_c1;
    double a_c2;
    double e_c0;
    double e_c1;
    double e_c2;
    double L_c0;
    double L_c1;
    double L_c2;
} almanac_orbital_segment_t;

typedef struct almanac_poly4_t {
    double c0;
    double c1;
    double c2;
    double c3;
    double c4;
} almanac_poly4_t;

typedef struct almanac_lunar_fundamentals_t {
    almanac_poly4_t Lp;
    almanac_poly4_t D;
    almanac_poly4_t M;
    almanac_poly4_t Mp;
    almanac_poly4_t F;
} almanac_lunar_fundamentals_t;

typedef struct almanac_lunar_fundamentals_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double span_days;
    almanac_poly4_t Lp;
    almanac_poly4_t D;
    almanac_poly4_t M;
    almanac_poly4_t Mp;
    almanac_poly4_t F;
} almanac_lunar_fundamentals_segment_t;

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

typedef struct almanac_lunar_correction_segment_t {
    double start_jd;
    double end_jd;
    double reference_jd;
    double span_days;
    almanac_poly8_t lon;
    almanac_poly8_t lat;
    almanac_poly8_t radius;
} almanac_lunar_correction_segment_t;

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

static double almanac_poly4_eval(const almanac_poly4_t *poly, double x)
{
    if (!poly)
        return 0.0;
    return ((((poly->c4 * x) + poly->c3) * x + poly->c2) * x + poly->c1) * x + poly->c0;
}

static double almanac_poly8_eval(const almanac_poly8_t *poly, double x)
{
    if (!poly)
        return 0.0;
    return (((((((poly->c7 * x) + poly->c6) * x + poly->c5) * x + poly->c4) * x
        + poly->c3) * x + poly->c2) * x + poly->c1) * x + poly->c0;
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
        !almanac_fetch_constant(almanac, "omega_c0_deg", &almanac->omega_c0_deg) ||
        !almanac_fetch_constant(almanac, "omega_c1_deg_per_century", &almanac->omega_c1_deg_per_century) ||
        !almanac_fetch_constant(almanac, "omega_c2_deg_per_century2", &almanac->omega_c2_deg_per_century2) ||
        !almanac_fetch_constant(almanac, "omega_c3_century3_divisor", &almanac->omega_c3_century3_divisor) ||
        !almanac_fetch_constant(almanac, "solar_mean_longitude_c0_deg", &almanac->solar_mean_longitude_c0_deg) ||
        !almanac_fetch_constant(almanac, "solar_mean_longitude_c1_deg_per_century", &almanac->solar_mean_longitude_c1_deg_per_century) ||
        !almanac_fetch_constant(almanac, "lunar_mean_longitude_c0_deg", &almanac->lunar_mean_longitude_c0_deg) ||
        !almanac_fetch_constant(almanac, "lunar_mean_longitude_c1_deg_per_century", &almanac->lunar_mean_longitude_c1_deg_per_century) ||
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
        "       f.epoch_jd, f.ra_hours, f.dec_degrees, f.pm_ra_mas_per_year, f.pm_dec_mas_per_year,"
        "       k.epoch_jd, k.N0, k.N_dot, k.i0, k.i_dot, k.w0, k.w_dot, k.a0, k.a_dot, k.e0, k.e_dot, k.M0, k.M_dot "
        "FROM almanac_body AS b "
        "LEFT JOIN almanac_fixed_equatorial_model AS f ON f.body_code = b.body_code "
        "LEFT JOIN almanac_keplerian_model AS k ON k.body_code = b.body_code "
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
    out->orbit_epoch_jd = sqlite_stmt_column_double(stmt, 16);
    out->N0 = sqlite_stmt_column_double(stmt, 17);
    out->N_dot = sqlite_stmt_column_double(stmt, 18);
    out->i0 = sqlite_stmt_column_double(stmt, 19);
    out->i_dot = sqlite_stmt_column_double(stmt, 20);
    out->w0 = sqlite_stmt_column_double(stmt, 21);
    out->w_dot = sqlite_stmt_column_double(stmt, 22);
    out->a0 = sqlite_stmt_column_double(stmt, 23);
    out->a_dot = sqlite_stmt_column_double(stmt, 24);
    out->e0 = sqlite_stmt_column_double(stmt, 25);
    out->e_dot = sqlite_stmt_column_double(stmt, 26);
    out->M0 = sqlite_stmt_column_double(stmt, 27);
    out->M_dot = sqlite_stmt_column_double(stmt, 28);

    sqlite_stmt_finalize(stmt);
    return true;
}

static bool almanac_load_lunar_fundamentals(almanac_t *almanac,
                                            almanac_lunar_fundamentals_t *out)
{
    static const char *sql =
        "SELECT term_code, c0, c1, c2, c3, c4 "
        "FROM almanac_lunar_fundamental_coeff "
        "ORDER BY sort_order ASC, term_code ASC";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    bool have_Lp = false;
    bool have_D = false;
    bool have_M = false;
    bool have_Mp = false;
    bool have_F = false;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid lunar fundamentals lookup");
        return false;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        const char *term_code = sqlite_stmt_column_text(stmt, 0);
        almanac_poly4_t poly;

        poly.c0 = sqlite_stmt_column_double(stmt, 1);
        poly.c1 = sqlite_stmt_column_double(stmt, 2);
        poly.c2 = sqlite_stmt_column_double(stmt, 3);
        poly.c3 = sqlite_stmt_column_double(stmt, 4);
        poly.c4 = sqlite_stmt_column_double(stmt, 5);

        if (!term_code)
            continue;
        if (strcmp(term_code, "Lp") == 0) {
            out->Lp = poly;
            have_Lp = true;
        } else if (strcmp(term_code, "D") == 0) {
            out->D = poly;
            have_D = true;
        } else if (strcmp(term_code, "M") == 0) {
            out->M = poly;
            have_M = true;
        } else if (strcmp(term_code, "Mp") == 0) {
            out->Mp = poly;
            have_Mp = true;
        } else if (strcmp(term_code, "F") == 0) {
            out->F = poly;
            have_F = true;
        }
    }

    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!have_Lp || !have_D || !have_M || !have_Mp || !have_F) {
        almanac_set_error(almanac, "lunar fundamentals are incomplete");
        return false;
    }
    return true;
}

static bool almanac_fetch_lunar_fundamentals_segment(almanac_t *almanac,
                                                     double jd,
                                                     almanac_lunar_fundamentals_segment_t *out)
{
    static const char *sql =
        "SELECT start_jd, end_jd, reference_jd, span_days, "
        "       Lp_c0, Lp_c1, Lp_c2, Lp_c3, Lp_c4, "
        "       D_c0, D_c1, D_c2, D_c3, D_c4, "
        "       M_c0, M_c1, M_c2, M_c3, M_c4, "
        "       Mp_c0, Mp_c1, Mp_c2, Mp_c3, Mp_c4, "
        "       F_c0, F_c1, F_c2, F_c3, F_c4 "
        "FROM almanac_lunar_fundamentals_model "
        "WHERE body_code = 'MOON' AND start_jd <= ? AND end_jd > ? "
        "ORDER BY sort_order ASC, model_id ASC "
        "LIMIT 1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid lunar fundamentals segment lookup");
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
    out->Lp.c0 = sqlite_stmt_column_double(stmt, 4);
    out->Lp.c1 = sqlite_stmt_column_double(stmt, 5);
    out->Lp.c2 = sqlite_stmt_column_double(stmt, 6);
    out->Lp.c3 = sqlite_stmt_column_double(stmt, 7);
    out->Lp.c4 = sqlite_stmt_column_double(stmt, 8);
    out->D.c0 = sqlite_stmt_column_double(stmt, 9);
    out->D.c1 = sqlite_stmt_column_double(stmt, 10);
    out->D.c2 = sqlite_stmt_column_double(stmt, 11);
    out->D.c3 = sqlite_stmt_column_double(stmt, 12);
    out->D.c4 = sqlite_stmt_column_double(stmt, 13);
    out->M.c0 = sqlite_stmt_column_double(stmt, 14);
    out->M.c1 = sqlite_stmt_column_double(stmt, 15);
    out->M.c2 = sqlite_stmt_column_double(stmt, 16);
    out->M.c3 = sqlite_stmt_column_double(stmt, 17);
    out->M.c4 = sqlite_stmt_column_double(stmt, 18);
    out->Mp.c0 = sqlite_stmt_column_double(stmt, 19);
    out->Mp.c1 = sqlite_stmt_column_double(stmt, 20);
    out->Mp.c2 = sqlite_stmt_column_double(stmt, 21);
    out->Mp.c3 = sqlite_stmt_column_double(stmt, 22);
    out->Mp.c4 = sqlite_stmt_column_double(stmt, 23);
    out->F.c0 = sqlite_stmt_column_double(stmt, 24);
    out->F.c1 = sqlite_stmt_column_double(stmt, 25);
    out->F.c2 = sqlite_stmt_column_double(stmt, 26);
    out->F.c3 = sqlite_stmt_column_double(stmt, 27);
    out->F.c4 = sqlite_stmt_column_double(stmt, 28);
    sqlite_stmt_finalize(stmt);
    return out->span_days > 0.0;
}

static bool almanac_lunar_fundamental_angles(almanac_t *almanac,
                                             double jd,
                                             double *Lp_deg,
                                             double *D_deg,
                                             double *M_deg,
                                             double *Mp_deg,
                                             double *F_deg)
{
    almanac_lunar_fundamentals_segment_t segment;
    almanac_lunar_fundamentals_t global;

    if (!almanac || !Lp_deg || !D_deg || !M_deg || !Mp_deg || !F_deg) {
        almanac_set_error(almanac, "invalid lunar angle request");
        return false;
    }

    if (almanac_fetch_lunar_fundamentals_segment(almanac, jd, &segment)) {
        double x = (jd - segment.reference_jd) / segment.span_days;

        *Lp_deg = almanac_poly4_eval(&segment.Lp, x);
        *D_deg = almanac_poly4_eval(&segment.D, x);
        *M_deg = almanac_poly4_eval(&segment.M, x);
        *Mp_deg = almanac_poly4_eval(&segment.Mp, x);
        *F_deg = almanac_poly4_eval(&segment.F, x);
        return true;
    }

    if (!almanac_load_lunar_fundamentals(almanac, &global))
        return false;

    {
        double T = (jd - 2451545.0) / 36525.0;

        *Lp_deg = almanac_poly4_eval(&global.Lp, T);
        *D_deg = almanac_poly4_eval(&global.D, T);
        *M_deg = almanac_poly4_eval(&global.M, T);
        *Mp_deg = almanac_poly4_eval(&global.Mp, T);
        *F_deg = almanac_poly4_eval(&global.F, T);
    }
    return true;
}

static bool almanac_fetch_lunar_correction_segment(almanac_t *almanac,
                                                   double jd,
                                                   almanac_lunar_correction_segment_t *out)
{
    static const char *sql =
        "SELECT start_jd, end_jd, reference_jd, span_days, "
        "       lon_c0_deg, lon_c1_deg, lon_c2_deg, lon_c3_deg, lon_c4_deg, lon_c5_deg, lon_c6_deg, lon_c7_deg, "
        "       lat_c0_deg, lat_c1_deg, lat_c2_deg, lat_c3_deg, lat_c4_deg, lat_c5_deg, lat_c6_deg, lat_c7_deg, "
        "       radius_c0_km, radius_c1_km, radius_c2_km, radius_c3_km, radius_c4_km, radius_c5_km, radius_c6_km, radius_c7_km "
        "FROM almanac_lunar_correction_model "
        "WHERE body_code = 'MOON' AND start_jd <= ? AND end_jd > ? "
        "ORDER BY sort_order ASC, model_id ASC "
        "LIMIT 1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid lunar correction segment lookup");
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
    out->lon.c0 = sqlite_stmt_column_double(stmt, 4);
    out->lon.c1 = sqlite_stmt_column_double(stmt, 5);
    out->lon.c2 = sqlite_stmt_column_double(stmt, 6);
    out->lon.c3 = sqlite_stmt_column_double(stmt, 7);
    out->lon.c4 = sqlite_stmt_column_double(stmt, 8);
    out->lon.c5 = sqlite_stmt_column_double(stmt, 9);
    out->lon.c6 = sqlite_stmt_column_double(stmt, 10);
    out->lon.c7 = sqlite_stmt_column_double(stmt, 11);
    out->lat.c0 = sqlite_stmt_column_double(stmt, 12);
    out->lat.c1 = sqlite_stmt_column_double(stmt, 13);
    out->lat.c2 = sqlite_stmt_column_double(stmt, 14);
    out->lat.c3 = sqlite_stmt_column_double(stmt, 15);
    out->lat.c4 = sqlite_stmt_column_double(stmt, 16);
    out->lat.c5 = sqlite_stmt_column_double(stmt, 17);
    out->lat.c6 = sqlite_stmt_column_double(stmt, 18);
    out->lat.c7 = sqlite_stmt_column_double(stmt, 19);
    out->radius.c0 = sqlite_stmt_column_double(stmt, 20);
    out->radius.c1 = sqlite_stmt_column_double(stmt, 21);
    out->radius.c2 = sqlite_stmt_column_double(stmt, 22);
    out->radius.c3 = sqlite_stmt_column_double(stmt, 23);
    out->radius.c4 = sqlite_stmt_column_double(stmt, 24);
    out->radius.c5 = sqlite_stmt_column_double(stmt, 25);
    out->radius.c6 = sqlite_stmt_column_double(stmt, 26);
    out->radius.c7 = sqlite_stmt_column_double(stmt, 27);
    sqlite_stmt_finalize(stmt);
    return out->span_days > 0.0;
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

static double almanac_quadratic_eval(double c0, double c1, double c2, double x)
{
    return (c2 * x + c1) * x + c0;
}
static bool almanac_fetch_orbital_segment(almanac_t *almanac,
                                          const char *body_code,
                                          double jd,
                                          almanac_orbital_segment_t *out)
{
    static const char *sql =
        "SELECT start_jd, end_jd, reference_jd, span_days, "
        "       Omega_c0, Omega_c1, Omega_c2, "
        "       i_c0, i_c1, i_c2, "
        "       varpi_c0, varpi_c1, varpi_c2, "
        "       a_c0, a_c1, a_c2, "
        "       e_c0, e_c1, e_c2, "
        "       L_c0, L_c1, L_c2 "
        "FROM almanac_orbital_elements_model "
        "WHERE body_code = ?1 AND start_jd <= ?2 AND end_jd >= ?2 "
        "ORDER BY start_jd ASC "
        "LIMIT 1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !almanac->db || !body_code || !out) {
        almanac_set_error(almanac, "invalid orbital-elements segment lookup");
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

        almanac_set_error(almanac, stmt_error ? string_c_str(stmt_error) : "failed to bind orbital-elements lookup");
        sqlite_stmt_finalize(stmt);
        return false;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc != SQLITE_STEP_ROW) {
        sqlite_stmt_finalize(stmt);
        if (rc == SQLITE_STEP_DONE)
            almanac_set_error(almanac, "no orbital-elements segment matched the requested date");
        else
            almanac_set_sqlite_error(almanac);
        return false;
    }

    out->start_jd = sqlite_stmt_column_double(stmt, 0);
    out->end_jd = sqlite_stmt_column_double(stmt, 1);
    out->reference_jd = sqlite_stmt_column_double(stmt, 2);
    out->span_days = sqlite_stmt_column_double(stmt, 3);
    out->Omega_c0 = sqlite_stmt_column_double(stmt, 4);
    out->Omega_c1 = sqlite_stmt_column_double(stmt, 5);
    out->Omega_c2 = sqlite_stmt_column_double(stmt, 6);
    out->i_c0 = sqlite_stmt_column_double(stmt, 7);
    out->i_c1 = sqlite_stmt_column_double(stmt, 8);
    out->i_c2 = sqlite_stmt_column_double(stmt, 9);
    out->varpi_c0 = sqlite_stmt_column_double(stmt, 10);
    out->varpi_c1 = sqlite_stmt_column_double(stmt, 11);
    out->varpi_c2 = sqlite_stmt_column_double(stmt, 12);
    out->a_c0 = sqlite_stmt_column_double(stmt, 13);
    out->a_c1 = sqlite_stmt_column_double(stmt, 14);
    out->a_c2 = sqlite_stmt_column_double(stmt, 15);
    out->e_c0 = sqlite_stmt_column_double(stmt, 16);
    out->e_c1 = sqlite_stmt_column_double(stmt, 17);
    out->e_c2 = sqlite_stmt_column_double(stmt, 18);
    out->L_c0 = sqlite_stmt_column_double(stmt, 19);
    out->L_c1 = sqlite_stmt_column_double(stmt, 20);
    out->L_c2 = sqlite_stmt_column_double(stmt, 21);
    sqlite_stmt_finalize(stmt);

    if (out->span_days <= 0.0) {
        almanac_set_error(almanac, "orbital-elements segment has invalid span");
        return false;
    }
    return true;
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

static bool solve_kepler(double mean_anomaly_radians,
                         double eccentricity,
                         double *eccentric_anomaly_radians)
{
    double E;
    int i;

    if (!eccentric_anomaly_radians)
        return false;

    E = mean_anomaly_radians;
    for (i = 0; i < 16; i++) {
        double delta = (E - eccentricity * sin(E) - mean_anomaly_radians) /
                       (1.0 - eccentricity * cos(E));
        E -= delta;
        if (fabs(delta) < 1e-12)
            break;
    }
    *eccentric_anomaly_radians = E;
    return true;
}

static bool heliocentric_from_model(const almanac_model_row_t *model,
                                    double jd,
                                    cartesian3_t *out)
{
    double days;
    double N;
    double i;
    double w;
    double a;
    double e;
    double M;
    double E;
    double xv;
    double yv;
    double v;
    double r;
    double cos_N;
    double sin_N;
    double cos_i;
    double sin_i;
    double cos_vw;
    double sin_vw;

    if (!model || !out)
        return false;

    days = jd - model->orbit_epoch_jd;
    N = normalize_degrees(model->N0 + model->N_dot * days);
    i = model->i0 + model->i_dot * days;
    w = normalize_degrees(model->w0 + model->w_dot * days);
    a = model->a0 + model->a_dot * days;
    e = model->e0 + model->e_dot * days;
    M = normalize_degrees(model->M0 + model->M_dot * days);

    if (!solve_kepler(degrees_to_radians(M), e, &E))
        return false;

    xv = a * (cos(E) - e);
    yv = a * (sqrt(1.0 - e * e) * sin(E));
    v = atan2(yv, xv);
    r = sqrt(xv * xv + yv * yv);

    cos_N = cos(degrees_to_radians(N));
    sin_N = sin(degrees_to_radians(N));
    cos_i = cos(degrees_to_radians(i));
    sin_i = sin(degrees_to_radians(i));
    cos_vw = cos(v + degrees_to_radians(w));
    sin_vw = sin(v + degrees_to_radians(w));

    out->x = r * (cos_N * cos_vw - sin_N * sin_vw * cos_i);
    out->y = r * (sin_N * cos_vw + cos_N * sin_vw * cos_i);
    out->z = r * (sin_vw * sin_i);
    return true;
}

static bool heliocentric_velocity_from_model(const almanac_model_row_t *model,
                                             double jd,
                                             cartesian3_t *out_velocity_au_per_day)
{
    const double h = 0.0005;
    cartesian3_t before;
    cartesian3_t after;

    if (!model || !out_velocity_au_per_day)
        return false;
    if (!heliocentric_from_model(model, jd - h, &before) ||
        !heliocentric_from_model(model, jd + h, &after))
        return false;

    out_velocity_au_per_day->x = (after.x - before.x) / (2.0 * h);
    out_velocity_au_per_day->y = (after.y - before.y) / (2.0 * h);
    out_velocity_au_per_day->z = (after.z - before.z) / (2.0 * h);
    return true;
}

static bool heliocentric_from_orbital_segment(const almanac_orbital_segment_t *segment,
                                              double jd,
                                              cartesian3_t *out)
{
    double x;
    double Omega;
    double i;
    double varpi;
    double a;
    double e;
    double L;
    double M;
    double E;
    double xv;
    double yv;
    double v;
    double r;
    double cos_Omega;
    double sin_Omega;
    double cos_i;
    double sin_i;
    double cos_vw;
    double sin_vw;
    double arg_perihelion;

    if (!segment || !out || segment->span_days <= 0.0)
        return false;

    x = (jd - segment->reference_jd) / segment->span_days;
    Omega = normalize_degrees(almanac_quadratic_eval(segment->Omega_c0, segment->Omega_c1, segment->Omega_c2, x));
    i = almanac_quadratic_eval(segment->i_c0, segment->i_c1, segment->i_c2, x);
    varpi = normalize_degrees(almanac_quadratic_eval(segment->varpi_c0, segment->varpi_c1, segment->varpi_c2, x));
    a = almanac_quadratic_eval(segment->a_c0, segment->a_c1, segment->a_c2, x);
    e = almanac_quadratic_eval(segment->e_c0, segment->e_c1, segment->e_c2, x);
    L = normalize_degrees(almanac_quadratic_eval(segment->L_c0, segment->L_c1, segment->L_c2, x));
    M = normalize_degrees(L - varpi);
    arg_perihelion = normalize_degrees(varpi - Omega);

    if (!solve_kepler(degrees_to_radians(M), e, &E))
        return false;

    xv = a * (cos(E) - e);
    yv = a * (sqrt(1.0 - e * e) * sin(E));
    v = atan2(yv, xv);
    r = sqrt(xv * xv + yv * yv);

    cos_Omega = cos(degrees_to_radians(Omega));
    sin_Omega = sin(degrees_to_radians(Omega));
    cos_i = cos(degrees_to_radians(i));
    sin_i = sin(degrees_to_radians(i));
    cos_vw = cos(v + degrees_to_radians(arg_perihelion));
    sin_vw = sin(v + degrees_to_radians(arg_perihelion));

    out->x = r * (cos_Omega * cos_vw - sin_Omega * sin_vw * cos_i);
    out->y = r * (sin_Omega * cos_vw + cos_Omega * sin_vw * cos_i);
    out->z = r * (sin_vw * sin_i);
    return true;
}

static bool heliocentric_velocity_from_orbital_segment(const almanac_orbital_segment_t *segment,
                                                       double jd,
                                                       cartesian3_t *out_velocity_au_per_day)
{
    const double h = 0.0005;
    cartesian3_t before;
    cartesian3_t after;

    if (!segment || !out_velocity_au_per_day)
        return false;
    if (!heliocentric_from_orbital_segment(segment, jd - h, &before) ||
        !heliocentric_from_orbital_segment(segment, jd + h, &after))
        return false;

    out_velocity_au_per_day->x = (after.x - before.x) / (2.0 * h);
    out_velocity_au_per_day->y = (after.y - before.y) / (2.0 * h);
    out_velocity_au_per_day->z = (after.z - before.z) / (2.0 * h);
    return true;
}

static cartesian3_t equatorial_from_ecliptic_vector(const cartesian3_t *ecliptic,
                                                    almanac_t *almanac,
                                                    double jd)
{
    return cartesian_rotate_x(ecliptic, almanac_mean_obliquity_radians(almanac, jd));
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
    almanac_model_row_t earth;
    almanac_orbital_segment_t earth_segment;
    cartesian3_t earth_velocity_ecliptic;
    cartesian3_t earth_velocity_equatorial;
    cartesian3_t earth_heliocentric_ecliptic;
    cartesian3_t earth_heliocentric_equatorial;
    cartesian3_t sun_to_body_direction;
    cartesian3_t direction;

    if (!almanac || !model || !state || !state->has_direction_vector)
        return false;
    if (!almanac_load_correction_model(almanac))
        return false;
    if (!almanac_fetch_model(almanac, "EARTH", &earth))
        return false;
    if (strcmp(earth.model_kind, "orbital_elements") == 0) {
        if (!almanac_fetch_orbital_segment(almanac, "EARTH", jd, &earth_segment) ||
            !heliocentric_from_orbital_segment(&earth_segment, jd, &earth_heliocentric_ecliptic) ||
            !heliocentric_velocity_from_orbital_segment(&earth_segment, jd, &earth_velocity_ecliptic))
            return false;
    } else {
        if (!heliocentric_from_model(&earth, jd, &earth_heliocentric_ecliptic) ||
            !heliocentric_velocity_from_model(&earth, jd, &earth_velocity_ecliptic))
            return false;
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

static double almanac_lunar_eccentricity_factor(double e, int exponent)
{
    double factor = 1.0;
    int i;

    for (i = 0; i < exponent; ++i)
        factor *= e;
    return factor;
}

static bool almanac_lunar_geocentric_ecliptic(almanac_t *almanac,
                                              double jd,
                                              double *longitude_radians,
                                              double *latitude_radians,
                                              double *distance_km)
{
    static const char *lonrad_sql =
        "SELECT multiplier_D, multiplier_M, multiplier_Mp, multiplier_F, "
        "       longitude_coeff_microdeg, radius_coeff_millikm "
        "FROM almanac_lunar_longitude_radius_term "
        "ORDER BY sort_order ASC, term_id ASC";
    static const char *lat_sql =
        "SELECT multiplier_D, multiplier_M, multiplier_Mp, multiplier_F, latitude_coeff_microdeg "
        "FROM almanac_lunar_latitude_term "
        "ORDER BY sort_order ASC, term_id ASC";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    double T;
    double e;
    double Lp_deg;
    double D_deg;
    double M_deg;
    double Mp_deg;
    double F_deg;
    double Lp;
    double D;
    double M;
    double Mp;
    double F;
    double A1;
    double A2;
    double A3;
    double sl = 0.0;
    double sr = 0.0;
    double sb = 0.0;

    if (!almanac || !longitude_radians || !latitude_radians || !distance_km) {
        almanac_set_error(almanac, "invalid lunar series request");
        return false;
    }
    if (!almanac_lunar_fundamental_angles(almanac, jd, &Lp_deg, &D_deg, &M_deg, &Mp_deg, &F_deg))
        return false;

    T = (jd - 2451545.0) / 36525.0;
    e = 1.0 - 0.002516 * T - 0.0000074 * T * T;
    Lp = degrees_to_radians(normalize_degrees(Lp_deg));
    D = degrees_to_radians(normalize_degrees(D_deg));
    M = degrees_to_radians(normalize_degrees(M_deg));
    Mp = degrees_to_radians(normalize_degrees(Mp_deg));
    F = degrees_to_radians(normalize_degrees(F_deg));
    A1 = degrees_to_radians(normalize_degrees(119.75 + 131.849 * T));
    A2 = degrees_to_radians(normalize_degrees(53.09 + 479264.29 * T));
    A3 = degrees_to_radians(normalize_degrees(313.45 + 481266.484 * T));

    stmt = sqlite_stmt_prepare(almanac->db, lonrad_sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        int multiplier_D = sqlite_stmt_column_int(stmt, 0);
        int multiplier_M = sqlite_stmt_column_int(stmt, 1);
        int multiplier_Mp = sqlite_stmt_column_int(stmt, 2);
        int multiplier_F = sqlite_stmt_column_int(stmt, 3);
        double lon_coeff = sqlite_stmt_column_double(stmt, 4);
        double rad_coeff = sqlite_stmt_column_double(stmt, 5);
        double arg = multiplier_D * D + multiplier_M * M + multiplier_Mp * Mp + multiplier_F * F;
        double scale = almanac_lunar_eccentricity_factor(e, abs(multiplier_M));

        if (lon_coeff != 0.0)
            sl += scale * lon_coeff * sin(arg);
        if (rad_coeff != 0.0)
            sr += scale * rad_coeff * cos(arg);
    }
    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        almanac_set_sqlite_error(almanac);
        return false;
    }

    stmt = sqlite_stmt_prepare(almanac->db, lat_sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        int multiplier_D = sqlite_stmt_column_int(stmt, 0);
        int multiplier_M = sqlite_stmt_column_int(stmt, 1);
        int multiplier_Mp = sqlite_stmt_column_int(stmt, 2);
        int multiplier_F = sqlite_stmt_column_int(stmt, 3);
        double lat_coeff = sqlite_stmt_column_double(stmt, 4);
        double arg = multiplier_D * D + multiplier_M * M + multiplier_Mp * Mp + multiplier_F * F;
        double scale = almanac_lunar_eccentricity_factor(e, abs(multiplier_M));

        if (lat_coeff != 0.0)
            sb += scale * lat_coeff * sin(arg);
    }
    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        almanac_set_sqlite_error(almanac);
        return false;
    }

    sl += 3958.0 * sin(A1) + 1962.0 * sin(Lp - F) + 318.0 * sin(A2);
    sb += -2235.0 * sin(Lp) + 382.0 * sin(A3) + 175.0 * sin(A1 - F)
        + 175.0 * sin(A1 + F) + 127.0 * sin(Lp - Mp) - 115.0 * sin(Lp + Mp);

    {
        almanac_lunar_correction_segment_t correction;

        if (almanac_fetch_lunar_correction_segment(almanac, jd, &correction)) {
            double x = (jd - correction.reference_jd) / correction.span_days;

            Lp_deg += almanac_poly8_eval(&correction.lon, x);
            sb += almanac_poly8_eval(&correction.lat, x) * 1.0e6;
            sr += almanac_poly8_eval(&correction.radius, x) * 1000.0;
        }
    }

    *longitude_radians = degrees_to_radians(normalize_degrees(Lp_deg + sl * 1.0e-6));
    *latitude_radians = degrees_to_radians(sb * 1.0e-6);
    *distance_km = 385000.56 + sr / 1000.0;
    return true;
}

static bool almanac_equatorial_from_moon(almanac_t *almanac,
                                         const almanac_model_row_t *model,
                                         double jd,
                                         almanac_state_t *state)
{
    double lon;
    double lat;
    double distance_km;
    double cos_lat;
    double sin_lat;
    double cos_lon;
    double sin_lon;
    double xh;
    double yh;
    double zh;
    double obliquity;

    (void)model;

    if (!almanac || !model || !state)
        return false;
    if (!almanac_lunar_geocentric_ecliptic(almanac, jd, &lon, &lat, &distance_km))
        return false;
    cos_lat = cos(lat);
    sin_lat = sin(lat);
    cos_lon = cos(lon);
    sin_lon = sin(lon);
    xh = distance_km * cos_lat * cos_lon;
    yh = distance_km * cos_lat * sin_lon;
    zh = distance_km * sin_lat;

    obliquity = almanac_mean_obliquity_radians(almanac, jd);
    if (obliquity == DBL_MAX)
        return false;
    state->geocentric_equatorial_au.x = xh / 149597870.7;
    state->geocentric_equatorial_au.y = (yh * cos(obliquity) - zh * sin(obliquity)) / 149597870.7;
    state->geocentric_equatorial_au.z = (yh * sin(obliquity) + zh * cos(obliquity)) / 149597870.7;
    state->has_geocentric_vector = true;
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

static bool almanac_equatorial_from_keplerian(almanac_t *almanac,
                                              const almanac_model_row_t *model,
                                              double jd,
                                              almanac_state_t *state)
{
    almanac_model_row_t earth;
    cartesian3_t body_helio;
    cartesian3_t earth_helio;
    cartesian3_t geocentric;
    double light_time_days = 0.0;
    int iteration;
    double days;
    double obliquity;
    if (!almanac || !model || !state)
        return false;

    if (!almanac_fetch_model(almanac, "EARTH", &earth))
        return false;
    if (!heliocentric_from_model(&earth, jd, &earth_helio))
        return false;

    if (strcmp(model->body_code, "SUN") == 0) {
        geocentric.x = -earth_helio.x;
        geocentric.y = -earth_helio.y;
        geocentric.z = -earth_helio.z;
    } else {
        for (iteration = 0; iteration < 3; ++iteration) {
            if (!heliocentric_from_model(model, jd - light_time_days, &body_helio))
                return false;
            geocentric.x = body_helio.x - earth_helio.x;
            geocentric.y = body_helio.y - earth_helio.y;
            geocentric.z = body_helio.z - earth_helio.z;
            if (!almanac_load_correction_model(almanac))
                return false;
            light_time_days = almanac_relativistic_light_time_days(&earth_helio, &body_helio, &geocentric);
        }
    }

    days = jd - 2451545.0;
    (void)days;
    obliquity = almanac_mean_obliquity_radians(almanac, jd);
    if (obliquity == DBL_MAX)
        return false;

    state->geocentric_equatorial_au.x = geocentric.x;
    state->geocentric_equatorial_au.y = geocentric.y * cos(obliquity) - geocentric.z * sin(obliquity);
    state->geocentric_equatorial_au.z = geocentric.y * sin(obliquity) + geocentric.z * cos(obliquity);
    state->has_geocentric_vector = true;
    state->has_direction_vector = true;
    return true;
}

static bool almanac_equatorial_from_orbital_elements(almanac_t *almanac,
                                                     const almanac_model_row_t *model,
                                                     double jd,
                                                     almanac_state_t *state)
{
    almanac_orbital_segment_t body_segment;
    almanac_orbital_segment_t earth_segment;
    cartesian3_t body_helio;
    cartesian3_t earth_helio;
    cartesian3_t geocentric;
    double light_time_days = 0.0;
    int iteration;
    double obliquity;

    if (!almanac || !model || !state)
        return false;
    if (!almanac_fetch_orbital_segment(almanac, "EARTH", jd, &earth_segment) ||
        !heliocentric_from_orbital_segment(&earth_segment, jd, &earth_helio))
        return false;

    if (strcmp(model->body_code, "SUN") == 0) {
        geocentric.x = -earth_helio.x;
        geocentric.y = -earth_helio.y;
        geocentric.z = -earth_helio.z;
    } else {
        for (iteration = 0; iteration < 3; ++iteration) {
            if (!almanac_fetch_orbital_segment(almanac, model->body_code, jd - light_time_days, &body_segment) ||
                !heliocentric_from_orbital_segment(&body_segment, jd - light_time_days, &body_helio))
                return false;
            geocentric.x = body_helio.x - earth_helio.x;
            geocentric.y = body_helio.y - earth_helio.y;
            geocentric.z = body_helio.z - earth_helio.z;
            if (!almanac_load_correction_model(almanac))
                return false;
            light_time_days = almanac_relativistic_light_time_days(&earth_helio, &body_helio, &geocentric);
        }
    }

    obliquity = almanac_mean_obliquity_radians(almanac, jd);
    if (obliquity == DBL_MAX)
        return false;

    state->geocentric_equatorial_au.x = geocentric.x;
    state->geocentric_equatorial_au.y = geocentric.y * cos(obliquity) - geocentric.z * sin(obliquity);
    state->geocentric_equatorial_au.z = geocentric.y * sin(obliquity) + geocentric.z * cos(obliquity);
    state->has_geocentric_vector = true;
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
    } else if (strcmp(model->model_kind, "orbital_elements") == 0) {
        if (!almanac_equatorial_from_orbital_elements(almanac, model, ephemeris_jd, &state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute orbital-elements position");
            return false;
        }
    } else if (strcmp(model->model_kind, "lunar_keplerian") == 0) {
        if (!almanac_equatorial_from_moon(almanac, model, ephemeris_jd, &state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute lunar position");
            return false;
        }
    } else if (strcmp(model->model_kind, "keplerian") == 0) {
        if (!almanac_equatorial_from_keplerian(almanac, model, ephemeris_jd, &state)) {
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to compute planetary position");
            return false;
        }
    } else {
        almanac_set_error(almanac, "unsupported almanac model kind");
        return false;
    }

    if (!almanac_apply_apparent_direction_corrections(almanac, model, ephemeris_jd, &state)) {
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
        if (strcmp(sun_model.model_kind, "keplerian") == 0) {
            if (!almanac_equatorial_from_keplerian(almanac, &sun_model, ephemeris_jd, &sun_state))
                return false;
        } else if (strcmp(sun_model.model_kind, "orbital_elements") == 0) {
            if (!almanac_equatorial_from_orbital_elements(almanac, &sun_model, ephemeris_jd, &sun_state))
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
