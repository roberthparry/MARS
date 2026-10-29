/**
 * @file TeX_semantic_symbols.c
 * @brief Compact direct lookups for Unicode Greek symbols.
 *
 * The common uppercase and lowercase ranges use direct indexing. Six variants
 * use a small perfect hash with an exact code-point check for non-members.
 */
#include <stddef.h>

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

static const char *const greek_uppercase[25] = {
    [ 0] = "A",
    [ 1] = "B",
    [ 2] = "\\Gamma",
    [ 3] = "\\Delta",
    [ 4] = "E",
    [ 5] = "Z",
    [ 6] = "H",
    [ 7] = "\\Theta",
    [ 8] = "I",
    [ 9] = "K",
    [10] = "\\Lambda",
    [11] = "M",
    [12] = "N",
    [13] = "\\Xi",
    [14] = "O",
    [15] = "\\Pi",
    [16] = "P",
    [18] = "\\Sigma",
    [19] = "T",
    [20] = "\\Upsilon",
    [21] = "\\Phi",
    [22] = "X",
    [23] = "\\Psi",
    [24] = "\\Omega",
};

static const char *const greek_lowercase[25] = {
    [ 0] = "\\alpha",
    [ 1] = "\\beta",
    [ 2] = "\\gamma",
    [ 3] = "\\delta",
    [ 4] = "\\epsilon",
    [ 5] = "\\zeta",
    [ 6] = "\\eta",
    [ 7] = "\\theta",
    [ 8] = "\\iota",
    [ 9] = "\\kappa",
    [10] = "\\lambda",
    [11] = "\\mu",
    [12] = "\\nu",
    [13] = "\\xi",
    [14] = "o",
    [15] = "\\pi",
    [16] = "\\rho",
    [17] = "\\varsigma",
    [18] = "\\sigma",
    [19] = "\\tau",
    [20] = "\\upsilon",
    [21] = "\\phi",
    [22] = "\\chi",
    [23] = "\\psi",
    [24] = "\\omega",
};

typedef struct {
    unsigned codepoint;
    const char *notation;
} TeX_greek_variant_t;

static const TeX_greek_variant_t greek_variants[7] = {
    [0] = {0x03D1u, "\\vartheta"},
    [2] = {0x03D5u, "\\varphi"},
    [3] = {0x03F0u, "\\varkappa"},
    [4] = {0x03F1u, "\\varrho"},
    [5] = {0x03D6u, "\\varpi"},
    [6] = {0x03F5u, "\\varepsilon"},
};

/* Resolve one code point with direct indexing or one perfect-hash probe. */
const char *TeX_semantic_greek(unsigned codepoint)
{
    if (codepoint >= 0x0391u && codepoint <= 0x03A9u)
        return greek_uppercase[codepoint - 0x0391u];
    if (codepoint >= 0x03B1u && codepoint <= 0x03C9u)
        return greek_lowercase[codepoint - 0x03B1u];
    const TeX_greek_variant_t *entry = &greek_variants[(codepoint ^ (codepoint >> 2u)) % 7u];
    return entry->codepoint == codepoint ? entry->notation : NULL;
}
