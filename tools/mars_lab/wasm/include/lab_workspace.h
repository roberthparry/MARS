/**
 * @file lab_workspace.h
 * @brief Private interface: freestanding worksheet mode, precision and bounded history ownership.
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
 */
#ifndef LAB_WASM_WORKSPACE_H
#define LAB_WASM_WORKSPACE_H

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

/** @brief Borrow staging buffer zero or one; invalid indices return null. */
unsigned char *lab_workspace_input(unsigned index);

/** @brief Return the maximum size of an editor or encoded snapshot (4 MiB). */
unsigned lab_workspace_capacity(void);

/** @brief Borrow the last returned snapshot/editor bytes until the next read. */
const unsigned char *lab_workspace_output(void);

/** @brief Match one of seven exact UTF-8 tokens in staging buffer zero; not a general text parser. */
unsigned lab_workspace_mode_id(unsigned length);

/** @brief Return the selected mode index, with expression at zero. */
unsigned lab_workspace_mode(void);

/** @brief Select a mode; return whether it changed, with invalid indices selecting expression. */
int lab_workspace_select(unsigned mode);

/** @brief Return a mode's initial precision in bits. */
unsigned lab_workspace_default_precision(unsigned mode);

/** @brief Store finite saved bit precision, clamped to 17..1048576, retaining the old value for non-finite input. */
unsigned lab_workspace_precision_set(unsigned mode, double bits);

/** @brief Return a mode's stored bit precision, preserving the legacy 17-bit saved-value minimum. */
unsigned lab_workspace_precision(unsigned mode);

/** @brief Return requested precision with the binary64 minimum of 53 bits, without changing saved state. */
unsigned lab_workspace_requested_precision(unsigned mode);

/** @brief Apply a user precision request with the binary64 minimum, retaining state for non-finite input. */
unsigned lab_workspace_request_precision(unsigned mode, double bits);

/** @brief Return significant decimal digits for a bit precision, including the binary64 17-digit policy. */
unsigned lab_workspace_digits(double bits);

/** @brief Return the next or previous precision step; positive direction means increase. */
unsigned lab_workspace_precision_step(double bits, int direction);

/** @brief Report whether precision can increase (positive direction) or decrease at the current request setting. */
int lab_workspace_precision_can_step(unsigned mode, int direction);

/** @brief Store editor bytes from staging buffer zero; reject invalid modes/lengths without mutation. */
int lab_workspace_editor_set(unsigned mode, unsigned length);

/** @brief Copy editor bytes into output, returning their length, or -1 for an invalid mode. */
int lab_workspace_editor_get(unsigned mode);

/**
 * @brief Store one active editor source field from staging buffer zero.
 * @param field Full text 0, displayed text 1, last evaluation input 2, goal source 3, or goal target 4.
 * @param length UTF-8 byte length, at most 4 MiB. Zero clears the field.
 * @return One on success; zero on invalid input or insufficient capacity without changing owned state.
 * @details These five fields are pinned in the shared 32 MiB arena and are never evicted as history.
 */
int lab_workspace_source_set(unsigned field, unsigned length);

/**
 * @brief Copy one active editor field into the shared output buffer.
 * @param field Field index 0..4, as for lab_workspace_source_set.
 * @return Byte count, or -1 for an invalid field without changing output.
 */
int lab_workspace_source_get(unsigned field);

/**
 * @brief Check exact equality between staged editor text and retained display bytes.
 * @param length Trimmed browser input byte count in staging buffer zero, at most 4 MiB.
 * @return One on equality, including two empty strings; zero otherwise or on invalid length.
 */
int lab_workspace_source_matches(unsigned length);

/**
 * @brief Copy retained source only when the staged input still matches its displayed text.
 * @param length Trimmed browser input byte count in staging buffer zero.
 * @param goal Non-zero selects goal source; zero selects full editor source and requires a non-empty display.
 * @return Copied byte count, or zero when absent, changed or invalid. Does not modify owned state.
 */
int lab_workspace_source_resolve(unsigned length, int goal);

/**
 * @brief Capture authored text, retaining the previous editor when blank; calendar modes use the supplied default.
 * @param mode Mode index in 0..6.
 * @param length Candidate byte length in staging buffer zero; already trimmed by the editor adapter.
 * @param default_length Default byte length in staging buffer one.
 * @return One on success, zero for invalid input or insufficient storage; owned state is unchanged on failure.
 */
int lab_workspace_editor_capture(unsigned mode, unsigned length, unsigned default_length);

/**
 * @brief Copy saved text or its default to output without changing the saved editor; calendars always use defaults.
 * @param mode Mode index in 0..6.
 * @param default_length Default byte length in staging buffer one.
 * @return Output byte length, or -1 for invalid input, leaving the previous output unchanged.
 */
int lab_workspace_editor_restore(unsigned mode, unsigned default_length);

/**
 * @brief Decide whether restoration should use the binding-aware editor rather than plain text.
 * @param mode Mode index in 0..6; invalid modes return zero.
 * @param has_bindings Whether native editor metadata reports bindings for the restored text.
 * @return One for expression/equation modes, or bound matrix/integrator text; zero otherwise.
 */
int lab_workspace_editor_bound(unsigned mode, int has_bindings);

/** @brief Compare staging buffers by length and bytes. */
int lab_workspace_equal(unsigned left_length, unsigned right_length);

/** @brief Commit the staged snapshot for one mode; reject empty/oversized encodings. */
int lab_workspace_commit(unsigned mode, unsigned length);

/** @brief Return a committed snapshot only when different from staging buffer zero; zero means absent/unchanged. */
int lab_workspace_previous(unsigned mode, unsigned length);

/** @brief Return the selected stack depth (zero back, one forward); invalid indices return zero. */
unsigned lab_workspace_history_count(unsigned mode, unsigned direction);

/** @brief Clear one mode's selected stack without affecting its committed snapshot or other modes. */
void lab_workspace_history_clear(unsigned mode, unsigned direction);

/**
 * @brief Push staged bytes, with optional adjacent deduplication and forward invalidation.
 * @details has_text is the host's measurement of the captured editor/summary text.
 * Blank snapshots are not pushed. Flags: bit zero deduplicates, bit one clears
 * forward history, including for blanks/duplicates. Returns -1 on invalid input,
 * otherwise the resulting depth. Each stack retains at most 128 entries. The
 * Mandatory forward clearing and entry-limit removal happen before extra byte
 * eviction, so their reclaimed bytes do not cause unnecessary loss of history.
 * The shared 32 MiB pool evicts oldest history entries, scanning mode order and back
 * before forward. Active editor and committed values are never evicted. If these
 * alone leave insufficient capacity, the operation fails without mutation.
 */
int lab_workspace_history_push(unsigned mode, unsigned direction, unsigned length, int has_text, unsigned flags);

/** @brief Pop one owned snapshot into output; zero means empty and -1 means invalid indices. */
int lab_workspace_history_pop(unsigned mode, unsigned direction);

/**
 * @brief Atomically move back/forward, saving the staged current state on the opposite stack when nonblank.
 * @details Validates before mutating. Returns restored snapshot length, zero for
 * an empty stack, or -1 for invalid input. Does not change committed state until
 * the subsequent evaluation explicitly commits its result. Capacity is checked
 * without mutation before popping; saving the current state then cannot fail.
 */
int lab_workspace_navigate(unsigned mode, unsigned direction, unsigned length, int has_text);

/** @brief Decide whether a nonempty local copy supersedes the server copy using timestamps/default status. */
int lab_workspace_prefer_local(int local_present, double local_time, double server_time, int server_present,
                               int server_is_default);

#endif
