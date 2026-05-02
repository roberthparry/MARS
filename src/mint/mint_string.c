#include "mint_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
mint_t *mi_create_string(const char *text)
{
    mint_t *mint = mi_new();

    if (!mint)
        return NULL;

    if (mi_set_string(mint, text) != 0) {
        mi_free(mint);
        return NULL;
    }

    return mint;
}

mint_t *mi_create_hex(const char *text)
{
    mint_t *mint = mi_new();

    if (!mint)
        return NULL;

    if (mi_set_hex(mint, text) != 0) {
        mi_free(mint);
        return NULL;
    }

    return mint;
}
int mi_set_string(mint_t *mint, const char *text)
{
    const unsigned char *p;
    short sign = 1;
    int saw_digit = 0;

    if (!mint || !text || mint_is_immortal(mint))
        return -1;

    mi_clear(mint);
    p = (const unsigned char *)text;

    while (*p && isspace(*p))
        p++;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        p++;
    }

    while (*p == '0')
        p++;

    for (; *p && isdigit(*p); ++p) {
        if (mint_mul_small(mint, 10) != 0)
            goto fail;
        if (mint_add_small(mint, (uint32_t)(*p - '0')) != 0)
            goto fail;
        saw_digit = 1;
    }

    while (*p && isspace(*p))
        p++;

    if (*p != '\0')
        goto fail;

    if (!saw_digit) {
        mi_clear(mint);
        return 0;
    }

    mint->sign = mint->length == 0 ? 0 : sign;
    return 0;

fail:
    mi_clear(mint);
    return -1;
}

int mi_set_hex(mint_t *mint, const char *text)
{
    const unsigned char *p;
    short sign = 1;
    int saw_digit = 0;

    if (!mint || !text || mint_is_immortal(mint))
        return -1;

    mi_clear(mint);
    p = (const unsigned char *)text;

    while (*p && isspace(*p))
        p++;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            sign = -1;
        p++;
    }

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;

    while (*p) {
        int digit;

        if (isspace(*p))
            break;

        digit = mint_hex_digit_value(*p);
        if (digit < 0)
            goto fail;

        if (mint_shl_inplace(mint, 4) != 0)
            goto fail;
        if (digit != 0 && mint_add_small(mint, (uint32_t)digit) != 0)
            goto fail;
        saw_digit = 1;
        p++;
    }

    while (*p && isspace(*p))
        p++;

    if (*p != '\0')
        goto fail;

    if (!saw_digit) {
        mi_clear(mint);
        return 0;
    }

    mint->sign = mint->length == 0 ? 0 : sign;
    return 0;

fail:
    mi_clear(mint);
    return -1;
}

char *mi_to_string(const mint_t *mint)
{
    mint_t *tmp;
    char *buf;
    size_t buf_len, pos;

    if (!mint)
        return NULL;
    if (mint->sign == 0) {
        buf = malloc(2);
        if (!buf)
            return NULL;
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    tmp = mi_new();
    if (!tmp)
        return NULL;
    if (mint_copy_value(tmp, mint) != 0) {
        mi_free(tmp);
        return NULL;
    }
    tmp->sign = 1;

    buf_len = mint->length * 20 + 2;
    buf = malloc(buf_len);
    if (!buf) {
        mi_free(tmp);
        return NULL;
    }

    pos = buf_len;
    buf[--pos] = '\0';

    while (tmp->sign != 0)
        buf[--pos] = (char)('0' + mint_div_small_inplace(tmp, 10));

    if (mint->sign < 0)
        buf[--pos] = '-';

    memmove(buf, buf + pos, buf_len - pos);
    mi_free(tmp);
    return buf;
}

char *mi_to_hex(const mint_t *mint)
{
    static const char digits[] = "0123456789abcdef";
    size_t bitlen, digit_count, first_bits, i, pos = 0;
    char *out;

    if (!mint)
        return NULL;
    if (mint->sign == 0) {
        out = malloc(2);
        if (!out)
            return NULL;
        out[0] = '0';
        out[1] = '\0';
        return out;
    }

    bitlen = mint_bit_length_internal(mint);
    digit_count = (bitlen + 3) / 4;
    out = malloc((mint->sign < 0 ? 1u : 0u) + digit_count + 1u);
    if (!out)
        return NULL;

    if (mint->sign < 0)
        out[pos++] = '-';

    first_bits = bitlen % 4;
    if (first_bits == 0)
        first_bits = 4;

    for (i = bitlen; i > 0;) {
        size_t chunk = (i == bitlen) ? first_bits : 4;
        unsigned value = 0;
        size_t j;

        i -= chunk;
        for (j = 0; j < chunk; ++j) {
            value <<= 1;
            value |= (unsigned)mint_get_bit(mint, i + (chunk - 1 - j));
        }
        out[pos++] = digits[value];
    }

    out[pos] = '\0';
    return out;
}
