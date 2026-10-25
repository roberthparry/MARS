/**
 * @file lab_history.h
 * @brief Bounded, browser-separated worksheet history persistence.
 *
 * Native request handlers store versioned Protobuf snapshots in a private plaintext
 * SQLite database beside the configured state file. Optimistic revisions prevent
 * stale tabs from replacing newer saves; client identifiers are not credentials.
 */
#ifndef MARS_LAB_HISTORY_H
#define MARS_LAB_HISTORY_H

#include "json-t.h"

/**
 * @brief Load or conditionally save a browser's history through a validated request object.
 * @param request Borrowed client/session identifiers, action and optional mode, epoch, revision and history.
 * @param status Receives HTTP status: 200, 400, 409, 413 or 503.
 * @return Owned response released with json_free, or NULL on allocation failure.
 * @details Each mode accepts 64 entries in either direction, 256 KiB per snapshot and
 * 1 MiB encoded history. Global payloads are limited to 16 MiB and 448 mode records;
 * least-recently-updated records are evicted first. The database is limited to 32 MiB,
 * with a bounded rollback journal (roughly another 32 MiB plus headers), never an unbounded WAL.
 */
json_t *lab_history_request(const json_t *request, unsigned *status);

#endif
