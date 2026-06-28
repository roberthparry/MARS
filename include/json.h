#ifndef JSON_H
#define JSON_H

/**
 * @file json.h
 * @brief Opaque JSON value tree backed by MARS string, array, and dictionary types.
 *
 * Standard JSON numbers are read and written as ordinary JSON numbers.
 * `number_t` values that cannot be represented by standard JSON number
 * syntax, such as rationals, complex values, infinities, NaN, and symbolic
 * constants, are serialised as valid JSON objects using a MARS-specific tag:
 *
 * @code{.json}
 * { "$mars.number": "π" }
 * @endcode
 *
 * The tag is not part of the JSON standard; other parsers will see it as an
 * ordinary object. MARS recognises it and restores the original `number_t`.
 * Typeable Greek aliases such as `@pi` are accepted on input, but canonical
 * output prefers mathematical spellings such as `π`.
 */

#include <stdbool.h>
#include <stddef.h>

#include "number.h"
#include "ustring.h"

typedef struct _json_t json_t;

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

json_t *json_new_null(void);
json_t *json_new_bool(bool value);
json_t *json_new_number(const string_t *number_text);
json_t *json_new_number_value(number_t value);
json_t *json_new_string(const string_t *value);
json_t *json_new_array(void);
json_t *json_new_object(void);
json_t *json_clone(const json_t *json);
void json_free(json_t *json);

json_t *json_from_text(const string_t *text);
json_t *json_from_file(const string_t *path);

string_t *json_to_string(const json_t *json);
string_t *json_to_string_pretty(const json_t *json, int indent_size);
int json_to_file(const json_t *json, const string_t *path);
int json_to_file_pretty(const json_t *json, const string_t *path, int indent_size);

json_type_t json_type(const json_t *json);
bool json_bool_value(const json_t *json, bool *out);
const string_t *json_number_text(const json_t *json);
bool json_number_value(const json_t *json, number_t *out);
const string_t *json_string_value(const json_t *json);

size_t json_array_size(const json_t *json);
const json_t *json_array_get(const json_t *json, size_t index);
/**
 * @brief Append a copy of @p value to an array.
 *
 * The caller keeps ownership of @p value in every outcome and should release
 * any value it created with json_free().
 *
 * @return true if the copy was stored, false otherwise.
 */
bool json_array_append(json_t *json, const json_t *value);

size_t json_object_size(const json_t *json);
const json_t *json_object_get(const json_t *json, const string_t *key);
json_t *json_object_get_mutable(json_t *json, const string_t *key);
const string_t *json_object_key_at(const json_t *json, size_t index);
const json_t *json_object_value_at(const json_t *json, size_t index);
/**
 * @brief Store a copy of @p value under @p key.
 *
 * The caller keeps ownership of @p key and @p value in every outcome. If the
 * key already exists, the stored value is replaced by a copied value.
 *
 * @return true if the copy was stored, false otherwise.
 */
bool json_object_set(json_t *json, const string_t *key, const json_t *value);

/**
 * @brief Serialise a JSON tree into a SQLite-ready payload.
 *
 * The payload is compact JSON text. On success, the caller owns
 * @p out_type, @p out_encoding, and @p out_data and must release them with
 * @c string_free() and @c free().
 *
 * @param json JSON value tree to serialise.
 * @param out_type Receives a newly allocated type label.
 * @param out_encoding Receives a newly allocated encoding label.
 * @param out_data Receives a newly allocated payload buffer.
 * @param out_len Receives the payload length in bytes.
 * @return @c true on success, otherwise @c false.
 */
bool json_serialize(const json_t *json,
                    string_t **out_type,
                    string_t **out_encoding,
                    void **out_data,
                    size_t *out_len);

/**
 * @brief Reconstruct a JSON tree from a serialised payload.
 *
 * @param data Serialised payload bytes.
 * @param len Payload length in bytes.
 * @param type Stored type label.
 * @param encoding Stored encoding label.
 * @return Newly allocated JSON tree on success, otherwise @c NULL.
 */
json_t *json_deserialise(const void *data,
                         size_t len,
                         const string_t *type,
                         const string_t *encoding);

#endif /* JSON_H */
