/**
 * @file lab_evaluate.c
 * @brief Validate and dispatch native MARS Lab mathematical requests.
 *
 * This server adapter supplies bounded argv vectors to prebuilt Lab workers.
 * It does not interpret mathematical syntax, rewrite results, build executables,
 * or persist editor state. The process module resolves build-directory defaults
 * and CLI/environment worker overrides. Relative paths require the repository
 * root as the server working directory. Field adaptation and TeX rendering are
 * separate private implementation units. Integrator deadlines include independent
 * base, work-budget and decimal-precision allowances, bounded at two minutes.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "lab_evaluate_internal.h"
#include "lab_process.h"

typedef struct {
    const char *route;
    const char *input;
    const char *worker;
    const char *mode;
    unsigned timeout_ms;
} route_t;

/* Sorted for binary lookup; the route table is immutable across requests. */
static const route_t routes[] = {
    {"/diffequation-eval", "diffequation", "diffequation_lab", "diffequation", 10000u},
    {"/equation-eval", "equation", "equation_lab", "equation", 40000u},
    {"/eval", "expression", "mars_lab", "expression", 30000u},
    {"/function-run", "source", "ophelia", "function", 30000u},
    {"/goal_seek", "expression", "mars_lab", "goal_seek", 10000u},
    {"/integrator-eval", "expression", "integrator_lab", "integrator", 10000u},
    {"/matrix-eval", "matrix", "matrix_lab", "matrix", 10000u},
    {"/render_TeX", "tex", NULL, "render", 10000u},
};

typedef struct {
    const char *argv[256];
    string_t *owned[256];
    size_t count;
    size_t bytes;
} arguments_t;

static int lab_eval_compare_route(const void *key, const void *entry)
{
    return strcmp(key, ((const route_t *)entry)->route);
}

static bool lab_eval_valid_text(const string_t *text, bool required)
{
    if (!text || string_byte_length(text) > 65536u)
        return false;
    string_view_t view = string_view_all(text);
    for (size_t i = 0u; i < string_byte_length(text); ++i) {
        unsigned char byte = 0u;
        if (string_view_peek_ascii(view, i, &byte) && byte == 0u)
            return false;
    }
    return !required || !string_view_is_empty(string_view_trim(string_view_all(text)));
}

static bool lab_eval_argument(arguments_t *args, const char *text)
{
    if (args->count >= 255u || args->bytes + strlen(text) + 1u > 262144u)
        return false;
    string_t *copy = string_new_with(text);
    if (!copy)
        return false;
    args->owned[args->count] = copy;
    args->argv[args->count++] = string_c_str(copy);
    args->bytes += string_byte_length(copy) + 1u;
    return true;
}

static bool lab_eval_argument_member(arguments_t *args, const json_t *payload, const char *key, const char *fallback,
                                     bool required)
{
    const json_t *member = lab_eval_get(payload, key);
    if (!member)
        return lab_eval_argument(args, fallback);
    const string_t *text = json_string_value(member);
    return lab_eval_valid_text(text, required) && lab_eval_argument(args, string_c_str(text));
}

static bool lab_eval_unsigned_member(const json_t *payload, const char *key, unsigned fallback, unsigned minimum,
                                     unsigned maximum, unsigned *out)
{
    const json_t *member = lab_eval_get(payload, key);
    if (!member) {
        *out = fallback;
        return true;
    }
    const string_t *text = json_number_text(member);
    if (!text || !string_byte_length(text) || string_byte_length(text) > 9u)
        return false;
    unsigned value = 0u;
    string_view_t digits = string_view_all(text);
    for (size_t i = 0u; i < string_byte_length(text); ++i) {
        unsigned char digit = 0u;
        if (!string_view_peek_ascii(digits, i, &digit) || digit < '0' || digit > '9')
            return false;
        value = value * 10u + (unsigned)(digit - '0');
    }
    if (value < minimum || value > maximum)
        return false;
    *out = value;
    return true;
}

static json_t *lab_eval_failure(unsigned *status, unsigned code, const char *message)
{
    if (status)
        *status = code;
    json_t *result = json_new_object();
    if (!result || !lab_eval_put(result, "ok", json_new_bool(false)) || !lab_eval_set(result, "error", message)) {
        json_free(result);
        return NULL;
    }
    return result;
}

/* Keep deadline arithmetic bounded even for inputs beyond HTTP validation limits. */
unsigned lab_eval_integrator_timeout(unsigned precision, unsigned cap)
{
    unsigned blocks = precision / 96u + (precision % 96u != 0u);
    unsigned work_seconds = cap / 500u;
    if (blocks >= 10u || work_seconds >= 90u)
        return 120000u;
    unsigned seconds = 30u + work_seconds + (blocks ? blocks - 1u : 0u) * 10u;
    return (seconds < 120u ? seconds : 120u) * 1000u;
}

/* Preserve a process deadline failure separately from unavailable workers and output limits. */
json_t *lab_eval_worker_failure(unsigned *status, int process_error, unsigned timeout_ms)
{
    if (process_error != ETIMEDOUT)
        return lab_eval_failure(status, 422u, "Native worker could not complete (unavailable or output limit)");
    string_t *message = string_sprintf("Native worker timed out after %u ms (ETIMEDOUT)", timeout_ms);
    string_t *deadline = string_sprintf("%u", timeout_ms);
    json_t *result = message ? lab_eval_failure(status, 422u, string_c_str(message)) : NULL;
    if (result && (!deadline || !lab_eval_set(result, "error_code", "ETIMEDOUT") ||
                   !lab_eval_put(result, "timeout_ms", json_new_number(deadline)))) {
        json_free(result);
        result = NULL;
    }
    string_free(message);
    string_free(deadline);
    return result;
}

static bool lab_eval_expression_arguments(arguments_t *args, const json_t *payload, const char *precision)
{
    const char *action = lab_eval_text(payload, "action");
    const char *wrt = lab_eval_text(payload, "wrt");
    if (lab_eval_get(payload, "action") &&
        !lab_eval_valid_text(json_string_value(lab_eval_get(payload, "action")), false))
        return false;
    if (!*action)
        action = *wrt ? "derivative" : "evaluate";
    if (strcmp(action, "evaluate") && strcmp(action, "bindings") && strcmp(action, "binding-edit") &&
        strcmp(action, "derivative") && strcmp(action, "integral"))
        return false;
    if (!lab_eval_argument_member(args, payload, "wrt", "x", false) || !lab_eval_argument(args, precision) ||
        !lab_eval_argument(args, action))
        return false;
    /* Empty UI differentiation names have the same default as omitted names. */
    if (!*args->argv[2]) {
        if (args->bytes >= 262144u || string_append_cstr(args->owned[2], "x"))
            return false;
        ++args->bytes;
        args->argv[2] = string_c_str(args->owned[2]);
    }
    if (!strcmp(action, "bindings"))
        return lab_eval_argument_member(args, payload, "binding_source", "", false);
    if (!strcmp(action, "binding-edit"))
        return lab_eval_argument_member(args, payload, "binding_value", "", false);
    return true;
}

static int lab_eval_compare_word(const void *key, const void *entry)
{
    return strcmp(key, *(const char *const *)entry);
}

static bool lab_eval_matrix_arguments(arguments_t *args, const json_t *payload, const char *precision)
{
    static const char *const operations[] = {
        "charpoly", "det",  "eigendecompose", "eigenvalues", "eval",  "inverse",
        "multiply", "rank", "simplify",       "solve",       "trace",
    };
    const json_t *member = lab_eval_get(payload, "operation");
    if (member && !lab_eval_valid_text(json_string_value(member), false))
        return false;
    const char *operation = lab_eval_text(payload, "operation");
    if (!*operation)
        operation = "eval";
    if (!bsearch(operation, operations, sizeof(operations) / sizeof(*operations), sizeof(*operations),
                 lab_eval_compare_word))
        return false;
    return lab_eval_argument(args, operation) && lab_eval_argument(args, precision) &&
           lab_eval_argument_member(args, payload, "operand", "", false);
}

static bool lab_eval_goal_arguments(arguments_t *args, const json_t *payload, const char *precision)
{
    if (!lab_eval_argument_member(args, payload, "target", "0", true) || !lab_eval_argument(args, precision))
        return false;
    const json_t *start = lab_eval_get(payload, "start");
    if (!start)
        return true;
    if (json_type(start) != JSON_OBJECT || json_object_size(start) > 64u)
        return false;
    for (size_t i = 0u; i < json_object_size(start); ++i) {
        const string_t *name = json_object_key_at(start, i);
        const json_t *member = json_object_value_at(start, i);
        const string_t *value = json_type(member) == JSON_NUMBER ? json_number_text(member) : json_string_value(member);
        if (!lab_eval_valid_text(name, true) || string_find(name, "=") >= 0 || !lab_eval_valid_text(value, true))
            return false;
        string_t *assignment = string_sprintf("%s=%s", string_c_str(name), string_c_str(value));
        bool ok = assignment && lab_eval_argument(args, string_c_str(assignment));
        string_free(assignment);
        if (!ok)
            return false;
    }
    return true;
}

static bool lab_eval_integrator_bounds(arguments_t *args, const json_t *payload)
{
    const json_t *bounds = lab_eval_get(payload, "bounds");
    if (bounds && json_type(bounds) != JSON_ARRAY)
        return false;
    size_t count = bounds ? json_array_size(bounds) : 0u;
    if (!count)
        return lab_eval_argument(args, "x") && lab_eval_argument(args, "") && lab_eval_argument(args, "");
    if (count > 32u)
        return false;
    for (size_t i = 0u; i < count; ++i) {
        const json_t *bound = json_array_get(bounds, i);
        if (json_type(bound) != JSON_OBJECT || !lab_eval_get(bound, "name"))
            return false;
        if (*lab_eval_text(bound, "lo") && !*lab_eval_text(bound, "hi"))
            return false;
        if (!lab_eval_argument_member(args, bound, "name", "", true) ||
            !lab_eval_argument_member(args, bound, "lo", "", false) ||
            !lab_eval_argument_member(args, bound, "hi", "", false))
            return false;
    }
    return true;
}

static void lab_eval_integrator_bindings(json_t *fields, const char *precision)
{
    const char *expression = lab_eval_text(fields, "binding_expression");
    if (!*expression)
        return;
    string_t *worker = lab_proc_worker_path("mars_lab");
    if (!worker) {
        lab_eval_set(fields, "binding_error", "Could not resolve the native binding worker path.");
        return;
    }
    const char *argv[] = {string_c_str(worker), expression, "x", precision, "bindings", NULL};
    string_t *output = NULL;
    int exit_status = -1;
    if (lab_proc_run(argv, ".", 10000u, 4u * 1024u * 1024u, &output, &exit_status) && !exit_status) {
        json_t *bindings = lab_eval_fields(output);
        lab_eval_set(fields, "bindings", lab_eval_text(bindings, "bindings"));
        json_free(bindings);
    } else {
        lab_eval_set(fields, "binding_error", "The native binding worker could not enumerate integrand bindings.");
    }
    string_free(output);
    string_free(worker);
}

static json_t *lab_eval_function_run(const string_t *source, unsigned precision, unsigned *status)
{
    string_t *digits = string_sprintf("%u", precision);
    if (!digits)
        return NULL;
    string_t *worker = lab_proc_worker_path("ophelia");
    if (!worker) {
        string_free(digits);
        return lab_eval_failure(status, 422u, "Could not resolve the Ophelia worker path");
    }
    const char *argv[] = {string_c_str(worker), string_c_str(digits), NULL};
    string_t *output = NULL;
    int exit_status = -1;
    bool completed = lab_proc_run_input(argv, ".", source, 30000u, 4u * 1024u * 1024u, &output, &exit_status);
    json_t *result = json_new_object();
    bool ok = completed && exit_status == 0;
    lab_eval_put(result, "ok", json_new_bool(ok));
    string_t *display = ok && output ? lab_eval_display_bindings(output) : NULL;
    lab_eval_set(result, "output", display ? string_c_str(display) : output ? string_c_str(output) : "");
    string_free(display);
    lab_eval_set(result, "error",
                 ok           ? ""
                 : !completed ? "Ophelia could not complete (unavailable, timeout or output limit)"
                 : output     ? string_c_str(output)
                              : "Ophelia failed");
    if (status)
        *status = ok ? 200u : 422u;
    string_free(output);
    string_free(digits);
    string_free(worker);
    return result;
}

/* Dispatch a mathematical request without saving state or changing native algebra. */
json_t *lab_eval_request(const string_t *route, const json_t *payload, unsigned *status)
{
    if (!lab_eval_valid_text(route, true))
        return NULL;
    const route_t *entry =
        bsearch(string_c_str(route), routes, sizeof(routes) / sizeof(*routes), sizeof(*routes), lab_eval_compare_route);
    if (!entry)
        return NULL;
    if (status)
        *status = 500u;
    if (!payload || json_type(payload) != JSON_OBJECT)
        return lab_eval_failure(status, 400u, "Expected a JSON request object");
    const string_t *input = json_string_value(lab_eval_get(payload, entry->input));
    if (!lab_eval_valid_text(input, true))
        return lab_eval_failure(status, 400u, "Input must be a non-empty string of at most 64 KiB without NUL bytes");
    if (!entry->worker) {
        string_t *error = NULL;
        string_t *svg = lab_eval_render(input, &error);
        json_t *result =
            svg ? json_new_object() : lab_eval_failure(status, 422u, error ? string_c_str(error) : "Rendering failed");
        if (svg) {
            lab_eval_put(result, "ok", json_new_bool(true));
            lab_eval_set(result, "svg", string_c_str(svg));
            if (status)
                *status = 200u;
        }
        string_free(svg);
        string_free(error);
        return result;
    }
    unsigned precision = 96u;
    bool function = !strcmp(entry->mode, "function");
    if (!lab_eval_unsigned_member(payload, "precision", !strcmp(entry->mode, "function") ? 50u : 96u, 17u,
                                  function ? 10000u : 315653u, &precision))
        return lab_eval_failure(status, 400u,
                                function ? "Function precision must be an integer from 17 to 10000 decimal digits"
                                         : "Precision must be an integer from 17 to 315653 decimal digits");
    if (function)
        return lab_eval_function_run(input, precision, status);

    arguments_t args = {0};
    string_t *worker = lab_proc_worker_path(entry->worker);
    if (!worker)
        return lab_eval_failure(status, 422u, "Could not resolve the native worker path");
    string_t *digits = string_sprintf("%u", precision);
    unsigned timeout_ms = entry->timeout_ms;
    bool valid = worker && digits && lab_eval_argument(&args, string_c_str(worker));
    if (valid && !strcmp(entry->mode, "goal_seek"))
        valid = lab_eval_argument(&args, "--goal-seek");
    if (valid && !strcmp(entry->mode, "integrator")) {
        unsigned cap = 5000u;
        valid = lab_eval_unsigned_member(payload, "max_intervals", 5000u, 500u, 100000u, &cap);
        string_t *cap_text = string_sprintf("%u", cap);
        valid = valid && cap_text && lab_eval_argument(&args, "--max-intervals") &&
                lab_eval_argument(&args, string_c_str(cap_text));
        string_free(cap_text);
        timeout_ms = lab_eval_integrator_timeout(precision, cap);
    }
    /* Prevent worker option parsers from interpreting an expression as a mode. */
    valid = valid && !string_starts_with(input, "--") && lab_eval_argument(&args, string_c_str(input));
    if (valid && !strcmp(entry->mode, "expression"))
        valid = lab_eval_expression_arguments(&args, payload, string_c_str(digits));
    else if (valid && !strcmp(entry->mode, "matrix"))
        valid = lab_eval_matrix_arguments(&args, payload, string_c_str(digits));
    else if (valid && !strcmp(entry->mode, "goal_seek"))
        valid = lab_eval_goal_arguments(&args, payload, string_c_str(digits));
    else if (valid && !strcmp(entry->mode, "integrator"))
        valid = lab_eval_argument(&args, string_c_str(digits)) && lab_eval_integrator_bounds(&args, payload);
    else if (valid && !strcmp(entry->mode, "equation"))
        valid = lab_eval_argument(&args, string_c_str(digits));

    json_t *result = NULL;
    string_t *output = NULL;
    int exit_status = -1;
    if (!valid) {
        result = lab_eval_failure(status, 400u, "Invalid operation, binding, bounds or argument size");
    } else if (!lab_proc_run(args.argv, ".", timeout_ms, 4u * 1024u * 1024u, &output, &exit_status)) {
        int process_error = errno;
        result = lab_eval_worker_failure(status, process_error, timeout_ms);
        if (output)
            lab_eval_set(result, "raw", string_c_str(output));
    } else {
        result = lab_eval_fields(output);
        if (result) {
            if (!exit_status && !*lab_eval_text(result, "input")) {
                json_free(result);
                result = lab_eval_failure(status, 502u, "Native worker returned no labelled result");
            } else {
                if (!strcmp(entry->mode, "integrator"))
                    lab_eval_integrator_bindings(result, string_c_str(digits));
                lab_eval_adapt(result, entry->mode);
                if (!strcmp(entry->mode, "expression")) {
                    if (!strcmp(lab_eval_text(payload, "action"), "binding-edit"))
                        lab_eval_set(result, "editor_expression", lab_eval_text(result, "expression"));
                    else
                        lab_eval_set(result, "editor_expression", string_c_str(input));
                } else if (!strcmp(entry->mode, "goal_seek")) {
                    lab_eval_set(result, "editor_expression", lab_eval_text(result, "expression"));
                }
                lab_eval_put(result, "ok", json_new_bool(exit_status == 0));
                if (status)
                    *status = exit_status ? 422u : 200u;
                if (exit_status) {
                    const char *raw = output ? string_c_str(output) : "Native evaluation failed";
                    lab_eval_set(result, "error", raw);
                    lab_eval_set(result, "raw", raw);
                    lab_eval_set(result, "raw_error", raw);
                    string_t *code = string_sprintf("%d", exit_status);
                    lab_eval_put(result, "returncode", json_new_number(code));
                    string_free(code);
                }
                if (!strcmp(entry->mode, "goal_seek"))
                    lab_eval_put(result, "precision", json_new_number(digits));
            }
        }
    }
    string_free(output);
    for (size_t i = 0u; i < args.count; ++i)
        string_free(args.owned[i]);
    string_free(worker);
    string_free(digits);
    return result;
}
