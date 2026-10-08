/**
 * @file tba_forecast.h
 * @brief Native forecast orchestration and safe result presentation.
 *
 * Runs the existing C timeseries worker with validated argument vectors and
 * bounded deadlines, preserving single- and multi-model response contracts.
 * Mathematical fitting remains in MARSlib, never in browser code.
 */
#ifndef TBA_FORECAST_H
#define TBA_FORECAST_H
#include "tba_app.h"
/** @brief Enumerate allowed models. @param selection Comma-separated tokens. @return Owned selected-token array; NULL for invalid input. */
json_t *tba_forecast_models(const char *selection);
/** @brief Borrow a supported model token. @param index Index 0..5. @return Static token or NULL. */
const char *tba_forecast_model(size_t index);
/** @brief Borrow a model label. @param token Supported model token. @return Static label or token when unknown. */
const char *tba_forecast_label(const char *token);
/** @brief Run sequential selected forecasts. @param app Application. @param payload Settings object. @return Owned response or NULL. */
json_t *tba_forecast_run(tba_app_t *app, const json_t *payload);
/** @brief Render safe summary cards. @param text Summary text. @param drivers Driver headers. @return Owned HTML or NULL. */
string_t *tba_forecast_summary(const char *text, const char *drivers);
/** @brief Analyse forecast values. @param csv Forecast CSV text. @return Owned warning object, empty when no concern, or NULL. */
json_t *tba_forecast_plausibility(const char *csv);
/** @brief Merge successful model outputs. @param results Borrowed array. @return Owned combined response or NULL. */
json_t *tba_forecast_combine(const json_t *results);
#endif
