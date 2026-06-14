# `json_t`

`json_t` is an opaque JSON value tree. It parses JSON from `string_t`, writes
JSON back to `string_t` or files, and stores arrays and objects through MARS'
generic container layer without exposing those internals to callers.

## Capabilities

- parse JSON from an owning `string_t`
- load JSON from a file path stored in a `string_t`
- serialise compact or pretty JSON into a new `string_t`
- write compact or pretty JSON to a file
- construct null, boolean, number, string, array and object values
- inspect arrays and objects without exposing `array_t` or `dictionary_t`
- round-trip standard JSON numbers as text and as `number_t`
- preserve full `number_t` values through a valid JSON extension object

## Example: Parse and Inspect

```c
#include "json.h"
#include "ustring.h"

static string_t *s(const char *text) {
    return string_new_with(text);
}

int main(void) {
    string_t *text = s("{\"name\":\"mars\",\"enabled\":true,\"items\":[1,2,3]}");
    string_t *name_key = s("name");
    string_t *enabled_key = s("enabled");
    string_t *items_key = s("items");
    json_t *root = json_from_text(text);
    const json_t *name = json_object_get(root, name_key);
    const json_t *enabled_json = json_object_get(root, enabled_key);
    const json_t *items = json_object_get(root, items_key);
    bool enabled = false;

    json_bool_value(enabled_json, &enabled);

    string_printf("name=%S\n", json_string_value(name));
    string_printf("enabled=%s\n", enabled ? "true" : "false");
    string_printf("items=%zu\n", json_array_size(items));

    json_free(root);
    string_free(items_key);
    string_free(enabled_key);
    string_free(name_key);
    string_free(text);
    return 0;
}
```

Expected output:

```text
name=mars
enabled=true
items=3
```

## Example: Build and Serialise

```c
#include "json.h"
#include "ustring.h"

static string_t *s(const char *text) {
    return string_new_with(text);
}

int main(void) {
    json_t *items = json_new_array();
    string_t *name = s("MARS");
    string_t *two = s("2");
    json_t *name_value = json_new_string(name);
    json_t *true_value = json_new_bool(true);
    json_t *two_value = json_new_number(two);
    string_t *out;

    json_array_append(items, name_value);
    json_array_append(items, true_value);
    json_array_append(items, two_value);

    out = json_to_string_pretty(items, 2);
    string_printf("%S", out);

    string_free(out);
    json_free(two_value);
    json_free(true_value);
    json_free(name_value);
    string_free(two);
    string_free(name);
    json_free(items);
    return 0;
}
```

Expected output:

```json
[
  "MARS",
  true,
  2
]
```

## Full `number_t` Fidelity

Standard JSON has a deliberately small number grammar. `json_t` keeps ordinary
JSON numbers standard when parsing and serialising:

```json
123
-4.5e+6
```

When a MARS value cannot be represented by standard JSON number syntax,
`json_t` serialises it as a normal JSON object with a MARS tag:

```json
{"$mars.number":"π"}
```

Other JSON parsers will simply see an object. MARS recognises the tag and
delegates the value string to `number_t`, so rationals, multiprecision values,
complex numbers, infinities, NaN and symbolic constants can round-trip without
losing meaning.

```c
#include "json.h"
#include "number.h"
#include "ustring.h"

int main(void) {
    number_t rational = num_create_from_frac(2, 3);
    json_t *json = json_new_number_value(rational);
    string_t *text = json_to_string(json);

    string_printf("%S\n", text);

    string_free(text);
    json_free(json);
    num_destroy(&rational);
    return 0;
}
```

Expected output:

```json
{"$mars.number":"⅔"}
```

`@pi` and similar typeable aliases are accepted on input for convenience, but
serialised output prefers mathematical spellings such as `π`.

## File Round-Tripping

```c
#include "json.h"
#include "ustring.h"

int main(void) {
    string_t *path = string_new_with("settings.json");
    string_t *key = string_new_with("enabled");
    json_t *root = json_new_object();
    json_t *enabled = json_new_bool(true);
    json_t *loaded;

    json_object_set(root, key, enabled);
    json_to_file_pretty(root, path, 2);

    loaded = json_from_file(path);
    string_printf("type=%d\n", (int)json_type(loaded));

    json_free(loaded);
    json_free(enabled);
    json_free(root);
    string_free(key);
    string_free(path);
    return 0;
}
```

Expected output:

```text
type=5
```

`5` is `JSON_OBJECT`.

## Ownership

Every `json_new_*`, `json_from_*`, `json_clone` and `json_to_string*` result is
heap allocated. The caller owns it and must release it with `json_free()` or
`string_free()` as appropriate.

`json_array_append()` and `json_object_set()` copy the value into the container.
They return `true` when the copy was stored and `false` when nothing was stored.
The caller still owns every `json_t *` it explicitly created and should free it
with `json_free()`.

Strings passed into JSON constructors or object lookup functions are copied or
borrowed only for the duration of the call as documented by the API. Callers
continue to own their `string_t` keys and values.

## API Reference

All declarations are in `include/json.h`.

### Types

- `json_t` — opaque heap-allocated JSON value
- `json_type_t` — one of `JSON_NULL`, `JSON_BOOL`, `JSON_NUMBER`, `JSON_STRING`, `JSON_ARRAY`, or `JSON_OBJECT`

### Construction and Lifetime

- `json_t *json_new_null(void)` — create `null`
- `json_t *json_new_bool(bool value)` — create a boolean
- `json_t *json_new_number(const string_t *number_text)` — create a JSON number from standard JSON number text
- `json_t *json_new_number_value(number_t value)` — create a number value, using the MARS number envelope when needed
- `json_t *json_new_string(const string_t *value)` — create a JSON string
- `json_t *json_new_array(void)` — create an empty array
- `json_t *json_new_object(void)` — create an empty object
- `json_t *json_clone(const json_t *json)` — deep-copy a value tree
- `void json_free(json_t *json)` — destroy a value tree; safe for `NULL`

### Parsing and Serialisation

- `json_t *json_from_text(const string_t *text)` — parse JSON text
- `json_t *json_from_file(const string_t *path)` — load and parse a JSON file
- `string_t *json_to_string(const json_t *json)` — serialise compact JSON
- `string_t *json_to_string_pretty(const json_t *json, int indent_size)` — serialise pretty JSON
- `int json_to_file(const json_t *json, const string_t *path)` — write compact JSON to a file
- `int json_to_file_pretty(const json_t *json, const string_t *path, int indent_size)` — write pretty JSON to a file

### Inspection

- `json_type_t json_type(const json_t *json)` — value kind
- `bool json_bool_value(const json_t *json, bool *out)` — read a boolean
- `const string_t *json_number_text(const json_t *json)` — read preserved number spelling
- `bool json_number_value(const json_t *json, number_t *out)` — parse a number as `number_t`
- `const string_t *json_string_value(const json_t *json)` — read a JSON string value

### Arrays

- `size_t json_array_size(const json_t *json)` — number of elements
- `const json_t *json_array_get(const json_t *json, size_t index)` — borrow an element
- `bool json_array_append(json_t *json, const json_t *value)` — append a copy

### Objects

- `size_t json_object_size(const json_t *json)` — number of members
- `const json_t *json_object_get(const json_t *json, const string_t *key)` — borrow a member
- `json_t *json_object_get_mutable(json_t *json, const string_t *key)` — borrow a mutable member
- `const string_t *json_object_key_at(const json_t *json, size_t index)` — borrow the key at an index
- `const json_t *json_object_value_at(const json_t *json, size_t index)` — borrow the value at an index
- `bool json_object_set(json_t *json, const string_t *key, const json_t *value)` — set a copied value
