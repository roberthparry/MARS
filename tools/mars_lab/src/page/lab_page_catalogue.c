/**
 * @file lab_page_catalogue.c
 * @brief Native packaged worksheet defaults and presentation constants for MARS Lab.
 *
 * Preserves the user's packaged settings snapshot as ordinary C data. Each call
 * builds an independently owned value tree for state defaults or Protobuf
 * transport; no JSON asset is opened or parsed. Geographic data remains in the
 * jurisdiction database. Change these tables and rebuild to change packaged defaults.
 */
#include "lab_page.h"
#include "number.h"

typedef struct {
    const char *name;
    const char *text;
    long number;
} lab_page_setting_t;

static const char lab_page_accuracy_note[] =
    "Packaged ephemeris coverage: 1550-2649 GMT. Navigation body positions are "
    "reported rounded to the nearest arc-second.";

static const char lab_page_expression_default[] =
    "{ 100 - 100/(√(2π)σ)·∫^x_-∞ e^(-½·(t - μ)²/σ²)·dt | σ = 15, x = 134.895218110612"
    "61651328409245020367585027725712298019603598460371836314939684051731035234396186"
    "426975413794385305751, μ = 100 }";

static const char lab_page_subtitle[] =
    "Switch between expression, equation, differential-equation, matrix, integrator, "
    "datetime, and almanac experiments. Each mode runs through a local MARS worker "
    "binary and shows the result on the right.";

static const lab_page_setting_t lab_page_constants[] = {
    {"ALMANAC_ACCURACY_NOTE", lab_page_accuracy_note, 0},
    {"ALMANAC_COVERAGE_TEXT", "1550-2649 GMT", 0},
    {"ALMANAC_LAND_TOTALITY_SEARCH_TIMEOUT_SECONDS", NULL, 20},
    {"ALMANAC_WORKSHEET_TITLE", "AstroNav Navigation Almanac", 0},
    {"DEFAULT_ALMANAC_BODY", "MOON", 0},
    {"DEFAULT_ALMANAC_DATE", "2026-10-05", 0},
    {"DEFAULT_ALMANAC_ELEVATION", "75", 0},
    {"DEFAULT_ALMANAC_LATITUDE", "52.707700", 0},
    {"DEFAULT_ALMANAC_LONGITUDE", "-2.754100", 0},
    {"DEFAULT_ALMANAC_TEXT", "Navigation almanac worksheet", 0},
    {"DEFAULT_ALMANAC_TIME", "14:43:34", 0},
    {"DEFAULT_ALMANAC_VISIBILITY", "visible", 0},
    {"DEFAULT_ALMANAC_ZONE", "1.00", 0},
    {"DEFAULT_DATETIME_DATE", "2026-10-05", 0},
    {"DEFAULT_DATETIME_ELEVATION", "75", 0},
    {"DEFAULT_DATETIME_GMT_OFFSET", "", 0},
    {"DEFAULT_DATETIME_LATITUDE", "52.7077", 0},
    {"DEFAULT_DATETIME_LONGITUDE", "-2.7541", 0},
    {"DEFAULT_DATETIME_TEXT", "Calendar and solar calculations, with optional holiday lookup", 0},
    {"DEFAULT_DIFFEQUATION", "y''+4y'+5y = 50t; y(0) = 5; y'(0) = -5", 0},
    {"DEFAULT_EQUATION", "(x+1)^4 = 1", 0},
    {"DEFAULT_EQUATION_VARIABLE", "x", 0},
    {"DEFAULT_EXPRESSION", lab_page_expression_default, 0},
    {"DEFAULT_HOLIDAY_JURISDICTION", "GB-ENG", 0},
    {"DEFAULT_HOLIDAY_JURISDICTION_FROM_LOCALE", "GB-ENG", 0},
    {"DEFAULT_HOLIDAY_JURISDICTION_FROM_TIMEZONE", "GB-ENG", 0},
    {"DEFAULT_INTEGRATOR_BOUNDS", "x = 0 .. 1", 0},
    {"DEFAULT_INTEGRATOR_EXPRESSION", "{ exp(Li(x)) | x = ? }", 0},
    {"DEFAULT_INTEGRATOR_INTERVAL_CAP", NULL, 20000},
    {"DEFAULT_MATRIX", "{ (1, 2; 3, 4)^x | x = @pi }", 0},
    {"DEFAULT_MATRIX_OPERATION", "eval", 0},
    {"DEFAULT_TIMEZONE_LATITUDE", "52.7077", 0},
    {"DEFAULT_TIMEZONE_LONGITUDE", "-2.7541", 0},
    {"LAB_APP_NAME", "MARS Lab", 0},
    {"LAB_DESCRIPTION", "Explore MARS mathematics with rendered TeX.", 0},
    {"LAB_MANIFEST_BACKGROUND", "#f6f0e5", 0},
    {"LAB_MANIFEST_THEME", "#0b4f8a", 0},
    {"LAB_SHORT_NAME", "MARS Lab", 0},
    {"LAB_SUBTITLE", lab_page_subtitle, 0},
    {"LAB_THEME_COLOR", "#071913", 0},
};

static const lab_page_setting_t lab_page_default_settings[] = {
    {"almanac_date", "2026-10-05", 0},
    {"almanac_elevation", "75", 0},
    {"almanac_jurisdiction", "GB-ENG", 0},
    {"almanac_latitude", "52.707700", 0},
    {"almanac_longitude", "-2.754100", 0},
    {"almanac_time", "14:43:34", 0},
    {"almanac_town", "Shrewsbury|52.7077|-2.7541|75", 0},
    {"almanac_visibility", "visible", 0},
    {"almanac_zone", "1.00", 0},
    {"datetime_date", "2026-10-05", 0},
    {"datetime_elevation", "75", 0},
    {"datetime_end", "2027-01-01", 0},
    {"datetime_gmt_offset", "", 0},
    {"datetime_jdn", "2461319", 0},
    {"datetime_jurisdiction", "GB-ENG", 0},
    {"datetime_latitude", "52.7077", 0},
    {"datetime_longitude", "-2.7541", 0},
    {"datetime_start", "2026-01-01", 0},
    {"datetime_town", "Shrewsbury|52.7077|-2.7541|75", 0},
    {"datetime_year", "2026", 0},
    {"diffequation", "y''+4y'+5y = 50t; y(0) = 5; y'(0) = -5", 0},
    {"equation", "(x+1)^4 = 1", 0},
    {"equation_updated_at", NULL, 0},
    {"equation_variable", "x", 0},
    {"expression", lab_page_expression_default, 0},
    {"expression_updated_at", NULL, 0},
    {"integrator_bounds", "x = 0 .. 1", 0},
    {"integrator_expression", "{ exp(Li(x)) | x = ? }", 0},
    {"integrator_interval_cap", NULL, 20000},
    {"lab_mode", "equation", 0},
    {"matrix", "{ (1, 2; 3, 4)^x | x = @pi }", 0},
    {"matrix_operand", "", 0},
    {"matrix_operation", "eval", 0},
};

static const lab_page_setting_t lab_page_precision[] = {
    {"almanac", NULL, 17},     {"datetime", NULL, 17},     {"diffequation", NULL, 256}, {"equation", NULL, 256},
    {"expression", NULL, 384}, {"integrator", NULL, 1280}, {"matrix", NULL, 106},
};

static bool lab_page_catalogue_attach(json_t *parent, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = parent && key && value && json_object_set(parent, key, value);
    string_free(key);
    return ok;
}

static json_t *lab_page_catalogue_object(const lab_page_setting_t *settings, size_t count)
{
    json_t *object = json_new_object();
    /* One bounded construction pass over the fixed packaged fields, not a lookup scan. */
    for (size_t i = 0; object && i < count; ++i) {
        const lab_page_setting_t *setting = &settings[i];
        string_t *text = setting->text ? string_new_with(setting->text) : NULL;
        json_t *value = NULL;
        if (setting->text)
            value = text ? json_new_string(text) : NULL;
        else {
            number_t number = num_create_from_long(setting->number);
            value = json_new_number_value(number);
            num_destroy(&number);
        }
        bool ok = lab_page_catalogue_attach(object, setting->name, value);
        json_free(value);
        string_free(text);
        if (!ok) {
            json_free(object);
            object = NULL;
        }
    }
    return object;
}

/* Build an owned catalogue directly from native defaults, without file I/O or text parsing. */
json_t *lab_page_catalogue(void)
{
    json_t *constants =
        lab_page_catalogue_object(lab_page_constants, sizeof lab_page_constants / sizeof *lab_page_constants);
    json_t *defaults = lab_page_catalogue_object(lab_page_default_settings,
                                                 sizeof lab_page_default_settings / sizeof *lab_page_default_settings);
    json_t *precision =
        lab_page_catalogue_object(lab_page_precision, sizeof lab_page_precision / sizeof *lab_page_precision);
    json_t *catalogue = json_new_object();
    bool ok = lab_page_catalogue_attach(defaults, "precision_bits", precision) &&
              lab_page_catalogue_attach(catalogue, "constants", constants) &&
              lab_page_catalogue_attach(catalogue, "defaults", defaults);
    json_free(precision);
    json_free(defaults);
    json_free(constants);
    if (!ok) {
        json_free(catalogue);
        catalogue = NULL;
    }
    return catalogue;
}
