/**
 * @file lab_evaluate_render_jobs.c
 * @brief Bounded concurrent supervision of independent TeX card renderings.
 *
 * Inputs are copied before dispatch. Workers own no JSON objects and perform no
 * mathematical evaluation: they supervise isolated latex/dvisvgm processes.
 * The request thread joins the short-lived pool, then publishes SVGs and errors
 * in deterministic card order. No pool survives a request or a server fork.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "executor-t.h"
#include "lab_process_request.h"
#define MARS_LAB_EVALUATE_INTERNAL_ACCESS
#include "lab_evaluate_internal.h"

typedef struct {
    string_t *TeX;
    lab_proc_cancel_context_t cancellation;
} lab_eval_render_job_t;

static void lab_eval_svg_free(void *result)
{
    string_free(result);
}

static bool lab_eval_render_job(void *argument, void **result, diagnostic_t **diagnostic)
{
    lab_eval_render_job_t *job = argument;
    lab_proc_set_cancel_check(job->cancellation.check, job->cancellation.argument);
    *result = lab_eval_render(job->TeX, diagnostic);
    int saved = errno;
    lab_proc_set_cancel_check(NULL, NULL);
    errno = saved;
    return *result != NULL;
}

/* Copy inputs before starting workers and never mutate response fields concurrently. */
void lab_eval_render_fields(json_t *fields, const lab_eval_render_spec_t *specs, size_t count)
{
    if (!fields || !specs || !count || count > 3) {
        errno = EINVAL;
        return;
    }
    lab_eval_render_job_t jobs[3] = {0};
    executor_task_t *tasks[3] = {0};
    size_t present = 0;
    lab_proc_cancel_context_t cancellation = lab_proc_cancel_context();
    for (size_t i = 0; i < count; ++i) {
        const string_t *TeX = json_string_value(lab_eval_get(fields, specs[i].source));
        if (TeX && string_byte_length(TeX) && strcmp(string_c_str(TeX), "(null)")) {
            jobs[i].TeX = string_clone(TeX);
            jobs[i].cancellation = cancellation;
            ++present;
        }
    }
    /* A single card needs no scheduling. Allocation failures preserve the existing synchronous path. */
    executor_t *pool = present > 1 ? executor_new(2, 3) : NULL;
    executor_task_options_t options = {.result_free = lab_eval_svg_free};
    for (size_t i = 0; pool && i < count; ++i) {
        if (jobs[i].TeX)
            tasks[i] = executor_submit(pool, lab_eval_render_job, &jobs[i], &options);
    }
    /* No context can outlive this frame; joining does not allocate or call mathematical code. */
    bool joined = executor_free(pool);
    if (!joined)
        abort();
    for (size_t i = 0; i < count; ++i) {
        if (!tasks[i]) {
            lab_eval_render_field(fields, specs[i].source, specs[i].destination, specs[i].error_key);
        } else {
            void *svg = NULL;
            if (executor_task_result(tasks[i], &svg)) {
                lab_eval_set(fields, specs[i].destination, string_c_str(svg));
            } else {
                diagnostic_t *error = executor_task_diagnostic(tasks[i]);
                string_t *description = error ? diagnostic_to_string(error) : NULL;
                lab_eval_set(fields, specs[i].error_key, description ? string_c_str(description) :
                                                        error ? diagnostic_message(error) : "TeX rendering failed");
                string_free(description);
                diagnostic_free(error);
            }
            executor_task_free(tasks[i]);
        }
        string_free(jobs[i].TeX);
    }
}
