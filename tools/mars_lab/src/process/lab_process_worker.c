/**
 * @file lab_process_worker.c
 * @brief Server-specific command construction for built-in Lab calculations.
 *
 * Resolves the calculation registry and optional diagnostic executable overrides,
 * then delegates bounded child execution to the independent lab_process.c core.
 * Keeping this adapter in its own translation unit lets other native tools reuse
 * the generic process runner without linking the Lab's calculation entry points.
 */
#include <errno.h>
#include <stdlib.h>

#include "ustring.h"
#include "lab_worker.h"
#include "lab_process.h"

#ifndef MARS_LAB_SERVER_PATH
#define MARS_LAB_SERVER_PATH "tools/mars_lab/build/release/mars_lab"
#endif

/* Resolve an explicit diagnostic override or the server containing the calculation. */
string_t *lab_proc_worker_path(const char *name)
{
    const char *environment = lab_worker_environment(name);
    if (!environment) {
        errno = EINVAL;
        return NULL;
    }
    const char *override = getenv(environment);
    string_t *path = string_new_with(override && *override ? override : MARS_LAB_SERVER_PATH);
    if (!path)
        errno = ENOMEM;
    return path;
}

/* Wrap a calculation invocation in the server's built-in worker command. */
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
    const char *command[259] = {string_c_str(path)};
    size_t next = 1;
    if (!external) {
        command[next++] = "--worker";
        command[next++] = argv[0];
    }
    for (size_t i = 1; i < count; ++i)
        command[next++] = argv[i];
    bool ran = lab_proc_run_input(command, cwd, input, timeout_ms, max_output, output, exit_status);
    int error = errno;
    string_free(path);
    errno = error;
    return ran;
}
