/**
 * @file lab_profile.h
 * @brief Application API: calendar field ordering, defaults and persistence policy in C/WebAssembly.
 *
 * A directly indexed schema replaces repeated browser field lists. Ordered
 * date/range/year defaults preserve dependent fallbacks. Restore retains the
 * authored DateTime whitespace; capture normalises it. Fill only touches blank
 * defaultable controls; reset deliberately preserves town selection and the
 * almanac jurisdiction. Browser adapters still own DOM access and text transport.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean arguments crossing the WebAssembly i32 ABI must be canonical 0 or 1;
 * C callers use false or true.
 */
#ifndef LAB_WASM_PROFILE_H
#define LAB_WASM_PROFILE_H

#include <stdbool.h>

#include "lab_mode.h"

/** @brief String component of a calendar field's fixed schema entry. */
typedef enum {
    LAB_PROFILE_KEY = 0,          /**< Key in captured and saved calendar state. */
    LAB_PROFILE_ELEMENT = 1,      /**< DOM control identifier. */
    LAB_PROFILE_INITIAL = 2       /**< Bootstrap configuration key for the default value. */
} lab_profile_part_t;

/** @brief Validation applied to a calendar field before choosing its value. */
typedef enum {
    LAB_PROFILE_TEXT = 0,         /**< Plain text with no specialised validation. */
    LAB_PROFILE_DATE = 1,         /**< Date supported by the calendar input. */
    LAB_PROFILE_JURISDICTION = 2, /**< Code present in the jurisdiction catalogue. */
    LAB_PROFILE_VISIBILITY = 3    /**< Recognised visibility choice. */
} lab_profile_kind_t;

/** @brief Default source for a calendar field, resolved in schema order. */
typedef enum {
    LAB_PROFILE_DEFAULT = 0,      /**< Named bootstrap default. */
    LAB_PROFILE_RESOLVED_DATE = 1, /**< Previously resolved date. */
    LAB_PROFILE_RESOLVED_YEAR = 2 /**< Year extracted from the resolved date. */
} lab_profile_fallback_t;

/** @brief Calendar state operation shared by the schema and DOM projection. */
typedef enum {
    LAB_CALENDAR_RESTORE = 0, /**< Restore authored saved state. */
    LAB_CALENDAR_CAPTURE = 1, /**< Capture normalised current state. */
    LAB_CALENDAR_FILL = 2,    /**< Fill blank defaultable controls. */
    LAB_CALENDAR_RESET = 3    /**< Reset controls permitted by the schema. */
} lab_calendar_operation_t;

/** @brief Chosen text source for one calendar field. */
typedef enum {
    LAB_PROFILE_CHOICE_INVALID = -1, /**< Invalid mode, field or operation. */
    LAB_PROFILE_RAW = 0,             /**< Preserve the authored value. */
    LAB_PROFILE_NORMALISED = 1,      /**< Use the normalised and validated text. */
    LAB_PROFILE_FALLBACK = 2         /**< Use the resolved default. */
} lab_profile_choice_t;

/**
 * @brief Return the number of fields in a calendar schema.
 * @param mode DateTime 5 or almanac 6.
 * @return Field count, or zero for another mode.
 */
unsigned lab_profile_count(lab_mode_t mode);

/**
 * @brief Borrow a fixed schema string.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param part Schema component: state key, DOM identifier or bootstrap default key.
 * @return Static string, or an empty string for invalid indices.
 */
const char *lab_profile_text(lab_mode_t mode, unsigned field, lab_profile_part_t part);

/**
 * @brief Return the byte length of a fixed schema string.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param part Zero for state key, one for DOM ID, two for bootstrap default key.
 * @return Length excluding NUL, or zero for invalid indices.
 */
unsigned lab_profile_text_length(lab_mode_t mode, unsigned field, lab_profile_part_t part);

/**
 * @brief Read a schema field's validation category.
 * @param mode DateTime 5 or almanac 6.
 * @param field Zero-based index below lab_profile_count(mode).
 * @return Validation category; invalid mode/field pairs return LAB_PROFILE_TEXT.
 */
lab_profile_kind_t lab_profile_kind(lab_mode_t mode, unsigned field);

/**
 * @brief Select a schema field's fallback source without parsing browser text.
 * @param mode DateTime 5 or almanac 6.
 * @param field Zero-based index below lab_profile_count(mode).
 * @return Named bootstrap default 0, previously resolved date 1 or its year 2;
 * invalid mode/field pairs also return zero.
 */
lab_profile_fallback_t lab_profile_fallback(lab_mode_t mode, unsigned field);

/**
 * @brief Select raw, normalised or fallback text for one ordered field.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param operation Calendar restore, capture, fill-blanks or reset operation.
 * @param present True if the raw value is non-empty.
 * @param nonblank True if the normalised value is non-empty.
 * @param valid True if host-classified date, jurisdiction or visibility is valid.
 * @return Raw, normalised or fallback text choice; LAB_PROFILE_CHOICE_INVALID for invalid arguments.
 */
lab_profile_choice_t lab_profile_choose(lab_mode_t mode, unsigned field, lab_calendar_operation_t operation,
                                        bool present, bool nonblank, bool valid);

/**
 * @brief Report whether applying an operation may write this control.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param operation Restore 0, capture 1, fill blanks 2, or reset 3.
 * @return True when writable, otherwise false; town selection is restored separately.
 */
bool lab_profile_write(lab_mode_t mode, unsigned field, lab_calendar_operation_t operation);

/**
 * @brief Decide whether a jurisdiction reply may replace the displayed offset.
 * @param mode Calendar mode, 5 or 6.
 * @param town_applied True if a named town already supplied the location.
 * @param touched True if the DateTime offset was edited by the user.
 * @param present True if the displayed offset is non-blank.
 * @param automatic True if it still equals the last automatic offset.
 * @param suggested True if the reply supplied a non-empty offset.
 * @return True when replacement is allowed, otherwise false.
 */
bool lab_profile_accept_offset(lab_mode_t mode, bool town_applied, bool touched, bool present, bool automatic, bool suggested);

/**
 * @brief Return change-event actions for a schema field.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @return Bit flags: date picker 1, update year/clear JDN 2, apply town 4,
 * clear custom town 8, refresh jurisdiction 16, refresh coordinates 32.
 * Invalid indices return zero.
 */
unsigned lab_profile_event(lab_mode_t mode, unsigned field);

#endif
