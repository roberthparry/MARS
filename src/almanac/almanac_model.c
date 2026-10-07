/**
 * @file almanac_model.c
 * @brief Database-backed astronomical models and segment caches.
 *
 * Loads catalogue model records, nutation terms and position or frame-rotation segments. This unit owns the
 * translation from stored model data to the structures used by ephemeris evaluation.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Database model loading and cached ephemeris segments. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "almanac_engine_internal.h"

static const double ALMANAC_NUTATION_START_JD = 2287185.5;
static const double ALMANAC_NUTATION_END_JD = 2688952.5;
static const double ALMANAC_NUTATION_SEGMENT_DAYS = 365.25 * 5.0;
static const double ALMANAC_FIXED_EQUATORIAL_EPOCH_JD = 2451545.0;

static bool almanac_body_kind_from_text(const char *text, almanac_body_kind_t *out);
static bool almanac_model_kind_from_text(const char *text, almanac_model_kind_t *out);
static bool almanac_brightness_model_from_text(const char *text, almanac_brightness_model_t *out);

static bool almanac_unpack_fixed_equatorial_blob(const unsigned char *blob, size_t blob_size, almanac_model_row_t *out)
{
    const size_t expected_size = sizeof(double) * 4u;

    if (!blob || !out || blob_size != expected_size)
        return false;

    memcpy(&out->fixed_ra_hours, blob + sizeof(double) * 0u, sizeof(double));
    memcpy(&out->fixed_dec_degrees, blob + sizeof(double) * 1u, sizeof(double));
    memcpy(&out->fixed_pm_ra_mas_per_year, blob + sizeof(double) * 2u, sizeof(double));
    memcpy(&out->fixed_pm_dec_mas_per_year, blob + sizeof(double) * 3u, sizeof(double));
    out->fixed_epoch_jd = ALMANAC_FIXED_EQUATORIAL_EPOCH_JD;
    return true;
}

static bool almanac_unpack_magnitude_coeff_blob(const unsigned char *blob, size_t blob_size, almanac_model_row_t *out)
{
    double *coeffs[5];
    size_t i;

    if (!blob || !out || blob_size != 5u * sizeof(double))
        return false;

    coeffs[0] = &out->magnitude_constant;
    coeffs[1] = &out->magnitude_linear;
    coeffs[2] = &out->magnitude_quadratic;
    coeffs[3] = &out->magnitude_cubic;
    coeffs[4] = &out->magnitude_quartic;
    for (i = 0u; i < 5u; ++i)
        memcpy(coeffs[i], blob + i * sizeof(double), sizeof(double));
    return true;
}

static bool almanac_load_nutation_terms(almanac_t *almanac)
{
    static const char *sql = "select l_multiplier.multiplier_L, "
                             "       lprime_multiplier.multiplier_Lprime, "
                             "       omega_multiplier.multiplier_omega, "
                             "       sin_coeff.sin_coeff_arcsec, "
                             "       cos_coeff.cos_coeff_arcsec "
                             "from almanac_nutation_term as term "
                             "join almanac_nutation_term_l_multiplier as l_multiplier "
                             "  on l_multiplier.term_id = term.term_id "
                             "join almanac_nutation_term_lprime_multiplier as lprime_multiplier "
                             "  on lprime_multiplier.term_id = term.term_id "
                             "join almanac_nutation_term_omega_multiplier as omega_multiplier "
                             "  on omega_multiplier.term_id = term.term_id "
                             "join almanac_nutation_term_sin_coeff as sin_coeff "
                             "  on sin_coeff.term_id = term.term_id "
                             "join almanac_nutation_term_cos_coeff as cos_coeff "
                             "  on cos_coeff.term_id = term.term_id "
                             "join almanac_nutation_term_sort_order as sort_order "
                             "  on sort_order.term_id = term.term_id "
                             "order by sort_order.sort_order asc, term.term_id asc";
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
        almanac_nutation_term_t *grown = realloc(terms, (count + 1u) * sizeof(*terms));

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

bool almanac_fetch_nutation_segment(almanac_t *almanac, double jd, almanac_nutation_segment_t *out)
{
    static const char *sql = "select model.model_id, coeff.coefficient_blob "
                             "from almanac_nutation_model as model "
                             "join almanac_nutation_model_coeff as coeff "
                             "  on coeff.model_id = model.model_id "
                             "join almanac_nutation_model_sort_order as sort_order "
                             "  on sort_order.model_id = model.model_id "
                             "where model.model_id = ? "
                             "order by sort_order.sort_order asc, model.model_id asc "
                             "limit 1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *blob;
    size_t blob_size;
    size_t expected_size;
    int model_id;
    int coeff_index;
    double start_jd;
    double end_jd;

    if (!almanac || !almanac->db || !out) {
        almanac_set_error(almanac, "invalid nutation segment lookup");
        return false;
    }
    if (jd < ALMANAC_NUTATION_START_JD || jd > ALMANAC_NUTATION_END_JD) {
        almanac_set_error(almanac, "requested date is outside the nutation model range");
        return false;
    }
    model_id = (int)((jd - ALMANAC_NUTATION_START_JD) / ALMANAC_NUTATION_SEGMENT_DAYS) + 1;

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    sqlite_stmt_bind_int(stmt, 1, model_id);
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

    model_id = sqlite_stmt_column_int(stmt, 0);
    blob = sqlite_stmt_column_blob(stmt, 1);
    blob_size = sqlite_stmt_column_bytes(stmt, 1);
    expected_size = (size_t)(ALMANAC_NUTATION_COEFF_COUNT * 2) * sizeof(double);
    if (model_id <= 0 || !blob || blob_size != expected_size) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "nutation segment is malformed");
        return false;
    }
    start_jd = ALMANAC_NUTATION_START_JD + (double)(model_id - 1) * ALMANAC_NUTATION_SEGMENT_DAYS;
    end_jd = start_jd + ALMANAC_NUTATION_SEGMENT_DAYS;
    if (end_jd > ALMANAC_NUTATION_END_JD)
        end_jd = ALMANAC_NUTATION_END_JD;
    out->start_jd = start_jd;
    out->end_jd = end_jd;
    out->reference_jd = 0.5 * (start_jd + end_jd);
    out->span_days = 0.5 * (end_jd - start_jd);
    if (!(out->span_days > 0.0)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "nutation segment has invalid time span");
        return false;
    }
    for (coeff_index = 0; coeff_index < ALMANAC_NUTATION_COEFF_COUNT; ++coeff_index) {
        memcpy((&out->dpsi.c0) + coeff_index, blob + sizeof(double) * (size_t)coeff_index, sizeof(double));
        memcpy((&out->deps.c0) + coeff_index,
               blob + sizeof(double) * (size_t)(ALMANAC_NUTATION_COEFF_COUNT + coeff_index), sizeof(double));
    }
    sqlite_stmt_finalize(stmt);
    return out->span_days > 0.0;
}

bool almanac_load_correction_model(almanac_t *almanac)
{
    if (!almanac)
        return false;
    if (almanac->correction_model_loaded)
        return true;

    if (!almanac_load_nutation_terms(almanac)) {
        return false;
    }

    almanac->correction_model_loaded = true;
    return true;
}

static bool almanac_load_model_row(almanac_t *almanac, almanac_body_id_t body_id, almanac_model_row_t *out)
{
    static const char *sql = "select b.body_id, "
                             "       kind.body_kind, model.model_kind, brightness.brightness_model, sort.sort_order,"
                             "       magnitude.magnitude_coeff_blob, "
                             "       f.coefficient_blob "
                             "from almanac_body as b "
                             "join almanac_body_kind as kind on kind.body_id = b.body_id "
                             "join almanac_body_model_kind as model on model.body_id = b.body_id "
                             "join almanac_body_brightness_model as brightness on brightness.body_id = b.body_id "
                             "join almanac_body_sort_order as sort on sort.body_id = b.body_id "
                             "join almanac_body_magnitude_coeff as magnitude on magnitude.body_id = b.body_id "
                             "left join almanac_fixed_equatorial_model as f on f.body_id = b.body_id "
                             "where b.body_id = ?1";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;
    const unsigned char *magnitude_blob;
    size_t magnitude_blob_size;
    const unsigned char *fixed_blob;
    size_t fixed_blob_size;
    const char *body_kind_text;
    const char *model_kind_text;
    const char *brightness_model_text;

    if (!almanac || !almanac->db || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !out) {
        almanac_set_error(almanac, "invalid almanac lookup");
        return false;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_int(stmt, 1, (int)body_id)) {
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

    out->body_id = (almanac_body_id_t)sqlite_stmt_column_int(stmt, 0);
    body_kind_text = sqlite_stmt_column_text(stmt, 1);
    model_kind_text = sqlite_stmt_column_text(stmt, 2);
    brightness_model_text = sqlite_stmt_column_text(stmt, 3);
    if (!almanac_body_kind_from_text(body_kind_text, &out->body_kind) ||
        !almanac_model_kind_from_text(model_kind_text, &out->model_kind) ||
        !almanac_brightness_model_from_text(brightness_model_text, &out->brightness_model)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "almanac body classifiers are malformed");
        return false;
    }
    out->sort_order = sqlite_stmt_column_int(stmt, 4);
    magnitude_blob = sqlite_stmt_column_blob(stmt, 5);
    magnitude_blob_size = sqlite_stmt_column_bytes(stmt, 5);
    if (!almanac_unpack_magnitude_coeff_blob(magnitude_blob, magnitude_blob_size, out)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "almanac body magnitude coefficients are malformed");
        return false;
    }
    fixed_blob = sqlite_stmt_column_blob(stmt, 6);
    fixed_blob_size = sqlite_stmt_column_bytes(stmt, 6);
    if (fixed_blob_size > 0u && !almanac_unpack_fixed_equatorial_blob(fixed_blob, fixed_blob_size, out)) {
        sqlite_stmt_finalize(stmt);
        almanac_set_error(almanac, "almanac fixed equatorial coefficients are malformed");
        return false;
    }

    sqlite_stmt_finalize(stmt);
    return true;
}

bool almanac_fetch_model(almanac_t *almanac, almanac_body_id_t body_id, almanac_model_row_t *out)
{
    if (!almanac || !almanac->db || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !out) {
        almanac_set_error(almanac, "invalid almanac lookup");
        return false;
    }
    if (!almanac->model_row_cached[body_id]) {
        if (!almanac_load_model_row(almanac, body_id, &almanac->model_row_cache[body_id]))
            return false;
        almanac->model_row_cached[body_id] = true;
    }
    *out = almanac->model_row_cache[body_id];
    return true;
}

static bool almanac_body_kind_from_text(const char *text, almanac_body_kind_t *out)
{
    static const struct {
        const char *text;
        almanac_body_kind_t value;
    } body_kinds[] = {{"star", ALMANAC_BODY_STAR},
                      {"planet", ALMANAC_BODY_PLANET},
                      {"sun", ALMANAC_BODY_SUN},
                      {"moon", ALMANAC_BODY_MOON}};
    size_t i;

    if (!text || !out)
        return false;
    for (i = 0u; i < sizeof(body_kinds) / sizeof(body_kinds[0]); ++i) {
        if (strcmp(text, body_kinds[i].text) == 0) {
            *out = body_kinds[i].value;
            return true;
        }
    }
    return false;
}

static bool almanac_model_kind_from_text(const char *text, almanac_model_kind_t *out)
{
    static const struct {
        const char *text;
        almanac_model_kind_t value;
    } model_kinds[] = {{"fixed_equatorial", ALMANAC_MODEL_KIND_FIXED_EQUATORIAL},
                       {"chebyshev_position", ALMANAC_MODEL_KIND_CHEBYSHEV_POSITION},
                       {"lunar_chebyshev", ALMANAC_MODEL_KIND_LUNAR_CHEBYSHEV}};
    size_t i;

    if (!text || !out)
        return false;
    for (i = 0u; i < sizeof(model_kinds) / sizeof(model_kinds[0]); ++i) {
        if (strcmp(text, model_kinds[i].text) == 0) {
            *out = model_kinds[i].value;
            return true;
        }
    }
    return false;
}

static bool almanac_brightness_model_from_text(const char *text, almanac_brightness_model_t *out)
{
    static const struct {
        const char *text;
        almanac_brightness_model_t value;
    } brightness_models[] = {{"none", ALMANAC_BRIGHTNESS_MODEL_NONE},
                             {"catalogued", ALMANAC_BRIGHTNESS_MODEL_CATALOGUED},
                             {"sun_distance", ALMANAC_BRIGHTNESS_MODEL_SUN_DISTANCE},
                             {"planetary_phase", ALMANAC_BRIGHTNESS_MODEL_PLANETARY_PHASE},
                             {"lunar_phase", ALMANAC_BRIGHTNESS_MODEL_LUNAR_PHASE}};
    size_t i;

    if (!text || !out)
        return false;
    for (i = 0u; i < sizeof(brightness_models) / sizeof(brightness_models[0]); ++i) {
        if (strcmp(text, brightness_models[i].text) == 0) {
            *out = brightness_models[i].value;
            return true;
        }
    }
    return false;
}

bool almanac_fetch_chebyshev_position_segment(almanac_t *almanac, almanac_body_ref_id_t body_ref_id, double jd,
    almanac_chebyshev_position_segment_t *out)
{
    static const char *sql = "select selected.start_jd, selected.end_jd, selected.segment_span_days, "
                             "       selected.degree, selected.segment_index, segment.coefficient_blob "
                             "from ( "
                             "    select series.series_id, start.start_jd, finish.end_jd, "
                             "           span.segment_span_days, degree.degree, "
                             "           case when ?2 >= finish.end_jd then segment_count.segment_count - 1 "
                             "                else cast((?2 - start.start_jd) / span.segment_span_days as integer) "
                             "           end as segment_index "
                             "    from almanac_chebyshev_position_series as series "
                             "    join almanac_chebyshev_position_series_body_ref as body_ref "
                             "      on body_ref.series_id = series.series_id "
                             "    join almanac_chebyshev_position_series_frame as series_frame "
                             "      on series_frame.series_id = series.series_id "
                             "    join almanac_frame_code as frame_code "
                             "      on frame_code.frame_id = series_frame.frame_id "
                             "    join almanac_chebyshev_position_series_start_jd as start "
                             "      on start.series_id = series.series_id "
                             "    join almanac_chebyshev_position_series_end_jd as finish "
                             "      on finish.series_id = series.series_id "
                             "    join almanac_chebyshev_position_series_segment_span_days as span "
                             "      on span.series_id = series.series_id "
                             "    join almanac_chebyshev_position_series_segment_count as segment_count "
                             "      on segment_count.series_id = series.series_id "
                             "    join almanac_chebyshev_position_series_degree as degree "
                             "      on degree.series_id = series.series_id "
                             "    where body_ref.body_ref_id = ?1 and frame_code.frame_code = 'ECLIPJ2000' "
                             "      and start.start_jd <= ?2 and finish.end_jd >= ?2 "
                             "    order by start.start_jd asc "
                             "    limit 1 "
                             ") as selected "
                             "join almanac_chebyshev_position_segment as segment "
                             "  on segment.series_id = selected.series_id "
                             " and segment.segment_index = selected.segment_index";
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

    if (!almanac || !almanac->db || body_ref_id <= ALMANAC_BODY_REF_ID_NONE || !out) {
        almanac_set_error(almanac, "invalid Chebyshev position segment lookup");
        return false;
    }
    if (body_ref_id < ALMANAC_BODY_REF_ID_LIMIT && almanac->chebyshev_position_segment_cached[body_ref_id] &&
        jd >= almanac->chebyshev_position_segment_cache[body_ref_id].start_jd &&
        jd <= almanac->chebyshev_position_segment_cache[body_ref_id].end_jd) {
        *out = almanac->chebyshev_position_segment_cache[body_ref_id];
        return true;
    }

    memset(out, 0, sizeof(*out));
    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        almanac_set_sqlite_error(almanac);
        return false;
    }
    if (!sqlite_stmt_bind_int(stmt, 1, body_ref_id) || !sqlite_stmt_bind_double(stmt, 2, jd)) {
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

    if (degree < 1 || degree >= ALMANAC_CHEB_MAX_COEFF_COUNT || segment_index < 0 || segment_span_days <= 0.0 ||
        !blob) {
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

            memcpy(&value, blob + sizeof(double) * ((size_t)component * (size_t)(degree + 1) + (size_t)coeff),
                   sizeof(value));
            out->coeff[component][coeff] = value;
        }
    }
    sqlite_stmt_finalize(stmt);
    if (body_ref_id < ALMANAC_BODY_REF_ID_LIMIT) {
        almanac->chebyshev_position_segment_cache[body_ref_id] = *out;
        almanac->chebyshev_position_segment_cached[body_ref_id] = true;
    }
    return true;
}

bool almanac_fetch_frame_rotation_segment(almanac_t *almanac, double jd, almanac_frame_rotation_segment_t *out)
{
    static const char *sql = "select selected.start_jd, selected.end_jd, selected.segment_span_days, "
                             "       selected.degree, selected.segment_index, segment.coefficient_blob "
                             "from ( "
                             "    select series.series_id, start.start_jd, finish.end_jd, "
                             "           span.segment_span_days, degree.degree, "
                             "           case when ?1 >= finish.end_jd then segment_count.segment_count - 1 "
                             "                else cast((?1 - start.start_jd) / span.segment_span_days as integer) "
                             "           end as segment_index "
                             "    from almanac_frame_rotation_series as series "
                             "    join almanac_frame_rotation_series_source_frame as source_frame "
                             "      on source_frame.series_id = series.series_id "
                             "    join almanac_frame_rotation_series_target_frame as target_frame "
                             "      on target_frame.series_id = series.series_id "
                             "    join almanac_frame_code as source_frame_code "
                             "      on source_frame_code.frame_id = source_frame.frame_id "
                             "    join almanac_frame_code as target_frame_code "
                             "      on target_frame_code.frame_id = target_frame.frame_id "
                             "    join almanac_frame_rotation_series_start_jd as start "
                             "      on start.series_id = series.series_id "
                             "    join almanac_frame_rotation_series_end_jd as finish "
                             "      on finish.series_id = series.series_id "
                             "    join almanac_frame_rotation_series_segment_span_days as span "
                             "      on span.series_id = series.series_id "
                             "    join almanac_frame_rotation_series_segment_count as segment_count "
                             "      on segment_count.series_id = series.series_id "
                             "    join almanac_frame_rotation_series_degree as degree "
                             "      on degree.series_id = series.series_id "
                             "    where source_frame_code.frame_code = 'ECLIPJ2000' "
                             "      and target_frame_code.frame_code = 'TRUE_EQUATOR_DATE' "
                             "      and start.start_jd <= ?1 and finish.end_jd >= ?1 "
                             "    order by start.start_jd asc "
                             "    limit 1 "
                             ") as selected "
                             "join almanac_frame_rotation_segment as segment "
                             "  on segment.series_id = selected.series_id "
                             " and segment.segment_index = selected.segment_index";
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
    if (almanac->frame_rotation_segment_cached && jd >= almanac->frame_rotation_segment_cache.start_jd &&
        jd <= almanac->frame_rotation_segment_cache.end_jd) {
        *out = almanac->frame_rotation_segment_cache;
        return true;
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

    if (degree < 1 || degree >= ALMANAC_CHEB_MAX_COEFF_COUNT || segment_index < 0 || segment_span_days <= 0.0 ||
        !blob) {
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

            memcpy(&value, blob + sizeof(double) * ((size_t)component * (size_t)(degree + 1) + (size_t)coeff),
                   sizeof(value));
            out->coeff[component][coeff] = value;
        }
    }
    sqlite_stmt_finalize(stmt);
    almanac->frame_rotation_segment_cache = *out;
    almanac->frame_rotation_segment_cached = true;
    return true;
}
