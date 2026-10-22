/**
 * @file lab_serial.c
 * @brief Serial execution of complete singleton-service operations without hidden global state.
 *
 * The native implementation blocks contenders on an embedded pthread mutex rather than spinning.
 * The browser implementation relies on the existing single-owner, non-shared WASM memory model and
 * uses the bridge's re-entry guard. Its scoped DOM bridge is also single-owner; worker requests must be marshalled
 * to that owner rather than sharing a service or bridge handles between workers.
 */
#include "lab_serial.h"

/* Dispatch synchronously so stack-owned request values and browser handles never escape their scope. */
bool lab_serial_dispatch(lab_serial_t *serial, lab_serial_operation_t operation, void *request)
{
    if (!serial || !operation) return false;
#ifdef __wasm__
    /* The bridge owns serial execution and exception unwinding; do not leave a C busy flag on host throws. */
    operation(request);
#else
    if (pthread_mutex_lock(&serial->mutex)) return false;
    operation(request);
    pthread_mutex_unlock(&serial->mutex);
#endif
    return true;
}
