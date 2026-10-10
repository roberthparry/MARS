/**
 * @file lab_binding_rows.c
 * @brief Structured integrator row policy for the MARS Lab browser.
 *
 * Owns row defaults, exact-name allocation, native reference retention and
 * reconciliation with result bounds. Existing lab_rows planners enforce edits
 * and the 256-row limit. Authored expressions remain opaque; only native metadata
 * determines references. Browser values and temporary handles are scoped to a
 * synchronous call, with indexed name lookup and bounded temporary storage.
 */
#include "lab_dom.h"
#include "../include/lab_forms.h"
#include "lab_binding_editor.h"
#include "lab_binding.h"
#include "lab_rows.h"
#include "lab_workspace.h"
#include "lab_binding_rows.h"

static int lab_binding_row_name(unsigned index)
{
    static const char *const names[] = {"x", "y", "z", "t", "u", "v", "w", "r", "s"};
    return index < 9 ? lab_dom_string(names[index]) : lab_dom_format(index - 8, "x", "");
}

/* Expose the exact candidate catalogue used by the native occupancy policy. */
void lab_binding_row_names(void)
{
    int names = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < 108; ++i) {
        lab_dom_push(names, lab_binding_row_name(i));
        lab_dom_release(mark);
    }
    lab_dom_return(names);
}

static int lab_binding_row_index(void)
{
    int index = lab_dom_map_new();
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < 108; ++i) {
        lab_dom_map_set(index, lab_binding_row_name(i), lab_dom_numeric(i));
        lab_dom_release(mark);
    }
    return index;
}

static void lab_binding_row_occupy(int row, int index, uint32_t occupied[4])
{
    int entry = lab_dom_map_get(index, lab_binding_name(row));
    if (entry) {
        unsigned bit = (unsigned)lab_dom_number(entry);
        occupied[bit >> 5] |= 1u << (bit & 31);
    }
}

static int lab_binding_row_available(const uint32_t occupied[4])
{
    return lab_binding_row_name(lab_forms_name_candidate(occupied[0], occupied[1], occupied[2], occupied[3]));
}

static int lab_binding_row_next(int rows)
{
    int index = lab_binding_row_index();
    uint32_t occupied[4] = {0};
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(rows); ++i) {
        lab_binding_row_occupy(lab_dom_item(rows, i), index, occupied);
        lab_dom_release(mark);
    }
    return lab_binding_row_available(occupied);
}

/* Resolve an exact candidate name without parsing identifiers or retaining browser handles. */
void lab_binding_rows_name(int rows)
{
    lab_dom_return(lab_binding_row_next(rows));
}

static int lab_binding_row_clean(int row, int fallback)
{
    int result = lab_dom_object(5), name = lab_dom_get(row, "name");
    lab_dom_set(result, "kind", lab_dom_string(lab_binding_is(lab_dom_get(row, "kind"), "free") ? "free" : "bound"));
    lab_dom_set(result, "name", lab_dom_text(lab_dom_truth(name) ? name : fallback));
    lab_dom_set(result, "lo", lab_dom_text(lab_binding_or(lab_dom_get(row, "lo"), "")));
    lab_dom_set(result, "hi", lab_dom_text(lab_binding_or(lab_dom_get(row, "hi"), "")));
    return result;
}

/* Normalise structured row fields while preserving authored whitespace and opaque expressions. */
void lab_binding_row_normalise(int row, int fallback)
{
    lab_dom_return(lab_binding_row_clean(row, fallback));
}

/* Serialise a bounded structured batch in C; publish no partial result on invalid Unicode or overflow. */
void lab_binding_rows_text(int rows)
{
    unsigned count = lab_binding_array_count(rows), capacity = lab_workspace_capacity();
    if (count > 256) {
        lab_dom_return(0);
        return;
    }
    unsigned mark = lab_dom_mark(), output = 0;
    static const char *const fields[] = {"name", "lo", "hi"};
    for (unsigned i = 0; i < count; ++i) {
        if (!lab_dom_index_present(rows, i))
            continue;
        int row = lab_binding_row_clean(lab_dom_item(rows, i), lab_dom_string("x"));
        unsigned lengths[3], input = 0;
        for (unsigned f = 0; f < 3; ++f) {
            int length =
                lab_dom_utf8_copy(lab_dom_get(row, fields[f]), lab_workspace_input(0) + input, capacity - input);
            if (length < 0) {
                lab_dom_return(0);
                return;
            }
            lengths[f] = (unsigned)length;
            input += lengths[f];
        }
        int end = lab_rows_text_append(lengths[0], lengths[1], lengths[2],
                                       lab_binding_is(lab_dom_get(row, "kind"), "free"), output);
        if (end < 0) {
            lab_dom_return(0);
            return;
        }
        output = (unsigned)end;
        lab_dom_release(mark);
    }
    lab_dom_return(lab_dom_utf8_string(lab_workspace_input(1), output));
}

static int lab_binding_row_default(int blank)
{
    int row = lab_binding_row_clean(0, lab_dom_string("x"));
    if (!blank) {
        lab_dom_set(row, "lo", lab_dom_string("0"));
        lab_dom_set(row, "hi", lab_dom_string("1"));
    }
    return row;
}

/* Return the standard or blank single-bound default as an independent row array. */
void lab_binding_rows_default(int blank)
{
    int rows = lab_dom_object(4);
    lab_dom_push(rows, lab_binding_row_default(blank));
    lab_dom_return(rows);
}

/* Normalise render input in one pass over preceding authored names, avoiding repeated prefix scans. */
void lab_binding_rows_prepare(int rows)
{
    int result = lab_dom_object(4), index = lab_binding_row_index();
    uint32_t occupied[4] = {0};
    unsigned count = lab_binding_array_count(rows), mark = lab_dom_mark();
    if (!count)
        lab_dom_push(result, lab_binding_row_default(0));
    for (unsigned i = 0; i < count; ++i) {
        int row = lab_dom_item(rows, i);
        lab_dom_push(result, lab_binding_row_clean(row, lab_binding_row_available(occupied)));
        /* Fallbacks depend on the original prefix, as before, not on newly assigned fallback names. */
        lab_binding_row_occupy(row, index, occupied);
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Capture every visible structured row, retaining the established fallback for an empty stack. */
void lab_binding_rows_read(int root)
{
    int result = lab_dom_object(4), nodes = root ? lab_dom_all(root, ".integrator-bound-row") : 0;
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_dom_count(nodes); ++i) {
        int node = lab_dom_item(nodes, i), row = lab_dom_object(5);
        lab_dom_set(row, "kind", lab_dom_read(node, 3, "kind"));
        lab_dom_set(row, "name", lab_dom_read(lab_dom_query(node, "[data-integrator-name]"), 5, ""));
        lab_dom_set(row, "lo", lab_dom_read(lab_dom_query(node, "[data-integrator-lower]"), 5, ""));
        lab_dom_set(row, "hi", lab_dom_read(lab_dom_query(node, "[data-integrator-upper]"), 5, ""));
        lab_dom_push(result, lab_binding_row_clean(row, lab_dom_string("x")));
        lab_dom_release(mark);
    }
    if (!lab_dom_count(result))
        lab_dom_push(result, lab_binding_row_default(0));
    lab_dom_return(result);
}

/* Index native reference metadata by exact name, retaining the source identity alongside it. */
void lab_binding_rows_metadata(int expression, int response)
{
    int metadata = lab_dom_object(5), names = lab_dom_map_new(), rows = lab_dom_get(response, "rows");
    lab_dom_set(metadata, "expression", expression);
    lab_dom_set(metadata, "valid", lab_dom_get(response, "references_valid"));
    lab_dom_set(metadata, "names", names);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(rows); ++i) {
        int row = lab_dom_item(rows, i);
        lab_dom_map_set(names, lab_dom_get(row, "name"), lab_dom_get(row, "referenced"));
        lab_dom_release(mark);
    }
    lab_dom_return(metadata);
}

/* Unknown, invalid and stale metadata retain rows rather than guessing mathematical references. */
int lab_binding_rows_reference(int metadata, int expression, int name)
{
    if (!lab_dom_truth(metadata) || !lab_dom_truth(lab_dom_get(metadata, "valid")) ||
        !lab_dom_equal(expression, lab_dom_get(metadata, "expression")))
        return 1;
    int referenced = lab_dom_map_get(lab_dom_get(metadata, "names"), name);
    return !lab_dom_type(referenced) || lab_dom_truth(referenced);
}

static void lab_binding_rows_flags(int rows)
{
    uint32_t *flags = lab_rows_input();
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(rows); ++i) {
        int row = lab_dom_item(rows, i);
        flags[i] = (lab_binding_is(lab_dom_get(row, "kind"), "free") ? 1u : 0u) |
                   (lab_dom_truth(lab_dom_get(row, "lo")) ? 2u : 0u) |
                   (lab_dom_truth(lab_dom_get(row, "hi")) ? 4u : 0u) | 8u;
        lab_dom_release(mark);
    }
}

/* Resolve the native row plan to opaque records, including its single fallback bound. */
void lab_binding_rows_plan(int rows, int metadata, int expression)
{
    unsigned count = lab_binding_array_count(rows);
    if (count > 256) {
        lab_dom_return(0);
        return;
    }
    lab_binding_rows_flags(rows);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        if (!lab_binding_rows_reference(metadata, expression, lab_dom_get(lab_dom_item(rows, i), "name")))
            lab_rows_input()[i] &= ~8u;
        lab_dom_release(mark);
    }
    const uint32_t *plan = lab_rows_select(count);
    int result = lab_dom_object(5), bounds = lab_dom_object(4), active = lab_dom_object(4);
    mark = lab_dom_mark();
    for (unsigned i = 0; i < plan[0] + plan[1]; ++i) {
        unsigned index = plan[2 + i];
        lab_dom_push(i < plan[0] ? bounds : active,
                     index == 256 ? lab_binding_row_default(0) : lab_dom_item(rows, index));
        lab_dom_release(mark);
    }
    lab_dom_set(result, "bounds", bounds);
    lab_dom_set(result, "rows", active);
    lab_dom_return(result);
}

/* Apply row edits atomically, retaining metadata on existing rows and clearing disabled bounds. */
void lab_binding_rows_edit(int rows, unsigned index, unsigned operation)
{
    unsigned count = lab_binding_array_count(rows);
    if (count > 256) {
        lab_dom_return(0);
        return;
    }
    lab_binding_rows_flags(rows);
    const uint32_t *plan = lab_rows_edit(count, index, operation);
    if (!plan) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(4), next = lab_binding_row_next(rows);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < plan[0]; ++i) {
        unsigned source = plan[1 + 2 * i], flags = plan[2 + 2 * i];
        int row = source == 256 ? lab_binding_row_clean(0, next) : lab_binding_copy(lab_dom_item(rows, source));
        lab_dom_set(row, "kind", lab_dom_string(flags & 1 ? "free" : "bound"));
        if (!(flags & 2))
            lab_dom_set(row, "lo", lab_dom_string(""));
        if (!(flags & 4))
            lab_dom_set(row, "hi", lab_dom_string(""));
        lab_dom_push(result, row);
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}

/* Merge native result bounds while retaining only free rows reported as variable parameters. */
void lab_binding_rows_merge(int rows, int response)
{
    int bounds = lab_dom_get(response, "bounds"), bindings = lab_dom_get(response, "binding_values");
    unsigned count = lab_binding_array_count(rows), bound_count = lab_binding_array_count(bounds);
    if (!bound_count) {
        lab_dom_return(0);
        return;
    }
    if (count > 256 || bound_count > 256) {
        lab_dom_return(lab_dom_scalar(1, 0));
        return;
    }
    int names = lab_dom_map_new();
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(bindings); ++i) {
        int name = lab_binding_variable_name(lab_dom_item(bindings, i));
        if (name)
            lab_dom_map_set(names, name, lab_dom_scalar(1, 1));
        lab_dom_release(mark);
    }
    lab_binding_rows_flags(rows);
    for (unsigned i = 0; i < count; ++i) {
        if (!lab_dom_truth(lab_dom_map_get(names, lab_binding_name(lab_dom_item(rows, i)))))
            lab_rows_input()[i] &= ~8u;
        lab_dom_release(mark);
    }
    const uint32_t *plan = lab_rows_merge(count, bound_count);
    if (!plan) {
        lab_dom_return(lab_dom_scalar(1, 0));
        return;
    }
    int result = lab_dom_object(4);
    mark = lab_dom_mark();
    for (unsigned i = 0; i < plan[0]; ++i) {
        unsigned source = plan[1 + i];
        lab_dom_push(result, source < 256 ? lab_dom_item(rows, source) : lab_dom_item(bounds, source - 256));
        lab_dom_release(mark);
    }
    lab_dom_return(result);
}
