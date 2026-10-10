/**
 * @file lab_memory.h
 * @brief Freestanding memory primitives required by the WebAssembly compiler.
 *
 * These standard C entry points satisfy compiler-generated aggregate copies and
 * initialisation without importing libc from JavaScript. They operate on valid
 * byte ranges in the module's linear memory; callers own bounds checking. This
 * is a private runtime interface, not a MARS library or browser host API.
 */
#ifndef LAB_WASM_MEMORY_H
#define LAB_WASM_MEMORY_H

#include <stddef.h>

/**
 * @brief Fill a byte range with the unsigned-byte conversion of value.
 * @param destination Writable range of at least length bytes.
 * @param value Byte value to repeat.
 * @param length Byte count; zero performs no memory access.
 * @return The original destination pointer.
 */
void *memset(void *destination, int value, size_t length);

/**
 * @brief Copy non-overlapping byte ranges.
 * @param destination Writable range of at least length bytes, disjoint from source.
 * @param source Readable range of at least length bytes.
 * @param length Byte count; zero performs no memory access.
 * @return The original destination pointer.
 */
void *memcpy(void *destination, const void *source, size_t length);

/**
 * @brief Copy byte ranges, including overlapping ranges.
 * @param destination Writable range of at least length bytes.
 * @param source Readable range of at least length bytes; may overlap destination.
 * @param length Byte count; zero performs no memory access.
 * @return The original destination pointer.
 */
void *memmove(void *destination, const void *source, size_t length);

/**
 * @brief Compare byte ranges lexicographically as unsigned bytes.
 * @param left First readable range of at least length bytes.
 * @param right Second readable range of at least length bytes.
 * @param length Byte count; zero performs no memory access.
 * @return Negative, zero or positive according to the first differing byte.
 */
int memcmp(const void *left, const void *right, size_t length);

#endif
