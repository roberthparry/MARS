/**
 * @file lab_process.h
 * @brief Private Linux child-process execution for native MARS Lab tools.
 *
 * Runs argument vectors synchronously, collecting merged standard output and
 * standard error in caller-owned string_t storage. Deadlines and byte limits
 * bound collection, and failed runs terminate their process group and reap the
 * direct child. This is a tools-only interface, not part of the MARS library API.
 * Worker paths share build-directory defaults and per-worker environment overrides.
 * Calls have independent state; callers must not reap these children elsewhere
 * or configure SIGCHLD with SIG_IGN or SA_NOCLDWAIT.
 */
#ifndef MARS_LAB_PROCESS_H
#define MARS_LAB_PROCESS_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct _string_t string_t;

/**
 * @brief Resolves a known Lab worker basename into a caller-owned executable path.
 * @param name One of mars_lab, equation_lab, diffequation_lab, matrix_lab,
 * integrator_lab, datetime_lab, almanac_lab, almanac_event_lab, holiday_lab or ophelia.
 * @return Owned path released with string_free; NULL with EINVAL for an unknown or
 * NULL name, or ENOMEM on allocation failure. Does not test existence or execute it.
 * @details A non-empty per-worker environment override takes precedence. mars_lab
 * uses MARS_LAB_BINARY; the others use MARS_LAB_EQUATION_BINARY,
 * MARS_LAB_DIFFEQUATION_BINARY, MARS_LAB_MATRIX_BINARY, MARS_LAB_INTEGRATOR_BINARY,
 * MARS_LAB_DATETIME_BINARY, MARS_LAB_ALMANAC_BINARY, MARS_LAB_ALMANAC_EVENT_BINARY,
 * MARS_LAB_HOLIDAY_BINARY and MARS_LAB_OPHELIA_BINARY respectively. Empty overrides
 * select the default MARS_LAB_WORKER_DIR/name; the compile-time directory falls
 * back to tools/mars_lab/build/release/workers. Paths are not shell-expanded or whitespace-trimmed.
 * Relative paths are interpreted in the spawned child's working directory.
 */
string_t *lab_proc_worker_path(const char *name);

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
 * are copied through file.h into a private temporary file before spawning. The file
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
