#ifndef ARRAY_H
#define ARRAY_H

/**
 * @file array.h
 * @brief Generic, opaque, appendable, and sortable array container.
 *
 * Elements are stored inline in a dense arena. Supports deep copy and cleanup
 * via user-supplied clone and destroy callbacks.
 */

#include <stddef.h>
#include <stdbool.h>

/* --- Function pointer types --- */

/**
 * @brief Element-level clone function.
 *
 * Copies the element from @p src to @p dst. If NULL, memcpy is used.
 *
 * @param dst Pointer to destination element.
 * @param src Pointer to source element.
 */
typedef void (*array_clone_fn)(void *dst, const void *src);

/**
 * @brief Element-level destroy function.
 *
 * Cleans up resources held by an element. If NULL, no action is taken.
 *
 * @param elem Pointer to the element to destroy.
 */
typedef void (*array_destroy_fn)(void *elem);

/**
 * @brief Comparison function for sorting.
 *
 * Returns <0, 0, or >0 as per qsort.
 *
 * @param a Pointer to first element.
 * @param b Pointer to second element.
 * @return Negative if a < b, zero if a == b, positive if a > b.
 */
typedef int (*array_cmp_fn)(const void *a, const void *b);

/**
 * @brief Opaque array type.
 */
typedef struct array array_t;

/* --- Lifecycle --- */

/**
 * @brief Create a new array for elements of size @p elem_size.
 *
 * If @p clone is NULL, memcpy is used for copying elements.
 * If @p destroy is NULL, no cleanup is performed.
 *
 * @param elem_size Size of each element in bytes (must be > 0).
 * @param clone     Element clone function (NULL for memcpy).
 * @param destroy   Element destroy function (NULL for no-op).
 * @return Pointer to new array, or NULL on allocation failure.
 */
array_t *array_create(size_t elem_size,
                      array_clone_fn clone,
                      array_destroy_fn destroy);

/**
 * @brief Destroy the array and free all memory.
 *
 * Calls destroy for each element if provided. Safe to pass NULL.
 *
 * @param arr Pointer to the array.
 */
void array_destroy(array_t *arr);

/**
 * @brief Remove all elements from the array.
 *
 * Calls destroy for each element if provided. Retains arena capacity.
 *
 * @param arr Pointer to the array.
 */
void array_clear(array_t *arr);

/* --- Capacity/size --- */

/**
 * @brief Get the number of elements in the array.
 *
 * @param arr Pointer to the array.
 * @return Number of elements.
 */
size_t array_size(const array_t *arr);

/* --- Element access --- */

/**
 * @brief Get a pointer to the element at @p index.
 *
 * @param arr   Pointer to the array.
 * @param index Zero-based index.
 * @return Pointer to the element, or NULL if out of bounds.
 */
void *array_get(const array_t *arr, size_t index);

/* --- Mutation: add, insert, remove, bulk ops --- */

/**
 * @brief Append an element (by value) to the array.
 *
 * Uses clone if provided, otherwise memcpy. Grows arena as needed.
 *
 * @param arr  Pointer to the array.
 * @param elem Pointer to the element to append.
 * @return true on success, false on allocation failure.
 */
bool array_add(array_t *arr, const void *elem);

/**
 * @brief Insert an element at the given index.
 *
 * Uses clone if provided, otherwise memcpy. Grows arena as needed.
 *
 * @param arr   Pointer to the array.
 * @param index Index at which to insert (0 <= index <= size).
 * @param elem  Pointer to the element to insert.
 * @return true on success, false on allocation failure or out of bounds.
 */
bool array_insert(array_t *arr, size_t index, const void *elem);

/**
 * @brief Insert all elements from another array at a given index (deep copy).
 *
 * Uses clone if provided, otherwise memcpy. Grows arena as needed.
 * Both arrays must have the same element size.
 *
 * @param dst   Pointer to destination array.
 * @param index Index at which to insert (0 <= index <= size).
 * @param src   Pointer to source array.
 * @return true on success, false on allocation failure or out of bounds.
 */
bool array_insert_array(array_t *dst, size_t index, const array_t *src);

/**
 * @brief Insert @p n elements from a C array at a given index (deep copy).
 *
 * Uses clone if provided, otherwise memcpy. Grows arena as needed.
 *
 * @param dst   Pointer to destination array.
 * @param index Index at which to insert (0 <= index <= size).
 * @param src   Pointer to source C array.
 * @param n     Number of elements to insert.
 * @return true on success, false on allocation failure or out of bounds.
 */
bool array_insert_carray(array_t *dst, size_t index, const void *src, size_t n);

/**
 * @brief Append all elements from another array_t (deep copy).
 *
 * Uses clone if provided, otherwise memcpy. Both arrays must have the same element size.
 *
 * @param dst Pointer to destination array.
 * @param src Pointer to source array.
 * @return true on success, false on allocation failure.
 */
bool array_append_array(array_t *dst, const array_t *src);

/**
 * @brief Append @p n elements from a C array of objects (deep copy).
 *
 * Uses clone if provided, otherwise memcpy.
 *
 * @param dst Pointer to destination array.
 * @param src Pointer to source C array.
 * @param n   Number of elements to append.
 * @return true on success, false on allocation failure.
 */
bool array_append_carray(array_t *dst, const void *src, size_t n);

/**
 * @brief Remove the element at the given index.
 *
 * Calls destroy if provided. Preserves order.
 *
 * @param arr   Pointer to the array.
 * @param index Index of the element to remove.
 * @return true on success, false if out of bounds or arr is NULL.
 */
bool array_remove(array_t *arr, size_t index);

/**
 * @brief Remove a range of elements from the array.
 *
 * Removes @p count elements starting at @p index. Calls destroy for each if provided.
 * Preserves order.
 *
 * @param arr   Pointer to the array.
 * @param index Index of the first element to remove.
 * @param count Number of elements to remove.
 * @return true on success, false if out of bounds or arr is NULL.
 */
bool array_remove_elements(array_t *arr, size_t index, size_t count);

/* --- Sorting --- */

/**
 * @brief Sort the array in-place using the provided comparison function.
 *
 * @param arr Pointer to the array.
 * @param cmp Comparison function.
 */
void array_sort(array_t *arr, array_cmp_fn cmp);

/**
 * @brief Swap two elements at indices i and j.
 *
 * @param arr Pointer to the array.
 * @param i   Index of the first element.
 * @param j   Index of the second element.
 * @return true on success, false if out of bounds or arr is NULL.
 */
bool array_swap(array_t *arr, size_t i, size_t j);

/**
 * @brief Rotate the array left by one element.
 *
 * The first element becomes the last; all other elements shift down by one.
 *
 * @param arr Pointer to the array.
 * @return true on success, false if array is empty or NULL.
 */
bool array_rotate_left(array_t *arr);

/**
 * @brief Rotate the array right by one element.
 *
 * The last element becomes the first; all other elements shift up by one.
 *
 * @param arr Pointer to the array.
 * @return true on success, false if array is empty or NULL.
 */
bool array_rotate_right(array_t *arr);

/* --- Slicing --- */

/**
 * @brief Opaque slice type (view into an array).
 *
 * An array slice (`array_slice_t`) is a lightweight, non-owning view into a contiguous
 * subrange of an `array_t`. Slices allow you to operate on a portion of an array
 * without copying data. They are useful for:
 *
 *   - Iterating over a subrange of an array efficiently.
 *   - Creating logical windows or partitions of data for algorithms.
 *   - Providing a "view" that can be sorted, rotated, or swapped independently
 *     of the underlying array order (via an internal index array).
 *   - Passing subarrays to functions without exposing the entire array.
 *
 * Key properties:
 *   - Slices do not own the underlying data; they reference the parent array's arena.
 *   - Slices are safe to use concurrently with the parent array and other slices.
 *   - Slices can be further sub-sliced, creating views of views.
 *   - Slices can maintain their own logical order (for sorting, swapping, rotating)
 *     without modifying the parent array.
 *   - The parent array is reference-counted and will not be destroyed until all
 *     slices referencing it are destroyed.
 *
 * Limitations:
 *   - Mutating the parent array (inserting/removing elements) may invalidate
 *     slices or change their contents if the arena is reallocated or elements
 *     are shifted. Slices are best used for read-only or view-based operations.
 *   - Slices must be destroyed with array_slice_destroy() when no longer needed.
 *
 * Typical usage:
 *   - Create a slice with array_slice().
 *   - Access elements with array_slice_get().
 *   - Sort, swap, or rotate the slice view as needed.
 *   - Destroy the slice with array_slice_destroy().
 */
typedef struct array_slice array_slice_t;

/**
 * @brief Create a slice [start, start+count) from the array (returns NULL if out of bounds).
 *
 * The returned slice must be freed with array_slice_destroy().
 *
 * @param arr   Pointer to the array.
 * @param start Start index.
 * @param count Number of elements in the slice.
 * @return Pointer to a new slice, or NULL if out of bounds or allocation fails.
 */
array_slice_t *array_slice(const array_t *arr, size_t start, size_t count);

/**
 * @brief Destroy a slice created by array_slice.
 *
 * @param slice Pointer to the slice.
 */
void array_slice_destroy(array_slice_t *slice);

/**
 * @brief Get the number of elements in the slice.
 *
 * @param slice Pointer to the slice.
 * @return Number of elements.
 */
size_t array_slice_size(const array_slice_t *slice);

/**
 * @brief Get pointer to element at index in slice (returns NULL if out of bounds).
 *
 * @param slice Pointer to the slice.
 * @param index Index in the slice.
 * @return Pointer to the element, or NULL if out of bounds.
 */
void *array_slice_get(const array_slice_t *slice, size_t index);

/**
 * @brief Create a slice from a slice (sub-slice).
 *
 * @param slice Pointer to the original slice.
 * @param start Start index in the slice.
 * @param count Number of elements in the sub-slice.
 * @return Pointer to a new slice, or NULL if out of bounds or allocation fails.
 */
array_slice_t *array_slice_subslice(const array_slice_t *slice, size_t start, size_t count);

/**
 * @brief Get the element size of the slice.
 *
 * @param slice Pointer to the slice.
 * @return Element size in bytes.
 */
size_t array_slice_elem_size(const array_slice_t *slice);

/**
 * @brief Create a new array from a slice.
 *
 * @param slice Pointer to the slice.
 * @param clone Clone function for elements (may be NULL).
 * @param destroy Destroy function for elements (may be NULL).
 * @return Pointer to a new array, or NULL on allocation failure.
 */
array_t *array_from_slice(const array_slice_t *slice, array_clone_fn clone, array_destroy_fn destroy);

/**
 * @brief Sort the slice view (does not affect underlying array).
 */
void array_slice_sort(array_slice_t *slice, array_cmp_fn cmp);

/**
 * @brief Swap two elements in the slice view.
 */
bool array_slice_swap(array_slice_t *slice, size_t i, size_t j);

/**
 * @brief Rotate the slice view left.
 */
bool array_slice_rotate_left(array_slice_t *slice);

/**
 * @brief Rotate the slice view right.
 */
bool array_slice_rotate_right(array_slice_t *slice);

#endif /* ARRAY_H */