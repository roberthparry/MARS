/**
 * @file lab_rows.c
 * @brief Integrator row planning, structured text formatting and form revision ownership.
 *
 * Owns row activity, fallback, editing and free-parameter retention policy. The host
 * supplies presence flags and applies returned indices to its structured rows.
 * Revision tokens reject obsolete asynchronous form preparations. Structured row
 * text uses the shared workspace transfer buffers, copying authored fields verbatim.
 * No names, bounds or mathematical expressions are parsed here. Fixed planning buffers
 * match the native form limit; bounded passes visit each supplied row once per
 * phase and replace repeated host crossings and temporary JavaScript sets.
 */
#include "lab_workspace.h"
#include "lab_rows.h"

enum { lab_rows_limit = 256, lab_rows_free = 1, lab_rows_lower = 2, lab_rows_upper = 4, lab_rows_referenced = 8 };

static uint32_t lab_rows_flags[lab_rows_limit];
static uint32_t lab_rows_plan[2 + 2 * lab_rows_limit];
static uint32_t lab_rows_revision;
static int lab_rows_exhausted;

/* Revisions never wrap, so a very old asynchronous response cannot become current again. */
uint32_t lab_rows_revision_next(void)
{
    if (lab_rows_revision == UINT32_MAX) {
        lab_rows_exhausted = 1;
        return 0;
    }
    return ++lab_rows_revision;
}

/* Expression identity is supplied as an exact host comparison, never mathematical interpretation. */
int lab_rows_revision_accept(uint32_t revision, int same_expression)
{
    return !lab_rows_exhausted && revision && revision == lab_rows_revision && same_expression;
}

static unsigned lab_rows_copy(unsigned char *target, unsigned offset, const unsigned char *text, unsigned length)
{
    for (unsigned i = 0; i < length; ++i)
        target[offset + i] = text[i];
    return offset + length;
}

/* Append one row with its separator after validating the complete output extent. */
int lab_rows_text_append(unsigned name, unsigned lower, unsigned upper, int free_row, unsigned offset)
{
    unsigned capacity = lab_workspace_capacity();
    if (name > capacity || lower > capacity - name || upper > capacity - name - lower)
        return -1;
    unsigned length = free_row ? name : name + (upper ? upper + (lower ? lower : 0) : 0);
    unsigned notation = free_row ? 5 : upper ? (lower ? 7 : 3) : 0;
    if (!length && !notation)
        return offset <= capacity ? (int)offset : -1;
    unsigned extra = notation + (offset != 0);
    if (offset > capacity || extra > capacity - offset || length > capacity - offset - extra)
        return -1;
    const unsigned char *input = lab_workspace_input(0);
    unsigned char *output = lab_workspace_input(1);
    if (offset)
        output[offset++] = '\n';
    if (free_row)
        offset = lab_rows_copy(output, offset, (const unsigned char *)"free ", 5);
    offset = lab_rows_copy(output, offset, input, name);
    if (!free_row && upper) {
        offset = lab_rows_copy(output, offset, (const unsigned char *)" = ", 3);
        if (lower) {
            offset = lab_rows_copy(output, offset, input + name, lower);
            offset = lab_rows_copy(output, offset, (const unsigned char *)" .. ", 4);
        }
        offset = lab_rows_copy(output, offset, input + name + lower, upper);
    }
    return (int)offset;
}

/* Copy authored fields around fixed notation without interpreting bound expressions. */
int lab_rows_text(unsigned name, unsigned lower, unsigned upper, int free_row)
{
    return lab_rows_text_append(name, lower, upper, free_row, 0);
}

static int lab_rows_valid(unsigned count)
{
    if (count > lab_rows_limit)
        return 0;
    for (unsigned i = 0; i < count; ++i)
        if (lab_rows_flags[i] & ~15u)
            return 0;
    return 1;
}

/* Borrow structured input storage, with no transfer of ownership. */
uint32_t *lab_rows_input(void)
{
    return lab_rows_flags;
}

/* Select indices in one bounded transaction; fallbacks never reinterpret authored text. */
const uint32_t *lab_rows_select(unsigned count)
{
    if (!lab_rows_valid(count))
        return 0;
    unsigned bounds = 0, selected = 0, active = 0;
    uint32_t bound_indices[lab_rows_limit], active_indices[lab_rows_limit];
    for (unsigned i = 0; i < count; ++i)
        bounds += !(lab_rows_flags[i] & lab_rows_free);
    for (unsigned i = 0; i < count; ++i) {
        uint32_t flags = lab_rows_flags[i];
        if (flags & lab_rows_free) {
            if (flags & lab_rows_referenced)
                active_indices[active++] = i;
        } else if (bounds == 1 || (flags & (lab_rows_lower | lab_rows_upper | lab_rows_referenced))) {
            bound_indices[selected++] = i;
            active_indices[active++] = i;
        }
    }
    if (!selected)
        bound_indices[selected++] = lab_rows_limit;
    if (!active)
        active_indices[active++] = lab_rows_limit;
    lab_rows_plan[0] = selected;
    lab_rows_plan[1] = active;
    for (unsigned i = 0; i < selected; ++i)
        lab_rows_plan[2 + i] = bound_indices[i];
    for (unsigned i = 0; i < active; ++i)
        lab_rows_plan[2 + selected + i] = active_indices[i];
    return lab_rows_plan;
}

/* Preserve free rows in place while consuming result bounds, then append remaining bounds. */
const uint32_t *lab_rows_merge(unsigned count, unsigned bounds)
{
    if (!lab_rows_valid(count) || bounds > lab_rows_limit)
        return 0;
    unsigned kept = 0;
    for (unsigned i = 0; i < count; ++i)
        kept += (lab_rows_flags[i] & (lab_rows_free | lab_rows_referenced)) == (lab_rows_free | lab_rows_referenced);
    if (bounds && kept + bounds > lab_rows_limit)
        return 0;
    unsigned size = 0, next = 0;
    for (unsigned i = 0; i < count; ++i) {
        uint32_t flags = lab_rows_flags[i];
        if (!bounds || ((flags & lab_rows_free) && (flags & lab_rows_referenced)))
            lab_rows_plan[1 + size++] = i;
        else if (!(flags & lab_rows_free) && next < bounds)
            lab_rows_plan[1 + size++] = lab_rows_limit + next++;
    }
    while (next < bounds)
        lab_rows_plan[1 + size++] = lab_rows_limit + next++;
    lab_rows_plan[0] = size;
    return lab_rows_plan;
}

/* Stage the entire edit before publication, so rejected edits leave the previous plan intact. */
const uint32_t *lab_rows_edit(unsigned count, unsigned index, unsigned operation)
{
    if (!count || !lab_rows_valid(count) || index >= count || operation < 1 || operation > 3)
        return 0;
    unsigned bounds = 0;
    for (unsigned i = 0; i < count; ++i)
        bounds += !(lab_rows_flags[i] & lab_rows_free);
    if (operation == 3 && (count == 1 || (bounds == 1 && !(lab_rows_flags[index] & lab_rows_free))))
        return 0;
    uint32_t plan[1 + 2 * (lab_rows_limit + 1)];
    unsigned size = 0, result_bounds = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (operation == 3 && i == index)
            continue;
        uint32_t flags = lab_rows_flags[i];
        if (operation == 1 && i == index) {
            flags ^= lab_rows_free;
            if (flags & lab_rows_free)
                flags &= ~(lab_rows_lower | lab_rows_upper);
        }
        plan[1 + 2 * size] = i;
        plan[2 + 2 * size++] = flags;
        result_bounds += !(flags & lab_rows_free);
        if (operation == 2 && i == index) {
            plan[1 + 2 * size] = lab_rows_limit;
            plan[2 + 2 * size++] = 0;
            ++result_bounds;
        }
    }
    if (!result_bounds) {
        plan[1 + 2 * size] = lab_rows_limit;
        plan[2 + 2 * size++] = 0;
    }
    if (size > lab_rows_limit)
        return 0;
    plan[0] = size;
    for (unsigned i = 0; i <= 2 * size; ++i)
        lab_rows_plan[i] = plan[i];
    return lab_rows_plan;
}
