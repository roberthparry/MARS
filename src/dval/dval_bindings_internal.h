#ifndef DVAL_BINDINGS_INTERNAL_H
#define DVAL_BINDINGS_INTERNAL_H

#include <stddef.h>

#include "dval_internal.h"

dv_binding_expr_t *dv_binding_expr_parse_region(const char *start,
                                                const char *end,
                                                char *errmsg,
                                                size_t errmsg_n);
dval_t *dv_binding_expr_eval_dval(const dv_binding_expr_t *expr);
char *dv_binding_expr_to_string(const dv_binding_expr_t *expr);
char *dv_binding_expr_to_tex(const dv_binding_expr_t *expr);

#endif
