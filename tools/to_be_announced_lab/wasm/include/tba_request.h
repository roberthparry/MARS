/**
 * @file tba_request.h
 * @brief Native request sequencing and completion handling.
 *
 * Owns request generations, upload/save transitions and forecast submission.
 * Fetch itself is a browser capability; no HTTP or application policy lives in JS.
 */
#ifndef TBA_REQUEST_H
#define TBA_REQUEST_H
/**
 * @brief Load metadata for the current dataset.
 * @param driver Non-zero selects driver data.
 */
void tba_request_metadata(int driver);

/**
 * @brief Upload the selected CSV.
 * @param driver Non-zero selects driver data.
 */
void tba_request_upload(int driver);

/**
 * @brief Save current worksheet settings.
 */
void tba_request_save(void);

/**
 * @brief Submit a forecast unless one is already running.
 */
void tba_request_forecast(void);

/**
 * @brief Handle an asynchronous completion.
 * @param action Action identifier.
 * @param serial Generation.
 * @param data Scoped response.
 * @param success HTTP success.
 */
void tba_request_complete(unsigned action, unsigned serial, int data, int success);
#endif
