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

#endif /* ARRAY_H */