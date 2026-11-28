/**
 * @file attributes-t.h
 * @brief Portable compiler annotations for public MARS interfaces.
 *
 * Supplies source-level migration diagnostics without changing an interface's availability, binary calling
 * convention or ownership. GCC and Clang emit deprecation diagnostics at call sites; other compilers
 * retain the declarations and their documented deprecation status. No implementation-mode flag disables warnings.
 */
#ifndef MARS_ATTRIBUTES_H
#define MARS_ATTRIBUTES_H

#if defined(__GNUC__) || defined(__clang__)
/** @brief Mark a retained interface as deprecated. @param message String literal naming its replacement. */
#define MARS_DEPRECATED(message) __attribute__((deprecated(message)))
#else
/** @brief Retain a documented deprecated interface without compiler diagnostics. @param message Migration text. */
#define MARS_DEPRECATED(message)
#endif

#endif
