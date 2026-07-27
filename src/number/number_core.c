#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_NUMBER_IMPLEMENTATION
#include "number.h"
#define MARS_NUMBER_INTERNAL_ACCESS
#include "number_internal.h"
#include "number_scope_alloc.h"
#include "ustring.h"

#include <complex.h>
#undef complex

typedef struct number_scope_record_t {
    number_kind_t kind;
    void *payload;
} number_scope_record_t;

#define NUMBER_SCOPE_BLOCK_CAPACITY 64u
#define NUMBER_SCOPE_ARENA_BLOCK_BYTES 16384u
#define NUMBER_SCOPE_ALLOC_MAGIC 0x4e53434fu
#define NUMBER_SCOPE_ALLOC_DESTROYED 0x1u
#define NUMBER_SCOPE_ALLOC_TRACKED   0x2u

typedef struct number_scope_block_t {
    struct number_scope_block_t *next;
    size_t used;
    number_scope_record_t records[NUMBER_SCOPE_BLOCK_CAPACITY];
} number_scope_block_t;

typedef struct number_scope_arena_block_t {
    struct number_scope_arena_block_t *next;
    size_t used;
    size_t capacity;
    unsigned char data[];
} number_scope_arena_block_t;

typedef struct number_scope_alloc_header_t {
    uint32_t magic;
    uint32_t reserved;
    size_t alloc_offset;
    size_t base_offset;
    size_t size;
    num_scope_t *scope;
    number_scope_record_t *record;
    number_scope_arena_block_t *arena_block;
} number_scope_alloc_header_t;

typedef struct number_scope_state_t {
    number_scope_block_t *records;
    number_scope_arena_block_t *arena_blocks;
} number_scope_state_t;

struct num_scope_t {
    number_scope_state_t *state;
    struct num_scope_t *previous;
    int active;
};

static num_scope_t *number_scope_current = NULL;
static size_t number_scope_suspend_depth = 0u;
static bool number_value_is_immortal(const number_t *number);
void number_destroy_none(number_t *number);

const number_math_family_t number_math_family_binary_table[][NUMBER_MATH_COMPLEX + 1] = {
    [NUMBER_MATH_INVALID] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_MPFR] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_COMPLEX] = NUMBER_MATH_INVALID
    },
    [NUMBER_MATH_QREAL] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_QREAL,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_QCOMPLEX,
        [NUMBER_MATH_MPFR] = NUMBER_MATH_MPFR,
        [NUMBER_MATH_COMPLEX] = NUMBER_MATH_COMPLEX
    },
    [NUMBER_MATH_QCOMPLEX] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_QCOMPLEX,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_QCOMPLEX,
        [NUMBER_MATH_MPFR] = NUMBER_MATH_COMPLEX,
        [NUMBER_MATH_COMPLEX] = NUMBER_MATH_COMPLEX
    },
    [NUMBER_MATH_MPFR] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_MPFR,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_COMPLEX,
        [NUMBER_MATH_MPFR] = NUMBER_MATH_MPFR,
        [NUMBER_MATH_COMPLEX] = NUMBER_MATH_COMPLEX
    },
    [NUMBER_MATH_COMPLEX] = {
        [NUMBER_MATH_INVALID] = NUMBER_MATH_INVALID,
        [NUMBER_MATH_QREAL] = NUMBER_MATH_COMPLEX,
        [NUMBER_MATH_QCOMPLEX] = NUMBER_MATH_COMPLEX,
        [NUMBER_MATH_MPFR] = NUMBER_MATH_COMPLEX,
        [NUMBER_MATH_COMPLEX] = NUMBER_MATH_COMPLEX
    }
};

const number_kind_t number_math_family_target_kind_table[] = {
    [NUMBER_MATH_INVALID] = NUMBER_INVALID,
    [NUMBER_MATH_QREAL] = NUMBER_QFLOAT,
    [NUMBER_MATH_QCOMPLEX] = NUMBER_QCOMPLEX,
    [NUMBER_MATH_MPFR] = NUMBER_MPFR,
    [NUMBER_MATH_COMPLEX] = NUMBER_COMPLEX
};

size_t number_default_precision_bits = 1024u;

_Static_assert(sizeof(number_private_t) <= sizeof(number_t),
    "number_t public storage is too small for internal representation");
_Static_assert(_Alignof(number_private_t) <= _Alignof(number_t),
    "number_t public storage alignment is too small for internal representation");

typedef struct {
    const char *name;
    const number_t *value;
    bool canonical;
} number_constant_name_t;

enum {
    NUMBER_CONSTANT_NAME_COUNT = 128,
    NUMBER_CONSTANT_HASH_VERTEX_COUNT = 149,
    NUMBER_CONSTANT_HASH_SEED0 = 105264080u,
    NUMBER_CONSTANT_HASH_SEED1 = 343043389u,
    NUMBER_CONSTANT_HASH_SEED2 = 755892437u
};

static const unsigned char number_constant_hash_g[NUMBER_CONSTANT_HASH_VERTEX_COUNT] = {
      9,  35,  81,  30,  34,  22,  81,  37,  70, 105,   0,  32, 116,   0,  12,  20,
     63,  86,   0,  97,  46,  74,  99,   0,  67,  37,  44,  52,   0,  48,  33,  34,
     11,  48,  56,   1, 123, 120,  45,  67,   1,   0, 101,   0,   2, 125, 102,   0,
     94,  72,   8,  23,   0,  34, 111,  29,  24,  48,   0,  11,   0,  48,   0,  67,
      0,  89,  88,  16,   0,  18,  60,  95, 103,   6,  34,  31,  56,  62,  68,  95,
     84,   2, 126,   0,  21,  39,   0, 113, 118,  12,  74, 102,  87,   0,  16, 123,
     92,  63,   4,  61,  29,   4,  76,  66,  78,  38, 127,  80, 111,   0,  64,  74,
    108, 124,  66, 106,  71,  19, 112,  90,  27,  81,   0,  23,   0,  83,  13,  19,
     98,   2,  46,  31,  69,  94, 109,  94,  97, 100,  81,  14,   6,  18,  66, 110,
    110,  53,  78,   0, 106
};

static const number_constant_name_t number_constant_names[NUMBER_CONSTANT_NAME_COUNT] = {
    [  0] = { "1/e", &NUM_INV_E, true },
    [  1] = { "1/ln(2)", &NUM_INVLN2, true },
    [  2] = { "2*pi", &NUM_2PI, false },
    [  3] = { "log(sqrt(2@pi))", &NUM_LOG_SQRT_2PI, false },
    [  4] = { "-pi/2", &NUM_NEG_PI_2, false },
    [  5] = { "π²", &NUM_PI_SQUARED, true },
    [  6] = { "ln(2*pi)", &NUM_LN_2PI, false },
    [  7] = { "(2pi)^3", &NUM_2PI_CUBED, false },
    [  8] = { "sqrt(@pi)", &NUM_SQRT_PI, false },
    [  9] = { "-π/2", &NUM_NEG_PI_2, true },
    [ 10] = { "∞", &NUM_INF, true },
    [ 11] = { "1/sqrt(pi)", &NUM_SQRT1ONPI, false },
    [ 12] = { "10", &NUM_TEN, true },
    [ 13] = { "2/sqrt(pi)", &NUM_2_SQRTPI, false },
    [ 14] = { "π/2", &NUM_PI_2, true },
    [ 15] = { "π/3", &NUM_PI_3, true },
    [ 16] = { "π/6", &NUM_PI_6, true },
    [ 17] = { "π/4", &NUM_PI_4, true },
    [ 18] = { "pi", &NUM_PI, false },
    [ 19] = { "1/√(2π)", &NUM_INV_SQRT_2PI, true },
    [ 20] = { "sqrt(2)/2", &NUM_SQRT2_OVER_TWO, false },
    [ 21] = { "pi^2", &NUM_PI_SQUARED, false },
    [ 22] = { "2/@pi", &NUM_2_PI, false },
    [ 23] = { "0", &NUM_ZERO, true },
    [ 24] = { "1", &NUM_ONE, true },
    [ 25] = { "2", &NUM_TWO, true },
    [ 26] = { "sqrt2/2", &NUM_SQRT2_OVER_TWO, false },
    [ 27] = { "√2", &NUM_SQRT2, true },
    [ 28] = { "√3", &NUM_SQRT3, true },
    [ 29] = { "log(sqrt(2*pi))", &NUM_LOG_SQRT_2PI, false },
    [ 30] = { "-1", &NUM_NEG_ONE, true },
    [ 31] = { "-@pi/2", &NUM_NEG_PI_2, false },
    [ 32] = { "ln10", &NUM_LN10, false },
    [ 33] = { "sqrt_pi_over_two", &NUM_SQRT_PI_OVER_TWO, false },
    [ 34] = { "@pi^2", &NUM_PI_SQUARED, false },
    [ 35] = { "sqrt(@pi/2)", &NUM_SQRT_PI_OVER_TWO, false },
    [ 36] = { "3π/4", &NUM_3PI_4, true },
    [ 37] = { "i", &NUM_I, true },
    [ 38] = { "1/ln2", &NUM_INVLN2, false },
    [ 39] = { "3*pi/4", &NUM_3PI_4, false },
    [ 40] = { "e", &NUM_E, true },
    [ 41] = { "-i", &NUM_NEG_I, true },
    [ 42] = { "sqrt(2@pi)", &NUM_SQRT_2PI, false },
    [ 43] = { "√π", &NUM_SQRT_PI, true },
    [ 44] = { "-2/√π", &NUM_NEG_TWO_OVER_SQRT_PI, true },
    [ 45] = { "-2/sqrt(@pi)", &NUM_NEG_TWO_OVER_SQRT_PI, false },
    [ 46] = { "1/sqrt(@pi)", &NUM_SQRT1ONPI, false },
    [ 47] = { "2π", &NUM_2PI, true },
    [ 48] = { "inf", &NUM_INF, false },
    [ 49] = { "(2@pi)^3", &NUM_2PI_CUBED, false },
    [ 50] = { "(2*pi)^3", &NUM_2PI_CUBED, false },
    [ 51] = { "1/sqrt_pi", &NUM_SQRT1ONPI, false },
    [ 52] = { "phi", &NUM_PHI, false },
    [ 53] = { "sqrt(3)/2", &NUM_SQRT3_OVER_TWO, false },
    [ 54] = { "NaN", &NUM_NAN, true },
    [ 55] = { "@pi/6", &NUM_PI_6, false },
    [ 56] = { "@pi/4", &NUM_PI_4, false },
    [ 57] = { "-1/e", &NUM_NEG_INV_E, true },
    [ 58] = { "@pi/2", &NUM_PI_2, false },
    [ 59] = { "@pi/3", &NUM_PI_3, false },
    [ 60] = { "√(2π)", &NUM_SQRT_2PI, true },
    [ 61] = { "log(√(2π))", &NUM_LOG_SQRT_2PI, true },
    [ 62] = { "nan", &NUM_NAN, false },
    [ 63] = { "1/sqrt(2*pi)", &NUM_INV_SQRT_2PI, false },
    [ 64] = { "2pi", &NUM_2PI, false },
    [ 65] = { "sqrt3/2", &NUM_SQRT3_OVER_TWO, false },
    [ 66] = { "3/2", &NUM_ONE_AND_HALF, true },
    [ 67] = { "ln(2π)", &NUM_LN_2PI, true },
    [ 68] = { "sqrt_pi", &NUM_SQRT_PI, false },
    [ 69] = { "2/pi", &NUM_2_PI, false },
    [ 70] = { "-infinity", &NUM_NINF, false },
    [ 71] = { "gamma", &NUM_EULER_MASCHERONI, false },
    [ 72] = { "1/sqrt(2@pi)", &NUM_INV_SQRT_2PI, false },
    [ 73] = { "√3/2", &NUM_SQRT3_OVER_TWO, true },
    [ 74] = { "-2/sqrt(pi)", &NUM_NEG_TWO_OVER_SQRT_PI, false },
    [ 75] = { "√2/2", &NUM_SQRT2_OVER_TWO, true },
    [ 76] = { "2/sqrt(@pi)", &NUM_2_SQRTPI, false },
    [ 77] = { "(2π)³", &NUM_2PI_CUBED, true },
    [ 78] = { "2/π", &NUM_2_PI, true },
    [ 79] = { "@phi", &NUM_PHI, false },
    [ 80] = { "ln2", &NUM_LN2, false },
    [ 81] = { "2/sqrt_pi", &NUM_2_SQRTPI, false },
    [ 82] = { "ln(2pi)", &NUM_LN_2PI, false },
    [ 83] = { "-2/sqrt_pi", &NUM_NEG_TWO_OVER_SQRT_PI, false },
    [ 84] = { "ln(2)", &NUM_LN2, true },
    [ 85] = { "3@pi/4", &NUM_3PI_4, false },
    [ 86] = { "@pi", &NUM_PI, false },
    [ 87] = { "√(π/2)", &NUM_SQRT_PI_OVER_TWO, true },
    [ 88] = { "sqrt(2pi)", &NUM_SQRT_2PI, false },
    [ 89] = { "-∞", &NUM_NINF, true },
    [ 90] = { "@gamma", &NUM_EULER_MASCHERONI, false },
    [ 91] = { "sqrt(1/2)", &NUM_SQRT_HALF, false },
    [ 92] = { "1/√π", &NUM_SQRT1ONPI, true },
    [ 93] = { "ln(2@pi)", &NUM_LN_2PI, false },
    [ 94] = { "1/10", &NUM_ONE_TENTH, true },
    [ 95] = { "log(sqrt_2pi)", &NUM_LOG_SQRT_2PI, false },
    [ 96] = { "3pi/4", &NUM_3PI_4, false },
    [ 97] = { "invln2", &NUM_INVLN2, false },
    [ 98] = { "sqrt_2pi", &NUM_SQRT_2PI, false },
    [ 99] = { "sqrt(2*pi)", &NUM_SQRT_2PI, false },
    [100] = { "sqrt(pi/2)", &NUM_SQRT_PI_OVER_TWO, false },
    [101] = { "2@pi", &NUM_2PI, false },
    [102] = { "sqrt(2)", &NUM_SQRT2, false },
    [103] = { "sqrt(pi)", &NUM_SQRT_PI, false },
    [104] = { "2/√π", &NUM_2_SQRTPI, true },
    [105] = { "infinity", &NUM_INF, false },
    [106] = { "ln(10)", &NUM_LN10, true },
    [107] = { "1/sqrt_2pi", &NUM_INV_SQRT_2PI, false },
    [108] = { "sqrt(3)", &NUM_SQRT3, false },
    [109] = { "log(sqrt(2pi))", &NUM_LOG_SQRT_2PI, false },
    [110] = { "1/3", &NUM_ONE_THIRD, true },
    [111] = { "1/2", &NUM_HALF, true },
    [112] = { "1/4", &NUM_QUARTER, true },
    [113] = { "1/6", &NUM_ONE_SIXTH, true },
    [114] = { "1/8", &NUM_ONE_EIGHTH, true },
    [115] = { "sqrt2", &NUM_SQRT2, false },
    [116] = { "sqrt3", &NUM_SQRT3, false },
    [117] = { "pi/4", &NUM_PI_4, false },
    [118] = { "pi/6", &NUM_PI_6, false },
    [119] = { "pi/2", &NUM_PI_2, false },
    [120] = { "pi/3", &NUM_PI_3, false },
    [121] = { "sqrt_half", &NUM_SQRT_HALF, false },
    [122] = { "1/sqrt(2pi)", &NUM_INV_SQRT_2PI, false },
    [123] = { "φ", &NUM_PHI, true },
    [124] = { "π", &NUM_PI, true },
    [125] = { "-inf", &NUM_NINF, false },
    [126] = { "γ", &NUM_EULER_MASCHERONI, true },
    [127] = { "√(1/2)", &NUM_SQRT_HALF, true },
};

_Static_assert(NUMBER_CONSTANT_NAME_COUNT ==
               sizeof(number_constant_names) / sizeof(number_constant_names[0]),
    "number constant name count mismatch");
_Static_assert(NUMBER_CONSTANT_HASH_VERTEX_COUNT ==
               sizeof(number_constant_hash_g) / sizeof(number_constant_hash_g[0]),
    "number constant hash vertex count mismatch");

static size_t number_constant_name_count(void)
{
    return NUMBER_CONSTANT_NAME_COUNT;
}

static uint32_t number_constant_hash_feed_byte(uint32_t hash, unsigned char byte)
{
    return (hash * 65599u) ^ (uint32_t)byte;
}

static uint32_t number_constant_hash_feed_rune(uint32_t hash, uint32_t rune)
{
    if (rune <= 0x7fu)
        return number_constant_hash_feed_byte(hash, (unsigned char)rune);
    if (rune <= 0x7ffu) {
        hash = number_constant_hash_feed_byte(hash,
            (unsigned char)(0xc0u | (rune >> 6)));
        return number_constant_hash_feed_byte(hash,
            (unsigned char)(0x80u | (rune & 0x3fu)));
    }
    if (rune <= 0xffffu) {
        hash = number_constant_hash_feed_byte(hash,
            (unsigned char)(0xe0u | (rune >> 12)));
        hash = number_constant_hash_feed_byte(hash,
            (unsigned char)(0x80u | ((rune >> 6) & 0x3fu)));
        return number_constant_hash_feed_byte(hash,
            (unsigned char)(0x80u | (rune & 0x3fu)));
    }

    hash = number_constant_hash_feed_byte(hash,
        (unsigned char)(0xf0u | (rune >> 18)));
    hash = number_constant_hash_feed_byte(hash,
        (unsigned char)(0x80u | ((rune >> 12) & 0x3fu)));
    hash = number_constant_hash_feed_byte(hash,
        (unsigned char)(0x80u | ((rune >> 6) & 0x3fu)));
    return number_constant_hash_feed_byte(hash,
        (unsigned char)(0x80u | (rune & 0x3fu)));
}

static uint32_t number_constant_hash_view(string_view_t view, uint32_t seed)
{
    uint32_t hash = seed;
    size_t len = string_view_length(view);
    string_pos_t pos = 0u;

    while (pos < len) {
        uint32_t rune = 0u;
        string_pos_t next = 0u;

        if (!string_view_peek_rune_value(view, pos, &rune, &next) ||
            next <= pos) {
            break;
        }

        hash = number_constant_hash_feed_rune(hash, rune);
        pos = next;
    }

    hash ^= (uint32_t)(len * 0x9e3779b9u);
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    return hash;
}

static size_t number_constant_hash_index(string_view_t view)
{
    size_t vertex0;
    size_t vertex1;
    size_t vertex2;

    vertex0 = number_constant_hash_view(view, NUMBER_CONSTANT_HASH_SEED0) %
              NUMBER_CONSTANT_HASH_VERTEX_COUNT;
    vertex1 = number_constant_hash_view(view, NUMBER_CONSTANT_HASH_SEED1) %
              NUMBER_CONSTANT_HASH_VERTEX_COUNT;
    vertex2 = number_constant_hash_view(view, NUMBER_CONSTANT_HASH_SEED2) %
              NUMBER_CONSTANT_HASH_VERTEX_COUNT;

    return ((size_t)number_constant_hash_g[vertex0] +
            (size_t)number_constant_hash_g[vertex1] +
            (size_t)number_constant_hash_g[vertex2]) %
           NUMBER_CONSTANT_NAME_COUNT;
}

static bool number_constant_value_view(string_view_t view, number_t *out)
{
    const number_constant_name_t *entry;
    size_t len;
    size_t index;

    if (!out)
        return false;

    view = string_view_trim(view);
    len = string_view_length(view);
    if (len == 0u)
        return false;

    index = number_constant_hash_index(view);
    entry = &number_constant_names[index];

    if (string_view_equals_literal(view, entry->name)) {
        *out = num_clone(*entry->value);
        return true;
    }

    return false;
}

bool num_constant_value_text(const string_t *text, number_t *out)
{
    return text ? number_constant_value_view(string_view_all(text), out) : false;
}

bool num_constant_value(const char *text, number_t *out)
{
    string_t *owned;
    bool found;

    if (!out)
        return false;

    owned = text ? string_new_with(text) : NULL;
    if (!owned)
        return false;

    found = num_constant_value_text(owned, out);
    string_free(owned);
    return found;
}

const char *num_constant_name(number_t value)
{
    number_const_id_t constant_id;
    number_const_id_t candidate_id;
    size_t precision_bits = 0u;

    if (num_is_nan(value))
        return "NAN";
    if (number_const_id_from_immortal(&value, &constant_id)) {
        for (size_t i = 0u; i < number_constant_name_count(); ++i) {
            if (number_constant_names[i].canonical &&
                number_const_id_from_immortal(number_constant_names[i].value,
                                              &candidate_id) &&
                candidate_id == constant_id)
                return number_constant_names[i].name;
        }
        return NULL;
    }

    if (num_is_exact(value))
        return NULL;

    precision_bits = num_get_prec_bits(value);
    if (precision_bits == 0u)
        precision_bits = num_get_effective_prec_bits(value);
    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;

    for (size_t i = 0u; i < number_constant_name_count(); ++i) {
        number_t candidate;
        bool match;

        if (!number_constant_names[i].canonical ||
            num_is_exact(*number_constant_names[i].value))
            continue;

        candidate = num_const_prec(*number_constant_names[i].value,
                                   precision_bits);
        match = num_eq(value, candidate);
        num_destroy(&candidate);
        if (match)
            return number_constant_names[i].name;
    }

    return NULL;
}

static number_t *number_alloc(number_kind_t kind)
{
    number_t *number;

    number = calloc(1, sizeof(*number));
    if (!number)
        return NULL;
    number_impl(number)->kind = kind;
    return number;
}

static number_scope_state_t *number_scope_state_get(const num_scope_t *scope)
{
    return scope ? scope->state : NULL;
}

static number_scope_state_t *number_scope_state_ensure(num_scope_t *scope)
{
    number_scope_state_t *state;

    if (!scope)
        return NULL;
    state = number_scope_state_get(scope);
    if (state)
        return state;
    state = (number_scope_state_t *)calloc(1, sizeof(*state));
    if (!state)
        return NULL;
    scope->state = state;
    return state;
}

static size_t number_scope_align_up(size_t value, size_t align)
{
    size_t mask;

    if (align <= 1u)
        return value;
    mask = align - 1u;
    return (value + mask) & ~mask;
}

void *number_scope_mem_alloc_heap(size_t size, size_t align)
{
    size_t total_needed;
    unsigned char *raw;
    size_t payload_offset;
    number_scope_alloc_header_t *header;

    if (size == 0u)
        return NULL;
    if (align < _Alignof(max_align_t))
        align = _Alignof(max_align_t);
    total_needed = sizeof(number_scope_alloc_header_t) + align - 1u + size;
    raw = (unsigned char *)malloc(total_needed);
    if (!raw)
        return NULL;
    payload_offset = number_scope_align_up(sizeof(number_scope_alloc_header_t), align);
    header = (number_scope_alloc_header_t *)(raw + payload_offset - sizeof(*header));
    header->magic = NUMBER_SCOPE_ALLOC_MAGIC;
    header->reserved = 0u;
    header->alloc_offset = 0u;
    header->base_offset = payload_offset;
    header->size = size;
    header->scope = NULL;
    header->record = NULL;
    header->arena_block = NULL;
    return raw + payload_offset;
}

static number_scope_alloc_header_t *number_scope_alloc_header_from_ptr(const void *ptr)
{
    const number_scope_alloc_header_t *header;

    if (!ptr)
        return NULL;
    header = (const number_scope_alloc_header_t *)((const unsigned char *)ptr - sizeof(*header));
    return header->magic == NUMBER_SCOPE_ALLOC_MAGIC
        ? (number_scope_alloc_header_t *)header : NULL;
}

static void *number_scope_arena_alloc_from_scope(num_scope_t *scope,
                                                 size_t size,
                                                 size_t align)
{
    number_scope_state_t *state;
    number_scope_arena_block_t *block;
    size_t total_needed;

    if (!scope || size == 0u)
        return NULL;
    if (align < _Alignof(max_align_t))
        align = _Alignof(max_align_t);
    state = number_scope_state_ensure(scope);
    if (!state)
        return NULL;

    total_needed = sizeof(number_scope_alloc_header_t) + align - 1u + size;
    block = state->arena_blocks;
    if (!block || block->used + total_needed > block->capacity) {
        size_t capacity = NUMBER_SCOPE_ARENA_BLOCK_BYTES;

        if (capacity < total_needed)
            capacity = total_needed;
        block = (number_scope_arena_block_t *)malloc(sizeof(*block) + capacity);
        if (!block)
            return NULL;
        block->next = state->arena_blocks;
        block->used = 0u;
        block->capacity = capacity;
        state->arena_blocks = block;
    }

    {
        size_t base = block->used;
        size_t payload_offset = number_scope_align_up(base + sizeof(number_scope_alloc_header_t), align);
        number_scope_alloc_header_t *header =
            (number_scope_alloc_header_t *)(block->data + payload_offset - sizeof(*header));
        void *payload = block->data + payload_offset;

        header->magic = NUMBER_SCOPE_ALLOC_MAGIC;
        header->reserved = 0u;
        header->alloc_offset = base;
        header->base_offset = payload_offset;
        header->size = size;
        header->scope = scope;
        header->record = NULL;
        header->arena_block = block;
        block->used = payload_offset + size;
        return payload;
    }
}

num_scope_t *number_scope_suspend(void)
{
    number_scope_suspend_depth++;
    return number_scope_current;
}

void number_scope_resume(num_scope_t *scope)
{
    (void)scope;
    if (number_scope_suspend_depth > 0u)
        number_scope_suspend_depth--;
}

void num_scope_resume_cleanup(num_scope_t **scope)
{
    number_scope_resume(scope ? *scope : NULL);
}

static void *number_scope_payload_pointer(const number_t *number)
{
    const number_vtable_t *vt;

    if (!number || !number_is_valid_value(number))
        return NULL;
    vt = number_vt(number);
    return vt && vt->scope_payload ? vt->scope_payload(number) : NULL;
}

static bool number_scope_trackable_value(const number_t *number)
{
    const number_vtable_t *vt;
    void *payload;

    if (!number_scope_current || number_scope_suspend_depth > 0u ||
        !number || !number_is_valid_value(number))
        return false;
    vt = number_vt(number);
    if (!vt || !vt->destroy_payload || vt->destroy_payload == number_destroy_none)
        return false;
    if (number_value_is_immortal(number))
        return false;
    payload = number_scope_payload_pointer(number);
    if (!payload)
        return false;
    return true;
}

static void number_scope_destroy_record(const number_scope_record_t *record)
{
    const number_vtable_t *vt;

    if (!record)
        return;
    vt = (unsigned)record->kind < number_dispatch_count
        ? number_dispatch[record->kind] : NULL;
    if (!vt || !vt->destroy_scope_payload)
        return;
    vt->destroy_scope_payload(record->payload);
}

static void number_scope_trim_block(number_scope_block_t *block)
{
    if (!block)
        return;
    while (block->used > 0u &&
           block->records[block->used - 1u].payload == NULL)
        block->used--;
}

void number_scope_register_value(const number_t *number)
{
    num_scope_t *scope;
    number_scope_state_t *state;
    number_scope_block_t *block;
    number_scope_record_t *record;
    number_kind_t kind;
    void *payload;

    if (!number_scope_trackable_value(number))
        return;
    kind = number_kind_value(number);
    payload = number_scope_payload_pointer(number);

    /* Some conversion paths return an already-scoped payload. Registering it
     * again would leave a second owner behind after num_scope_detach(). */
    for (scope = number_scope_current; scope; scope = scope->previous) {
        number_scope_state_t *existing_state = number_scope_state_get(scope);
        number_scope_block_t *existing_block = existing_state
            ? existing_state->records : NULL;

        while (existing_block) {
            size_t i = existing_block->used;

            while (i-- > 0u) {
                const number_scope_record_t *existing =
                    &existing_block->records[i];

                if (existing->kind == kind && existing->payload == payload)
                    return;
            }
            existing_block = existing_block->next;
        }
    }

    state = number_scope_state_ensure(number_scope_current);
    if (!state)
        return;
    block = state->records;
    if (!block || block->used == NUMBER_SCOPE_BLOCK_CAPACITY) {
        block = (number_scope_block_t *)calloc(1, sizeof(*block));
        if (!block)
            return;
        block->next = state->records;
        state->records = block;
    }
    record = &block->records[block->used++];
    record->kind = kind;
    record->payload = payload;
}

int number_scope_unregister_value(const number_t *number)
{
    num_scope_t *scope;
    number_kind_t kind;
    void *payload;

    if (!number || !number_is_valid_value(number))
        return 0;
    kind = number_kind_value(number);
    payload = number_scope_payload_pointer(number);
    if (!payload)
        return 0;

    for (scope = number_scope_current; scope; scope = scope->previous) {
        number_scope_state_t *state = number_scope_state_get(scope);
        number_scope_block_t *block = state ? state->records : NULL;

        while (block) {
            size_t i = block->used;

            while (i-- > 0u) {
                number_scope_record_t *record = &block->records[i];

                if (record->payload == NULL)
                    continue;
                if (record->kind == kind && record->payload == payload)
                {
                    record->kind = NUMBER_INVALID;
                    record->payload = NULL;
                    number_scope_trim_block(block);
                    return 1;
                }
            }
            block = block->next;
        }
    }
    return 0;
}

void *number_scope_mem_alloc(size_t size, size_t align)
{
    if (size == 0u)
        return NULL;
    return (number_scope_current && number_scope_suspend_depth == 0u)
        ? number_scope_arena_alloc_from_scope(number_scope_current, size, align)
        : number_scope_mem_alloc_heap(size, align);
}

void *number_scope_mem_calloc(size_t count, size_t size, size_t align)
{
    size_t total;
    void *ptr;

    if (count == 0u || size == 0u)
        return NULL;
    if (SIZE_MAX / count < size)
        return NULL;
    total = count * size;
    ptr = number_scope_mem_alloc(total, align);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void *number_scope_mem_calloc_heap(size_t count, size_t size, size_t align)
{
    size_t total;
    void *ptr;

    if (count == 0u || size == 0u)
        return NULL;
    if (SIZE_MAX / count < size)
        return NULL;
    total = count * size;
    ptr = number_scope_mem_alloc_heap(total, align);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void *number_scope_mem_realloc(void *ptr, size_t size, size_t align)
{
    number_scope_alloc_header_t *header;

    if (!ptr)
        return number_scope_mem_alloc(size, align);
    if (size == 0u) {
        number_scope_mem_free(ptr);
        return NULL;
    }
    header = number_scope_alloc_header_from_ptr(ptr);
    if (!header)
        return realloc(ptr, size);
    if (size <= header->size) {
        if (header->scope && header->arena_block) {
            size_t payload_offset = header->base_offset;
            size_t old_end = payload_offset + header->size;
            number_scope_arena_block_t *block = header->arena_block;

            if (block->used == old_end)
                block->used = payload_offset + size;
        }
        header->size = size;
        return ptr;
    }

    {
        if (header->scope && header->arena_block) {
            size_t payload_offset = header->base_offset;
            size_t old_end = payload_offset + header->size;
            number_scope_arena_block_t *block = header->arena_block;

            if (block->used == old_end &&
                payload_offset + size <= block->capacity)
            {
                block->used = payload_offset + size;
                header->size = size;
                return ptr;
            }
        }
        void *grown = header->scope
            ? number_scope_arena_alloc_from_scope(header->scope, size, align)
            : number_scope_mem_alloc_heap(size, align);

        if (!grown)
            return NULL;
        memcpy(grown, ptr, header->size);
        if (header->record) {
            number_scope_alloc_header_t *grown_header =
                number_scope_alloc_header_from_ptr(grown);

            if (grown_header) {
                grown_header->record = header->record;
                header->record->payload = grown;
                header->record = NULL;
            }
        }
        if (!header->scope)
            free((unsigned char *)ptr - header->base_offset);
        return grown;
    }
}

void number_scope_mem_free(void *ptr)
{
    number_scope_alloc_header_t *header;

    if (!ptr)
        return;
    header = number_scope_alloc_header_from_ptr(ptr);
    if (!header)
    {
        free(ptr);
        return;
    }
    if (header->scope)
        return;
    free((unsigned char *)ptr - header->base_offset);
}

bool number_scope_mem_is_arena_ptr(const void *ptr)
{
    number_scope_alloc_header_t *header = number_scope_alloc_header_from_ptr(ptr);

    return header != NULL && header->scope != NULL;
}

number_t number_invalid(void)
{
    number_t number;

    number.storage[0] = NUMBER_INVALID;
    return number;
}

static number_t number_make_double(double value)
{
    number_t number;

    number.storage[0] = NUMBER_DOUBLE;
    number_impl(&number)->value.d = value;
    return number;
}

static number_t number_make_cdouble(double _Complex value)
{
    number_t number;

    number.storage[0] = NUMBER_CDOUBLE;
    number_impl(&number)->value.cd.value = value;
    number_impl(&number)->value.cd.constant_id = NUMBER_CONST_COUNT;
    number_impl(&number)->value.cd.immortal = false;
    return number;
}

static number_t number_make_qfloat(qfloat_t value)
{
    number_t number;

    number.storage[0] = NUMBER_QFLOAT;
    number_impl(&number)->value.qf = value;
    return number;
}

static number_t number_make_qcomplex(qcomplex_t value)
{
    number_t number;

    number.storage[0] = NUMBER_QCOMPLEX;
    number_impl(&number)->value.qc = value;
    return number;
}

number_t number_take_mpfr(number_mpfr_t *value)
{
    number_t number;

    if (!value)
        return number_invalid();
    number.storage[0] = NUMBER_MPFR;
    number_impl(&number)->value.mpfr = value;
    number_scope_register_value(&number);
    return number;
}

number_t number_take_mpz(number_mpz_t *value)
{
    number_t number;

    if (!value)
        return number_invalid();
    number.storage[0] = NUMBER_MPZ;
    number_impl(&number)->value.mpz = value;
    number_scope_register_value(&number);
    return number;
}

static bool number_mpq_exceeds_working_precision_budget(const number_mpq_t *value)
{
    size_t precision_bits;
    size_t numerator_bits;
    size_t denominator_bits;
    mpq_srcptr q;

    if (!value || number_mpq_ensure(value) != 0)
        return false;

    precision_bits = number_default_precision_bits;
    if (precision_bits == 0u)
        return false;

    q = value->value;
    numerator_bits = mpz_sizeinbase(mpq_numref(q), 2);
    denominator_bits = mpz_sizeinbase(mpq_denref(q), 2);

    return denominator_bits > precision_bits ||
           numerator_bits + denominator_bits > 2u * precision_bits;
}

static number_t number_take_mpq_budgeted(number_t number)
{
    number_t converted;
    number_mpq_t *value = number_impl(&number)->value.mpq;

    if (!value || !number_mpq_exceeds_working_precision_budget(value)) {
        number_scope_register_value(&number);
        return number;
    }

    converted = num_as_inexact_real_prec(number, number_default_precision_bits);
    if (num_is_nan(converted)) {
        number_scope_register_value(&number);
        return number;
    }

    number_mpq_free(value);
    number_scope_register_value(&converted);
    return converted;
}

number_t number_take_mpq(number_mpq_t *value)
{
    number_t number;

    if (!value)
        return number_invalid();
    number.storage[0] = NUMBER_MPQ;
    number_impl(&number)->value.mpq = value;
    return number_take_mpq_budgeted(number);
}

number_t number_take(number_t *boxed_number)
{
    number_t value;

    if (!boxed_number)
        return number_invalid();
    memcpy(&value, boxed_number, sizeof(value));
    free(boxed_number);
    if (number_kind_value(&value) == NUMBER_MPQ)
        return number_take_mpq_budgeted(value);
    number_scope_register_value(&value);
    return value;
}

number_t *number_box_value(number_t value)
{
    number_t *boxed = malloc(sizeof(*boxed));

    if (!boxed) {
        num_destroy(&value);
        return NULL;
    }
    number_scope_unregister_value(&value);
    memcpy(boxed, &value, sizeof(*boxed));
    return boxed;
}

static unsigned char number_ascii_lower(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (unsigned char)(ch - 'A' + 'a') : ch;
}

static bool number_text_has_ascii(const string_t *text, char needle)
{
    string_cursor_t *cursor;
    unsigned char ch;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &ch) &&
            ch == (unsigned char)needle) {
            string_cursor_free(cursor);
            return true;
        }
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return false;
}

static bool number_text_has_ascii_ci(const string_t *text, char needle)
{
    string_cursor_t *cursor;
    unsigned char want = number_ascii_lower((unsigned char)needle);
    unsigned char ch;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &ch) &&
            number_ascii_lower(ch) == want) {
            string_cursor_free(cursor);
            return true;
        }
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return false;
}

static bool number_text_has_unicode_fraction(const string_t *text)
{
    string_cursor_t *cursor;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (rune_is_fraction(string_cursor_peek(cursor))) {
            string_cursor_free(cursor);
            return true;
        }
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return false;
}

static bool number_text_is_decimal(const string_t *text)
{
    return text &&
           (number_text_has_ascii(text, '.') ||
            number_text_has_ascii_ci(text, 'e') ||
            number_text_has_ascii_ci(text, 'n') ||
            number_text_has_ascii_ci(text, 'f'));
}

static bool number_kind_is_complex_component(number_kind_t kind)
{
    return kind == NUMBER_MPZ || kind == NUMBER_MPQ || kind == NUMBER_MPFR;
}

static bool number_is_mpfr_complex_components(const complex_t *value)
{
    return value &&
           number_kind_value(&value->real) == NUMBER_MPFR &&
           number_kind_value(&value->imag) == NUMBER_MPFR;
}

void number_complex_clear_mpc_cache(complex_t *value)
{
    if (!value || !value->mpc_cache_valid)
        return;
    mpc_clear(value->mpc_cache);
    value->mpc_cache_valid = false;
}

static int number_complex_set_mpc_cache_from_parts(complex_t *value,
                                                   const number_mpfr_t *real,
                                                   const number_mpfr_t *imag,
                                                   size_t precision_bits)
{
    if (!value || !real || !imag)
        return -1;
    precision_bits = precision_bits ? precision_bits : number_default_precision_bits;
    number_complex_clear_mpc_cache(value);
    mpc_init2(value->mpc_cache, (mpfr_prec_t)precision_bits);
    value->mpc_cache_valid = true;
    if (number_mpfr_ensure(real, precision_bits) != 0 ||
        number_mpfr_ensure(imag, precision_bits) != 0) {
        number_complex_clear_mpc_cache(value);
        return -1;
    }
    mpc_set_fr_fr(value->mpc_cache, real->value, imag->value, MPC_RNDNN);
    value->precision_bits = precision_bits;
    return 0;
}

int number_complex_set_mpc_cache_from_mpc(complex_t *value,
                                          mpc_srcptr source,
                                          size_t precision_bits)
{
    if (!value || !source)
        return -1;
    precision_bits = precision_bits ? precision_bits : (size_t)mpc_get_prec(source);
    number_complex_clear_mpc_cache(value);
    mpc_init2(value->mpc_cache, (mpfr_prec_t)precision_bits);
    value->mpc_cache_valid = true;
    mpc_set(value->mpc_cache, source, MPC_RNDNN);
    value->precision_bits = precision_bits;
    return 0;
}

static void number_complex_refresh_mpc_cache(complex_t *value)
{
    if (!number_is_mpfr_complex_components(value))
        return;
    (void)number_complex_set_mpc_cache_from_parts(
        value,
        number_impl_const(&value->real)->value.mpfr,
        number_impl_const(&value->imag)->value.mpfr,
        value->precision_bits ? value->precision_bits : num_get_prec_bits(value->real));
}

number_t number_complex_component_from_number(const number_t *value,
                                              size_t precision_bits)
{
    typedef enum {
        NUMBER_COMPONENT_REJECT = 0,
        NUMBER_COMPONENT_CLONE,
        NUMBER_COMPONENT_INEXACT_REAL
    } number_component_action_t;
    static const unsigned char action_table[] = {
        [NUMBER_INVALID] = NUMBER_COMPONENT_REJECT,
        [NUMBER_DOUBLE] = NUMBER_COMPONENT_INEXACT_REAL,
        [NUMBER_QFLOAT] = NUMBER_COMPONENT_INEXACT_REAL,
        [NUMBER_QCOMPLEX] = NUMBER_COMPONENT_REJECT,
        [NUMBER_MPZ] = NUMBER_COMPONENT_CLONE,
        [NUMBER_MPQ] = NUMBER_COMPONENT_CLONE,
        [NUMBER_MPFR] = NUMBER_COMPONENT_CLONE,
        [NUMBER_CDOUBLE] = NUMBER_COMPONENT_REJECT,
        [NUMBER_COMPLEX] = NUMBER_COMPONENT_REJECT
    };
    number_t out = number_invalid();
    number_kind_t kind;
    number_component_action_t action;

    if (!value || !number_is_valid_value(value))
        return out;
    kind = number_kind_value(value);
    action = (kind >= 0 && (size_t)kind < sizeof(action_table))
        ? (number_component_action_t)action_table[kind]
        : NUMBER_COMPONENT_REJECT;
    if (action == NUMBER_COMPONENT_CLONE)
        return num_clone(*value);
    if (action == NUMBER_COMPONENT_INEXACT_REAL)
        return num_as_inexact_real_prec(
            *value,
            precision_bits ? precision_bits : number_default_precision_bits);
    return out;
}

static number_t number_complex_component_from_qfloat(qfloat_t value,
                                                     size_t precision_bits)
{
    number_mpfr_t *mpfr_value;

    mpfr_value = number_mpfr_from_qfloat(value, precision_bits);
    return mpfr_value ? number_take(number_wrap_mpfr(mpfr_value)) : number_invalid();
}

complex_t *number_complex_create(number_t real, number_t imag)
{
    complex_t *out;

    if (!number_kind_is_complex_component(number_kind_value(&real)) ||
        !number_kind_is_complex_component(number_kind_value(&imag)))
        return NULL;
    out = (complex_t *)calloc(1, sizeof(*out));
    if (!out)
        return NULL;
    out->constant_id = NUMBER_CONST_COUNT;
    out->precision_bits = 0u;
    out->real = num_scope_detach(real);
    out->imag = num_scope_detach(imag);
    return out;
}

number_t *number_wrap_complex_parts(number_t real, number_t imag)
{
    complex_t *value = number_complex_create(real, imag);

    if (!value) {
        num_destroy(&real);
        num_destroy(&imag);
        return NULL;
    }
    return number_wrap_complex(value);
}

static void number_complex_free(complex_t *value)
{
    if (!value)
        return;
    number_complex_clear_mpc_cache(value);
    num_destroy(&value->real);
    num_destroy(&value->imag);
    free(value);
}

complex_t *number_complex_clone(const complex_t *value)
{
    number_t real;
    number_t imag;
    complex_t *out;

    if (!value)
        return NULL;
    real = num_clone(value->real);
    imag = num_clone(value->imag);
    out = number_complex_create(real, imag);
    if (!out) {
        num_destroy(&real);
        num_destroy(&imag);
        return NULL;
    }
    out->constant_id = value->constant_id;
    out->precision_bits = value->precision_bits;
    if (value->mpc_cache_valid)
        (void)number_complex_set_mpc_cache_from_mpc(out, value->mpc_cache,
            value->precision_bits);
    return out;
}

const number_t *number_complex_real_ref(const complex_t *value)
{
    return value ? &value->real : NULL;
}

const number_t *number_complex_imag_ref(const complex_t *value)
{
    return value ? &value->imag : NULL;
}

number_const_id_t number_complex_const_id(const complex_t *value)
{
    return value ? value->constant_id : NUMBER_CONST_COUNT;
}

complex_t *number_complex_create_from_qcomplex(qcomplex_t value,
                                               size_t precision_bits)
{
    number_t real = number_complex_component_from_qfloat(qc_real(value), precision_bits);
    number_t imag = number_complex_component_from_qfloat(qc_imag(value), precision_bits);
    complex_t *out = number_complex_create(real, imag);

    if (!out) {
        num_destroy(&real);
        num_destroy(&imag);
    }
    else {
        out->precision_bits = precision_bits;
        number_complex_refresh_mpc_cache(out);
    }
    return out;
}

static number_t number_complex_component_as_mpfr(const number_t *value,
                                                   size_t precision_bits)
{
    const number_private_t *impl;
    number_mpfr_t *copy;

    if (!value)
        return number_invalid();
    precision_bits = precision_bits ? precision_bits : number_default_precision_bits;
    impl = number_impl_const(value);
    if (impl->kind != NUMBER_MPFR)
        return num_as_inexact_real_prec(*value, precision_bits);

    copy = number_mpfr_from_mpfr(number_mpfr_value(impl->value.mpfr), precision_bits);
    return copy ? number_take(number_wrap_mpfr(copy)) : number_invalid();
}

int number_complex_get_mpc(mpc_t out,
                           const complex_t *value,
                           size_t precision_bits)
{
    number_t real;
    number_t imag;
    int rc;

    if (!out || !value)
        return -1;
    precision_bits = precision_bits ? precision_bits : number_default_precision_bits;
    if (value->mpc_cache_valid) {
        mpc_set(out, value->mpc_cache, MPC_RNDNN);
        return 0;
    }

    real = number_complex_component_as_mpfr(&value->real, precision_bits);
    imag = number_complex_component_as_mpfr(&value->imag, precision_bits);
    if (!num_is_inexact_real_backend(real) ||
        !num_is_inexact_real_backend(imag)) {
        num_destroy(&real);
        num_destroy(&imag);
        return -1;
    }
    rc = (number_mpfr_ensure(number_impl_const(&real)->value.mpfr, precision_bits) != 0 ||
          number_mpfr_ensure(number_impl_const(&imag)->value.mpfr, precision_bits) != 0)
        ? -1 : 0;
    if (rc == 0)
        mpc_set_fr_fr(out,
                      number_impl_const(&real)->value.mpfr->value,
                      number_impl_const(&imag)->value.mpfr->value,
                      MPC_RNDNN);
    num_destroy(&real);
    num_destroy(&imag);
    return rc;
}

number_t *number_wrap_complex_mpc(mpc_srcptr source, size_t precision_bits)
{
    number_t real;
    number_t imag;
    number_mpfr_t *real_mf;
    number_mpfr_t *imag_mf;
    number_t *boxed;
    complex_t *value;

    if (!source)
        return NULL;
    precision_bits = precision_bits ? precision_bits : (size_t)mpc_get_prec(source);
    real_mf = number_mpfr_from_mpfr(mpc_realref(source), precision_bits);
    imag_mf = number_mpfr_from_mpfr(mpc_imagref(source), precision_bits);
    if (!real_mf || !imag_mf) {
        number_mpfr_free(real_mf);
        number_mpfr_free(imag_mf);
        return NULL;
    }

    real = number_take(number_wrap_mpfr(real_mf));
    imag = number_take(number_wrap_mpfr(imag_mf));
    boxed = number_wrap_complex_parts(real, imag);
    if (!boxed) {
        num_destroy(&real);
        num_destroy(&imag);
        return NULL;
    }

    value = number_impl(boxed)->value.cx;
    value->precision_bits = precision_bits;
    (void)number_complex_set_mpc_cache_from_mpc(value, source, precision_bits);
    return boxed;
}

static bool number_text_equals_literal(const string_t *text, const char *literal)
{
    return text && string_view_equals_literal(string_view_all(text), literal);
}

static string_t *number_complex_compact_text(const string_t *text)
{
    string_cursor_t *cursor;
    string_t *out;

    if (!text)
        return NULL;

    cursor = string_cursor_new(text);
    out = string_new();
    if (!cursor || !out) {
        string_cursor_free(cursor);
        string_free(out);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        string_pos_t before = string_cursor_position(cursor);
        rune_t rune;

        string_cursor_skip_spaces(cursor);
        if (string_cursor_position(cursor) != before)
            continue;

        rune = string_cursor_peek(cursor);
        if (rune_is_none(rune) ||
            string_append_rune(out, rune) != 0 ||
            string_cursor_next(cursor) != 0) {
            string_cursor_free(cursor);
            string_free(out);
            return NULL;
        }
    }

    string_cursor_free(cursor);
    return out;
}

static bool number_text_last_ascii_pos(const string_t *text,
                                       string_pos_t *pos_out,
                                       unsigned char *ascii_out)
{
    string_cursor_t *cursor;
    string_pos_t last_pos = 0u;
    unsigned char last_ascii = 0u;
    bool found = false;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        unsigned char ascii = 0u;

        last_pos = string_cursor_position(cursor);
        (void)string_cursor_peek_ascii(cursor, &ascii);
        last_ascii = ascii;
        found = true;

        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    if (!found)
        return false;

    if (pos_out)
        *pos_out = last_pos;
    if (ascii_out)
        *ascii_out = last_ascii;
    return true;
}

static string_t *number_complex_strip_outer_parens_text(const string_t *text)
{
    string_cursor_t *cursor;
    string_t *inside;
    string_t *out;
    string_pos_t content_start;
    string_pos_t last_pos;
    unsigned char ascii = 0u;
    unsigned char last_ascii = 0u;
    char sign = '\0';

    if (!text)
        return NULL;

    cursor = string_cursor_new(text);
    if (!cursor)
        return NULL;

    if (string_cursor_peek_ascii(cursor, &ascii) &&
        (ascii == '+' || ascii == '-')) {
        sign = (char)ascii;
        if (string_cursor_next(cursor) != 0) {
            string_cursor_free(cursor);
            return string_clone(text);
        }
    }

    if (!string_cursor_peek_ascii(cursor, &ascii) || ascii != '(') {
        string_cursor_free(cursor);
        return string_clone(text);
    }

    if (string_cursor_next(cursor) != 0) {
        string_cursor_free(cursor);
        return string_clone(text);
    }
    content_start = string_cursor_position(cursor);

    if (!number_text_last_ascii_pos(text, &last_pos, &last_ascii) ||
        last_ascii != ')' ||
        last_pos < content_start) {
        string_cursor_free(cursor);
        return string_clone(text);
    }

    inside = string_cursor_slice_between(content_start, last_pos, cursor);
    string_cursor_free(cursor);
    if (!inside)
        return NULL;

    if (sign != '-')
        return inside;

    out = string_new_with("-");
    if (!out)
        string_free(inside);
    else if (string_append_string(out, inside) != 0) {
        string_free(out);
        out = NULL;
    }

    string_free(inside);
    return out;
}

static bool number_complex_split_text(const string_t *text,
                                      string_t **real_out,
                                      string_t **imag_out)
{
    string_t *compact;
    string_t *real = NULL;
    string_t *imag = NULL;
    string_t *stripped = NULL;
    string_cursor_t *cursor;
    string_pos_t start;
    string_pos_t end;
    string_pos_t i_pos = 0u;
    string_pos_t i_end = 0u;
    string_pos_t split_pos = 0u;
    unsigned char prev_ascii = 0u;
    bool prev_was_ascii = false;
    bool have_i = false;
    bool have_split = false;

    if (!text || !real_out || !imag_out)
        return false;

    *real_out = NULL;
    *imag_out = NULL;

    compact = number_complex_compact_text(text);
    if (!compact)
        return false;

    cursor = string_cursor_new(compact);
    if (!cursor) {
        string_free(compact);
        return false;
    }

    start = string_cursor_position(cursor);

    while (!string_cursor_done(cursor)) {
        string_pos_t pos = string_cursor_position(cursor);
        unsigned char ascii = 0u;
        bool is_ascii = string_cursor_peek_ascii(cursor, &ascii);

        if (is_ascii && ascii == 'i') {
            have_i = true;
            i_pos = pos;
        }
        else if (is_ascii &&
                 (ascii == '+' || ascii == '-') &&
                 pos != start &&
                 !(prev_was_ascii &&
                   (prev_ascii == 'e' || prev_ascii == 'E'))) {
            have_split = true;
            split_pos = pos;
        }

        if (string_cursor_next(cursor) != 0) {
            string_cursor_free(cursor);
            string_free(compact);
            return false;
        }

        if (is_ascii && ascii == 'i')
            i_end = string_cursor_position(cursor);

        prev_ascii = ascii;
        prev_was_ascii = is_ascii;
    }

    end = string_cursor_end_position(cursor);
    if (!have_i || i_end != end) {
        string_cursor_free(cursor);
        string_free(compact);
        return false;
    }

    if (have_split && split_pos != start) {
        real = string_cursor_slice_between(start, split_pos, cursor);
        imag = string_cursor_slice_between(split_pos, i_pos, cursor);
    }
    else {
        real = string_new_with("0");
        imag = string_cursor_slice_between(start, i_pos, cursor);
    }

    string_cursor_free(cursor);
    string_free(compact);
    if (!real || !imag) {
        string_free(real);
        string_free(imag);
        return false;
    }

    stripped = number_complex_strip_outer_parens_text(real);
    string_free(real);
    real = stripped;
    stripped = number_complex_strip_outer_parens_text(imag);
    string_free(imag);
    imag = stripped;
    if (!real || !imag) {
        string_free(real);
        string_free(imag);
        return false;
    }

    if (string_length(imag) == 0u || number_text_equals_literal(imag, "+")) {
        string_free(imag);
        imag = string_new_with("1");
    }
    else if (number_text_equals_literal(imag, "-")) {
        string_free(imag);
        imag = string_new_with("-1");
    }

    if (!imag) {
        string_free(real);
        return false;
    }

    *real_out = real;
    *imag_out = imag;
    return true;
}

static complex_t *number_complex_create_from_text(const string_t *text,
                                                  size_t precision_bits)
{
    string_t *real_text = NULL;
    string_t *imag_text = NULL;
    number_t real;
    number_t imag;
    number_t real_component;
    number_t imag_component;
    complex_t *out = NULL;

    if (!number_complex_split_text(text, &real_text, &imag_text))
        return NULL;
    real = num_create_from_text(real_text);
    imag = num_create_from_text(imag_text);
    string_free(real_text);
    string_free(imag_text);
    real_component = number_complex_component_from_number(&real, precision_bits);
    imag_component = number_complex_component_from_number(&imag, precision_bits);
    num_destroy(&real);
    num_destroy(&imag);
    out = number_complex_create(real_component, imag_component);
    if (!out) {
        num_destroy(&real_component);
        num_destroy(&imag_component);
    }
    else {
        out->precision_bits = precision_bits;
        number_complex_refresh_mpc_cache(out);
    }
    return out;
}

 int number_set_precision_noop(number_t *number, size_t precision_bits)
{
    (void)number;
    return precision_bits == 0u ? -1 : 0;
}

 size_t number_precision_fixed53(const number_t *number)
{
    (void)number;
    return 53u;
}

 size_t number_precision_fixed106(const number_t *number)
{
    (void)number;
    return 106u;
}

 size_t number_precision_zero(const number_t *number)
{
    (void)number;
    return 0u;
}

size_t number_get_effective_precision_dynamic106(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;
    size_t precision_bits = vt && vt->get_precision ? vt->get_precision(number) : 0u;

    return precision_bits == 0u ? 106u : precision_bits;
}

void number_destroy_none(number_t *number)
{
    (void)number;
}

void number_destroy_mpz(number_t *number)
{
    if (!number)
        return;
    number_mpz_free(number_impl(number)->value.mpz);
}

void number_destroy_mpq(number_t *number)
{
    if (!number)
        return;
    number_mpq_free(number_impl(number)->value.mpq);
}

void number_destroy_complex(number_t *number)
{
    if (!number || !number_impl(number)->value.cx)
        return;
    number_complex_free(number_impl(number)->value.cx);
}

void *number_scope_payload_none(const number_t *number)
{
    (void)number;
    return NULL;
}

void *number_scope_payload_mpz(const number_t *number)
{
    return number ? number_impl_const(number)->value.mpz : NULL;
}

void *number_scope_payload_mpq(const number_t *number)
{
    return number ? number_impl_const(number)->value.mpq : NULL;
}

void *number_scope_payload_complex(const number_t *number)
{
    return number ? number_impl_const(number)->value.cx : NULL;
}

void number_destroy_scope_none(void *payload)
{
    (void)payload;
}

void number_destroy_scope_mpz(void *payload)
{
    number_mpz_free((number_mpz_t *)payload);
}

void number_destroy_scope_mpq(void *payload)
{
    number_mpq_free((number_mpq_t *)payload);
}

void number_destroy_scope_complex(void *payload)
{
    number_complex_free((complex_t *)payload);
}

 number_t number_const_like_double(const number_t *like, number_const_id_t id);
 number_t number_const_like_cdouble(const number_t *like, number_const_id_t id);
 number_t number_const_like_qfloat(const number_t *like, number_const_id_t id);
 number_t number_const_like_qcomplex(const number_t *like, number_const_id_t id);
 number_t number_const_like_mexact(const number_t *like, number_const_id_t id);
 number_t number_const_like_mpfr(const number_t *like, number_const_id_t id);
 bool number_is_real_default(const number_t *number)
{
    return number != NULL;
}

 bool number_value_is_immortal_double(const number_t *number)
{
    (void)number;
    return false;
}

 bool number_value_is_immortal_qfloat(const number_t *number)
{
    (void)number;
    return false;
}

 bool number_value_is_immortal_qcomplex(const number_t *number)
{
    (void)number;
    return false;
}

static bool number_value_is_immortal(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    if (!number || !number_is_valid_value(number) || !vt || !vt->is_immortal)
        return false;
    return vt->is_immortal(number);
}

#define NUMBER_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

bool number_immortal_id_mpfr(const number_t *number,
                               number_const_id_t *id_out)
{
    const number_mpfr_t *value = number ? number_impl_const(number)->value.mpfr : NULL;

    if (!value || !id_out || value->constant_id == NUMBER_CONST_COUNT)
        return false;
    *id_out = value->constant_id;
    return true;
}

bool number_immortal_id_mpz(const number_t *number,
                             number_const_id_t *id_out)
{
    const number_mpz_t *value = number ? number_impl_const(number)->value.mpz : NULL;

    if (!value || !id_out || value->constant_id == NUMBER_CONST_COUNT)
        return false;
    *id_out = value->constant_id;
    return true;
}

bool number_immortal_id_mpq(const number_t *number,
                                  number_const_id_t *id_out)
{
    const number_mpq_t *value = number ? number_impl_const(number)->value.mpq : NULL;

    if (!value || !id_out || value->constant_id == NUMBER_CONST_COUNT)
        return false;
    *id_out = value->constant_id;
    return true;
}

bool number_const_id_from_immortal(const number_t *number,
                                   number_const_id_t *id_out)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    if (id_out)
        *id_out = NUMBER_CONST_COUNT;
    if (!number || !id_out || !vt || !vt->immortal_id)
        return false;
    return vt->immortal_id(number, id_out);
}

 bool number_value_is_immortal_mpz(const number_t *number)
{
    const number_mpz_t *value = number ? number_impl_const(number)->value.mpz : NULL;

    return value && value->immortal &&
        value->constant_id != NUMBER_CONST_COUNT;
}

 bool number_value_is_immortal_mpq(const number_t *number)
{
    const number_mpq_t *value = number ? number_impl_const(number)->value.mpq : NULL;

    return value && value->immortal &&
        value->constant_id != NUMBER_CONST_COUNT;
}

 bool number_value_is_immortal_mpfr(const number_t *number)
{
    const number_mpfr_t *value = number ? number_impl_const(number)->value.mpfr : NULL;

    return value && value->immortal &&
        value->constant_id != NUMBER_CONST_COUNT;
}

bool number_immortal_id_complex(const number_t *number,
                                number_const_id_t *id_out)
{
    const complex_t *value = number ? number_impl_const(number)->value.cx : NULL;

    if (!value || !id_out)
        return false;
    if (value == number_impl_const(&NUM_I)->value.cx) {
        *id_out = NUMBER_CONST_I;
        return true;
    }
    if (value == number_impl_const(&NUM_NEG_I)->value.cx) {
        *id_out = NUMBER_CONST_NEG_I;
        return true;
    }
    return false;
}

bool number_value_is_immortal_complex(const number_t *number)
{
    number_const_id_t id;

    return number_immortal_id_complex(number, &id);
}

const complex_t *number_complex_value(const number_t *number)
{
    return number && number_kind_value(number) == NUMBER_COMPLEX
        ? number_impl_const(number)->value.cx : NULL;
}

bool number_eq_same_tol_with_precision(const number_t *a,
                                       const number_t *b,
                                       size_t precision_bits);

bool number_is_zero_mpz(const number_t *number)
{
    return number && number_mpz_is_zero(number_impl_const(number)->value.mpz);
}

bool number_is_zero_mpq(const number_t *number)
{
    return number && number_mpq_is_zero(number_impl_const(number)->value.mpq);
}

bool number_is_one_mpz(const number_t *number)
{
    mpz_srcptr value = number && number_impl_const(number)->value.mpz
        ? number_mpz_value(number_impl_const(number)->value.mpz) : NULL;

    return value && mpz_cmp_si(value, 1L) == 0;
}

bool number_is_one_mpq(const number_t *number)
{
    return number && number_mpq_is_one(number_impl_const(number)->value.mpq);
}

bool number_eq_same_mpz(const number_t *a, const number_t *b)
{
    return a && b &&
        number_mpz_cmp(number_impl_const(a)->value.mpz,
                       number_impl_const(b)->value.mpz) == 0;
}

bool number_eq_same_mpq(const number_t *a, const number_t *b)
{
    return a && b &&
        number_mpq_cmp(number_impl_const(a)->value.mpq,
                       number_impl_const(b)->value.mpq) == 0;
}

bool number_eq_same_tol_with_precision(const number_t *a,
                                       const number_t *b,
                                       size_t precision_bits)
{
    number_t delta;
    number_t diff;
    number_t one;
    number_t tolerance;
    bool rc;

    if (!a || !b || precision_bits == 0u)
        return false;
    if (num_is_nan(*a) || num_is_nan(*b))
        return false;
    if (!num_is_finite(*a) || !num_is_finite(*b))
        return num_is_inf(*a) && num_is_inf(*b) && num_cmp(*a, *b) == 0;
    if (precision_bits > 106u)
        precision_bits = 106u;
    delta = num_sub(*a, *b);
    diff = num_abs(delta);
    one = number_create_exact_mpfr_long_prec(1, precision_bits);
    tolerance = num_ldexp(one, 4 - (int)precision_bits);
    rc = number_is_valid_value(&diff) && number_is_valid_value(&tolerance) &&
        num_cmp(diff, tolerance) <= 0;
    num_destroy(&tolerance);
    num_destroy(&one);
    num_destroy(&diff);
    num_destroy(&delta);
    return rc;
}

bool number_eq_same_tol_mpz(const number_t *a, const number_t *b)
{
    return number_eq_same_mpz(a, b);
}

bool number_eq_same_tol_mpq(const number_t *a, const number_t *b)
{
    return number_eq_same_mpq(a, b);
}

bool number_is_finite_exact(const number_t *number)
{
    return number != NULL;
}

bool number_is_nan_exact(const number_t *number)
{
    (void)number;
    return false;
}

bool number_is_inf_exact(const number_t *number)
{
    (void)number;
    return false;
}

int number_cmp_same_mpz(const number_t *a, const number_t *b)
{
    return (a && b) ? number_mpz_cmp(number_impl_const(a)->value.mpz,
                                     number_impl_const(b)->value.mpz) : 0;
}

int number_cmp_same_mpq(const number_t *a, const number_t *b)
{
    return (a && b) ? number_mpq_cmp(number_impl_const(a)->value.mpq,
                                     number_impl_const(b)->value.mpq) : 0;
}

long number_get_exponent2_zero(const number_t *number)
{
    (void)number;
    return 0l;
}

long number_get_exponent2_mpz(const number_t *number)
{
    size_t bits = number ? number_mpz_bit_length(number_impl_const(number)->value.mpz) : 0u;

    return bits ? (long)bits - 1l : 0l;
}

long number_get_exponent2_mpq(const number_t *number)
{
    mpz_t num;
    mpz_t den;
    long exp2 = 0l;

    if (!number || number_mpq_ensure(number_impl_const(number)->value.mpq) != 0)
        return 0l;
    mpz_init(num);
    mpz_init(den);
    mpz_abs(num, mpq_numref(number_impl_const(number)->value.mpq->value));
    mpz_set(den, mpq_denref(number_impl_const(number)->value.mpq->value));
    if (mpz_sgn(num) == 0 || mpz_sgn(den) == 0) {
        mpz_clear(num);
        mpz_clear(den);
        return 0l;
    }
    exp2 = (long)mpz_sizeinbase(num, 2) - (long)mpz_sizeinbase(den, 2);
    if (exp2 >= 0) {
        mpz_t scaled_den;
        mpz_init_set(scaled_den, den);
        mpz_mul_2exp(scaled_den, scaled_den, (mp_bitcnt_t)exp2);
        if (mpz_cmp(num, scaled_den) < 0)
            --exp2;
        mpz_clear(scaled_den);
    }
    else {
        long shift = -exp2;
        mpz_t scaled_num;
        mpz_init_set(scaled_num, num);
        mpz_mul_2exp(scaled_num, scaled_num, (mp_bitcnt_t)shift);
        if (mpz_cmp(scaled_num, den) < 0)
            --exp2;
        mpz_clear(scaled_num);
    }
    mpz_clear(num);
    mpz_clear(den);
    return exp2;
}

bool number_is_integer_mpz(const number_t *number)
{
    return number != NULL;
}

bool number_is_integer_mpq(const number_t *number)
{
    return number && number_mpq_is_integer(number_impl_const(number)->value.mpq);
}

size_t number_get_mantissa_bits_zero(const number_t *number)
{
    (void)number;
    return 0u;
}

bool number_get_mantissa_u64_false(const number_t *number, uint64_t *out)
{
    (void)number;
    (void)out;
    return false;
}

int number_sign_zero(const number_t *number)
{
    (void)number;
    return 0;
}

int number_sign_mpz(const number_t *number)
{
    return number && number_mpz_is_negative(number_impl_const(number)->value.mpz) ? -1 : 1;
}

int number_sign_mpq(const number_t *number)
{
    return number && number_mpq_is_negative(number_impl_const(number)->value.mpq) ? -1 : 1;
}

string_t *number_to_text_mpz(const number_t *number)
{
    return number ? number_mpz_to_text(number_impl_const(number)->value.mpz) : NULL;
}

string_t *number_to_text_mpq(const number_t *number)
{
    return number ? number_mpq_to_text(number_impl_const(number)->value.mpq) : NULL;
}

number_t *number_clone_mpz(const number_t *number)
{
    number_mpz_t *copy;

    if (!number || !number_impl_const(number)->value.mpz)
        return NULL;
    copy = number_mpz_clone(number_impl_const(number)->value.mpz);
    return copy ? number_wrap_mpz(copy) : NULL;
}

number_t *number_clone_mpq(const number_t *number)
{
    number_mpq_t *copy;

    if (!number || !number_impl_const(number)->value.mpq)
        return NULL;
    copy = number_mpq_clone(number_impl_const(number)->value.mpq);
    return copy ? number_wrap_mpq(copy) : NULL;
}

typedef void (*number_mpz_binary_core_fn)(mpz_ptr, mpz_srcptr, mpz_srcptr);
typedef void (*number_mpq_binary_core_fn)(mpq_ptr, mpq_srcptr, mpq_srcptr);

static number_t number_make_mpz_binary(const number_t *a,
                                       const number_t *b,
                                       number_mpz_binary_core_fn op)
{
    number_mpz_t *out;

    if (!a || !b || !op ||
        number_mpz_ensure(number_impl_const(a)->value.mpz) != 0 ||
        number_mpz_ensure(number_impl_const(b)->value.mpz) != 0)
        return number_invalid();
    out = number_mpz_new();
    if (!out)
        return number_invalid();
    op(out->value,
       number_impl_const(a)->value.mpz->value,
       number_impl_const(b)->value.mpz->value);
    return number_take_mpz(out);
}

static number_t number_make_mpq_binary(const number_t *a,
                                       const number_t *b,
                                       number_mpq_binary_core_fn op)
{
    number_mpq_t *out;

    if (!a || !b || !op ||
        number_mpq_ensure(number_impl_const(a)->value.mpq) != 0 ||
        number_mpq_ensure(number_impl_const(b)->value.mpq) != 0)
        return number_invalid();
    out = number_mpq_new();
    if (!out)
        return number_invalid();
    op(out->value,
       number_impl_const(a)->value.mpq->value,
       number_impl_const(b)->value.mpq->value);
    return number_take_mpq(out);
}

number_t *number_neg_mpz(const number_t *number)
{
    number_mpz_t *out;

    if (!number || !number_impl_const(number)->value.mpz ||
        number_mpz_ensure(number_impl_const(number)->value.mpz) != 0)
        return NULL;
    out = number_mpz_new();
    if (!out)
        return NULL;
    mpz_neg(out->value, number_impl_const(number)->value.mpz->value);
    return number_box_value(number_take_mpz(out));
}

number_t *number_neg_mpq(const number_t *number)
{
    number_mpq_t *out;

    if (!number || !number_impl_const(number)->value.mpq)
        return NULL;
    if (number_mpq_ensure(number_impl_const(number)->value.mpq) != 0)
        return NULL;
    out = number_mpq_new();
    if (!out)
        return NULL;
    mpq_neg(out->value, number_impl_const(number)->value.mpq->value);
    return number_box_value(number_take_mpq(out));
}

static number_t number_make_inv_mpq(const number_t *number)
{
    number_mpq_t *out;

    if (!number || !number_impl_const(number)->value.mpq ||
        number_mpq_ensure(number_impl_const(number)->value.mpq) != 0 ||
        mpq_sgn(number_impl_const(number)->value.mpq->value) == 0)
        return number_invalid();
    out = number_mpq_new();
    if (!out)
        return number_invalid();
    mpq_inv(out->value, number_impl_const(number)->value.mpq->value);
    return number_take_mpq(out);
}

number_t *number_inv_mpq(const number_t *number)
{
    number_t out = number_make_inv_mpq(number);

    return num_is_nan(out) ? NULL : number_box_value(out);
}

number_t *number_abs_mpz(const number_t *number)
{
    number_mpz_t *out;

    if (!number || !number_impl_const(number)->value.mpz ||
        number_mpz_ensure(number_impl_const(number)->value.mpz) != 0)
        return NULL;
    out = number_mpz_new();
    if (!out)
        return NULL;
    mpz_abs(out->value, number_impl_const(number)->value.mpz->value);
    return number_box_value(number_take_mpz(out));
}

number_t *number_abs_mpq(const number_t *number)
{
    number_mpq_t *out;

    if (!number || !number_impl_const(number)->value.mpq ||
        number_mpq_ensure(number_impl_const(number)->value.mpq) != 0)
        return NULL;
    out = number_mpq_new();
    if (!out)
        return NULL;
    mpq_abs(out->value, number_impl_const(number)->value.mpq->value);
    return number_box_value(number_take_mpq(out));
}

number_t *number_imag_mpz_zero(const number_t *number)
{
    (void)number;
    return number_box_value(number_take_mpz(number_mpz_from_long(0L)));
}

number_t *number_imag_mpq_zero(const number_t *number)
{
    (void)number;
    return number_box_value(number_take_mpq(number_mpq_from_frac_long(0L, 1L)));
}

number_t *number_pow_int_mpz(const number_t *number, int exponent)
{
    number_mpz_t *out;

    if (!number || exponent < 0 ||
        number_mpz_ensure(number_impl_const(number)->value.mpz) != 0)
        return NULL;
    out = number_mpz_new();
    if (!out)
        return NULL;
    mpz_pow_ui(out->value,
               number_impl_const(number)->value.mpz->value,
               (unsigned long)exponent);
    return number_box_value(number_take_mpz(out));
}

number_t *number_pow_int_mpq(const number_t *number, int exponent)
{
    number_mpq_t *copy;
    unsigned long mag;

    if (!number)
        return NULL;
    copy = number_mpq_clone(number_impl_const(number)->value.mpq);
    if (!copy || number_mpq_ensure(copy) != 0) {
        number_mpq_free(copy);
        return NULL;
    }
    if (exponent < 0) {
        if (mpq_sgn(copy->value) == 0) {
            number_mpq_free(copy);
            return NULL;
        }
        mpq_inv(copy->value, copy->value);
        mag = (unsigned long)(-(exponent + 1)) + 1ul;
    } else {
        mag = (unsigned long)exponent;
    }
    mpz_pow_ui(mpq_numref(copy->value), mpq_numref(copy->value), mag);
    mpz_pow_ui(mpq_denref(copy->value), mpq_denref(copy->value), mag);
    mpq_canonicalize(copy->value);
    return number_box_value(number_take_mpq(copy));
}

number_t *number_add_same_mpz(const number_t *a, const number_t *b)
{
    number_t out = number_make_mpz_binary(a, b, mpz_add);

    return num_is_nan(out) ? NULL : number_box_value(out);
}

number_t *number_sub_same_mpz(const number_t *a, const number_t *b)
{
    number_t out = number_make_mpz_binary(a, b, mpz_sub);

    return num_is_nan(out) ? NULL : number_box_value(out);
}

number_t *number_mul_same_mpz(const number_t *a, const number_t *b)
{
    number_t out = number_make_mpz_binary(a, b, mpz_mul);

    return num_is_nan(out) ? NULL : number_box_value(out);
}

number_t *number_add_same_mpq(const number_t *a, const number_t *b)
{
    number_t out = number_make_mpq_binary(a, b, mpq_add);

    return num_is_nan(out) ? NULL : number_box_value(out);
}

number_t *number_sub_same_mpq(const number_t *a, const number_t *b)
{
    number_t out = number_make_mpq_binary(a, b, mpq_sub);

    return num_is_nan(out) ? NULL : number_box_value(out);
}

number_t *number_mul_same_mpq(const number_t *a, const number_t *b)
{
    number_t out = number_make_mpq_binary(a, b, mpq_mul);

    return num_is_nan(out) ? NULL : number_box_value(out);
}

number_t *number_div_same_mpq(const number_t *a, const number_t *b)
{
    number_t out;

    if (!b || number_mpq_ensure(number_impl_const(b)->value.mpq) != 0 ||
        mpq_sgn(number_impl_const(b)->value.mpq->value) == 0)
        return NULL;
    out = number_make_mpq_binary(a, b, mpq_div);
    return num_is_nan(out) ? NULL : number_box_value(out);
}

void number_box_free(number_t *number)
{
    const number_vtable_t *vt;

    if (!number)
        return;
    vt = number_vt(number);
    if (vt && vt->destroy_payload)
        vt->destroy_payload(number);
    free(number);
}

number_t *number_wrap_double(double value)
{
    number_t *number = number_alloc(NUMBER_DOUBLE);

    if (number)
        number_impl(number)->value.d = value;
    return number;
}

number_t *number_wrap_cdouble(double _Complex value)
{
    number_t *number = number_alloc(NUMBER_CDOUBLE);

    if (number) {
        number_impl(number)->value.cd.value = value;
        number_impl(number)->value.cd.constant_id = NUMBER_CONST_COUNT;
        number_impl(number)->value.cd.immortal = false;
    }
    return number;
}

number_t *number_wrap_qfloat(qfloat_t value)
{
    number_t *number = number_alloc(NUMBER_QFLOAT);

    if (number)
        number_impl(number)->value.qf = value;
    return number;
}

number_t *number_wrap_qcomplex(qcomplex_t value)
{
    number_t *number = number_alloc(NUMBER_QCOMPLEX);

    if (number)
        number_impl(number)->value.qc = value;
    return number;
}

number_t *number_wrap_mpz(number_mpz_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_MPZ);
    if (!number) {
        number_mpz_free(value);
        return NULL;
    }
    number_impl(number)->value.mpz = value;
    return number;
}

number_t *number_wrap_mpq(number_mpq_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_MPQ);
    if (!number) {
        number_mpq_free(value);
        return NULL;
    }
    number_impl(number)->value.mpq = value;
    return number;
}

number_t *number_wrap_mpfr(number_mpfr_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_MPFR);
    if (!number) {
        number_mpfr_free(value);
        return NULL;
    }
    number_impl(number)->value.mpfr = value;
    return number;
}

number_t *number_wrap_complex(complex_t *value)
{
    number_t *number;

    if (!value)
        return NULL;
    number = number_alloc(NUMBER_COMPLEX);
    if (!number) {
        number_complex_free(value);
        return NULL;
    }
    number_impl(number)->value.cx = value;
    return number;
}

typedef number_t *(*number_binary_dispatch_fn)(const number_vtable_t *vt,
                                               const number_t *a,
                                               const number_t *b);

static number_t *number_apply_add_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->add_same ? vt->add_same(a, b) : NULL;
}

static number_t *number_apply_sub_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->sub_same ? vt->sub_same(a, b) : NULL;
}

static number_t *number_apply_mul_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->mul_same ? vt->mul_same(a, b) : NULL;
}

static number_t *number_apply_div_same_kind(const number_vtable_t *vt,
                                            const number_t *a,
                                            const number_t *b)
{
    return vt && vt->div_same ? vt->div_same(a, b) : NULL;
}

static const number_binary_dispatch_fn number_binary_dispatch[] = {
    [NUMBER_OP_ADD] = number_apply_add_same_kind,
    [NUMBER_OP_SUB] = number_apply_sub_same_kind,
    [NUMBER_OP_MUL] = number_apply_mul_same_kind,
    [NUMBER_OP_DIV] = number_apply_div_same_kind
};

static number_t *number_apply_binary_same_kind(const number_t *a,
                                               const number_t *b,
                                               number_binary_op_t op)
{
    const number_vtable_t *vt;
    number_binary_dispatch_fn fn;

    if (!a || !b || (size_t)op >= (sizeof(number_binary_dispatch) / sizeof(number_binary_dispatch[0])))
        return NULL;
    vt = number_vt(a);
    fn = number_binary_dispatch[op];
    return fn ? fn(vt, a, b) : NULL;
}

static number_t *number_apply_binary_generic(const number_t *a,
                                             const number_t *b,
                                             number_binary_op_t op)
{
    number_kind_t kind;
    size_t precision_bits = 0u;
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    number_t *result = NULL;

    if (!a || !b)
        return NULL;
    kind = number_common_kind(a, b, op);
    if (number_kind_value(a) == kind && number_kind_value(b) == kind &&
        num_get_prec_bits(*a) == num_get_prec_bits(*b))
        return number_apply_binary_same_kind(a, b, op);
    lhs = number_coerce(a, kind);
    rhs = number_coerce(b, kind);
    if (kind == NUMBER_COMPLEX) {
        size_t a_bits = num_get_prec_bits(*a);
        size_t b_bits = num_get_prec_bits(*b);

        precision_bits = a_bits > b_bits ? a_bits : b_bits;
        if (precision_bits != 0u) {
            if (lhs)
                num_set_prec_bits(lhs, precision_bits);
            if (rhs)
                num_set_prec_bits(rhs, precision_bits);
        }
    }
    else if (kind == NUMBER_MPFR) {
        size_t a_bits = num_get_prec_bits(*a);
        size_t b_bits = num_get_prec_bits(*b);

        precision_bits = a_bits > b_bits ? a_bits : b_bits;
        if (precision_bits == 0u)
            precision_bits = number_default_precision_bits;
        if (lhs)
            num_set_prec_bits(lhs, precision_bits);
        if (rhs)
            num_set_prec_bits(rhs, precision_bits);
    }
    if (!lhs || !rhs || !number_same_kind_value(lhs, rhs))
        goto done;
    result = number_apply_binary_same_kind(lhs, rhs, op);

done:
    number_box_free(lhs);
    number_box_free(rhs);
    return result;
}

number_t num_create_from_double(double value)
{
    return number_make_double(value);
}

number_t num_create_from_cdouble(double _Complex value)
{
    return number_make_cdouble(value);
}

number_t num_create_from_qfloat(qfloat_t value)
{
    return number_make_qfloat(value);
}

number_t num_create_from_qcomplex(qcomplex_t value)
{
    return number_make_qcomplex(value);
}

number_t num_create_from_text(const string_t *text)
{
    string_view_t view;
    string_t *trimmed;
    number_t constant;
    number_t out = number_invalid();

    if (!text)
        return number_invalid();

    view = string_view_trim(string_view_all(text));
    if (string_view_length(view) == 0u)
        return number_invalid();

    trimmed = string_from_view(&view);
    if (!trimmed)
        return number_invalid();

    if (num_constant_value_text(trimmed, &constant))
        out = constant;
    else if (number_text_has_ascii_ci(trimmed, 'i'))
        out = number_take(number_wrap_complex(
            number_complex_create_from_text(trimmed, number_default_precision_bits)));
    else if (number_text_has_ascii(trimmed, '/') ||
             number_text_has_unicode_fraction(trimmed))
        out = number_take(number_wrap_mpq(
            number_mpq_from_text(trimmed)));
    else if (number_text_is_decimal(trimmed))
        out = number_take(number_wrap_mpfr(
            number_mpfr_from_text(trimmed, number_default_precision_bits)));
    else
        out = number_take(number_wrap_mpz(
            number_mpz_from_text(trimmed)));

    string_free(trimmed);
    return out;
}

number_t num_create_from_string(const char *text)
{
    string_t *owned = text ? string_new_with(text) : NULL;
    number_t out;

    if (!owned)
        return number_invalid();

    out = num_create_from_text(owned);
    string_free(owned);
    return out;
}

number_t num_create_from_frac(long numerator, long denominator)
{
    return number_take(number_wrap_mpq(
        number_mpq_from_frac_long(numerator, denominator)));
}

number_t *number_const_prec_double(const number_t *number, size_t precision_bits)
{
    number_mpfr_t *mpfr_value;

    if (!number)
        return NULL;
    mpfr_value = number_mpfr_from_double(number_impl_const(number)->value.d,
                                         precision_bits);
    return mpfr_value ? number_wrap_mpfr(mpfr_value) : NULL;
}

number_t *number_const_prec_qfloat(const number_t *number, size_t precision_bits)
{
    number_mpfr_t *out;

    if (!number)
        return NULL;
    out = number_mpfr_from_qfloat(number_impl_const(number)->value.qf,
                                  precision_bits);
    return out ? number_wrap_mpfr(out) : NULL;
}

number_t *number_const_prec_qcomplex(const number_t *number, size_t precision_bits)
{
    return number ? number_wrap_complex(number_complex_create_from_qcomplex(
        number_impl_const(number)->value.qc, precision_bits)) : NULL;
}

number_t *number_const_prec_mpz(const number_t *number, size_t precision_bits)
{
    (void)precision_bits;
    return number_clone_mpz(number);
}

number_t *number_const_prec_mpq(const number_t *number, size_t precision_bits)
{
    (void)precision_bits;
    return number_clone_mpq(number);
}

number_t *number_const_prec_complex(const number_t *number, size_t precision_bits)
{
    const complex_t *value = number_complex_value(number);
    number_t real;
    number_t imag;
    complex_t *out;

    if (!value)
        return NULL;
    real = num_const_prec(value->real, precision_bits);
    imag = num_const_prec(value->imag, precision_bits);
    out = number_complex_create(real, imag);
    if (!out) {
        num_destroy(&real);
        num_destroy(&imag);
        return NULL;
    }
    out->constant_id = value->constant_id;
    out->precision_bits = precision_bits;
    if (value->mpc_cache_valid)
        (void)number_complex_set_mpc_cache_from_mpc(out, value->mpc_cache,
            precision_bits);
    return number_wrap_complex(out);
}

number_t num_const_prec(number_t constant, size_t precision_bits)
{
    const number_vtable_t *vt;
    number_t *materialized;
    size_t bits = precision_bits != 0u ? precision_bits : number_default_precision_bits;

    if (!number_is_valid_value(&constant))
        return number_invalid();

    vt = number_vt(&constant);
    materialized = vt && vt->const_prec ? vt->const_prec(&constant, bits) : NULL;
    return materialized ? number_take(materialized) : number_invalid();
}

number_t num_const_prec_digits(number_t constant, size_t significant_digits)
{
    size_t precision_bits;

    precision_bits = significant_digits == 0u
        ? number_default_precision_bits
        : (size_t)ceil((double)significant_digits * 3.3219280948873626);
    return num_const_prec(constant, precision_bits);
}

number_t num_const(number_t constant)
{
    return num_const_prec(constant, number_default_precision_bits);
}

number_t num_clone(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (number_value_is_immortal(&number))
        return number;
    return vt ? number_take(vt->clone(&number)) : number_invalid();
}

bool num_is_immortal(number_t number)
{
    return number_value_is_immortal(&number);
}

void num_destroy(number_t *number)
{
    num_destroy_slow(number);
}

void num_destroy_slow(number_t *number)
{
    const number_vtable_t *vt;
    number_kind_t kind;

    if (!number)
        return;
    kind = number_impl_const(number)->kind;
    if (kind == NUMBER_INVALID || kind == NUMBER_DOUBLE ||
        kind == NUMBER_CDOUBLE || kind == NUMBER_QFLOAT ||
        kind == NUMBER_QCOMPLEX) {
        return;
    }
    number_scope_unregister_value(number);
    if (number_value_is_immortal(number)) {
        memset(number, 0, sizeof(*number));
        number_impl(number)->kind = NUMBER_INVALID;
        return;
    }
    vt = number_vt(number);
    if (vt && vt->destroy_payload)
        vt->destroy_payload(number);
    memset(number, 0, sizeof(*number));
    number_impl(number)->kind = NUMBER_INVALID;
}

num_scope_t *num_scope_enter(void)
{
    num_scope_t *scope = (num_scope_t *)calloc(1, sizeof(*scope));

    if (!scope)
        return NULL;
    scope->state = NULL;
    scope->previous = number_scope_current;
    scope->active = 1;
    number_scope_current = scope;
    return scope;
}

void num_scope_leave(num_scope_t **scope_ptr)
{
    num_scope_t *scope;
    number_scope_state_t *state;
    number_scope_arena_block_t *arena_block;
    number_scope_block_t *block;

    if (!scope_ptr)
        return;
    scope = *scope_ptr;
    if (!scope || !scope->active || number_scope_current != scope)
        return;

    state = number_scope_state_get(scope);
    block = state ? state->records : NULL;
    while (block) {
        number_scope_block_t *next = block->next;
        size_t i = block->used;

        while (i-- > 0) {
            number_scope_record_t *record = &block->records[i];

            if (record->payload != NULL)
                number_scope_destroy_record(record);
        }
        if (state && state->records == block)
            state->records = next;
        free(block);
        block = next;
    }
    arena_block = state ? state->arena_blocks : NULL;
    while (arena_block) {
        number_scope_arena_block_t *next = arena_block->next;

        free(arena_block);
        arena_block = next;
    }
    free(state);

    number_scope_current = scope->previous;
    scope->state = NULL;
    scope->previous = NULL;
    scope->active = 0;
    free(scope);
    *scope_ptr = NULL;
}

bool num_scope_is_active(void)
{
    return number_scope_current != NULL && number_scope_suspend_depth == 0u;
}

number_t num_scope_detach(number_t value)
{
    if (number_value_is_immortal(&value))
        return value;
    number_scope_unregister_value(&value);
    return value;
}

bool num_is_exact(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->exact;
}

bool num_is_real(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->is_real && vt->is_real(&number);
}

bool num_is_zero(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->is_zero && vt->is_zero(&number);
}

bool num_is_one(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->is_one && vt->is_one(&number);
}

short num_get_sign(const number_t number)
{
    return (short)num_sign(number);
}

long num_get_exponent2(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number) || num_is_zero(number) ||
        !num_is_finite(number) || !num_is_real(number))
        return 0l;
    return vt && vt->get_exponent2 ? vt->get_exponent2(&number) : 0l;
}

int num_set_prec_bits(number_t *number, size_t precision_bits)
{
    const number_vtable_t *vt = number_vt(number);

    return vt && vt->set_precision ? vt->set_precision(number, precision_bits) : -1;
}

size_t num_get_prec_bits(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->get_precision ? vt->get_precision(&number) : 0u;
}

size_t num_get_effective_prec_bits(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->get_effective_precision
        ? vt->get_effective_precision(&number) : 0u;
}

bool num_is_inexact_real_backend(number_t number)
{
    return number_kind_value(&number) == NUMBER_MPFR;
}

bool num_is_complex_backend(number_t number)
{
    return number_kind_value(&number) == NUMBER_COMPLEX;
}

bool num_get_small_rational(number_t number, long *numerator, long *denominator)
{
    const number_private_t *impl = number_impl_const(&number);
    long n;
    long d;

    if (!numerator || !denominator || !num_is_real(number))
        return false;

    if (impl->kind == NUMBER_MPZ) {
        if (!number_mpz_get_long(impl->value.mpz, &n))
            return false;
        *numerator = n;
        *denominator = 1L;
        return true;
    }

    if (impl->kind != NUMBER_MPQ)
        return false;

    if (!number_mpq_get_small_fraction(impl->value.mpq, &n, &d) || d == 0L)
        return false;
    if (d < 0L) {
        n = -n;
        d = -d;
    }
    *numerator = n;
    *denominator = d;
    return true;
}

number_t num_as_inexact_real_prec(number_t number, size_t precision_bits)
{
    number_t *boxed = number_coerce(&number, NUMBER_MPFR);

    if (!boxed)
        return number_invalid();
    if (precision_bits > 0u)
        num_set_prec_bits(boxed, precision_bits);
    return number_take(boxed);
}

number_t num_as_complex_prec(number_t number, size_t precision_bits)
{
    number_t *boxed = number_coerce(&number, NUMBER_COMPLEX);

    if (!boxed)
        return number_invalid();
    if (precision_bits > 0u)
        num_set_prec_bits(boxed, precision_bits);
    return number_take(boxed);
}

char *number_to_cstring(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return vt && vt->to_text ? number_cstring_from_text(vt->to_text(number)) : NULL;
}

string_t *num_to_string(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return vt && vt->to_text ? vt->to_text(&number) : NULL;
}

bool num_eq(const number_t a, const number_t b)
{
    number_kind_t kind;
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    bool eq = false;

    if (!number_is_valid_value(&a) || !number_is_valid_value(&b))
        return false;
    if (number_same_kind_value(&a, &b)) {
        const number_vtable_t *vt = number_vt(&a);
        if (vt && vt->eq_same)
            return vt->eq_same(&a, &b);
    }

    kind = number_common_kind(&a, &b, NUMBER_OP_ADD);
    lhs = number_coerce(&a, kind);
    rhs = number_coerce(&b, kind);
    if (!lhs || !rhs || !number_same_kind_value(lhs, rhs))
        goto done;
    const number_vtable_t *vt = number_vt(lhs);
    if (!vt || !vt->eq_same)
        goto done;
    eq = vt->eq_same(lhs, rhs);

done:
    number_box_free(lhs);
    number_box_free(rhs);
    return eq;
}

number_t num_neg(const number_t number)
{
    const number_vtable_t *vt;
    number_const_id_t id;
    number_kind_t kind = number_impl_const(&number)->kind;

    if (kind == NUMBER_DOUBLE)
        return number_make_double(-number_impl_const(&number)->value.d);
    if (kind == NUMBER_QFLOAT)
        return number_make_qfloat(qf_neg(number_impl_const(&number)->value.qf));
    if (kind == NUMBER_QCOMPLEX)
        return number_make_qcomplex(qc_neg(number_impl_const(&number)->value.qc));
    if (kind == NUMBER_MPZ) {
        number_mpz_t *out;

        if (number_mpz_ensure(number_impl_const(&number)->value.mpz) != 0)
            return number_invalid();
        out = number_mpz_new();
        if (!out)
            return number_invalid();
        mpz_neg(out->value, number_impl_const(&number)->value.mpz->value);
        return number_take_mpz(out);
    }
    if (kind == NUMBER_MPQ) {
        number_mpq_t *out;

        if (number_mpq_ensure(number_impl_const(&number)->value.mpq) != 0)
            return number_invalid();
        out = number_mpq_new();
        if (!out)
            return number_invalid();
        mpq_neg(out->value, number_impl_const(&number)->value.mpq->value);
        return number_take_mpq(out);
    }
    if (number_const_id_from_immortal(&number, &id)) {
        if (id == NUMBER_CONST_PI_2)
            return NUM_NEG_PI_2;
        if (id == NUMBER_CONST_NEG_PI_2)
            return NUM_PI_2;
        if (id == NUMBER_CONST_INF)
            return NUM_NINF;
        if (id == NUMBER_CONST_NINF)
            return NUM_INF;
    }
    vt = number_vt(&number);
    return vt && vt->neg ? number_take(vt->neg(&number)) : number_invalid();
}

number_t num_inv(const number_t number)
{
    const number_vtable_t *vt;
    number_kind_t kind = number_impl_const(&number)->kind;

    if (kind == NUMBER_DOUBLE)
        return number_make_double(1.0 / number_impl_const(&number)->value.d);
    if (kind == NUMBER_QFLOAT)
        return number_make_qfloat(qf_div(QF_ONE, number_impl_const(&number)->value.qf));
    if (kind == NUMBER_QCOMPLEX)
        return number_make_qcomplex(qc_div(qc_make(QF_ONE, QF_ZERO),
            number_impl_const(&number)->value.qc));
    if (kind == NUMBER_MPQ)
        return number_make_inv_mpq(&number);
    vt = number_vt(&number);
    if (!vt)
        return number_invalid();
    if (vt->inv)
        return number_take(vt->inv(&number));
    if (kind == NUMBER_MPZ) {
        number_mpq_t *value;
        mpq_t tmp;

        if (number_mpz_ensure(number_impl_const(&number)->value.mpz) != 0 ||
            mpz_sgn(number_impl_const(&number)->value.mpz->value) == 0)
            return number_invalid();
        mpq_init(tmp);
        mpz_set_ui(mpq_numref(tmp), 1u);
        mpz_set(mpq_denref(tmp), number_impl_const(&number)->value.mpz->value);
        mpq_canonicalize(tmp);
        value = number_mpq_from_mpq(tmp);
        mpq_clear(tmp);
        return value ? number_take_mpq(value) : number_invalid();
    }
    return number_invalid();
}

static bool number_try_exact_immortal_binary(const number_t *a,
                                             const number_t *b,
                                             number_binary_op_t op,
                                             number_t *out)
{
    typedef enum {
        NUMBER_EXACT_RESULT_CONST,
        NUMBER_EXACT_RESULT_NEG_CONST
    } number_exact_result_kind_t;
    typedef struct {
        number_binary_op_t op;
        number_const_id_t left;
        number_const_id_t right;
        number_const_id_t result;
        number_exact_result_kind_t result_kind;
        bool commutative;
    } number_exact_binary_rule_t;
    static const number_exact_binary_rule_t rules[] = {
        { NUMBER_OP_MUL, NUMBER_CONST_PI,       NUMBER_CONST_TWO,  NUMBER_CONST_2PI,  NUMBER_EXACT_RESULT_CONST,     true  },
        { NUMBER_OP_MUL, NUMBER_CONST_PI,       NUMBER_CONST_HALF, NUMBER_CONST_PI_2, NUMBER_EXACT_RESULT_CONST,     true  },
        { NUMBER_OP_MUL, NUMBER_CONST_PI_2,     NUMBER_CONST_TWO,  NUMBER_CONST_PI,   NUMBER_EXACT_RESULT_CONST,     true  },
        { NUMBER_OP_MUL, NUMBER_CONST_NEG_PI_2, NUMBER_CONST_TWO,  NUMBER_CONST_PI,   NUMBER_EXACT_RESULT_NEG_CONST, true  },
        { NUMBER_OP_DIV, NUMBER_CONST_PI,       NUMBER_CONST_TWO,  NUMBER_CONST_PI_2, NUMBER_EXACT_RESULT_CONST,     false },
        { NUMBER_OP_DIV, NUMBER_CONST_2PI,      NUMBER_CONST_TWO,  NUMBER_CONST_PI,   NUMBER_EXACT_RESULT_CONST,     false },
        { NUMBER_OP_DIV, NUMBER_CONST_PI,       NUMBER_CONST_PI,   NUMBER_CONST_ONE,  NUMBER_EXACT_RESULT_CONST,     false }
    };
    number_const_id_t aid;
    number_const_id_t bid;
    size_t i;

    if (!a || !b || !out ||
        !number_const_id_from_immortal(a, &aid) ||
        !number_const_id_from_immortal(b, &bid))
        return false;

    for (i = 0u; i < NUMBER_ARRAY_LEN(rules); ++i) {
        bool direct = rules[i].left == aid && rules[i].right == bid;
        bool swapped = rules[i].commutative &&
                       rules[i].left == bid && rules[i].right == aid;

        if (rules[i].op != op || (!direct && !swapped))
            continue;

        *out = rules[i].result_kind == NUMBER_EXACT_RESULT_NEG_CONST
            ? number_neg_const_return_like(a, rules[i].result)
            : number_const_return_like(a, rules[i].result);
        return true;
    }

    return false;
}

number_t num_add(const number_t a, const number_t b)
{
    return num_add_slow(a, b);
}

typedef int (*number_mpfr_binary_core_fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr,
                                          mpfr_rnd_t);

static number_t number_apply_binary_mpfr_value(const number_t *a,
                                               const number_t *b,
                                               number_mpfr_binary_core_fn op)
{
    number_mpfr_t *copy;

    if (!a || !b || !op)
        return number_invalid();
    copy = number_mpfr_clone(number_impl_const(a)->value.mpfr);
    if (!copy || number_mpfr_ensure(number_impl_const(b)->value.mpfr,
                                    (size_t)mpfr_get_prec(copy->value)) != 0) {
        number_mpfr_free(copy);
        return number_invalid();
    }
    op(copy->value, copy->value, number_impl_const(b)->value.mpfr->value, MPFR_RNDN);
    return number_take_mpfr(copy);
}

number_t num_add_slow(const number_t a, const number_t b)
{
    number_kind_t kind = number_impl_const(&a)->kind;

    if (kind == number_impl_const(&b)->kind) {
        if (kind == NUMBER_DOUBLE)
            return number_make_double(number_impl_const(&a)->value.d +
                number_impl_const(&b)->value.d);
        if (kind == NUMBER_CDOUBLE)
            return number_make_cdouble(number_impl_const(&a)->value.cd.value +
                number_impl_const(&b)->value.cd.value);
        if (kind == NUMBER_QFLOAT)
            return number_make_qfloat(qf_add(number_impl_const(&a)->value.qf,
                number_impl_const(&b)->value.qf));
        if (kind == NUMBER_QCOMPLEX)
            return number_make_qcomplex(qc_add(number_impl_const(&a)->value.qc,
                number_impl_const(&b)->value.qc));
        if (kind == NUMBER_MPFR)
            return number_apply_binary_mpfr_value(&a, &b, mpfr_add);
        if (kind == NUMBER_MPZ)
            return number_make_mpz_binary(&a, &b, mpz_add);
        if (kind == NUMBER_MPQ)
            return number_make_mpq_binary(&a, &b, mpq_add);
    }
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_ADD));
}

number_t num_sub(const number_t a, const number_t b)
{
    return num_sub_slow(a, b);
}

number_t num_sub_slow(const number_t a, const number_t b)
{
    number_kind_t kind = number_impl_const(&a)->kind;

    if (kind == number_impl_const(&b)->kind) {
        if (kind == NUMBER_DOUBLE)
            return number_make_double(number_impl_const(&a)->value.d -
                number_impl_const(&b)->value.d);
        if (kind == NUMBER_CDOUBLE)
            return number_make_cdouble(number_impl_const(&a)->value.cd.value -
                number_impl_const(&b)->value.cd.value);
        if (kind == NUMBER_QFLOAT)
            return number_make_qfloat(qf_sub(number_impl_const(&a)->value.qf,
                number_impl_const(&b)->value.qf));
        if (kind == NUMBER_QCOMPLEX)
            return number_make_qcomplex(qc_sub(number_impl_const(&a)->value.qc,
                number_impl_const(&b)->value.qc));
        if (kind == NUMBER_MPFR)
            return number_apply_binary_mpfr_value(&a, &b, mpfr_sub);
        if (kind == NUMBER_MPZ)
            return number_make_mpz_binary(&a, &b, mpz_sub);
        if (kind == NUMBER_MPQ)
            return number_make_mpq_binary(&a, &b, mpq_sub);
    }
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_SUB));
}

number_t num_mul(const number_t a, const number_t b)
{
    return num_mul_slow(a, b);
}

number_t num_mul_slow(const number_t a, const number_t b)
{
    number_kind_t kind = number_impl_const(&a)->kind;
    number_t out;

    if (kind == number_impl_const(&b)->kind) {
        if (kind == NUMBER_DOUBLE)
            return number_make_double(number_impl_const(&a)->value.d *
                number_impl_const(&b)->value.d);
        if (kind == NUMBER_CDOUBLE)
            return number_make_cdouble(number_impl_const(&a)->value.cd.value *
                number_impl_const(&b)->value.cd.value);
        if (kind == NUMBER_QFLOAT)
            return number_make_qfloat(qf_mul(number_impl_const(&a)->value.qf,
                number_impl_const(&b)->value.qf));
        if (kind == NUMBER_QCOMPLEX)
            return number_make_qcomplex(qc_mul(number_impl_const(&a)->value.qc,
                number_impl_const(&b)->value.qc));
        if (kind == NUMBER_MPFR)
            return number_apply_binary_mpfr_value(&a, &b, mpfr_mul);
        if (kind == NUMBER_MPZ)
            return number_make_mpz_binary(&a, &b, mpz_mul);
        if (kind == NUMBER_MPQ)
            return number_make_mpq_binary(&a, &b, mpq_mul);
    }
    if (number_try_exact_immortal_binary(&a, &b, NUMBER_OP_MUL, &out))
        return out;
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_MUL));
}

number_t num_div(const number_t a, const number_t b)
{
    return num_div_slow(a, b);
}

number_t num_div_slow(const number_t a, const number_t b)
{
    number_kind_t kind = number_impl_const(&a)->kind;
    number_t out;

    if (kind == number_impl_const(&b)->kind) {
        if (kind == NUMBER_DOUBLE)
            return number_make_double(number_impl_const(&a)->value.d /
                number_impl_const(&b)->value.d);
        if (kind == NUMBER_CDOUBLE)
            return number_make_cdouble(number_impl_const(&a)->value.cd.value /
                number_impl_const(&b)->value.cd.value);
        if (kind == NUMBER_QFLOAT)
            return number_make_qfloat(qf_div(number_impl_const(&a)->value.qf,
                number_impl_const(&b)->value.qf));
        if (kind == NUMBER_QCOMPLEX)
            return number_make_qcomplex(qc_div(number_impl_const(&a)->value.qc,
                number_impl_const(&b)->value.qc));
        if (kind == NUMBER_MPFR)
            return number_apply_binary_mpfr_value(&a, &b, mpfr_div);
        if (kind == NUMBER_MPQ) {
            if (number_mpq_ensure(number_impl_const(&b)->value.mpq) != 0 ||
                mpq_sgn(number_impl_const(&b)->value.mpq->value) == 0)
                return number_invalid();
            return number_make_mpq_binary(&a, &b, mpq_div);
        }
    }
    if (number_try_exact_immortal_binary(&a, &b, NUMBER_OP_DIV, &out))
        return out;
    return number_take(number_apply_binary_generic(&a, &b, NUMBER_OP_DIV));
}

static size_t number_bits_to_digits(size_t precision_bits)
{
    if (precision_bits == 0u)
        return 0u;
    return (size_t)floor((double)precision_bits * 0.3010299956639812);
}

static size_t number_digits_to_bits(size_t significant_digits)
{
    if (significant_digits == 0u)
        return 0u;
    return (size_t)ceil((double)significant_digits * 3.3219280948873623);
}

void number_assign(number_t *dst, number_t value)
{
    if (!dst) {
        num_destroy(&value);
        return;
    }
    num_destroy(dst);
    *dst = value;
}

static inline bool number_is_finite_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && number_is_valid_value(number) &&
        vt && vt->is_finite && vt->is_finite(number);
}

static inline bool number_is_nan_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return !number || !number_is_valid_value(number) ||
        (vt && vt->is_nan && vt->is_nan(number));
}

static inline bool number_is_inf_value(const number_t *number)
{
    const number_vtable_t *vt = number ? number_vt(number) : NULL;

    return number && number_is_valid_value(number) &&
        vt && vt->is_inf && vt->is_inf(number);
}

static inline int number_cmp_same_kind(const number_t *a, const number_t *b)
{
    const number_vtable_t *vt = a ? number_vt(a) : NULL;

    return a && b && vt && vt->cmp_same ? vt->cmp_same(a, b) : 0;
}

static inline bool number_eq_same_kind(const number_t *a, const number_t *b)
{
    const number_vtable_t *vt = a ? number_vt(a) : NULL;

    return a && b && vt && vt->eq_same ? vt->eq_same(a, b) : false;
}

bool number_matches_value(const number_t *reference, const number_t *target)
{
    const number_vtable_t *reference_vt;
    const number_vtable_t *target_vt;
    number_t *coerced_target;
    size_t precision_bits;
    bool matches;

    if (!reference || !target || !number_is_valid_value(reference) ||
        !number_is_valid_value(target))
        return false;
    reference_vt = number_vt(reference);
    if (!reference_vt)
        return false;
    coerced_target = number_coerce(target, number_impl_const(reference)->kind);
    if (!coerced_target)
        return false;
    target_vt = number_vt(coerced_target);
    precision_bits = reference_vt->get_precision
        ? reference_vt->get_precision(reference)
        : 0u;
    if (precision_bits != 0u && target_vt && target_vt->set_precision)
        target_vt->set_precision(coerced_target, precision_bits);
    matches = reference_vt->eq_same_tol
        ? reference_vt->eq_same_tol(reference, coerced_target)
        : number_eq_same_kind(reference, coerced_target);
    number_box_free(coerced_target);
    return matches;
}

static number_t number_const_mpfr_special(number_const_id_t id, size_t precision_bits)
{
    number_mpfr_t *mpfr;

    if (id == NUMBER_CONST_NEG_ONE)
        return number_create_exact_mpfr_long_prec(-1, precision_bits);
    if (number_const_has_ldexp(id))
        return number_create_exact_mpfr_dyadic_prec(
            1, number_const_ldexp_value(id), precision_bits);
    mpfr = number_mpfr_from_const_id(id, precision_bits);
    return mpfr ? number_take(number_wrap_mpfr(mpfr)) : number_invalid();
}

static number_t number_make_complex_from_real(number_t real, size_t precision_bits)
{
    number_t imag;
    complex_t *complex_value;

    if (!number_is_valid_value(&real))
        return number_invalid();
    imag = num_create_from_long(0);
    complex_value = number_complex_create(real, imag);
    if (!complex_value) {
        num_destroy(&real);
        num_destroy(&imag);
        return number_invalid();
    }
    complex_value->precision_bits = precision_bits;
    return number_take(number_wrap_complex(complex_value));
}

static number_t number_make_complex_imag_unit(number_const_id_t id,
                                              size_t precision_bits)
{
    number_t real;
    number_t imag;
    complex_t *out;

    if (id != NUMBER_CONST_I && id != NUMBER_CONST_NEG_I)
        return number_invalid();
    real = num_create_from_long(0);
    imag = num_create_from_long(id == NUMBER_CONST_I ? 1 : -1);
    out = number_complex_create(real, imag);
    if (!out) {
        num_destroy(&real);
        num_destroy(&imag);
        return number_invalid();
    }
    out->constant_id = id;
    out->precision_bits = precision_bits;
    return number_take(number_wrap_complex(out));
}

 number_t number_const_like_double(const number_t *like, number_const_id_t id)
{
    (void)like;
    if (number_const_has_double(id))
        return num_create_from_double(number_const_double_value(id));
    return number_invalid();
}

 number_t number_const_like_qfloat(const number_t *like, number_const_id_t id)
{
    (void)like;
    return num_create_from_qfloat(number_const_qfloat(id));
}

 number_t number_const_like_qcomplex(const number_t *like, number_const_id_t id)
{
    (void)like;
    return num_create_from_qcomplex(number_const_qcomplex(id));
}

 number_t number_const_like_mexact(const number_t *like, number_const_id_t id)
{
    size_t precision_bits;
    number_t exact;

    (void)like;
    exact = number_const_mpfr_exact(id);
    if (number_is_valid_value(&exact))
        return exact;
    precision_bits = number_default_precision_bits;
    if (id == NUMBER_CONST_I || id == NUMBER_CONST_NEG_I)
        return number_make_complex_imag_unit(id, precision_bits);
    return number_const_mpfr_special(id, precision_bits);
}

 number_t number_const_like_mpfr(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt = number_vt(like);
    size_t precision_bits = vt && vt->get_precision ? vt->get_precision(like) : 0u;
    number_t out;

    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;
    if (id == NUMBER_CONST_I || id == NUMBER_CONST_NEG_I)
        return number_make_complex_imag_unit(id, precision_bits);
    out = number_const_mpfr_special(id, precision_bits);
    if (number_is_valid_value(&out))
        return out;
    return number_invalid();
}

number_t number_const_like_complex(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt = number_vt(like);
    size_t precision_bits = vt && vt->get_precision ? vt->get_precision(like) : 0u;
    number_t real;
    number_t out;

    if (precision_bits == 0u)
        precision_bits = number_default_precision_bits;
    if (id == NUMBER_CONST_I || id == NUMBER_CONST_NEG_I)
        return number_make_complex_imag_unit(id, precision_bits);
    real = number_const_mpfr_special(id, precision_bits);
    if (!number_is_valid_value(&real))
        return number_invalid();
    out = number_make_complex_from_real(real, precision_bits);
    return out;
}

number_t number_const_like(const number_t *like, number_const_id_t id)
{
    const number_vtable_t *vt;

    if (!like || !number_is_valid_value(like) || (unsigned)id >= NUMBER_CONST_COUNT)
        return number_invalid();
    vt = number_vt(like);
    return vt && vt->const_like ? vt->const_like(like, id) : number_invalid();
}

number_t num_new(void)
{
    return number_take(number_wrap_mpfr(
        number_mpfr_new_prec(number_default_precision_bits)));
}

number_t num_new_with_prec_bits(size_t precision_bits)
{
    return precision_bits == 0u ? number_invalid() :
        number_take(number_wrap_mpfr(number_mpfr_new_prec(precision_bits)));
}

number_t num_create_from_long(long value)
{
    return number_take(number_wrap_mpz(number_mpz_from_long(value)));
}

int num_set_default_prec_bits(size_t precision_bits)
{
    if (precision_bits == 0u)
        return -1;
    number_default_precision_bits = precision_bits;
    return 0;
}

size_t num_get_default_prec_bits(void)
{
    return number_default_precision_bits;
}

int num_set_default_prec_digits(size_t significant_digits)
{
    size_t bits = number_digits_to_bits(significant_digits);
    return bits == 0u ? -1 : num_set_default_prec_bits(bits);
}

size_t num_get_default_prec_digits(void)
{
    return number_bits_to_digits(number_default_precision_bits);
}

int num_set_prec_digits(number_t *number, size_t significant_digits)
{
    size_t bits = number_digits_to_bits(significant_digits);
    return bits == 0u ? -1 : num_set_prec_bits(number, bits);
}

size_t num_get_prec_digits(const number_t number)
{
    return number_bits_to_digits(num_get_prec_bits(number));
}

int num_set_long(number_t *number, long value)
{
    if (!number)
        return -1;
    number_assign(number, num_create_from_long(value));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_frac(number_t *number, long numerator, long denominator)
{
    if (!number)
        return -1;
    number_assign(number, num_create_from_frac(numerator, denominator));
    return number_is_valid_value(number) ? 0 : -1;
}

static number_t number_create_mpfr_from_double_prec(double value,
                                                    size_t precision_bits)
{
    number_mpfr_t *mpfr_value = number_mpfr_from_double(
        value,
        precision_bits ? precision_bits : number_default_precision_bits);

    return mpfr_value ? number_take(number_wrap_mpfr(mpfr_value))
                      : number_invalid();
}

static number_t number_create_mpfr_from_qfloat_prec(qfloat_t value,
                                                    size_t precision_bits)
{
    number_mpfr_t *mpfr_value = number_mpfr_from_qfloat(
        value,
        precision_bits ? precision_bits : number_default_precision_bits);

    return mpfr_value ? number_take(number_wrap_mpfr(mpfr_value))
                      : number_invalid();
}

static number_t number_create_complex_from_double_prec(double value,
                                                       size_t precision_bits)
{
    number_t real;
    number_t imag;
    number_t *boxed;

    precision_bits = precision_bits ? precision_bits : number_default_precision_bits;
    real = number_create_mpfr_from_double_prec(value, precision_bits);
    imag = number_create_exact_mpfr_long_prec(0, precision_bits);
    if (!number_is_valid_value(&real) || !number_is_valid_value(&imag)) {
        num_destroy(&real);
        num_destroy(&imag);
        return number_invalid();
    }
    boxed = number_wrap_complex_parts(real, imag);
    return boxed ? number_take(boxed) : number_invalid();
}

static number_t number_create_complex_from_cdouble_prec(double _Complex value,
                                                        size_t precision_bits)
{
    number_t real;
    number_t imag;
    number_t *boxed;

    precision_bits = precision_bits ? precision_bits : number_default_precision_bits;
    real = number_create_mpfr_from_double_prec(creal(value), precision_bits);
    imag = number_create_mpfr_from_double_prec(cimag(value), precision_bits);
    if (!number_is_valid_value(&real) || !number_is_valid_value(&imag)) {
        num_destroy(&real);
        num_destroy(&imag);
        return number_invalid();
    }
    boxed = number_wrap_complex_parts(real, imag);
    return boxed ? number_take(boxed) : number_invalid();
}

static number_t number_create_complex_from_qfloat_prec(qfloat_t value,
                                                       size_t precision_bits)
{
    number_t real;
    number_t imag;
    number_t *boxed;

    precision_bits = precision_bits ? precision_bits : number_default_precision_bits;
    real = number_create_mpfr_from_qfloat_prec(value, precision_bits);
    imag = number_create_exact_mpfr_long_prec(0, precision_bits);
    if (!number_is_valid_value(&real) || !number_is_valid_value(&imag)) {
        num_destroy(&real);
        num_destroy(&imag);
        return number_invalid();
    }
    boxed = number_wrap_complex_parts(real, imag);
    return boxed ? number_take(boxed) : number_invalid();
}

static number_t number_create_complex_from_qcomplex_prec(qcomplex_t value,
                                                         size_t precision_bits)
{
    complex_t *complex_value = number_complex_create_from_qcomplex(
        value,
        precision_bits ? precision_bits : number_default_precision_bits);

    return complex_value ? number_take(number_wrap_complex(complex_value))
                         : number_invalid();
}

int num_set_double(number_t *number, double value)
{
    number_kind_t kind;
    size_t precision_bits;
    number_t replacement;

    if (!number)
        return -1;

    kind = number_kind_value(number);
    precision_bits = num_get_prec_bits(*number);
    if (kind == NUMBER_MPFR) {
        replacement = number_create_mpfr_from_double_prec(value, precision_bits);
    } else if (kind == NUMBER_COMPLEX) {
        replacement = number_create_complex_from_double_prec(value, precision_bits);
    } else if (kind == NUMBER_CDOUBLE) {
        replacement = num_create_from_cdouble(value);
    } else if (kind == NUMBER_QFLOAT) {
        replacement = num_create_from_qfloat(qf_from_double(value));
    } else if (kind == NUMBER_QCOMPLEX) {
        replacement = num_create_from_qcomplex(
            qc_make(qf_from_double(value), QF_ZERO));
    } else {
        replacement = num_create_from_double(value);
    }

    number_assign(number, replacement);
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_cdouble(number_t *number, double _Complex value)
{
    number_kind_t kind;
    size_t precision_bits;
    number_t replacement;

    if (!number)
        return -1;

    kind = number_kind_value(number);
    precision_bits = num_get_prec_bits(*number);
    if (kind == NUMBER_COMPLEX) {
        replacement = number_create_complex_from_cdouble_prec(value,
                                                              precision_bits);
    } else if (kind == NUMBER_QCOMPLEX) {
        replacement = num_create_from_qcomplex(qc_make(
            qf_from_double(creal(value)),
            qf_from_double(cimag(value))));
    } else if (kind == NUMBER_QFLOAT) {
        replacement = cimag(value) == 0.0
            ? num_create_from_qfloat(qf_from_double(creal(value)))
            : num_create_from_qcomplex(qc_make(
                qf_from_double(creal(value)),
                qf_from_double(cimag(value))));
    } else if (kind == NUMBER_MPFR && cimag(value) == 0.0) {
        replacement = number_create_mpfr_from_double_prec(creal(value),
                                                          precision_bits);
    } else if (kind == NUMBER_MPFR) {
        replacement = number_create_complex_from_cdouble_prec(value,
                                                              precision_bits);
    } else {
        replacement = num_create_from_cdouble(value);
    }

    number_assign(number, replacement);
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_qfloat(number_t *number, qfloat_t value)
{
    number_kind_t kind;
    size_t precision_bits;
    number_t replacement;

    if (!number)
        return -1;

    kind = number_kind_value(number);
    precision_bits = num_get_prec_bits(*number);
    if (kind == NUMBER_MPFR) {
        replacement = number_create_mpfr_from_qfloat_prec(value, precision_bits);
    } else if (kind == NUMBER_COMPLEX) {
        replacement = number_create_complex_from_qfloat_prec(value, precision_bits);
    } else if (kind == NUMBER_CDOUBLE) {
        replacement = num_create_from_cdouble(qf_to_double(value));
    } else if (kind == NUMBER_DOUBLE) {
        replacement = num_create_from_double(qf_to_double(value));
    } else if (kind == NUMBER_QCOMPLEX) {
        replacement = num_create_from_qcomplex(qc_make(value, QF_ZERO));
    } else {
        replacement = num_create_from_qfloat(value);
    }

    number_assign(number, replacement);
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_qcomplex(number_t *number, qcomplex_t value)
{
    number_kind_t kind;
    size_t precision_bits;
    number_t replacement;

    if (!number)
        return -1;

    kind = number_kind_value(number);
    precision_bits = num_get_prec_bits(*number);
    if (kind == NUMBER_COMPLEX) {
        replacement = number_create_complex_from_qcomplex_prec(value,
                                                               precision_bits);
    } else if (kind == NUMBER_CDOUBLE) {
        replacement = num_create_from_cdouble(
            qf_to_double(qc_real(value)) + qf_to_double(qc_imag(value)) * I);
    } else {
        replacement = num_create_from_qcomplex(value);
    }

    number_assign(number, replacement);
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_from_string(number_t *number, const char *text)
{
    if (!number || !text)
        return -1;
    number_assign(number, num_create_from_string(text));
    return number_is_valid_value(number) ? 0 : -1;
}

int num_set_from_text(number_t *number, const string_t *text)
{
    if (!number || !text)
        return -1;
    number_assign(number, num_create_from_text(text));
    return number_is_valid_value(number) ? 0 : -1;
}

double num_to_double(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t *tmp = NULL;

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return NAN;
    if (vt && vt->to_double)
        return vt->to_double(&number);
    tmp = number_coerce(&number, NUMBER_MPFR);
    if (!tmp)
        return NAN;
    vt = number_vt(tmp);
    double value = (vt && vt->to_double) ? vt->to_double(tmp) : NAN;
    number_box_free(tmp);
    return value;
}

qfloat_t num_to_qfloat(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);
    number_t *tmp = NULL;

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return QF_NAN;
    if (vt && vt->to_qfloat)
        return vt->to_qfloat(&number);
    tmp = number_coerce(&number, NUMBER_MPFR);
    if (!tmp)
        return QF_NAN;
    vt = number_vt(tmp);
    qfloat_t value = (vt && vt->to_qfloat) ? vt->to_qfloat(tmp) : QF_NAN;
    number_box_free(tmp);
    return value;
}

bool num_is_integer(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number) || !num_is_real(number))
        return false;
    return vt && vt->is_integer ? vt->is_integer(&number) : false;
}

bool num_is_finite(const number_t number)
{
    return number_is_finite_value(&number);
}

bool num_is_nan(const number_t number)
{
    return number_is_nan_value(&number);
}

bool num_is_inf(const number_t number)
{
    return number_is_inf_value(&number);
}

size_t num_get_mantissa_bits(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    return number_is_valid_value(&number) && vt && vt->get_mantissa_bits
        ? vt->get_mantissa_bits(&number) : 0u;
}

bool num_get_mantissa_u64(const number_t number, uint64_t *out)
{
    const number_vtable_t *vt = number_vt(&number);

    return out && number_is_valid_value(&number) && vt && vt->get_mantissa_u64
        ? vt->get_mantissa_u64(&number, out) : false;
}

int num_sign(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (num_is_zero(number))
        return 0;
    return vt && vt->sign ? vt->sign(&number) : 0;
}

bool num_lt(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) < 0;
}

bool num_le(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) <= 0;
}

bool num_gt(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) > 0;
}

bool num_ge(const number_t a, const number_t b)
{
    if (!num_is_real(a) || !num_is_real(b))
        return false;
    return num_cmp(a, b) >= 0;
}

int num_cmp(const number_t a, const number_t b)
{
    number_math_family_t family;
    number_kind_t kind;
    number_t *lhs = NULL;
    number_t *rhs = NULL;
    int rc = 0;

    if (!number_is_valid_value(&a) || !number_is_valid_value(&b) ||
        !num_is_real(a) || !num_is_real(b))
        return 0;
    if (number_same_kind_value(&a, &b))
        return number_cmp_same_kind(&a, &b);
    family = number_math_family_binary(number_math_family_value(&a),
        number_math_family_value(&b));
    kind = number_math_family_target_kind(family);
    if (kind == NUMBER_INVALID || family == NUMBER_MATH_MPFR)
        kind = number_common_kind(&a, &b, NUMBER_OP_ADD);
    lhs = number_coerce(&a, kind);
    rhs = number_coerce(&b, kind);
    if (lhs && rhs)
        rc = number_cmp_same_kind(lhs, rhs);
    number_box_free(lhs);
    number_box_free(rhs);
    return rc;
}

number_t num_abs(const number_t number)
{
    const number_vtable_t *vt;
    number_kind_t kind = number_impl_const(&number)->kind;

    if (kind == NUMBER_DOUBLE)
        return number_make_double(fabs(number_impl_const(&number)->value.d));
    if (kind == NUMBER_QFLOAT)
        return number_make_qfloat(qf_abs(number_impl_const(&number)->value.qf));
    if (kind == NUMBER_QCOMPLEX)
        return number_make_qfloat(qc_abs(number_impl_const(&number)->value.qc));
    if (kind == NUMBER_INVALID)
        return number_invalid();
    vt = number_vt(&number);
    return vt && vt->abs_value ? number_take(vt->abs_value(&number)) : number_invalid();
}

number_t num_conj(const number_t number)
{
    const number_vtable_t *vt;
    number_kind_t kind = number_impl_const(&number)->kind;

    if (kind == NUMBER_DOUBLE)
        return number_make_double(number_impl_const(&number)->value.d);
    if (kind == NUMBER_QFLOAT)
        return number_make_qfloat(number_impl_const(&number)->value.qf);
    if (kind == NUMBER_QCOMPLEX)
        return number_make_qcomplex(qc_conj(number_impl_const(&number)->value.qc));
    if (kind == NUMBER_INVALID)
        return number_invalid();
    vt = number_vt(&number);
    return vt && vt->conj_value ? number_take(vt->conj_value(&number)) : number_invalid();
}

number_t num_real_part(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    return vt && vt->real_part ? number_take(vt->real_part(&number)) : number_invalid();
}

number_t num_imag_part(const number_t number)
{
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    return vt && vt->imag_part ? number_take(vt->imag_part(&number)) : number_invalid();
}

number_t num_arg(const number_t number)
{
    NUM_SCOPE(scope);
    const number_vtable_t *vt = number_vt(&number);

    if (!number_is_valid_value(&number))
        return number_invalid();
    if (vt && vt->arg_value)
        return number_take(vt->arg_value(&number));
    number_t zero = number_create_exact_mpfr_long_prec(
        0, num_get_prec_bits(number) ? num_get_prec_bits(number) : number_default_precision_bits);
    number_t real = vt && vt->is_complex ? num_real_part(number) : num_clone(number);
    number_t result = num_atan2(zero, real);
    return num_scope_detach(result);
}

number_t num_add_long(const number_t number, long value)
{
    NUM_SCOPE(scope);
    number_t rhs = num_create_from_long(value);
    number_t result = num_add(number, rhs);
    return num_scope_detach(result);
}

number_t num_mul_long(const number_t number, long value)
{
    NUM_SCOPE(scope);
    number_t rhs = num_create_from_long(value);
    number_t result = num_mul(number, rhs);
    return num_scope_detach(result);
}
