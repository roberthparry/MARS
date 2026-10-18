/**
 * @file lab_browser_storage.c
 * @brief Bounded base64 storage envelopes for the browser's Protobuf codec.
 *
 * Local storage requires text rather than bytes. These freestanding operations preserve
 * the existing padded base64 format while keeping conversion and validation in C.
 * Encoding reuses the codec input buffer; decoding contracts that buffer in place.
 * No allocation or browser base64 facilities are required. The host copies output and
 * clears staged input before releasing the codec's synchronous operation scope.
 */
#include <stdint.h>

#include "lab_browser.h"

static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int digit(unsigned char value)
{
    if (value >= 'A' && value <= 'Z')
        return value - 'A';
    if (value >= 'a' && value <= 'z')
        return value - 'a' + 26;
    if (value >= '0' && value <= '9')
        return value - '0' + 52;
    if (value == '+')
        return 62;
    if (value == '/')
        return 63;
    return -1;
}

/* Encode the existing Protobuf envelope, then copy its base64 representation into shared input storage. */
uint32_t lab_browser_storage_encode(codec_jsref root)
{
    uint32_t size = lab_browser_encode(root);
    if (!size)
        return 0;
    const unsigned char *bytes = lab_browser_output_buffer();
    unsigned char *text = lab_browser_input_buffer();
    uint32_t used = 0;
    for (uint32_t offset = 0; offset < size; offset += 3) {
        uint32_t value = (uint32_t)bytes[offset] << 16;
        if (offset + 1 < size)
            value |= (uint32_t)bytes[offset + 1] << 8;
        if (offset + 2 < size)
            value |= bytes[offset + 2];
        text[used++] = alphabet[value >> 18];
        text[used++] = alphabet[(value >> 12) & 63];
        text[used++] = offset + 1 < size ? alphabet[(value >> 6) & 63] : '=';
        text[used++] = offset + 2 < size ? alphabet[value & 63] : '=';
    }
    return used;
}

/* Validate canonical base64 and its decoded size before allowing the Protobuf parser to consume it. */
codec_jsref lab_browser_storage_decode(uint32_t size)
{
    if (!size || size > lab_browser_storage_limit || size % 4)
        return -1;
    unsigned char *bytes = lab_browser_input_buffer();
    uint32_t padding = (bytes[size - 1] == '=') + (bytes[size - 2] == '=');
    uint32_t length = size / 4 * 3 - padding;
    if (length > lab_browser_limit)
        return -1;
    uint32_t used = 0;
    for (uint32_t offset = 0; offset < size; offset += 4) {
        int a = digit(bytes[offset]), b = digit(bytes[offset + 1]);
        int c = digit(bytes[offset + 2]), d = digit(bytes[offset + 3]);
        int last = offset + 4 == size;
        if (a < 0 || b < 0)
            return -1;
        if (last && padding == 2) {
            if (bytes[offset + 2] != '=' || bytes[offset + 3] != '=' || (b & 15))
                return -1;
            c = d = 0;
        } else if (last && padding == 1) {
            if (bytes[offset + 3] != '=' || c < 0 || (c & 3))
                return -1;
            d = 0;
        } else if (c < 0 || d < 0) {
            return -1;
        }
        uint32_t value = (uint32_t)a << 18 | (uint32_t)b << 12 | (uint32_t)c << 6 | (uint32_t)d;
        bytes[used++] = value >> 16;
        if (used < length)
            bytes[used++] = value >> 8;
        if (used < length)
            bytes[used++] = value;
    }
    return lab_browser_decode(length);
}
