/**
 * @file lab_evaluate.h
 * @brief Native mathematical request adapter for MARS Lab.
 *
 * Dispatches mathematical JSON requests to prebuilt Lab workers and maps their
 * labelled output to the Lab UI. Algebra and binding values remain native worker
 * results. The caller owns responses and HTTP/state persistence. Call from the
 * repository root so relative worker paths can be located. The process module
 * resolves the configured build directory and CLI/environment worker overrides.
 * No worker is built implicitly, and no shell or Python interpreter is used.
 */
#ifndef LAB_EVALUATE_H
#define LAB_EVALUATE_H

#include "json-t.h"
#include "string-t.h"

/**
 * @brief Evaluate a Lab mathematical route and return an owned JSON response.
 *
 * Accepts /eval, /equation-eval, /diffequation-eval, /matrix-eval,
 * /integrator-eval, /function-run, /goal_seek and /render_TeX. Inputs are
 * bounded to 64 KiB per string, precision to 17–78914 decimal digits (10000
 * for Ophelia, which has that native limit), and
 * worker output to 4 MiB. Workers have individual bounded execution times.
 * Unknown routes return NULL without changing status, permitting dispatch to
 * other adapters. Recognised invalid requests return 400; worker failures return
 * 422. Optional rendering failures do not discard mathematical results.
 * Failure responses additionally carry failure_diagnostic: an outcome domain,
 * numeric code, message and flat ordered causes array. Request codes are HTTP
 * statuses, process codes are errno values, and worker codes are exit statuses.
 * Existing error text and timeout fields remain available even if creating the
 * optional diagnostic fails. These objects are transported as Protobuf, not JSON text.
 * State is never saved here. Free the result with json_free(). NULL with status set to
 * 500 indicates allocation failure for a recognised route.
 * @param route Borrowed exact browser route, including its leading slash.
 * @param payload Borrowed JSON request object.
 * @param status Optional output for the HTTP response status.
 * @return Owned JSON response released with json_free, or NULL as described above.
 */
json_t *lab_eval_request(const string_t *route, const json_t *payload, unsigned *status);

#endif
