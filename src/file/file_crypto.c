#define MARS_FILE_INTERNAL_ACCESS
#include "file_internal.h"

#include <errno.h>
#include <string.h>
#include <sodium.h>

#define FILE_CIPHER_EXTRA crypto_secretstream_xchacha20poly1305_ABYTES
#define FILE_CIPHER_HEADER (16u + crypto_secretstream_xchacha20poly1305_HEADERBYTES)
static const unsigned char file_cipher_magic[8] = {'M', 'A', 'R', 'S', 'E', 'N', 'C', 1};

struct _file_key_t {
    unsigned char bytes[crypto_secretstream_xchacha20poly1305_KEYBYTES];
};

typedef struct {
    const file_key_t *key;
    bool compress;
    uint64_t limit;
} file_crypto_options_t;

typedef struct {
    crypto_secretstream_xchacha20poly1305_state state;
    unsigned char associated[FILE_CIPHER_HEADER + 4];
    file_t *output;
} file_encrypt_state_t;

/* Allocate guarded key storage and fill it using libsodium's random generator. */
file_key_t *file_key_generate(void)
{
    if (sodium_init() < 0) {
        errno = EIO;
        return NULL;
    }
    file_key_t *key = sodium_malloc(sizeof(*key));
    if (!key) {
        errno = ENOMEM;
        return NULL;
    }
    crypto_secretstream_xchacha20poly1305_keygen(key->bytes);
    return key;
}

/* Import raw key material, without treating passwords as keys. */
file_key_t *file_key_from_bytes(const void *bytes, size_t size)
{
    if (!bytes || size != crypto_secretstream_xchacha20poly1305_KEYBYTES) {
        errno = EINVAL;
        return NULL;
    }
    file_key_t *key = file_key_generate();
    if (key)
        memcpy(key->bytes, bytes, size);
    return key;
}

/* Export to caller-owned secret storage. */
bool file_key_export(const file_key_t *key, void *bytes, size_t size)
{
    if (!key || !bytes || size != sizeof(key->bytes)) {
        errno = EINVAL;
        return false;
    }
    memcpy(bytes, key->bytes, size);
    return true;
}

/* sodium_free erases the guarded allocation before releasing it. */
void file_key_free(file_key_t *key)
{
    if (key)
        sodium_free(key);
}

static bool file_encrypt_record(file_encrypt_state_t *context, const unsigned char *bytes, size_t size,
                                 unsigned char tag)
{
    unsigned char ciphertext[FILE_STREAM_BLOCK + FILE_CIPHER_EXTRA];
    if (size > FILE_STREAM_BLOCK)
        return file_fail(context->output, EINVAL);
    size_t cipher_size = size + FILE_CIPHER_EXTRA;
    file_encode_u64(context->associated + FILE_CIPHER_HEADER, cipher_size, 4);
    int rc = crypto_secretstream_xchacha20poly1305_push(&context->state, ciphertext, NULL, bytes, size,
        context->associated, sizeof(context->associated), tag);
    bool ok = rc == 0 && file_emit_output(context->output, context->associated + FILE_CIPHER_HEADER, 4) &&
              file_emit_output(context->output, ciphertext, cipher_size);
    sodium_memzero(ciphertext, sizeof(ciphertext));
    return rc ? file_fail(context->output, EIO) : ok;
}

static bool file_encrypt_emit(void *context, const unsigned char *bytes, size_t size)
{
    return file_encrypt_record(context, bytes, size, crypto_secretstream_xchacha20poly1305_TAG_MESSAGE);
}

static bool file_encrypt_transform(file_t *source, file_t *output, void *argument)
{
    const file_crypto_options_t *options = argument;
    file_encrypt_state_t context = {0};
    unsigned char buffer[FILE_STREAM_BLOCK];
    context.output = output;
    memcpy(context.associated, file_cipher_magic, sizeof(file_cipher_magic));
    context.associated[8] = options->compress ? 1 : 0;
    int rc = crypto_secretstream_xchacha20poly1305_init_push(&context.state, context.associated + 16,
                                                           options->key->bytes);
    bool ok = rc == 0 && file_emit_output(output, context.associated, FILE_CIPHER_HEADER);
    if (rc)
        file_fail(source, EIO);
    if (ok && options->compress) {
        ok = file_compress_emit(source, file_encrypt_emit, &context);
    } else if (ok) {
        size_t count;
        while ((ok = file_read(source, buffer, sizeof(buffer), &count)) && count)
            if (!(ok = file_encrypt_emit(&context, buffer, count)))
                break;
    }
    if (ok)
        ok = file_encrypt_record(&context, NULL, 0, crypto_secretstream_xchacha20poly1305_TAG_FINAL);
    sodium_memzero(buffer, sizeof(buffer));
    sodium_memzero(&context, sizeof(context));
    return ok;
}

static bool file_decrypt_transform(file_t *source, file_t *output, void *argument)
{
    const file_crypto_options_t *options = argument;
    crypto_secretstream_xchacha20poly1305_state state;
    unsigned char associated[FILE_CIPHER_HEADER + 4];
    unsigned char ciphertext[FILE_STREAM_BLOCK + FILE_CIPHER_EXTRA], plaintext[FILE_STREAM_BLOCK];
    file_decoder_t *decoder = NULL;
    uint64_t total = 0;
    bool ok = file_read_exact(source, associated, FILE_CIPHER_HEADER);
    if (!ok)
        goto done;
    if (memcmp(associated, file_cipher_magic, sizeof(file_cipher_magic)) != 0 || associated[8] > 1 ||
        memcmp(associated + 9, "\0\0\0\0\0\0\0", 7) != 0) {
        ok = file_fail(source, EBADMSG);
        goto done;
    }
    if (crypto_secretstream_xchacha20poly1305_init_pull(&state, associated + 16, options->key->bytes) != 0) {
        ok = file_fail(source, EBADMSG);
        goto done;
    }
    if (associated[8]) {
        decoder = file_decoder_new(source, output, options->limit);
        if (!decoder) {
            ok = false;
            goto done;
        }
    }
    for (;;) {
        if (!(ok = file_read_exact(source, associated + FILE_CIPHER_HEADER, 4)))
            break;
        uint64_t size = file_decode_u64(associated + FILE_CIPHER_HEADER, 4);
        if (size < FILE_CIPHER_EXTRA || size > sizeof(ciphertext)) {
            ok = file_fail(source, EBADMSG);
            break;
        }
        if (!(ok = file_read_exact(source, ciphertext, (size_t)size)))
            break;
        unsigned long long count;
        unsigned char tag;
        if (crypto_secretstream_xchacha20poly1305_pull(&state, plaintext, &count, &tag, ciphertext, size,
                                                      associated, sizeof(associated)) != 0) {
            ok = file_fail(source, EBADMSG);
            break;
        }
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
            unsigned char trailing;
            size_t remaining;
            if (count)
                ok = file_fail(source, EBADMSG);
            else if ((ok = file_read(source, &trailing, 1, &remaining)) && remaining)
                ok = file_fail(source, EBADMSG);
            if (ok && decoder)
                ok = file_decoder_finish(decoder);
            break;
        }
        if (tag != crypto_secretstream_xchacha20poly1305_TAG_MESSAGE || !count) {
            ok = file_fail(source, EBADMSG);
            break;
        }
        if (decoder) {
            ok = file_decoder_feed(decoder, plaintext, (size_t)count);
        } else if (count > options->limit - total) {
            ok = file_fail(source, EFBIG);
        } else {
            ok = file_emit_output(output, plaintext, (size_t)count);
            total += count;
        }
        sodium_memzero(plaintext, sizeof(plaintext));
        if (!ok)
            break;
    }
done:
    file_decoder_free(decoder);
    sodium_memzero(&state, sizeof(state));
    sodium_memzero(plaintext, sizeof(plaintext));
    sodium_memzero(ciphertext, sizeof(ciphertext));
    return ok;
}

/* Write a versioned authenticated stream, optionally fed directly by a compressor. */
bool file_encrypt(file_t *source, file_t *destination, const file_key_t *key, bool compress, bool overwrite)
{
    if (!key)
        return file_fail(source, EINVAL);
    file_crypto_options_t options = {key, compress, 0};
    return file_transform(source, destination, overwrite, file_encrypt_transform, &options);
}

/* Publish plaintext only after every record, the final tag and EOF have been verified. */
bool file_decrypt(file_t *source, file_t *destination, const file_key_t *key, uint64_t max_bytes, bool overwrite)
{
    if (!key)
        return file_fail(source, EINVAL);
    file_crypto_options_t options = {key, false, max_bytes};
    return file_transform(source, destination, overwrite, file_decrypt_transform, &options);
}
