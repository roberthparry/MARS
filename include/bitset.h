#ifndef BITSET_H
#define BITSET_H

/**
 * @file bitset.h
 * @brief Dynamic, thread-safe bitset backed by a uint64_t arena.
 *
 * Bits are indexed from 0. The bitset grows automatically to accommodate
 * any index passed to a mutating function. Out-of-range reads return false.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Sentinel returned by bitset_next_set() when no further set bit exists.
 */
#define BITSET_NPOS ((size_t)-1)

/**
 * @brief Opaque dynamic bitset type.
 */
typedef struct _bitset_t bitset_t;

/* --- Lifecycle --- */

/**
 * @brief Create a new bitset with capacity for at least @p initial_capacity bits.
 *
 * All bits are initially clear. Pass 0 to use the default initial capacity.
 *
 * @param initial_capacity Hint for the minimum number of bits to preallocate.
 * @return Pointer to a new bitset, or NULL on allocation failure.
 */
bitset_t *bitset_create(size_t initial_capacity);

/**
 * @brief Destroy the bitset and free all memory. Safe to pass NULL.
 *
 * @param bs Pointer to the bitset.
 */
void bitset_destroy(bitset_t *bs);

/**
 * @brief Clear all bits without freeing the underlying arena.
 *
 * @param bs Pointer to the bitset.
 */
void bitset_clear(bitset_t *bs);

/**
 * @brief Create a deep copy of a bitset.
 *
 * @param bs Pointer to the source bitset.
 * @return Pointer to a new independent bitset, or NULL on allocation failure.
 */
bitset_t *bitset_clone(const bitset_t *bs);

/* --- Capacity --- */

/**
 * @brief Return the number of bits the bitset can hold without growing.
 *
 * @param bs Pointer to the bitset.
 * @return Current bit capacity.
 */
size_t bitset_capacity(const bitset_t *bs);

/* --- Single-bit operations --- */

/**
 * @brief Set the bit at @p index. Grows the bitset if necessary.
 *
 * @param bs    Pointer to the bitset.
 * @param index Bit index.
 * @return true on success, false on allocation failure.
 */
bool bitset_set(bitset_t *bs, size_t index);

/**
 * @brief Clear the bit at @p index. No-op if @p index is out of range.
 *
 * @param bs    Pointer to the bitset.
 * @param index Bit index.
 */
void bitset_unset(bitset_t *bs, size_t index);

/**
 * @brief Toggle the bit at @p index. Grows the bitset if necessary.
 *
 * @param bs    Pointer to the bitset.
 * @param index Bit index.
 * @return true on success, false on allocation failure.
 */
bool bitset_toggle(bitset_t *bs, size_t index);

/**
 * @brief Test the bit at @p index.
 *
 * Returns false if @p index is beyond the current capacity.
 *
 * @param bs    Pointer to the bitset.
 * @param index Bit index.
 * @return true if the bit is set, false otherwise.
 */
bool bitset_test(const bitset_t *bs, size_t index);

/* --- Range operations --- */

/**
 * @brief Set all bits in [@p start, @p end). Grows if necessary.
 *
 * @param bs    Pointer to the bitset.
 * @param start First bit index (inclusive).
 * @param end   One past the last bit index.
 * @return true on success, false on allocation failure or if start >= end.
 */
bool bitset_set_range(bitset_t *bs, size_t start, size_t end);

/**
 * @brief Clear all bits in [@p start, @p end). No-op for out-of-range indices.
 *
 * @param bs    Pointer to the bitset.
 * @param start First bit index (inclusive).
 * @param end   One past the last bit index.
 */
void bitset_unset_range(bitset_t *bs, size_t start, size_t end);

/* --- Queries --- */

/**
 * @brief Count the number of set bits.
 *
 * @param bs Pointer to the bitset.
 * @return Number of bits that are set.
 */
size_t bitset_popcount(const bitset_t *bs);

/**
 * @brief Return true if any bit is set.
 *
 * @param bs Pointer to the bitset.
 */
bool bitset_any(const bitset_t *bs);

/**
 * @brief Return true if no bits are set.
 *
 * @param bs Pointer to the bitset.
 */
bool bitset_none(const bitset_t *bs);

/**
 * @brief Find the next set bit at or after @p from.
 *
 * @param bs   Pointer to the bitset.
 * @param from Start search from this index (inclusive).
 * @return Index of the next set bit, or BITSET_NPOS if none found.
 */
size_t bitset_next_set(const bitset_t *bs, size_t from);

/* --- Bitwise operations (in-place) --- */

/**
 * @brief Bitwise AND: dst &= src. dst grows to match src if smaller.
 *
 * @param dst Pointer to the destination bitset.
 * @param src Pointer to the source bitset.
 * @return true on success, false on allocation failure.
 */
bool bitset_and(bitset_t *dst, const bitset_t *src);

/**
 * @brief Bitwise OR: dst |= src. dst grows to match src if smaller.
 *
 * @param dst Pointer to the destination bitset.
 * @param src Pointer to the source bitset.
 * @return true on success, false on allocation failure.
 */
bool bitset_or(bitset_t *dst, const bitset_t *src);

/**
 * @brief Bitwise XOR: dst ^= src. dst grows to match src if smaller.
 *
 * @param dst Pointer to the destination bitset.
 * @param src Pointer to the source bitset.
 * @return true on success, false on allocation failure.
 */
bool bitset_xor(bitset_t *dst, const bitset_t *src);

/**
 * @brief Bitwise NOT: flip every allocated bit in-place.
 *
 * @param bs Pointer to the bitset.
 */
void bitset_not(bitset_t *bs);

#endif /* BITSET_H */
