/**
 * @file lab_workspace.c
 * @brief Freestanding worksheet mode, precision and bounded history ownership.
 *
 * Stores actual editor and codec snapshot bytes in WebAssembly, not host object
 * handles. DOM rendering, serialisation and persistence I/O belong to the host;
 * mode transitions, source/display matching, precision policy, history equality, eviction and navigation
 * belong here. Fixed storage uses just over 44 MiB; library byte operations require no host imports or allocation.
 * The bounded mode/entry scans below update offsets after compacting a byte arena;
 * no scan depends on unbounded user input or accumulated browsing history.
 */
#include <stdint.h>
#include <string.h>

#include "lab_mode.h"
#include "lab_workspace.h"

enum { LAB_WS_MODES = LAB_MODE_COUNT, LAB_WS_BYTES = 4194304, LAB_WS_ARENA = 33554432, LAB_WS_ENTRIES = 128 };

typedef struct {
    uint32_t offset, length;
} lab_workspace_entry_t;

typedef struct {
    lab_workspace_entry_t entries[2][LAB_WS_ENTRIES];
    lab_workspace_entry_t editor, committed;
    unsigned count[2], precision, revision;
} lab_workspace_mode_t;

static lab_workspace_mode_t lab_workspace_modes[LAB_WS_MODES];
static lab_workspace_entry_t lab_workspace_source[5];
static unsigned char lab_workspace_staging[2][LAB_WS_BYTES], lab_workspace_result[LAB_WS_BYTES];
static unsigned char lab_workspace_storage[LAB_WS_ARENA];
static unsigned lab_workspace_used;
static lab_mode_t lab_workspace_selected;
static const char *const lab_workspace_names[LAB_WS_MODES] = {"expression", "equation", "diffequation", "matrix",
                                                              "integrator", "datetime", "almanac"};

static bool lab_workspace_bytes_equal(const unsigned char *left, unsigned left_size, const unsigned char *right,
                                      unsigned right_size)
{
    return left_size == right_size && memcmp(left, right, left_size) == 0;
}

static void lab_workspace_remove_bytes(unsigned offset, unsigned length)
{
    unsigned end = offset + length;
    memmove(lab_workspace_storage + offset, lab_workspace_storage + end, lab_workspace_used - end);
    lab_workspace_used -= length;
    for (lab_workspace_source_t field = LAB_WORKSPACE_FULL_TEXT; field <= LAB_WORKSPACE_GOAL_TARGET; ++field)
        if (lab_workspace_source[field].length && lab_workspace_source[field].offset >= end)
            lab_workspace_source[field].offset -= length;
    for (lab_mode_t mode = LAB_MODE_EXPRESSION; (unsigned)mode < LAB_WS_MODES; ++mode) {
        lab_workspace_mode_t *state = &lab_workspace_modes[mode];
        if (state->editor.length && state->editor.offset >= end)
            state->editor.offset -= length;
        if (state->committed.length && state->committed.offset >= end)
            state->committed.offset -= length;
        for (lab_workspace_history_t side = LAB_WORKSPACE_BACK; side <= LAB_WORKSPACE_FORWARD; ++side)
            for (unsigned i = 0; i < state->count[side]; ++i)
                if (state->entries[side][i].offset >= end)
                    state->entries[side][i].offset -= length;
    }
}

static void lab_workspace_remove(lab_workspace_mode_t *state, lab_workspace_history_t direction, unsigned index)
{
    unsigned offset = state->entries[direction][index].offset;
    unsigned length = state->entries[direction][index].length;
    --state->count[direction];
    ++state->revision;
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
    for (lab_mode_t mode = LAB_MODE_EXPRESSION; (unsigned)mode < LAB_WS_MODES; ++mode)
        for (lab_workspace_history_t side = LAB_WORKSPACE_BACK; side <= LAB_WORKSPACE_FORWARD; ++side) {
            lab_workspace_mode_t *state = &lab_workspace_modes[mode];
            for (unsigned i = 0; i < state->count[side]; ++i)
                available += state->entries[side][i].length;
        }
    return length <= available;
}

/* Preflight has proved capacity; subsequent removals cannot invalidate that proof. */
static void lab_workspace_reclaim(unsigned length, unsigned replaced)
{
    for (lab_mode_t mode = LAB_MODE_EXPRESSION;
         (unsigned)mode < LAB_WS_MODES && length > LAB_WS_ARENA - lab_workspace_used + replaced; ++mode)
        for (lab_workspace_history_t side = LAB_WORKSPACE_BACK;
             side <= LAB_WORKSPACE_FORWARD && length > LAB_WS_ARENA - lab_workspace_used + replaced; ++side) {
            lab_workspace_mode_t *state = &lab_workspace_modes[mode];
            while (state->count[side] && length > LAB_WS_ARENA - lab_workspace_used + replaced)
                lab_workspace_remove(state, side, 0);
        }
}

static int lab_workspace_replace(lab_workspace_entry_t *entry, unsigned length)
{
    if (lab_workspace_bytes_equal(lab_workspace_storage + entry->offset, entry->length,
                                  lab_workspace_staging[LAB_WORKSPACE_INPUT], length))
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
    memcpy(lab_workspace_storage + lab_workspace_used, lab_workspace_staging[LAB_WORKSPACE_INPUT], length);
    lab_workspace_used += length;
    return 1;
}

/* A preflighted append cannot fail. Apply mandatory entry eviction before byte-budget eviction. */
static void lab_workspace_append(lab_workspace_mode_t *state, lab_workspace_history_t direction, unsigned length)
{
    ++state->revision;
    if (state->count[direction] == LAB_WS_ENTRIES)
        lab_workspace_remove(state, direction, 0);
    lab_workspace_reclaim(length, 0);
    lab_workspace_entry_t *entry = &state->entries[direction][state->count[direction]++];
    entry->offset = lab_workspace_used;
    entry->length = length;
    memcpy(lab_workspace_storage + lab_workspace_used, lab_workspace_staging[LAB_WORKSPACE_INPUT], length);
    lab_workspace_used += length;
}

/* Reset logical ownership; inaccessible old bytes need not be cleared. */
void lab_workspace_reset(void)
{
    lab_workspace_selected = LAB_MODE_EXPRESSION;
    lab_workspace_used = 0;
    for (lab_workspace_source_t field = LAB_WORKSPACE_FULL_TEXT; field <= LAB_WORKSPACE_GOAL_TARGET; ++field)
        lab_workspace_source[field].length = 0;
    for (lab_mode_t mode = LAB_MODE_EXPRESSION; (unsigned)mode < LAB_WS_MODES; ++mode) {
        lab_workspace_mode_t *state = &lab_workspace_modes[mode];
        state->count[LAB_WORKSPACE_BACK] = state->count[LAB_WORKSPACE_FORWARD] = 0;
        state->editor.length = state->committed.length = 0;
        ++state->revision;
        state->precision = lab_workspace_default_precision(mode);
    }
}

/* Expose bounded transfer storage only, never the state structure. */
unsigned char *lab_workspace_input(lab_workspace_buffer_t index)
{
    return (unsigned)index <= LAB_WORKSPACE_ALTERNATE ? lab_workspace_staging[index] : (unsigned char *)0;
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
lab_mode_t lab_workspace_mode_id(unsigned length)
{
    if (length > 12)
        return LAB_MODE_EXPRESSION;
    for (lab_mode_t mode = LAB_MODE_EXPRESSION; (unsigned)mode < LAB_WS_MODES; ++mode) {
        const char *name = lab_workspace_names[mode];
        if (strlen(name) == length && memcmp(name, lab_workspace_staging[LAB_WORKSPACE_INPUT], length) == 0)
            return mode;
    }
    return LAB_MODE_EXPRESSION;
}

/* Return the native selection. */
lab_mode_t lab_workspace_mode(void)
{
    return lab_workspace_selected;
}

/* Keep selection and change detection together. */
bool lab_workspace_select(lab_mode_t mode)
{
    if ((unsigned)mode >= LAB_WS_MODES)
        mode = LAB_MODE_EXPRESSION;
    bool changed = mode != lab_workspace_selected;
    lab_workspace_selected = mode;
    return changed;
}

/* Preserve the four mathematical and three low-precision defaults. */
unsigned lab_workspace_default_precision(lab_mode_t mode)
{
    return (unsigned)mode < LAB_MODE_INTEGRATOR ? 256 : 53;
}

/* Clamp saved and interactive settings to the Lab limits before converting to an integer. */
unsigned lab_workspace_precision_set(lab_mode_t mode, double bits)
{
    if ((unsigned)mode >= LAB_WS_MODES)
        return 0;
    if (bits != bits || bits > 1.7976931348623157e308 || bits < -1.7976931348623157e308)
        return lab_workspace_modes[mode].precision;
    unsigned precision = bits < 53 ? 53 : bits > 262144 ? 262144 : (unsigned)bits;
    lab_workspace_modes[mode].precision = precision;
    return precision;
}

/* Access precision without a host-side mutable mirror. */
unsigned lab_workspace_precision(lab_mode_t mode)
{
    return (unsigned)mode < LAB_WS_MODES ? lab_workspace_modes[mode].precision : 0;
}

/* Retain the binary64 floor even before a mode has initialised its precision. */
unsigned lab_workspace_requested_precision(lab_mode_t mode)
{
    unsigned bits = lab_workspace_precision(mode);
    return bits < 53 ? 53 : bits;
}

/* Interactive precision requests use the same limits as restored settings. */
unsigned lab_workspace_request_precision(lab_mode_t mode, double bits)
{
    if (bits == bits && bits >= -1.7976931348623157e308 && bits <= 1.7976931348623157e308 && bits < 53)
        bits = 53;
    return lab_workspace_precision_set(mode, bits);
}

/* Round bits * log10(2) upwards to obtain decimal digits, without a freestanding libm dependency. */
unsigned lab_workspace_digits(double bits)
{
    if (!(bits > 53))
        return 17;
    if (bits > 262144)
        bits = 262144;
    double digits = bits * 0.30102999566398119521;
    unsigned whole = (unsigned)digits;
    return whole + (digits > whole);
}

/* Follow binary64, double-double, then 192 bits and 64-bit steps. */
unsigned lab_workspace_precision_step(double bits, lab_workspace_precision_direction_t direction)
{
    unsigned current = !(bits >= 53) ? 53 : bits > 6656 ? 6656 : (unsigned)bits;
    if (direction > 0) {
        if (current < 106)
            return 106;
        if (current < 192)
            return 192;
        return current >= 6656 ? 6656 : (current / 64 + 1) * 64;
    }
    if (current <= 106)
        return 53;
    if (current <= 192)
        return 106;
    return ((current - 1) / 64) * 64;
}

/* Binary64 and double-double precede 192 bits and regular 64-bit slider steps. */
unsigned lab_workspace_precision_slider_bits(double position)
{
    unsigned index = !(position >= 0) ? 0 : position > 103 ? 103 : (unsigned)position;
    if (index < 2)
        return index ? 106 : 53;
    return (index + 1) * 64;
}

/* Round to the nearest offered precision without scanning the slider positions. */
unsigned lab_workspace_precision_slider_position(unsigned bits)
{
    if (bits <= 79)
        return 0;
    if (bits <= 149)
        return 1;
    if (bits < 192)
        return 2;
    return bits >= 6656 ? 103 : (bits + 31) / 64 - 1;
}

/* Control availability follows the same native request bounds as stepping. */
bool lab_workspace_precision_can_step(lab_mode_t mode, lab_workspace_precision_direction_t direction)
{
    unsigned bits = lab_workspace_requested_precision(mode);
    return (unsigned)mode < LAB_WS_MODES && (direction > 0 ? bits < 6656 : bits > 53);
}

/* Reject overflow rather than truncate mathematical editor text. */
bool lab_workspace_editor_set(lab_mode_t mode, unsigned length)
{
    if ((unsigned)mode >= LAB_WS_MODES || length > LAB_WS_BYTES)
        return false;
    return lab_workspace_replace(&lab_workspace_modes[mode].editor, length);
}

/* Copy editor bytes out of opaque ownership. */
int lab_workspace_editor_get(lab_mode_t mode)
{
    if ((unsigned)mode >= LAB_WS_MODES)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    memcpy(lab_workspace_result, lab_workspace_storage + state->editor.offset, state->editor.length);
    return (int)state->editor.length;
}

/* Source fields share the bounded arena and are pinned just like active editors. */
bool lab_workspace_source_set(lab_workspace_source_t field, unsigned length)
{
    if ((unsigned)field > LAB_WORKSPACE_GOAL_TARGET || length > LAB_WS_BYTES)
        return false;
    return lab_workspace_replace(&lab_workspace_source[field], length);
}

/* Copy exact authored bytes, never numerical or mathematical interpretations. */
int lab_workspace_source_get(lab_workspace_source_t field)
{
    if ((unsigned)field > LAB_WORKSPACE_GOAL_TARGET)
        return -1;
    const lab_workspace_entry_t *entry = &lab_workspace_source[field];
    memcpy(lab_workspace_result, lab_workspace_storage + entry->offset, entry->length);
    return (int)entry->length;
}

/* Compare the current trimmed browser input with the retained display bytes. */
bool lab_workspace_source_matches(unsigned length)
{
    const lab_workspace_entry_t *display = &lab_workspace_source[LAB_WORKSPACE_DISPLAY_TEXT];
    return length <= LAB_WS_BYTES &&
           lab_workspace_bytes_equal(lab_workspace_staging[LAB_WORKSPACE_INPUT], length, lab_workspace_storage + display->offset,
                                     display->length);
}

/* Resolve only an unchanged display; a changed editor must go through native presentation metadata. */
int lab_workspace_source_resolve(unsigned length, bool goal)
{
    lab_workspace_source_t field = goal ? LAB_WORKSPACE_GOAL_SOURCE : LAB_WORKSPACE_FULL_TEXT;
    if (!lab_workspace_source_matches(length) || (!goal && !lab_workspace_source[LAB_WORKSPACE_DISPLAY_TEXT].length))
        return 0;
    return lab_workspace_source_get(field);
}

/* Calendar summaries are regenerated from their forms, never captured as authored mathematics. */
bool lab_workspace_editor_capture(lab_mode_t mode, unsigned length, unsigned default_length)
{
    if ((unsigned)mode >= LAB_WS_MODES || length > LAB_WS_BYTES || default_length > LAB_WS_BYTES)
        return false;
    if (mode >= LAB_MODE_DATETIME) {
        memcpy(lab_workspace_staging[LAB_WORKSPACE_INPUT], lab_workspace_staging[LAB_WORKSPACE_ALTERNATE],
                           default_length);
        length = default_length;
    } else if (!length) {
        return true;
    }
    return lab_workspace_editor_set(mode, length);
}

/* Selecting a default is a read, so merely visiting a mode never overwrites its saved editor. */
int lab_workspace_editor_restore(lab_mode_t mode, unsigned default_length)
{
    if ((unsigned)mode >= LAB_WS_MODES || default_length > LAB_WS_BYTES)
        return -1;
    if ((unsigned)mode < LAB_MODE_DATETIME && lab_workspace_modes[mode].editor.length)
        return lab_workspace_editor_get(mode);
    memcpy(lab_workspace_result, lab_workspace_staging[LAB_WORKSPACE_ALTERNATE], default_length);
    return (int)default_length;
}

/* The first three editors always maintain bindings; structured editors require explicit bound metadata. */
bool lab_workspace_editor_bound(lab_mode_t mode, bool has_bindings)
{
    return (unsigned)mode < LAB_MODE_MATRIX || ((unsigned)mode < LAB_MODE_DATETIME && has_bindings);
}

/* Compare complete serialisations, including form fields and their ordering. */
bool lab_workspace_equal(unsigned left_length, unsigned right_length)
{
    return left_length <= LAB_WS_BYTES && right_length <= LAB_WS_BYTES &&
           lab_workspace_bytes_equal(lab_workspace_staging[LAB_WORKSPACE_INPUT], left_length,
                                     lab_workspace_staging[LAB_WORKSPACE_ALTERNATE], right_length);
}

/* Commit owns a copy and cannot be changed by later host mutations. */
bool lab_workspace_commit(lab_mode_t mode, unsigned length)
{
    if ((unsigned)mode >= LAB_WS_MODES || !length || length > LAB_WS_BYTES)
        return false;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    bool changed = !lab_workspace_bytes_equal(lab_workspace_storage + state->committed.offset,
                                               state->committed.length, lab_workspace_staging[0], length);
    bool ok = lab_workspace_replace(&state->committed, length);
    if (ok && changed) ++state->revision;
    return ok;
}

/* Revision polling copies no snapshot bytes. */
unsigned lab_workspace_history_revision(lab_mode_t mode)
{
    return (unsigned)mode < LAB_WS_MODES ? lab_workspace_modes[mode].revision : 0;
}

/* Read-only enumeration preserves the live navigation cursor. */
int lab_workspace_history_read(lab_mode_t mode, unsigned side, unsigned index)
{
    if ((unsigned)mode >= LAB_WS_MODES || side > 2) return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (side < 2 && index >= state->count[side]) return 0;
    lab_workspace_entry_t entry = side == 2 ? state->committed : state->entries[side][index];
    memcpy(lab_workspace_result, lab_workspace_storage + entry.offset, entry.length);
    return (int)entry.length;
}

/* Validate and reserve the whole replacement before altering any retained snapshot. */
bool lab_workspace_history_replace(lab_mode_t mode, unsigned back, unsigned forward, unsigned length)
{
    if ((unsigned)mode >= LAB_WS_MODES || back > LAB_WS_ENTRIES || forward > LAB_WS_ENTRIES || length > LAB_WS_BYTES)
        return false;
    lab_workspace_entry_t records[1 + 2 * LAB_WS_ENTRIES];
    unsigned position = 0, total = 0, count = 1 + back + forward;
    for (unsigned i = 0; i < count; ++i) {
        if (length - position < 4) return false;
        const unsigned char *p = lab_workspace_staging[0] + position;
        unsigned size = (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
        position += 4;
        if (!size || size > length - position) return false;
        records[i] = (lab_workspace_entry_t){position, size};
        position += size;
        total += size;
    }
    if (position != length) return false;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (!lab_workspace_can_reserve(total, state->committed.length)) return false;
    lab_workspace_history_clear(mode, LAB_WORKSPACE_BACK);
    lab_workspace_history_clear(mode, LAB_WORKSPACE_FORWARD);
    unsigned old_length = state->committed.length, old_offset = state->committed.offset;
    state->committed.length = 0;
    if (old_length) lab_workspace_remove_bytes(old_offset, old_length);
    lab_workspace_reclaim(total, 0);
    for (unsigned i = 0; i < count; ++i) {
        lab_workspace_entry_t *entry = i == 0 ? &state->committed : i <= back ?
            &state->entries[0][state->count[0]++] : &state->entries[1][state->count[1]++];
        *entry = (lab_workspace_entry_t){lab_workspace_used, records[i].length};
        memcpy(lab_workspace_storage + lab_workspace_used, lab_workspace_staging[0] + records[i].offset, records[i].length);
        lab_workspace_used += records[i].length;
    }
    ++state->revision;
    return true;
}

/* Unchanged evaluations do not create an extra history item. */
int lab_workspace_previous(lab_mode_t mode, unsigned length)
{
    if ((unsigned)mode >= LAB_WS_MODES || !length || length > LAB_WS_BYTES)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (!state->committed.length ||
        lab_workspace_bytes_equal(lab_workspace_storage + state->committed.offset, state->committed.length,
                                  lab_workspace_staging[LAB_WORKSPACE_INPUT], length))
        return 0;
    memcpy(lab_workspace_result, lab_workspace_storage + state->committed.offset, state->committed.length);
    return (int)state->committed.length;
}

/* Stack indices are direct and bounded. */
unsigned lab_workspace_history_count(lab_mode_t mode, lab_workspace_history_t direction)
{
    return (unsigned)mode < LAB_WS_MODES && (unsigned)direction <= LAB_WORKSPACE_FORWARD
               ? lab_workspace_modes[mode].count[direction]
               : 0;
}

/* Clearing forward history retains back history and committed state. */
void lab_workspace_history_clear(lab_mode_t mode, lab_workspace_history_t direction)
{
    if ((unsigned)mode >= LAB_WS_MODES || (unsigned)direction > LAB_WORKSPACE_FORWARD)
        return;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    while (state->count[direction])
        lab_workspace_remove(state, direction, state->count[direction] - 1);
}

/* Native policy owns blank suppression, deduplication, invalidation and bounded eviction. */
int lab_workspace_history_push(lab_mode_t mode, lab_workspace_history_t direction, unsigned length, bool has_text,
                               unsigned flags)
{
    if ((unsigned)mode >= LAB_WS_MODES || (unsigned)direction > LAB_WORKSPACE_FORWARD || !length ||
        length > LAB_WS_BYTES || flags > 3)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    int duplicate = 0;
    if ((flags & 1) && state->count[direction]) {
        lab_workspace_entry_t *last = &state->entries[direction][state->count[direction] - 1];
        duplicate = lab_workspace_bytes_equal(lab_workspace_storage + last->offset, last->length,
                                              lab_workspace_staging[LAB_WORKSPACE_INPUT], length);
    }
    if (has_text && !duplicate && !lab_workspace_can_reserve(length, 0))
        return -1;
    if (flags & 2)
        lab_workspace_history_clear(mode, LAB_WORKSPACE_FORWARD);
    if (!has_text || duplicate)
        return (int)state->count[direction];
    lab_workspace_append(state, direction, length);
    return (int)state->count[direction];
}

/* Copy before removal compacts the arena. */
int lab_workspace_history_pop(lab_mode_t mode, lab_workspace_history_t direction)
{
    if ((unsigned)mode >= LAB_WS_MODES || (unsigned)direction > LAB_WORKSPACE_FORWARD)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (!state->count[direction])
        return 0;
    lab_workspace_entry_t *entry = &state->entries[direction][state->count[direction] - 1];
    unsigned length = entry->length;
    memcpy(lab_workspace_result, lab_workspace_storage + entry->offset, length);
    lab_workspace_remove(state, direction, state->count[direction] - 1);
    return (int)length;
}

/* Navigation uses no await or host callback between its two state changes. */
int lab_workspace_navigate(lab_mode_t mode, lab_workspace_history_t direction, unsigned length, bool has_text)
{
    if ((unsigned)mode >= LAB_WS_MODES || (unsigned)direction > LAB_WORKSPACE_FORWARD || !length || length > LAB_WS_BYTES)
        return -1;
    lab_workspace_mode_t *state = &lab_workspace_modes[mode];
    if (!state->count[direction])
        return 0;
    if (has_text && !lab_workspace_can_reserve(length, 0))
        return -1;
    int restored = lab_workspace_history_pop(mode, direction);
    if (has_text)
        lab_workspace_append(state, direction == LAB_WORKSPACE_BACK ? LAB_WORKSPACE_FORWARD : LAB_WORKSPACE_BACK,
                             length);
    return restored;
}

/* Persistence reconciliation is policy; storage reads and writes remain host I/O. */
bool lab_workspace_prefer_local(bool local_present, double local_time, double server_time, bool server_present,
                                bool server_is_default)
{
    return local_present && (local_time > server_time || !server_present || server_is_default);
}
