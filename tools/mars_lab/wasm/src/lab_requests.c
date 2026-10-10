/**
 * @file lab_requests.c
 * @brief Freestanding browser request ownership and operation policy for MARS Lab.
 *
 * Owns monotonically numbered requests, mode invalidation, busy state, dependent
 * weather work, operation routing and response acceptance. JavaScript retains
 * browser fetch, timers and DOM access, but cannot revive an obsolete request.
 * Fixed direct-indexed channels require no allocation or libc. No mathematics
 * is parsed or rewritten here. Calls run synchronously on the browser thread.
 */
#include <stdint.h>

#include "lab_requests.h"

enum { lab_request_modes = 7, lab_request_channels = 9, lab_request_operations = 14 };
enum {
    lab_request_main,
    lab_request_function,
    lab_request_weather,
    lab_request_binding,
    lab_request_land,
    lab_request_datetime_location,
    lab_request_almanac_location,
    lab_request_holidays,
    lab_request_solver
};
enum {
    lab_request_evaluate,
    lab_request_derivative,
    lab_request_integral,
    lab_request_programme,
    lab_request_forecast,
    lab_request_goal,
    lab_request_bindings,
    lab_request_land_totality,
    lab_request_date_location,
    lab_request_sky_location,
    lab_request_binding_commit,
    lab_request_local_holidays,
    lab_request_solver_render,
    lab_request_solver_restore_render
};

typedef struct {
    unsigned modes, channel, needs_text, needs_variable, busy, timeout;
    const char *name;
    unsigned endpoint, action;
} lab_request_policy_t;

static const lab_request_policy_t lab_request_policy[lab_request_operations] = {
    [lab_request_evaluate] = {127, lab_request_main, 1, 0, 1, 0, "evaluate", 0, 0},
    [lab_request_derivative] = {9, lab_request_main, 1, 1, 1, 0, "derivative", 0, 0},
    [lab_request_integral] = {9, lab_request_main, 1, 1, 1, 0, "integral", 0, 1},
    [lab_request_programme] = {127, lab_request_function, 1, 0, 0, 45000, "function", 8, 0},
    [lab_request_forecast] = {32, lab_request_weather, 0, 0, 0, 0, "weather", 9, 0},
    [lab_request_goal] = {1, lab_request_main, 1, 0, 1, 0, "goal", 10, 0},
    [lab_request_bindings] = {1, lab_request_binding, 1, 0, 0, 0, "bindings", 0, 2},
    [lab_request_land_totality] = {64, lab_request_land, 0, 0, 0, 22000, "land", 11, 0},
    [lab_request_date_location] = {32, lab_request_datetime_location, 0, 0, 0, 0, "datetimeLocation", 12, 0},
    [lab_request_sky_location] = {64, lab_request_almanac_location, 0, 0, 0, 0, "almanacLocation", 13, 0},
    [lab_request_binding_commit] = {31, lab_request_binding, 1, 0, 1, 0, "bindingCommit", 0, 2},
    [lab_request_local_holidays] = {32, lab_request_holidays, 0, 0, 0, 0, "holidays", 6, 0},
    [lab_request_solver_render] = {4, lab_request_solver, 1, 0, 0, 0, "solverRender", 14, 0},
    [lab_request_solver_restore_render] = {4, lab_request_solver, 1, 0, 0, 45000, "solverRestoreRender", 14, 0}};

static const char *const lab_request_paths[] = {"",
                                                "/eval",
                                                "/matrix-eval",
                                                "/equation-eval",
                                                "/diffequation-eval",
                                                "/integrator-eval",
                                                "/datetime-eval",
                                                "/almanac-eval",
                                                "/function-run",
                                                "/datetime-weather",
                                                "/goal_seek",
                                                "/almanac-land-totality",
                                                "/datetime-jurisdiction-location",
                                                "/datetime-jurisdiction-location",
                                                "/render_TeX"};
static const char *const lab_request_actions[] = {"", "integral", "bindings"};

/* The host builds its exact-name index once from the native operation catalogue. */
unsigned lab_request_operation_count(void)
{
    return lab_request_operations;
}

/* Borrow immutable ABI labels: kind zero names operations, one endpoints, and two expression actions. */
const char *lab_request_text(unsigned kind, unsigned index)
{
    if (kind == 0 && index < lab_request_operations)
        return lab_request_policy[index].name;
    if (kind == 1 && index < sizeof(lab_request_paths) / sizeof(*lab_request_paths))
        return lab_request_paths[index];
    if (kind == 2 && index < sizeof(lab_request_actions) / sizeof(*lab_request_actions))
        return lab_request_actions[index];
    return "";
}

/* Count only short, trusted static labels; no user-authored text is scanned. */
unsigned lab_request_text_length(unsigned kind, unsigned index)
{
    const char *text = lab_request_text(kind, index);
    unsigned length = 0;
    while (text[length])
        ++length;
    return length;
}

typedef struct {
    uint32_t token, parent;
    unsigned operation, mode, active, expired, scalar;
} lab_request_slot_t;

static lab_request_slot_t lab_request_slots[lab_request_channels];
static uint32_t lab_request_serial;
static unsigned lab_request_mode = lab_request_modes;
static uint32_t lab_request_mode_generation;

/* Invalidate an owned channel without ever reusing its previous request token. */
void lab_request_cancel(unsigned channel)
{
    if (channel < lab_request_channels) {
        lab_request_slots[channel].active = 0;
        lab_request_slots[channel].token = 0;
        if (channel == lab_request_main) {
            lab_request_slots[lab_request_weather].active = 0;
            lab_request_slots[lab_request_weather].token = 0;
            lab_request_slots[lab_request_land].active = 0;
            lab_request_slots[lab_request_land].token = 0;
            lab_request_slots[lab_request_solver].active = 0;
            lab_request_slots[lab_request_solver].token = 0;
        }
    }
}

/* A mode change invalidates all channels, including changes away and back. */
void lab_request_set_mode(unsigned mode)
{
    if (mode == lab_request_mode)
        return;
    lab_request_mode = mode < lab_request_modes ? mode : lab_request_modes;
    ++lab_request_mode_generation;
    for (unsigned i = 0; i < lab_request_channels; ++i)
        lab_request_cancel(i);
}

/* Capture mode identity across asynchronous setup, including a switch away and back. */
uint32_t lab_request_context(void)
{
    return lab_request_mode_generation;
}

/* Return the direct-indexed channel for an operation, or an invalid sentinel. */
unsigned lab_request_channel(unsigned operation)
{
    return operation < lab_request_operations ? lab_request_policy[operation].channel : lab_request_channels;
}

/* Begin validated work; zero means invalid, disallowed, empty or exhausted. */
uint32_t lab_request_begin(unsigned mode, unsigned operation, unsigned has_text, unsigned has_variable, unsigned scalar,
                           uint32_t parent)
{
    if (mode >= lab_request_modes || mode != lab_request_mode || operation >= lab_request_operations ||
        lab_request_serial == UINT32_MAX)
        return 0;
    const lab_request_policy_t *policy = &lab_request_policy[operation];
    if (!(policy->modes & (1u << mode)) || (policy->needs_text && !has_text) ||
        (policy->needs_variable && !has_variable))
        return 0;
    /* Restored cards use a fresh DOM identity, never an expired evaluation parent. */
    if (operation == lab_request_solver_restore_render && parent)
        return 0;
    if ((operation == lab_request_forecast || operation == lab_request_solver_render ||
         (operation == lab_request_land_totality && parent)) &&
        (!parent || parent != lab_request_slots[lab_request_main].token ||
         lab_request_slots[lab_request_main].mode != mode ||
         lab_request_slots[lab_request_main].operation != lab_request_evaluate))
        return 0;
    if (policy->channel == lab_request_main)
        for (unsigned i = 0; i < lab_request_channels; ++i)
            lab_request_cancel(i);
    lab_request_slots[policy->channel] =
        (lab_request_slot_t){++lab_request_serial, parent, operation, mode, 1, 0, !!scalar};
    return lab_request_serial;
}

/* Return whether a token still owns active work in the selected mode. */
unsigned lab_request_live(unsigned channel, uint32_t token)
{
    if (channel >= lab_request_channels || !token)
        return 0;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    return slot->active && slot->token == token && slot->mode == lab_request_mode &&
           (!slot->parent || slot->parent == lab_request_slots[lab_request_main].token);
}

/* Validate editor presence after any awaited binding commit, without interpreting its algebra. */
unsigned lab_request_input(unsigned channel, uint32_t token, unsigned has_text, unsigned has_variable)
{
    if (!lab_request_live(channel, token))
        return 0;
    const lab_request_policy_t *policy = &lab_request_policy[lab_request_slots[channel].operation];
    return (!policy->needs_text || has_text) && (!policy->needs_variable || has_variable);
}

/* Complete only the matching request; an old finaliser cannot clear newer busy state. */
unsigned lab_request_finish(unsigned channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return 0;
    lab_request_slots[channel].active = 0;
    return 1;
}

/* Expose native busy state; background weather and programme execution do not block the editor. */
unsigned lab_request_busy(void)
{
    const lab_request_slot_t *slot = &lab_request_slots[lab_request_main];
    const lab_request_slot_t *binding = &lab_request_slots[lab_request_binding];
    return (lab_request_live(lab_request_main, slot->token) && lab_request_policy[slot->operation].busy) ||
           (lab_request_live(lab_request_binding, binding->token) && lab_request_policy[binding->operation].busy);
}

/* Borrow the current main token for dependent weather or land-totality work, including after completion. */
uint32_t lab_request_latest_main(void)
{
    return lab_request_slots[lab_request_main].mode == lab_request_mode ? lab_request_slots[lab_request_main].token : 0;
}

/* Select the endpoint: eval, matrix, equation, differential equation, integrator, date, almanac, run, weather, goal. */
unsigned lab_request_endpoint(unsigned channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return 0;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    static const unsigned evaluation_endpoints[lab_request_modes] = {1, 3, 4, 2, 5, 6, 7};
    if (slot->operation == lab_request_evaluate)
        return evaluation_endpoints[slot->mode];
    unsigned fixed = lab_request_policy[slot->operation].endpoint;
    if (fixed)
        return fixed;
    return slot->mode == 3 && !slot->scalar ? 2 : 1;
}

/* Preserve expression persistence and transient matrix calculus independently of the current DOM. */
unsigned lab_request_flags(unsigned channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return 0;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    return (slot->mode == 0 ? 1u : 0u) |
           (slot->mode == 3 && (slot->operation == lab_request_derivative || slot->operation == lab_request_integral)
                ? 2u
                : 0u);
}

/* Select the native expression action: ordinary/derivative, integral or binding refresh. */
unsigned lab_request_action(unsigned channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return 0;
    return lab_request_policy[lab_request_slots[channel].operation].action;
}

/* Return the operation's timeout in milliseconds, with zero denoting no client deadline. */
unsigned lab_request_deadline(unsigned channel, uint32_t token)
{
    return lab_request_live(channel, token) ? lab_request_policy[lab_request_slots[channel].operation].timeout : 0;
}

/* Record a deadline before the host aborts fetch, retaining ownership of the timeout diagnostic. */
unsigned lab_request_timeout(unsigned channel, uint32_t token)
{
    if (!lab_request_live(channel, token) || !lab_request_deadline(channel, token))
        return 0;
    lab_request_slots[channel].expired = 1;
    return 1;
}

/* Distinguish an owned deadline from explicit cancellation and stale rejection. */
unsigned lab_request_timed_out(unsigned channel, uint32_t token)
{
    return lab_request_live(channel, token) && lab_request_slots[channel].expired;
}

/* Classify a response: zero stale, one failure, two success, three partial expression result. */
unsigned lab_request_accept(unsigned channel, uint32_t token, unsigned http_ok, unsigned native_ok, unsigned has_result,
                            unsigned partial)
{
    if (!lab_request_live(channel, token))
        return 0;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    unsigned calculus = slot->operation == lab_request_derivative || slot->operation == lab_request_integral;
    if (slot->expired || !http_ok || !native_ok ||
        (calculus && lab_request_endpoint(channel, token) == 1 && !has_result))
        return 1;
    return slot->operation == lab_request_evaluate && slot->mode == 0 && partial ? 3 : 2;
}
