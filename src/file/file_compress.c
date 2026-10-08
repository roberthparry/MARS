/**
 * @file file_compress.c
 * @brief Bounded streaming Zstandard file transforms.
 *
 * Compresses and decompresses file contents through the shared staging machinery. Decompression checks the
 * configured output limit before publication, so compressed input cannot silently request unbounded output.
 *
 * This belongs to the Linux-only file.h implementation. Filesystem operations and transforms must retain the
 * public error, ownership and output-publication contracts.
 */

#define MARS_FILE_INTERNAL_ACCESS
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include <zstd.h>
#include <zstd_errors.h>

#include "file_internal.h"

struct file_decoder_t {
    ZSTD_DCtx *context;
    file_t *source;
    file_t *output;
    uint64_t limit;
    uint64_t count;
    unsigned char magic[4];
    size_t magic_size;
    bool complete;
    unsigned char buffer[FILE_STREAM_BLOCK];
};

bool file_compress_emit(file_t *source, file_emit_fn emit, void *context)
{
    ZSTD_CCtx *codec = ZSTD_createCCtx();
    if (!codec)
        return file_fail(source, ENOMEM);
    unsigned char input[FILE_STREAM_BLOCK], output[FILE_STREAM_BLOCK];
    bool ok = !ZSTD_isError(ZSTD_CCtx_setParameter(codec, ZSTD_c_compressionLevel, 3)) &&
              !ZSTD_isError(ZSTD_CCtx_setParameter(codec, ZSTD_c_windowLog, 20)) &&
              !ZSTD_isError(ZSTD_CCtx_setParameter(codec, ZSTD_c_hashLog, 17)) &&
              !ZSTD_isError(ZSTD_CCtx_setParameter(codec, ZSTD_c_chainLog, 16)) &&
              !ZSTD_isError(ZSTD_CCtx_setParameter(codec, ZSTD_c_checksumFlag, 1));
    if (!ok)
        file_fail(source, EIO);
    while (ok) {
        size_t count;
        ok = file_read(source, input, sizeof(input), &count);
        if (!ok)
            break;
        ZSTD_inBuffer in = {input, count, 0};
        size_t remaining;
        do {
            ZSTD_outBuffer out = {output, sizeof(output), 0};
            remaining = ZSTD_compressStream2(codec, &out, &in, count ? ZSTD_e_continue : ZSTD_e_end);
            if (ZSTD_isError(remaining)) {
                ok = file_fail(source, EIO);
                break;
            }
            if (out.pos && !emit(context, output, out.pos)) {
                ok = false;
                break;
            }
        } while (in.pos < in.size || (!count && remaining));
        if (!count)
            break;
    }
    sodium_memzero(input, sizeof(input));
    sodium_memzero(output, sizeof(output));
    ZSTD_freeCCtx(codec);
    return ok;
}

file_decoder_t *file_decoder_new(file_t *source, file_t *output, uint64_t limit)
{
    file_decoder_t *decoder = calloc(1, sizeof(*decoder));
    if (decoder)
        decoder->context = ZSTD_createDCtx();
    if (!decoder || !decoder->context) {
        free(decoder);
        file_fail(source, ENOMEM);
        return NULL;
    }
    decoder->source = source;
    decoder->output = output;
    decoder->limit = limit;
    if (ZSTD_isError(ZSTD_DCtx_setParameter(decoder->context, ZSTD_d_windowLogMax, 23))) {
        file_decoder_free(decoder);
        file_fail(source, EIO);
        return NULL;
    }
    return decoder;
}

static bool file_decode_block(file_decoder_t *decoder, const unsigned char *bytes, size_t size)
{
    if (decoder->complete)
        return size ? file_fail(decoder->source, EBADMSG) : true;
    ZSTD_inBuffer in = {bytes, size, 0};
    size_t produced;
    do {
        ZSTD_outBuffer out = {decoder->buffer, sizeof(decoder->buffer), 0};
        size_t remaining = ZSTD_decompressStream(decoder->context, &out, &in);
        if (ZSTD_isError(remaining))
            return file_fail(decoder->source, ZSTD_getErrorCode(remaining) == ZSTD_error_frameParameter_windowTooLarge
                ? EFBIG : EBADMSG);
        if (out.pos > decoder->limit - decoder->count)
            return file_fail(decoder->source, EFBIG);
        if (out.pos && !file_emit_output(decoder->output, decoder->buffer, out.pos))
            return file_fail(decoder->source, file_last_error(decoder->output));
        decoder->count += out.pos;
        if (!remaining) {
            decoder->complete = true;
            return in.pos == in.size ? true : file_fail(decoder->source, EBADMSG);
        }
        produced = out.pos;
    } while (in.pos < in.size || produced == sizeof(decoder->buffer));
    return true;
}

bool file_decoder_feed(file_decoder_t *decoder, const unsigned char *bytes, size_t size)
{
    if (decoder->magic_size < sizeof(decoder->magic)) {
        size_t needed = sizeof(decoder->magic) - decoder->magic_size;
        size_t take = size < needed ? size : needed;
        memcpy(decoder->magic + decoder->magic_size, bytes, take);
        decoder->magic_size += take;
        bytes += take;
        size -= take;
        if (decoder->magic_size < sizeof(decoder->magic))
            return true;
        if (memcmp(decoder->magic, "\x28\xb5\x2f\xfd", 4) != 0)
            return file_fail(decoder->source, EBADMSG);
        if (!file_decode_block(decoder, decoder->magic, sizeof(decoder->magic)))
            return false;
    }
    return file_decode_block(decoder, bytes, size);
}

bool file_decoder_finish(file_decoder_t *decoder)
{
    return decoder->complete ? true : file_fail(decoder->source, EBADMSG);
}

void file_decoder_free(file_decoder_t *decoder)
{
    if (decoder) {
        ZSTD_freeDCtx(decoder->context);
        sodium_memzero(decoder, sizeof(*decoder));
        free(decoder);
    }
}

static bool file_compress_transform(file_t *source, file_t *output, void *context)
{
    (void)context;
    return file_compress_emit(source, file_emit_output, output);
}

static bool file_decompress_transform(file_t *source, file_t *output, void *context)
{
    file_decoder_t *decoder = file_decoder_new(source, output, *(uint64_t *)context);
    if (!decoder)
        return false;
    unsigned char buffer[FILE_STREAM_BLOCK];
    bool ok;
    size_t count;
    while ((ok = file_read(source, buffer, sizeof(buffer), &count)) && count)
        if (!(ok = file_decoder_feed(decoder, buffer, count)))
            break;
    if (ok)
        ok = file_decoder_finish(decoder);
    file_decoder_free(decoder);
    return ok;
}

/* Compress one frame with a fixed, bounded-memory profile and a content checksum. */
bool file_compress_zstd(file_t *source, file_t *destination, bool overwrite)
{
    return file_transform(source, destination, overwrite, file_compress_transform, NULL);
}

/* Decompress exactly one frame without exceeding the caller's output budget. */
bool file_decompress_zstd(file_t *source, file_t *destination, uint64_t max_bytes, bool overwrite)
{
    return file_transform(source, destination, overwrite, file_decompress_transform, &max_bytes);
}
