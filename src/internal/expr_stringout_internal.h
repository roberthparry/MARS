/**
 * @file expr_stringout_internal.h
 * @brief Controlled cross-module access to shared native expression formatting.
 *
 * Equation rendering uses the expression formatter's buffers, names and output
 * state so its native and TeX representations follow the same rules. This façade
 * preserves that existing dependency; ordinary callers use expr_t.
 * It is private, requires explicit shared access and is not installed.
 */
#ifndef MARS_SHARED_EXPR_STRINGOUT_INTERNAL_H
#define MARS_SHARED_EXPR_STRINGOUT_INTERNAL_H

#if !defined(MARS_SHARED_EXPR_STRINGOUT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_EXPR_STRINGOUT_INTERNAL_ACCESS."
#endif

#define MARS_EXPR_INTERNAL_ACCESS
#include "expression/expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expression/expr_stringout_internal.h"

#endif
