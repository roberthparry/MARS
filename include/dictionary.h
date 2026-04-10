/**
 * @file dictionary.h
 * @brief Generic key/value dictionary with optional deep-copy semantics,
 *        opaque entry handles, and lazy sorted views.
 *
 * This dictionary provides:
 *   - insertion, lookup, replacement, and removal by key
 *   - optional deep or shallow copy behaviour for keys and values
 *   - stable opaque entry handles for efficient repeated access
 *   - sorted views of keys, values, or entries
 *
 * The internal representation is intentionally hidden. Users interact only
 * through the public API and its documented behaviour.
 */

#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------------- */
/* Function pointer types                                                    */
/* ------------------------------------------------------------------------- */

/**
 * @typedef dictionary_hash_fn
 * @brief Hash function for keys.
 *
 * Must return a stable hash value for the given key. The dictionary relies on
 * this function for key lookup performance.
 */
typedef size_t (*dictionary_hash_fn)(const void *key);

/**
 * @typedef dictionary_cmp_fn
 * @brief Comparison function for keys or values.
 *
 * Must return:
 *   - <0 if a < b
 *   -  0 if a == b
 *   - >0 if a > b
 *
 * Used for key equality and for sorted views.
 */
typedef int (*dictionary_cmp_fn)(const void *a, const void *b);

/**
 * @typedef dictionary_clone_fn
 * @brief Clone function for keys or values.
 *
 * Copies the element from @p src into @p dst. If NULL, the dictionary performs
 * a shallow byte copy.
 */
typedef void (*dictionary_clone_fn)(void *dst, const void *src);

/**
 * @typedef dictionary_destroy_fn
 * @brief Destroy function for keys or values.
 *
 * Called when an element is removed or replaced. If NULL, no destruction
 * occurs.
 */
typedef void (*dictionary_destroy_fn)(void *elem);

/* ------------------------------------------------------------------------- */
/* Opaque types                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Opaque dictionary type.
 *
 * The internal structure is hidden. Users interact only through the API.
 */
typedef struct dictionary dictionary_t;

/**
 * @brief Opaque entry handle.
 *
 * Represents a stable reference to a key/value pair stored in the dictionary.
 * Entry handles remain valid until the referenced entry is removed or the
 * dictionary is destroyed.
 */
typedef struct dict_entry dictionary_entry_t;

/* ------------------------------------------------------------------------- */
/* Sorting mode                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @enum dictionary_sort_mode
 * @brief Sorting mode for retrieving entries in sorted order.
 */
typedef enum {
    DICTIONARY_SORT_BY_KEY,   /**< Sort entries by key */
    DICTIONARY_SORT_BY_VALUE  /**< Sort entries by value */
} dictionary_sort_mode;

/* ------------------------------------------------------------------------- */
/* Creation / destruction                                                    */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a new dictionary.
 *
 * @param key_size      Size of each key in bytes.
 * @param value_size    Size of each value in bytes.
 * @param key_hash      Hash function for keys (required).
 * @param key_cmp       Comparison function for keys (required).
 * @param key_clone     Clone function for keys (NULL for shallow copy).
 * @param key_destroy   Destroy function for keys (NULL for no-op).
 * @param value_clone   Clone function for values (NULL for shallow copy).
 * @param value_destroy Destroy function for values (NULL for no-op).
 *
 * @return A new dictionary, or NULL on allocation failure.
 *
 * Keys and values are copied into the dictionary using the provided clone
 * functions (or shallow copies if clone is NULL). The dictionary owns all
 * stored elements and will destroy them when appropriate.
 */
dictionary_t *dictionary_create(size_t key_size,
                                size_t value_size,
                                dictionary_hash_fn key_hash,
                                dictionary_cmp_fn key_cmp,
                                dictionary_clone_fn key_clone,
                                dictionary_destroy_fn key_destroy,
                                dictionary_clone_fn value_clone,
                                dictionary_destroy_fn value_destroy);

/**
 * @brief Destroy a dictionary and free all associated memory.
 *
 * All stored keys and values are destroyed using the provided destroy
 * callbacks.
 */
void dictionary_destroy(dictionary_t *dict);

/**
 * @brief Remove all entries from the dictionary.
 *
 * All keys and values are destroyed using the provided destroy callbacks.
 * The dictionary remains usable and retains its allocated capacity.
 */
void dictionary_clear(dictionary_t *dict);

/* ------------------------------------------------------------------------- */
/* Size                                                                      */
/* ------------------------------------------------------------------------- */

/**
 * @brief Return the number of key/value pairs stored in the dictionary.
 */
size_t dictionary_size(const dictionary_t *dict);

/* ------------------------------------------------------------------------- */
/* Insertion / lookup / removal                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Insert or replace a key/value pair.
 *
 * Use this function when you already have the key and simply want to store or
 * update its associated value.
 *
 * If the key already exists, its value is replaced (destroying the old value
 * if a destroy callback is provided). If the key does not exist, a new entry
 * is created.
 *
 * @return true on success, false on allocation failure.
 */
bool dictionary_set(dictionary_t *dict, const void *key, const void *value);

/**
 * @brief Look up a value by key.
 *
 * Use this for one‑off lookups when you do not need a persistent entry handle.
 *
 * The stored value is copied into @p out_value using either the value clone
 * function (if provided) or a shallow copy.
 *
 * @return true if the key exists, false otherwise.
 */
bool dictionary_get(const dictionary_t *dict, const void *key, void *out_value);

/**
 * @brief Remove an entry by key.
 *
 * If the key exists, the associated key and value are destroyed using the
 * provided destroy callbacks.
 *
 * @return true if the key was removed, false if it was not present.
 */
bool dictionary_remove(dictionary_t *dict, const void *key);

/* ------------------------------------------------------------------------- */
/* Direct access in unspecified order                                        */
/* ------------------------------------------------------------------------- */

/**
 * @brief Retrieve a key by index.
 *
 * The order of keys returned by this function is unspecified and may change
 * after insertions or removals. This function is intended for iteration over
 * all keys without requiring sorted order.
 *
 * @return Pointer to the key, or NULL if the index is out of range.
 */
const void *dictionary_get_key(const dictionary_t *dict, size_t index);

/**
 * @brief Retrieve a value by index.
 *
 * The order is unspecified and may change after modifications.
 *
 * @return Pointer to the value, or NULL if the index is out of range.
 */
const void *dictionary_get_value(const dictionary_t *dict, size_t index);

/* ------------------------------------------------------------------------- */
/* Sorted views                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Retrieve the key at a given position in sorted order.
 *
 * Calling this function repeatedly with indices 0 through
 * dictionary_size(dict)-1 yields all keys in ascending sorted order.
 *
 * The sorted view is built lazily and automatically refreshed after any
 * modification to the dictionary.
 *
 * @return Pointer to the key, or NULL on error.
 */
const void *dictionary_get_key_sorted(dictionary_t *dict, size_t index);

/**
 * @brief Retrieve the value at a given position in sorted order.
 *
 * Values are sorted using the comparison function supplied for keys.
 *
 * Calling this function repeatedly with indices 0 through
 * dictionary_size(dict)-1 yields all values in ascending sorted order.
 *
 * @return Pointer to the value, or NULL on error.
 */
const void *dictionary_get_value_sorted(dictionary_t *dict, size_t index);

/**
 * @brief Retrieve an entry at a given position in sorted order.
 *
 * Returns an opaque entry handle representing the key/value pair at the given
 * sorted position. This is the preferred way to iterate through entries in
 * sorted order.
 *
 * @param dict      Dictionary.
 * @param index     Sorted index (0 ≤ index < size).
 * @param mode      Sort by key or by value.
 * @param out_entry Output pointer to the entry handle.
 *
 * @return true on success, false on error.
 */
bool dictionary_get_entry_sorted(dictionary_t *dict,
                                 size_t index,
                                 dictionary_sort_mode mode,
                                 dictionary_entry_t **out_entry);

/* ------------------------------------------------------------------------- */
/* Entry handles                                                             */
/* ------------------------------------------------------------------------- */

/**
 * @brief Retrieve an entry by key.
 *
 * Use this when you want a persistent handle to an entry for efficient
 * repeated access or in‑place modification of its value.
 *
 * Entry handles remain valid until the entry is removed or the dictionary is
 * destroyed.
 *
 * @return true if the key exists, false otherwise.
 */
bool dictionary_get_entry(const dictionary_t *dict,
                          const void *key,
                          dictionary_entry_t **out_entry);

/**
 * @brief Replace the value associated with an existing entry handle.
 *
 * This is the most efficient way to update a value because it avoids a key
 * lookup. The entry must have been obtained from dictionary_get_entry() or
 * dictionary_get_entry_sorted().
 *
 * @return true on success, false if the entry is invalid.
 */
bool dictionary_set_entry(dictionary_t *dict,
                          dictionary_entry_t *entry,
                          const void *value);

/**
 * @brief Retrieve the key associated with an entry handle.
 *
 * @return Pointer to the key, or NULL if the entry is invalid.
 */
const void *dictionary_entry_key(const dictionary_entry_t *entry);

/**
 * @brief Retrieve the value associated with an entry handle.
 *
 * @return Pointer to the value, or NULL if the entry is invalid.
 */
const void *dictionary_entry_value(const dictionary_entry_t *entry);

/* ------------------------------------------------------------------------- */
/* Iteration                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @typedef dictionary_foreach_fn
 * @brief Callback type for dictionary_foreach().
 *
 * Called once for each entry in the dictionary.
 */
typedef void (*dictionary_foreach_fn)(const dictionary_entry_t *entry, void *user_data);

/**
 * @brief Iterate through all entries in unspecified order.
 *
 * The order is not guaranteed and may change after modifications. For sorted
 * iteration, use dictionary_get_entry_sorted().
 */
void dictionary_foreach(const dictionary_t *dict,
                        dictionary_foreach_fn fn,
                        void *user_data);

/* ------------------------------------------------------------------------- */
/* Cloning                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create a full copy of a dictionary.
 *
 * Keys and values are copied using the clone functions supplied at creation
 * time (or shallow copies if clone is NULL). The resulting dictionary is
 * independent of the original.
 *
 * @return A new dictionary, or NULL on allocation failure.
 */
dictionary_t *dictionary_clone(const dictionary_t *dict);

#endif /* DICTIONARY_H */
