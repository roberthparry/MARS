/**
 * @file tba_model.h
 * @brief Browser-independent forecasting setup policy, shared by WASM and native tests.
 *
 * Numeric identifiers are a private browser ABI: frequencies are unknown, daily,
 * monthly, quarterly, yearly; strengths are unknown, none, weak, moderate, strong,
 * very strong; models are regression, ARIMA, ARIMAX, SARIMA, SARIMAX, Auto-ARIMA.
 * These helpers suggest settings only. Model fitting remains in the native server.
 */
#ifndef TBA_MODEL_H
#define TBA_MODEL_H

/** @brief Identify this application's browser ABI. @return ABI version one. */
unsigned tba_browser_abi_version(void);

/**
 * @brief Suggest the seasonal period.
 * @param frequency Frequency identifier, zero through four.
 * @param strength Seasonality identifier, zero through five.
 * @param lag Detected integral lag, or zero when unavailable.
 * @return Suggested period, or -1 when no suggestion is available.
 */
int tba_model_season(unsigned frequency, unsigned strength, int lag);

/**
 * @brief Suggest an individual model order without mutating worksheet state.
 * @param model Model identifier, zero through five.
 * @param strength Seasonality identifier, zero through five.
 * @param order Index of p, d, q, P, D or Q, zero through five.
 * @return Suggested order, or -1 for invalid arguments.
 */
int tba_model_order(unsigned model, unsigned strength, unsigned order);

/**
 * @brief Count seasonal options for a frequency.
 * @param frequency Frequency identifier.
 * @return Number of options, including none and weak/unclear.
 */
unsigned tba_model_option_count(unsigned frequency);

/**
 * @brief Read a seasonal option's value.
 * @param frequency Frequency identifier.
 * @param index Zero-based option index.
 * @return Period value, or -1 for an invalid index.
 */
int tba_model_option_value(unsigned frequency, unsigned index);

/**
 * @brief Borrow a seasonal option's display label.
 * @param frequency Frequency identifier.
 * @param index Zero-based option index.
 * @return Module-owned UTF-8 label, or an empty string for an invalid index.
 */
const char *tba_model_option_label(unsigned frequency, unsigned index);

/** @brief Classify a selected model. @param model Identifier 0..5. @return Non-zero when seasonal orders apply. */
int tba_model_seasonal(unsigned model);

/**
 * @brief Choose which selected model supplies suggestions.
 * @param mask Model selection bit mask.
 * @param strength Detected strength 0..5.
 * @return Model identifier.
 */
unsigned tba_model_preferred(unsigned mask, unsigned strength);

/**
 * @brief Evaluate seasonal settings.
 * @param mask Selection bit mask.
 * @param strength Strength 0..5.
 * @param lag Detected lag.
 * @param season Selected period.
 * @param p Seasonal P.
 * @param d Seasonal D.
 * @param q Seasonal Q.
 * @return Warning code 0..7.
 */
unsigned tba_model_warning(unsigned mask, unsigned strength, int lag, int season, int p, int d, int q);

/** @brief Borrow warning text. @param code Warning code 0..7. @return Static text or empty string. */
const char *tba_model_warning_text(unsigned code);

/** @brief Borrow warning rating class. @param code Warning code. @return Static CSS class. */
const char *tba_model_warning_class(unsigned code);

/**
 * @brief Decide whether a suggestion may replace an edited field.
 * @param force Explicit suggestion request.
 * @param clear Clear seasonal signal.
 * @param edited User-edited flag.
 * @param empty Empty field.
 * @param zero Field currently zero.
 * @return Non-zero to replace.
 */
int tba_model_replace(int force, int clear, int edited, int empty, int zero);

#endif
