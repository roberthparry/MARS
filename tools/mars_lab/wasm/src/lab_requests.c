/**
 * @file lab_requests.c
 * @brief Freestanding browser request ownership and operation policy for MARS Lab.
 *
 * Owns monotonically numbered requests, mode invalidation, busy state, dependent
 * weather work, operation routing and response acceptance. JavaScript retains
 * browser fetch, timers and DOM access, but cannot revive an obsolete request.
 * Fixed direct-indexed channels require no allocation or host system imports. No mathematics
 * is parsed or rewritten here. Calls run synchronously on the browser thread.
 */
#include <stdint.h>
#include <string.h>

#include "lab_mode.h"
#include "lab_requests.h"

typedef struct {
    unsigned modes;
    unsigned needs_text, needs_variable, busy, timeout;
} lab_request_policy_t;

/* Admission metadata is separate from routing so each direct-indexed entry stays readable. */
static const lab_request_policy_t lab_request_policy[LAB_REQUEST_OPERATIONS] = {
    [LAB_REQUEST_EVALUATE]              = {127, 1, 0, 1, 0},
    [LAB_REQUEST_DERIVATIVE]            = {9, 1, 1, 1, 0},
    [LAB_REQUEST_INTEGRAL]              = {9, 1, 1, 1, 0},
    [LAB_REQUEST_PROGRAMME]             = {127, 1, 0, 0, 45000},
    [LAB_REQUEST_FORECAST]              = {32, 0, 0, 0, 0},
    [LAB_REQUEST_GOAL]                  = {1, 1, 0, 1, 0},
    [LAB_REQUEST_BINDINGS]              = {1, 1, 0, 0, 0},
    [LAB_REQUEST_LAND_TOTALITY]         = {64, 0, 0, 0, 22000},
    [LAB_REQUEST_DATE_LOCATION]         = {32, 0, 0, 0, 0},
    [LAB_REQUEST_SKY_LOCATION]          = {64, 0, 0, 0, 0},
    [LAB_REQUEST_BINDING_COMMIT]        = {31, 1, 0, 1, 0},
    [LAB_REQUEST_LOCAL_HOLIDAYS]        = {32, 0, 0, 0, 0},
    [LAB_REQUEST_SOLVER_RENDER]         = {4, 1, 0, 0, 0},
    [LAB_REQUEST_SOLVER_RESTORE_RENDER] = {4, 1, 0, 0, 45000},
    [LAB_REQUEST_PLOT]                  = {23, 1, 0, 0, 0},
};

static const lab_request_channel_t lab_request_operation_channels[LAB_REQUEST_OPERATIONS] = {
    [LAB_REQUEST_EVALUATE]              = LAB_REQUEST_MAIN,
    [LAB_REQUEST_DERIVATIVE]            = LAB_REQUEST_MAIN,
    [LAB_REQUEST_INTEGRAL]              = LAB_REQUEST_MAIN,
    [LAB_REQUEST_PROGRAMME]             = LAB_REQUEST_FUNCTION,
    [LAB_REQUEST_FORECAST]              = LAB_REQUEST_WEATHER,
    [LAB_REQUEST_GOAL]                  = LAB_REQUEST_MAIN,
    [LAB_REQUEST_BINDINGS]              = LAB_REQUEST_BINDING,
    [LAB_REQUEST_LAND_TOTALITY]         = LAB_REQUEST_LAND,
    [LAB_REQUEST_DATE_LOCATION]         = LAB_REQUEST_DATETIME_LOCATION,
    [LAB_REQUEST_SKY_LOCATION]          = LAB_REQUEST_ALMANAC_LOCATION,
    [LAB_REQUEST_BINDING_COMMIT]        = LAB_REQUEST_BINDING,
    [LAB_REQUEST_LOCAL_HOLIDAYS]        = LAB_REQUEST_HOLIDAYS,
    [LAB_REQUEST_SOLVER_RENDER]         = LAB_REQUEST_SOLVER,
    [LAB_REQUEST_SOLVER_RESTORE_RENDER] = LAB_REQUEST_SOLVER,
    [LAB_REQUEST_PLOT]                  = LAB_REQUEST_PLOT_CHANNEL,
};

static const lab_request_endpoint_t lab_request_operation_endpoints[LAB_REQUEST_OPERATIONS] = {
    [LAB_REQUEST_EVALUATE]              = LAB_REQUEST_ENDPOINT_NONE,
    [LAB_REQUEST_DERIVATIVE]            = LAB_REQUEST_ENDPOINT_NONE,
    [LAB_REQUEST_INTEGRAL]              = LAB_REQUEST_ENDPOINT_NONE,
    [LAB_REQUEST_PROGRAMME]             = LAB_REQUEST_ENDPOINT_FUNCTION,
    [LAB_REQUEST_FORECAST]              = LAB_REQUEST_ENDPOINT_WEATHER,
    [LAB_REQUEST_GOAL]                  = LAB_REQUEST_ENDPOINT_GOAL,
    [LAB_REQUEST_BINDINGS]              = LAB_REQUEST_ENDPOINT_NONE,
    [LAB_REQUEST_LAND_TOTALITY]         = LAB_REQUEST_ENDPOINT_LAND,
    [LAB_REQUEST_DATE_LOCATION]         = LAB_REQUEST_ENDPOINT_DATETIME_LOCATION,
    [LAB_REQUEST_SKY_LOCATION]          = LAB_REQUEST_ENDPOINT_ALMANAC_LOCATION,
    [LAB_REQUEST_BINDING_COMMIT]        = LAB_REQUEST_ENDPOINT_NONE,
    [LAB_REQUEST_LOCAL_HOLIDAYS]        = LAB_REQUEST_ENDPOINT_DATETIME,
    [LAB_REQUEST_SOLVER_RENDER]         = LAB_REQUEST_ENDPOINT_RENDER,
    [LAB_REQUEST_SOLVER_RESTORE_RENDER] = LAB_REQUEST_ENDPOINT_RENDER,
    [LAB_REQUEST_PLOT]                  = LAB_REQUEST_ENDPOINT_EXPRESSION,
};

static const lab_request_action_t lab_request_operation_actions[LAB_REQUEST_OPERATIONS] = {
    [LAB_REQUEST_EVALUATE]              = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_DERIVATIVE]            = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_INTEGRAL]              = LAB_REQUEST_ACTION_INTEGRAL,
    [LAB_REQUEST_PROGRAMME]             = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_FORECAST]              = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_GOAL]                  = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_BINDINGS]              = LAB_REQUEST_ACTION_BINDINGS,
    [LAB_REQUEST_LAND_TOTALITY]         = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_DATE_LOCATION]         = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_SKY_LOCATION]          = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_BINDING_COMMIT]        = LAB_REQUEST_ACTION_BINDINGS,
    [LAB_REQUEST_LOCAL_HOLIDAYS]        = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_SOLVER_RENDER]         = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_SOLVER_RESTORE_RENDER] = LAB_REQUEST_ACTION_ORDINARY,
    [LAB_REQUEST_PLOT]                  = LAB_REQUEST_ACTION_PLOT,
};

static const char *const lab_request_names[LAB_REQUEST_OPERATIONS] = {
    [LAB_REQUEST_EVALUATE]              = "evaluate",
    [LAB_REQUEST_DERIVATIVE]            = "derivative",
    [LAB_REQUEST_INTEGRAL]              = "integral",
    [LAB_REQUEST_PROGRAMME]             = "function",
    [LAB_REQUEST_FORECAST]              = "weather",
    [LAB_REQUEST_GOAL]                  = "goal",
    [LAB_REQUEST_BINDINGS]              = "bindings",
    [LAB_REQUEST_LAND_TOTALITY]         = "land",
    [LAB_REQUEST_DATE_LOCATION]         = "datetimeLocation",
    [LAB_REQUEST_SKY_LOCATION]          = "almanacLocation",
    [LAB_REQUEST_BINDING_COMMIT]        = "bindingCommit",
    [LAB_REQUEST_LOCAL_HOLIDAYS]        = "holidays",
    [LAB_REQUEST_SOLVER_RENDER]         = "solverRender",
    [LAB_REQUEST_SOLVER_RESTORE_RENDER] = "solverRestoreRender",
    [LAB_REQUEST_PLOT]                  = "plot",
};

static const char *const lab_request_paths[] = {
    [LAB_REQUEST_ENDPOINT_NONE]                  = "",
    [LAB_REQUEST_ENDPOINT_EXPRESSION]            = "/eval",
    [LAB_REQUEST_ENDPOINT_MATRIX]                = "/matrix-eval",
    [LAB_REQUEST_ENDPOINT_EQUATION]              = "/equation-eval",
    [LAB_REQUEST_ENDPOINT_DIFFERENTIAL_EQUATION] = "/diffequation-eval",
    [LAB_REQUEST_ENDPOINT_INTEGRATOR]            = "/integrator-eval",
    [LAB_REQUEST_ENDPOINT_DATETIME]              = "/datetime-eval",
    [LAB_REQUEST_ENDPOINT_ALMANAC]               = "/almanac-eval",
    [LAB_REQUEST_ENDPOINT_FUNCTION]              = "/function-run",
    [LAB_REQUEST_ENDPOINT_WEATHER]               = "/datetime-weather",
    [LAB_REQUEST_ENDPOINT_GOAL]                  = "/goal_seek",
    [LAB_REQUEST_ENDPOINT_LAND]                  = "/almanac-land-totality",
    [LAB_REQUEST_ENDPOINT_DATETIME_LOCATION]     = "/datetime-jurisdiction-location",
    [LAB_REQUEST_ENDPOINT_ALMANAC_LOCATION]      = "/datetime-jurisdiction-location",
    [LAB_REQUEST_ENDPOINT_RENDER]                = "/render_TeX",
};

static const char *const lab_request_actions[] = {
    [LAB_REQUEST_ACTION_ORDINARY] = "",
    [LAB_REQUEST_ACTION_INTEGRAL] = "integral",
    [LAB_REQUEST_ACTION_BINDINGS] = "bindings",
    [LAB_REQUEST_ACTION_PLOT]     = "plot",
};

/* The host builds its exact-name index once from the native operation catalogue. */
unsigned lab_request_operation_count(void)
{
    return LAB_REQUEST_OPERATIONS;
}

/* Borrow an operation name, endpoint path or expression action from the selected catalogue. */
const char *lab_request_text(lab_request_catalogue_t kind, unsigned index)
{
    static const struct {
        const char *const *labels;
        unsigned count;
    } catalogues[] = {
        [LAB_REQUEST_CATALOGUE_OPERATIONS] = {lab_request_names, sizeof lab_request_names / sizeof *lab_request_names},
        [LAB_REQUEST_CATALOGUE_ENDPOINTS]  = {lab_request_paths, sizeof lab_request_paths / sizeof *lab_request_paths},
        [LAB_REQUEST_CATALOGUE_ACTIONS]    = {lab_request_actions, sizeof lab_request_actions / sizeof *lab_request_actions},
    };
    if ((unsigned)kind >= sizeof catalogues / sizeof *catalogues || index >= catalogues[kind].count)
        return "";
    return catalogues[kind].labels[index];
}

/* Count only short, trusted static labels; no user-authored text is scanned. */
unsigned lab_request_text_length(lab_request_catalogue_t kind, unsigned index)
{
    return (unsigned)strlen(lab_request_text(kind, index));
}

typedef struct {
    uint32_t token, parent;
    lab_request_operation_t operation;
    lab_mode_t mode;
    unsigned active, expired, scalar;
} lab_request_slot_t;

static lab_request_slot_t lab_request_slots[LAB_REQUEST_CHANNELS];
static uint32_t lab_request_serial;
static lab_mode_t lab_request_mode = LAB_MODE_INVALID;
static uint32_t lab_request_mode_generation;

/* Invalidate an owned channel without ever reusing its previous request token. */
void lab_request_cancel(lab_request_channel_t channel)
{
    if ((unsigned)channel < LAB_REQUEST_CHANNELS) {
        lab_request_slots[channel].active = 0;
        lab_request_slots[channel].token = 0;
        if (channel == LAB_REQUEST_MAIN) {
            lab_request_slots[LAB_REQUEST_WEATHER].active = 0;
            lab_request_slots[LAB_REQUEST_WEATHER].token = 0;
            lab_request_slots[LAB_REQUEST_LAND].active = 0;
            lab_request_slots[LAB_REQUEST_LAND].token = 0;
            lab_request_slots[LAB_REQUEST_SOLVER].active = 0;
            lab_request_slots[LAB_REQUEST_SOLVER].token = 0;
            lab_request_slots[LAB_REQUEST_PLOT_CHANNEL].active = 0;
            lab_request_slots[LAB_REQUEST_PLOT_CHANNEL].token = 0;
        }
    }
}

/* A mode change invalidates all channels, including changes away and back. */
void lab_request_set_mode(lab_mode_t mode)
{
    if (mode == lab_request_mode)
        return;
    lab_request_mode = (unsigned)mode < LAB_MODE_COUNT ? mode : LAB_MODE_INVALID;
    ++lab_request_mode_generation;
    for (lab_request_channel_t i = LAB_REQUEST_MAIN; i < LAB_REQUEST_CHANNELS; ++i)
        lab_request_cancel(i);
}

/* Capture mode identity across asynchronous setup, including a switch away and back. */
uint32_t lab_request_context(void)
{
    return lab_request_mode_generation;
}

/* Return the direct-indexed channel for an operation, or an invalid sentinel. */
lab_request_channel_t lab_request_channel(lab_request_operation_t operation)
{
    return (unsigned)operation < LAB_REQUEST_OPERATIONS
               ? lab_request_operation_channels[operation] : LAB_REQUEST_CHANNELS;
}

/* Begin validated work; zero means invalid, disallowed, empty or exhausted. */
uint32_t lab_request_begin(lab_mode_t mode, lab_request_operation_t operation, bool has_text, bool has_variable,
                           bool scalar, uint32_t parent)
{
    if ((unsigned)mode >= LAB_MODE_COUNT || mode != lab_request_mode || (unsigned)operation >= LAB_REQUEST_OPERATIONS ||
        lab_request_serial == UINT32_MAX)
        return 0;
    const lab_request_policy_t *policy = &lab_request_policy[operation];
    if (!(policy->modes & (1u << mode)) || (policy->needs_text && !has_text) ||
        (policy->needs_variable && !has_variable))
        return 0;
    /* Restored cards use a fresh DOM identity, never an expired evaluation parent. */
    if (operation == LAB_REQUEST_SOLVER_RESTORE_RENDER && parent)
        return 0;
    if ((operation == LAB_REQUEST_FORECAST || operation == LAB_REQUEST_SOLVER_RENDER ||
         (operation == LAB_REQUEST_PLOT && parent) ||
         (operation == LAB_REQUEST_LAND_TOTALITY && parent)) &&
        (!parent || parent != lab_request_slots[LAB_REQUEST_MAIN].token ||
         lab_request_slots[LAB_REQUEST_MAIN].mode != mode ||
         lab_request_slots[LAB_REQUEST_MAIN].operation != LAB_REQUEST_EVALUATE))
        return 0;
    lab_request_channel_t channel = lab_request_operation_channels[operation];
    if (channel == LAB_REQUEST_MAIN)
        for (lab_request_channel_t i = LAB_REQUEST_MAIN; i < LAB_REQUEST_CHANNELS; ++i)
            lab_request_cancel(i);
    lab_request_slots[channel] =
        (lab_request_slot_t){++lab_request_serial, parent, operation, mode, 1, 0, !!scalar};
    return lab_request_serial;
}

/* Return whether a token still owns active work in the selected mode. */
bool lab_request_live(lab_request_channel_t channel, uint32_t token)
{
    if ((unsigned)channel >= LAB_REQUEST_CHANNELS || !token)
        return false;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    return slot->active && slot->token == token && slot->mode == lab_request_mode &&
           (!slot->parent || slot->parent == lab_request_slots[LAB_REQUEST_MAIN].token);
}

/* Validate editor presence after any awaited binding commit, without interpreting its algebra. */
bool lab_request_input(lab_request_channel_t channel, uint32_t token, bool has_text, bool has_variable)
{
    if (!lab_request_live(channel, token))
        return false;
    const lab_request_policy_t *policy = &lab_request_policy[lab_request_slots[channel].operation];
    return (!policy->needs_text || has_text) && (!policy->needs_variable || has_variable);
}

/* Complete only the matching request; an old finaliser cannot clear newer busy state. */
bool lab_request_finish(lab_request_channel_t channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return false;
    lab_request_slots[channel].active = 0;
    return true;
}

/* Expose native busy state; background weather and programme execution do not block the editor. */
bool lab_request_busy(void)
{
    const lab_request_slot_t *slot = &lab_request_slots[LAB_REQUEST_MAIN];
    const lab_request_slot_t *binding = &lab_request_slots[LAB_REQUEST_BINDING];
    return (lab_request_live(LAB_REQUEST_MAIN, slot->token) && lab_request_policy[slot->operation].busy) ||
           (lab_request_live(LAB_REQUEST_BINDING, binding->token) && lab_request_policy[binding->operation].busy);
}

/* Borrow the current main token for dependent weather or land-totality work, including after completion. */
uint32_t lab_request_latest_main(void)
{
    return lab_request_slots[LAB_REQUEST_MAIN].mode == lab_request_mode ? lab_request_slots[LAB_REQUEST_MAIN].token : 0;
}

/* Select the endpoint: eval, matrix, equation, differential equation, integrator, date, almanac, run, weather, goal. */
lab_request_endpoint_t lab_request_endpoint(lab_request_channel_t channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return LAB_REQUEST_ENDPOINT_NONE;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    static const lab_request_endpoint_t evaluation_endpoints[LAB_MODE_COUNT] = {
        LAB_REQUEST_ENDPOINT_EXPRESSION, LAB_REQUEST_ENDPOINT_EQUATION, LAB_REQUEST_ENDPOINT_DIFFERENTIAL_EQUATION,
        LAB_REQUEST_ENDPOINT_MATRIX, LAB_REQUEST_ENDPOINT_INTEGRATOR, LAB_REQUEST_ENDPOINT_DATETIME,
        LAB_REQUEST_ENDPOINT_ALMANAC
    };
    if (slot->operation == LAB_REQUEST_EVALUATE)
        return evaluation_endpoints[slot->mode];
    lab_request_endpoint_t fixed = lab_request_operation_endpoints[slot->operation];
    if (fixed != LAB_REQUEST_ENDPOINT_NONE)
        return fixed;
    return slot->mode == LAB_MODE_MATRIX && !slot->scalar
               ? LAB_REQUEST_ENDPOINT_MATRIX : LAB_REQUEST_ENDPOINT_EXPRESSION;
}

/* Preserve expression persistence and transient matrix calculus independently of the current DOM. */
unsigned lab_request_flags(lab_request_channel_t channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return 0;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    return (slot->mode == LAB_MODE_EXPRESSION ? 1u : 0u) |
           (slot->mode == LAB_MODE_MATRIX &&
            (slot->operation == LAB_REQUEST_DERIVATIVE || slot->operation == LAB_REQUEST_INTEGRAL)
                ? 2u
                : 0u);
}

/* Select the native expression action: ordinary/derivative, integral or binding refresh. */
lab_request_action_t lab_request_action(lab_request_channel_t channel, uint32_t token)
{
    if (!lab_request_live(channel, token))
        return LAB_REQUEST_ACTION_ORDINARY;
    return lab_request_operation_actions[lab_request_slots[channel].operation];
}

/* Return the operation's timeout in milliseconds, with zero denoting no client deadline. */
unsigned lab_request_deadline(lab_request_channel_t channel, uint32_t token)
{
    return lab_request_live(channel, token) ? lab_request_policy[lab_request_slots[channel].operation].timeout : 0;
}

/* Record a deadline before the host aborts fetch, retaining ownership of the timeout diagnostic. */
bool lab_request_timeout(lab_request_channel_t channel, uint32_t token)
{
    if (!lab_request_live(channel, token) || !lab_request_deadline(channel, token))
        return false;
    lab_request_slots[channel].expired = 1;
    return true;
}

/* Distinguish an owned deadline from explicit cancellation and stale rejection. */
bool lab_request_timed_out(lab_request_channel_t channel, uint32_t token)
{
    return lab_request_live(channel, token) && lab_request_slots[channel].expired;
}

/* Classify the response only while its token still owns the channel, retaining partial expression results. */
lab_request_outcome_t lab_request_accept(lab_request_channel_t channel, uint32_t token, bool http_ok,
                                         bool native_ok, bool has_result, bool partial)
{
    if (!lab_request_live(channel, token))
        return LAB_REQUEST_OUTCOME_STALE;
    const lab_request_slot_t *slot = &lab_request_slots[channel];
    unsigned calculus = slot->operation == LAB_REQUEST_DERIVATIVE || slot->operation == LAB_REQUEST_INTEGRAL ||
                        slot->operation == LAB_REQUEST_PLOT;
    if (slot->expired || !http_ok || !native_ok ||
        (calculus && lab_request_endpoint(channel, token) == LAB_REQUEST_ENDPOINT_EXPRESSION && !has_result))
        return LAB_REQUEST_OUTCOME_FAILURE;
    return slot->operation == LAB_REQUEST_EVALUATE && slot->mode == LAB_MODE_EXPRESSION && partial
               ? LAB_REQUEST_OUTCOME_PARTIAL
               : LAB_REQUEST_OUTCOME_SUCCESS;
}
