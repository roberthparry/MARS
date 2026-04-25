#ifndef MATRIX_INTERNAL_H
#define MATRIX_INTERNAL_H

#include <stddef.h>

#include "matrix.h"
#include "qfloat.h"
#include "qcomplex.h"

/* ============================================================
   Element kinds
   ============================================================ */

typedef enum {
    ELEM_DOUBLE = 0,
    ELEM_QFLOAT = 1,
    ELEM_QCOMPLEX = 2,
    ELEM_MAX
} elem_kind;

/* ============================================================
   Element vtable
   ============================================================ */

struct elem_vtable {
    size_t size;
    elem_kind kind;

    /* arithmetic */
    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    void (*mul)(void *out, const void *a, const void *b);
    void (*inv)(void *out, const void *a);

    /* scalar queries — return/accept double for generic algorithm use */
    double (*abs2)(const void *a);          /* |a|² as double          */
    double (*to_real)(const void *a);       /* Re(a) as double          */
    void   (*from_real)(void *out, double x); /* construct from real     */
    void   (*conj_elem)(void *out, const void *a); /* complex conjugate  */

    /* qfloat-precision scalar queries for high-precision algorithms */
    void (*to_qf)(qfloat_t *out, const void *a);    /* Re(a) as qfloat  */
    void (*abs_qf)(qfloat_t *out, const void *a);   /* |a| as qfloat    */
    void (*from_qf)(void *out, const qfloat_t *x);  /* construct pure-real from qfloat */

    /* qcomplex cast — used by the general QR eigensolver */
    void (*to_qc)(qcomplex_t *out, const void *a);   /* cast element to qcomplex */
    void (*from_qc)(void *out, const qcomplex_t *z); /* cast qcomplex back (drops im for real types) */

    /* constants */
    const void *zero;
    const void *one;

    /* printing */
    void (*print)(const void *val, char *buf, size_t buflen);

    /* matrix constructors */
    struct matrix_t *(*create_matrix)(size_t rows, size_t cols);
    struct matrix_t *(*create_identity)(size_t n);
};

/* ============================================================
   Storage vtable
   ============================================================ */

struct store_vtable {
    void (*alloc)(struct matrix_t *A);
    void (*free)(struct matrix_t *A);

    void (*get)(const struct matrix_t *A, size_t i, size_t j, void *out);
    void (*set)(struct matrix_t *A, size_t i, size_t j, const void *val);

    void (*materialise)(struct matrix_t *A);
};

/* ============================================================
   Matrix object (opaque in matrix.h)
   ============================================================ */

struct matrix_t {
    size_t rows;
    size_t cols;

    const struct elem_vtable  *elem;
    const struct store_vtable *store;

    void **data;   /* row pointers for dense; NULL for identity */
};

/* ============================================================
   Element vtable instances (defined in matrix_core.c)
   ============================================================ */

extern const struct elem_vtable double_elem;
extern const struct elem_vtable qfloat_elem;
extern const struct elem_vtable qcomplex_elem;

/* ============================================================
   Convenience accessor
   ============================================================ */

static inline const struct elem_vtable *elem_of(const struct matrix_t *A) {
    return A->elem;
}

#endif /* MATRIX_INTERNAL_H */
