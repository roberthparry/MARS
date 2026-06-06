#ifndef EXPR_BINDINGS_H
#define EXPR_BINDINGS_H

#include <stddef.h>

#include "expr_internal.h"

expr_binding_expr_t *expr_binding_expr_parse_region(const char *start,
                                                const char *end,
                                                char *errmsg,
                                                size_t errmsg_n);
expr_t *expr_binding_expr_eval_expr(const expr_binding_expr_t *expr);
bool expr_binding_expr_number_value(const expr_binding_expr_t *expr, number_t *out);
bool expr_binding_expr_struct_eq(const expr_binding_expr_t *left,
                               const expr_binding_expr_t *right);
bool expr_binding_expr_split_leading_number(const expr_binding_expr_t *expr,
                                          number_t *coeff_out,
                                          expr_binding_expr_t **rest_out);
bool expr_binding_expr_needs_explicit_mul_separator(const expr_binding_expr_t *expr);
expr_binding_expr_t *expr_binding_expr_simplify(expr_binding_expr_t *expr);
char *expr_binding_expr_to_string(const expr_binding_expr_t *expr);
char *expr_binding_expr_to_function_string(const expr_binding_expr_t *expr);
char *expr_binding_expr_to_tex(const expr_binding_expr_t *expr);

#endif
