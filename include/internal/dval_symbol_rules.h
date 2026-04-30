#ifndef DVAL_SYMBOL_RULES_H
#define DVAL_SYMBOL_RULES_H

#include "qcomplex.h"

/**
 * @file dval_symbol_rules.h
 * @brief Shared symbol-normalisation and default-inference rules for dval-style parsers.
 *
 * These helpers are used by both dval and matrix string parsing so that
 * canonical symbol spelling and implicit constant inference live in one place.
 */

char *dv_normalize_name(const char *name);
char *dv_normalize_binding_name(const char *name);
int dv_is_default_constant_name(const char *name);
int dv_get_default_constant_value(const char *name, qcomplex_t *value_out);
const char *dv_default_constant_canonical_name(const char *name);

#endif
