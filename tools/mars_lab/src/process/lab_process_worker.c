/**
 * @file lab_process_worker.c
 * @brief Server-specific command construction for built-in Lab calculations.
 *
 * Resolves the calculation registry and optional diagnostic executable overrides.
 * Built-ins use resident calculation sessions; overrides use the independent process runner.
 * Keeping this adapter in its own translation unit lets other native tools reuse
 * the generic process runner without linking the Lab's calculation entry points.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "mars_paths.h"
#include "lab_worker.h"
#include "lab_process.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "lab_process_plot_cache.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "lab_process_session.h"

/* Resolve an explicit diagnostic override or the server containing the calculation. */
string_t *lab_proc_worker_path(const char *name)
{
    const char *environment = lab_worker_environment(name);
    if (!environment) {
        errno = EINVAL;
        return NULL;
    }
    const char *override = getenv(environment);
    string_t *path = override && *override ? string_new_with(override) : mars_path_resource(MARS_PATH_WORKER, NULL, "");
    if (!path)
        errno = ENOMEM;
    return path;
}

/* Dispatch a built-in calculation without launching another executable. */
bool lab_proc_run_worker(const char *const argv[], const char *cwd, const string_t *input, unsigned timeout_ms,
                         size_t max_output, string_t **output, int *exit_status)
{
    if (output)
        *output = NULL;
    if (exit_status)
        *exit_status = -1;
    const char *environment = argv ? lab_worker_environment(argv[0]) : NULL;
    if (!environment || !output || !exit_status) {
        errno = EINVAL;
        return false;
    }
    size_t count = 0;
    while (count < 256 && argv[count])
        ++count;
    if (count == 256) {
        errno = E2BIG;
        return false;
    }
    string_t *path = lab_proc_worker_path(argv[0]);
    if (!path)
        return false;
    const char *override = getenv(environment);
    bool external = override && *override;
    bool plot = !external && !input && count == 19 && !strcmp(argv[0], "mars_lab") &&
                !strcmp(argv[4], "plot") &&
                (!strcmp(argv[18], "equation") || !strcmp(argv[18], "diffequation"));
    string_t *plot_key = plot ? lab_proc_plot_key(argv, cwd, path, false) : NULL;
    string_t *prepared_key = plot_key ? lab_proc_plot_key(argv, cwd, path, true) : NULL;
    bool equation = !external && !input && !strcmp(argv[0], "equation_lab") &&
                    (count == 3 || (count == 5 && !strcmp(argv[3], "--solution-pages") && !strcmp(argv[4], "0")));
    string_t *equation_key = equation ? lab_proc_plot_key(argv, cwd, path, false) : NULL;
    string_t *prepared = NULL;
    lab_proc_cancel_context_t cancel = lab_proc_cancel_context();
    if (plot_key && !(cancel.check && cancel.check(cancel.argument))) {
        string_t *cached = lab_proc_plot_cache_get(plot_key, false);
        if (cached && string_byte_length(cached) <= max_output) {
            *output = cached;
            *exit_status = 0;
            string_free(prepared_key);
            string_free(equation_key);
            string_free(plot_key);
            string_free(path);
            return true;
        }
        string_free(cached);
        prepared = lab_proc_plot_cache_get(prepared_key, true);
    }
    const char *command[258] = {external ? string_c_str(path) : argv[0]};
    size_t next = 1;
    for (size_t i = 1; i < count; ++i)
        command[next++] = argv[i];
    if (prepared_key || equation_key)
        command[next++] = "--plot-cache";
    bool ran = external ? lab_proc_run_input(command, cwd, input, timeout_ms, max_output, output, exit_status)
                        : lab_proc_persistent_run(command, cwd, prepared ? prepared : input,
                                                  timeout_ms, max_output, output, exit_status);
    int error = errno;
    if (ran && !external && *exit_status >= LAB_WORKER_CANCELLED && *exit_status <= LAB_WORKER_CONTROL_FAILED) {
        static const int errors[] = {ECANCELED, ETIMEDOUT, E2BIG, EIO};
        error = errors[*exit_status - LAB_WORKER_CANCELLED];
        ran = false;
    }
    if (ran && !*exit_status && (plot_key || equation_key) && *output) {
        string_t *record = lab_proc_plot_cache_extract(output);
        string_t *record_key = equation_key ? lab_proc_plot_record_key(record, cwd, path) : NULL;
        if (record)
            lab_proc_plot_cache_put(equation_key ? record_key : prepared_key, record, true);
        if (plot_key)
            lab_proc_plot_cache_put(plot_key, *output, false);
        string_free(record_key);
        string_free(record);
    }
    string_free(prepared);
    string_free(prepared_key);
    string_free(equation_key);
    string_free(plot_key);
    string_free(path);
    errno = error;
    return ran;
}
