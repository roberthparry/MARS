/**
 * @file lab_process.h
 * @brief Private Linux child-process execution for native MARS Lab tools.
 *
 * Runs argument vectors synchronously, collecting merged standard output and
 * standard error in caller-owned string_t storage. Deadlines and byte limits
 * bound collection, and failed runs terminate their process group and reap the
 * direct child. This is a tools-only interface, not part of the MARS library API.
 * Calculations use bounded resident processes and reusable module caches, with optional diagnostic executable overrides.
 * Calls have independent state; callers must not reap these children elsewhere
 * or configure SIGCHLD with SIG_IGN or SA_NOCLDWAIT.
 */
#ifndef MARS_LAB_PROCESS_H
#define MARS_LAB_PROCESS_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>

#include "diagnostic-t.h"
#include "lab_process_request.h"

typedef struct _string_t string_t;

/**
 * @brief Start this HTTP process's bounded persistent calculation service before starting threads.
 * @param memory_limit Maximum calculation address-space bytes; zero chooses one thirty-second of physical RAM,
 * clamped to 128..512 MiB. Existing operating-system limits may be stricter.
 * @param max_jobs Maximum completed calculations before recycling a worker; zero selects 256.
 * @return True when ready, including an already started service; false sets errno on invalid limits or startup
 * failure. One single-threaded supervisor owns one calculation process. Requests are sequential per HTTP process.
 * @note Call during single-threaded startup. Forked request fixtures receive independent services on first use.
 * No ownership escapes; stop with lab_proc_workers_stop() before this owning process exits.
 */
bool lab_proc_workers_start(size_t memory_limit, unsigned max_jobs);

/**
 * @brief Stop this process's calculation service and reap its supervisor.
 * @return No value; repeated calls and inherited sessions from another PID are harmless. Complete the current
 * request first. Calculation children terminate with their supervisor and never keep listener descriptors open.
 */
void lab_proc_workers_stop(void);

/**
 * @brief Obtain the automatic per-process address-space ceiling used for native calculations and HTTP workers.
 * @return Bytes, between 128 and 512 MiB; no allocation or ownership transfer occurs. Uses physical RAM size,
 * falling back to 256 MiB if the operating system cannot report it. This is independent of temporary free memory.
 */
size_t lab_proc_memory_budget(void);

/**
 * @brief Apply an address-space ceiling to the current process without relaxing an existing hard limit.
 * @param bytes Desired non-zero ceiling in bytes.
 * @return True on success, false with errno from getrlimit/setrlimit. Call during single-threaded startup;
 * it affects subsequent allocations and mappings, not existing ownership. No resources need releasing.
 */
bool lab_proc_memory_limit(size_t bytes);

/** @brief Initialise bounded anonymous plot caches before forking request workers; failure disables caching. */
void lab_proc_plot_cache_init(void);

/**
 * @brief Snapshot a completed process call using its saved errno and exit status.
 * Pass the boolean return, saved errno, returned status and supplied timeout immediately after a run.
 * Success with exit zero returns NULL; otherwise returns an owned diagnostic released with diagnostic_free().
 * The lab.process domain uses errno codes, lab.worker uses exit statuses. Collection failures retain an
 * available child status as a cause. No arguments, input, environment or captured output are copied.
 * errno is preserved, including on diagnostic allocation failure; legacy status/output remain authoritative.
 */
diagnostic_t *lab_proc_diagnostic(bool completed, int process_error, int exit_status, unsigned timeout_ms);

/**
 * @brief Resolves a known calculation mode to the server or an explicit diagnostic override.
 * @param name Borrowed built-in mode name; see lab_worker.h.
 * @return Owned executable path released with string_free; NULL with EINVAL for
 * an unknown name, or ENOMEM on allocation failure.
 * @details By default every mode resolves to the configured lab_worker resource.
 * A non-empty per-mode MARS_LAB_*_BINARY override
 * selects an external diagnostic executable. Use lab_proc_run_worker to supply
 * the necessary dispatch arguments; this path alone is not a command. Normal built-in calls do not execute this path.
 */
string_t *lab_proc_worker_path(const char *name);

/**
 * @brief Run a built-in calculation in this HTTP process's resident calculation service.
 * @param argv Borrowed NULL-terminated vector: mode name followed by its arguments.
 * At most 255 entries including the mode name are accepted; excess gives E2BIG.
 * @param cwd Optional child working directory; NULL inherits the current directory.
 * @param input Optional borrowed standard-input contents, at most 8 MiB; NULL selects empty input.
 * @param timeout_ms Execution deadline in milliseconds; zero disables the deadline.
 * @param max_output Maximum captured output bytes, at most 64 MiB for built-ins.
 * @param output Required owned output destination, initialised to NULL; release with string_free.
 * @param exit_status Required child exit status destination, initialised to -1.
 * @return Collection success as for lab_proc_run_input; an unknown mode gives EINVAL.
 * Built-in control stops return false with ECANCELED, ETIMEDOUT, E2BIG or EIO.
 * @details Uses the same cancellation, descriptor, output-limit and process-group
 * cleanup as lab_proc_run_input. Explicit per-mode executable overrides receive
 * the original calculation arguments without the server's --worker-control prefix.
 * Built-ins receive a fresh computation scope and at most 250 ms of cooperative cleanup.
 * Successful jobs retain the worker and its module caches, resetting numeric precision, standard input,
 * environment and working directory between jobs. The service must be started before creating threads.
 * A failed supervisor makes later calls fail explicitly until single-threaded service reinitialisation;
 * worker failures are recovered inside the supervisor. Concurrent calls within one HTTP process give EBUSY.
 */
bool lab_proc_run_worker(const char *const argv[], const char *cwd, const string_t *input, unsigned timeout_ms,
                         size_t max_output, string_t **output, int *exit_status);

/**
 * @brief Registers a borrowed, process-wide cancellation flag, or NULL to disable cancellation.
 * Install outside active calls and retain the flag until all calls finish. A signal
 * handler may set the flag, but must not call this setter or a run function. A non-zero
 * flag makes runs fail with ECANCELED and terminate and reap any spawned child.
 * The worker's SIGTERM handler must set this flag and allow the run to unwind before
 * exiting; default signal termination and SIGKILL cannot perform this cleanup.
 * @param flag Borrowed signal-safe cancellation flag, or NULL to disable it.
 */
void lab_proc_set_cancel_flag(const volatile sig_atomic_t *flag);

/**
 * @brief Run a cooperative worker, allowing at most 250 ms to unwind after cancellation or timeout.
 * The parent sends SIGTERM, then retains SIGKILL and reaping as final safeguards.
 * Other arguments and errors match lab_proc_run_input(); output-limit failures still kill immediately.
 */
bool lab_proc_run_input_cooperative(const char *const argv[], const char *cwd, const string_t *input,
                                   unsigned timeout_ms, size_t max_output, string_t **output, int *exit_status);

/**
 * @brief Executes a programme without a shell and collects its merged output.
 * @param argv Borrowed NULL-terminated argument vector; argv[0] must be non-empty.
 * Executable names without a slash are searched using PATH.
 * @param cwd Optional child working directory; NULL inherits the current directory.
 * @param timeout_ms Monotonic execution and collection deadline in milliseconds; zero disables it.
 * @param max_output Maximum captured byte count; zero permits only empty output.
 * @param output Required destination, initialised to NULL. Receives an owned string,
 * including partial output on failure once storage exists; release with string_free.
 * Existing storage at this address is not freed. Valid UTF-8, including embedded
 * NUL bytes, is preserved verbatim without normalisation. Invalid UTF-8 is rejected.
 * @param exit_status Required destination, initialised to -1. Receives the child's
 * exit code or 128 plus its terminating signal whenever a child has been reaped.
 * @return True after collection and reaping, even for a non-zero child exit code.
 * False sets errno: EINVAL for invalid arguments, ETIMEDOUT for expiry, EFBIG for
 * excessive output, EILSEQ for invalid or incomplete UTF-8, ENOMEM for allocation
 * failure, ECANCELED for cancellation, or the system/spawn error.
 * @details The environment is inherited; standard input reads from /dev/null. Other descriptors
 * are closed in the child. On collection failure, SIGKILL is sent to the process
 * group and direct child before reaping. Descendants which leave the group cannot
 * be terminated by this interface. Successful collection waits for pipe EOF as
 * well as child termination, so descendants retaining the pipe count towards the
 * deadline. Storage is O(max_output), with a fixed-size read buffer and string
 * allocation overhead; unrepresentable string capacities give EOVERFLOW.
 */
bool lab_proc_run(const char *const argv[], const char *cwd, unsigned timeout_ms, size_t max_output, string_t **output,
                  int *exit_status);

/**
 * @brief Runs a programme with optional string contents as standard input.
 * @param argv Borrowed NULL-terminated argument vector, as for lab_proc_run.
 * @param cwd Optional child working directory, as for lab_proc_run.
 * @param input Borrowed input string; NULL selects /dev/null. Bytes, including NUL,
 * are copied through the file module into a private temporary file before spawning. The file
 * is unlinked once the child has opened it. No input/output pipe deadlock is possible.
 * @param timeout_ms Monotonic deadline including input preparation; zero disables it.
 * @param max_output Maximum captured output bytes, as for lab_proc_run.
 * @param output Required destination for caller-owned complete or partial output.
 * @param exit_status Required conventional exit status destination.
 * @return Success and errno rules match lab_proc_run; file preparation can also fail.
 * @details Input uses O(1) additional memory and disk space proportional to its byte
 * length. Cancellation and deadlines are checked between bounded writes; individual
 * filesystem and spawn operations cannot be interrupted by this interface.
 */
bool lab_proc_run_input(const char *const argv[], const char *cwd, const string_t *input, unsigned timeout_ms,
                        size_t max_output, string_t **output, int *exit_status);

#endif
