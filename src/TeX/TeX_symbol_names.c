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

static const int16_t symbol_displacements[59] = {
    [ 0] =  -1,
    [ 1] =   0,
    [ 2] =  -5,
    [ 3] =   1,
    [ 4] =   1,
    [ 5] =   3,
    [ 6] =  -6,
    [ 7] =  -7,
    [ 8] =  -8,
    [ 9] = -10,
    [10] =   0,
    [11] = -11,
    [12] =   0,
    [13] =   0,
    [14] = -12,
    [15] =   0,
    [16] = -13,
    [17] =   2,
    [18] =   1,
    [19] =   0,
    [20] = -14,
    [21] =   1,
    [22] =   0,
    [23] = -19,
    [24] = -27,
    [25] =   0,
    [26] =   0,
    [27] =   2,
    [28] =   0,
    [29] = -32,
    [30] =   1,
    [31] =   2,
    [32] =   0,
    [33] =   0,
    [34] =   0,
    [35] = -33,
    [36] =   0,
    [37] =   0,
    [38] =   0,
    [39] = -34,
    [40] =   0,
    [41] =   1,
    [42] =   0,
    [43] = -36,
    [44] =   3,
    [45] = -37,
    [46] = -39,
    [47] =   0,
    [48] = -41,
    [49] = -42,
    [50] =   1,
    [51] =   0,
    [52] = -49,
    [53] =   4,
    [54] =   0,
    [55] =   2,
    [56] = -50,
    [57] = -52,
    [58] = -58,
};

static const struct { const char *name; unsigned codepoint; } symbol_names[59] = {
    [ 0] = {"Epsilon"     , 0x395u},
    [ 1] = {"Eta"         , 0x397u},
    [ 2] = {"Nu"          , 0x39du},
    [ 3] = {"Gamma"       , 0x393u},
    [ 4] = {"Rho"         , 0x3a1u},
    [ 5] = {"hbar"        , 0x210fu},
    [ 6] = {"varkappa"    , 0x3f0u},
    [ 7] = {"iota"        , 0x3b9u},
    [ 8] = {"chi"         , 0x3c7u},
    [ 9] = {"zeta"        , 0x3b6u},
    [10] = {"Sigma"       , 0x3a3u},
    [11] = {"kappa"       , 0x3bau},
    [12] = {"Pi"          , 0x3a0u},
    [13] = {"varepsilon"  , 0x3f5u},
    [14] = {"Zeta"        , 0x396u},
    [15] = {"varrho"      , 0x3f1u},
    [16] = {"Upsilon"     , 0x3a5u},
    [17] = {"mu"          , 0x3bcu},
    [18] = {"alpha"       , 0x3b1u},
    [19] = {"omega"       , 0x3c9u},
    [20] = {"Phi"         , 0x3a6u},
    [21] = {"sigma"       , 0x3c3u},
    [22] = {"upsilon"     , 0x3c5u},
    [23] = {"delta"       , 0x3b4u},
    [24] = {"Omicron"     , 0x39fu},
    [25] = {"varphi"      , 0x3d5u},
    [26] = {"rho"         , 0x3c1u},
    [27] = {"lambda"      , 0x3bbu},
    [28] = {"xi"          , 0x3beu},
    [29] = {"eta"         , 0x3b7u},
    [30] = {"infty"       , 0x221eu},
    [31] = {"infinity"    , 0x221eu},
    [32] = {"varsigma"    , 0x3c2u},
    [33] = {"Delta"       , 0x394u},
    [34] = {"beta"        , 0x3b2u},
    [35] = {"Theta"       , 0x398u},
    [36] = {"psi"         , 0x3c8u},
    [37] = {"vartheta"    , 0x3d1u},
    [38] = {"varpi"       , 0x3d6u},
    [39] = {"Omega"       , 0x3a9u},
    [40] = {"Iota"        , 0x399u},
    [41] = {"Alpha"       , 0x391u},
    [42] = {"Lambda"      , 0x39bu},
    [43] = {"Tau"         , 0x3a4u},
    [44] = {"epsilon"     , 0x3b5u},
    [45] = {"Chi"         , 0x3a7u},
    [46] = {"pi"          , 0x3c0u},
    [47] = {"Psi"         , 0x3a8u},
    [48] = {"omicron"     , 0x3bfu},
    [49] = {"Kappa"       , 0x39au},
    [50] = {"nu"          , 0x3bdu},
    [51] = {"Xi"          , 0x39eu},
    [52] = {"gamma"       , 0x3b3u},
    [53] = {"Mu"          , 0x39cu},
    [54] = {"partial"     , 0x2202u},
    [55] = {"Beta"        , 0x392u},
    [56] = {"phi"         , 0x3c6u},
    [57] = {"tau"         , 0x3c4u},
    [58] = {"theta"       , 0x3b8u},
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
    static const struct { unsigned codepoint; const char *source; } glyphs[5] = {
        [1] = {0x2202u, "\\partial"},
        [3] = {0x210fu, "\\hbar"},
        [4] = {0x221eu, "\\infty"}
    };
    if (codepoint < 0x0400u)
        return TeX_semantic_greek(codepoint);
    size_t glyph = codepoint % 5u;
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
