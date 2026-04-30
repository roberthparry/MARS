#ifndef DVAL_REFCOUNT_H
#define DVAL_REFCOUNT_H

#include "dval.h"

/**
 * @brief Increment the reference count of a node.
 *
 * Internal MARS ownership helper. This is for implementation code such as
 * matrix and dval internals only; it is not part of the public API and should
 * not be used by tests or external callers. Safe to call with NULL.
 */
void dv_retain(dval_t *dv);

#endif
