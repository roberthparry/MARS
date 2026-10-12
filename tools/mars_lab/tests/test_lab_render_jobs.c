/**
 * @file test_lab_render_jobs.c
 * @brief Executor-backed SVG batches, retained renderer failures and cancellation propagation.
 *
 * Calls the controlled evaluator façade with independent card inputs. Real TeX
 * success tests need latex/dvisvgm; cancellation and rejection cases do not.
 * Every batch joins before fixtures, request observers or JSON fields are released.
 */
#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include "lab_process_request.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static const lab_eval_render_spec_t render_cards[] = {
    {"first", "first_svg", "first_error"},
    {"second", "second_svg", "second_error"},
    {"third", "third_svg", "third_error"},
};

static void test_lab_render_batch(void)
{
    json_t *fields = json_new_object();
    TEST_ASSERT_NOT_NULL(fields);
    lab_eval_set(fields, "first", "x^2");
    lab_eval_set(fields, "second", "\\input{/etc/passwd}");
    lab_eval_set(fields, "third", "\\frac{1}{2}");
    lab_eval_render_fields(fields, render_cards, 3);
    bool ok = strstr(lab_eval_text(fields, "first_svg"), "<svg") &&
              strstr(lab_eval_text(fields, "third_svg"), "<svg") &&
              strcmp(lab_eval_text(fields, "first_svg"), lab_eval_text(fields, "third_svg")) &&
              strstr(lab_eval_text(fields, "second_error"), "unsupported") &&
              !*lab_eval_text(fields, "second_svg") && !*lab_eval_text(fields, "first_error") &&
              !strcmp(lab_eval_text(fields, "first"), "x^2");
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "independent SVG results and errors publish to their own cards without modifying inputs");
}

static bool render_cancelled(void *argument)
{
    atomic_uint *calls = argument;
    atomic_fetch_add(calls, 1);
    return true;
}

static void test_lab_render_batch_cancellation(void)
{
    json_t *fields = json_new_object();
    TEST_ASSERT_NOT_NULL(fields);
    lab_eval_set(fields, "first", "x");
    lab_eval_set(fields, "second", "y");
    atomic_uint calls = 0;
    lab_proc_set_cancel_check(render_cancelled, &calls);
    lab_eval_render_fields(fields, render_cards, 2);
    lab_proc_cancel_context_t restored = lab_proc_cancel_context();
    lab_proc_set_cancel_check(NULL, NULL);
    bool ok = atomic_load(&calls) >= 2 && restored.check == render_cancelled && restored.argument == &calls &&
              !*lab_eval_text(fields, "first_svg") && !*lab_eval_text(fields, "second_svg") &&
              *lab_eval_text(fields, "first_error") && *lab_eval_text(fields, "second_error");
    json_free(fields);
    fields = json_new_object();
    TEST_ASSERT_NOT_NULL(fields);
    lab_eval_set(fields, "first", "x");
    lab_eval_render_fields(fields, render_cards, 1);
    ok = strstr(lab_eval_text(fields, "first_svg"), "<svg") && ok;
    json_free(fields);
    TEST_ASSERT_TRUE(ok, "request cancellation reaches pool threads without leaking their TLS into the next batch");
}

static void test_lab_render_diagnostic(void)
{
    string_t *TeX = string_new_with("\\input{/etc/passwd}");
    diagnostic_t *error = NULL;
    string_t *svg = lab_eval_render(TeX, &error);
    bool ok = !svg && errno == EINVAL && error && diagnostic_code(error) == EINVAL &&
              !strcmp(diagnostic_domain(error), "lab.render");
    string_free(svg);
    string_free(TeX);
    diagnostic_free(error);
    TeX = string_new_with("\\frac");
    error = NULL;
    svg = lab_eval_render(TeX, &error);
    ok = !svg && error && diagnostic_code(error) == EIO && diagnostic_cause(error) &&
         !strcmp(diagnostic_domain(diagnostic_cause(error)), "lab.process") && ok;
    string_free(svg);
    string_free(TeX);
    diagnostic_free(error);
    TEST_ASSERT_TRUE(ok, "renderer errors retain machine-readable failure codes and child-output causes");
}

/* Register bounded renderer cases in the sequential native Lab suite. */
void test_lab_render_job_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_render_batch, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_render_batch_cancellation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_render_diagnostic, tests, NULL);
}
