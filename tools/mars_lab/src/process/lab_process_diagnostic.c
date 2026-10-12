/**
 * @file lab_process_diagnostic.c
 * @brief Owned process outcome diagnostics without copying sensitive command data.
 *
 * Converts explicit run results into immutable records for rendering, calculation
 * and calendar adapters. No last-error global or thread-local record is retained.
 * Callers keep normal output and exit-status ownership even if allocation fails.
 */
#include <errno.h>

#include "lab_process.h"

/* Capture failure metadata without changing the process call's errno contract. */
diagnostic_t *lab_proc_diagnostic(bool completed, int process_error, int exit_status, unsigned timeout_ms)
{
    static const char *const messages[] = {
        [EINVAL]     = "Invalid native process request",
        [ENOENT]     = "Native executable or working directory is unavailable",
        [EACCES]     = "Native executable or working directory is not accessible",
        [ECANCELED]  = "Native process cancelled",
        [EFBIG]      = "Native process output limit exceeded",
        [EILSEQ]     = "Native process returned invalid UTF-8",
        [EOVERFLOW]  = "Native process size limit cannot be represented",
        [ENOMEM]     = "Native process allocation failed",
        [E2BIG]      = "Native process argument or work budget exceeded",
        [EIO]        = "Native process control or input/output failed",
    };
    if (completed && !exit_status)
        return NULL;
    int saved = errno;
    diagnostic_t *child = NULL;
    if (exit_status >= 0) {
        string_t *message = string_sprintf("Native worker exited (exit %d)", exit_status);
        child = message ? diagnostic_new(exit_status ? DIAGNOSTIC_ERROR : DIAGNOSTIC_NOTE, "lab.worker", exit_status,
                                           string_c_str(message), NULL, NULL) : NULL;
        string_free(message);
    }
    diagnostic_t *error = child;
    if (!completed) {
        int code = process_error ? process_error : EIO;
        const char *message = (unsigned)code < sizeof(messages) / sizeof(*messages) ? messages[code] : NULL;
        string_t *timeout = code == ETIMEDOUT ? string_sprintf("Native process timed out after %u ms", timeout_ms) : NULL;
        error = diagnostic_new(DIAGNOSTIC_ERROR, "lab.process", code,
                                timeout ? string_c_str(timeout) : message ? message : "Native process could not complete",
                                NULL, child);
        string_free(timeout);
        diagnostic_free(child);
    }
    errno = saved;
    return error;
}
