# `bitset_t`

`bitset_t` is a dynamic, thread-safe bitset backed by a `uint64_t` arena. Bits are indexed from 0. The arena grows automatically — doubling its word count — whenever a mutating operation addresses a bit beyond the current capacity. Out-of-range reads return false without error.

---

## Examples

### Basic usage

```c
#include <stdio.h>
#include "bitset.h"

int main(void) {
    bitset_t *bs = bitset_create(0);

    bitset_set(bs, 0);
    bitset_set(bs, 5);
    bitset_set(bs, 63);
    bitset_set(bs, 200);

    printf("popcount: %zu\n", bitset_popcount(bs));

    for (size_t i = bitset_next_set(bs, 0);
         i != BITSET_NPOS;
         i = bitset_next_set(bs, i + 1))
        printf("  bit %zu is set\n", i);

    bitset_destroy(bs);
    return 0;
}
```

```text
popcount: 4
  bit 0 is set
  bit 5 is set
  bit 63 is set
  bit 200 is set
```

### Bitwise operations

```c
#include <stdio.h>
#include "bitset.h"

int main(void) {
    bitset_t *a = bitset_create(64);
    bitset_t *b = bitset_create(64);

    bitset_set_range(a, 0, 8);   /* bits 0–7 */
    bitset_set_range(b, 4, 12);  /* bits 4–11 */

    bitset_and(a, b);

    printf("AND result:");
    for (size_t i = bitset_next_set(a, 0); i != BITSET_NPOS; i = bitset_next_set(a, i + 1))
        printf(" %zu", i);
    printf("\n");

    bitset_destroy(a);
    bitset_destroy(b);
    return 0;
}
```

```text
AND result: 4 5 6 7
```

---

## API Reference

All declarations are in `include/bitset.h`.

### Constants

- `BITSET_NPOS` — sentinel value returned by `bitset_next_set()` when no further set bit exists (`(size_t)-1`)

### Types

- `bitset_t` — opaque dynamic bitset

### Lifecycle

- `bitset_t *bitset_create(size_t initial_capacity)` — create a new bitset with capacity for at least `initial_capacity` bits; all bits clear. Pass 0 to use the default initial capacity. Returns NULL on allocation failure.
- `void bitset_destroy(bitset_t *bs)` — free the bitset and all memory. Safe to call with NULL.
- `void bitset_clear(bitset_t *bs)` — set all bits to zero without freeing the arena.
- `bitset_t *bitset_clone(const bitset_t *bs)` — deep copy into a new independent bitset. Returns NULL on allocation failure.

### Capacity

- `size_t bitset_capacity(const bitset_t *bs)` — number of bits the bitset can hold without growing.

### Single-bit operations

- `bool bitset_set(bitset_t *bs, size_t index)` — set the bit at `index`. Grows if necessary. Returns false on allocation failure.
- `void bitset_unset(bitset_t *bs, size_t index)` — clear the bit at `index`. No-op if `index` is beyond capacity.
- `bool bitset_toggle(bitset_t *bs, size_t index)` — flip the bit at `index`. Grows if necessary. Returns false on allocation failure.
- `bool bitset_test(const bitset_t *bs, size_t index)` — true if the bit at `index` is set. Returns false if `index` is beyond capacity.

### Range operations

- `bool bitset_set_range(bitset_t *bs, size_t start, size_t end)` — set all bits in `[start, end)`. Grows if necessary. Returns false if `start >= end` or on allocation failure.
- `void bitset_unset_range(bitset_t *bs, size_t start, size_t end)` — clear all bits in `[start, end)`. Out-of-range indices are silently ignored.

### Queries

- `size_t bitset_popcount(const bitset_t *bs)` — count of set bits across the entire arena.
- `bool bitset_any(const bitset_t *bs)` — true if at least one bit is set.
- `bool bitset_none(const bitset_t *bs)` — true if no bits are set.
- `size_t bitset_next_set(const bitset_t *bs, size_t from)` — index of the next set bit at or after `from`, or `BITSET_NPOS` if none.

### Bitwise operations (in-place)

All binary operations lock both operands in a consistent order to avoid deadlock. `dst` grows to match `src` for OR and XOR; for AND, bits beyond `src`'s capacity are treated as zero and cleared in `dst`.

- `bool bitset_and(bitset_t *dst, const bitset_t *src)` — `dst &= src`. Returns false on allocation failure.
- `bool bitset_or(bitset_t *dst, const bitset_t *src)` — `dst |= src`. Returns false on allocation failure.
- `bool bitset_xor(bitset_t *dst, const bitset_t *src)` — `dst ^= src`. Returns false on allocation failure.
- `void bitset_not(bitset_t *bs)` — flip every bit in the current arena in-place.

## Design Notes

Each `uint64_t` word holds 64 bits. The arena doubles in word count whenever a requested index requires more space. `__builtin_popcountll` and `__builtin_ctzll` are used for efficient population count and next-set-bit search respectively.

`bitset_t` is fully thread-safe: every public function acquires the internal `PTHREAD_MUTEX_RECURSIVE` lock. Binary operations (`and`, `or`, `xor`) acquire both locks in pointer address order to prevent deadlock.

## Tradeoffs

The design favours simplicity and correctness over sparse-set optimisations. For very sparse bitsets over large index spaces an alternative structure (e.g. a sorted set or hash set of indices) may be more memory-efficient.
