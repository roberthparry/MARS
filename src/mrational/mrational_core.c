#include "mrational_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *mr_dup_trimmed(const char *text)
{
    const unsigned char *start;
    const unsigned char *end;
    size_t len;
    char *copy;

    if (!text)
        return NULL;

    start = (const unsigned char *)text;
    while (*start && isspace(*start))
        start++;

    end = start + strlen((const char *)start);
    while (end > start && isspace(end[-1]))
        end--;

    len = (size_t)(end - start);
    copy = calloc(len + 1u, 1u);
    if (!copy)
        return NULL;
    if (len > 0u)
        memcpy(copy, start, len);
    return copy;
}

static int mr_set_frac_mints(mrational_t *rational, const mint_t *numerator,
                               const mint_t *denominator)
{
    if (!rational || !rational->numerator || !rational->denominator ||
        !numerator || !denominator || mi_is_zero(denominator))
        return -1;
    if (mi_clear(rational->numerator), mi_add(rational->numerator, numerator) != 0)
        return -1;
    if (mi_clear(rational->denominator), mi_add(rational->denominator, denominator) != 0)
        return -1;
    return mr_normalise(rational);
}

int mr_normalise(mrational_t *rational)
{
    mint_t *gcd = NULL;
    mint_t *num_abs = NULL;

    if (!rational || !rational->numerator || !rational->denominator)
        return -1;
    if (mi_is_zero(rational->denominator))
        return -1;

    if (mi_is_zero(rational->numerator)) {
        if (mi_set_long(rational->denominator, 1) != 0)
            return -1;
        return 0;
    }

    if (mi_is_negative(rational->denominator)) {
        if (mi_neg(rational->denominator) != 0 || mi_neg(rational->numerator) != 0)
            return -1;
    }

    num_abs = mi_clone(rational->numerator);
    gcd = mi_clone(rational->denominator);
    if (!num_abs || !gcd)
        goto cleanup;
    if (mi_abs(num_abs) != 0 || mi_gcd(gcd, num_abs) != 0)
        goto cleanup;
    if (!mi_is_zero(gcd) && mi_cmp_long(gcd, 1) != 0) {
        if (mi_div(rational->numerator, gcd, NULL) != 0 ||
            mi_div(rational->denominator, gcd, NULL) != 0)
            goto cleanup;
    }

    mi_free(gcd);
    mi_free(num_abs);
    return 0;

cleanup:
    mi_free(gcd);
    mi_free(num_abs);
    return -1;
}

int mr_copy_value(mrational_t *dst, const mrational_t *src)
{
    if (!dst || !src)
        return -1;
    return mr_set_frac_mints(dst, src->numerator, src->denominator);
}

mrational_t *mr_new(void)
{
    mrational_t *rational = calloc(1, sizeof(*rational));

    if (!rational)
        return NULL;
    rational->numerator = mi_new();
    rational->denominator = mi_create_long(1);
    if (!rational->numerator || !rational->denominator) {
        mr_free(rational);
        return NULL;
    }
    return rational;
}

mrational_t *mr_create_long(long value)
{
    mrational_t *rational = mr_new();

    if (!rational)
        return NULL;
    if (mr_set_long(rational, value) != 0) {
        mr_free(rational);
        return NULL;
    }
    return rational;
}

mrational_t *mr_create_frac_long(long numerator, long denominator)
{
    mrational_t *rational = mr_new();

    if (!rational)
        return NULL;
    if (mr_set_frac_long(rational, numerator, denominator) != 0) {
        mr_free(rational);
        return NULL;
    }
    return rational;
}

mrational_t *mr_create_string(const char *text)
{
    mrational_t *rational = mr_new();

    if (!rational)
        return NULL;
    if (mr_set_string(rational, text) != 0) {
        mr_free(rational);
        return NULL;
    }
    return rational;
}

mrational_t *mr_clone(const mrational_t *rational)
{
    mrational_t *copy;

    if (!rational)
        return NULL;
    copy = mr_new();
    if (!copy)
        return NULL;
    if (mr_copy_value(copy, rational) != 0) {
        mr_free(copy);
        return NULL;
    }
    return copy;
}

void mr_free(mrational_t *rational)
{
    if (!rational)
        return;
    mi_free(rational->numerator);
    mi_free(rational->denominator);
    free(rational);
}

void mr_clear(mrational_t *rational)
{
    if (!rational)
        return;
    mi_clear(rational->numerator);
    if (rational->denominator)
        mi_set_long(rational->denominator, 1);
}

int mr_set_long(mrational_t *rational, long value)
{
    if (!rational || !rational->numerator || !rational->denominator)
        return -1;
    if (mi_set_long(rational->numerator, value) != 0 ||
        mi_set_long(rational->denominator, 1) != 0)
        return -1;
    return 0;
}

int mr_set_frac_long(mrational_t *rational, long numerator, long denominator)
{
    if (!rational || denominator == 0)
        return -1;
    if (mi_set_long(rational->numerator, numerator) != 0 ||
        mi_set_long(rational->denominator, denominator) != 0)
        return -1;
    return mr_normalise(rational);
}

int mr_set_string(mrational_t *rational, const char *text)
{
    char *copy = NULL;
    char *slash = NULL;
    mint_t *num = NULL;
    mint_t *den = NULL;
    int rc = -1;

    if (!rational || !text)
        return -1;
    copy = mr_dup_trimmed(text);
    if (!copy || copy[0] == '\0')
        goto cleanup;

    slash = strchr(copy, '/');
    if (!slash) {
        num = mi_create_string(copy);
        den = mi_create_long(1);
    } else {
        *slash = '\0';
        slash++;
        num = mi_create_string(copy);
        den = mi_create_string(slash);
    }
    if (!num || !den)
        goto cleanup;
    rc = mr_set_frac_mints(rational, num, den);

cleanup:
    free(copy);
    mi_free(num);
    mi_free(den);
    return rc;
}

char *mr_to_string(const mrational_t *rational)
{
    char *num = NULL;
    char *den = NULL;
    char *text = NULL;
    size_t num_len, den_len;

    if (!rational)
        return NULL;
    num = mi_to_string(rational->numerator);
    den = mi_to_string(rational->denominator);
    if (!num || !den)
        goto cleanup;
    if (mi_cmp_long(rational->denominator, 1) == 0) {
        text = num;
        num = NULL;
        goto cleanup;
    }
    num_len = strlen(num);
    den_len = strlen(den);
    text = calloc(num_len + den_len + 2u, 1u);
    if (!text)
        goto cleanup;
    memcpy(text, num, num_len);
    text[num_len] = '/';
    memcpy(text + num_len + 1u, den, den_len);

cleanup:
    free(num);
    free(den);
    return text;
}

bool mr_is_zero(const mrational_t *rational)
{
    return rational && rational->numerator && mi_is_zero(rational->numerator);
}

bool mr_is_integer(const mrational_t *rational)
{
    return rational && rational->denominator && mi_cmp_long(rational->denominator, 1) == 0;
}

mint_t *mr_numerator(const mrational_t *rational)
{
    if (!rational)
        return NULL;
    return mi_clone(rational->numerator);
}

mint_t *mr_denominator(const mrational_t *rational)
{
    if (!rational)
        return NULL;
    return mi_clone(rational->denominator);
}

int mr_cmp(const mrational_t *a, const mrational_t *b)
{
    mint_t *lhs = NULL;
    mint_t *rhs = NULL;
    int cmp = 0;

    if (!a || !b)
        return 0;
    lhs = mi_clone(a->numerator);
    rhs = mi_clone(b->numerator);
    if (!lhs || !rhs)
        goto cleanup;
    if (mi_mul(lhs, b->denominator) != 0 || mi_mul(rhs, a->denominator) != 0)
        goto cleanup;
    cmp = mi_cmp(lhs, rhs);

cleanup:
    mi_free(lhs);
    mi_free(rhs);
    return cmp;
}

bool mr_eq(const mrational_t *a, const mrational_t *b) { return mr_cmp(a, b) == 0; }
bool mr_lt(const mrational_t *a, const mrational_t *b) { return mr_cmp(a, b) < 0; }
bool mr_le(const mrational_t *a, const mrational_t *b) { return mr_cmp(a, b) <= 0; }
bool mr_gt(const mrational_t *a, const mrational_t *b) { return mr_cmp(a, b) > 0; }
bool mr_ge(const mrational_t *a, const mrational_t *b) { return mr_cmp(a, b) >= 0; }

int mr_neg(mrational_t *rational)
{
    if (!rational)
        return -1;
    return mi_neg(rational->numerator);
}

int mr_abs(mrational_t *rational)
{
    if (!rational)
        return -1;
    return mi_abs(rational->numerator);
}

int mr_inv(mrational_t *rational)
{
    mint_t *tmp = NULL;

    if (!rational || mi_is_zero(rational->numerator))
        return -1;
    tmp = rational->numerator;
    rational->numerator = rational->denominator;
    rational->denominator = tmp;
    return mr_normalise(rational);
}

int mr_add(mrational_t *rational, const mrational_t *other)
{
    mint_t *lhs_num = NULL, *rhs_num = NULL, *den = NULL;
    int rc = -1;

    if (!rational || !other)
        return -1;
    lhs_num = mi_clone(rational->numerator);
    rhs_num = mi_clone(other->numerator);
    den = mi_clone(rational->denominator);
    if (!lhs_num || !rhs_num || !den)
        goto cleanup;
    if (mi_mul(lhs_num, other->denominator) != 0 ||
        mi_mul(rhs_num, rational->denominator) != 0 ||
        mi_add(lhs_num, rhs_num) != 0 ||
        mi_mul(den, other->denominator) != 0)
        goto cleanup;
    rc = mr_set_frac_mints(rational, lhs_num, den);

cleanup:
    mi_free(lhs_num);
    mi_free(rhs_num);
    mi_free(den);
    return rc;
}

int mr_sub(mrational_t *rational, const mrational_t *other)
{
    mrational_t *tmp;
    int rc;

    if (!rational || !other)
        return -1;
    tmp = mr_clone(other);
    if (!tmp)
        return -1;
    if (mr_neg(tmp) != 0) {
        mr_free(tmp);
        return -1;
    }
    rc = mr_add(rational, tmp);
    mr_free(tmp);
    return rc;
}

int mr_mul(mrational_t *rational, const mrational_t *other)
{
    mint_t *num = NULL;
    mint_t *den = NULL;
    int rc = -1;

    if (!rational || !other)
        return -1;
    num = mi_clone(rational->numerator);
    den = mi_clone(rational->denominator);
    if (!num || !den)
        goto cleanup;
    if (mi_mul(num, other->numerator) != 0 || mi_mul(den, other->denominator) != 0)
        goto cleanup;
    rc = mr_set_frac_mints(rational, num, den);

cleanup:
    mi_free(num);
    mi_free(den);
    return rc;
}

int mr_div(mrational_t *rational, const mrational_t *other)
{
    mrational_t *tmp;
    int rc;

    if (!rational || !other || mr_is_zero(other))
        return -1;
    tmp = mr_clone(other);
    if (!tmp)
        return -1;
    if (mr_inv(tmp) != 0) {
        mr_free(tmp);
        return -1;
    }
    rc = mr_mul(rational, tmp);
    mr_free(tmp);
    return rc;
}
