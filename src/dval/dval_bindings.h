#ifndef DVAL_BINDINGS_H
#define DVAL_BINDINGS_H

#include <stddef.h>

#include "dval_internal.h"

dv_binding_expr_t *dv_binding_expr_parse_region(const char *start,
                                                const char *end,
                                                char *errmsg,
                                                size_t errmsg_n);
dval_t *dv_binding_expr_eval_dval(const dv_binding_expr_t *expr);
bool dv_binding_expr_number_value(const dv_binding_expr_t *expr, number_t *out);
bool dv_binding_expr_struct_eq(const dv_binding_expr_t *left,
                               const dv_binding_expr_t *right);
bool dv_binding_expr_split_leading_number(const dv_binding_expr_t *expr,
                                          number_t *coeff_out,
                                          dv_binding_expr_t **rest_out);
dv_binding_expr_t *dv_binding_expr_simplify(dv_binding_expr_t *expr);
char *dv_binding_expr_to_string(const dv_binding_expr_t *expr);
char *dv_binding_expr_to_function_string(const dv_binding_expr_t *expr);
char *dv_binding_expr_to_tex(const dv_binding_expr_t *expr);

#endif
