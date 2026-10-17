/**
 * @file tba_process.h
 * @brief Bounded child-process execution for private Lab native facilities.
 *
 * Runs argument vectors without a shell, with inherited environment and closed
 * standard input redirected to /dev/null. Captured UTF-8 output belongs to the caller. Deadlines, byte
 * limits and a borrowed cancellation flag bound execution. Callers must not reap
 * these children elsewhere or configure SIGCHLD with SIG_IGN or SA_NOCLDWAIT.
 */
#ifndef TBA_PROCESS_H
#define TBA_PROCESS_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct _string_t string_t;

/**
 * @brief Registers a borrowed, process-wide cancellation flag, or NULL to disable cancellation.
 * Install outside active calls and retain the flag until all calls finish. A signal
 * handler may set the flag, but must not call this setter or a run function. A non-zero
 * flag makes runs fail with ECANCELED and terminate and reap any spawned child.
 * The worker's SIGTERM handler must set this flag and allow the run to unwind before
 * exiting; default signal termination and SIGKILL cannot perform this cleanup.
 * @param flag Borrowed signal-safe cancellation flag, or NULL to disable it.
 */
void tba_proc_set_cancel_flag(const volatile sig_atomic_t *flag);

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
bool tba_proc_run(const char *const argv[], const char *cwd, unsigned timeout_ms, size_t max_output,
                  string_t **output, int *exit_status);

#endif
