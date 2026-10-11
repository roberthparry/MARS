/**
 * @file lock_internal.h
 * @brief Private monitor, ownership and backend contracts for the lock class cluster.
 *
 * Shared only by lock lifecycle/wait code and specialised backend operations.
 * Backend callbacks run under the state monitor; they never sleep or call user
 * code. Other modules, including thread, use the public lock-t.h API exclusively.
 */
#ifndef MARS_LOCK_INTERNAL_H
#define MARS_LOCK_INTERNAL_H

#if !defined(MARS_LOCK_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LOCK_INTERNAL_ACCESS."
#endif

#include <pthread.h>
#include <stddef.h>

#include "lock-t.h"

typedef struct _lock_state_t lock_state_t;
typedef struct {
    bool (*ready)(const lock_state_t *state, bool read);
    void (*take)(lock_state_t *state, bool read);
    void (*drop)(lock_state_t *state, bool read, size_t count);
} lock_backend_t;

struct _lock_state_t {
    const lock_backend_t *backend;
    lock_kind_t kind;
    pthread_mutex_t gate;
    pthread_cond_t changed;
    size_t references;
    union {
        struct {
            pthread_t owner;
            size_t depth;
        } mutex;
        struct {
            pthread_t owner;
            size_t readers;
            bool writing;
        } rw;
        unsigned permits;
        bool signalled;
    } value;
};

struct _lock_t {
    lock_state_t *state;
    pthread_t owner;
    size_t depth;
    bool read;
};

/** @brief Direct-indexed backend table, addressed only after validating the public kind. */
extern const lock_backend_t lock_backends[LOCK_EVENT + 1];

#endif
