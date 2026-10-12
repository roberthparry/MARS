/**
 * @file lab_evaluate_diagnostic.c
 * @brief Explicit structured-error projection for the Lab's Protobuf response objects.
 *
 * Uses JSON objects only as the server's existing in-memory value representation;
 * wire encoding remains Protobuf. Copies metadata and messages, never inspects
 * request contents or mathematical expressions. Ordinary error text is retained
 * separately so diagnostic allocation failure does not mask the original error.
 */
#include <inttypes.h>

#define MARS_LAB_EVALUATE_INTERNAL_ACCESS
#include "lab_evaluate_internal.h"

static json_t *lab_eval_diagnostic_object(const diagnostic_t *error)
{
    json_t *object = json_new_object();
    string_t *code = string_sprintf("%" PRId64, diagnostic_code(error));
    bool ok = object && code && lab_eval_set(object, "domain", diagnostic_domain(error)) &&
              lab_eval_set(object, "message", diagnostic_message(error)) &&
              lab_eval_put(object, "code", json_new_number(code));
    string_free(code);
    if (!ok) {
        json_free(object);
        return NULL;
    }
    return object;
}

/* A flat, bounded cause array fits the wire's depth limit even for a full 64-record chain. */
bool lab_eval_set_diagnostic(json_t *object, const diagnostic_t *error)
{
    if (!object || !error)
        return false;
    json_t *record = lab_eval_diagnostic_object(error);
    json_t *causes = json_new_array();
    bool ok = record && causes;
    for (const diagnostic_t *cause = diagnostic_cause(error); ok && cause; cause = diagnostic_cause(cause)) {
        json_t *entry = lab_eval_diagnostic_object(cause);
        ok = entry && json_array_append(causes, entry);
        json_free(entry);
    }
    if (ok)
        ok = lab_eval_put(record, "causes", causes);
    else
        json_free(causes);
    if (ok)
        return lab_eval_put(object, "failure_diagnostic", record);
    json_free(record);
    return false;
}
