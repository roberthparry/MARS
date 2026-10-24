/**
 * @file lab_globe_fetch.h
 * @brief Native request and response ownership for asynchronous shoreline HTTP delivery.
 *
 * The browser worker supplies only networking, cancellation and chunk copies into reserved WASM memory.
 * Native code encodes the request, bounds response storage, decodes Protobuf and atomically admits polygons.
 * Tokens protect against late completion; no pointer is retained by the host across an asynchronous boundary.
 */
#ifndef LAB_GLOBE_FETCH_H
#define LAB_GLOBE_FETCH_H

#include <stdbool.h>

/** @brief Start a bounded request, cancelling its predecessor; return a non-zero token or zero on failure. */
unsigned lab_globe_fetch_begin(double west, double south, double east, double north);

/** @brief Borrow the native Protobuf request for token until the next native call; NULL for a stale token. */
const unsigned char *lab_globe_fetch_request(unsigned token);

/** @brief Return the request byte length, or zero for a stale token. */
unsigned lab_globe_fetch_request_size(unsigned token);

/** @brief Reserve a response chunk in WASM memory; NULL on a stale token, allocation failure or exceeded budget. */
unsigned char *lab_globe_fetch_reserve(unsigned token, unsigned size);

/** @brief Commit a copied response chunk before awaiting the next one; false on invalid or stale input. */
bool lab_globe_fetch_commit(unsigned token, unsigned size);

/** @brief Validate a completed HTTP/Protobuf response and publish its polygons; old geometry survives any failure. */
bool lab_globe_fetch_finish(unsigned token, unsigned status);

/** @brief Cancel a matching transfer and release its response; stale tokens do nothing. */
void lab_globe_fetch_cancel(unsigned token);

#endif
