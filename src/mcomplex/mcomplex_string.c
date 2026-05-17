#include <stdlib.h>
#include <string.h>

#include "mcomplex_internal.h"
#include "mrational.h"

static char *mcomplex_strip_spaces(const char *text)
{
    size_t len = 0u;
    char *out;

    if (!text)
        return NULL;

    for (const char *p = text; *p; ++p) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            ++len;
    }

    out = malloc(len + 1u);
    if (!out)
        return NULL;

    len = 0u;
    for (const char *p = text; *p; ++p) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            out[len++] = *p;
    }
    out[len] = '\0';
    return out;
}

static int mcomplex_find_split(const char *text)
{
    int split = -1;

    if (!text)
        return -1;

    for (int i = 1; text[i] != '\0'; ++i) {
        if ((text[i] == '+' || text[i] == '-') &&
            text[i - 1] != 'e' && text[i - 1] != 'E') {
            split = i;
        }
    }

    return split;
}

static char *mcomplex_normalize_scalar_token(const char *text)
{
    const char *body;
    size_t len;
    int has_sign;
    char *out;

    if (!text)
        return NULL;

    has_sign = text[0] == '+' || text[0] == '-';
    body = text + has_sign;
    len = strlen(body);

    if (len >= 2u && body[0] == '(' && body[len - 1u] == ')') {
        body++;
        len -= 2u;
    }

    out = malloc((size_t)has_sign + len + 1u);
    if (!out)
        return NULL;

    if (has_sign)
        out[0] = text[0];
    memcpy(out + has_sign, body, len);
    out[has_sign + len] = '\0';
    return out;
}

static int mcomplex_has_unicode_fraction_text(const char *text)
{
    static const char *const glyphs[] = {
        "½", "⅓", "⅔", "¼", "¾", "⅕", "⅖", "⅗", "⅘",
        "⅙", "⅚", "⅐", "⅛", "⅜", "⅝", "⅞", "⅑", "⅒",
    };
    size_t i;

    if (!text)
        return 0;
    if (strstr(text, "⁄"))
        return 1;

    for (i = 0u; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
        if (strstr(text, glyphs[i]))
            return 1;
    }

    return 0;
}

static int mcomplex_set_scalar_token(mfloat_t *value, const char *text)
{
    char *normalized = NULL;
    mrational_t *rational = NULL;
    int rc = -1;

    if (!value || !text)
        return -1;

    normalized = mcomplex_normalize_scalar_token(text);
    if (!normalized || normalized[0] == '\0')
        goto cleanup;

    if (strchr(normalized, '/') || mcomplex_has_unicode_fraction_text(normalized)) {
        rational = mr_create_string(normalized);
        if (!rational)
            goto cleanup;
        rc = mf_set_mrational(value, rational);
        goto cleanup;
    }

    rc = mf_set_string(value, normalized);

cleanup:
    free(normalized);
    mr_free(rational);
    return rc;
}

static int mcomplex_set_imag_token(mfloat_t *imag, const char *text)
{
    if (!imag || !text)
        return -1;
    if (text[0] == '\0' || strcmp(text, "+") == 0)
        return mf_set_string(imag, "1");
    if (strcmp(text, "-") == 0)
        return mf_set_string(imag, "-1");
    return mcomplex_set_scalar_token(imag, text);
}

mcomplex_t *mc_create_string(const char *text)
{
    mcomplex_t *mcomplex = mc_new();

    if (!mcomplex || mc_set_string(mcomplex, text) != 0) {
        mc_free(mcomplex);
        return NULL;
    }
    return mcomplex;
}

int mc_set_string(mcomplex_t *mcomplex, const char *text)
{
    char *compact = NULL;
    char *imag_text = NULL;
    mfloat_t *real = NULL;
    mfloat_t *imag = NULL;
    int split;
    size_t precision_bits;
    int rc = -1;

    if (!mcomplex || !text || mcomplex_prepare_mutable(mcomplex) != 0)
        return -1;

    compact = mcomplex_strip_spaces(text);
    if (!compact)
        goto cleanup;

    precision_bits = mc_get_precision(mcomplex);
    real = mf_new_prec(precision_bits);
    imag = mf_new_prec(precision_bits);
    if (!real || !imag)
        goto cleanup;

    if (!strchr(compact, 'i')) {
        if (mf_set_string(real, compact) != 0)
            goto cleanup;
        mf_clear(imag);
        rc = mc_set(mcomplex, real, imag);
        goto cleanup;
    }

    if (strchr(compact, 'i') != strrchr(compact, 'i') ||
        compact[strlen(compact) - 1u] != 'i')
        goto cleanup;

    compact[strlen(compact) - 1u] = '\0';
    split = mcomplex_find_split(compact);

    if (split >= 0) {
        char sign = compact[split];

        compact[split] = '\0';
        if (mcomplex_set_scalar_token(real, compact) != 0)
            goto cleanup;
        compact[split] = sign;
        imag_text = compact + split;
    } else {
        mf_clear(real);
        imag_text = compact;
    }

    if (mcomplex_set_imag_token(imag, imag_text) != 0)
        goto cleanup;

    rc = mc_set(mcomplex, real, imag);

cleanup:
    free(compact);
    mf_free(real);
    mf_free(imag);
    return rc;
}

char *mc_to_string(const mcomplex_t *mcomplex)
{
    int needed;
    char *out;

    if (!mcomplex)
        return NULL;

    needed = mc_sprintf(NULL, 0u, "%mz", mcomplex);
    if (needed < 0)
        return NULL;

    out = malloc((size_t)needed + 1u);
    if (!out)
        return NULL;

    if (mc_sprintf(out, (size_t)needed + 1u, "%mz", mcomplex) < 0) {
        free(out);
        return NULL;
    }

    return out;
}
