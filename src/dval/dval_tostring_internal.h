#ifndef DVAL_TOSTRING_INTERNAL_H
#define DVAL_TOSTRING_INTERNAL_H

#include <stddef.h>

#include "qcomplex.h"
#include "dval_internal.h"

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} sbuf_t;

void *dv_tostring_xmalloc(size_t n);
char *dv_tostring_xstrdup(const char *s);

void sbuf_init(sbuf_t *b);
void sbuf_free(sbuf_t *b);
void sbuf_reserve(sbuf_t *b, size_t extra);
void sbuf_putc(sbuf_t *b, char c);
void sbuf_puts(sbuf_t *b, const char *s);

int dv_tostring_utf8_decode(const char *s, unsigned int *out);
void qf_to_string_simple(qcomplex_t v, char *buf, size_t n);

int dv_tostring_is_negative_const(const dval_t *f);
int dv_tostring_is_var_pow_d(const dval_t *f);
int dv_tostring_is_unicode_letter(unsigned int c);
int dv_tostring_is_simple_name(const char *name);
int dv_tostring_is_safe_func_name(const char *name);
void emit_name(sbuf_t *b, const char *name);
void emit_name_func(sbuf_t *b, const char *name);

#endif
