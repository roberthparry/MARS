/**
 * @file tba_stream.h
 * @brief Ephemeral authenticated Protobuf streams shared by prefork request workers.
 *
 * Browser Web Crypto wraps a fresh AES-256 key with RSA-OAEP/SHA-256. AES-GCM
 * authenticates each bounded chunk, its sequence, stream token and final marker.
 * Only ciphertext is retained between requests, in anonymous shared memory with
 * a two-minute absolute expiry. Completed plaintext and reply keys are caller
 * owned and transient. There is no file, database or logging interface here.
 * Public keys must be delivered through trusted HTTPS or local loopback.
 */
#ifndef TBA_STREAM_H
#define TBA_STREAM_H

#include "tba_support.h"

typedef struct tba_stream tba_stream_t;
typedef struct tba_stream_reply tba_stream_reply_t;

/** @brief Create ephemeral keys and four bounded stream slots before forking.
 * @return Owned service, or NULL; release with tba_stream_free. */
tba_stream_t *tba_stream_new(void);

/** @brief Release this process's service and mapping.
 * @param stream Owned service, or NULL. */
void tba_stream_free(tba_stream_t *stream);

/** @brief Reserve a stream slot and return its public key and random token.
 * @param stream Borrowed service.
 * @return Owned Protobuf-compatible object, including a structured error when full. */
json_t *tba_stream_begin(tba_stream_t *stream);

/** @brief Erase expired reservations during idle listener polling.
 * @param stream Borrowed service, or NULL. */
void tba_stream_expire(tba_stream_t *stream);

/** @brief Authenticate and append one encrypted chunk, or cancel a stream.
 * @param stream Borrowed service.
 * @param frame Borrowed envelope with slot, token, wrapped_key, sequence, final and ciphertext.
 * @param message Required output, set to an owned decoded message only on verified completion.
 * @param reply Required output, set to an owned response-encryption context on completion.
 * @return Owned acknowledgement/error. A failed stream cannot be resumed. */
json_t *tba_stream_receive(tba_stream_t *stream, const json_t *frame, json_t **message, tba_stream_reply_t **reply);

/** @brief Encrypt a completed calculation's Protobuf result.
 * @param reply Borrowed one-use response context.
 * @param result Borrowed result tree.
 * @return Owned ciphertext envelope, or NULL. Never returns plaintext on failure. */
json_t *tba_stream_seal(tba_stream_reply_t *reply, const json_t *result);

/** @brief Erase and release a response key.
 * @param reply Owned context, or NULL. */
void tba_stream_reply_free(tba_stream_reply_t *reply);
#endif
