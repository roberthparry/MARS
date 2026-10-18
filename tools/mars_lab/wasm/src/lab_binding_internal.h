/**
 * @file lab_binding_internal.h
 * @brief Private value helpers shared by the native binding implementation.
 *
 * Binding editors, row reconciliation and synchronisation use these scoped-value
 * operations without parsing authored mathematics. Only binding implementation
 * files may include this header, with an explicit access grant at each include.
 * Other application modules use the public lab_binding.h API for presentation,
 * metadata copying and array counts. No browser handles survive a native call.
 */
#if !defined(MARS_LAB_BINDING_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_BINDING_INTERNAL_ACCESS."
#endif

#ifndef LAB_BINDING_INTERNAL_H
#define LAB_BINDING_INTERNAL_H

#include <stdbool.h>

/**
 * @brief Compare a scoped binding value with exact text for the binding policy modules.
 * @param value Scoped value to compare without mathematical interpretation.
 * @param text Null-terminated literal string.
 * @return True for an exact match; false otherwise.
 */
bool lab_binding_is(int value, const char *text);

/**
 * @brief Share the binding modules' existing truthy-value fallback.
 * @param value Scoped candidate value.
 * @param fallback Null-terminated fallback text.
 * @return The candidate handle when truthy, otherwise a scoped fallback string handle.
 */
int lab_binding_or(int value, const char *fallback);

/**
 * @brief Obtain a trimmed opaque binding name for presentation and reconciliation.
 * @param binding Scoped binding or integrator-row record.
 * @return Scoped trimmed name, or a scoped empty string when absent.
 */
int lab_binding_name(int binding);

#endif
