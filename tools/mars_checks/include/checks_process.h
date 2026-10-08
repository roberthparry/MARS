/**
 * @file checks_process.h
 * @brief Shell-free subprocesses and compiler argument parsing for check tools.
 *
 * Argument strings follow POSIX shlex quoting without expansion or execution.
 * Children receive independent output files, optional working directories and a
 * monotonic deadline. Captured text belongs to the caller. This Linux tools API
 * is synchronous and intended for sequential repository checks. Owned process
 * groups are terminated on completion and failure, preventing retained background
 * writers from outliving a README programme or compiler invocation.
 */
#ifndef MARS_CHECKS_PROCESS_H
#define MARS_CHECKS_PROCESS_H

#include "checks_support.h"

/**
 * @brief Install cooperative SIGINT and SIGTERM handlers for one sequential command.
 * @return True on success; false with errno set on installation failure or an already active scope.
 * @details Call outside subprocess execution and pair with checks_process_interrupt_end.
 * Handlers only record the first signal; ordinary cleanup performs process termination.
 */
bool checks_process_interrupt_begin(void);

/**
 * @brief Restore the signal dispositions saved by checks_process_interrupt_begin.
 * @return No value; an inactive scope is ignored. The recorded signal is cleared.
 */
void checks_process_interrupt_end(void);

/**
 * @brief Read the interruption recorded by the active command scope.
 * @return SIGINT or SIGTERM when interrupted, otherwise zero.
 */
int checks_process_interrupt_signal(void);

/**
 * @brief Append POSIX-shlex words to an owned collection; false means malformed quoting.
 * @param text Borrowed UTF-8 input string.
 * @param words Borrowed collection receiving owned argument strings.
 * @return True when the condition or operation described above succeeds; false otherwise.
 */
bool checks_shell_words(const string_t *text, checks_strings_t *words);

/**
 * @brief Run argv without a shell; timeout zero means unlimited and signals produce negative status values.
 * @param arguments Borrowed argument collection; each element is one literal argument.
 * @param cwd Borrowed child working directory, or NULL to inherit the current directory.
 * @param timeout Execution deadline in seconds; zero disables the deadline.
 * @param output Required destination for owned standard output; release with string_free.
 * @param errors Required destination for owned standard error; release with string_free.
 * @param status Required destination for the child exit code, or negative terminating signal.
 * @return True after collection and reaping, including non-zero child exit codes;
 * false on infrastructure failure, with errno set (ETIMEDOUT for expiry or ECANCELED for interruption).
 * @details Sends SIGKILL to the owned process group on normal completion, timeout
 * and wait errors, then reaps the direct child before collecting output. The
 * child's ordinary exit status is retained when it has already exited. Descendants
 * which leave the group cannot be terminated by this interface. Callers must not
 * reap these children elsewhere or ignore SIGCHLD. Adopted descendants, if the
 * caller is a subreaper, remain the caller's responsibility to reap.
 */
bool checks_process_run(const checks_strings_t *arguments, const string_t *cwd, double timeout, string_t **output,
                        string_t **errors, int *status);

/**
 * @brief Read sorted NUL-separated Git index paths; return NULL on a Git inspection failure.
 * @param root Borrowed repository root path.
 * @param staged Whether to inspect only staged added, copied, modified and renamed paths.
 * @return Owned result; release with the corresponding free function. NULL denotes an inspection error where
 * documented.
 */
checks_strings_t *checks_git_paths(const string_t *root, bool staged);

#endif
