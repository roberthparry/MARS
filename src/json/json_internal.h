#ifndef JSON_INTERNAL_H
#define JSON_INTERNAL_H

#include <stdbool.h>

#include "array.h"
#include "dictionary.h"
#include "json.h"

typedef struct {
    string_t *text;
    number_t value;
} json_number_payload_t;

struct _json_t {
    json_type_t type;
    union {
        bool boolean;
        json_number_payload_t number;
        string_t *text;
        array_t *array;
        dictionary_t *object;
    } u;
};

bool json_type_valid(json_type_t type);
json_t *json_alloc(json_type_t type);

bool json_number_scan(const string_t *number_text, bool require_end);
bool json_number_value_from_spelling(const string_t *text, number_t *out);
string_t *json_number_spelling(number_t value);

#endif /* JSON_INTERNAL_H */
