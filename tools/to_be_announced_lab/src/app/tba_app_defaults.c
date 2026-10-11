/**
 * @file tba_app_defaults.c
 * @brief Compiled initial settings for the forecasting worksheet.
 *
 * The native application owns these defaults; no configuration asset is parsed
 * at startup. Values retain their scalar types in the in-memory tree consumed by
 * the shared Protobuf codec. Each caller receives an independent owned copy.
 */
#include "tba_app.h"

/* Construct every setting once, preserving the worksheet's established defaults. */
json_t *tba_app_defaults(void)
{
    static const struct {
        const char *key;
        const char *value;
        bool number;
    } defaults[] = {
        {"target_path",         "sample_data/Monthly Target Numbers.csv", false},
        {"target_display_name", "Monthly Sample LD Numbers.csv",         false},
        {"target_date_column",  "",                                      false},
        {"target_value_column", "18-24 LD No Health contrib Tot.",        false},
        {"outlier_mode",        "flag",                                  false},
        {"outlier_dates",       "",                                      false},
        {"xreg_path",           "sample_data/Monthly Population.csv",     false},
        {"xreg_display_name",   "Monthly Sample Population.csv",          false},
        {"xreg_date_column",    "DATE",                                  false},
        {"xreg_columns",        "All",                                   false},
        {"models",              "sarimax",                               false},
        {"frequency",           "monthly",                               false},
        {"year_type",           "fiscal",                                false},
        {"forecast_end_date",   "",                                      false},
        {"p",                   "1",                                      true},
        {"d",                   "1",                                      true},
        {"q",                   "0",                                      true},
        {"P",                   "1",                                      true},
        {"D",                   "0",                                      true},
        {"Q",                   "0",                                      true},
        {"season_period",       "12",                                     true},
        {"criterion",           "aic",                                   false},
        {"level",               "0.95",                                  false}
    };
    json_t *result = json_new_object();
    for (size_t i = 0; result && i < sizeof defaults / sizeof *defaults; ++i) {
        string_t *text = string_new_with(defaults[i].value);
        json_t *value = !text ? NULL : defaults[i].number ? json_new_number(text) : json_new_string(text);
        bool ok = value && tba_json_set(result, defaults[i].key, value);
        json_free(value);
        string_free(text);
        if (!ok) {
            json_free(result);
            result = NULL;
        }
    }
    return result;
}
