/**
 * @file array_mutex.h
 * @brief Private recursive locking policy for native and owner-thread WebAssembly arrays.
 *
 * Native arrays retain POSIX recursive mutexes. Non-shared WebAssembly instances have
 * one executing owner, so require no pthread ABI. Shared-memory WebAssembly is rejected
 * rather than silently disabling synchronisation. Only array.c consumes this header;
 * applications use array-t.h.
 */
#ifndef MARS_ARRAY_INTERNAL_ACCESS
#if !defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0
#error "Private header requires MARS_ARRAY_INTERNAL_ACCESS."
#endif
#endif
#ifndef MARS_ARRAY_MUTEX_H
#define MARS_ARRAY_MUTEX_H

#if defined(__wasm__)
#if defined(__wasm_atomics__)
#error "Array owner-thread locking does not support shared-memory WebAssembly."
#endif
typedef unsigned char array_mutex_t;
/** @brief Initialise an owner-thread gate without a pthread dependency. */
static inline void array_mutex_init(array_mutex_t *mutex) { *mutex = 0; }

/** @brief Release an owner-thread gate, which owns no external resources. */
static inline void array_mutex_destroy(array_mutex_t *mutex) { (void)mutex; }

/** @brief Enter an operation on this non-shared instance's single executing thread. */
static inline void array_mutex_lock(array_mutex_t *mutex) { (void)mutex; }

/** @brief Leave an operation on this non-shared instance's single executing thread. */
static inline void array_mutex_unlock(array_mutex_t *mutex) { (void)mutex; }
#else
#include <pthread.h>

typedef pthread_mutex_t array_mutex_t;
/** @brief Initialise the recursive native mutex used by nested array operations. */
static void array_mutex_init(array_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

/** @brief Destroy the native mutex after its final reference is released. */
static inline void array_mutex_destroy(array_mutex_t *mutex) { pthread_mutex_destroy(mutex); }

/** @brief Acquire the native recursive mutex. */
static inline void array_mutex_lock(array_mutex_t *mutex) { pthread_mutex_lock(mutex); }

/** @brief Release one native recursive acquisition. */
static inline void array_mutex_unlock(array_mutex_t *mutex) { pthread_mutex_unlock(mutex); }
#endif

#endif
