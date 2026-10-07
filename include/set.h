/**
 * @file set.h
 * @brief Generic value-set container with a dense arena and lazy sorting.
 *
 * Use set_t to maintain unique values with hash-based membership, insertion and
 * removal. It supports iteration and sorted access while hiding the storage layout.
 * Equality and hashing callbacks must describe the same notion of a unique value.
 *
 * Clone and destroy callbacks control element resource ownership. Prefer array.h
 * when duplicates and positional sequence semantics matter, or dictionary.h when
 * each unique key needs an associated value.
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

/**
 * @brief Compute the hash used to locate an element in the set's hash table.
 *
 * @param elem Non-NULL pointer to readable element bytes, borrowed for the duration of the call.
 * @return Hash value for the element; elements comparing equal must have equal hashes.
 *
 * The pointer addresses the element's bytes, not an additional pointer to the element. Hashing must be consistent
 * with set_cmp_fn: if cmp(a, b) == 0, then hash(a) == hash(b). Hashes are cached in arena slots, so hash-relevant
 * state must remain unchanged while an element is stored. Do not modify or free the borrowed element.
 */
typedef size_t (*set_hash_fn)(const void *elem);

/**
 * @brief Compare two elements for equality and ascending sorted access.
 *
 * @param a Non-NULL pointer to the first element's bytes, borrowed for the duration of the call.
 * @param b Non-NULL pointer to the second element's bytes, borrowed for the duration of the call.
 * @return A negative value if a precedes b, zero if they are equal, or a positive value if a follows b.
 *
 * Provide a consistent ordering, with equality defined by cmp(a, b) == 0 and equal elements having equal hashes.
 * Comparison-relevant state must remain unchanged while an element is stored. Do not modify or free either element.
 */
typedef int (*set_cmp_fn)(const void *a, const void *b);

/**
 * @brief Construct a copy of one element in destination arena storage.
 *
 * @param dst Non-NULL writable arena storage for one element, owned by the destination set.
 * @param src Non-NULL readable source element, borrowed for the duration of the call.
 *
 * The source is a caller's element during insertion or an arena element during copying or removal compaction.
 * Construct an equal element with the same hash; do not modify or release the source or free destination storage.
 * This void callback cannot report allocation failure: it must leave a valid element usable by the other callbacks.
 * Any copied resources must follow an ownership policy compatible with set_destroy_fn, including when a copy is
 * destroyed during insertion rollback. Arena addresses are not stable across mutations and must not be retained
 * as permanent element addresses. With a NULL clone callback, the set uses memcpy(dst, src, elem_size).
 */
typedef void (*set_clone_fn)(void *dst, const void *src);

/**
 * @brief Release resources held by an element without freeing its arena storage.
 *
 * @param elem Non-NULL pointer to an element in set-owned arena storage, borrowed until this callback returns.
 *
 * Called for removed elements, elements discarded by clear or destruction, constructed copies rolled back after
 * insertion failure, and moved-from elements when removal compaction uses a clone callback. Do not free the slot
 * itself or retain its address. A NULL destroy callback performs no element-level cleanup; any referenced resources
 * then require an external lifetime policy.
 */
typedef void (*set_destroy_fn)(void *elem);

/** @brief Opaque container owning its arena, hash table and optional sorted index array. */
typedef struct _set_t set_t;

/**
 * @brief Allocate an empty set storing fixed-size elements by value.
 *
 * @param elem_size Non-zero size in bytes of each stored element.
 * @param hash Non-NULL callback computing element hashes consistently with cmp.
 * @param cmp Non-NULL callback defining element equality and ascending ordering.
 * @param clone Callback constructing element copies, or NULL to copy elem_size bytes with memcpy.
 * @param destroy Callback releasing element resources, or NULL for no element-level cleanup.
 * @return A newly owned empty set, or NULL if elem_size is zero, hash or cmp is NULL, internal slot padding would
 * overflow, or allocation fails. Release a successful result with set_destroy().
 *
 * Elements are stored inline in an internal arena with fundamental alignment up to max_align_t; extended alignment
 * is unsupported. Elements must tolerate bytewise relocation when the arena grows, even when clone is supplied.
 * The set stores callback pointers without owning them; their functions must remain available throughout its
 * lifetime and the lifetimes of any derived sets that reuse them.
 *
 * Copying pointer fields with memcpy does not duplicate the resources they reference. Callers must choose clone
 * and destroy policies that avoid dangling references and duplicate releases, including during failure cleanup.
 * The set owns the stored bytes; ownership of referenced resources is determined by these callbacks.
 */
set_t *set_create(size_t elem_size, set_hash_fn hash, set_cmp_fn cmp, set_clone_fn clone, set_destroy_fn destroy);

/**
 * @brief Destroy all stored elements and release the set's container storage.
 *
 * @param set Set whose ownership is relinquished, or NULL for no action.
 *
 * Calls the configured destroy callback once for each stored element, then frees the arena, hash table, sorted
 * index array and set itself. With no destroy callback, resources referenced by elements are not released.
 * All pointers into the set and the set pointer itself become invalid.
 */
void set_destroy(set_t *set);

/**
 * @brief Remove every element while retaining container capacity for reuse.
 *
 * @param set Set to empty, or NULL for no action.
 *
 * Calls the configured destroy callback once for each removed element and frees the hash-chain nodes. The arena,
 * bucket array and sorted index storage remain allocated. The resulting size is zero, and the set remains usable
 * for insertion. All former element pointers cease to designate live elements. An already empty set is unchanged.
 */
void set_clear(set_t *set);

/**
 * @brief Read the number of distinct elements currently stored.
 * @param set Set to inspect, or NULL to obtain zero.
 * @return Number of stored elements, or zero if set is NULL.
 */
size_t set_get_size(const set_t *set);

/**
 * @brief Look up an element using its hash and the configured equality comparison.
 *
 * @param set Set to search, or NULL to report absence.
 * @param elem Readable element used as a search key, borrowed for this call; NULL reports absence.
 * @return true if an equal element is present; false if absent or either argument is NULL.
 *
 * Equality is determined by cmp(stored, elem) == 0. The key must have the element representation expected by the
 * set's callbacks. No ownership is transferred, no elements are copied, and stored element pointers remain valid.
 */
bool set_contains(const set_t *set, const void *elem);

/**
 * @brief Insert a copy of an element if no equal element is already present.
 *
 * @param set Destination set, or NULL to fail without insertion.
 * @param elem Readable element to copy, borrowed for this call; NULL causes failure.
 * @return true if newly inserted; false for a duplicate, a NULL argument, an unrepresentable arena capacity or
 * allocation failure. The return value does not distinguish these cases.
 *
 * A duplicate leaves the set unchanged. Otherwise, the hash is computed once and cached in the new arena slot,
 * the clone callback or memcpy constructs the copy, and the hash table records its arena index. Successful
 * insertion invalidates the cached sorted order. The source element's storage is not retained or freed.
 *
 * Failed allocation leaves membership unchanged, but arena growth or rehashing may already have occurred. If
 * allocation of the hash-chain node fails after construction, the destroy callback, when supplied, destroys the
 * new copy. Shallowly shared resources must tolerate that cleanup. Existing element pointers may be invalidated
 * even when insertion fails; reacquire them after an insertion attempt. A source that may be inserted must remain
 * readable across arena growth and must not depend on an address within the destination arena.
 */
bool set_add(set_t *set, const void *elem);

/**
 * @brief Remove an equal element and compact the arena by relocating its last element.
 *
 * @param set Set from which to remove an element, or NULL to report failure.
 * @param elem Readable search key in the set's element representation, borrowed for lookup; NULL reports failure.
 * @return true if an element was removed; false if absent or either argument is NULL.
 *
 * The configured destroy callback releases the removed element's resources. If its slot is not last, the last
 * element replaces it: with a clone callback it is cloned and its old instance is destroyed when destroy is set;
 * otherwise its bytes are moved with memcpy without destroying the old instance. The hash table is updated and
 * sorted order invalidated. The container performs no allocation, although callbacks may allocate.
 *
 * An external search key is neither copied nor destroyed. A key may refer to a stored element during lookup, but
 * must not be used afterwards if removal invalidates it. Reacquire element pointers and indices after successful
 * removal because slot contents and arena order can change. An unsuccessful lookup leaves the set unchanged.
 */
bool set_remove(set_t *set, const void *elem);

/**
 * @brief Borrow an element by its current position in the dense arena.
 *
 * @param set Set whose element storage is inspected, or NULL to obtain NULL.
 * @param index Zero-based arena position, less than set_get_size(set).
 * @return Read-only pointer to set-owned element storage, or NULL if set is NULL or index is out of range.
 *
 * Arena order (B1) is not stable across removals because the last element fills the removed slot. Do not free the
 * returned pointer or change hash- or comparison-relevant state through referenced resources. Reacquire pointers
 * after add or remove operations; even a failed insertion can relocate the arena. Clear and destruction end all
 * stored element lifetimes. Read-only queries and building the sorted view do not relocate element storage.
 */
const void *set_get(const set_t *set, size_t index);

/**
 * @brief Borrow an element by ascending comparison order, building the sorted index lazily.
 *
 * @param set Set whose sorted view may be allocated or rebuilt, or NULL to obtain NULL.
 * @param index Zero-based sorted position, less than set_get_size(set).
 * @return Read-only pointer to set-owned element storage, or NULL if set is NULL, index is out of range or
 * allocation of the sorted index array fails.
 *
 * The first valid access after a membership change builds and sorts arena indices using the comparison callback.
 * Later accesses reuse the array until another membership change. Sorting is unstable for elements comparing
 * equal (S1), and never changes arena order. Allocation failure leaves membership unchanged and permits a retry.
 *
 * The pointer has the same ownership and lifetime restrictions as set_get(): do not free it or change element
 * identity, and reacquire it after insertion attempts or removals. Clear or destruction ends its element's lifetime.
 * Building or reusing the sorted index does not itself invalidate element pointers.
 */
const void *set_get_sorted(set_t *set, size_t index);

/**
 * @brief Copy a set using its configured element-copy policy.
 *
 * @param set Source set, borrowed for this call, or NULL to fail.
 * @return A newly owned copy, or NULL if set is NULL or container allocation fails. Release the copy with
 * set_destroy().
 *
 * The copy preserves element size, callback pointers, arena order and cached hashes, and owns separate arena and
 * hash-table storage. Its sorted view is built lazily. Each element is copied with clone or, when absent, memcpy;
 * resources referenced by pointer fields can therefore remain shared. This is a deep copy only to the extent
 * implemented by clone. The source arena and its element addresses are unchanged.
 *
 * Failure destroys any constructed elements in the new set using destroy when supplied. The callback policy must
 * allow both copies to be destroyed independently, including this failure cleanup; a shallow copy paired with a
 * callback that unconditionally frees shared resources does not provide that guarantee.
 */
set_t *set_clone(const set_t *set);

/**
 * @brief Construct the union by copying distinct elements from both input sets.
 *
 * @param a First input set and source of the result's element size and callbacks, borrowed for this call.
 * @param b Second input set, borrowed for this call and required to be compatible with a.
 * @return A newly owned result, or NULL if either input is NULL or result creation or initial arena reservation
 * fails. Later insertion failures are ignored and can produce an incomplete non-NULL result.
 *
 * Inputs must have the same element size and compatible representations, hash semantics and comparison semantics;
 * compatibility is not checked. The combined element count must be representable by size_t because the initial
 * capacity sum is not checked for overflow. Elements from a are inserted before elements from b, so equal elements
 * retain a's representative. With successful insertions the result contains every element present in either input.
 *
 * The result uses all of a's callbacks, including clone and destroy for elements copied from b. These callbacks
 * must support both inputs' elements and allow copies to be destroyed independently, including failure cleanup.
 * Copies use clone or memcpy; separate arena storage does not imply separate pointed-to resources. Inputs retain
 * their arena storage and membership. The caller owns the result and must release it with set_destroy().
 */
set_t *set_union(const set_t *a, const set_t *b);

/**
 * @brief Construct the intersection by copying elements present in both input sets.
 *
 * @param a First input set and source of the result's element size and callbacks, borrowed for this call.
 * @param b Second input set, borrowed for this call and required to be compatible with a.
 * @return A newly owned result, or NULL if either input is NULL or result creation or initial arena reservation
 * fails. Later insertion failures are ignored and can produce an incomplete non-NULL result.
 *
 * Inputs must have the same element size and compatible representations, hash semantics and comparison semantics;
 * compatibility is not checked. The smaller set is traversed and supplies the representatives copied into the
 * result; a is traversed when sizes are equal. Membership is tested using the other input's callbacks.
 *
 * The result uses all of a's callbacks, with clone or memcpy copying each selected element. Its copy and destruction
 * policy must work for either input's elements and allow independent destruction, including failure cleanup.
 * Result arena storage is separate, but pointed-to resources may be shared. Inputs retain their arena storage and
 * membership. The caller owns the result and must release it with set_destroy().
 */
set_t *set_intersection(const set_t *a, const set_t *b);

/**
 * @brief Construct the difference by copying elements of a that are absent from b.
 *
 * @param a Input set whose elements are selected and whose element size and callbacks define the result.
 * @param b Compatible input set used to exclude elements; both inputs are borrowed for this call.
 * @return A newly owned result, or NULL if either input is NULL or result creation or initial arena reservation
 * fails. Later insertion failures are ignored and can produce an incomplete non-NULL result.
 *
 * Inputs must have the same element size and compatible representations, hash semantics and comparison semantics;
 * compatibility is not checked. Each element of a is looked up using b's callbacks and copied only when absent.
 * The result uses all of a's callbacks and copies with clone or memcpy. Its arena is separate, but resources
 * referenced by elements may be shared. The copy and destruction policy must allow independent destruction,
 * including failure cleanup. Inputs retain their arena storage and membership. The caller owns the result and
 * must release it with set_destroy().
 */
set_t *set_difference(const set_t *a, const set_t *b);

/**
 * @brief Check whether every element of a is also present in b.
 *
 * @param a Candidate subset, borrowed for this call.
 * @param b Compatible set required to contain every element of a, borrowed for this call.
 * @return true if every element of a is contained in b, including an empty a; false if either input is NULL,
 * a has more elements than b, or any element of a is absent from b.
 *
 * Inputs must have the same element size and compatible representations, hash semantics and comparison semantics;
 * compatibility is not checked. Lookups use b's callbacks. No container allocation, element copying or ownership
 * transfer occurs, and input element pointers remain valid.
 */
bool set_is_subset(const set_t *a, const set_t *b);

#endif /* SET_H */
