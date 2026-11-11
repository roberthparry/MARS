/**
 * @file lab_process_session.h
 * @brief Private transport and lifecycle boundary for persistent native calculations.
 *
 * Only the process adapter, its single-threaded supervisor and authorised lifecycle tests use this header.
 * Protobuf requests and anonymous descriptors stay local to these processes; mathematical entry points are
 * reached through lab_worker.h. Ordinary application callers use lab_process.h.
 */
#ifndef MARS_LAB_PROCESS_SESSION_H
#define MARS_LAB_PROCESS_SESSION_H
#if !defined(MARS_LAB_PROCESS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private process sessions require MARS_LAB_PROCESS_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include "string-t.h"

/** @brief Local protocol operations; the job operation alone transfers four owned descriptor copies. */
enum {
    LAB_SESSION_JOB = 1, /**< Dispatch the job described by four accompanying descriptors. */
    LAB_SESSION_CANCEL, /**< Stop the active job, carrying the parent's errno-style reason. */
    LAB_SESSION_STARTED, /**< Report the selected calculation PID before dispatch. */
    LAB_SESSION_FINISHED, /**< Report exit status and resident high-water bytes. */
    LAB_SESSION_READY, /**< Report initial process readiness or startup error. */
    LAB_SESSION_FDS = 4, /**< Request, standard input, merged output and working-directory descriptor count. */
    LAB_SESSION_MAX_REQUEST = 8 * 1024 * 1024 /**< Maximum encoded request or input bytes. */
};

/** @brief Fixed local control record; no addresses, mathematical values or credentials enter it. */
typedef struct {
    uint32_t kind; /**< One LAB_SESSION operation. */
    uint32_t status; /**< Exit status or startup errno, interpreted according to kind. */
    int64_t pid; /**< Calculation process identity; zero when unavailable. */
    uint64_t resident_bytes; /**< Worker resident high-water mark after a completed calculation. */
} lab_proc_session_message_t;

/**
 * @brief Send one atomic local record, optionally copying four descriptors to the peer.
 * @param socket Owned connected local sequenced-packet descriptor; remains open.
 * @param message Borrowed record, valid throughout the call.
 * @param descriptors Borrowed four-descriptor array for a job, otherwise NULL; ownership is not transferred.
 * @return True on a complete send; false with errno on transport failure. No pointers escape the call.
 */
bool lab_proc_session_send(int socket, const lab_proc_session_message_t *message, const int *descriptors);

/**
 * @brief Receive and validate a complete local control packet and its descriptor count.
 * @param socket Owned connected descriptor; remains open.
 * @param[out] message Required destination for the copied record.
 * @param[out] descriptors Required four-element destination, initialised to minus one. On success the caller
 * owns each received descriptor and must close it; malformed packets release all received copies internally.
 * @return One for a record, zero for disconnect, minus one with errno on malformed input or transport failure.
 */
int lab_proc_session_receive(int socket, lab_proc_session_message_t *message, int descriptors[LAB_SESSION_FDS]);

/**
 * @brief Close every non-negative descriptor in an owned job array, replacing entries with minus one.
 * @param[in,out] descriptors Four owned descriptor entries, or NULL.
 * @return No value; repeated calls are harmless and no ownership remains in closed entries.
 */
void lab_proc_session_close(int descriptors[LAB_SESSION_FDS]);

/**
 * @brief Run the single-threaded calculation supervisor until its parent disconnects.
 * @param channel Owned private channel, closed before returning.
 * @param memory_limit Maximum calculation address-space bytes; inherited limits may be stricter.
 * @param max_jobs Maximum completed jobs before replacing a healthy calculation process.
 * @return No value; reaps its calculation child on all ordinary exits. Called only immediately after fork.
 */
void lab_proc_session_supervise(int channel, size_t memory_limit, unsigned max_jobs);

/**
 * @brief Dispatch sequential requests in one cache-retaining calculation process.
 * @param channel Owned supervisor channel, closed before returning.
 * @param memory_limit Required address-space ceiling in bytes.
 * @return No value; owns and releases each request's descriptors, strings and numeric scope. Never starts threads.
 */
void lab_proc_session_worker(int channel, size_t memory_limit);

/**
 * @brief Submit one native calculation to this process's persistent service.
 * @param argv Borrowed NULL-terminated argument vector including a registered mode name.
 * @param cwd Borrowed working directory, or NULL to use the caller's current directory.
 * @param input Borrowed standard-input text, or NULL for empty input.
 * @param timeout_ms Complete request deadline; zero disables it.
 * @param max_output Maximum captured UTF-8 bytes, at most 64 MiB.
 * @param[out] output Owned result or partial output, released with string_free().
 * @param[out] exit_status Exit status, or minus one before a calculation begins.
 * @return True after ordinary completion, including non-zero mathematical exits; false sets errno for
 * cancellation, timeout, output overflow, transport or allocation failure. Calls in one process are sequential.
 */
bool lab_proc_persistent_run(const char *const argv[], const char *cwd, const string_t *input,
                              unsigned timeout_ms, size_t max_output, string_t **output, int *exit_status);

/**
 * @brief Observe the most recently selected calculation process identity without starting a worker.
 * @return Positive PID last reported by this service, otherwise minus one. Recycling may have retired that PID.
 * Intended for lifecycle regression tests;
 * no ownership transfers and the value is only a snapshot, not a synchronisation primitive.
 */
pid_t lab_proc_persistent_pid(void);

/**
 * @brief Poll the calling request's borrowed cancellation observers.
 * @return True when either installed observer requests cancellation; no ownership changes. Signal observers
 * and their arguments retain the lifetimes documented by lab_process.h and lab_process_request.h.
 */
bool lab_proc_cancelled(void);

/**
 * @brief Obtain a bounded polling interval within an existing monotonic deadline.
 * @param start Borrowed CLOCK_MONOTONIC start time.
 * @param timeout_ms Deadline allowance; zero disables the deadline.
 * @return Non-negative milliseconds to poll; minus one sets errno on timeout or clock failure.
 */
int lab_proc_interval(const struct timespec *start, unsigned timeout_ms);

/**
 * @brief Append complete UTF-8 records while retaining an incomplete trailing code point.
 * @param output Borrowed mutable destination, whose ownership remains with the caller.
 * @param buffer Mutable input storage, with room for the retained trailing bytes.
 * @param count Number of available bytes.
 * @param[out] pending Required count of trailing bytes retained at the front of buffer.
 * @return Zero on success, otherwise an errno value; existing complete output remains available on failure.
 */
int lab_proc_append(string_t *output, char *buffer, size_t count, size_t *pending);

#endif
