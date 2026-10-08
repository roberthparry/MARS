/**
 * @file cfg_locales_internal.h
 * @brief Private ownership and data operations for the native locale generator.
 *
 * Only locales implementation units consume this interface. The public command
 * and generator entry points are declared in cfg_locales.h. JSON containers own
 * copies; lookup results and sorted key arrays borrow their underlying strings.
 */
#ifndef MARS_CONFIG_LOCALES_INTERNAL_H
#define MARS_CONFIG_LOCALES_INTERNAL_H

#include "file.h"
#include "json.h"
#include "ustring.h"

/** Finite snapshot and expansion limits, checked before parsing or appending. */
enum {
    CFG_LOCALES_PATTERN_BYTES = 1024,
    CFG_LOCALES_AFFIX_BYTES = 256,
    CFG_LOCALES_PATTERN_PARTS = 128,
    CFG_LOCALES_OUTPUT_BYTES = 16 * 1024 * 1024,
    CFG_LOCALES_INPUT_BYTES = 32 * 1024 * 1024
};

/** Generator-owned source trees and derived indexed maps. */
typedef struct cfg_locales_state {
    json_t *raw;
    json_t *supp;
    json_t *records;
    json_t *selected;
    json_t *order;
    json_t *active;
    json_t *names;
    json_t *choices;
    json_t *tables[9];
} cfg_locales_state;

/** Look up a literal ASCII object key; return a borrowed value or NULL. */
const json_t *cfg_locales_get(const json_t *object, const char *key);

/** Look up a literal key and return its borrowed text, or NULL. */
const string_t *cfg_locales_text(const json_t *object, const char *key);

/** Compare a borrowed string with an ASCII literal. */
bool cfg_locales_equal(const string_t *text, const char *literal);

/** Set a copied text value in an object. */
bool cfg_locales_set(json_t *object, const string_t *key, const string_t *value);

/** Append a copied text value to an array; NULL becomes JSON null. */
bool cfg_locales_push(json_t *array, const string_t *text);

/** Append a non-negative integer to an array. */
bool cfg_locales_number(json_t *array, size_t value);

/** Return an owned array of borrowed keys sorted by exact ASCII identifier. */
const string_t **cfg_locales_keys(const json_t *object);

/** Read bounded exact UTF-8 through the file module. */
string_t *cfg_locales_read(const string_t *path);

/** Join a borrowed directory and ASCII relative suffix without normalising path bytes. */
string_t *cfg_locales_path(const string_t *directory, const char *suffix);

/** Load and validate pinned data; populate owned source trees. */
bool cfg_locales_load(cfg_locales_state *state, const string_t *directory);

/** Select territories, supported languages and active locales from the country source. */
bool cfg_locales_select(cfg_locales_state *state, const string_t *root);

/** Build the nine output tables, deduplicating names and pattern parts. */
bool cfg_locales_tables(cfg_locales_state *state);

/** Tokenise a display pattern exactly as Babel 2.17, returning owned part pairs. */
json_t *cfg_locales_pattern(const string_t *pattern, const string_t *era, const string_t *suffix);

/** Emit complete SQL from the generated tables, preserving all label bytes. */
string_t *cfg_locales_sql(const cfg_locales_state *state);

/** Append borrowed text bytes without normalisation. */
bool cfg_locales_append(string_t *out, const string_t *text);

/** Make an exact hexadecimal signature of a JSON array of text pairs. */
string_t *cfg_locales_signature(const json_t *pairs);

#endif
