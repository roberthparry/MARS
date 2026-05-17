#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "mint_internal.h"

static char *mint_trimmed_copy(const char *text, int base, int allow_hex_prefix)
{
    const unsigned char *start;
    const unsigned char *end;
    const unsigned char *p;
    char *out;
    char *q;
    size_t len;

    if (!text)
        return NULL;

    start = (const unsigned char *)text;
    while (*start && isspace(*start))
        start++;

    end = start + strlen((const char *)start);
    while (end > start && isspace(end[-1]))
        end--;

    if (end == start) {
        out = malloc(2u);
        if (!out)
            return NULL;
        out[0] = '0';
        out[1] = '\0';
        return out;
    }

    len = (size_t)(end - start);
    out = malloc(len + 3u);
    if (!out)
        return NULL;

    p = start;
    q = out;

    if (*p == '+' || *p == '-') {
        if (*p == '-')
            *q++ = (char)*p;
        p++;
    }

    if (allow_hex_prefix && p + 1 < end && p[0] == '0' &&
        (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }

    if (p == end) {
        free(out);
        return NULL;
    }

    while (p < end) {
        unsigned char ch = *p++;

        if (isspace(ch)) {
            free(out);
            return NULL;
        }
        if (base == 10 && !isdigit(ch) && !(q == out + 1 && (out[0] == '+' || out[0] == '-'))) {
            free(out);
            return NULL;
        }
        if (base == 16 && !isxdigit(ch)) {
            free(out);
            return NULL;
        }
        *q++ = (char)ch;
    }

    if (q == out || (q == out + 1 && (out[0] == '+' || out[0] == '-')))
        *q++ = '0';

    *q = '\0';
    return out;
}

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
    char *trimmed;
    int rc;

    if (!mint || !text || mint->constant_id != MICONST_NONE)
        return -1;

    trimmed = mint_trimmed_copy(text, 10, 0);
    if (!trimmed) {
        mpz_set_ui(mint->value, 0u);
        return -1;
    }

    rc = mpz_set_str(mint->value, trimmed, 10);
    free(trimmed);
    if (rc != 0)
        mpz_set_ui(mint->value, 0u);
    return rc == 0 ? 0 : -1;
}

int mi_set_hex(mint_t *mint, const char *text)
{
    char *trimmed;
    int rc;

    if (!mint || !text || mint->constant_id != MICONST_NONE)
        return -1;

    trimmed = mint_trimmed_copy(text, 16, 1);
    if (!trimmed) {
        mpz_set_ui(mint->value, 0u);
        return -1;
    }

    rc = mpz_set_str(mint->value, trimmed, 16);
    free(trimmed);
    if (rc != 0)
        mpz_set_ui(mint->value, 0u);
    return rc == 0 ? 0 : -1;
}

char *mi_to_string(const mint_t *mint)
{
    if (!mint)
        return NULL;
    if (mint->constant_id != MICONST_NONE)
        mint_constant_ensure(mint);
    return mpz_get_str(NULL, 10, mint->value);
}

char *mi_to_hex(const mint_t *mint)
{
    if (!mint)
        return NULL;
    if (mint->constant_id != MICONST_NONE)
        mint_constant_ensure(mint);
    return mpz_get_str(NULL, 16, mint->value);
}
