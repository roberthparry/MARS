/**
 * @file lab_requests.h
 * @brief Application API: freestanding browser request ownership and operation policy for MARS Lab.
 *
 * Owns monotonically numbered requests, mode invalidation, busy state, dependent
 * weather work, operation routing and response acceptance. JavaScript retains
 * browser fetch, timers and DOM access, but cannot revive an obsolete request.
 * Fixed direct-indexed channels require no allocation or host system imports. No mathematics
 * is parsed or rewritten here. Calls run synchronously on the browser thread.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean arguments crossing the WebAssembly i32 ABI must be canonical 0 or 1;
 * C callers use false or true.
 */
#ifndef LAB_WASM_REQUESTS_H
#define LAB_WASM_REQUESTS_H

#include <stdbool.h>
#include <stdint.h>

#include "lab_mode.h"

/** @brief Request operations; values and the invalid sentinel are fixed by the browser ABI. */
typedef enum {
    LAB_REQUEST_EVALUATE              =  0, /**< Evaluate the current worksheet. */
    LAB_REQUEST_DERIVATIVE            =  1, /**< Differentiate the selected expression. */
    LAB_REQUEST_INTEGRAL              =  2, /**< Integrate the selected expression. */
    LAB_REQUEST_PROGRAMME             =  3, /**< Run the Function card. */
    LAB_REQUEST_FORECAST              =  4, /**< Fetch worksheet weather data. */
    LAB_REQUEST_GOAL                  =  5, /**< Perform goal seeking. */
    LAB_REQUEST_BINDINGS              =  6, /**< Refresh expression bindings. */
    LAB_REQUEST_LAND_TOTALITY         =  7, /**< Fetch land totality. */
    LAB_REQUEST_DATE_LOCATION         =  8, /**< Resolve a DateTime location. */
    LAB_REQUEST_SKY_LOCATION          =  9, /**< Resolve an Almanac location. */
    LAB_REQUEST_BINDING_COMMIT        = 10, /**< Commit edited bindings. */
    LAB_REQUEST_LOCAL_HOLIDAYS        = 11, /**< Fetch local holidays. */
    LAB_REQUEST_SOLVER_RENDER         = 12, /**< Render solver output. */
    LAB_REQUEST_SOLVER_RESTORE_RENDER = 13, /**< Restore solver rendering. */
    LAB_REQUEST_PLOT                  = 14, /**< Evaluate a plot. */
    LAB_REQUEST_OPERATIONS            = 15, /**< Operation count and invalid-operation sentinel. */
} lab_request_operation_t;

/** @brief Request ownership channels; invalid browser values must be checked before indexing. */
typedef enum {
    LAB_REQUEST_MAIN              =  0, /**< Main evaluation channel. */
    LAB_REQUEST_FUNCTION          =  1, /**< Function execution channel. */
    LAB_REQUEST_WEATHER           =  2, /**< Weather request channel. */
    LAB_REQUEST_BINDING           =  3, /**< Binding refresh and commit channel. */
    LAB_REQUEST_LAND              =  4, /**< Land-totality channel. */
    LAB_REQUEST_DATETIME_LOCATION =  5, /**< DateTime location channel. */
    LAB_REQUEST_ALMANAC_LOCATION  =  6, /**< Almanac location channel. */
    LAB_REQUEST_HOLIDAYS          =  7, /**< Holiday request channel. */
    LAB_REQUEST_SOLVER            =  8, /**< Solver rendering channel. */
    LAB_REQUEST_PLOT_CHANNEL      =  9, /**< Plot evaluation channel. */
    LAB_REQUEST_CHANNELS          = 10, /**< Channel count and invalid-channel sentinel. */
} lab_request_channel_t;

/** @brief Request completion outcomes; EXCEPTION is supplied locally, not by the request controller. */
typedef enum {
    LAB_REQUEST_OUTCOME_STALE     = 0, /**< The request no longer owns its channel. */
    LAB_REQUEST_OUTCOME_FAILURE   = 1, /**< The owned request failed. */
    LAB_REQUEST_OUTCOME_SUCCESS   = 2, /**< The owned request succeeded. */
    LAB_REQUEST_OUTCOME_PARTIAL   = 3, /**< Expression evaluation produced a partial result. */
    LAB_REQUEST_OUTCOME_EXCEPTION = 4, /**< Locally caught exception during completion; not controller-produced. */
} lab_request_outcome_t;

/** @brief Expression evaluation action forwarded to the native endpoint. */
typedef enum {
    LAB_REQUEST_ACTION_ORDINARY = 0, /**< Ordinary evaluation or differentiation; also the stale fallback. */
    LAB_REQUEST_ACTION_INTEGRAL = 1, /**< Integration action. */
    LAB_REQUEST_ACTION_BINDINGS = 2, /**< Binding refresh action. */
    LAB_REQUEST_ACTION_PLOT     = 3, /**< Plot action. */
} lab_request_action_t;

/** @brief Immutable ABI label catalogues. */
typedef enum {
    LAB_REQUEST_CATALOGUE_OPERATIONS = 0, /**< Operation names. */
    LAB_REQUEST_CATALOGUE_ENDPOINTS  = 1, /**< Request endpoint paths. */
    LAB_REQUEST_CATALOGUE_ACTIONS    = 2, /**< Expression action names. */
} lab_request_catalogue_t;

/** @brief Native endpoint catalogue; zero denotes no fixed endpoint or stale work. */
typedef enum {
    LAB_REQUEST_ENDPOINT_NONE                  =  0, /**< No endpoint. */
    LAB_REQUEST_ENDPOINT_EXPRESSION            =  1, /**< Expression evaluation. */
    LAB_REQUEST_ENDPOINT_MATRIX                =  2, /**< Matrix evaluation. */
    LAB_REQUEST_ENDPOINT_EQUATION              =  3, /**< Equation evaluation. */
    LAB_REQUEST_ENDPOINT_DIFFERENTIAL_EQUATION =  4, /**< Differential-equation evaluation. */
    LAB_REQUEST_ENDPOINT_INTEGRATOR            =  5, /**< Integrator evaluation. */
    LAB_REQUEST_ENDPOINT_DATETIME              =  6, /**< DateTime evaluation. */
    LAB_REQUEST_ENDPOINT_ALMANAC               =  7, /**< Almanac evaluation. */
    LAB_REQUEST_ENDPOINT_FUNCTION              =  8, /**< Function execution. */
    LAB_REQUEST_ENDPOINT_WEATHER               =  9, /**< Weather data. */
    LAB_REQUEST_ENDPOINT_GOAL                  = 10, /**< Goal seeking. */
    LAB_REQUEST_ENDPOINT_LAND                  = 11, /**< Land totality. */
    LAB_REQUEST_ENDPOINT_DATETIME_LOCATION     = 12, /**< DateTime location. */
    LAB_REQUEST_ENDPOINT_ALMANAC_LOCATION      = 13, /**< Almanac location. */
    LAB_REQUEST_ENDPOINT_RENDER                = 14, /**< TeX rendering. */
    LAB_REQUEST_ENDPOINT_ALMANAC_WEATHER       = 15, /**< Background Almanac atmosphere refresh. */
} lab_request_endpoint_t;

/** @brief Count available operations. @return Operation count; also the invalid-operation sentinel. */
unsigned lab_request_operation_count(void);

/**
 * @brief Borrow an immutable operation, endpoint or action label.
 * @param kind Catalogue: zero for operations, one for endpoints, two for expression actions.
 * @param index Zero-based index in the selected catalogue.
 * @return Borrowed NUL-terminated label, or an empty string for an invalid kind or index.
 */
const char *lab_request_text(lab_request_catalogue_t kind, unsigned index);

/**
 * @brief Measure a catalogue label for copying into the browser.
 * @param kind Catalogue identifier, as for lab_request_text.
 * @param index Zero-based index in that catalogue.
 * @return UTF-8 byte count excluding the terminator; zero for empty or invalid labels.
 */
unsigned lab_request_text_length(lab_request_catalogue_t kind, unsigned index);

/** @brief Identify the current request-mode generation across asynchronous work.
 * @return Generation changed by every mode transition, including leaving and returning to the same mode.
 */
uint32_t lab_request_context(void);

/**
 * @brief Read evaluation policy flags for a current request.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Ownership token returned by lab_request_begin.
 * @return Bit 0 (1) denotes Expression-mode work; bit 1 (2) denotes Matrix-mode derivative/integral work.
 * Zero means neither policy applies or the request is obsolete.
 */
unsigned lab_request_flags(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Read the expression action selector for a current request.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Ownership token returned by lab_request_begin.
 * @return Action catalogue index: ordinary/derivative 0, integral 1, bindings 2 or plot 3;
 * obsolete requests also return zero.
 */
lab_request_action_t lab_request_action(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Cancel a channel and invalidate dependent work when cancelling the main channel.
 * @param channel Channel index in 0..9; invalid indices are ignored.
 */
void lab_request_cancel(lab_request_channel_t channel);

/**
 * @brief Select the request mode and invalidate outstanding work on a change.
 * @param mode Mode index in 0..6; other values select the invalid-mode sentinel.
 */
void lab_request_set_mode(lab_mode_t mode);

/**
 * @brief Find the channel assigned to an operation.
 * @param operation Index in the native operation catalogue, below lab_request_operation_count().
 * @return Main 0, Function 1, weather 2, bindings 3, land 4, DateTime location 5, Almanac location 6,
 * holidays 7, solver rendering 8 or plot 9; invalid operations return the channel-count sentinel 10.
 */
lab_request_channel_t lab_request_channel(lab_request_operation_t operation);

/**
 * @brief Begin work after checking mode, operation and required inputs.
 * @param mode Worksheet mode index in 0..6, which must match the selected request mode.
 * @param operation Native operation catalogue index, below lab_request_operation_count().
 * @param has_text True if required authored text is present.
 * @param has_variable True if a required calculus variable is present.
 * @param scalar True if matrix calculus uses scalar expression evaluation.
 * @param parent Main request token for dependent work, or zero for independent work.
 * @return New non-zero token, or zero for disallowed input or exhausted token space.
 */
uint32_t lab_request_begin(lab_mode_t mode, lab_request_operation_t operation, bool has_text, bool has_variable,
                           bool scalar, uint32_t parent);

/**
 * @brief Check current ownership, including mode and parent identity.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token to validate.
 * @return True for live owned work; false otherwise.
 */
bool lab_request_live(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Recheck required input after asynchronous editor preparation.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token to validate.
 * @param has_text True if authored text is present.
 * @param has_variable True if a calculus variable is present.
 * @return True for live work with all required inputs; false otherwise.
 */
bool lab_request_input(lab_request_channel_t channel, uint32_t token, bool has_text, bool has_variable);

/**
 * @brief Complete only the matching live request.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token being finalised.
 * @return True when completed; false for stale or invalid work.
 */
bool lab_request_finish(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Check whether a blocking request is active.
 * @return True for active blocking main or binding work; false otherwise.
 */
bool lab_request_busy(void);

/**
 * @brief Obtain the current mode's latest main request identity.
 * @return Token, including after completion, or zero when absent or from another mode.
 */
uint32_t lab_request_latest_main(void);

/**
 * @brief Resolve the operation's endpoint for live work.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token to validate.
 * @return Endpoint catalogue index; zero for stale or invalid work.
 */
lab_request_endpoint_t lab_request_endpoint(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Read the operation's client timeout.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token to validate.
 * @return Deadline in milliseconds, or zero for no deadline or invalid work.
 */
unsigned lab_request_deadline(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Record a deadline before the host aborts fetch.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token to validate.
 * @return True if live work with a deadline was marked expired; false otherwise.
 */
bool lab_request_timeout(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Distinguish a request's own timeout from cancellation.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token to validate.
 * @return True if this live owner expired; false otherwise.
 */
bool lab_request_timed_out(lab_request_channel_t channel, uint32_t token);

/**
 * @brief Classify an owned response without interpreting mathematical text.
 * @param channel Request channel index in 0..9, as returned by lab_request_channel.
 * @param token Token to validate.
 * @param http_ok True if the HTTP response succeeded.
 * @param native_ok True if the native operation succeeded.
 * @param has_result True if the required result is present.
 * @param partial True if ordinary expression evaluation reported a partial result.
 * @return Zero for stale work, one for failure, two for success or three for a partial expression result.
 */
lab_request_outcome_t lab_request_accept(lab_request_channel_t channel, uint32_t token, bool http_ok,
                                        bool native_ok, bool has_result, bool partial);

#endif
