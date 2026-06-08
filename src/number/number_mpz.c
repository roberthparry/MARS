#include <stdlib.h>

#include "number_internal.h"
#include "ustring.h"

typedef struct {
    number_const_id_t id;
    long value;
} number_mpz_const_source_t;

static const number_mpz_const_source_t number_mpz_const_sources[] = {
    { NUMBER_CONST_ZERO, 0L },
    { NUMBER_CONST_ONE, 1L },
    { NUMBER_CONST_NEG_ONE, -1L },
    { NUMBER_CONST_TWO, 2L },
    { NUMBER_CONST_TEN, 10L }
};

static int number_mpz_init(number_mpz_t *value)
{
    if (!value || value->initialised)
        return value ? 0 : -1;
    mpz_init(value->value);
    value->initialised = true;
    return 0;
}

static bool number_mpz_const_long(number_const_id_t id, long *out)
{
    for (size_t i = 0u;
         i < sizeof(number_mpz_const_sources) / sizeof(number_mpz_const_sources[0]);
         ++i) {
        if (number_mpz_const_sources[i].id == id) {
            if (out)
                *out = number_mpz_const_sources[i].value;
            return true;
        }
    }
    return false;
}

number_mpz_t *number_mpz_from_const_id(number_const_id_t id)
{
    number_mpz_t *out;
    long ignored;

    if (!number_mpz_const_long(id, &ignored))
        return NULL;
    out = malloc(sizeof(*out));
    if (!out)
        return NULL;
    out->constant_id = id;
    out->immortal = false;
    out->initialised = false;
    return out;
}

number_mpz_t *number_mpz_new(void)
{
    number_mpz_t *out = malloc(sizeof(*out));

    if (!out)
        return NULL;
    out->constant_id = NUMBER_CONST_COUNT;
    out->immortal = false;
    out->initialised = false;
    if (number_mpz_init(out) != 0) {
        free(out);
        return NULL;
    }
    return out;
}

number_mpz_t *number_mpz_from_long(long value)
{
    number_mpz_t *out = number_mpz_new();

    if (!out)
        return NULL;
    mpz_set_si(out->value, value);
    return out;
}

number_mpz_t *number_mpz_from_text(const string_t *text)
{
    string_cursor_t *cursor;
    string_t *digits;
    number_mpz_t *out;
    string_pos_t start;
    int rc;

    if (!text)
        return NULL;

    cursor = string_cursor_new(text);
    if (!cursor)
        return NULL;

    if (string_cursor_consume(cursor, "+"))
        start = string_cursor_position(cursor);
    else
        start = 0u;

    digits = string_cursor_slice_between(start,
                                         string_cursor_end_position(cursor),
                                         cursor);
    string_cursor_free(cursor);
    if (!digits)
        return NULL;

    out = number_mpz_new();
    if (!out) {
        string_free(digits);
        return NULL;
    }

    rc = mpz_set_str(out->value, string_c_str(digits), 10);
    string_free(digits);
    if (rc != 0) {
        number_mpz_free(out);
        return NULL;
    }
    return out;
}

number_mpz_t *number_mpz_from_mpz(mpz_srcptr value)
{
    number_mpz_t *out;

    if (!value)
        return NULL;
    out = number_mpz_new();
    if (!out)
        return NULL;
    mpz_set(out->value, value);
    return out;
}

number_mpz_t *number_mpz_clone(const number_mpz_t *value)
{
    number_mpz_t *out;

    if (!value || number_mpz_ensure(value) != 0)
        return NULL;
    out = number_mpz_new();
    if (!out)
        return NULL;
    mpz_set(out->value, value->value);
    return out;
}

void number_mpz_free(number_mpz_t *value)
{
    if (!value || value->immortal)
        return;
    if (value->initialised)
        mpz_clear(value->value);
    free(value);
}

int number_mpz_ensure(const number_mpz_t *value)
{
    number_mpz_t *mutable_value = (number_mpz_t *)value;
    long source;

    if (!value)
        return -1;
    if (value->initialised)
        return 0;
    if (!number_mpz_const_long(value->constant_id, &source))
        return -1;
    if (number_mpz_init(mutable_value) != 0)
        return -1;
    mpz_set_si(mutable_value->value, source);
    return 0;
}

mpz_srcptr number_mpz_value(const number_mpz_t *value)
{
    return number_mpz_ensure(value) == 0 ? value->value : NULL;
}

string_t *number_mpz_to_text(const number_mpz_t *value)
{
    char *text;
    string_t *out;

    if (number_mpz_ensure(value) != 0)
        return NULL;
    text = mpz_get_str(NULL, 10, value->value);
    if (!text)
        return NULL;
    out = string_new_with(text);
    free(text);
    return out;
}

bool number_mpz_get_long(const number_mpz_t *value, long *out)
{
    if (!out || number_mpz_ensure(value) != 0 || !mpz_fits_slong_p(value->value))
        return false;
    *out = mpz_get_si(value->value);
    return true;
}

size_t number_mpz_bit_length(const number_mpz_t *value)
{
    if (number_mpz_ensure(value) != 0 || mpz_sgn(value->value) == 0)
        return 0u;
    return mpz_sizeinbase(value->value, 2);
}

int number_mpz_cmp(const number_mpz_t *a, const number_mpz_t *b)
{
    if (number_mpz_ensure(a) != 0 || number_mpz_ensure(b) != 0)
        return 0;
    return mpz_cmp(a->value, b->value);
}

bool number_mpz_is_zero(const number_mpz_t *value)
{
    return number_mpz_ensure(value) == 0 && mpz_sgn(value->value) == 0;
}

bool number_mpz_is_negative(const number_mpz_t *value)
{
    return number_mpz_ensure(value) == 0 && mpz_sgn(value->value) < 0;
}
