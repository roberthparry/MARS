/**
 * @file test_harness_adapter.h
 * @brief Optional configuration and reporting services for the independent C test harness.
 *
 * Implementations supply ordinary C values and callbacks. The core knows no JSON, database or mathematical
 * representation. All callbacks execute sequentially on the main thread. Text passed to callbacks is owned by
 * the core and valid only during the call; adapters must copy anything retained. Ordinary suites need only
 * test_harness.h; this interface is for service implementations and independent integration fixtures.
 */
#ifndef TEST_HARNESS_ADAPTER_H
#define TEST_HARNESS_ADAPTER_H

#include "test_harness.h"

/** @brief Read-only completed-case view; every string remains core-owned for the duration of the callback. */
typedef struct _test_harness_record_t {
    const char *source; /**< Source-file identity. */
    const char *path; /**< Complete dotted case path. */
    const char *name; /**< Local case name. */
    const char *parent; /**< Parent path, or an empty string. */
    const char *tags; /**< Comma-separated tags. */
    const char *detail; /**< Failure or skip description, possibly empty. */
    const char *outcome; /**< Stable pass, fail or skip outcome. */
    const char *finished_utc; /**< UTC completion time, or an empty string if unavailable. */
    int declaration_line; /**< Registration line. */
    bool enabled; /**< Whether the case was selected. */
    bool is_group; /**< Whether this record summarises nested cases. */
    bool is_output; /**< Whether this is an output example. */
    double elapsed_ms; /**< Monotonic elapsed milliseconds. */
} test_harness_record_t;

/**
 * @brief Optional process-lifetime service table; NULL callbacks use the core's documented defaults.
 * @details The adapter owns its implementation state. release must close all resources acquired by start or
 * reporting, including partially initialised state after an argument error. A table and its callbacks remain
 * valid until process exit. The core never takes ownership of the table.
 */
typedef struct _test_harness_adapter_t {
    /**
     * @brief Parse process arguments and initialise optional selection state.
     * @param[in] argc Number of command-line arguments.
     * @param[in] argv Caller-owned argument vector, valid until process exit.
     * @param[in] mode Requested configuration scope.
     * @param[out] plan_only Required output set true for a planning-only invocation.
     * @return Zero continues execution, one means help completed, and -1 reports invalid arguments or initialisation failure.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    int (*start)(int argc, char **argv, test_config_mode_t mode, bool *plan_only);
    /**
     * @brief Count selected leaf cases without running them or creating storage.
     * @param[in] source Caller-owned primary suite source identity.
     * @param[out] count Required output receiving a complete selected count, including zero.
     * @return True for a reliable count; false when discovery or an error prevents planning.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    bool (*plan)(const char *source, size_t *count);
    /**
     * @brief Resolve optional configuration selection or key presence.
     * @param[in] file Caller-owned source identity.
     * @param[in] name Caller-owned local case name.
     * @param[in] parent Optional caller-owned parent path.
     * @param[in] key_only True asks whether a key exists; false asks whether the case is enabled.
     * @return The requested selection or presence flag. Missing selections normally default to enabled.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    bool (*query)(const char *file, const char *name, const char *parent, bool key_only);
    /**
     * @brief Apply a profile override to an existing selection.
     * @param[in] file Caller-owned source identity.
     * @param[in] path Caller-owned complete dotted case path.
     * @param[in] fallback Selection already resolved from configuration.
     * @return Effective enabled state; return fallback when there is no override.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    bool (*selected)(const char *file, const char *path, bool fallback);
    /**
     * @brief Complete configuration discovery and release its in-memory snapshot.
     * @param[in] aborted True suppresses pruning because registration may be incomplete.
     * @return No value; report persistence failures through diagnostics while retaining existing saved selections.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    void (*finish_selection)(bool aborted);
    /**
     * @brief Choose whether missing configuration keys should be reported.
     * @return True permits the core's console report; false suppresses it, for example when a profile controls selection.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    bool (*report_missing)(void);
    /**
     * @brief Open optional reporting storage after case execution.
     * @param[in] executable Caller-owned executable identity, valid during this call.
     * @return True permits record callbacks; false skips reporting. release must clean up partially opened resources.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    bool (*report_begin)(const char *executable);
    /**
     * @brief Record one completed case without retaining core-owned pointers.
     * @param[in] record Core-owned read-only view, valid only until this callback returns; copy retained fields.
     * @return No value; remember storage failures and report them when closing the transaction.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    void (*report_record)(const test_harness_record_t *record);
    /**
     * @brief Finish the reporting transaction and close storage.
     * @param[in] exit_code Actual executable outcome.
     * @param[in] elapsed_ms Total non-negative monotonic elapsed milliseconds.
     * @return No value; close or roll back even after a failed record and keep console results available.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    void (*report_finish)(int exit_code, double elapsed_ms);
    /**
     * @brief Release all adapter-owned resources after execution or early exit.
     * @return No value. Called once, including after help, planning or failed initialisation; accept partial state.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    void (*release)(void);
    /**
     * @brief Snapshot module case data and run synchronous checking callbacks.
     * @param[in] file Caller-owned registration source identity.
     * @param[in] line Registration source line.
     * @param[in] module Caller-owned module-table registry key.
     * @param[in] parent Optional caller-owned containing case path.
     * @param[in] check Caller-owned synchronous checking callback; its opaque row is valid only during the call.
     * @return No value. Record load failures or empty enabled tables through the core, and release all snapshot rows afterwards.
     * @note Sequential calls only. Inputs remain caller-owned unless the callback explicitly copies them.
     */
    void (*run_cases)(const char *file, int line, const char *module, const char *parent,
                      void (*check)(const void *data));
} test_harness_adapter_t;

/**
 * @brief Supply optional services to the harness main function.
 * @return A caller-owned immutable process-lifetime table, or NULL for the independent default.
 * @details An adapter supplies one strong definition; the core's weak default returns NULL. The hook runs
 * after test_program_setup and before argument parsing. It must not depend on suite-owned fixtures.
 */
const test_harness_adapter_t *test_harness_adapter(void);

#endif
