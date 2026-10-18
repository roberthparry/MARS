/**
 * @file tba_request.h
 * @brief Native request sequencing and completion handling.
 *
 * Owns request generations, local selection and encrypted submission.
 * Browser capabilities supply file reads, Web Crypto and fetch.
 */
#ifndef TBA_REQUEST_H
#define TBA_REQUEST_H

#include "tba_handle.h"
/**
 * @brief Load metadata for the current dataset.
 * @param driver Non-zero selects driver data.
 */
void tba_request_metadata(int driver);

/**
 * @brief Read the selected CSV into browser-local memory.
 * @param driver Non-zero selects driver data.
 */
void tba_request_upload(int driver);

/**
 * @brief Retain current settings in browser memory only.
 */
void tba_request_save(void);

/**
 * @brief Submit an encrypted request unless one is already running.
 */
void tba_request_forecast(void);

/**
 * @brief Handle an asynchronous completion.
 * @param action Action identifier.
 * @param serial Generation.
 * @param data Scoped response.
 * @param success HTTP success.
 */
void tba_request_complete(unsigned action, unsigned serial, jsref data, int success);
#endif
