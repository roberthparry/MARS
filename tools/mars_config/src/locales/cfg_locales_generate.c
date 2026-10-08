/**
 * @file cfg_locales_generate.c
 * @brief Orchestrate native generation from pinned resolved CLDR and supplements.
 *
 * Owns all source and intermediate containers for one invocation. Nothing is
 * published until loading, selection, tokenisation and complete SQL rendering
 * succeed. Each failure returns NULL and releases the entire generation state.
 */
#include <stdio.h>

#include "cfg_locales.h"
#include "cfg_locales_internal.h"

/* Generate complete caller-owned SQL without changing repository or installed data. */
string_t *cfg_locales_generate(const string_t *root, const string_t *data_dir)
{
    cfg_locales_state state = {0};
    bool ok = root && data_dir && cfg_locales_load(&state, data_dir);
    if (!ok)
        fputs("Invalid or incomplete pinned calendar locale data.\n", stderr);
    if (ok && !cfg_locales_select(&state, root)) {
        fputs("Cannot select calendar locales from the pinned catalogue.\n", stderr);
        ok = false;
    }
    if (ok && !cfg_locales_tables(&state)) {
        fputs("Cannot derive calendar name sets or supported date patterns.\n", stderr);
        ok = false;
    }
    string_t *out = ok ? cfg_locales_sql(&state) : NULL;
    if (ok && !out)
        fputs("Cannot render calendar locale SQL.\n", stderr);
    json_free(state.raw);
    json_free(state.supp);
    json_free(state.records);
    json_free(state.selected);
    json_free(state.order);
    json_free(state.active);
    json_free(state.names);
    json_free(state.choices);
    for (size_t i = 0; i < 9; ++i)
        json_free(state.tables[i]);
    return out;
}
