/* set.h - generic value-set container with dense arena and lazy sorting
 *
 * Design:
 *   - Elements stored inline in a dense arena (no holes, compacted on removal)
 *   - Each element slot stores its precomputed hash (HF2)
 *   - Hash table buckets store only indices into the arena (H1)
 *   - Unsorted access is arena order (B1)
 *   - Sorted access is lazy: rebuilt on demand, invalidated on mutation (S1)
 *   - Element-level clone/destroy callbacks (C1)
 *   - Slot layout is private (E1)
 */

#ifndef SET_H
#define SET_H

#include <stdbool.h>
#include <stddef.h>

/* Hash function for elements.
 *
 * The pointer points to the element's bytes (not a pointer to a pointer).
 * The implementation must be consistent with the comparison function:
 *   if cmp(a, b) == 0 then hash(a) == hash(b)
 */
typedef size_t (*set_hash_fn)(const void *elem);

/* Comparison function for elements.
 *
 * Must return:
 *   < 0  if a < b
 *   = 0  if a == b
 *   > 0  if a > b
 *
 * Used both for equality (cmp(a, b) == 0) and for sorted iteration.
 */
typedef int (*set_cmp_fn)(const void *a, const void *b);

/* Element-level clone function.
 *
 * Copies a single element from src to dst.
 *   - dst points to storage inside the set's arena
 *   - src points to the caller's element (for add) or another arena element
 *
 * If clone_fn is NULL, the set will use memcpy(dst, src, elem_size).
 */
typedef void (*set_clone_fn)(void *dst, const void *src);

/* Element-level destroy function.
 *
 * Destroys a single element stored inside the set's arena.
 *   - elem points to the element's storage inside the arena
 *
 * If destroy_fn is NULL, the set will do nothing when elements are removed
 * or when the set is cleared/destroyed.
 */
typedef void (*set_destroy_fn)(void *elem);

/* Opaque set type. */
typedef struct set set_t;

/* Create a new set.
 *
 * Parameters:
 *   elem_size   - size in bytes of each element stored in the set
 *   hash        - hash function for elements (must not be NULL)
 *   cmp         - comparison function for elements (must not be NULL)
 *   clone       - element-level clone function (may be NULL for shallow copy)
 *   destroy     - element-level destroy function (may be NULL for no-op)
 *
 * Returns:
 *   A pointer to a newly allocated set, or NULL on allocation failure.
 *
 * Semantics:
 *   - The set takes ownership of the hash, cmp, clone, and destroy callbacks
 *     for its lifetime (it does not free them, but assumes they remain valid).
 *   - Elements are stored by value, inline in an internal arena.
 *   - If clone is NULL, elements are copied with memcpy.
 *   - If destroy is NULL, elements are not individually destroyed.
 */
set_t *set_create(size_t elem_size,
                  set_hash_fn hash,
                  set_cmp_fn cmp,
                  set_clone_fn clone,
                  set_destroy_fn destroy);

/* Destroy a set and free all associated memory.
 *
 * If a destroy_fn was provided at creation time, it is called once for each
 * element currently stored in the set before the memory is freed.
 *
 * It is safe to pass NULL; in that case, this function does nothing.
 */
void set_destroy(set_t *set);

/* Remove all elements from the set.
 *
 * After this call:
 *   - set_get_size(set) == 0
 *   - The set remains usable for further insertions.
 *
 * If a destroy_fn was provided, it is called once for each removed element.
 */
void set_clear(set_t *set);

/* Get the number of elements currently stored in the set. */
size_t set_get_size(const set_t *set);

/* Check whether the set contains an element equal to 'elem'.
 *
 * Returns:
 *   true  if an equal element is present
 *   false otherwise
 *
 * Equality is determined by cmp(a, b) == 0.
 */
bool set_contains(const set_t *set, const void *elem);

/* Add an element to the set.
 *
 * Parameters:
 *   elem - pointer to the element to insert (not stored by reference)
 *
 * Returns:
 *   true  if the element was newly inserted
 *   false if an equal element was already present (no change to the set)
 *
 * Behaviour:
 *   - If an equal element already exists (cmp(existing, elem) == 0),
 *     the set is unchanged and false is returned.
 *   - Otherwise, a new element is created in the arena:
 *       * If clone_fn was provided, clone_fn(dst, elem) is used.
 *       * Otherwise, memcpy(dst, elem, elem_size) is used.
 *   - The element's hash is computed once and stored in its slot.
 *   - The hash table is updated to reference the new index.
 *   - Sorted order is invalidated and will be rebuilt lazily on demand.
 */
bool set_add(set_t *set, const void *elem);

/* Remove an element from the set.
 *
 * Parameters:
 *   elem - pointer to an element equal to the one to remove
 *
 * Returns:
 *   true  if an equal element was found and removed
 *   false if no equal element was present
 *
 * Behaviour:
 *   - If destroy_fn was provided, it is called on the removed element.
 *   - The arena remains dense: the last element is moved into the removed
 *     slot, and the hash table is updated to point to the new index.
 *   - Sorted order is invalidated and will be rebuilt lazily on demand.
 */
bool set_remove(set_t *set, const void *elem);

/* Get a pointer to the element at the given index in arena order.
 *
 * Parameters:
 *   index - zero-based index in [0, set_get_size(set))
 *
 * Returns:
 *   Pointer to the element's storage inside the arena, or NULL if index
 *   is out of range.
 *
 * Notes:
 *   - The order is the internal arena order (B1), which is not stable
 *     across removals (removal compacts by moving the last element).
 *   - The returned pointer is invalidated by any subsequent mutation
 *     of the set (add/remove/clear/destroy).
 */
const void *set_get(const set_t *set, size_t index);

/* Get a pointer to the element at the given index in sorted order.
 *
 * Parameters:
 *   index - zero-based index in [0, set_get_size(set))
 *
 * Returns:
 *   Pointer to the element's storage inside the arena, or NULL if index
 *   is out of range.
 *
 * Behaviour:
 *   - On first call after any mutation, the set builds an internal array
 *     of indices [0..size-1] and sorts it using the cmp function.
 *   - Subsequent calls reuse this sorted index array until the next mutation.
 *   - Sorting is unstable: elements that compare equal may appear in any
 *     relative order (S1).
 *
 * Notes:
 *   - The returned pointer is invalidated by any subsequent mutation
 *     of the set (add/remove/clear/destroy).
 */
const void *set_get_sorted(set_t *set, size_t index);

/* Create a deep/shallow clone of a set.
 *
 * Returns:
 *   A new set with the same element_size, hash, cmp, clone, and destroy
 *   callbacks, containing copies of all elements from 'set', or NULL on
 *   allocation failure.
 *
 * Behaviour:
 *   - If clone_fn was provided, it is used to copy each element.
 *   - Otherwise, elements are copied with memcpy.
 *   - The new set has its own arena and hash table; it shares no storage
 *     with the original.
 */
set_t *set_clone(const set_t *set);

/* Compute the union of two sets.
 *
 * Parameters:
 *   a, b - input sets (must have the same element_size, hash, and cmp
 *          semantics; this is not checked at runtime).
 *
 * Returns:
 *   A new set containing all elements that are in a or b (or both),
 *   or NULL on allocation failure.
 *
 * Behaviour:
 *   - The result set uses the same callbacks (hash, cmp, clone, destroy)
 *     as 'a'.
 *   - Elements are copied using clone_fn if provided, otherwise memcpy.
 */
set_t *set_union(const set_t *a, const set_t *b);

/* Compute the intersection of two sets.
 *
 * Returns:
 *   A new set containing all elements that are in both a and b,
 *   or NULL on allocation failure.
 *
 * Behaviour:
 *   - The result set uses the same callbacks as 'a'.
 *   - Elements are copied using clone_fn if provided, otherwise memcpy.
 */
set_t *set_intersection(const set_t *a, const set_t *b);

/* Compute the set difference a \ b.
 *
 * Returns:
 *   A new set containing all elements that are in a but not in b,
 *   or NULL on allocation failure.
 *
 * Behaviour:
 *   - The result set uses the same callbacks as 'a'.
 *   - Elements are copied using clone_fn if provided, otherwise memcpy.
 */
set_t *set_difference(const set_t *a, const set_t *b);

/* Check whether a is a subset of b.
 *
 * Returns:
 *   true  if every element of a is also contained in b
 *   false otherwise
 */
bool set_is_subset(const set_t *a, const set_t *b);

#endif /* SET_H */
