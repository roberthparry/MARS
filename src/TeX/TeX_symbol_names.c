/**
 * @file TeX_symbol_names.c
 * @brief Compact perfect-hash spelling of keyboard-friendly mathematical symbols.
 *
 * All standard Greek names, their variants and common mathematical glyphs have
 * ASCII names. Two fixed hash probes select one dense candidate, followed by an
 * exact name check. Unicode spelling remains available through the symbol API.
 */
#include <stdint.h>
#include <string.h>

#include "string-t.h"

#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

static const int16_t symbol_displacements[60] = {
    [ 0] =   0,
    [ 1] =   1,
    [ 2] =   2,
    [ 3] =   0,
    [ 4] =   1,
    [ 5] =   0,
    [ 6] =  -1,
    [ 7] =   1,
    [ 8] =   0,
    [ 9] =  -2,
    [10] =  -8,
    [11] =   4,
    [12] =   0,
    [13] = -11,
    [14] = -14,
    [15] = -19,
    [16] = -20,
    [17] =   1,
    [18] =   0,
    [19] =   0,
    [20] =   2,
    [21] =   0,
    [22] =   0,
    [23] =   1,
    [24] =   0,
    [25] = -21,
    [26] = -22,
    [27] =   0,
    [28] = -25,
    [29] =   0,
    [30] = -26,
    [31] = -41,
    [32] =   1,
    [33] =   0,
    [34] =   0,
    [35] = -43,
    [36] =   0,
    [37] =   0,
    [38] =  21,
    [39] =  14,
    [40] = -46,
    [41] =   3,
    [42] =   0,
    [43] =   2,
    [44] =   3,
    [45] = -48,
    [46] =   6,
    [47] = -49,
    [48] = -51,
    [49] = -52,
    [50] =   0,
    [51] =   0,
    [52] = -54,
    [53] =   1,
    [54] = -58,
    [55] =   0,
    [56] =   3,
    [57] = -60,
    [58] =   0,
    [59] =   0,
};

static const struct { const char *name; unsigned codepoint; } symbol_names[60] = {
    [ 0] = {"partial"     , 0x2202u},
    [ 1] = {"nabla"       , 0x2207u},
    [ 2] = {"Zeta"        , 0x396u},
    [ 3] = {"Omicron"     , 0x39fu},
    [ 4] = {"Gamma"       , 0x393u},
    [ 5] = {"omega"       , 0x3c9u},
    [ 6] = {"Omega"       , 0x3a9u},
    [ 7] = {"Iota"        , 0x399u},
    [ 8] = {"varrho"      , 0x3f1u},
    [ 9] = {"psi"         , 0x3c8u},
    [10] = {"varpi"       , 0x3d6u},
    [11] = {"xi"          , 0x3beu},
    [12] = {"pi"          , 0x3c0u},
    [13] = {"lambda"      , 0x3bbu},
    [14] = {"epsilon"     , 0x3b5u},
    [15] = {"nu"          , 0x3bdu},
    [16] = {"Theta"       , 0x398u},
    [17] = {"varepsilon"  , 0x3f5u},
    [18] = {"mu"          , 0x3bcu},
    [19] = {"sigma"       , 0x3c3u},
    [20] = {"delta"       , 0x3b4u},
    [21] = {"vartheta"    , 0x3d1u},
    [22] = {"upsilon"     , 0x3c5u},
    [23] = {"beta"        , 0x3b2u},
    [24] = {"Phi"         , 0x3a6u},
    [25] = {"Pi"          , 0x3a0u},
    [26] = {"kappa"       , 0x3bau},
    [27] = {"phi"         , 0x3c6u},
    [28] = {"eta"         , 0x3b7u},
    [29] = {"zeta"        , 0x3b6u},
    [30] = {"Beta"        , 0x392u},
    [31] = {"hbar"        , 0x210fu},
    [32] = {"Lambda"      , 0x39bu},
    [33] = {"Sigma"       , 0x3a3u},
    [34] = {"chi"         , 0x3c7u},
    [35] = {"omicron"     , 0x3bfu},
    [36] = {"varkappa"    , 0x3f0u},
    [37] = {"theta"       , 0x3b8u},
    [38] = {"Epsilon"     , 0x395u},
    [39] = {"Xi"          , 0x39eu},
    [40] = {"Eta"         , 0x397u},
    [41] = {"Alpha"       , 0x391u},
    [42] = {"Psi"         , 0x3a8u},
    [43] = {"Nu"          , 0x39du},
    [44] = {"tau"         , 0x3c4u},
    [45] = {"Rho"         , 0x3a1u},
    [46] = {"infinity"    , 0x221eu},
    [47] = {"Tau"         , 0x3a4u},
    [48] = {"alpha"       , 0x3b1u},
    [49] = {"Mu"          , 0x39cu},
    [50] = {"rho"         , 0x3c1u},
    [51] = {"varphi"      , 0x3d5u},
    [52] = {"varsigma"    , 0x3c2u},
    [53] = {"Kappa"       , 0x39au},
    [54] = {"Delta"       , 0x394u},
    [55] = {"infty"       , 0x221eu},
    [56] = {"iota"        , 0x3b9u},
    [57] = {"gamma"       , 0x3b3u},
    [58] = {"Chi"         , 0x3a7u},
    [59] = {"Upsilon"     , 0x3a5u},
};

static uint32_t TeX_symbol_hash(const char *name, uint32_t seed)
{
    uint32_t hash = UINT32_C(2166136261) ^ seed;
    /* Reading the input name is necessary; candidate selection never scans the table. */
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
        hash = (hash ^ *p) * UINT32_C(16777619);
    return hash;
}

/* Share Greek and mathematical-glyph spelling between Unicode and ASCII input. */
const char *TeX_symbol_codepoint(unsigned codepoint)
{
    static const struct { unsigned codepoint; const char *source; } glyphs[6] = {
        [0] = {0x2202u, "\\partial"},
        [3] = {0x210fu, "\\hbar"},
        [4] = {0x221eu, "\\infty"},
        [5] = {0x2207u, "\\nabla"}
    };
    if (codepoint < 0x0400u)
        return TeX_semantic_greek(codepoint);
    size_t glyph = codepoint % 6u;
    return glyphs[glyph].codepoint == codepoint ? glyphs[glyph].source : NULL;
}

/* Resolve an ASCII name without confusing non-members or embedded NUL with a known symbol. */
const char *TeX_symbol_name(const string_t *name)
{
    if (!name || !string_byte_length(name))
        return NULL;
    const char *text = string_c_str(name);
    if (strlen(text) != string_byte_length(name))
        return NULL;
    const size_t count = sizeof symbol_names / sizeof *symbol_names;
    int displacement = symbol_displacements[TeX_symbol_hash(text, 0) % count];
    if (!displacement)
        return NULL;
    size_t slot = displacement < 0 ? (size_t)(-displacement - 1)
        : TeX_symbol_hash(text, (uint32_t)displacement) % count;
    if (strcmp(symbol_names[slot].name, text))
        return NULL;
    return TeX_symbol_codepoint(symbol_names[slot].codepoint);
}
