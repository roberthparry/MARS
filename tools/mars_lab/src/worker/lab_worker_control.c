/**
 * @file lab_worker_control.c
 * @brief Per-invocation native computation scope and signal-safe cancellation bridge.
 *
 * The parent sends SIGTERM to request cooperative stopping. The signal handler
 * only sets a flag; ordinary calculation checkpoints translate it into control
 * state. This executable-local adapter restores signal dispositions and reports
 * reserved exit codes, never sharing a heap pointer with the parent process.
 */
#include <signal.h>
#include <stdint.h>
#include <stdio.h>

#include "string-t.h"
#include "computation-t.h"
#include "lab_worker.h"

static volatile sig_atomic_t lab_worker_cancelled;

typedef struct {
    int argc;
    char **argv;
    int result;
} lab_worker_call_t;

static void lab_worker_stop(int signal_number)
{
    (void)signal_number;
    lab_worker_cancelled = 1;
}

static bool lab_worker_cancel_poll(void *argument)
{
    (void)argument;
    return lab_worker_cancelled != 0;
}

static bool lab_worker_unsigned(const char *text, uint64_t *value)
{
    string_t *source = string_new_with(text);
    string_view_t view = string_view_all(source);
    bool ok = source && string_view_length(view) > 0;
    uint64_t number = 0;
    for (size_t i = 0; ok && i < string_view_length(view); ++i) {
        unsigned char c;
        ok = string_view_peek_ascii(view, i, &c) && c >= '0' && c <= '9' &&
             number <= (UINT64_MAX - (uint64_t)(c - '0')) / 10;
        if (ok)
            number = number * 10 + (uint64_t)(c - '0');
    }
    string_free(source);
    if (ok)
        *value = number;
    return ok;
}

static bool lab_worker_control_run(void *argument)
{
    lab_worker_call_t *call = argument;
    call->result = lab_worker_dispatch(call->argc, call->argv);
    return call->result == 0;
}

/* Enter once, before listener startup, and preserve the first observed stopping reason. */
int lab_worker_dispatch_controlled(int argc, char **argv)
{
    computation_options_t options = {.cancelled = lab_worker_cancel_poll};
    if (argc < 3 || !argv || !lab_worker_unsigned(argv[0], &options.timeout_ms) ||
        !lab_worker_unsigned(argv[1], &options.work_limit))
        return LAB_WORKER_CONTROL_FAILED;
    struct sigaction action = {.sa_handler = lab_worker_stop}, previous;
    sigemptyset(&action.sa_mask);
    lab_worker_cancelled = 0;
    if (sigaction(SIGTERM, &action, &previous) != 0)
        return LAB_WORKER_CONTROL_FAILED;
    computation_t *control = computation_new(&options);
    lab_worker_call_t call = {.argc = argc - 2, .argv = argv + 2, .result = LAB_WORKER_CONTROL_FAILED};
    bool ran = control && computation_run(control, lab_worker_control_run, &call);
    if (ran)
        computation_complete(control);
    computation_status_t status = computation_status(control);
    computation_free(control);
    sigaction(SIGTERM, &previous, NULL);
    static const int codes[] = {
        [COMPUTATION_ACTIVE]           = 0,
        [COMPUTATION_COMPLETED]        = 0,
        [COMPUTATION_CANCELLED]        = LAB_WORKER_CANCELLED,
        [COMPUTATION_TIMED_OUT]        = LAB_WORKER_TIMED_OUT,
        [COMPUTATION_BUDGET_EXHAUSTED]  = LAB_WORKER_BUDGET_EXHAUSTED,
        [COMPUTATION_FAILED]           = LAB_WORKER_CONTROL_FAILED,
    };
    int stopped = status == COMPUTATION_INVALID ? LAB_WORKER_CONTROL_FAILED : codes[status];
    if (stopped)
        fputs("error Calculation stopped by its computation control\n", stderr);
    return stopped ? stopped : call.result;
}
