/**
 * @file lab_serial.h
 * @brief Synchronous serial dispatch for native browser-owned singleton services.
 *
 * Native builds use a mutex, covering each entire operation and its state transitions. Browser WASM
 * instances use non-shared memory and the bridge's single owning JavaScript thread and re-entry guard.
 * This is not a worker queue: callbacks must not redispatch to the same service, retain request pointers,
 * or invoke browser capabilities from a native background thread. No process-wide state is owned here.
 */
#ifndef LAB_SERIAL_H
#define LAB_SERIAL_H

#include <stdbool.h>
#if defined(__wasm__) && defined(__wasm_atomics__)
#error "Browser singleton services require owner-thread dispatch, not a shared-memory WASM build"
#endif
#ifndef __wasm__
#include <pthread.h>
#endif

/** @brief Embedded serial-dispatch gate, initialised once with LAB_SERIAL_INITIALIZER. */
typedef struct _lab_serial_t {
#ifdef __wasm__
    unsigned char reserved;
#else
    pthread_mutex_t mutex;
#endif
} lab_serial_t;

#ifdef __wasm__
#define LAB_SERIAL_INITIALIZER {0}
#else
#define LAB_SERIAL_INITIALIZER {PTHREAD_MUTEX_INITIALIZER}
#endif

/** @brief Callback executed synchronously under its service's exclusive dispatch gate. */
typedef void (*lab_serial_operation_t)(void *request);

/** @brief Execute one complete operation exclusively; reject invalid arguments or a failed native lock. */
bool lab_serial_dispatch(lab_serial_t *serial, lab_serial_operation_t operation, void *request);

#endif
