/**
 * @file lab_workspace.c
 * @brief Freestanding worksheet mode, precision and bounded history ownership.
 *
 * Stores actual editor and codec snapshot bytes in WebAssembly, not host object
 * handles. DOM rendering, serialisation and persistence I/O belong to the host;
 * mode transitions, source/display matching, precision policy, history equality, eviction and navigation
 * belong here. Fixed storage uses just over 44 MiB and requires no libc or imports.
 * The bounded mode/entry scans below update offsets after compacting a byte arena;
 * no scan depends on unbounded user input or accumulated browsing history.
 */
#include <stdint.h>

#include "lab_workspace.h"

enum { LAB_WS_MODES = 7, LAB_WS_BYTES = 4194304, LAB_WS_ARENA = 33554432, LAB_WS_ENTRIES = 128 };

typedef struct {
    uint32_t offset, length;
} lab_workspace_entry_t;

typedef struct {
    lab_workspace_entry_t entries[2][LAB_WS_ENTRIES];
    lab_workspace_entry_t editor, committed;
    unsigned count[2], precision;
} lab_workspace_mode_t;

static lab_workspace_mode_t lab_workspace_modes[LAB_WS_MODES];
static lab_workspace_entry_t lab_workspace_source[5];
static unsigned char lab_workspace_staging[2][LAB_WS_BYTES], lab_workspace_result[LAB_WS_BYTES];
static unsigned char lab_workspace_storage[LAB_WS_ARENA];
static unsigned lab_workspace_used;
static unsigned lab_workspace_selected;
static const char *const lab_workspace_names[LAB_WS_MODES] = {"expression", "equation", "diffequation", "matrix",
                                                              "integrator", "datetime", "almanac"};

static void lab_workspace_copy(unsigned char *out, const unsigned char *in, unsigned size)
{
    for (unsigned i = 0; i < size; ++i)
        out[i] = in[i];
}

static int lab_workspace_bytes_equal(const unsigned char *left, unsigned left_size, const unsigned char *right,
                                     unsigned right_size)
{
    if (left_size != right_size)
        return 0;
    for (unsigned i = 0; i < left_size; ++i)
        if (left[i] != right[i])
            return 0;
    return 1;
}

static void lab_workspace_remove_bytes(unsigned offset, unsigned length)
{
    unsigned end = offset + length;
    lab_workspace_copy(lab_workspace_storage + offset, lab_workspace_storage + end, lab_workspace_used - end);
    lab_workspace_used -= length;
    for (unsigned field = 0; field < 5; ++field)
        if (lab_workspace_source[field].length && lab_workspace_source[field].offset >= end)
            lab_workspace_source[field].offset -= length;
    for (unsigned mode = 0; mode < LAB_WS_MODES; ++mode) {
        lab_workspace_mode_t *state = &lab_workspace_modes[mode];
        if (state->editor.length && state->editor.offset >= end)
            state->editor.offset -= length;
        if (state->committed.length && state->committed.offset >= end)
            state->committed.offset -= length;
        for (unsigned side = 0; side < 2; ++side)
            for (unsigned i = 0; i < state->count[side]; ++i)
                if (state->entries[side][i].offset >= end)
                    state->entries[side][i].offset -= length;
    }
}

static void lab_workspace_remove(lab_workspace_mode_t *state, unsigned direction, unsigned index)
{
    unsigned offset = state->entries[direction][index].offset;
    unsigned length = state->entries[direction][index].length;
    --state->count[direction];
    for (unsigned i = index; i < state->count[direction]; ++i) {
        state->entries[direction][i].offset = state->entries[direction][i + 1].offset;
        state->entries[direction][i].length = state->entries[direction][i + 1].length;
    }
    lab_workspace_remove_bytes(offset, length);
}

/* Read-only preflight: only history and the explicitly replaced bytes are reclaimable. */
static int lab_workspace_can_reserve(unsigned length, unsigned replaced)
{
    unsigned available = LAB_WS_ARENA - lab_workspace_used + replaced;
    if (length <= available)
        return 1;
    for (unsigned mode = 0; mode < LAB_WS_MODES; ++mode)
        for (unsigned side = 0; side < 2; ++side) {
            lab_workspace_mode_t *state = &lab_workspace_modes[mode];
            for (unsigned i = 0; i < state->count[side]; ++i)
                available += state->entries[side][i].length;
        }
    return length <= available;
}

/* Preflight has proved capacity; subsequent removals cannot invalidate that proof. */
static void lab_workspace_reclaim(unsigned length, unsigned replaced)
{
    for (unsigned mode = 0; mode < LAB_WS_MODES && length > LAB_WS_ARENA - lab_workspace_used + replaced; ++mode)
        for (unsigned side = 0; side < 2 && length > LAB_WS_ARENA - lab_workspace_used + replaced; ++side) {
            lab_workspace_mode_t *state = &lab_workspace_modes[mode];
            while (state->count[side] && length > LAB_WS_ARENA - lab_workspace_used + replaced)
                lab_workspace_remove(state, side, 0);
        }
}

static int lab_workspace_replace(lab_workspace_entry_t *entry, unsigned length)
{
    if (lab_workspace_bytes_equal(lab_workspace_storage + entry->offset, entry->length, lab_workspace_staging[0],
                                  length))
        return 1;
    if (!lab_workspace_can_reserve(length, entry->length))
        return 0;
    lab_workspace_reclaim(length, entry->length);
    unsigned old_length = entry->length;
    entry->length = 0;
    if (old_length)
        lab_workspace_remove_bytes(entry->offset, old_length);
    entry->offset = lab_workspace_used;
    entry->length = length;
    lab_workspace_copy(lab_workspace_storage + lab_workspace_used, lab_workspace_staging[0], length);
    lab_workspace_used += length;
    return 1;
}

/* A preflighted append cannot fail. Apply mandatory entry eviction before byte-budget eviction. */
static void lab_workspace_append(lab_workspace_mode_t *state, unsigned direction, unsigned length)
{
    if (state->count[direction] == LAB_WS_ENTRIES)
        lab_workspace_remove(state, direction, 0);
    lab_workspace_reclaim(length, 0);
    lab_workspace_entry_t *entry = &state->entries[direction][state->count[direction]++];
    entry->offset = lab_workspace_used;
    entry->length = length;
    lab_workspace_copy(lab_workspace_storage + lab_workspace_used, lab_workspace_staging[0], length);
    lab_workspace_used += length;
}

/* Reset logical ownership; inaccessible old bytes need not be cleared. */
void lab_workspace_reset(void)
{
    lab_workspace_selected = 0;
    lab_workspace_used = 0;
    for (unsigned field = 0; field < 5; ++field)
        lab_workspace_source[field].length = 0;
    for (unsigned mode = 0; mode < LAB_WS_MODES; ++mode) {
        lab_workspace_mode_t *state = &lab_workspace_modes[mode];
        state->count[0] = state->count[1] = 0;
        state->editor.length = state->committed.length = 0;
        state->precision = lab_workspace_default_precision(mode);
    }
}

/* Expose bounded transfer storage only, never the state structure. */
unsigned char *lab_workspace_input(unsigned index)
{
    return index < 2 ? lab_workspace_staging[index] : (unsigned char *)0;
}

/* Report the byte bound to the host before it copies. */
unsigned lab_workspace_capacity(void)
{
    return LAB_WS_BYTES;
}

/* Borrow the last copied result. */
const unsigned char *lab_workspace_output(void)
{
    return lab_workspace_result;
}

/* Resolve the seven fixed names; invalid names select expression. */
unsigned lab_workspace_mode_id(unsigned length)
{
    if (length > 12)
        return 0;
    for (unsigned mode = 0; mode < LAB_WS_MODES; ++mode) {
        const char *name = lab_workspace_names[mode];
        unsigned i = 0;
        while (name[i] && i < length && (unsigned char)name[i] == lab_workspace_staging[0][i])
            ++i;
        if (i == length && !name[i])
            return mode;
    }
    return 0;
}

/* Return the native selection. */
unsigned lab_workspace_mode(void)
{
    return lab_workspace_selected;
}

/* Keep selection and change detection together. */
int lab_workspace_select(unsigned mode)
{
    if (mode >= LAB_WS_MODES)
        mode = 0;
    int changed = mode != lab_workspace_selected;
    lab_workspace_selected = mode;
    return changed;
}

/* Preserve the four mathematical and three low-precision defaults. */
unsigned lab_workspace_default_precision(unsigned mode)
{
    return mode < 4 ? 256 : 53;
}

/* Normalise before converting a possibly non-finite host double to an integer. */
unsigned lab_workspace_precision_set(unsigned mode, double bits)
{
    if (mode >= LAB_WS_MODES)
        return 0;
    if (bits != bits || bits > 1.7976931348623157e308 || bits < -1.7976931348623157e308)
        return lab_workspace_modes[mode].precision;
    unsigned precision = bits < 17 ? 17 : bits > 1048576 ? 1048576 : (unsigned)bits;
    lab_workspace_modes[mode].precision = precision;
    return precision;
}

/* Access precision without a host-side mutable mirror. */
unsigned lab_workspace_precision(unsigned mode)
{
    return mode < LAB_WS_MODES ? lab_workspace_modes[mode].precision : 0;
}

/* Saved legacy values below binary64 do not reduce the actual request precision. */
unsigned lab_workspace_requested_precision(unsigned mode)
{
    unsigned bits = lab_workspace_precision(mode);
    return bits < 53 ? 53 : bits;
}

/* Interactive precision requests preserve the original binary64 floor. */
unsigned lab_workspace_request_precision(unsigned mode, double bits)
{
    if (bits == bits && bits >= -1.7976931348623157e308 && bits <= 1.7976931348623157e308 && bits < 53)
        bits = 53;
    return lab_workspace_precision_set(mode, bits);
}

/* ceil(bits*log10(2)), with no freestanding libm dependency. */
unsigned lab_workspace_digits(double bits)
{
    if (!(bits > 53))
        return 17;
    if (bits > 1048576)
        bits = 1048576;
    double digits = bits * 0.30102999566398119521;
    unsigned whole = (unsigned)digits;
    return whole + (digits > whole);
}

/* Follow binary64, double-double, then 128-bit steps. */
unsigned lab_workspace_precision_step(double bits, int direction)
{
    unsigned current = !(bits >= 53) ? 53 : bits > 1048576 ? 1048576 : (unsigned)bits;
    if (direction > 0) {
        if (current < 106)
            return 106;
        if (current < 256)
            return 256;
        return current >= 1048576 ? 1048576 : (current / 128 + 1) * 128;
    }
    if (current <= 106)
        return 53;
    if (current <= 256)
        return 106;
    return ((current - 1) / 128) * 128;
}

/* Control availability follows the same native request bounds as stepping. */
int lab_workspace_precision_can_step(unsigned mode, int direction)
{
    unsigned bits = lab_workspace_requested_precision(mode);
    return mode < LAB_WS_MODES && (direction > 0 ? bits < 1048576 : bits > 53);
}

/* Reject overflow rather than truncate mathematical editor text. */
int lab_workspace_editor_set(unsigned mode, unsigned length)
{
    if (mode >= LAB_WS_MODES || length > LAB_WS_BYTES)
        return 0;
    return lab_workspace_replace(&lab_workspace_modes[mode].editor, length);
}

/* Copy editor bytes out of opaque ownership. */
int lab_workspace_editor_get(unsigned mode)
{
    if (mode >= LAB_WS_MODES)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    lab_workspace_copy(lab_workspace_result, lab_workspace_storage + state->editor.offset, state->editor.length);
    return (int)state->editor.length;
}

/* Source fields share the bounded arena and are pinned just like active editors. */
int lab_workspace_source_set(unsigned field, unsigned length)
{
    if (field >= 5 || length > LAB_WS_BYTES)
        return 0;
    return lab_workspace_replace(&lab_workspace_source[field], length);
}

/* Copy exact authored bytes, never numerical or mathematical interpretations. */
int lab_workspace_source_get(unsigned field)
{
    if (field >= 5)
        return -1;
    const lab_workspace_entry_t *entry = &lab_workspace_source[field];
    lab_workspace_copy(lab_workspace_result, lab_workspace_storage + entry->offset, entry->length);
    return (int)entry->length;
}

/* Compare the current trimmed browser input with the retained display bytes. */
int lab_workspace_source_matches(unsigned length)
{
    const lab_workspace_entry_t *display = &lab_workspace_source[1];
    return length <= LAB_WS_BYTES &&
           lab_workspace_bytes_equal(lab_workspace_staging[0], length, lab_workspace_storage + display->offset,
                                     display->length);
}

/* Resolve only an unchanged display; a changed editor must go through native presentation metadata. */
int lab_workspace_source_resolve(unsigned length, int goal)
{
    unsigned field = goal ? 3 : 0;
    if (!lab_workspace_source_matches(length) || (!goal && !lab_workspace_source[1].length))
        return 0;
    return lab_workspace_source_get(field);
}

/* Calendar summaries are regenerated from their forms, never captured as authored mathematics. */
int lab_workspace_editor_capture(unsigned mode, unsigned length, unsigned default_length)
{
    if (mode >= LAB_WS_MODES || length > LAB_WS_BYTES || default_length > LAB_WS_BYTES)
        return 0;
    if (mode >= 5) {
        lab_workspace_copy(lab_workspace_staging[0], lab_workspace_staging[1], default_length);
        length = default_length;
    } else if (!length) {
        return 1;
    }
    return lab_workspace_editor_set(mode, length);
}

/* Selecting a default is a read, so merely visiting a mode never overwrites its saved editor. */
int lab_workspace_editor_restore(unsigned mode, unsigned default_length)
{
    if (mode >= LAB_WS_MODES || default_length > LAB_WS_BYTES)
        return -1;
    if (mode < 5 && lab_workspace_modes[mode].editor.length)
        return lab_workspace_editor_get(mode);
    lab_workspace_copy(lab_workspace_result, lab_workspace_staging[1], default_length);
    return (int)default_length;
}

/* The first three editors always maintain bindings; structured editors require explicit bound metadata. */
int lab_workspace_editor_bound(unsigned mode, int has_bindings)
{
    return mode < 3 || (mode < 5 && has_bindings != 0);
}

/* Compare complete serialisations, including form fields and their ordering. */
int lab_workspace_equal(unsigned left_length, unsigned right_length)
{
    return left_length <= LAB_WS_BYTES && right_length <= LAB_WS_BYTES &&
           lab_workspace_bytes_equal(lab_workspace_staging[0], left_length, lab_workspace_staging[1], right_length);
}

/* Commit owns a copy and cannot be changed by later host mutations. */
int lab_workspace_commit(unsigned mode, unsigned length)
{
    if (mode >= LAB_WS_MODES || !length || length > LAB_WS_BYTES)
        return 0;
    return lab_workspace_replace(&lab_workspace_modes[mode].committed, length);
}

/* Unchanged evaluations do not create an extra history item. */
int lab_workspace_previous(unsigned mode, unsigned length)
{
    if (mode >= LAB_WS_MODES || !length || length > LAB_WS_BYTES)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (!state->committed.length ||
        lab_workspace_bytes_equal(lab_workspace_storage + state->committed.offset, state->committed.length,
                                  lab_workspace_staging[0], length))
        return 0;
    lab_workspace_copy(lab_workspace_result, lab_workspace_storage + state->committed.offset, state->committed.length);
    return (int)state->committed.length;
}

/* Stack indices are direct and bounded. */
unsigned lab_workspace_history_count(unsigned mode, unsigned direction)
{
    return mode < LAB_WS_MODES && direction < 2 ? lab_workspace_modes[mode].count[direction] : 0;
}

/* Clearing forward history retains back history and committed state. */
void lab_workspace_history_clear(unsigned mode, unsigned direction)
{
    if (mode >= LAB_WS_MODES || direction >= 2)
        return;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    while (state->count[direction])
        lab_workspace_remove(state, direction, state->count[direction] - 1);
}

/* Native policy owns blank suppression, deduplication, invalidation and bounded eviction. */
int lab_workspace_history_push(unsigned mode, unsigned direction, unsigned length, int has_text, unsigned flags)
{
    if (mode >= LAB_WS_MODES || direction >= 2 || !length || length > LAB_WS_BYTES || flags > 3)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    int duplicate = 0;
    if ((flags & 1) && state->count[direction]) {
        lab_workspace_entry_t *last = &state->entries[direction][state->count[direction] - 1];
        duplicate = lab_workspace_bytes_equal(lab_workspace_storage + last->offset, last->length,
                                              lab_workspace_staging[0], length);
    }
    if (has_text && !duplicate && !lab_workspace_can_reserve(length, 0))
        return -1;
    if (flags & 2)
        lab_workspace_history_clear(mode, 1);
    if (!has_text || duplicate)
        return (int)state->count[direction];
    lab_workspace_append(state, direction, length);
    return (int)state->count[direction];
}

/* Copy before removal compacts the arena. */
int lab_workspace_history_pop(unsigned mode, unsigned direction)
{
    if (mode >= LAB_WS_MODES || direction >= 2)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (!state->count[direction])
        return 0;
    lab_workspace_entry_t *entry = &state->entries[direction][state->count[direction] - 1];
    unsigned length = entry->length;
    lab_workspace_copy(lab_workspace_result, lab_workspace_storage + entry->offset, length);
    lab_workspace_remove(state, direction, state->count[direction] - 1);
    return (int)length;
}

/* Navigation uses no await or host callback between its two state changes. */
int lab_workspace_navigate(unsigned mode, unsigned direction, unsigned length, int has_text)
{
    if (mode >= LAB_WS_MODES || direction >= 2 || !length || length > LAB_WS_BYTES)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (!state->count[direction])
        return 0;
    if (has_text && !lab_workspace_can_reserve(length, 0))
        return -1;
    int restored = lab_workspace_history_pop(mode, direction);
    if (has_text)
        lab_workspace_append(state, 1 - direction, length);
    return restored;
}

/* Persistence reconciliation is policy; storage reads and writes remain host I/O. */
int lab_workspace_prefer_local(int local_present, double local_time, double server_time, int server_present,
                               int server_is_default)
{
    return local_present && (local_time > server_time || !server_present || server_is_default);
}
