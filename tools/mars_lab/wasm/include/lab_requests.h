/**
 * @file lab_requests.h
 * @brief Private interface: freestanding browser request ownership and operation policy for MARS Lab.
 *
 * Owns monotonically numbered requests, mode invalidation, busy state, dependent
 * weather work, operation routing and response acceptance. JavaScript retains
 * browser fetch, timers and DOM access, but cannot revive an obsolete request.
 * Fixed direct-indexed channels require no allocation or libc. No mathematics
 * is parsed or rewritten here. Calls run synchronously on the browser thread.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_REQUESTS_H
#define LAB_WASM_REQUESTS_H

#include <stdint.h>

/** @brief Count available operations. @return Operation count; also the invalid-operation sentinel. */
unsigned lab_request_operation_count(void);

/**
 * @brief Borrow an immutable operation, endpoint or action label.
 * @param kind Catalogue: zero for operations, one for endpoints, two for expression actions.
 * @param index Zero-based index in the selected catalogue.
 * @return Borrowed NUL-terminated label, or an empty string for an invalid kind or index.
 */
const char *lab_request_text(unsigned kind, unsigned index);

/**
 * @brief Measure a catalogue label for copying into the browser.
 * @param kind Catalogue identifier, as for lab_request_text.
 * @param index Zero-based index in that catalogue.
 * @return UTF-8 byte count excluding the terminator; zero for empty or invalid labels.
 */
unsigned lab_request_text_length(unsigned kind, unsigned index);

/** @brief Identify the current request-mode generation across asynchronous work.
 * @return Generation changed by every mode transition, including leaving and returning to the same mode.
 */
uint32_t lab_request_context(void);

/** @brief Read policy flags for a current request. @param channel Request channel. @param token Ownership token.
 * @return Persistence/transient policy bits, or zero for an obsolete request. */
unsigned lab_request_flags(unsigned channel, uint32_t token);

/** @brief Read the native action selector for a current request. @param channel Request channel.
 * @param token Ownership token. @return Action index, or zero for an obsolete request. */
unsigned lab_request_action(unsigned channel, uint32_t token);

/**
 * @brief Cancel a channel and invalidate dependent work when cancelling the main channel.
 * @param channel Channel index in 0..8; invalid indices are ignored.
 */
void lab_request_cancel(unsigned channel);

/**
 * @brief Select the request mode and invalidate outstanding work on a change.
 * @param mode Mode index in 0..6; other values select the invalid-mode sentinel.
 */
void lab_request_set_mode(unsigned mode);

/**
 * @brief Find the channel assigned to an operation.
 * @param operation Index in the native operation catalogue.
 * @return Channel index, or the channel-count sentinel for an invalid operation.
 */
unsigned lab_request_channel(unsigned operation);

/**
 * @brief Begin work after checking mode, operation and required inputs.
 * @param mode Mode index, which must match the selected request mode.
 * @param operation Index in the native operation catalogue.
 * @param has_text Whether required authored text is present.
 * @param has_variable Whether a required calculus variable is present.
 * @param scalar Whether matrix calculus uses scalar expression evaluation.
 * @param parent Main request token for dependent work, or zero for independent work.
 * @return New non-zero token, or zero for disallowed input or exhausted token space.
 */
uint32_t lab_request_begin(unsigned mode, unsigned operation, unsigned has_text, unsigned has_variable, unsigned scalar,
                           uint32_t parent);

/**
 * @brief Check current ownership, including mode and parent identity.
 * @param channel Channel index.
 * @param token Token to validate.
 * @return One for live owned work; zero otherwise.
 */
unsigned lab_request_live(unsigned channel, uint32_t token);

/**
 * @brief Recheck required input after asynchronous editor preparation.
 * @param channel Channel index.
 * @param token Token to validate.
 * @param has_text Whether authored text is present.
 * @param has_variable Whether a calculus variable is present.
 * @return One for live work with all required inputs; zero otherwise.
 */
unsigned lab_request_input(unsigned channel, uint32_t token, unsigned has_text, unsigned has_variable);

/**
 * @brief Complete only the matching live request.
 * @param channel Channel index.
 * @param token Token being finalised.
 * @return One when completed; zero for stale or invalid work.
 */
unsigned lab_request_finish(unsigned channel, uint32_t token);

/**
 * @brief Check whether a blocking request is active.
 * @return One for active blocking main or binding work; zero otherwise.
 */
unsigned lab_request_busy(void);

/**
 * @brief Obtain the current mode's latest main request identity.
 * @return Token, including after completion, or zero when absent or from another mode.
 */
uint32_t lab_request_latest_main(void);

/**
 * @brief Resolve the operation's endpoint for live work.
 * @param channel Channel index.
 * @param token Token to validate.
 * @return Endpoint catalogue index; zero for stale or invalid work.
 */
unsigned lab_request_endpoint(unsigned channel, uint32_t token);

/**
 * @brief Read the operation's client timeout.
 * @param channel Channel index.
 * @param token Token to validate.
 * @return Deadline in milliseconds, or zero for no deadline or invalid work.
 */
unsigned lab_request_deadline(unsigned channel, uint32_t token);

/**
 * @brief Record a deadline before the host aborts fetch.
 * @param channel Channel index.
 * @param token Token to validate.
 * @return One if live work with a deadline was marked expired; zero otherwise.
 */
unsigned lab_request_timeout(unsigned channel, uint32_t token);

/**
 * @brief Distinguish a request's own timeout from cancellation.
 * @param channel Channel index.
 * @param token Token to validate.
 * @return One if this live owner expired; zero otherwise.
 */
unsigned lab_request_timed_out(unsigned channel, uint32_t token);

/**
 * @brief Classify an owned response without interpreting mathematical text.
 * @param channel Channel index.
 * @param token Token to validate.
 * @param http_ok Whether the HTTP response succeeded.
 * @param native_ok Whether the native operation succeeded.
 * @param has_result Whether the required result is present.
 * @param partial Whether ordinary expression evaluation reported a partial result.
 * @return Zero for stale work, one for failure, two for success or three for a partial expression result.
 */
unsigned lab_request_accept(unsigned channel, uint32_t token, unsigned http_ok, unsigned native_ok, unsigned has_result,
                            unsigned partial);

#endif
