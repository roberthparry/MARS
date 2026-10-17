/**
 * @file tba_stream.c
 * @brief Authenticated, bounded, RAM-only transport for confidential observations.
 *
 * Four directly indexed slots retain ciphertext, never files or plaintext, across
 * prefork workers. A robust process-shared mutex protects reservations. Bad tags,
 * missing/repeated/reordered chunks and expired tokens discard the entire job.
 * Cryptographic primitives are OpenSSL EVP, not application implementations.
 */
#include <errno.h>
#include <math.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <pthread.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "tba_wire.h"
#include "tba_stream.h"

enum { TBA_STREAM_LIMIT = 2 * 1024 * 1024, TBA_STREAM_CHUNK = 32768, TBA_STREAM_SLOTS = 4,
       TBA_STREAM_FRAMES = TBA_STREAM_LIMIT / TBA_STREAM_CHUNK };

typedef struct {
    time_t expires;
    char token[65];
    unsigned count;
    size_t size;
    unsigned char wrapped[256];
    size_t lengths[TBA_STREAM_FRAMES];
    unsigned char data[TBA_STREAM_LIMIT + TBA_STREAM_FRAMES * 16];
} tba_stream_slot_t;

typedef struct {
    pthread_mutex_t mutex;
    tba_stream_slot_t slots[TBA_STREAM_SLOTS];
} tba_stream_pool_t;

struct tba_stream {
    EVP_PKEY *key;
    tba_stream_pool_t *pool;
};

struct tba_stream_reply {
    unsigned char key[32];
    char token[65];
    bool used;
};

static time_t tba_stream_now(void)
{
    struct timespec now;
    return clock_gettime(CLOCK_MONOTONIC, &now) == 0 ? now.tv_sec : 0;
}

static bool tba_stream_lock(tba_stream_t *stream)
{
    int status = pthread_mutex_lock(&stream->pool->mutex);
    if (status == EOWNERDEAD) {
        sodium_memzero(stream->pool->slots, sizeof stream->pool->slots);
        if (pthread_mutex_consistent(&stream->pool->mutex) == 0)
            return true;
        pthread_mutex_unlock(&stream->pool->mutex);
        return false;
    }
    return status == 0;
}

static json_t *tba_stream_ack(void)
{
    json_t *result = json_new_object();
    if (!result || !tba_json_flag(result, "ok", true)) {
        json_free(result);
        return NULL;
    }
    return result;
}

static bool tba_stream_binary(json_t *object, const char *name, const void *bytes, size_t size)
{
    size_t capacity = sodium_base64_ENCODED_LEN(size, sodium_base64_VARIANT_ORIGINAL);
    char *text = malloc(capacity);
    bool ok = text && sodium_bin2base64(text, capacity, bytes, size, sodium_base64_VARIANT_ORIGINAL) &&
              tba_json_string(object, name, text);
    free(text);
    return ok;
}

static bool tba_stream_decode(const json_t *object, const char *name, void *bytes, size_t capacity, size_t *length)
{
    const string_t *text = json_string_value(tba_json_get(object, name));
    return text && string_byte_length(text) <= sodium_base64_ENCODED_LEN(capacity, sodium_base64_VARIANT_ORIGINAL) &&
           sodium_base642bin(bytes, capacity, string_c_str(text), string_byte_length(text), NULL, length, NULL,
                             sodium_base64_VARIANT_ORIGINAL) == 0;
}

static bool tba_stream_number(const json_t *object, const char *name, unsigned maximum, unsigned *result)
{
    double value;
    const string_t *text = json_number_text(tba_json_get(object, name));
    if (!tba_text_number(text, &value) || value < 0 || value > maximum || floor(value) != value)
        return false;
    *result = (unsigned)value;
    return true;
}

static bool tba_stream_unwrap(tba_stream_t *stream, const unsigned char *wrapped, unsigned char *key)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(stream->key, NULL);
    unsigned char output[256];
    size_t length = sizeof output;
    bool ok = ctx && EVP_PKEY_decrypt_init(ctx) > 0 &&
              EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) > 0 &&
              EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) > 0 &&
              EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) > 0 &&
              EVP_PKEY_decrypt(ctx, output, &length, wrapped, 256) > 0 && length == 32;
    if (ok)
        memcpy(key, output, 32);
    sodium_memzero(output, sizeof output);
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

static bool tba_stream_crypt(const unsigned char *key, const char *token, unsigned sequence, bool final,
                             bool encrypt, const unsigned char *input, size_t length, unsigned char *output)
{
    /* Independent nonce spaces for client data and the one server response. */
    unsigned char iv[12] = {0};
    iv[0] = encrypt ? 1 : 0;
    for (unsigned i = 0; i < 4; ++i)
        iv[11 - i] = (unsigned char)(sequence >> (8 * i));
    string_t *aad = string_sprintf("MARS-TBA-1:%s:%u:%u:%u", token, encrypt ? 1 : 0, sequence, final ? 1 : 0);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int written = 0, tail = 0;
    size_t payload = encrypt ? length : length >= 16 ? length - 16 : 0;
    bool ok = aad && ctx && (encrypt || length >= 16) && payload <= TBA_STREAM_LIMIT &&
              EVP_CipherInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv, encrypt) > 0 &&
              EVP_CipherUpdate(ctx, NULL, &written, (const unsigned char *)string_c_str(aad),
                               (int)string_byte_length(aad)) > 0 &&
              EVP_CipherUpdate(ctx, output, &written, input, (int)payload) > 0;
    if (ok && !encrypt)
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void *)(input + payload)) > 0;
    ok = ok && EVP_CipherFinal_ex(ctx, output + written, &tail) > 0;
    if (ok && encrypt)
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, output + payload) > 0;
    if (!ok)
        sodium_memzero(output, payload);
    EVP_CIPHER_CTX_free(ctx);
    string_free(aad);
    return ok;
}

/* Allocate an ephemeral key and an anonymous shared ciphertext pool. */
tba_stream_t *tba_stream_new(void)
{
    if (sodium_init() < 0)
        return NULL;
    tba_stream_t *stream = calloc(1, sizeof *stream);
    if (!stream)
        return NULL;
    stream->key = EVP_RSA_gen(2048);
    stream->pool = mmap(NULL, sizeof *stream->pool, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    pthread_mutexattr_t attr;
    bool attrs = pthread_mutexattr_init(&attr) == 0;
    bool ok = stream->key && stream->pool != MAP_FAILED && attrs &&
              pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 &&
              pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) == 0 &&
              pthread_mutex_init(&stream->pool->mutex, &attr) == 0;
    if (attrs)
        pthread_mutexattr_destroy(&attr);
    if (!ok) {
        tba_stream_free(stream);
        return NULL;
    }
    madvise(stream->pool, sizeof *stream->pool, MADV_DONTDUMP);
    return stream;
}

/* Release this process's mapping without disturbing other live workers. */
void tba_stream_free(tba_stream_t *stream)
{
    if (!stream)
        return;
    if (stream->pool && stream->pool != MAP_FAILED) {
        munmap(stream->pool, sizeof *stream->pool);
    }
    EVP_PKEY_free(stream->key);
    free(stream);
}

/* Four slots make admission and expiry checking deliberately bounded. */
json_t *tba_stream_begin(tba_stream_t *stream)
{
    if (!stream || !tba_stream_lock(stream))
        return tba_json_error("Encrypted stream unavailable.");
    time_t now = tba_stream_now();
    unsigned index = TBA_STREAM_SLOTS;
    for (unsigned i = 0; i < TBA_STREAM_SLOTS; ++i) {
        if (stream->pool->slots[i].expires <= now) {
            sodium_memzero(&stream->pool->slots[i], sizeof stream->pool->slots[i]);
            index = i;
        }
    }
    json_t *result = NULL;
    if (now && index < TBA_STREAM_SLOTS) {
        tba_stream_slot_t *slot = &stream->pool->slots[index];
        unsigned char token[32], *der = NULL;
        randombytes_buf(token, sizeof token);
        sodium_bin2hex(slot->token, sizeof slot->token, token, sizeof token);
        int size = i2d_PUBKEY(stream->key, &der);
        result = tba_stream_ack();
        bool ok = result && size > 0 && tba_json_number(result, "slot", index) &&
                  tba_json_string(result, "token", slot->token) &&
                  tba_json_number(result, "chunk_limit", TBA_STREAM_CHUNK) &&
                  tba_json_number(result, "message_limit", TBA_STREAM_LIMIT) &&
                  tba_stream_binary(result, "public_key", der, (size_t)size);
        OPENSSL_free(der);
        slot->expires = ok ? now + 120 : 0;
        if (!ok) {
            json_free(result);
            result = NULL;
        }
    }
    pthread_mutex_unlock(&stream->pool->mutex);
    return result ? result : tba_json_error("Encrypted stream capacity is temporarily exhausted.");
}

/* Reclaim abandoned ciphertext even when no new reservations arrive. */
void tba_stream_expire(tba_stream_t *stream)
{
    if (!stream || !tba_stream_lock(stream))
        return;
    time_t now = tba_stream_now();
    for (unsigned i = 0; i < TBA_STREAM_SLOTS; ++i) {
        tba_stream_slot_t *slot = &stream->pool->slots[i];
        if (slot->expires && (!now || slot->expires <= now))
            sodium_memzero(slot, sizeof *slot);
    }
    pthread_mutex_unlock(&stream->pool->mutex);
}

/* Authenticate before retaining ciphertext; decode only after the authenticated final marker. */
json_t *tba_stream_receive(tba_stream_t *stream, const json_t *frame, json_t **message, tba_stream_reply_t **reply)
{
    *message = NULL;
    *reply = NULL;
    unsigned index, sequence = 0;
    if (!stream || !tba_stream_number(frame, "slot", TBA_STREAM_SLOTS - 1, &index) ||
        !tba_stream_lock(stream))
        return tba_json_error("Invalid encrypted stream.");
    tba_stream_slot_t *slot = &stream->pool->slots[index];
    const string_t *token = json_string_value(tba_json_get(frame, "token"));
    bool identified = token && string_byte_length(token) == 64 &&
                      sodium_memcmp(slot->token, string_c_str(token), 64) == 0;
    if (identified && tba_json_bool(frame, "cancel")) {
        sodium_memzero(slot, sizeof *slot);
        pthread_mutex_unlock(&stream->pool->mutex);
        return tba_stream_ack();
    }
    bool final = false;
    unsigned char cipher[TBA_STREAM_CHUNK + 16], plain[TBA_STREAM_CHUNK], key[32], wrapped[256];
    size_t length = 0, wrapped_size = 0;
    time_t now = tba_stream_now();
    bool ok = identified && now && slot->expires > now &&
              tba_stream_number(frame, "sequence", TBA_STREAM_FRAMES - 1, &sequence) && sequence == slot->count &&
              json_bool_value(tba_json_get(frame, "final"), &final) &&
              (final || sequence + 1 < TBA_STREAM_FRAMES) &&
              tba_stream_decode(frame, "wrapped_key", wrapped, sizeof wrapped, &wrapped_size) && wrapped_size == 256 &&
              (!sequence || sodium_memcmp(slot->wrapped, wrapped, 256) == 0) &&
              tba_stream_decode(frame, "ciphertext", cipher, sizeof cipher, &length) && length > 16 &&
              (final || length == sizeof cipher) && slot->size + length <= sizeof slot->data &&
              tba_stream_unwrap(stream, wrapped, key) &&
              tba_stream_crypt(key, slot->token, sequence, final, false, cipher, length, plain);
    if (ok) {
        memcpy(slot->wrapped, wrapped, sizeof wrapped);
        memcpy(slot->data + slot->size, cipher, length);
        slot->size += length;
        slot->lengths[slot->count++] = length;
    }
    if (ok && final) {
        size_t size = slot->size - 16 * slot->count, source = 0, target = 0;
        unsigned char *bytes = malloc(size);
        *reply = sodium_malloc(sizeof **reply);
        ok = bytes && *reply;
        for (unsigned i = 0; ok && i < slot->count; ++i) {
            ok = tba_stream_crypt(key, slot->token, i, i + 1 == slot->count, false,
                                  slot->data + source, slot->lengths[i], bytes + target);
            source += slot->lengths[i];
            target += slot->lengths[i] - 16;
        }
        *message = ok ? tba_wire_decode(bytes, size) : NULL;
        ok = ok && json_type(*message) == JSON_OBJECT;
        if (ok) {
            memcpy((*reply)->key, key, sizeof key);
            memcpy((*reply)->token, slot->token, sizeof slot->token);
            (*reply)->used = false;
        }
        if (bytes) {
            sodium_memzero(bytes, size);
            free(bytes);
        }
    }
    if (identified && (!ok || final))
        sodium_memzero(slot, sizeof *slot);
    pthread_mutex_unlock(&stream->pool->mutex);
    sodium_memzero(plain, sizeof plain);
    sodium_memzero(key, sizeof key);
    if (!ok) {
        json_free(*message);
        *message = NULL;
        tba_stream_reply_free(*reply);
        *reply = NULL;
    }
    return ok ? tba_stream_ack() : tba_json_error("Encrypted stream rejected; start a new stream.");
}

/* Encrypt the entire bounded reply in a separate nonce domain, with no plaintext fallback. */
json_t *tba_stream_seal(tba_stream_reply_t *reply, const json_t *result)
{
    if (!reply || reply->used)
        return NULL;
    reply->used = true;
    array_t *bytes = tba_wire_encode(result);
    size_t size = bytes ? array_size(bytes) : 0;
    unsigned char *cipher = size && size <= TBA_STREAM_LIMIT ? malloc(size + 16) : NULL;
    json_t *envelope = tba_stream_ack();
    bool ok = cipher && envelope && tba_stream_crypt(reply->key, reply->token, 0, true, true,
              array_get(bytes, 0), size, cipher) && tba_stream_binary(envelope, "ciphertext", cipher, size + 16);
    if (bytes && size)
        sodium_memzero(array_get(bytes, 0), size);
    array_destroy(bytes);
    free(cipher);
    if (!ok) {
        json_free(envelope);
        envelope = NULL;
    }
    return envelope;
}

/* sodium_free erases the guarded key allocation. */
void tba_stream_reply_free(tba_stream_reply_t *reply)
{
    sodium_free(reply);
}
