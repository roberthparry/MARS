/* json_core.c - opaque JSON value storage, construction, and access */

#include <stdlib.h>

#define MARS_JSON_INTERNAL_ACCESS
#include "json_internal.h"

typedef bool (*json_clone_value_fn)(json_t *dst, const json_t *src);
typedef void (*json_destroy_value_fn)(json_t *json);

typedef struct {
    json_clone_value_fn clone;
    json_destroy_value_fn destroy;
} json_type_ops_t;

static bool json_clone_null(json_t *dst, const json_t *src);
static bool json_clone_bool(json_t *dst, const json_t *src);
static bool json_clone_number(json_t *dst, const json_t *src);
static bool json_clone_text(json_t *dst, const json_t *src);
static bool json_clone_array(json_t *dst, const json_t *src);
static bool json_clone_object(json_t *dst, const json_t *src);
static void json_destroy_noop(json_t *json);
static void json_destroy_number(json_t *json);
static void json_destroy_text(json_t *json);
static void json_destroy_array(json_t *json);
static void json_destroy_object(json_t *json);

static const json_type_ops_t json_ops[JSON_OBJECT + 1u] = {
    [JSON_NULL] = {json_clone_null, json_destroy_noop},       [JSON_BOOL] = {json_clone_bool, json_destroy_noop},
    [JSON_NUMBER] = {json_clone_number, json_destroy_number}, [JSON_STRING] = {json_clone_text, json_destroy_text},
    [JSON_ARRAY] = {json_clone_array, json_destroy_array},    [JSON_OBJECT] = {json_clone_object, json_destroy_object}};

bool json_type_valid(json_type_t type)
{
    return type >= JSON_NULL && type <= JSON_OBJECT;
}

static void json_ptr_destroy(void *elem)
{
    json_t **value = elem;

    if (value)
        json_free(*value);
}

static size_t json_string_key_hash(const void *key)
{
    string_t *text = *(string_t *const *)key;

    return (size_t)string_hash(text);
}

static int json_string_key_cmp(const void *a, const void *b)
{
    string_t *left = *(string_t *const *)a;
    string_t *right = *(string_t *const *)b;

    return string_compare(left, right);
}

static void json_string_key_clone(void *dst, const void *src)
{
    string_t *text = *(string_t *const *)src;

    *(string_t **)dst = string_clone(text);
}

static void json_string_key_destroy(void *elem)
{
    string_free(*(string_t **)elem);
}

static dictionary_t *json_object_storage_new(void)
{
    return dictionary_create(sizeof(string_t *), sizeof(json_t *), json_string_key_hash, json_string_key_cmp,
                             json_string_key_clone, json_string_key_destroy, NULL, NULL, json_ptr_destroy);
}

static array_t *json_array_storage_new(void)
{
    return array_create(sizeof(json_t *), NULL, json_ptr_destroy);
}

json_t *json_alloc(json_type_t type)
{
    json_t *json;

    if (!json_type_valid(type))
        return NULL;

    json = calloc(1u, sizeof(*json));
    if (!json)
        return NULL;

    json->type = type;
    return json;
}

json_t *json_new_null(void)
{
    return json_alloc(JSON_NULL);
}

json_t *json_new_bool(bool value)
{
    json_t *json = json_alloc(JSON_BOOL);

    if (json)
        json->u.boolean = value;
    return json;
}

bool json_number_scan(const string_t *number_text, bool require_end)
{
    string_cursor_t *cursor;
    unsigned char ch;
    bool ok = false;

    if (!number_text)
        return false;

    cursor = string_cursor_new(number_text);
    if (!cursor)
        return false;

    if (string_cursor_peek_ascii(cursor, &ch) && ch == '-')
        (void)string_cursor_next(cursor);

    if (!string_cursor_peek_ascii(cursor, &ch))
        goto done;

    if (ch == '0') {
        (void)string_cursor_next(cursor);
    } else if (ch >= '1' && ch <= '9') {
        do {
            (void)string_cursor_next(cursor);
        } while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9');
    } else {
        goto done;
    }

    if (string_cursor_peek_ascii(cursor, &ch) && ch == '.') {
        (void)string_cursor_next(cursor);
        if (!string_cursor_peek_ascii(cursor, &ch) || ch < '0' || ch > '9')
            goto done;
        do {
            (void)string_cursor_next(cursor);
        } while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9');
    }

    if (string_cursor_peek_ascii(cursor, &ch) && (ch == 'e' || ch == 'E')) {
        (void)string_cursor_next(cursor);
        if (string_cursor_peek_ascii(cursor, &ch) && (ch == '+' || ch == '-'))
            (void)string_cursor_next(cursor);
        if (!string_cursor_peek_ascii(cursor, &ch) || ch < '0' || ch > '9')
            goto done;
        do {
            (void)string_cursor_next(cursor);
        } while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9');
    }

    ok = !require_end || string_cursor_done(cursor);

done:
    string_cursor_free(cursor);
    return ok;
}

bool json_number_value_from_spelling(const string_t *text, number_t *out)
{
    number_t value;
    string_t *round_trip;

    if (!text || !out)
        return false;

    value = num_create_from_text(text);
    round_trip = num_to_string(value);
    if (!round_trip) {
        num_destroy(&value);
        return false;
    }

    string_free(round_trip);
    *out = value;
    return true;
}

string_t *json_number_spelling(number_t value)
{
    const char *constant_name = num_constant_name(value);

    if (constant_name)
        return string_new_with(constant_name);

    return num_to_string(value);
}

json_t *json_new_number(const string_t *number_text)
{
    json_t *json;
    number_t value;

    if (!json_number_scan(number_text, true))
        return NULL;
    if (!json_number_value_from_spelling(number_text, &value))
        return NULL;

    json = json_alloc(JSON_NUMBER);
    if (!json) {
        num_destroy(&value);
        return NULL;
    }

    json->u.number.value = value;
    json->u.number.text = string_clone(number_text);
    if (!json->u.number.text) {
        json_free(json);
        return NULL;
    }

    return json;
}

json_t *json_new_number_value(number_t value)
{
    json_t *json;
    number_t owned;
    string_t *text;

    text = json_number_spelling(value);
    if (!text)
        return NULL;

    owned = num_clone(value);
    json = json_alloc(JSON_NUMBER);
    if (!json) {
        string_free(text);
        num_destroy(&owned);
        return NULL;
    }

    json->u.number.value = owned;
    json->u.number.text = text;
    return json;
}

json_t *json_new_string(const string_t *value)
{
    json_t *json;

    if (!value)
        return NULL;

    json = json_alloc(JSON_STRING);
    if (!json)
        return NULL;

    json->u.text = string_clone(value);
    if (!json->u.text) {
        json_free(json);
        return NULL;
    }

    return json;
}

json_t *json_new_array(void)
{
    json_t *json = json_alloc(JSON_ARRAY);

    if (!json)
        return NULL;

    json->u.array = json_array_storage_new();
    if (!json->u.array) {
        json_free(json);
        return NULL;
    }

    return json;
}

json_t *json_new_object(void)
{
    json_t *json = json_alloc(JSON_OBJECT);

    if (!json)
        return NULL;

    json->u.object = json_object_storage_new();
    if (!json->u.object) {
        json_free(json);
        return NULL;
    }

    return json;
}

json_t *json_clone(const json_t *json)
{
    json_t *clone;

    if (!json || !json_type_valid(json->type))
        return NULL;

    clone = json_alloc(json->type);
    if (!clone)
        return NULL;

    if (!json_ops[json->type].clone(clone, json)) {
        json_free(clone);
        return NULL;
    }

    return clone;
}

void json_free(json_t *json)
{
    if (!json)
        return;

    if (json_type_valid(json->type))
        json_ops[json->type].destroy(json);
    free(json);
}

static bool json_clone_null(json_t *dst, const json_t *src)
{
    (void)dst;
    (void)src;
    return true;
}

static bool json_clone_bool(json_t *dst, const json_t *src)
{
    dst->u.boolean = src->u.boolean;
    return true;
}

static bool json_clone_number(json_t *dst, const json_t *src)
{
    dst->u.number.text = string_clone(src->u.number.text);
    if (!dst->u.number.text)
        return false;

    dst->u.number.value = num_clone(src->u.number.value);
    return true;
}

static bool json_clone_text(json_t *dst, const json_t *src)
{
    dst->u.text = string_clone(src->u.text);
    return dst->u.text != NULL;
}

static bool json_clone_array(json_t *dst, const json_t *src)
{
    size_t count;

    dst->u.array = json_array_storage_new();
    if (!dst->u.array)
        return false;

    count = array_size(src->u.array);
    for (size_t i = 0u; i < count; ++i) {
        json_t **item_ptr = array_get(src->u.array, i);

        if (!item_ptr)
            return false;
        if (!json_array_append(dst, *item_ptr))
            return false;
    }

    return true;
}

static bool json_clone_object(json_t *dst, const json_t *src)
{
    size_t count;

    dst->u.object = json_object_storage_new();
    if (!dst->u.object)
        return false;

    count = dictionary_size(src->u.object);
    for (size_t i = 0u; i < count; ++i) {
        const void *key_ptr = dictionary_get_key(src->u.object, i);
        const void *value_ptr = dictionary_get_value(src->u.object, i);
        string_t *key;

        if (!key_ptr || !value_ptr)
            return false;

        key = *(string_t *const *)key_ptr;
        if (!json_object_set(dst, key, *(json_t *const *)value_ptr))
            return false;
    }

    return true;
}

static void json_destroy_noop(json_t *json)
{
    (void)json;
}

static void json_destroy_number(json_t *json)
{
    string_free(json->u.number.text);
    json->u.number.text = NULL;
    num_destroy(&json->u.number.value);
}

static void json_destroy_text(json_t *json)
{
    string_free(json->u.text);
    json->u.text = NULL;
}

static void json_destroy_array(json_t *json)
{
    array_destroy(json->u.array);
    json->u.array = NULL;
}

static void json_destroy_object(json_t *json)
{
    dictionary_destroy(json->u.object);
    json->u.object = NULL;
}

json_type_t json_type(const json_t *json)
{
    return json ? json->type : JSON_NULL;
}

bool json_bool_value(const json_t *json, bool *out)
{
    if (!json || json->type != JSON_BOOL || !out)
        return false;

    *out = json->u.boolean;
    return true;
}

const string_t *json_number_text(const json_t *json)
{
    return (json && json->type == JSON_NUMBER) ? json->u.number.text : NULL;
}

bool json_number_value(const json_t *json, number_t *out)
{
    if (!json || json->type != JSON_NUMBER || !out)
        return false;

    *out = num_clone(json->u.number.value);
    return true;
}

const string_t *json_string_value(const json_t *json)
{
    return (json && json->type == JSON_STRING) ? json->u.text : NULL;
}

size_t json_array_size(const json_t *json)
{
    return (json && json->type == JSON_ARRAY) ? array_size(json->u.array) : 0u;
}

const json_t *json_array_get(const json_t *json, size_t index)
{
    json_t **value;

    if (!json || json->type != JSON_ARRAY)
        return NULL;

    value = array_get(json->u.array, index);
    return value ? *value : NULL;
}

bool json_array_append(json_t *json, const json_t *value)
{
    json_t *copy;

    if (!json || json->type != JSON_ARRAY || !value)
        return false;

    copy = json_clone(value);
    if (!copy)
        return false;

    if (!array_add(json->u.array, &copy)) {
        json_free(copy);
        return false;
    }

    return true;
}

size_t json_object_size(const json_t *json)
{
    return (json && json->type == JSON_OBJECT) ? dictionary_size(json->u.object) : 0u;
}

const json_t *json_object_get(const json_t *json, const string_t *key)
{
    json_t *value = NULL;
    string_t *lookup = (string_t *)key;

    if (!json || json->type != JSON_OBJECT || !key)
        return NULL;

    return dictionary_get(json->u.object, &lookup, &value) ? value : NULL;
}

json_t *json_object_get_mutable(json_t *json, const string_t *key)
{
    json_t *value = NULL;
    string_t *lookup = (string_t *)key;

    if (!json || json->type != JSON_OBJECT || !key)
        return NULL;

    return dictionary_get(json->u.object, &lookup, &value) ? value : NULL;
}

const string_t *json_object_key_at(const json_t *json, size_t index)
{
    const void *key_ptr;

    if (!json || json->type != JSON_OBJECT)
        return NULL;

    key_ptr = dictionary_get_key(json->u.object, index);
    return key_ptr ? *(string_t *const *)key_ptr : NULL;
}

const json_t *json_object_value_at(const json_t *json, size_t index)
{
    const void *value_ptr;

    if (!json || json->type != JSON_OBJECT)
        return NULL;

    value_ptr = dictionary_get_value(json->u.object, index);
    return value_ptr ? *(json_t *const *)value_ptr : NULL;
}

bool json_object_set(json_t *json, const string_t *key, const json_t *value)
{
    string_t *lookup = (string_t *)key;
    json_t *copy;

    if (!json || json->type != JSON_OBJECT || !key || !value)
        return false;

    copy = json_clone(value);
    if (!copy)
        return false;

    if (!dictionary_set(json->u.object, &lookup, &copy)) {
        json_free(copy);
        return false;
    }

    return true;
}
