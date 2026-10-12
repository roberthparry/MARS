/**
 * @file quantity_units.c
 * @brief Bounded unit parsing, rational dimensions and conversion dispatch.
 *
 * Converts exact scale ratios before realising any remaining power of pi.
 * Compound units use explicit multiplication, division and rational powers;
 * affine temperature spellings are deliberately excluded from compounds.
 */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define MARS_QUANTITY_INTERNAL_ACCESS
#include "quantity_internal.h"

void *quantity_fail(int error)
{
    errno = error;
    return NULL;
}

bool quantity_error(int error)
{
    errno = error;
    return false;
}

void quantity_unit_init(quantity_unit_t *unit)
{
    *unit = (quantity_unit_t){.numerator = num_clone(NUM_ONE), .denominator = num_clone(NUM_ONE),
                              .offset = num_clone(NUM_ZERO)};
    for (unsigned i = 0; i < QUANTITY_POWERS; ++i)
        unit->powers[i].denominator = 1;
}

void quantity_unit_clear(quantity_unit_t *unit)
{
    num_destroy(&unit->numerator);
    num_destroy(&unit->denominator);
    num_destroy(&unit->offset);
    string_free(unit->name);
    *unit = (quantity_unit_t){0};
}

bool quantity_unit_copy(quantity_unit_t *target, const quantity_unit_t *source)
{
    *target = *source;
    target->numerator = num_clone(source->numerator);
    target->denominator = num_clone(source->denominator);
    target->offset = num_clone(source->offset);
    target->name = source->name ? string_clone(source->name) : string_new_with("1");
    return target->name != NULL;
}

bool quantity_power_set(quantity_power_t *power, int64_t numerator, int64_t denominator)
{
    if (denominator <= 0)
        return quantity_error(EINVAL);
    int64_t a = numerator < 0 ? -numerator : numerator, b = denominator;
    while (b) {
        int64_t remainder = a % b;
        a = b;
        b = remainder;
    }
    numerator /= a;
    denominator /= a;
    if (numerator < -QUANTITY_LIMIT || numerator > QUANTITY_LIMIT || denominator > QUANTITY_LIMIT)
        return quantity_error(EOVERFLOW);
    *power = (quantity_power_t){(int32_t)numerator, (int32_t)denominator};
    return true;
}

bool quantity_power_add(quantity_power_t *out, quantity_power_t left, quantity_power_t right, int sign)
{
    return quantity_power_set(out, (int64_t)left.numerator * right.denominator +
                                      sign * (int64_t)right.numerator * left.denominator,
                              (int64_t)left.denominator * right.denominator);
}

bool quantity_dimensions_equal(const quantity_unit_t *left, const quantity_unit_t *right)
{
    /* Exactly eight fixed physical axes, not a unit-name search. */
    for (unsigned i = 0; i < QUANTITY_AXES; ++i) {
        quantity_power_t a = left->powers[i], b = right->powers[i];
        if ((int64_t)a.numerator * b.denominator != (int64_t)b.numerator * a.denominator)
            return false;
    }
    return true;
}

bool quantity_unit_is_temperature(const quantity_unit_t *unit)
{
    for (unsigned i = 0; i < QUANTITY_AXES; ++i) {
        quantity_power_t power = unit->powers[i];
        if (power.numerator != (i == 4 ? power.denominator : 0))
            return false;
    }
    return true;
}

bool quantity_unit_coherent(quantity_unit_t *unit)
{
    static const char *const axes[QUANTITY_AXES] = {"m", "kg", "s", "A", "K", "mol", "cd", "rad"};
    string_t *name = string_new();
    if (!name)
        return false;
    for (unsigned i = 0; i < QUANTITY_AXES; ++i) {
        quantity_power_t power = unit->powers[i];
        if (!power.numerator)
            continue;
        if ((string_byte_length(name) && string_append_cstr(name, "*") < 0) ||
            string_append_cstr(name, axes[i]) < 0)
            goto failed;
        if (power.denominator != 1) {
            if (string_append_format(name, "^(%d/%d)", power.numerator, power.denominator) < 0)
                goto failed;
        } else if (power.numerator != 1 && string_append_format(name, "^%d", power.numerator) < 0)
            goto failed;
    }
    if (!string_byte_length(name) && string_append_cstr(name, "1") < 0)
        goto failed;
    string_free(unit->name);
    unit->name = name;
    num_destroy(&unit->numerator);
    num_destroy(&unit->denominator);
    num_destroy(&unit->offset);
    unit->numerator = num_clone(NUM_ONE);
    unit->denominator = num_clone(NUM_ONE);
    unit->offset = num_clone(NUM_ZERO);
    unit->powers[QUANTITY_PI] = (quantity_power_t){0, 1};
    /* A coherent single kelvin also admits point conversion. */
    unit->point_allowed = !strcmp(string_c_str(name), "K");
    return true;
failed:
    string_free(name);
    return false;
}

void quantity_unit_scale_product(quantity_unit_t *out, const quantity_unit_t *left,
                                 const quantity_unit_t *right, bool divide)
{
    number_t numerator = num_mul(left->numerator, divide ? right->denominator : right->numerator);
    number_t denominator = num_mul(left->denominator, divide ? right->numerator : right->denominator);
    num_destroy(&out->numerator);
    num_destroy(&out->denominator);
    out->numerator = numerator;
    out->denominator = denominator;
}

static number_t quantity_linear(number_t value, const quantity_unit_t *from, const quantity_unit_t *to)
{
    quantity_power_t power;
    if (!quantity_power_add(&power, from->powers[QUANTITY_PI], to->powers[QUANTITY_PI], -1))
        return (number_t){0};
    number_t numerator = num_mul(from->numerator, to->denominator);
    number_t denominator = num_mul(from->denominator, to->numerator);
    number_t scaled = num_mul(value, numerator);
    number_t result = num_div(scaled, denominator);
    num_destroy(&numerator);
    num_destroy(&scaled);
    num_destroy(&denominator);
    if (power.numerator) {
        number_t pi = num_const(NUM_PI);
        number_t exponent = num_create_from_frac(power.numerator, power.denominator);
        number_t factor = num_pow(pi, exponent);
        number_t product = num_mul(result, factor);
        num_destroy(&pi);
        num_destroy(&exponent);
        num_destroy(&factor);
        num_destroy(&result);
        result = product;
    }
    return result;
}

static number_t quantity_affine(number_t value, const quantity_unit_t *from, const quantity_unit_t *to)
{
    number_t linear = quantity_linear(value, from, to);
    number_t difference = num_sub(from->offset, to->offset);
    number_t scaled = num_mul(difference, to->denominator);
    number_t shift = num_div(scaled, to->numerator);
    number_t result = num_add(linear, shift);
    num_destroy(&linear);
    num_destroy(&difference);
    num_destroy(&scaled);
    num_destroy(&shift);
    return result;
}

number_t quantity_convert_value(number_t value, const quantity_unit_t *from, const quantity_unit_t *to, bool point)
{
    static number_t (*const conversions[])(number_t, const quantity_unit_t *, const quantity_unit_t *) = {
        [false] = quantity_linear,
        [true]  = quantity_affine,
    };
    return conversions[point](value, from, to);
}

typedef struct {
    const char *at;
    const char *end;
} quantity_parser_t;

static void quantity_skip_space(quantity_parser_t *parser)
{
    while (parser->at < parser->end && isspace((unsigned char)*parser->at))
        ++parser->at;
}

static bool quantity_integer(quantity_parser_t *parser, int32_t *out)
{
    quantity_skip_space(parser);
    int sign = 1, value = 0;
    if (parser->at < parser->end && (*parser->at == '-' || *parser->at == '+'))
        sign = *parser->at++ == '-' ? -1 : 1;
    const char *start = parser->at;
    while (parser->at < parser->end && isdigit((unsigned char)*parser->at)) {
        value = value * 10 + (*parser->at++ - '0');
        if (value > QUANTITY_LIMIT)
            return quantity_error(EOVERFLOW);
    }
    *out = sign * value;
    return parser->at != start;
}

static bool quantity_unit_expression(quantity_parser_t *parser, quantity_unit_t *unit, unsigned depth);

static bool quantity_unit_factor(quantity_parser_t *parser, quantity_unit_t *unit, unsigned depth)
{
    quantity_skip_space(parser);
    if (depth > 16)
        return quantity_error(EOVERFLOW);
    if (parser->at == parser->end)
        return false;
    if (*parser->at == '(') {
        ++parser->at;
        if (!quantity_unit_expression(parser, unit, depth + 1))
            return false;
        quantity_skip_space(parser);
        if (parser->at == parser->end || *parser->at++ != ')')
            return false;
        string_t *group = string_sprintf("(%S)", unit->name);
        if (!group)
            return false;
        string_free(unit->name);
        unit->name = group;
    } else {
        const char *start = parser->at;
        while (parser->at < parser->end && !strchr("*/^()", *parser->at))
            ++parser->at;
        const char *end = parser->at;
        while (end > start && isspace((unsigned char)end[-1]))
            --end;
        if (!quantity_unit_lookup(unit, start, (size_t)(end - start)))
            return false;
    }
    /* Point spellings are allowed only as a whole unit, never in a compound. */
    if (!num_is_zero(unit->offset))
        return quantity_error(EDOM);
    quantity_skip_space(parser);
    if (parser->at < parser->end && *parser->at == '^') {
        ++parser->at;
        quantity_skip_space(parser);
        bool fraction = parser->at < parser->end && *parser->at == '(';
        if (fraction)
            ++parser->at;
        int32_t numerator, denominator = 1;
        if (!quantity_integer(parser, &numerator))
            return false;
        quantity_skip_space(parser);
        if (fraction) {
            if (parser->at == parser->end || *parser->at++ != '/' ||
                !quantity_integer(parser, &denominator) || denominator <= 0)
                return false;
            quantity_skip_space(parser);
            if (parser->at == parser->end || *parser->at++ != ')')
                return false;
        }
        for (unsigned i = 0; i < QUANTITY_POWERS; ++i) {
            quantity_power_t *power = &unit->powers[i];
            if (!quantity_power_set(power, (int64_t)power->numerator * numerator,
                                    (int64_t)power->denominator * denominator))
                return false;
        }
        number_t exponent = num_create_from_frac(abs(numerator), denominator);
        number_t n = numerator < 0 ? unit->denominator : unit->numerator;
        number_t d = numerator < 0 ? unit->numerator : unit->denominator;
        number_t power_n = num_is_one(n) ? num_clone(NUM_ONE) : num_pow(n, exponent);
        number_t power_d = num_is_one(d) ? num_clone(NUM_ONE) : num_pow(d, exponent);
        num_destroy(&exponent);
        num_destroy(&unit->numerator);
        num_destroy(&unit->denominator);
        unit->numerator = power_n;
        unit->denominator = power_d;
        if ((fraction && string_append_format(unit->name, "^(%d/%d)", numerator, denominator) < 0) ||
            (!fraction && string_append_format(unit->name, "^%d", numerator) < 0))
            return false;
    }
    unit->point_allowed = false;
    return true;
}

static bool quantity_unit_expression(quantity_parser_t *parser, quantity_unit_t *unit, unsigned depth)
{
    if (!quantity_unit_factor(parser, unit, depth))
        return false;
    quantity_skip_space(parser);
    while (parser->at < parser->end && (*parser->at == '*' || *parser->at == '/')) {
        bool divide = *parser->at++ == '/';
        quantity_unit_t right = {0};
        bool ok = quantity_unit_factor(parser, &right, depth);
        for (unsigned i = 0; ok && i < QUANTITY_POWERS; ++i)
            ok = quantity_power_add(&unit->powers[i], unit->powers[i], right.powers[i], divide ? -1 : 1);
        if (ok) {
            quantity_unit_scale_product(unit, unit, &right, divide);
            ok = string_append_format(unit->name, "%s%S", divide ? "/" : "*", right.name) >= 0;
        }
        quantity_unit_clear(&right);
        if (!ok)
            return false;
        quantity_skip_space(parser);
    }
    return true;
}

bool quantity_unit_parse(quantity_unit_t *unit, const string_t *text)
{
    *unit = (quantity_unit_t){0};
    if (!text || !string_is_valid_utf8(text))
        return quantity_error(EINVAL);
    size_t length = string_byte_length(text);
    if (length > 1024)
        return quantity_error(EOVERFLOW);
    const char *start = string_c_str(text), *end = start + length;
    if (memchr(start, '\0', length))
        return quantity_error(EINVAL);
    while (start < end && isspace((unsigned char)*start))
        ++start;
    while (end > start && isspace((unsigned char)end[-1]))
        --end;
    errno = 0;
    if (quantity_unit_lookup(unit, start, (size_t)(end - start)))
        return true;
    quantity_unit_clear(unit);
    quantity_parser_t parser = {start, end};
    bool ok = quantity_unit_expression(&parser, unit, 0);
    quantity_skip_space(&parser);
    if (!ok || parser.at != end || !num_is_finite(unit->numerator) || !num_is_finite(unit->denominator)) {
        int error = errno ? errno : EINVAL;
        quantity_unit_clear(unit);
        return quantity_error(error);
    }
    return true;
}
