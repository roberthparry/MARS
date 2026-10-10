/**
 * @file lab_profile.h
 * @brief Private interface: calendar field ordering, defaults and persistence policy in C/WebAssembly.
 *
 * A directly indexed schema replaces repeated browser field lists. Ordered
 * date/range/year defaults preserve dependent fallbacks. Restore retains the
 * authored DateTime whitespace; capture normalises it. Fill only touches blank
 * defaultable controls; reset deliberately preserves town selection and the
 * almanac jurisdiction. Browser adapters still own DOM access and text transport.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_PROFILE_H
#define LAB_WASM_PROFILE_H

/**
 * @brief Return the number of fields in a calendar schema.
 * @param mode DateTime 5 or almanac 6.
 * @return Field count, or zero for another mode.
 */
unsigned lab_profile_count(unsigned mode);

/**
 * @brief Borrow a fixed schema string.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param part Zero for state key, one for DOM ID, two for bootstrap default key.
 * @return Static string, or an empty string for invalid indices.
 */
const char *lab_profile_text(unsigned mode, unsigned field, unsigned part);

/**
 * @brief Return the byte length of a fixed schema string.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param part Zero for state key, one for DOM ID, two for bootstrap default key.
 * @return Length excluding NUL, or zero for invalid indices.
 */
unsigned lab_profile_text_length(unsigned mode, unsigned field, unsigned part);

/**
 * @brief Return a field validation category.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @return Zero for plain text, one for date, two for jurisdiction, three for visibility.
 */
unsigned lab_profile_kind(unsigned mode, unsigned field);

/**
 * @brief Select a fallback source without parsing browser text.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @return Zero for the named bootstrap default, one for resolved date, two for its year.
 */
unsigned lab_profile_fallback(unsigned mode, unsigned field);

/**
 * @brief Select raw, normalised or fallback text for one ordered field.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param operation Restore 0, capture 1, fill blanks 2, or reset 3.
 * @param present Whether the raw value is non-empty.
 * @param nonblank Whether the normalised value is non-empty.
 * @param valid Whether host-classified date, jurisdiction or visibility is valid.
 * @return Source index: raw 0, normalised 1, fallback 2; -1 for invalid arguments.
 */
int lab_profile_choose(unsigned mode, unsigned field, unsigned operation, int present, int nonblank, int valid);

/**
 * @brief Report whether applying an operation may write this control.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @param operation Restore 0, capture 1, fill blanks 2, or reset 3.
 * @return One when writable, otherwise zero; town selection is restored separately.
 */
int lab_profile_write(unsigned mode, unsigned field, unsigned operation);

/**
 * @brief Decide whether a jurisdiction reply may replace the displayed offset.
 * @param mode Calendar mode, 5 or 6.
 * @param town_applied Whether a named town already supplied the location.
 * @param touched Whether the DateTime offset was edited by the user.
 * @param present Whether the displayed offset is non-blank.
 * @param automatic Whether it still equals the last automatic offset.
 * @param suggested Whether the reply supplied a non-empty offset.
 * @return One when replacement is allowed, otherwise zero.
 */
int lab_profile_accept_offset(unsigned mode, int town_applied, int touched, int present, int automatic, int suggested);

/**
 * @brief Return change-event actions for a schema field.
 * @param mode Calendar mode, 5 or 6.
 * @param field Zero-based field index.
 * @return Bit flags: date picker 1, update year/clear JDN 2, apply town 4,
 * clear custom town 8, refresh jurisdiction 16, refresh coordinates 32.
 * Invalid indices return zero.
 */
unsigned lab_profile_event(unsigned mode, unsigned field);

#endif
