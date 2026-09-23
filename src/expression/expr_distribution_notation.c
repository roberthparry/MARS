#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ustring.h"

/* Match one complete, explicitly grouped occurrence, not an identifier or term substring. */
const char *expr_distribution_group(const char *body, const char *operand)
{
    size_t length = strlen(operand);
    const char *found = NULL;
    unsigned brackets = 0u;
    /* A single lexical pass over the rendered body, bounded by its text length. */
    for (const char *p = body; *p; ++p) {
        if (*p == '[')
            ++brackets;
        else if (*p == ']' && brackets)
            --brackets;
        else if (!brackets && *p == '(' && strncmp(p + 1, operand, length) == 0 && p[length + 1u] == ')') {
            if (found)
                return NULL;
            found = p;
        }
    }
    return found;
}

static string_t *distribution_trimmed(const char *begin, const char *end)
{
    while (begin < end && isspace((unsigned char)*begin))
        ++begin;
    while (end > begin && isspace((unsigned char)end[-1]))
        --end;
    string_t *text = string_new();
    if (text)
        string_append_chars(text, begin, (size_t)(end - begin));
    return text;
}

/* Restore detached bound-Expression qualifications before parsing or simplifying the body. */
bool expr_distribution_restore(string_t **body, string_t **conditions)
{
    const char *source = string_c_str(*conditions);
    const char *start = source;
    const char *colon = NULL;
    unsigned depth = 0u;
    string_t *remaining = string_new();
    if (!remaining)
        return false;
    for (const char *p = source;; ++p) {
        if (*p == '(' || *p == '[')
            ++depth;
        else if ((*p == ')' || *p == ']') && depth)
            --depth;
        else if (*p == ':' && !depth)
            colon = p;
        if (*p && (*p != ';' || depth))
            continue;
        if (colon) {
            string_t *operand = distribution_trimmed(start, colon);
            string_t *label = distribution_trimmed(colon + 1, p);
            const char *kind = label ? string_c_str(label) : "";
            bool recognised = strcmp(kind, "principal value") == 0 || strcmp(kind, "finite part") == 0;
            const char *original = string_c_str(*body);
            const char *match = recognised && operand ? expr_distribution_group(original, string_c_str(operand)) : NULL;
            if (!match) {
                string_free(operand);
                string_free(label);
                string_free(remaining);
                return false;
            }
            string_t *restored = string_new();
            if (!restored) {
                string_free(operand);
                string_free(label);
                string_free(remaining);
                return false;
            }
            string_append_chars(restored, original, (size_t)(match - original));
            string_append_cstr(restored, "(");
            string_append_cstr(restored, string_c_str(operand));
            string_append_cstr(restored, " : ");
            string_append_cstr(restored, kind);
            string_append_cstr(restored, ")");
            string_append_cstr(restored, match + strlen(string_c_str(operand)) + 2u);
            string_free(*body);
            *body = restored;
            string_free(operand);
            string_free(label);
        } else {
            string_t *condition = distribution_trimmed(start, p);
            if (!condition) {
                string_free(remaining);
                return false;
            }
            if (string_byte_length(condition)) {
                if (string_byte_length(remaining))
                    string_append_cstr(remaining, "; ");
                string_append_cstr(remaining, string_c_str(condition));
            }
            string_free(condition);
        }
        if (!*p)
            break;
        start = p + 1;
        colon = NULL;
    }
    string_free(*conditions);
    *conditions = remaining;
    return true;
}
