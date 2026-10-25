/**
 * @file lab_workspace.h
 * @brief Application API: freestanding worksheet mode, precision and bounded history ownership.
 *
 * Stores actual editor and codec snapshot bytes in WebAssembly, not host object
 * handles. DOM rendering, serialisation and persistence I/O belong to the host;
 * mode transitions, source/display matching, precision policy, history equality, eviction and navigation
 * belong here. Fixed storage uses just over 44 MiB and requires no libc or imports.
 * Bounded mode/entry scans update offsets after compacting a byte arena;
 * no scan depends on unbounded user input or accumulated browsing history.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean arguments crossing the WebAssembly i32 ABI must be canonical 0 or 1;
 * C callers use false or true.
 */
#ifndef LAB_WASM_WORKSPACE_H
#define LAB_WASM_WORKSPACE_H

#include <stdbool.h>

#include "lab_mode.h"

/** @brief Fixed transfer buffer used to stage text or encoded snapshots. */
typedef enum {
    LAB_WORKSPACE_INPUT = 0,     /**< Primary input or left-hand comparison value. */
    LAB_WORKSPACE_ALTERNATE = 1  /**< Default text or right-hand comparison value. */
} lab_workspace_buffer_t;

/** @brief Pinned editor-source field retained independently of history. */
typedef enum {
    LAB_WORKSPACE_FULL_TEXT = 0,    /**< Complete native editor source. */
    LAB_WORKSPACE_DISPLAY_TEXT = 1, /**< Text currently displayed in the editor. */
    LAB_WORKSPACE_LAST_INPUT = 2,   /**< Last evaluated input. */
    LAB_WORKSPACE_GOAL_SOURCE = 3,  /**< Source retained for goal seeking. */
    LAB_WORKSPACE_GOAL_TARGET = 4   /**< Goal-seeking target text. */
} lab_workspace_source_t;

/** @brief History stack identity; these ABI values are not signed navigation deltas. */
typedef enum {
    LAB_WORKSPACE_BACK = 0,   /**< Older worksheet snapshots. */
    LAB_WORKSPACE_FORWARD = 1 /**< Snapshots ahead of the restored worksheet. */
} lab_workspace_history_t;

/** @brief Signed precision-step request; zero and all negative values decrease precision. */
typedef enum {
    LAB_WORKSPACE_PRECISION_DECREASE = -1, /**< Move to the preceding offered precision. */
    LAB_WORKSPACE_PRECISION_INCREASE = 1   /**< Move to the following offered precision. */
} lab_workspace_precision_direction_t;

/* Workspace: C owns seven modes, their precision, editor bytes, committed snapshots and
 * bounded back/forward histories, plus active source/display, last-input and goal-seek bytes.
 * A 32 MiB pool retains editor, committed and
 * history data, with at most 4 MiB per value. The host supplies UTF-8 editor bytes and opaque
 * codec snapshots, and copies returned bytes before another call. No host handles
 * are retained. Calls are synchronous and confined to the browser main thread.
 * This is a tool-private ABI, not an installed MARS library interface.
 */

/** @brief Reset every mode, precision and owned byte store to its initial state. */
void lab_workspace_reset(void);

/**
 * @brief Borrow a writable workspace transfer buffer.
 * @param index Primary or alternate staging buffer; other values are rejected.
 * @return Pointer to a fixed lab_workspace_capacity()-byte buffer, or null for another index.
 * Consumers copy staged bytes; callers must not write beyond the capacity.
 */
unsigned char *lab_workspace_input(lab_workspace_buffer_t index);

/**
 * @brief Report the capacity of each editor/snapshot transfer buffer.
 * @return Maximum byte count, 4 MiB; encoded values are length-delimited rather than NUL-terminated.
 */
unsigned lab_workspace_capacity(void);

/**
 * @brief Borrow the most recently copied editor or snapshot bytes.
 * @return Shared output buffer; use the length returned by the operation that filled it and copy before another read.
 */
const unsigned char *lab_workspace_output(void);

/**
 * @brief Match an exact UTF-8 mode token staged in buffer zero.
 * @param length Token byte count in staging buffer zero, excluding any terminator; counts above twelve are rejected.
 * @return Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * Unrecognised tokens return expression 0; this function does not select the mode.
 */
lab_mode_t lab_workspace_mode_id(unsigned length);

/**
 * @brief Read the selected worksheet mode.
 * @return Current mode index in 0..6, with expression at zero.
 */
lab_mode_t lab_workspace_mode(void);

/**
 * @brief Select a worksheet mode and report whether it changed.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @return True when selection changed; false otherwise. Invalid indices select expression.
 */
bool lab_workspace_select(lab_mode_t mode);

/**
 * @brief Read a mode's initial arithmetic precision.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @return 256 bits for indices 0..3, or 53 bits for all other indices.
 */
unsigned lab_workspace_default_precision(lab_mode_t mode);

/**
 * @brief Store finite saved precision, clamping to the current Lab limits.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param bits Requested binary precision; finite fractions truncate after clamping to 53..262144.
 * @return Stored bit count, unchanged for non-finite input; zero for an invalid mode.
 */
unsigned lab_workspace_precision_set(lab_mode_t mode, double bits);

/**
 * @brief Read a mode's stored binary precision.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @return Stored bit count, or zero for an invalid mode; saved settings use a minimum of 53 bits.
 */
unsigned lab_workspace_precision(lab_mode_t mode);

/**
 * @brief Read requested precision with the binary64 floor without changing saved state.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @return Stored bit count raised to at least 53, including for an invalid or uninitialised mode.
 */
unsigned lab_workspace_requested_precision(lab_mode_t mode);

/**
 * @brief Apply an interactive precision request within the Lab limits.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param bits Requested binary precision; finite fractions truncate after clamping to 53..262144.
 * @return Stored bit count, unchanged for non-finite input; zero for an invalid mode.
 */
unsigned lab_workspace_request_precision(lab_mode_t mode, double bits);

/**
 * @brief Convert binary precision to the displayed significant decimal digit count.
 * @param bits Binary precision; values at or below 53 and NaN use the binary64 policy, values above 262144 clamp.
 * @return 17 for the binary64 policy; otherwise ceil(bits * log10(2)) after clamping.
 */
unsigned lab_workspace_digits(double bits);

/**
 * @brief Step along the slider's offered binary precisions.
 * @param bits Current binary precision, truncated and clamped to 53..6656 before stepping; NaN selects 53.
 * @param direction Positive increases; zero or negative decreases.
 * @return Next precision in 53, 106, 192, then 64-bit increments up to 6656; endpoints do not wrap.
 */
unsigned lab_workspace_precision_step(double bits, lab_workspace_precision_direction_t direction);

/**
 * @brief Map a slider position to its offered binary precision.
 * @param position Index, truncated and clamped to 0..103; NaN selects zero.
 * @return 53 bits at zero, 106 at one, then 192 through 6656 in 64-bit steps.
 */
unsigned lab_workspace_precision_slider_bits(double position);

/**
 * @brief Find the nearest offered precision without changing saved state.
 * @param bits Arbitrary binary precision, including custom requests above the slider ceiling.
 * @return Slider index in 0..103, clamped at either end; exact midpoint ties choose the lower precision.
 */
unsigned lab_workspace_precision_slider_position(unsigned bits);

/**
 * @brief Check whether an offered precision step is available.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param direction Positive tests increase below 6656 bits; zero or negative tests decrease above 53 bits.
 * @return True if stepping is possible, false at the corresponding limit or for an invalid mode.
 */
bool lab_workspace_precision_can_step(lab_mode_t mode, lab_workspace_precision_direction_t direction);

/**
 * @brief Copy staged editor bytes into native ownership.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param length UTF-8 byte count in staging buffer zero, at most 4 MiB; zero clears the editor.
 * @return True on success; false for invalid input or insufficient capacity, without changing the editor.
 */
bool lab_workspace_editor_set(lab_mode_t mode, unsigned length);

/**
 * @brief Copy a mode's retained editor into the shared output buffer.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @return Byte count in lab_workspace_output(), including zero for an empty editor, or -1 for an invalid mode.
 */
int lab_workspace_editor_get(lab_mode_t mode);

/**
 * @brief Store one active editor source field from staging buffer zero.
 * @param field Retained full text, displayed text, last input, goal source or goal target.
 * @param length UTF-8 byte length, at most 4 MiB. Zero clears the field.
 * @return True on success; false on invalid input or insufficient capacity without changing owned state.
 * @details These five fields are pinned in the shared 32 MiB arena and are never evicted as history.
 */
bool lab_workspace_source_set(lab_workspace_source_t field, unsigned length);

/**
 * @brief Copy one active editor field into the shared output buffer.
 * @param field Retained source field, as for lab_workspace_source_set.
 * @return Byte count, or -1 for an invalid field without changing output.
 */
int lab_workspace_source_get(lab_workspace_source_t field);

/**
 * @brief Check exact equality between staged editor text and retained display bytes.
 * @param length Trimmed browser input byte count in staging buffer zero, at most 4 MiB.
 * @return True on equality, including two empty strings; false otherwise or on invalid length.
 */
bool lab_workspace_source_matches(unsigned length);

/**
 * @brief Copy retained source only when the staged input still matches its displayed text.
 * @param length Trimmed browser input byte count in staging buffer zero.
 * @param goal True selects goal source; false selects full editor source and requires a non-empty display.
 * @return Copied byte count, or zero when absent, changed or invalid. Does not modify owned state.
 */
int lab_workspace_source_resolve(unsigned length, bool goal);

/**
 * @brief Capture authored text, retaining the previous editor when blank; calendar modes use the supplied default.
 * @param mode Mode index in 0..6.
 * @param length Candidate byte length in staging buffer zero; already trimmed by the editor adapter.
 * @param default_length Default byte length in staging buffer one.
 * @return True on success, false for invalid input or insufficient storage; owned state is unchanged on failure.
 */
bool lab_workspace_editor_capture(lab_mode_t mode, unsigned length, unsigned default_length);

/**
 * @brief Copy saved text or its default to output without changing the saved editor; calendars always use defaults.
 * @param mode Mode index in 0..6.
 * @param default_length Default byte length in staging buffer one.
 * @return Output byte length, or -1 for invalid input, leaving the previous output unchanged.
 */
int lab_workspace_editor_restore(lab_mode_t mode, unsigned default_length);

/**
 * @brief Decide whether restoration needs a binding-aware editor.
 * @param mode Worksheet index in 0..6; invalid modes return false.
 * @param has_bindings True when native editor metadata reports bindings in the restored text.
 * @return True for expression, equation and differential-equation modes, or bound matrix/integrator text;
 * false for calendar modes, unbound structured editors and invalid modes.
 */
bool lab_workspace_editor_bound(lab_mode_t mode, bool has_bindings);

/**
 * @brief Compare the two complete staged byte sequences.
 * @param left_length Byte count in staging buffer zero, at most lab_workspace_capacity().
 * @param right_length Byte count in staging buffer one, at most lab_workspace_capacity().
 * @return True when lengths and bytes match, including empty inputs; false for a mismatch or oversized input.
 */
bool lab_workspace_equal(unsigned left_length, unsigned right_length);

/**
 * @brief Retain a copy of the staged snapshot as one mode's committed state.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param length Encoded snapshot byte count in staging buffer zero, from one to lab_workspace_capacity().
 * @return True on success; false for invalid input or insufficient capacity, without changing the committed snapshot.
 */
bool lab_workspace_commit(lab_mode_t mode, unsigned length);

/**
 * @brief Copy a committed snapshot only when it differs from staged current state.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param length Current encoded snapshot byte count in staging buffer zero, from one to lab_workspace_capacity().
 * @return Committed byte count copied into lab_workspace_output(), zero when absent or identical,
 * or -1 for invalid mode or length.
 */
int lab_workspace_previous(lab_mode_t mode, unsigned length);

/**
 * @brief Read a mode's history stack depth.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param direction Back stack 0 or forward stack 1.
 * @return Number of entries, at most 128; zero for invalid indices.
 */
unsigned lab_workspace_history_count(lab_mode_t mode, lab_workspace_history_t direction);

/** @brief Return a mode's mutation counter for incremental persistence; invalid modes return zero. */
unsigned lab_workspace_history_revision(lab_mode_t mode);

/** @brief Copy an indexed snapshot to the output buffer without mutation; side 2 selects the committed state. */
int lab_workspace_history_read(lab_mode_t mode, unsigned side, unsigned index);

/**
 * @brief Atomically replace one mode's committed state and two history stacks from staged bytes.
 * @param mode Valid worksheet mode.
 * @param back Number of oldest-first backward records, at most 128.
 * @param forward Number of oldest-first forward records, at most 128.
 * @param length Staged byte count: committed record followed by both stacks, each prefixed by a little-endian u32 size.
 * @return True on success; malformed lengths or insufficient space leave existing state untouched.
 * @details The caller validates the contained Protobuf snapshots before staging. Other modes' oldest
 * history entries may be evicted under the existing shared arena budget, but pinned sources remain intact.
 */
bool lab_workspace_history_replace(lab_mode_t mode, unsigned back, unsigned forward, unsigned length);

/**
 * @brief Clear one history stack without affecting committed or other-mode state.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param direction Back stack 0 or forward stack 1; invalid mode/direction pairs are ignored.
 */
void lab_workspace_history_clear(lab_mode_t mode, lab_workspace_history_t direction);

/**
 * @brief Push a staged snapshot with optional adjacent deduplication and forward invalidation.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param direction Destination stack: back 0 or forward 1.
 * @param length Encoded snapshot byte count in staging buffer zero, from one to lab_workspace_capacity().
 * @param has_text True when the captured editor/summary contains text; false suppresses the push.
 * @param flags Bit 0 (1) deduplicates against the stack top; bit 1 (2) clears forward history, even for
 * blank or duplicate snapshots. Only combinations 0..3 are accepted.
 * @return Resulting stack depth, or -1 for invalid input or insufficient capacity without mutation.
 * @details Each stack retains at most 128 entries. Mandatory forward clearing and entry-limit removal
 * precede extra byte eviction. The shared 32 MiB pool evicts oldest history entries, scanning mode order
 * and back before forward. Active editor and committed values are never evicted.
 */
int lab_workspace_history_push(lab_mode_t mode, lab_workspace_history_t direction, unsigned length, bool has_text,
                               unsigned flags);

/**
 * @brief Remove one history snapshot and copy it to the shared output buffer.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param direction Source stack: back 0 or forward 1.
 * @return Snapshot byte count in lab_workspace_output(), zero for an empty stack, or -1 for invalid indices.
 */
int lab_workspace_history_pop(lab_mode_t mode, lab_workspace_history_t direction);

/**
 * @brief Move through history and save nonblank staged current state on the opposite stack.
 * @param mode Worksheet index: expression 0, equation 1, differential equation 2, matrix 3, integrator 4,
 * DateTime 5 or almanac 6.
 * @param direction Navigation destination: back stack 0 or forward stack 1.
 * @param length Current snapshot byte count in staging buffer zero, from one to lab_workspace_capacity().
 * @param has_text True saves the current snapshot to the opposite stack; false suppresses that save.
 * @return Restored snapshot byte count in lab_workspace_output(), zero for an empty destination stack,
 * or -1 for invalid input or insufficient capacity.
 * @details Checks capacity before mutation; saving current state cannot fail after popping.
 * Committed state remains unchanged until a subsequent evaluation commits its result.
 */
int lab_workspace_navigate(lab_mode_t mode, lab_workspace_history_t direction, unsigned length, bool has_text);

/**
 * @brief Decide whether a non-empty local editor supersedes the server copy.
 * @param local_present True when local editor text is present.
 * @param local_time Local saved timestamp in the same clock units as server_time, normally Unix milliseconds.
 * @param server_time Server saved timestamp, normally Unix milliseconds.
 * @param server_present True when server editor text is present.
 * @param server_is_default True when server text is the configured initial placeholder.
 * @return True when local text exists and is newer, or the server copy is absent/default; false otherwise.
 */
bool lab_workspace_prefer_local(bool local_present, double local_time, double server_time, bool server_present,
                               bool server_is_default);

#endif
