/**
 * @file string_shell.c
 * @brief Literal shell-word decoding for configuration strings.
 *
 * Implements string_unquote_shell from ustring.h. A single bounded pass removes
 * quote delimiters and eligible escape backslashes while preserving the exact
 * remaining UTF-8 bytes. Concatenated fragments support installer-generated
 * quoting of passwords containing apostrophes; no expansion, command execution
 * or environment access occurs. Double-quote escapes follow shlex semantics.
 *
 * The implementation owns its new string and reads only the borrowed input.
 * Private string storage is used to avoid Unicode normalisation during decoding.
 * No global state is used; separate calls require no shared synchronisation.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#define MARS_STRING_INTERNAL_ACCESS
#include "string_internal.h"

/* Remove literal shell quoting without changing the spelling or interpreting the value. */
string_t *string_unquote_shell(const string_t *text)
{
    if (!text) {
        errno = EINVAL;
        return NULL;
    }
    if (text->len == SIZE_MAX) {
        errno = EOVERFLOW;
        return NULL;
    }
    string_t *value = string_new();
    if (!value) {
        errno = ENOMEM;
        return NULL;
    }
    /* Decoding never grows the input. Reserve exactly once without a doubling overflow. */
    if (value->cap <= text->len) {
        char *data = realloc(value->data, text->len + 1);
        if (!data) {
            string_free(value);
            errno = ENOMEM;
            return NULL;
        }
        value->data = data;
        value->cap = text->len + 1;
    }
    char quote = 0;
    bool started = false, finished = false, comment = false;
    for (size_t i = 0; i < text->len; ++i) {
        char ch = text->data[i];
        if (!ch || ch == '\r' || ch == '\n')
            goto invalid;
        if (comment)
            continue;
        if (finished) {
            if (ch == '#')
                comment = true;
            else if (ch != ' ' && ch != '\t')
                goto invalid;
            continue;
        }
        if (!quote && (ch == ' ' || ch == '\t')) {
            finished = started;
            continue;
        }
        if (!quote && !started && ch == '#') {
            comment = true;
            continue;
        }
        started = true;
        if (!quote && (ch == '\'' || ch == '"')) {
            quote = ch;
        } else if (quote && ch == quote) {
            quote = 0;
        } else if (ch == '\\' && quote != '\'') {
            if (++i == text->len)
                goto invalid;
            char next = text->data[i];
            if (!next || next == '\r' || next == '\n')
                goto invalid;
            if (quote == '"' && next != '\\' && next != '"')
                value->data[value->len++] = '\\';
            value->data[value->len++] = next;
        } else {
            value->data[value->len++] = ch;
        }
    }
    if (quote)
        goto invalid;
    value->data[value->len] = '\0';
    return value;

invalid:
    string_free(value);
    errno = EINVAL;
    return NULL;
}
