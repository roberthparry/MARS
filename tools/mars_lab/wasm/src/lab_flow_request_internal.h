/**
 * @file lab_flow_request_internal.h
 * @brief Private request helpers shared by the request and calculus continuations.
 *
 * Only the request workflow implementation may use these scoped frame accessors.
 * Other controllers enter through lab_flow_request() in lab_flow_request.h.
 */
#if !defined(MARS_LAB_FLOW_REQUEST_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_FLOW_REQUEST_INTERNAL_ACCESS."
#endif

#ifndef LAB_FLOW_REQUEST_INTERNAL_H
#define LAB_FLOW_REQUEST_INTERNAL_H

#include <stdbool.h>

/**
 * @brief Read the current continuation-stage index from a request frame.
 * @param frame Borrowed browser frame whose numeric stage property selects the next request handler.
 * @return The stage property converted to an unsigned index.
 */
unsigned lab_flow_request_stage(int frame);

/**
 * @brief Read a request-frame property using browser truth-value conversion.
 * @param frame Borrowed browser frame containing the property to inspect.
 * @param key NUL-terminated property name within the frame.
 * @return True when the named value is truthy; false otherwise.
 */
bool lab_flow_request_truth(int frame, const char *key);

/**
 * @brief Keep a truthy browser value or create a literal fallback string.
 * @param value Scoped browser value handle to test without coercing the retained value.
 * @param fallback NUL-terminated text used when value is falsey.
 * @return The original handle when truthy, otherwise a scoped handle for the fallback string.
 */
int lab_flow_request_fallback(int value, const char *fallback);

/**
 * @brief Schedule an ownership check before a request continuation.
 * @param frame Borrowed browser frame holding request and receiving its ownership result in current.
 * @param next Continuation-stage index stored in frame for resumption after the ownership check.
 * @return Scoped operation-plan handle calling requestCurrent.
 */
int lab_flow_request_current(int frame, unsigned next);

/**
 * @brief Read the transport response from the retained request result.
 * @param frame Borrowed browser frame whose result object contains response.
 * @return Scoped handle for result.response, or the bridge's absent-value handle when missing.
 */
int lab_flow_request_response(int frame);

/**
 * @brief Read the decoded native payload from the retained request result.
 * @param frame Borrowed browser frame whose result object contains data.
 * @return Scoped handle for result.data, or the bridge's absent-value handle when missing.
 */
int lab_flow_request_data(int frame);

/**
 * @brief Advance the calculus or plot request through its continuation table.
 * @param frame Borrowed browser frame retaining stage, request, action, differentiation variable and intermediate
 *   results.
 * @return Scoped operation or completion plan for the selected calculus/plot stage.
 */
int lab_flow_request_calculus(int frame);

#endif
