/**
 * @file test_tba_stream.c
 * @brief Deterministic authenticated transport rejection and lifetime checks.
 *
 * Exercises the native boundary without files or network access. An isolated
 * test clock advances expiry without a real two-minute wait. Independent EVP
 * operations construct client envelopes and verify the encrypted response.
 */
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tba_wire.h"
#include "tba_stream.h"
#include "test_tba_support.h"

static bool test_tba_clock_advance;
int __real_clock_gettime(clockid_t clock, struct timespec *value);
int __wrap_clock_gettime(clockid_t clock, struct timespec *value);

/* Link wrapping belongs only to this native test executable. */
int __wrap_clock_gettime(clockid_t clock, struct timespec *value)
{
    int status = __real_clock_gettime(clock, value);
    if (!status && clock == CLOCK_MONOTONIC && test_tba_clock_advance)
        value->tv_sec += 121;
    return status;
}

static bool test_tba_binary(json_t *value, const char *name, const unsigned char *bytes, size_t length)
{
    size_t capacity = sodium_base64_ENCODED_LEN(length, sodium_base64_VARIANT_ORIGINAL);
    char *text = malloc(capacity);
    bool ok = text && sodium_bin2base64(text, capacity, bytes, length, sodium_base64_VARIANT_ORIGINAL) &&
              tba_json_string(value, name, text);
    free(text);
    return ok;
}

static json_t *test_tba_frame(const json_t *begin, const unsigned char *key, const void *bytes,
                               size_t length, unsigned sequence, bool final)
{
    unsigned char der[512], wrapped[256], iv[12] = {0};
    size_t size = 0, wrapped_size = sizeof wrapped;
    const char *encoded = tba_json_text(begin, "public_key");
    bool ok = sodium_base642bin(der, sizeof der, encoded, strlen(encoded), NULL, &size, NULL,
                               sodium_base64_VARIANT_ORIGINAL) == 0;
    const unsigned char *cursor = der;
    EVP_PKEY *public_key = ok ? d2i_PUBKEY(NULL, &cursor, (long)size) : NULL;
    EVP_PKEY_CTX *rsa = public_key ? EVP_PKEY_CTX_new(public_key, NULL) : NULL;
    EVP_CIPHER_CTX *aes = EVP_CIPHER_CTX_new();
    unsigned char *cipher = malloc(length + 16);
    for (unsigned i = 0; i < 4; ++i)
        iv[11 - i] = sequence >> (8 * i);
    string_t *aad = string_sprintf("MARS-TBA-1:%s:0:%u:%u", tba_json_text(begin, "token"), sequence, final);
    int count = 0, tail = 0;
    ok = rsa && aes && cipher && aad && EVP_PKEY_encrypt_init(rsa) > 0 &&
         EVP_PKEY_CTX_set_rsa_padding(rsa, RSA_PKCS1_OAEP_PADDING) > 0 &&
         EVP_PKEY_CTX_set_rsa_oaep_md(rsa, EVP_sha256()) > 0 &&
         EVP_PKEY_CTX_set_rsa_mgf1_md(rsa, EVP_sha256()) > 0 &&
         EVP_PKEY_encrypt(rsa, wrapped, &wrapped_size, key, 32) > 0 &&
         EVP_EncryptInit_ex(aes, EVP_aes_256_gcm(), NULL, key, iv) > 0 &&
         EVP_EncryptUpdate(aes, NULL, &count, (const void *)string_c_str(aad), string_byte_length(aad)) > 0 &&
         EVP_EncryptUpdate(aes, cipher, &count, bytes, length) > 0 &&
         EVP_EncryptFinal_ex(aes, cipher + count, &tail) > 0 &&
         EVP_CIPHER_CTX_ctrl(aes, EVP_CTRL_GCM_GET_TAG, 16, cipher + length) > 0;
    json_t *frame = json_new_object();
    ok = ok && frame && tba_json_set(frame, "slot", tba_json_get(begin, "slot")) &&
         tba_json_set(frame, "token", tba_json_get(begin, "token")) &&
         tba_json_number(frame, "sequence", sequence) && tba_json_flag(frame, "final", final) &&
         test_tba_binary(frame, "wrapped_key", wrapped, wrapped_size) &&
         test_tba_binary(frame, "ciphertext", cipher, length + 16);
    EVP_PKEY_CTX_free(rsa);
    EVP_PKEY_free(public_key);
    EVP_CIPHER_CTX_free(aes);
    free(cipher);
    string_free(aad);
    if (!ok) {
        json_free(frame);
        frame = NULL;
    }
    return frame;
}

static bool test_tba_received(tba_stream_t *stream, const json_t *frame, bool accepted, bool complete)
{
    json_t *message = NULL;
    tba_stream_reply_t *reply = NULL;
    json_t *ack = tba_stream_receive(stream, frame, &message, &reply);
    bool ok = ack && tba_json_bool(ack, "ok") == accepted && !!message == complete && !!reply == complete;
    json_free(message);
    json_free(ack);
    tba_stream_reply_free(reply);
    return ok;
}

static bool test_tba_cancel(tba_stream_t *stream, json_t *begin)
{
    return tba_json_number(begin, "sequence", 0) && tba_json_flag(begin, "cancel", true) &&
           test_tba_received(stream, begin, true, false);
}

static bool test_tba_response(tba_stream_t *stream, const json_t *frame, const unsigned char *key,
                               const json_t *expected)
{
    json_t *message = NULL;
    tba_stream_reply_t *reply = NULL;
    json_t *ack = tba_stream_receive(stream, frame, &message, &reply);
    json_t *envelope = reply ? tba_stream_seal(reply, message) : NULL;
    json_t *repeated = reply ? tba_stream_seal(reply, message) : NULL;
    unsigned char cipher[1024], plain[1024], iv[12] = {1};
    size_t length = 0;
    const char *text = tba_json_text(envelope, "ciphertext");
    string_t *aad = string_sprintf("MARS-TBA-1:%s:1:0:1", tba_json_text(frame, "token"));
    EVP_CIPHER_CTX *aes = EVP_CIPHER_CTX_new();
    int count = 0, tail = 0;
    bool ok = tba_json_bool(ack, "ok") && message && envelope && !repeated && aes && aad &&
              sodium_base642bin(cipher, sizeof cipher, text, strlen(text), NULL, &length, NULL,
                                sodium_base64_VARIANT_ORIGINAL) == 0 && length > 16 &&
              EVP_DecryptInit_ex(aes, EVP_aes_256_gcm(), NULL, key, iv) > 0 &&
              EVP_DecryptUpdate(aes, NULL, &count, (const void *)string_c_str(aad), string_byte_length(aad)) > 0 &&
              EVP_DecryptUpdate(aes, plain, &count, cipher, length - 16) > 0 &&
              EVP_CIPHER_CTX_ctrl(aes, EVP_CTRL_GCM_SET_TAG, 16, cipher + length - 16) > 0 &&
              EVP_DecryptFinal_ex(aes, plain + count, &tail) > 0;
    json_t *decoded = ok ? tba_wire_decode(plain, length - 16) : NULL;
    ok = decoded && !strcmp(tba_json_text(decoded, "text"), tba_json_text(expected, "text")) && ok;
    json_free(decoded);
    json_free(repeated);
    json_free(envelope);
    json_free(ack);
    json_free(message);
    tba_stream_reply_free(reply);
    EVP_CIPHER_CTX_free(aes);
    string_free(aad);
    sodium_memzero(plain, sizeof plain);
    return ok;
}

static bool test_tba_sequence_rejections(tba_stream_t *stream, const unsigned char *key, const array_t *bytes)
{
    static const double invalid[] = {-1, 64, 0.5};
    bool ok = true;
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid + 1; ++i) {
        json_t *begin = tba_stream_begin(stream);
        json_t *valid = test_tba_frame(begin, key, array_get(bytes, 0), array_size(bytes), 0, true);
        json_t *frame = valid ? json_clone(valid) : NULL;
        bool changed = i < sizeof invalid / sizeof *invalid ? tba_json_number(frame, "sequence", invalid[i]) :
                                                              tba_json_string(frame, "sequence", "0");
        bool rejected = changed && test_tba_received(stream, frame, false, false);
        bool discarded = test_tba_received(stream, valid, false, false);
        if (!rejected || !discarded)
            string_fprintf(stderr, "Invalid sequence case %zu: rejected=%d discarded=%d\n", i, rejected, discarded);
        ok = rejected && discarded && ok;
        json_free(frame);
        json_free(valid);
        json_free(begin);
    }
    return ok;
}

static bool test_tba_chunk_boundary(tba_stream_t *stream, const unsigned char *key, bool complete)
{
    size_t capacity = 2u * 1024u * 1024u;
    unsigned char *buffer = malloc(capacity);
    if (!buffer)
        return false;
    memset(buffer, 'x', capacity);
    string_t *text = string_new();
    bool ready = text && string_append_chars(text, (const char *)buffer, capacity - 100) == 0;
    json_t *value = ready ? json_new_string(text) : NULL, *object = json_new_object();
    ready = ready && value && object && tba_json_set(object, "text", value);
    array_t *encoded = ready ? tba_wire_encode(object) : NULL;
    size_t length = complete ? array_size(encoded) : capacity;
    const unsigned char *source = complete ? array_get(encoded, 0) : buffer;
    json_t *begin = tba_stream_begin(stream), *wrapped = NULL;
    bool ok = source && length > capacity - 32768 && length <= capacity && tba_json_bool(begin, "ok");
    for (unsigned sequence = 0; ok && sequence < 64; ++sequence) {
        size_t offset = (size_t)sequence * 32768;
        size_t count = length - offset < 32768 ? length - offset : 32768;
        bool final = complete && sequence == 63;
        json_t *frame = test_tba_frame(begin, key, source + offset, count, sequence, final);
        if (!sequence)
            wrapped = json_clone(tba_json_get(frame, "wrapped_key"));
        ok = frame && wrapped && tba_json_set(frame, "wrapped_key", wrapped) &&
             test_tba_received(stream, frame, complete || sequence < 63, final);
        json_free(frame);
    }
    /* Completion or an impossible continuation releases the whole reservation. */
    ok = test_tba_received(stream, begin, false, false) && ok;
    json_free(wrapped);
    json_free(begin);
    array_destroy(encoded);
    json_free(object);
    json_free(value);
    string_free(text);
    free(buffer);
    return ok;
}

/* Test rejection without ever publishing confidential observations. */
bool test_tba_stream_checks(void)
{
    tba_stream_t *stream = tba_stream_new();
    json_t *value = json_new_object();
    bool ok = stream && value && tba_json_string(value, "text", "private test value £");
    array_t *bytes = ok ? tba_wire_encode(value) : NULL;
    unsigned char key[32];
    randombytes_buf(key, sizeof key);
    json_t *begin = stream ? tba_stream_begin(stream) : NULL;
    json_t *frame = bytes && begin ? test_tba_frame(begin, key, array_get(bytes, 0), array_size(bytes), 0, true) : NULL;
    ok = frame && test_tba_response(stream, frame, key, value) && ok;
    ok = test_tba_received(stream, frame, false, false) && ok;
    json_free(frame);
    json_free(begin);

    ok = bytes && test_tba_sequence_rejections(stream, key, bytes) && ok;
    ok = test_tba_chunk_boundary(stream, key, true) && ok;
    ok = test_tba_chunk_boundary(stream, key, false) && ok;

    /* Fixed, independent mutations cover envelope authentication and ordering. */
    for (unsigned mutation = 0; stream && bytes && mutation < 6; ++mutation) {
        begin = tba_stream_begin(stream);
        frame = test_tba_frame(begin, key, array_get(bytes, 0), array_size(bytes), 0, true);
        bool changed = frame != NULL;
        if (mutation == 0)
            changed = tba_json_string(frame, "ciphertext", "AAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        else if (mutation == 1)
            changed = tba_json_string(frame, "wrapped_key", "AAAA");
        else if (mutation == 2)
            changed = tba_json_number(frame, "sequence", 1);
        else if (mutation == 3)
            changed = tba_json_flag(frame, "final", false);
        else if (mutation == 4)
            changed = tba_json_string(frame, "token", "0000000000000000000000000000000000000000000000000000000000000000");
        else {
            test_tba_clock_advance = true;
            tba_stream_expire(stream);
            test_tba_clock_advance = false;
        }
        ok = changed && test_tba_received(stream, frame, false, false) && ok;
        if (mutation == 4)
            ok = test_tba_cancel(stream, begin) && ok;
        json_free(frame);
        json_free(begin);
    }

    unsigned char *large = calloc(32768, 1);
    begin = tba_stream_begin(stream);
    frame = large ? test_tba_frame(begin, key, large, 32768, 0, false) : NULL;
    ok = frame && test_tba_received(stream, frame, true, false) && ok;
    ok = test_tba_received(stream, frame, false, false) && ok;
    json_free(frame);
    json_free(begin);
    free(large);

    json_t *slots[4] = {0};
    for (unsigned i = 0; i < 4; ++i) {
        slots[i] = tba_stream_begin(stream);
        ok = tba_json_bool(slots[i], "ok") && ok;
    }
    json_t *full = tba_stream_begin(stream);
    ok = full && !tba_json_bool(full, "ok") && ok;
    json_free(full);
    for (unsigned i = 0; i < 4; ++i) {
        ok = test_tba_cancel(stream, slots[i]) && ok;
        json_free(slots[i]);
    }
    begin = tba_stream_begin(stream);
    ok = tba_json_bool(begin, "ok") && test_tba_cancel(stream, begin) && ok;
    json_free(begin);
    array_destroy(bytes);
    json_free(value);
    sodium_memzero(key, sizeof key);
    tba_stream_free(stream);
    return ok;
}
