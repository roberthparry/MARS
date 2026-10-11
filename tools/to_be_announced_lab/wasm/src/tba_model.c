/**
 * @file tba_model.c
 * @brief Table-driven seasonal options and suggested forecasting model orders.
 *
 * Built both natively for regression tests and as freestanding WebAssembly. No
 * DOM, allocation, parsing or fitting takes place here; lookup cost is constant.
 */
#include "tba_model.h"

typedef struct {
    int value;
    const char *label;
} tba_model_option_t;

static const tba_model_option_t tba_model_options[][6] = {
    {{0, "None"}, {1, "Weak / unclear"}},
    {{0, "None"}, {1, "Weak / unclear"}, {3, "Quarterly"}, {4, "Every 4 months"}, {6, "Half-yearly"}, {12, "Yearly"}},
    {{0, "None"}, {1, "Weak / unclear"}, {2, "Half-yearly"}, {4, "Yearly"}}
};

static unsigned tba_model_option_row(unsigned frequency)
{
    return frequency == 2 ? 1 : frequency == 3 ? 2 : 0;
}

/* Keep the browser adapter and application-specific exports in step. */
unsigned tba_browser_abi_version(void)
{
    return 2;
}

/* Prefer a measured clear seasonal lag over frequency-based defaults. */
int tba_model_season(unsigned frequency, unsigned strength, int lag)
{
    static const int defaults[] = {-1, -1, 12, 4, 1};
    if (strength >= 3 && strength <= 5 && lag > 1)
        return lag;
    if (strength == 1 || strength == 2)
        return (int)strength - 1;
    return frequency < sizeof defaults / sizeof *defaults ? defaults[frequency] : -1;
}

/* Model and order are direct indices; only seasonal autoregression varies by strength. */
int tba_model_order(unsigned model, unsigned strength, unsigned order)
{
    static const int orders[6][6] = {
        {0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0}, {2, 1, 1, 0, 0, 0}
    };
    if (model >= 6 || order >= 6 || strength > 5)
        return -1;
    return order == 3 && model >= 3 && strength >= 3 ? 1 : orders[model][order];
}

/* Offer a bounded set appropriate to the observed frequency. */
unsigned tba_model_option_count(unsigned frequency)
{
    static const unsigned counts[] = {2, 6, 4};
    return counts[tba_model_option_row(frequency)];
}

/* Read the value without exposing the native structure layout to JavaScript. */
int tba_model_option_value(unsigned frequency, unsigned index)
{
    return index < tba_model_option_count(frequency) ? tba_model_options[tba_model_option_row(frequency)][index].value : -1;
}

/* The adapter copies the borrowed NUL-terminated label out of WASM memory. */
const char *tba_model_option_label(unsigned frequency, unsigned index)
{
    return index < tba_model_option_count(frequency) ? tba_model_options[tba_model_option_row(frequency)][index].label : "";
}

/* Seasonal order controls apply only to the final three model kinds. */
int tba_model_seasonal(unsigned model)
{
    return model >= 3 && model <= 5;
}

/* Six fixed model bits preserve UI order without string dispatch. */
unsigned tba_model_preferred(unsigned mask, unsigned strength)
{
    unsigned preferred = strength >= 3 && strength <= 5 ? mask & 56u : 0;
    if (!preferred)
        preferred = mask & 62u;
    if (!preferred)
        return mask & 1u ? 0 : 4;
    return (unsigned)__builtin_ctz(preferred);
}

/* All warning decisions are shared by the browser and native policy tests. */
unsigned tba_model_warning(unsigned mask, unsigned strength, int lag, int season, int p, int d, int q)
{
    if (mask & 56u) {
        if (strength == 1)
            return season || p || d || q ? 1 : 2;
        if (strength == 2)
            return season != 1 || p ? 3 : 4;
        if (lag > 1)
            return season != lag || p != 1 ? 5 : 6;
    } else if (strength >= 3 && strength <= 5 && lag > 1) {
        return 7;
    }
    return 0;
}

/* Static guidance stays next to the policy which selects it. */
const char *tba_model_warning_text(unsigned code)
{
    static const char *const messages[] = {
        "",
        "No seasonality was detected. A safer first try is Season length = None and P, D, Q all set to 0.",
        "No seasonality was detected, and seasonal settings are turned off. That is a sensible starting point.",
        "Only a weak seasonal signal was detected. Weak / unclear season length and P = 0 are safer starting choices.",
        "Only a weak seasonal signal was detected, and the seasonal settings are staying light.",
        "The seasonal settings do not match the detected pattern. Use the detected lag as Season length and P = 1.",
        "The seasonal settings match the detected pattern reasonably well for a first run.",
        "The selected models are non-seasonal. Consider SARIMA or SARIMAX to use the detected seasonal pattern."
    };
    return code < sizeof messages / sizeof *messages ? messages[code] : "";
}

/* Ratings use direct warning-code indexing. */
const char *tba_model_warning_class(unsigned code)
{
    static const char *const classes[] = {
        "", "rating-mediocre", "rating-good", "rating-mediocre",
        "rating-good", "rating-poor", "rating-good", "rating-mediocre"
    };
    return code < sizeof classes / sizeof *classes ? classes[code] : "";
}

/* Preserve explicit user edits unless they request suggestions or leave an empty/default field. */
int tba_model_replace(int force, int clear, int edited, int empty, int zero)
{
    return force || (clear && (empty || zero)) || !edited || empty;
}
