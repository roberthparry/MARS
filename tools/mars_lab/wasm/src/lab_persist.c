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
#include "lab_persist.h"

enum { lab_persist_modes = 5 };

typedef struct {
    const char *server, *local;
    unsigned source, empty;
} lab_persist_field_t;

typedef struct {
    unsigned count, canonical, debounce;
    lab_persist_field_t fields[3];
} lab_persist_schema_t;

static const lab_persist_schema_t lab_persist_schemas[lab_persist_modes] = {
    {2,
     0,
     1,
     {{"expression", "mars.exprLab.lastExpression", 0, 0},
      {"expression_updated_at", "mars.exprLab.lastExpressionUpdatedAt", 1, 1}}},
    {2,
     1,
     1,
     {{"equation", "mars.exprLab.lastEquation", 0, 0},
      {"equation_updated_at", "mars.exprLab.lastEquationUpdatedAt", 1, 1}}},
    {1, 0, 0, {{"diffequation", "mars.exprLab.lastDiffequation", 0, 0}}},
    {3,
     0,
     0,
     {{"matrix", "mars.exprLab.lastMatrix", 0, 0},
      {"matrix_operation", "mars.exprLab.lastMatrixOperation", 2, 1},
      {"matrix_operand", "mars.exprLab.lastMatrixOperand", 3, 1}}},
    {3,
     1,
     0,
     {{"integrator_expression", "mars.exprLab.lastIntegratorExpression", 0, 0},
      {"integrator_bounds", "mars.exprLab.lastIntegratorBounds", 4, 0},
      {"integrator_interval_cap", "mars.exprLab.lastIntegratorIntervalCap", 5, 1}}}};

static uint32_t lab_persist_serial, lab_persist_tokens[lab_persist_modes];

/* Invalid indices have no schema, so callers cannot accidentally persist another mode. */
unsigned lab_persist_count(unsigned mode)
{
    return mode < lab_persist_modes ? lab_persist_schemas[mode].count : 0;
}

static const lab_persist_field_t *lab_persist_field(unsigned mode, unsigned field)
{
    return field < lab_persist_count(mode) ? &lab_persist_schemas[mode].fields[field] : 0;
}

/* Borrow immutable labels from the same table that supplies save policy. */
const char *lab_persist_text(unsigned mode, unsigned field, unsigned local)
{
    const lab_persist_field_t *entry = lab_persist_field(mode, field);
    return !entry || local > 1 ? "" : local ? entry->local : entry->server;
}

/* Only short fixed labels are measured; this is not a dispatch scan. */
unsigned lab_persist_text_length(unsigned mode, unsigned field, unsigned local)
{
    const char *text = lab_persist_text(mode, field, local);
    unsigned length = 0;
    while (text[length])
        ++length;
    return length;
}

/* Sources identify already captured host values, not expressions to evaluate. */
int lab_persist_source(unsigned mode, unsigned field)
{
    const lab_persist_field_t *entry = lab_persist_field(mode, field);
    return entry ? (int)entry->source : -1;
}

/* Empty editors and integrator bounds must not replace their last useful local copy. */
int lab_persist_local(unsigned mode, unsigned field, int present)
{
    const lab_persist_field_t *entry = lab_persist_field(mode, field);
    return entry && (present || entry->empty);
}

/* These modes use canonical editor text already returned by native MARS. */
int lab_persist_canonical(unsigned mode)
{
    return mode < lab_persist_modes && lab_persist_schemas[mode].canonical;
}

/* Restore only complete editor copies; preparation and canonicalisation stay native. */
unsigned lab_persist_restore(unsigned mode, int present, int abbreviated, int local)
{
    if (mode >= lab_persist_modes || !present || (mode >= 2 && abbreviated))
        return 0;
    static const unsigned actions[] = {1 | 2 | 8, 1 | 2 | 4, 1, 1, 1 | 16};
    return actions[mode] & ~(local ? 16u : 0u);
}

/* Only expression/equation autosaves defer; all other saves remain immediate. */
unsigned lab_persist_delay(unsigned mode, int debounce)
{
    return mode < lab_persist_modes && lab_persist_schemas[mode].debounce && debounce ? 250 : 0;
}

/* Blank expression saves leave any earlier queued non-empty snapshot intact. */
uint32_t lab_persist_begin(unsigned mode, int has_text)
{
    if (mode >= lab_persist_modes || (mode == 0 && !has_text))
        return 0;
    if (lab_persist_serial == UINT32_MAX) {
        for (unsigned i = 0; i < lab_persist_modes; ++i)
            lab_persist_tokens[i] = 0;
        return 0;
    }
    return lab_persist_tokens[mode] = ++lab_persist_serial;
}

/* A current snapshot may be published once; stale and cross-mode callbacks cannot claim it. */
int lab_persist_take(unsigned mode, uint32_t token)
{
    if (mode >= lab_persist_modes || !token || lab_persist_tokens[mode] != token)
        return 0;
    lab_persist_tokens[mode] = 0;
    return 1;
}
