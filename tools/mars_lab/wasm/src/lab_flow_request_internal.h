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

/** @brief Read the current stage from a borrowed browser frame. */
unsigned lab_flow_request_stage(int frame);

/** @brief Read the truth value of a named frame property. */
int lab_flow_request_truth(int frame, const char *key);

/** @brief Retain a truthy scoped value or return the supplied literal fallback. */
int lab_flow_request_fallback(int value, const char *fallback);

/** @brief Schedule an ownership check before the given continuation stage. */
int lab_flow_request_current(int frame, unsigned next);

/** @brief Borrow the transport response stored in a request frame. */
int lab_flow_request_response(int frame);

/** @brief Borrow the native payload stored in a request frame. */
int lab_flow_request_data(int frame);

/** @brief Advance the calculus or plot workflow through its indexed stage table. */
int lab_flow_request_calculus(int frame);

#endif
