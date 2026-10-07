/**
 * @file test_file_transforms.c
 * @brief Compression and authenticated encryption regressions.
 *
 * Checks successful and failing file transforms and documented compression or encryption round trips. Verification
 * includes safe handling of invalid data and publication of output.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include "file.h"
#include "array.h"
#include "test_harness.h"

#include <errno.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool encrypted;
    bool compressed;
} transform_mode_t;

static const transform_mode_t transform_modes[] = {
    {false, true},
    {true, false},
    {true, true}
};

static bool encode(transform_mode_t mode, file_t *source, file_t *destination,
                   const file_key_t *key, bool overwrite)
{
    if (mode.encrypted)
        return file_encrypt(source, destination, key, mode.compressed, overwrite);
    return file_compress_zstd(source, destination, overwrite);
}

static bool decode(transform_mode_t mode, file_t *source, file_t *destination,
                   const file_key_t *key, uint64_t limit, bool overwrite)
{
    if (mode.encrypted)
        return file_decrypt(source, destination, key, limit, overwrite);
    return file_decompress_zstd(source, destination, limit, overwrite);
}

static file_t *transform_fixture(const char *leaf)
{
    return file_new_cstr(test_case_temp_path(leaf));
}

/* Compare every byte, including embedded NULs, through the public array API. */
static bool has_bytes(file_t *file, const unsigned char *expected, size_t size)
{
    array_t *bytes = file_read_all_bytes(file);
    bool equal = bytes && array_size(bytes) == size;
    for (size_t i = 0; equal && i < size; ++i)
        equal = *(const unsigned char *)array_get(bytes, i) == expected[i];
    array_destroy(bytes);
    return equal;
}

static bool has_private_permissions(file_t *file)
{
    file_info_t *info = file_get_info(file);
    /* umask may remove owner permissions, but must never grant group/other access. */
    bool private = info && !(file_info_permissions(info) & 0177u);
    file_info_free(info);
    return private;
}

static bool directory_has_entries(file_t *directory, size_t expected)
{
    array_t *entries = file_list_directory(directory);
    bool equal = entries && array_size(entries) == expected;
    array_destroy(entries);
    return equal;
}

static bool flip_byte(file_t *file, int64_t offset, file_seek_t origin)
{
    unsigned char byte;
    size_t count;
    if (!file_open(file, FILE_MODE_OPEN, FILE_ACCESS_READ_WRITE))
        return false;
    bool ok = file_seek(file, offset, origin) && file_read(file, &byte, 1, &count) && count == 1;
    if (ok) {
        byte ^= 1;
        ok = file_seek(file, -1, FILE_SEEK_CURRENT) && file_write(file, &byte, 1, &count) && count == 1;
    }
    bool closed = file_close(file);
    return ok && closed;
}

/* Append an entire second stream without relying on either private container format. */
static bool append_file(file_t *source, file_t *destination)
{
    unsigned char buffer[4096];
    bool ok = file_open_read(source) && file_open(destination, FILE_MODE_APPEND, FILE_ACCESS_WRITE);
    size_t count = 0, written;
    while (ok && (ok = file_read(source, buffer, sizeof(buffer), &count)) && count)
        ok = file_write(destination, buffer, count, &written) && written == count;
    bool source_closed = file_close(source);
    bool destination_closed = file_close(destination);
    return ok && source_closed && destination_closed;
}

static void round_trip_sizes(const size_t *sizes, size_t count)
{
    file_t *source = transform_fixture("transform-source.bin");
    file_t *encoded = transform_fixture("transform-encoded.bin");
    file_t *restored = transform_fixture("transform-restored.bin");
    file_key_t *key = file_key_generate();
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(encoded);
    ASSERT_NOT_NULL(restored);
    ASSERT_NOT_NULL(key);
    for (size_t sample = 0; sample < count; ++sample) {
        size_t size = sizes[sample];
        unsigned char *bytes = malloc(size ? size : 1);
        ASSERT_NOT_NULL(bytes);
        /* Deterministic binary data with both repeated and poorly compressible regions. */
        uint32_t state = 0x12345678u;
        for (size_t i = 0; i < size; ++i) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            bytes[i] = i % 8192 < 4096 ? (unsigned char)i : (unsigned char)state;
        }
        ASSERT_TRUE(file_write_all_bytes(source, size ? bytes : NULL, size));
        for (size_t mode = 0; mode < sizeof(transform_modes) / sizeof(transform_modes[0]); ++mode) {
            ASSERT_TRUE(encode(transform_modes[mode], source, encoded, key, false));
            ASSERT_EQ_INT(file_last_error(source), 0);
            ASSERT_TRUE(!file_is_open(source) && !file_is_open(encoded));
            ASSERT_TRUE(has_private_permissions(encoded));
            ASSERT_TRUE(decode(transform_modes[mode], encoded, restored, key, size, false));
            ASSERT_EQ_INT(file_last_error(encoded), 0);
            ASSERT_TRUE(!file_is_open(encoded) && !file_is_open(restored));
            ASSERT_TRUE(has_private_permissions(restored));
            ASSERT_TRUE(has_bytes(restored, bytes, size));
            ASSERT_TRUE(has_bytes(source, bytes, size));
            ASSERT_TRUE(file_delete(restored));
            ASSERT_TRUE(file_delete(encoded));
        }
        free(bytes);
    }
    ASSERT_TRUE(file_delete(source));
    file_key_free(key);
    file_free(restored);
    file_free(encoded);
    file_free(source);
}

/* Exercise ordinary binary inputs and the exact zero-byte output allowance. */
void test_file_transforms_round_trip_and_empty(void)
{
    const size_t sizes[] = {0, 1, 257, 4097};
    round_trip_sizes(sizes, sizeof(sizes) / sizeof(sizes[0]));
}

/* Cross multiple codec buffers and authenticated records, with a partial final block. */
void test_file_transforms_large_round_trip(void)
{
    const size_t sizes[] = {2u * 1024u * 1024u + 137u};
    round_trip_sizes(sizes, sizeof(sizes) / sizeof(sizes[0]));
}

/* Reject damaged streams without publishing plaintext or leaking a staged file. */
void test_file_transforms_corrupt_truncated_and_trailing(void)
{
    static const unsigned char content[] = "binary\0payload\xff with a checksum and authentication";
    static const unsigned char saved[] = "keep existing destination";
    file_t *source = transform_fixture("damage-source.bin");
    file_t *encoded = transform_fixture("damage-good.bin");
    file_t *damaged = transform_fixture("damage-bad.bin");
    file_t *destination = transform_fixture("damage-destination.bin");
    file_t *directory = file_new_cstr(test_case_temp_dir());
    file_key_t *key = file_key_generate();
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(encoded);
    ASSERT_NOT_NULL(damaged);
    ASSERT_NOT_NULL(destination);
    ASSERT_NOT_NULL(directory);
    ASSERT_NOT_NULL(key);
    ASSERT_TRUE(file_write_all_bytes(source, content, sizeof(content)));
    for (size_t mode = 0; mode < sizeof(transform_modes) / sizeof(transform_modes[0]); ++mode) {
        ASSERT_TRUE(encode(transform_modes[mode], source, encoded, key, true));
        file_info_t *info = file_get_info(encoded);
        ASSERT_NOT_NULL(info);
        uint64_t size = file_info_size(info);
        file_info_free(info);
        ASSERT_TRUE(size > 8 && size < INT64_MAX);
        /* Include an empty stream, short header, partial body, and missing final byte. */
        const int64_t lengths[] = {0, 1, 7, (int64_t)(size / 2), (int64_t)size - 1};
        for (size_t cut = 0; cut < sizeof(lengths) / sizeof(lengths[0]); ++cut) {
            ASSERT_TRUE(file_copy(encoded, damaged, true));
            ASSERT_TRUE(file_truncate(damaged, lengths[cut]));
            ASSERT_TRUE(file_write_all_bytes(destination, saved, sizeof(saved)));
            ASSERT_TRUE(!decode(transform_modes[mode], damaged, destination, key, UINT64_MAX, true));
            ASSERT_EQ_INT(file_last_error(damaged), EBADMSG);
            ASSERT_TRUE(!file_is_open(damaged) && !file_is_open(destination));
            ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
            ASSERT_TRUE(directory_has_entries(directory, 4));
        }
        const int64_t offsets[] = {0, (int64_t)(size / 2), (int64_t)size - 1};
        for (size_t change = 0; change < sizeof(offsets) / sizeof(offsets[0]); ++change) {
            ASSERT_TRUE(file_copy(encoded, damaged, true));
            ASSERT_TRUE(flip_byte(damaged, offsets[change], FILE_SEEK_BEGIN));
            ASSERT_TRUE(!decode(transform_modes[mode], damaged, destination, key, UINT64_MAX, true));
            ASSERT_EQ_INT(file_last_error(damaged), EBADMSG);
            ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
            ASSERT_TRUE(directory_has_entries(directory, 4));
        }
        /* A second valid frame/stream is still forbidden trailing input. */
        ASSERT_TRUE(file_copy(encoded, damaged, true));
        ASSERT_TRUE(append_file(encoded, damaged));
        ASSERT_TRUE(!decode(transform_modes[mode], damaged, destination, key, UINT64_MAX, true));
        ASSERT_EQ_INT(file_last_error(damaged), EBADMSG);
        ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
        ASSERT_TRUE(directory_has_entries(directory, 4));
        ASSERT_TRUE(file_copy(encoded, damaged, true));
        ASSERT_TRUE(file_open(damaged, FILE_MODE_APPEND, FILE_ACCESS_WRITE));
        size_t written;
        ASSERT_TRUE(file_write(damaged, "x", 1, &written));
        ASSERT_EQ_LONG(written, 1);
        ASSERT_TRUE(file_close(damaged));
        ASSERT_TRUE(!decode(transform_modes[mode], damaged, destination, key, UINT64_MAX, true));
        ASSERT_EQ_INT(file_last_error(damaged), EBADMSG);
        ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
        ASSERT_TRUE(file_delete(destination));
        ASSERT_TRUE(!decode(transform_modes[mode], damaged, destination, key, UINT64_MAX, false));
        ASSERT_EQ_INT(file_last_error(damaged), EBADMSG);
        ASSERT_TRUE(!file_exists(destination));
        ASSERT_TRUE(directory_has_entries(directory, 3));
        /* A failed decode must not poison the handles for a later successful call. */
        ASSERT_TRUE(decode(transform_modes[mode], encoded, destination, key, sizeof(content), false));
        ASSERT_TRUE(has_bytes(destination, content, sizeof(content)));
        ASSERT_TRUE(file_delete(destination));
    }
    ASSERT_TRUE(has_bytes(source, content, sizeof(content)));
    ASSERT_TRUE(file_delete(damaged));
    ASSERT_TRUE(file_delete(encoded));
    ASSERT_TRUE(file_delete(source));
    file_key_free(key);
    file_free(directory);
    file_free(destination);
    file_free(damaged);
    file_free(encoded);
    file_free(source);
}

/* Limits apply to final plaintext, including expansion after authenticated decompression. */
void test_file_transforms_limits_and_destination_preservation(void)
{
    const size_t size = 256u * 1024u + 1u;
    unsigned char *bytes = calloc(size, 1);
    static const unsigned char saved[] = "original";
    file_t *source = transform_fixture("limit-source.bin");
    file_t *encoded = transform_fixture("limit-encoded.bin");
    file_t *destination = transform_fixture("limit-destination.bin");
    file_t *directory = file_new_cstr(test_case_temp_dir());
    file_key_t *key = file_key_generate();
    ASSERT_NOT_NULL(bytes);
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(encoded);
    ASSERT_NOT_NULL(destination);
    ASSERT_NOT_NULL(directory);
    ASSERT_NOT_NULL(key);
    ASSERT_TRUE(file_write_all_bytes(source, bytes, size));
    for (size_t mode = 0; mode < sizeof(transform_modes) / sizeof(transform_modes[0]); ++mode) {
        ASSERT_TRUE(file_write_all_bytes(encoded, saved, sizeof(saved)));
        ASSERT_TRUE(!encode(transform_modes[mode], source, encoded, key, false));
        ASSERT_EQ_INT(file_last_error(source), EEXIST);
        ASSERT_TRUE(has_bytes(encoded, saved, sizeof(saved)));
        ASSERT_TRUE(encode(transform_modes[mode], source, encoded, key, true));
        ASSERT_TRUE(file_write_all_bytes(destination, saved, sizeof(saved)));
        ASSERT_TRUE(!decode(transform_modes[mode], encoded, destination, key, size, false));
        ASSERT_EQ_INT(file_last_error(encoded), EEXIST);
        ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
        const uint64_t limits[] = {0, 1, size - 1};
        for (size_t limit = 0; limit < sizeof(limits) / sizeof(limits[0]); ++limit) {
            ASSERT_TRUE(!decode(transform_modes[mode], encoded, destination, key, limits[limit], true));
            ASSERT_EQ_INT(file_last_error(encoded), EFBIG);
            ASSERT_TRUE(!file_is_open(encoded) && !file_is_open(destination));
            ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
            ASSERT_TRUE(directory_has_entries(directory, 3));
        }
        ASSERT_TRUE(file_delete(destination));
        ASSERT_TRUE(!decode(transform_modes[mode], encoded, destination, key, size - 1, false));
        ASSERT_EQ_INT(file_last_error(encoded), EFBIG);
        ASSERT_TRUE(!file_exists(destination));
        ASSERT_TRUE(directory_has_entries(directory, 2));
        ASSERT_TRUE(file_write_all_bytes(destination, saved, sizeof(saved)));
        ASSERT_TRUE(decode(transform_modes[mode], encoded, destination, key, size, true));
        ASSERT_EQ_INT(file_last_error(encoded), 0);
        ASSERT_TRUE(has_bytes(destination, bytes, size));
        ASSERT_TRUE(has_bytes(source, bytes, size));
        ASSERT_TRUE(file_delete(destination));
    }
    ASSERT_TRUE(file_delete(encoded));
    ASSERT_TRUE(file_delete(source));
    free(bytes);
    file_key_free(key);
    file_free(directory);
    file_free(destination);
    file_free(encoded);
    file_free(source);
}

/* Imported keys interoperate; a deterministically different key cannot publish plaintext. */
void test_file_encryption_keys_and_wrong_key(void)
{
    unsigned char raw[32], exported[32];
    file_key_t *key = file_key_generate();
    ASSERT_NOT_NULL(key);
    ASSERT_TRUE(file_key_export(key, raw, sizeof(raw)));
    file_key_t *copy = file_key_from_bytes(raw, sizeof(raw));
    ASSERT_NOT_NULL(copy);
    ASSERT_TRUE(file_key_export(copy, exported, sizeof(exported)));
    ASSERT_TRUE(memcmp(raw, exported, sizeof(raw)) == 0);
    raw[0] ^= 1;
    file_key_t *wrong = file_key_from_bytes(raw, sizeof(raw));
    ASSERT_NOT_NULL(wrong);
    /* Mutating the import buffer must not change the imported key. */
    memset(raw, 0, sizeof(raw));
    ASSERT_TRUE(file_key_export(copy, raw, sizeof(raw)));
    ASSERT_TRUE(memcmp(raw, exported, sizeof(raw)) == 0);
    file_key_free(key);
    key = NULL;
    file_t *source = transform_fixture("key-source.txt");
    file_t *encoded = transform_fixture("key-encrypted.bin");
    file_t *destination = transform_fixture("key-restored.txt");
    file_t *directory = file_new_cstr(test_case_temp_dir());
    static const unsigned char content[] = "secret\0bytes";
    static const unsigned char saved[] = "preserved";
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(encoded);
    ASSERT_NOT_NULL(destination);
    ASSERT_NOT_NULL(directory);
    ASSERT_TRUE(file_write_all_bytes(source, content, sizeof(content)));
    key = file_key_from_bytes(exported, sizeof(exported));
    ASSERT_NOT_NULL(key);
    for (unsigned compressed = 0; compressed < 2; ++compressed) {
        ASSERT_TRUE(file_encrypt(source, encoded, copy, compressed != 0, true));
        ASSERT_TRUE(file_write_all_bytes(destination, saved, sizeof(saved)));
        ASSERT_TRUE(!file_decrypt(encoded, destination, wrong, UINT64_MAX, true));
        ASSERT_EQ_INT(file_last_error(encoded), EBADMSG);
        ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
        ASSERT_TRUE(directory_has_entries(directory, 3));
        ASSERT_TRUE(file_delete(destination));
        ASSERT_TRUE(!file_decrypt(encoded, destination, wrong, UINT64_MAX, false));
        ASSERT_EQ_INT(file_last_error(encoded), EBADMSG);
        ASSERT_TRUE(!file_exists(destination));
        ASSERT_TRUE(directory_has_entries(directory, 2));
        ASSERT_TRUE(file_decrypt(encoded, destination, key, sizeof(content), false));
        ASSERT_EQ_INT(file_last_error(encoded), 0);
        ASSERT_TRUE(has_bytes(destination, content, sizeof(content)));
    }
    ASSERT_TRUE(has_bytes(source, content, sizeof(content)));
    ASSERT_TRUE(file_delete(destination));
    ASSERT_TRUE(file_delete(encoded));
    ASSERT_TRUE(file_delete(source));
    memset(raw, 0, sizeof(raw));
    memset(exported, 0, sizeof(exported));
    file_key_free(wrong);
    file_key_free(copy);
    file_key_free(key);
    file_key_free(NULL);
    file_free(directory);
    file_free(destination);
    file_free(encoded);
    file_free(source);
}

/* Reject invalid key buffers before reading or writing any caller-owned bytes. */
void test_file_key_invalid_arguments(void)
{
    unsigned char bytes[33] = {0};
    unsigned char untouched[33];
    memset(untouched, 0xa5, sizeof(untouched));
    file_key_t *key = file_key_from_bytes(bytes, 32);
    ASSERT_NOT_NULL(key);
    const size_t sizes[] = {0, 1, 31, 33, SIZE_MAX};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        errno = 0;
        ASSERT_TRUE(file_key_from_bytes(bytes, sizes[i]) == NULL);
        ASSERT_EQ_INT(errno, EINVAL);
        memcpy(bytes, untouched, sizeof(bytes));
        errno = 0;
        ASSERT_TRUE(!file_key_export(key, bytes, sizes[i]));
        ASSERT_EQ_INT(errno, EINVAL);
        ASSERT_TRUE(memcmp(bytes, untouched, sizeof(bytes)) == 0);
    }
    errno = 0;
    ASSERT_TRUE(file_key_from_bytes(NULL, 32) == NULL);
    ASSERT_EQ_INT(errno, EINVAL);
    errno = 0;
    ASSERT_TRUE(!file_key_export(NULL, bytes, 32));
    ASSERT_EQ_INT(errno, EINVAL);
    ASSERT_TRUE(memcmp(bytes, untouched, sizeof(bytes)) == 0);
    errno = 0;
    ASSERT_TRUE(!file_key_export(key, NULL, 32));
    ASSERT_EQ_INT(errno, EINVAL);
    ASSERT_TRUE(file_key_export(key, bytes, 32));
    for (size_t i = 0; i < 32; ++i)
        ASSERT_EQ_INT(bytes[i], 0);
    ASSERT_EQ_INT(bytes[32], 0xa5);
    file_key_free(key);
}

/* Closed, distinct regular handles and a non-null encryption key are required. */
void test_file_transforms_invalid_arguments_and_aliases(void)
{
    static const unsigned char content[] = "unchanged";
    file_t *source = transform_fixture("invalid-source.bin");
    file_t *destination = transform_fixture("invalid-destination.bin");
    file_t *alias = transform_fixture("invalid-alias.bin");
    file_t *missing = transform_fixture("invalid-missing.bin");
    file_key_t *key = file_key_generate();
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(destination);
    ASSERT_NOT_NULL(alias);
    ASSERT_NOT_NULL(missing);
    ASSERT_NOT_NULL(key);
    ASSERT_TRUE(file_write_all_bytes(source, content, sizeof(content)));
    ASSERT_TRUE(file_write_all_bytes(destination, content, sizeof(content)));
    ASSERT_TRUE(file_create_hard_link(source, alias));
    file_t *same_path = file_new_cstr(file_path(source));
    ASSERT_NOT_NULL(same_path);
    file_t *const aliases[] = {source, same_path, alias};
    for (size_t mode = 0; mode < sizeof(transform_modes) / sizeof(transform_modes[0]); ++mode) {
        errno = 0;
        ASSERT_TRUE(!encode(transform_modes[mode], NULL, destination, key, true));
        ASSERT_EQ_INT(errno, EINVAL);
        errno = 0;
        ASSERT_TRUE(!decode(transform_modes[mode], NULL, destination, key, UINT64_MAX, true));
        ASSERT_EQ_INT(errno, EINVAL);
        ASSERT_TRUE(!encode(transform_modes[mode], source, NULL, key, true));
        ASSERT_EQ_INT(file_last_error(source), EINVAL);
        ASSERT_TRUE(!decode(transform_modes[mode], source, NULL, key, UINT64_MAX, true));
        ASSERT_EQ_INT(file_last_error(source), EINVAL);
        ASSERT_TRUE(!encode(transform_modes[mode], missing, destination, key, true));
        ASSERT_EQ_INT(file_last_error(missing), ENOENT);
        ASSERT_TRUE(!decode(transform_modes[mode], missing, destination, key, UINT64_MAX, true));
        ASSERT_EQ_INT(file_last_error(missing), ENOENT);
        for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
            ASSERT_TRUE(!encode(transform_modes[mode], source, aliases[i], key, true));
            ASSERT_EQ_INT(file_last_error(source), EINVAL);
            ASSERT_TRUE(!decode(transform_modes[mode], source, aliases[i], key, UINT64_MAX, true));
            ASSERT_EQ_INT(file_last_error(source), EINVAL);
            ASSERT_TRUE(has_bytes(source, content, sizeof(content)));
        }
        ASSERT_TRUE(file_open_read(source));
        ASSERT_TRUE(!encode(transform_modes[mode], source, destination, key, true));
        ASSERT_EQ_INT(file_last_error(source), EBUSY);
        ASSERT_TRUE(!decode(transform_modes[mode], source, destination, key, UINT64_MAX, true));
        ASSERT_EQ_INT(file_last_error(source), EBUSY);
        ASSERT_TRUE(file_is_open(source));
        ASSERT_TRUE(file_close(source));
        ASSERT_TRUE(file_open_read(destination));
        ASSERT_TRUE(!encode(transform_modes[mode], source, destination, key, true));
        ASSERT_EQ_INT(file_last_error(source), EBUSY);
        ASSERT_TRUE(!decode(transform_modes[mode], source, destination, key, UINT64_MAX, true));
        ASSERT_EQ_INT(file_last_error(source), EBUSY);
        ASSERT_TRUE(file_is_open(destination) && !file_is_open(source));
        ASSERT_TRUE(file_close(destination));
        ASSERT_TRUE(has_bytes(destination, content, sizeof(content)));
    }
    for (unsigned compressed = 0; compressed < 2; ++compressed) {
        ASSERT_TRUE(!file_encrypt(source, destination, NULL, compressed != 0, true));
        ASSERT_EQ_INT(file_last_error(source), EINVAL);
    }
    ASSERT_TRUE(!file_decrypt(source, destination, NULL, UINT64_MAX, true));
    ASSERT_EQ_INT(file_last_error(source), EINVAL);
    ASSERT_TRUE(has_bytes(source, content, sizeof(content)));
    ASSERT_TRUE(has_bytes(destination, content, sizeof(content)));
    ASSERT_TRUE(!file_exists(missing));
    ASSERT_TRUE(file_delete(alias));
    ASSERT_TRUE(file_delete(destination));
    ASSERT_TRUE(file_delete(source));
    file_key_free(key);
    file_free(same_path);
    file_free(missing);
    file_free(alias);
    file_free(destination);
    file_free(source);
}

enum { WIRE_HEADER_SIZE = 40, WIRE_PAYLOAD_MAX = 256, WIRE_RECORD_MAX = 8 };

typedef struct {
    const unsigned char *bytes;
    size_t size;
    unsigned char tag;
} wire_record_t;

/* Authenticate each original record before optionally duplicating or reordering its wire bytes. */
static bool write_authenticated_wire(file_t *file, const file_key_t *key, bool compressed,
                                     const wire_record_t *records, size_t count,
                                     const size_t *order, size_t order_count)
{
    unsigned char associated[WIRE_HEADER_SIZE + 4] = {'M', 'A', 'R', 'S', 'E', 'N', 'C', 1};
    unsigned char raw_key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    unsigned char wire[WIRE_RECORD_MAX][4 + WIRE_PAYLOAD_MAX + crypto_secretstream_xchacha20poly1305_ABYTES];
    size_t lengths[WIRE_RECORD_MAX];
    crypto_secretstream_xchacha20poly1305_state state;
    if (count > WIRE_RECORD_MAX || (order && order_count > WIRE_RECORD_MAX))
        return false;
    associated[8] = compressed ? 1 : 0;
    bool ok = file_key_export(key, raw_key, sizeof(raw_key));
    if (ok)
        ok = crypto_secretstream_xchacha20poly1305_init_push(&state, associated + 16, raw_key) == 0;
    for (size_t i = 0; ok && i < count; ++i) {
        if (records[i].size > WIRE_PAYLOAD_MAX) {
            ok = false;
            break;
        }
        size_t cipher_size = records[i].size + crypto_secretstream_xchacha20poly1305_ABYTES;
        /* The four-byte record length is big-endian and part of the associated data. */
        for (size_t byte = 0; byte < 4; ++byte)
            associated[WIRE_HEADER_SIZE + byte] = (unsigned char)(cipher_size >> (24 - 8 * byte));
        memcpy(wire[i], associated + WIRE_HEADER_SIZE, 4);
        unsigned long long produced = 0;
        ok = crypto_secretstream_xchacha20poly1305_push(&state, wire[i] + 4, &produced,
            records[i].bytes, records[i].size, associated, sizeof(associated), records[i].tag) == 0;
        ok = ok && produced == cipher_size;
        lengths[i] = 4 + cipher_size;
    }
    if (ok)
        ok = file_create(file);
    if (ok) {
        size_t written;
        ok = file_write(file, associated, WIRE_HEADER_SIZE, &written) && written == WIRE_HEADER_SIZE;
        size_t emitted = order ? order_count : count;
        for (size_t i = 0; ok && i < emitted; ++i) {
            size_t index = order ? order[i] : i;
            ok = index < count && file_write(file, wire[index], lengths[index], &written);
            if (ok)
                ok = written == lengths[index];
        }
    }
    bool closed = file_close(file);
    sodium_memzero(raw_key, sizeof(raw_key));
    sodium_memzero(&state, sizeof(state));
    sodium_memzero(wire, sizeof(wire));
    return ok && closed;
}

static void expect_wire_rejection(const char *name, file_t *wire, file_t *destination, file_t *directory,
                                  const file_key_t *key, int error)
{
    static const unsigned char saved[] = "preserve authenticated failure destination";
    ASSERT_TRUE(file_write_all_bytes(destination, saved, sizeof(saved)));
    TEST_ASSERT_FALSE(file_decrypt(wire, destination, key, 1024, true), name);
    ASSERT_EQ_INT(file_last_error(wire), error);
    ASSERT_TRUE(!file_is_open(wire) && !file_is_open(destination));
    ASSERT_TRUE(has_bytes(destination, saved, sizeof(saved)));
    ASSERT_TRUE(directory_has_entries(directory, 4));
    ASSERT_TRUE(file_delete(destination));
    TEST_ASSERT_FALSE(file_decrypt(wire, destination, key, 1024, false), name);
    ASSERT_EQ_INT(file_last_error(wire), error);
    ASSERT_TRUE(!file_exists(destination));
    ASSERT_TRUE(directory_has_entries(directory, 3));
}

/* Exercise authenticated protocol violations separately from accidental ciphertext corruption. */
void test_file_transform_wire_validation(void)
{
    static const unsigned char payload[] = {'a', 0, 'b', 0xff, 'c'};
    /* A non-single-segment frame with a 16 MiB window and a final empty raw block. */
    static const unsigned char large_window[] = {0x28, 0xb5, 0x2f, 0xfd, 0x00, 0x70, 0x01, 0x00, 0x00};
    /* Reserved block type 3 in an otherwise ordinary single-segment empty frame. */
    static const unsigned char invalid_block[] = {0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x00, 0x07, 0x00, 0x00};
    static const unsigned char skippable[] = {0x50, 0x2a, 0x4d, 0x18, 0x00, 0x00, 0x00, 0x00};
    file_t *source = transform_fixture("wire-source.bin");
    file_t *frame_file = transform_fixture("wire-frame.zst");
    file_t *wire = transform_fixture("wire-encrypted.bin");
    file_t *destination = transform_fixture("wire-destination.bin");
    file_t *directory = file_new_cstr(test_case_temp_dir());
    file_key_t *key = file_key_generate();
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(frame_file);
    ASSERT_NOT_NULL(wire);
    ASSERT_NOT_NULL(destination);
    ASSERT_NOT_NULL(directory);
    ASSERT_NOT_NULL(key);
    ASSERT_TRUE(file_write_all_bytes(source, payload, sizeof(payload)));
    ASSERT_TRUE(file_compress_zstd(source, frame_file, false));
    unsigned char frame[WIRE_PAYLOAD_MAX];
    size_t frame_size, remaining;
    ASSERT_TRUE(file_open_read(frame_file));
    ASSERT_TRUE(file_read(frame_file, frame, sizeof(frame), &frame_size));
    ASSERT_TRUE(frame_size > 4 && frame_size < sizeof(frame));
    unsigned char extra;
    ASSERT_TRUE(file_read(frame_file, &extra, 1, &remaining));
    ASSERT_EQ_LONG(remaining, 0);
    ASSERT_TRUE(file_close(frame_file));
    const unsigned char message = crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;
    const unsigned char final = crypto_secretstream_xchacha20poly1305_TAG_FINAL;
    const unsigned char push = crypto_secretstream_xchacha20poly1305_TAG_PUSH;
    const unsigned char rekey = crypto_secretstream_xchacha20poly1305_TAG_REKEY;
    const struct {
        const char *name;
        bool compressed;
        wire_record_t records[3];
        size_t count;
        int error;
    } cases[] = {
        {"TAG_PUSH", false, {{payload, 1, push}, {NULL, 0, final}}, 2, EBADMSG},
        {"TAG_REKEY", false, {{payload, 1, rekey}, {NULL, 0, final}}, 2, EBADMSG},
        {"FINAL with data", false, {{payload, 1, final}}, 1, EBADMSG},
        {"empty MESSAGE", false, {{NULL, 0, message}, {NULL, 0, final}}, 2, EBADMSG},
        {"invalid Zstandard magic", true, {{payload, 4, message}, {NULL, 0, final}}, 2, EBADMSG},
        {"invalid Zstandard block", true, {{invalid_block, sizeof(invalid_block), message}, {NULL, 0, final}}, 2, EBADMSG},
        {"truncated Zstandard magic", true, {{frame, 3, message}, {NULL, 0, final}}, 2, EBADMSG},
        {"truncated Zstandard frame", true, {{frame, frame_size - 1, message}, {NULL, 0, final}}, 2, EBADMSG},
        {"missing compressed frame", true, {{NULL, 0, final}}, 1, EBADMSG},
        {"oversized Zstandard window", true, {{large_window, sizeof(large_window), message}, {NULL, 0, final}}, 2, EFBIG},
        {"skippable frame", true, {{skippable, sizeof(skippable), message}, {NULL, 0, final}}, 2, EBADMSG},
        {"second frame in next record", true,
            {{frame, frame_size, message}, {frame, frame_size, message}, {NULL, 0, final}}, 3, EBADMSG},
        {"skippable frame in next record", true,
            {{frame, frame_size, message}, {skippable, sizeof(skippable), message}, {NULL, 0, final}}, 3, EBADMSG}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ASSERT_TRUE(write_authenticated_wire(wire, key, cases[i].compressed, cases[i].records, cases[i].count, NULL, 0));
        expect_wire_rejection(cases[i].name, wire, destination, directory, key, cases[i].error);
    }

    /* Each of the four magic bytes arrives in a separate authenticated record. */
    const wire_record_t fragmented[] = {
        {frame, 1, message},
        {frame + 1, 1, message},
        {frame + 2, 1, message},
        {frame + 3, 1, message},
        {frame + 4, frame_size - 4, message},
        {NULL, 0, final}
    };
    ASSERT_TRUE(write_authenticated_wire(wire, key, true, fragmented, 6, NULL, 0));
    ASSERT_TRUE(file_decrypt(wire, destination, key, sizeof(payload), false));
    ASSERT_EQ_INT(file_last_error(wire), 0);
    ASSERT_TRUE(has_bytes(destination, payload, sizeof(payload)));
    ASSERT_TRUE(has_private_permissions(destination));
    ASSERT_TRUE(file_delete(destination));

    const wire_record_t ordered[] = {
        {payload, 1, message},
        {payload + 1, 1, message},
        {payload + 2, sizeof(payload) - 2, message},
        {NULL, 0, final}
    };
    ASSERT_TRUE(write_authenticated_wire(wire, key, false, ordered, 4, NULL, 0));
    ASSERT_TRUE(file_decrypt(wire, destination, key, sizeof(payload), false));
    ASSERT_TRUE(has_bytes(destination, payload, sizeof(payload)));
    ASSERT_TRUE(file_delete(destination));
    const struct {
        const char *name;
        size_t order[5];
        size_t count;
    } sequences[] = {
        {"duplicated record", {0, 0, 1, 2, 3}, 5},
        {"reordered records", {0, 2, 1, 3}, 4},
        {"missing final record", {0, 1, 2}, 3}
    };
    for (size_t i = 0; i < sizeof(sequences) / sizeof(sequences[0]); ++i) {
        ASSERT_TRUE(write_authenticated_wire(wire, key, false, ordered, 4, sequences[i].order, sequences[i].count));
        expect_wire_rejection(sequences[i].name, wire, destination, directory, key, EBADMSG);
    }

    /* Exercise the standalone decoder's window limit and forbidden skippable magic too. */
    ASSERT_TRUE(file_write_all_bytes(frame_file, large_window, sizeof(large_window)));
    ASSERT_TRUE(!file_decompress_zstd(frame_file, destination, UINT64_MAX, false));
    ASSERT_EQ_INT(file_last_error(frame_file), EFBIG);
    ASSERT_TRUE(!file_exists(destination));
    ASSERT_TRUE(directory_has_entries(directory, 3));
    unsigned char allowed_window[sizeof(large_window)];
    memcpy(allowed_window, large_window, sizeof(allowed_window));
    allowed_window[5] = 0x68; /* Exactly 8 MiB: the same empty frame must be accepted. */
    ASSERT_TRUE(file_write_all_bytes(frame_file, allowed_window, sizeof(allowed_window)));
    ASSERT_TRUE(file_decompress_zstd(frame_file, destination, 0, false));
    ASSERT_TRUE(has_bytes(destination, NULL, 0));
    ASSERT_TRUE(file_delete(destination));
    ASSERT_TRUE(file_write_all_bytes(frame_file, skippable, sizeof(skippable)));
    ASSERT_TRUE(!file_decompress_zstd(frame_file, destination, UINT64_MAX, false));
    ASSERT_EQ_INT(file_last_error(frame_file), EBADMSG);
    ASSERT_TRUE(!file_exists(destination));
    ASSERT_TRUE(directory_has_entries(directory, 3));
    ASSERT_TRUE(file_delete(wire));
    ASSERT_TRUE(file_delete(frame_file));
    ASSERT_TRUE(file_delete(source));
    file_key_free(key);
    file_free(directory);
    file_free(destination);
    file_free(wire);
    file_free(frame_file);
    file_free(source);
}

/* README example: compression and decompression with Zstandard. */
void example_file_compression_round_trip(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("compress-source.txt"));
    file_t *compressed = file_new_cstr(test_case_temp_path("source.zst"));
    file_t *restored = file_new_cstr(test_case_temp_path("decompressed.txt"));
    ASSERT_TRUE(source && compressed && restored);
    ASSERT_TRUE(file_write_all_bytes(source, "sky sky sky\n", 12));
    /* false refuses to overwrite an existing destination. */
    ASSERT_TRUE(file_compress_zstd(source, compressed, false));
    /* Refuse decompressed output larger than 1024 bytes. */
    ASSERT_TRUE(file_decompress_zstd(compressed, restored, 1024, false));
    string_t *text = file_read_all_text(restored);
    ASSERT_TRUE(text);
    TEST_ASSERT_STR_EQ(string_c_str(text), "sky sky sky\n");
    printf("decompressed: %s", string_c_str(text));
    ASSERT_TRUE(file_delete(restored));
    ASSERT_TRUE(file_delete(compressed));
    ASSERT_TRUE(file_delete(source));
    string_free(text);
    file_free(restored);
    file_free(compressed);
    file_free(source);
}

/* README example: encryption and decryption without compression. */
void example_file_encryption_only(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("private.txt"));
    file_t *encrypted = file_new_cstr(test_case_temp_path("private.enc"));
    file_t *restored = file_new_cstr(test_case_temp_path("decrypted.txt"));
    file_key_t *key = file_key_generate();
    ASSERT_TRUE(source && encrypted && restored && key);
    ASSERT_TRUE(file_write_all_bytes(source, "Private notes.\n", 15));
    /* First false disables compression; second false refuses overwriting. */
    ASSERT_TRUE(file_encrypt(source, encrypted, key, false, false));
    /* Decrypt and authenticate, allowing at most 1024 plaintext bytes. */
    ASSERT_TRUE(file_decrypt(encrypted, restored, key, 1024, false));
    string_t *text = file_read_all_text(restored);
    ASSERT_TRUE(text);
    TEST_ASSERT_STR_EQ(string_c_str(text), "Private notes.\n");
    printf("decrypted: %s", string_c_str(text));
    ASSERT_TRUE(file_delete(restored));
    ASSERT_TRUE(file_delete(encrypted));
    ASSERT_TRUE(file_delete(source));
    string_free(text);
    file_key_free(key);
    file_free(restored);
    file_free(encrypted);
    file_free(source);
}

/* README example: compressed encryption round trip. Register after all ordinary tests.
 * Expected output: restored: Meet at noon.\n */
void example_file_encrypted_round_trip(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("message.txt"));
    file_t *encrypted = file_new_cstr(test_case_temp_path("message.enc"));
    file_t *restored = file_new_cstr(test_case_temp_path("restored.txt"));
    file_key_t *key = file_key_generate();
    string_t *message = string_new_with("Meet at noon.");
    ASSERT_NOT_NULL(source);
    ASSERT_NOT_NULL(encrypted);
    ASSERT_NOT_NULL(restored);
    ASSERT_NOT_NULL(key);
    ASSERT_NOT_NULL(message);
    ASSERT_TRUE(file_write_all_text(source, message));
    ASSERT_TRUE(file_encrypt(source, encrypted, key, true, false));
    ASSERT_TRUE(file_decrypt(encrypted, restored, key, 1024, false));
    string_t *text = file_read_all_text(restored);
    ASSERT_NOT_NULL(text);
    TEST_ASSERT_STR_EQ(string_c_str(text), "Meet at noon.");
    printf("restored: %s\n", string_c_str(text));
    ASSERT_TRUE(file_delete(restored));
    ASSERT_TRUE(file_delete(encrypted));
    ASSERT_TRUE(file_delete(source));
    string_free(text);
    string_free(message);
    file_key_free(key);
    file_free(restored);
    file_free(encrypted);
    file_free(source);
}
