/**
 * @file lab_persist.c
 * @brief Worksheet save schemas and deferred-save ownership in C/WebAssembly.
 *
 * Directly indexed schemas define server fields, local storage keys, value
 * sources and empty-value policy for the five mathematical worksheets. Independent
 * save tokens prevent superseded timers from publishing stale snapshots. The host
 * captures DOM values and supplies clock readings, browser storage, timers and
 * Protobuf transport. Mathematical text remains opaque and native MARS alone
 * supplies its canonical representation. No allocation or browser handles are used.
 */
#include <string.h>

#include "lab_mode.h"
#include "lab_persist.h"

enum { lab_persist_modes = 5 };

typedef struct {
    const char *server, *local;
    lab_persist_source_t source;
    unsigned empty;
} lab_persist_field_t;

typedef struct {
    unsigned count, canonical, debounce;
    lab_persist_field_t fields[3];
} lab_persist_schema_t;

static const lab_persist_schema_t lab_persist_schemas[lab_persist_modes] = {
    {2,
     0,
     1,
     {{"expression", "mars.exprLab.lastExpression", LAB_PERSIST_TEXT, 0},
      {"expression_updated_at", "mars.exprLab.lastExpressionUpdatedAt", LAB_PERSIST_TIMESTAMP, 1}}},
    {2,
     1,
     1,
     {{"equation", "mars.exprLab.lastEquation", LAB_PERSIST_TEXT, 0},
      {"equation_updated_at", "mars.exprLab.lastEquationUpdatedAt", LAB_PERSIST_TIMESTAMP, 1}}},
    {1, 0, 0, {{"diffequation", "mars.exprLab.lastDiffequation", LAB_PERSIST_TEXT, 0}}},
    {3,
     0,
     0,
     {{"matrix", "mars.exprLab.lastMatrix", LAB_PERSIST_TEXT, 0},
      {"matrix_operation", "mars.exprLab.lastMatrixOperation", LAB_PERSIST_OPERATION, 1},
      {"matrix_operand", "mars.exprLab.lastMatrixOperand", LAB_PERSIST_OPERAND, 1}}},
    {3,
     1,
     0,
     {{"integrator_expression", "mars.exprLab.lastIntegratorExpression", LAB_PERSIST_TEXT, 0},
      {"integrator_bounds", "mars.exprLab.lastIntegratorBounds", LAB_PERSIST_BOUNDS, 0},
      {"integrator_interval_cap", "mars.exprLab.lastIntegratorIntervalCap", LAB_PERSIST_INTERVAL_CAP, 1}}}};

static uint32_t lab_persist_serial, lab_persist_tokens[lab_persist_modes];

/* Invalid indices have no schema, so callers cannot accidentally persist another mode. */
unsigned lab_persist_count(lab_mode_t mode)
{
    return (unsigned)mode < lab_persist_modes ? lab_persist_schemas[mode].count : 0;
}

static const lab_persist_field_t *lab_persist_field(lab_mode_t mode, unsigned field)
{
    return field < lab_persist_count(mode) ? &lab_persist_schemas[mode].fields[field] : 0;
}

/* Borrow immutable labels from the same table that supplies save policy. */
const char *lab_persist_text(lab_mode_t mode, unsigned field, lab_persist_origin_t origin)
{
    const lab_persist_field_t *entry = lab_persist_field(mode, field);
    return !entry || (unsigned)origin > LAB_PERSIST_LOCAL ? ""
           : origin != LAB_PERSIST_SERVER ? entry->local : entry->server;
}

/* Only short fixed labels are measured; this is not a dispatch scan. */
unsigned lab_persist_text_length(lab_mode_t mode, unsigned field, lab_persist_origin_t origin)
{
    return (unsigned)strlen(lab_persist_text(mode, field, origin));
}

/* Sources identify already captured host values, not expressions to evaluate. */
lab_persist_source_t lab_persist_source(lab_mode_t mode, unsigned field)
{
    const lab_persist_field_t *entry = lab_persist_field(mode, field);
    return entry ? entry->source : LAB_PERSIST_SOURCE_INVALID;
}

/* Empty editors and integrator bounds must not replace their last useful local copy. */
bool lab_persist_local(lab_mode_t mode, unsigned field, bool present)
{
    const lab_persist_field_t *entry = lab_persist_field(mode, field);
    return entry && (present || entry->empty);
}

/* These modes use canonical editor text already returned by native MARS. */
bool lab_persist_canonical(lab_mode_t mode)
{
    return (unsigned)mode < lab_persist_modes && lab_persist_schemas[mode].canonical;
}

/* Restore only complete editor copies; preparation and canonicalisation stay native. */
unsigned lab_persist_restore(lab_mode_t mode, bool present, bool abbreviated, lab_persist_origin_t origin)
{
    if ((unsigned)mode >= lab_persist_modes || !present || (mode >= LAB_MODE_DIFFERENTIAL_EQUATION && abbreviated))
        return 0;
    static const unsigned actions[] = {1 | 2 | 8, 1 | 2 | 4, 1, 1, 1 | 16};
    return actions[mode] & ~(origin != LAB_PERSIST_SERVER ? 16u : 0u);
}

/* Only expression/equation autosaves defer; all other saves remain immediate. */
unsigned lab_persist_delay(lab_mode_t mode, bool debounce)
{
    return (unsigned)mode < lab_persist_modes && lab_persist_schemas[mode].debounce && debounce ? 250 : 0;
}

/* Blank expression saves leave any earlier queued non-empty snapshot intact. */
uint32_t lab_persist_begin(lab_mode_t mode, bool has_text)
{
    if ((unsigned)mode >= lab_persist_modes || (mode == LAB_MODE_EXPRESSION && !has_text))
        return 0;
    if (lab_persist_serial == UINT32_MAX) {
        for (unsigned i = 0; i < lab_persist_modes; ++i)
            lab_persist_tokens[i] = 0;
        return 0;
    }
    return lab_persist_tokens[mode] = ++lab_persist_serial;
}

/* A current snapshot may be published once; stale and cross-mode callbacks cannot claim it. */
bool lab_persist_take(lab_mode_t mode, uint32_t token)
{
    if ((unsigned)mode >= lab_persist_modes || !token || lab_persist_tokens[mode] != token)
        return false;
    lab_persist_tokens[mode] = 0;
    return true;
}
