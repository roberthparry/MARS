/**
 * @file almanac_entries.c
 * @brief Body catalogue lookup and computed almanac entries.
 *
 * Resolves catalogue identifiers and constructs entries and snapshots containing positions, magnitudes and related
 * body properties. This is the boundary between internal position models and the public per-body results.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Body catalogue, computed entries, magnitudes and snapshots. */
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "almanac_engine_internal.h"

static const char *const ALMANAC_BODY_CODES[ALMANAC_BODY_ID_COUNT] = {
    [ALMANAC_BODY_ID_SUN]             = "SUN",
    [ALMANAC_BODY_ID_MOON]            = "MOON",
    [ALMANAC_BODY_ID_MERCURY]         = "MERCURY",
    [ALMANAC_BODY_ID_VENUS]           = "VENUS",
    [ALMANAC_BODY_ID_MARS]            = "MARS",
    [ALMANAC_BODY_ID_JUPITER]         = "JUPITER",
    [ALMANAC_BODY_ID_SATURN]          = "SATURN",
    [ALMANAC_BODY_ID_URANUS]          = "URANUS",
    [ALMANAC_BODY_ID_NEPTUNE]         = "NEPTUNE",
    [ALMANAC_BODY_ID_ACAMAR]          = "ACAMAR",
    [ALMANAC_BODY_ID_ACHERNAR]        = "ACHERNAR",
    [ALMANAC_BODY_ID_ACRUX]           = "ACRUX",
    [ALMANAC_BODY_ID_ADHARA]          = "ADHARA",
    [ALMANAC_BODY_ID_ALNAIR]          = "ALNAIR",
    [ALMANAC_BODY_ID_ALDEBARAN]       = "ALDEBARAN",
    [ALMANAC_BODY_ID_ALIOTH]          = "ALIOTH",
    [ALMANAC_BODY_ID_ALKAID]          = "ALKAID",
    [ALMANAC_BODY_ID_ALNILAM]         = "ALNILAM",
    [ALMANAC_BODY_ID_ALPHARD]         = "ALPHARD",
    [ALMANAC_BODY_ID_ALPHECCA]        = "ALPHECCA",
    [ALMANAC_BODY_ID_ALPHERATZ]       = "ALPHERATZ",
    [ALMANAC_BODY_ID_ALTAIR]          = "ALTAIR",
    [ALMANAC_BODY_ID_ANKAA]           = "ANKAA",
    [ALMANAC_BODY_ID_ANTARES]         = "ANTARES",
    [ALMANAC_BODY_ID_ARCTURUS]        = "ARCTURUS",
    [ALMANAC_BODY_ID_ATRIA]           = "ATRIA",
    [ALMANAC_BODY_ID_AVIOR]           = "AVIOR",
    [ALMANAC_BODY_ID_BELLATRIX]       = "BELLATRIX",
    [ALMANAC_BODY_ID_BETELGEUSE]      = "BETELGEUSE",
    [ALMANAC_BODY_ID_CANOPUS]         = "CANOPUS",
    [ALMANAC_BODY_ID_CAPELLA]         = "CAPELLA",
    [ALMANAC_BODY_ID_DENEB]           = "DENEB",
    [ALMANAC_BODY_ID_DENEBOLA]        = "DENEBOLA",
    [ALMANAC_BODY_ID_DIPHDA]          = "DIPHDA",
    [ALMANAC_BODY_ID_DUBHE]           = "DUBHE",
    [ALMANAC_BODY_ID_ELNATH]          = "ELNATH",
    [ALMANAC_BODY_ID_ELTANIN]         = "ELTANIN",
    [ALMANAC_BODY_ID_ENIF]            = "ENIF",
    [ALMANAC_BODY_ID_FOMALHAUT]       = "FOMALHAUT",
    [ALMANAC_BODY_ID_GACRUX]          = "GACRUX",
    [ALMANAC_BODY_ID_GIENAH]          = "GIENAH",
    [ALMANAC_BODY_ID_HADAR]           = "HADAR",
    [ALMANAC_BODY_ID_HAMAL]           = "HAMAL",
    [ALMANAC_BODY_ID_KAUS_AUSTRALIS]  = "KAUS_AUSTRALIS",
    [ALMANAC_BODY_ID_KOCHAB]          = "KOCHAB",
    [ALMANAC_BODY_ID_MARKAB]          = "MARKAB",
    [ALMANAC_BODY_ID_MENKAR]          = "MENKAR",
    [ALMANAC_BODY_ID_MENKENT]         = "MENKENT",
    [ALMANAC_BODY_ID_MIAPLACIDUS]     = "MIAPLACIDUS",
    [ALMANAC_BODY_ID_MIRFAK]          = "MIRFAK",
    [ALMANAC_BODY_ID_NUNKI]           = "NUNKI",
    [ALMANAC_BODY_ID_PEACOCK]         = "PEACOCK",
    [ALMANAC_BODY_ID_POLARIS]         = "POLARIS",
    [ALMANAC_BODY_ID_POLLUX]          = "POLLUX",
    [ALMANAC_BODY_ID_PROCYON]         = "PROCYON",
    [ALMANAC_BODY_ID_RASALHAGUE]      = "RASALHAGUE",
    [ALMANAC_BODY_ID_REGULUS]         = "REGULUS",
    [ALMANAC_BODY_ID_RIGEL]           = "RIGEL",
    [ALMANAC_BODY_ID_RIGIL_KENTAURUS] = "RIGIL_KENTAURUS",
    [ALMANAC_BODY_ID_SABIK]           = "SABIK",
    [ALMANAC_BODY_ID_SCHEDAR]         = "SCHEDAR",
    [ALMANAC_BODY_ID_SHAULA]          = "SHAULA",
    [ALMANAC_BODY_ID_SIRIUS]          = "SIRIUS",
    [ALMANAC_BODY_ID_SPICA]           = "SPICA",
    [ALMANAC_BODY_ID_SUHAIL]          = "SUHAIL",
    [ALMANAC_BODY_ID_VEGA]            = "VEGA",
    [ALMANAC_BODY_ID_ZUBENELGENUBI]   = "ZUBENELGENUBI",
    [ALMANAC_BODY_ID_SIGMA_OCTANTIS]  = "SIGMA_OCTANTIS"
};

static const char *const ALMANAC_BODY_DISPLAY_NAMES[ALMANAC_BODY_ID_COUNT] = {
    [ALMANAC_BODY_ID_SUN]             = "Sun",
    [ALMANAC_BODY_ID_MOON]            = "Moon",
    [ALMANAC_BODY_ID_MERCURY]         = "Mercury",
    [ALMANAC_BODY_ID_VENUS]           = "Venus",
    [ALMANAC_BODY_ID_MARS]            = "Mars",
    [ALMANAC_BODY_ID_JUPITER]         = "Jupiter",
    [ALMANAC_BODY_ID_SATURN]          = "Saturn",
    [ALMANAC_BODY_ID_URANUS]          = "Uranus",
    [ALMANAC_BODY_ID_NEPTUNE]         = "Neptune",
    [ALMANAC_BODY_ID_ACAMAR]          = "Acamar",
    [ALMANAC_BODY_ID_ACHERNAR]        = "Achernar",
    [ALMANAC_BODY_ID_ACRUX]           = "Acrux",
    [ALMANAC_BODY_ID_ADHARA]          = "Adhara",
    [ALMANAC_BODY_ID_ALNAIR]          = "Alnair",
    [ALMANAC_BODY_ID_ALDEBARAN]       = "Aldebaran",
    [ALMANAC_BODY_ID_ALIOTH]          = "Alioth",
    [ALMANAC_BODY_ID_ALKAID]          = "Alkaid",
    [ALMANAC_BODY_ID_ALNILAM]         = "Alnilam",
    [ALMANAC_BODY_ID_ALPHARD]         = "Alphard",
    [ALMANAC_BODY_ID_ALPHECCA]        = "Alphecca",
    [ALMANAC_BODY_ID_ALPHERATZ]       = "Alpheratz",
    [ALMANAC_BODY_ID_ALTAIR]          = "Altair",
    [ALMANAC_BODY_ID_ANKAA]           = "Ankaa",
    [ALMANAC_BODY_ID_ANTARES]         = "Antares",
    [ALMANAC_BODY_ID_ARCTURUS]        = "Arcturus",
    [ALMANAC_BODY_ID_ATRIA]           = "Atria",
    [ALMANAC_BODY_ID_AVIOR]           = "Avior",
    [ALMANAC_BODY_ID_BELLATRIX]       = "Bellatrix",
    [ALMANAC_BODY_ID_BETELGEUSE]      = "Betelgeuse",
    [ALMANAC_BODY_ID_CANOPUS]         = "Canopus",
    [ALMANAC_BODY_ID_CAPELLA]         = "Capella",
    [ALMANAC_BODY_ID_DENEB]           = "Deneb",
    [ALMANAC_BODY_ID_DENEBOLA]        = "Denebola",
    [ALMANAC_BODY_ID_DIPHDA]          = "Diphda",
    [ALMANAC_BODY_ID_DUBHE]           = "Dubhe",
    [ALMANAC_BODY_ID_ELNATH]          = "Elnath",
    [ALMANAC_BODY_ID_ELTANIN]         = "Eltanin",
    [ALMANAC_BODY_ID_ENIF]            = "Enif",
    [ALMANAC_BODY_ID_FOMALHAUT]       = "Fomalhaut",
    [ALMANAC_BODY_ID_GACRUX]          = "Gacrux",
    [ALMANAC_BODY_ID_GIENAH]          = "Gienah",
    [ALMANAC_BODY_ID_HADAR]           = "Hadar",
    [ALMANAC_BODY_ID_HAMAL]           = "Hamal",
    [ALMANAC_BODY_ID_KAUS_AUSTRALIS]  = "Kaus Australis",
    [ALMANAC_BODY_ID_KOCHAB]          = "Kochab",
    [ALMANAC_BODY_ID_MARKAB]          = "Markab",
    [ALMANAC_BODY_ID_MENKAR]          = "Menkar",
    [ALMANAC_BODY_ID_MENKENT]         = "Menkent",
    [ALMANAC_BODY_ID_MIAPLACIDUS]     = "Miaplacidus",
    [ALMANAC_BODY_ID_MIRFAK]          = "Mirfak",
    [ALMANAC_BODY_ID_NUNKI]           = "Nunki",
    [ALMANAC_BODY_ID_PEACOCK]         = "Peacock",
    [ALMANAC_BODY_ID_POLARIS]         = "Polaris",
    [ALMANAC_BODY_ID_POLLUX]          = "Pollux",
    [ALMANAC_BODY_ID_PROCYON]         = "Procyon",
    [ALMANAC_BODY_ID_RASALHAGUE]      = "Rasalhague",
    [ALMANAC_BODY_ID_REGULUS]         = "Regulus",
    [ALMANAC_BODY_ID_RIGEL]           = "Rigel",
    [ALMANAC_BODY_ID_RIGIL_KENTAURUS] = "Rigil Kentaurus",
    [ALMANAC_BODY_ID_SABIK]           = "Sabik",
    [ALMANAC_BODY_ID_SCHEDAR]         = "Schedar",
    [ALMANAC_BODY_ID_SHAULA]          = "Shaula",
    [ALMANAC_BODY_ID_SIRIUS]          = "Sirius",
    [ALMANAC_BODY_ID_SPICA]           = "Spica",
    [ALMANAC_BODY_ID_SUHAIL]          = "Suhail",
    [ALMANAC_BODY_ID_VEGA]            = "Vega",
    [ALMANAC_BODY_ID_ZUBENELGENUBI]   = "Zubenelgenubi",
    [ALMANAC_BODY_ID_SIGMA_OCTANTIS]  = "Sigma Octantis"
};

static bool almanac_body_code_equals(const char *left, const char *right)
{
    unsigned char lc;
    unsigned char rc;

    if (!left || !right)
        return false;
    while (*left && *right) {
        lc = (unsigned char)*left++;
        rc = (unsigned char)*right++;
        if (lc == '-' || lc == ' ')
            lc = '_';
        if (rc == '-' || rc == ' ')
            rc = '_';
        if (toupper(lc) != toupper(rc))
            return false;
    }
    return *left == '\0' && *right == '\0';
}

static unsigned int almanac_body_code_hash(const char *body_code)
{
    const char *p;
    unsigned int hash = 0u;

    if (!body_code)
        return 0u;
    for (p = body_code; *p; ++p)
        ++hash;
    for (p = body_code; *p; ++p) {
        unsigned char ch = (unsigned char)*p;

        if (ch == '-' || ch == ' ')
            ch = '_';
        hash = hash * 71u + (unsigned int)toupper(ch);
    }
    return hash;
}

/* Parse a body code into its stable enum identifier. */
almanac_body_id_t almanac_body_id_from_code(const char *body_code)
{
    static const almanac_body_id_t body_ids_by_hash[275] = {
    [6]   = ALMANAC_BODY_ID_SCHEDAR,
    [7]   = ALMANAC_BODY_ID_RASALHAGUE,
    [8]   = ALMANAC_BODY_ID_HAMAL,
    [10]  = ALMANAC_BODY_ID_DIPHDA,
    [13]  = ALMANAC_BODY_ID_MENKENT,
    [15]  = ALMANAC_BODY_ID_GIENAH,
    [18]  = ALMANAC_BODY_ID_BETELGEUSE,
    [20]  = ALMANAC_BODY_ID_HADAR,
    [22]  = ALMANAC_BODY_ID_MIRFAK,
    [25]  = ALMANAC_BODY_ID_ACRUX,
    [29]  = ALMANAC_BODY_ID_VENUS,
    [31]  = ALMANAC_BODY_ID_SIGMA_OCTANTIS,
    [44]  = ALMANAC_BODY_ID_RIGEL,
    [49]  = ALMANAC_BODY_ID_SUN,
    [57]  = ALMANAC_BODY_ID_POLLUX,
    [61]  = ALMANAC_BODY_ID_PEACOCK,
    [65]  = ALMANAC_BODY_ID_CANOPUS,
    [66]  = ALMANAC_BODY_ID_SUHAIL,
    [74]  = ALMANAC_BODY_ID_ALIOTH,
    [75]  = ALMANAC_BODY_ID_ALTAIR,
    [79]  = ALMANAC_BODY_ID_ACHERNAR,
    [84]  = ALMANAC_BODY_ID_ALNAIR,
    [87]  = ALMANAC_BODY_ID_ATRIA,
    [89]  = ALMANAC_BODY_ID_RIGIL_KENTAURUS,
    [90]  = ALMANAC_BODY_ID_REGULUS,
    [91]  = ALMANAC_BODY_ID_MENKAR,
    [92]  = ALMANAC_BODY_ID_ELNATH,
    [97]  = ALMANAC_BODY_ID_MOON,
    [98]  = ALMANAC_BODY_ID_ANTARES,
    [105] = ALMANAC_BODY_ID_VEGA,
    [106] = ALMANAC_BODY_ID_NEPTUNE,
    [115] = ALMANAC_BODY_ID_ALDEBARAN,
    [123] = ALMANAC_BODY_ID_JUPITER,
    [133] = ALMANAC_BODY_ID_ALNILAM,
    [134] = ALMANAC_BODY_ID_ENIF,
    [136] = ALMANAC_BODY_ID_MIAPLACIDUS,
    [137] = ALMANAC_BODY_ID_NUNKI,
    [139] = ALMANAC_BODY_ID_DENEBOLA,
    [141] = ALMANAC_BODY_ID_MARS,
    [142] = ALMANAC_BODY_ID_ELTANIN,
    [148] = ALMANAC_BODY_ID_FOMALHAUT,
    [154] = ALMANAC_BODY_ID_ALPHERATZ,
    [166] = ALMANAC_BODY_ID_SIRIUS,
    [167] = ALMANAC_BODY_ID_POLARIS,
    [168] = ALMANAC_BODY_ID_DUBHE,
    [175] = ALMANAC_BODY_ID_BELLATRIX,
    [183] = ALMANAC_BODY_ID_SHAULA,
    [190] = ALMANAC_BODY_ID_SABIK,
    [192] = ALMANAC_BODY_ID_ADHARA,
    [195] = ALMANAC_BODY_ID_GACRUX,
    [202] = ALMANAC_BODY_ID_ARCTURUS,
    [207] = ALMANAC_BODY_ID_ZUBENELGENUBI,
    [211] = ALMANAC_BODY_ID_ACAMAR,
    [212] = ALMANAC_BODY_ID_ALKAID,
    [218] = ALMANAC_BODY_ID_KAUS_AUSTRALIS,
    [220] = ALMANAC_BODY_ID_MARKAB,
    [221] = ALMANAC_BODY_ID_CAPELLA,
    [231] = ALMANAC_BODY_ID_SPICA,
    [235] = ALMANAC_BODY_ID_KOCHAB,
    [239] = ALMANAC_BODY_ID_SATURN,
    [243] = ALMANAC_BODY_ID_DENEB,
    [244] = ALMANAC_BODY_ID_MERCURY,
    [247] = ALMANAC_BODY_ID_PROCYON,
    [256] = ALMANAC_BODY_ID_ALPHARD,
    [258] = ALMANAC_BODY_ID_AVIOR,
    [259] = ALMANAC_BODY_ID_ALPHECCA,
    [266] = ALMANAC_BODY_ID_ANKAA,
    [269] = ALMANAC_BODY_ID_URANUS
    };
    almanac_body_id_t body_id;
    unsigned int hash_index;

    if (!body_code)
        return ALMANAC_BODY_ID_UNKNOWN;
    hash_index = almanac_body_code_hash(body_code) % 275u;
    body_id = body_ids_by_hash[hash_index];
    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return ALMANAC_BODY_ID_UNKNOWN;
    if (!almanac_body_code_equals(body_code, ALMANAC_BODY_CODES[body_id]))
        return ALMANAC_BODY_ID_UNKNOWN;
    return body_id;
}

/* Return the canonical body code for an enum identifier. */
const char *almanac_body_code(almanac_body_id_t body_id)
{
    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return NULL;
    return ALMANAC_BODY_CODES[body_id];
}

/* Return a display label for an enum identifier. */
const char *almanac_body_display_name(almanac_body_id_t body_id)
{
    if (body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT)
        return NULL;
    return ALMANAC_BODY_DISPLAY_NAMES[body_id];
}

/* Free an almanac entry object allocated by an entry constructor. */
void almanac_entry_dealloc(almanac_entry_t *entry)
{
    free(entry);
}

static double almanac_entry_value_or_nan(const almanac_entry_t *entry, double value)
{
    return entry ? value : NAN;
}

/* Return the body id stored in an almanac entry. */
almanac_body_id_t almanac_entry_body_id(const almanac_entry_t *entry)
{
    return entry ? entry->body_id : ALMANAC_BODY_ID_UNKNOWN;
}

/* Return the body kind stored in an almanac entry. */
almanac_body_kind_t almanac_entry_body_kind(const almanac_entry_t *entry)
{
    return entry ? entry->body_kind : ALMANAC_BODY_STAR;
}

/* Return the civil Julian date used to compute the entry. */
double almanac_entry_moment_jd(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->moment_jd : NAN);
}

/* Return the entry's apparent Greenwich Hour Angle of Aries in degrees. */
double almanac_entry_gha_aries_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->gha_aries_degrees : NAN);
}

/* Return the entry's sidereal hour angle in degrees. */
double almanac_entry_sha_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->sha_degrees : NAN);
}

/* Return the entry's declination in degrees. */
double almanac_entry_declination_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->declination_degrees : NAN);
}

/* Return the entry's right ascension in hours. */
double almanac_entry_right_ascension_hours(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->right_ascension_hours : NAN);
}

/* Return the entry's geocentric distance in astronomical units. */
double almanac_entry_geocentric_distance_au(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->geocentric_distance_au : NAN);
}

/* Return the entry's heliocentric distance in astronomical units. */
double almanac_entry_heliocentric_distance_au(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->heliocentric_distance_au : NAN);
}

/* Return the entry's phase angle in degrees. */
double almanac_entry_phase_angle_degrees(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->phase_angle_degrees : NAN);
}

/* Return the entry's visual magnitude. */
double almanac_entry_visual_magnitude(const almanac_entry_t *entry)
{
    return almanac_entry_value_or_nan(entry, entry ? entry->visual_magnitude : NAN);
}

/* Derive the geographical position for one computed body entry. */
bool almanac_body_geographical_position(const almanac_entry_t *body, almanac_geographical_position_t *out)
{
    double gha_body_degrees;

    if (!body || !out)
        return false;

    gha_body_degrees = almanac_normalize_degrees(body->gha_aries_degrees + body->sha_degrees);
    if (!isfinite(body->declination_degrees) || !isfinite(gha_body_degrees))
        return false;

    out->latitude_degrees = body->declination_degrees;
    out->longitude_degrees = almanac_normalize_degrees_signed(-gha_body_degrees);
    return true;
}

static double almanac_phase_angle_degrees(const cartesian3_t *earth_to_object, const cartesian3_t *earth_to_sun)
{
    cartesian3_t object_to_earth;
    cartesian3_t object_to_sun;
    double len_earth;
    double len_sun;
    double cosine_angle;

    if (!earth_to_object || !earth_to_sun)
        return NAN;
    object_to_earth = cartesian_negate(earth_to_object);
    object_to_sun = cartesian_subtract(earth_to_sun, earth_to_object);
    len_earth = cartesian_length(&object_to_earth);
    len_sun = cartesian_length(&object_to_sun);
    if (len_earth <= 0.0 || len_sun <= 0.0)
        return NAN;
    cosine_angle = cartesian_dot(&object_to_earth, &object_to_sun) / (len_earth * len_sun);
    if (cosine_angle > 1.0)
        cosine_angle = 1.0;
    if (cosine_angle < -1.0)
        cosine_angle = -1.0;
    return almanac_radians_to_degrees(acos(cosine_angle));
}

static bool almanac_visual_magnitude(const almanac_model_row_t *model, const almanac_state_t *state,
    const almanac_state_t *sun_state, double *magnitude, double *phase_angle_degrees, double *heliocentric_distance_au)
{
    double delta;
    double phase_angle;
    cartesian3_t sun_to_body;
    double r;

    if (!model || !magnitude || !phase_angle_degrees || !heliocentric_distance_au)
        return false;

    *magnitude = NAN;
    *phase_angle_degrees = NAN;
    *heliocentric_distance_au = NAN;

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_NONE)
        return true;
    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_CATALOGUED) {
        *magnitude = model->magnitude_constant;
        return true;
    }
    if (!state || !state->has_geocentric_vector)
        return true;

    delta = cartesian_length(&state->geocentric_equatorial_au);
    if (delta <= 0.0)
        return true;

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_SUN_DISTANCE) {
        *magnitude = model->magnitude_constant + 5.0 * log10(delta);
        *heliocentric_distance_au = 0.0;
        return true;
    }

    if (!sun_state || !sun_state->has_geocentric_vector)
        return true;

    phase_angle = almanac_phase_angle_degrees(&state->geocentric_equatorial_au, &sun_state->geocentric_equatorial_au);
    sun_to_body = cartesian_subtract(&state->geocentric_equatorial_au, &sun_state->geocentric_equatorial_au);
    r = cartesian_length(&sun_to_body);
    *phase_angle_degrees = phase_angle;
    *heliocentric_distance_au = r;

    if (!(phase_angle == phase_angle) || r <= 0.0)
        return true;

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_PLANETARY_PHASE) {
        *magnitude = model->magnitude_constant + 5.0 * log10(r * delta) + model->magnitude_linear * phase_angle +
                     model->magnitude_quadratic * phase_angle * phase_angle +
                     model->magnitude_cubic * phase_angle * phase_angle * phase_angle +
                     model->magnitude_quartic * phase_angle * phase_angle * phase_angle * phase_angle;
        return true;
    }

    if (model->brightness_model == ALMANAC_BRIGHTNESS_MODEL_LUNAR_PHASE) {
        *magnitude = model->magnitude_constant + model->magnitude_linear * phase_angle +
                     model->magnitude_quadratic * phase_angle * phase_angle +
                     model->magnitude_cubic * phase_angle * phase_angle * phase_angle +
                     model->magnitude_quartic * phase_angle * phase_angle * phase_angle * phase_angle +
                     5.0 * log10(delta / (384400.0 / 149597870.7));
        return true;
    }

    return true;
}

static bool almanac_compute_entry(almanac_t *almanac, const almanac_model_row_t *model, const datetime_t *moment,
    almanac_entry_t *out)
{
    double civil_jd;
    almanac_state_t state;
    almanac_state_t sun_state;

    if (!almanac || !model || !moment || !out) {
        almanac_set_error(almanac, "invalid almanac computation request");
        return false;
    }
    civil_jd = datetime_jd(moment);
    if (civil_jd == DBL_MAX) {
        almanac_set_error(almanac, "failed to derive Julian dates for almanac computation");
        return false;
    }
    memset(&sun_state, 0, sizeof(sun_state));
    if (!almanac_compute_state_for_model(almanac, model, moment, &state))
        return false;

    memset(out, 0, sizeof(*out));
    out->body_id = model->body_id;
    out->moment_jd = civil_jd;
    out->body_kind = model->body_kind;
    out->gha_aries_degrees = almanac_apparent_gha_aries_for_jd(almanac, civil_jd);
    if (out->gha_aries_degrees == DBL_MAX) {
        almanac_set_error(almanac, "failed to compute apparent GHA of Aries");
        return false;
    }
    out->right_ascension_hours = state.right_ascension_hours;
    out->sha_degrees = almanac_normalize_degrees(360.0 - (state.right_ascension_hours * 15.0));
    out->declination_degrees = state.declination_degrees;
    out->geocentric_distance_au = state.has_geocentric_vector ? cartesian_length(&state.geocentric_equatorial_au) : NAN;
    out->heliocentric_distance_au = NAN;
    out->phase_angle_degrees = NAN;
    out->visual_magnitude = NAN;

    if (model->brightness_model != ALMANAC_BRIGHTNESS_MODEL_CATALOGUED &&
        model->brightness_model != ALMANAC_BRIGHTNESS_MODEL_NONE && model->body_id != ALMANAC_BODY_ID_SUN) {
        almanac_model_row_t sun_model;

        if (!almanac_fetch_model(almanac, ALMANAC_BODY_ID_SUN, &sun_model))
            return false;
        if (!almanac_compute_state_for_model(almanac, &sun_model, moment, &sun_state))
            return false;
    }

    if (!almanac_visual_magnitude(model, &state, model->body_id == ALMANAC_BODY_ID_SUN ? NULL : &sun_state,
                                  &out->visual_magnitude, &out->phase_angle_degrees, &out->heliocentric_distance_au)) {
        almanac_set_error(almanac, "failed to compute visual magnitude");
        return false;
    }
    if (model->body_id == ALMANAC_BODY_ID_MOON && out->phase_angle_degrees == out->phase_angle_degrees &&
        out->phase_angle_degrees >= 175.0) {
        out->visual_magnitude = NAN;
    }
    return true;
}

static bool almanac_entry_fill_body(almanac_t *almanac, almanac_body_id_t body_id, const datetime_t *moment,
    almanac_entry_t *out)
{
    almanac_model_row_t model;

    if (!almanac || !moment || !out || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT) {
        almanac_set_error(almanac, "invalid almanac entry request");
        return false;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return false;
    }
    if (!almanac_fetch_model(almanac, body_id, &model))
        return false;
    return almanac_compute_entry(almanac, &model, moment, out);
}

/* Create a populated almanac entry for one catalogued body by enum id. */
almanac_entry_t *almanac_new_body_entry(almanac_t *almanac, almanac_body_id_t body_id, const datetime_t *moment)
{
    almanac_entry_t *entry;

    entry = calloc(1u, sizeof(*entry));
    if (!entry) {
        almanac_set_error(almanac, "failed to allocate almanac entry");
        return NULL;
    }
    if (!almanac_entry_fill_body(almanac, body_id, moment, entry)) {
        free(entry);
        return NULL;
    }
    return entry;
}

/* Create a populated almanac entry for one catalogued body by legacy code. */
almanac_entry_t *almanac_new_entry(almanac_t *almanac, const char *body_code, const datetime_t *moment)
{
    almanac_body_id_t body_id = almanac_body_id_from_code(body_code);

    if (body_id == ALMANAC_BODY_ID_UNKNOWN) {
        almanac_set_error(almanac, "requested almanac body was not found");
        return NULL;
    }
    return almanac_new_body_entry(almanac, body_id, moment);
}

bool almanac_entry_fill_at_jd(almanac_t *almanac, almanac_body_id_t body_id, double jd, almanac_entry_t *out_entry)
{
    datetime_t *moment;
    bool ok;

    if (!almanac || body_id <= ALMANAC_BODY_ID_UNKNOWN || body_id >= ALMANAC_BODY_ID_COUNT || !out_entry) {
        almanac_set_error(almanac, "invalid almanac entry-at-jd request");
        return false;
    }
    moment = datetime_alloc();
    if (!moment) {
        almanac_set_error(almanac, "failed to allocate datetime for almanac entry");
        return false;
    }
    if (!datetime_init_jd(moment, jd)) {
        datetime_dealloc(moment);
        almanac_set_error(almanac, "failed to initialise datetime for almanac entry");
        return false;
    }
    ok = almanac_entry_fill_body(almanac, body_id, moment, out_entry);
    datetime_dealloc(moment);
    return ok;
}

/* Compute SHA and declination for every enabled catalogued body. */
array_t *almanac_snapshot(almanac_t *almanac, const datetime_t *moment)
{
    static const char *sql = "select b.body_id "
                             "from almanac_body as b "
                             "join almanac_body_enabled as enabled on enabled.body_id = b.body_id "
                             "join almanac_body_sort_order as sort on sort.body_id = b.body_id "
                             "where enabled.enabled = 'Y' "
                             "order by sort.sort_order asc, b.body_id asc";
    sqlite_stmt_t *stmt = NULL;
    array_t *entries = NULL;
    sqlite_step_result_t rc;

    if (!almanac || !moment) {
        almanac_set_error(almanac, "invalid almanac snapshot request");
        return NULL;
    }
    if (!almanac->db) {
        almanac_set_error(almanac, "almanac database is not open");
        return NULL;
    }

    entries = array_create(sizeof(almanac_entry_t), NULL, NULL);
    if (!entries) {
        almanac_set_error(almanac, "failed to allocate almanac snapshot array");
        return NULL;
    }

    stmt = sqlite_stmt_prepare(almanac->db, sql);
    if (!stmt) {
        array_destroy(entries);
        almanac_set_sqlite_error(almanac);
        return NULL;
    }

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        almanac_body_id_t body_id = (almanac_body_id_t)sqlite_stmt_column_int(stmt, 0);
        almanac_entry_t entry;

        if (!almanac_entry_fill_body(almanac, body_id, moment, &entry) || !array_add(entries, &entry)) {
            sqlite_stmt_finalize(stmt);
            array_destroy(entries);
            if (!string_length(almanac->error))
                almanac_set_error(almanac, "failed to build almanac snapshot");
            return NULL;
        }
    }

    sqlite_stmt_finalize(stmt);
    if (rc != SQLITE_STEP_DONE) {
        array_destroy(entries);
        almanac_set_sqlite_error(almanac);
        return NULL;
    }

    return entries;
}
