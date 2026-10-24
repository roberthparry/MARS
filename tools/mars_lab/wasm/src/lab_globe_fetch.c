/**
 * @file lab_globe_fetch.c
 * @brief Native asynchronous HTTP response ownership for the shoreline projection worker.
 *
 * Uses the MARS HTTP receiver and allocation-free Protobuf wire primitives. Requests contain native binary64
 * bounds. Responses are parsed directly from receiver-owned memory, without creating JavaScript values or
 * copying the geometry into another transport buffer. A small strict schema admits only successful shoreline
 * replies, skipping unrelated response metadata without constructing it. Malformed, cancelled, oversized and
 * stale replies leave the published polygon cache unchanged.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "http-t.h"
#include "protobuf_wire-t.h"
#include "lab_browser.h"
#include "lab_globe_fetch.h"
#define MARS_LAB_GLOBE_SHORE_INTERNAL_ACCESS
#include "lab_globe_shore_internal.h"

typedef struct _shore_fetch_t {
    http_receive_t *response;
    unsigned token, sequence, request_size;
    unsigned char request[256];
    double bounds[4];
} shore_fetch_t;

static shore_fetch_t transfer;

/* Fixed small request fields have a single-byte length; bounds retain their original binary64 bits. */
static size_t shore_request_entry(unsigned char *out, const char *key, const double *number)
{
    size_t key_size = strlen(key), value_size = 11;
    unsigned char *start = out;
    *out++ = 50; *out++ = (unsigned char)(2 + key_size + 2 + value_size);
    *out++ = 10; *out++ = (unsigned char)key_size;
    memcpy(out, key, key_size); out += key_size;
    *out++ = 18; *out++ = (unsigned char)value_size;
    *out++ = 8; *out++ = number ? 2 : 3;
    if (number) {
        uint64_t bits;
        memcpy(&bits, number, sizeof bits);
        *out++ = 25;
        size_t offset = 0;
        if (!protobuf_wire_write_fixed(out, 8, &offset, PROTOBUF_FIXED64, bits)) return 0;
        out += 8;
    } else {
        *out++ = 34; *out++ = 7;
        memcpy(out, "vectors", 7); out += 7;
    }
    return (size_t)(out - start);
}

/* Retire storage without changing the successfully admitted geometry. */
void lab_globe_fetch_cancel(unsigned token)
{
    if (!token || token != transfer.token) return;
    http_receive_fail(transfer.response, HTTP_ERROR_CANCELLED);
    http_receive_free(transfer.response);
    transfer.response = NULL;
    transfer.token = transfer.request_size = 0;
}

/* Construct the request natively; JavaScript supplies only camera bounds, not encoded coordinate text. */
unsigned lab_globe_fetch_begin(double west, double south, double east, double north)
{
    lab_globe_fetch_cancel(transfer.token);
    if (!isfinite(west) || !isfinite(south) || !isfinite(east) || !isfinite(north) ||
        east <= west || east - west > 360 || south < -90 || north > 90 || north <= south) return 0;
    transfer.response = http_receive_new(lab_browser_limit);
    if (!transfer.response) return 0;
    transfer.bounds[0] = west; transfer.bounds[1] = south;
    transfer.bounds[2] = east; transfer.bounds[3] = north;
    unsigned char root[128] = {8, 5};
    size_t used = 2;
    static const char *const keys[] = {"west", "south", "east", "north", "format"};
    for (unsigned i = 0; i < 5; ++i)
        used += shore_request_entry(root + used, keys[i], i < 4 ? &transfer.bounds[i] : NULL);
    size_t position = 0;
    bool ok = protobuf_wire_write_varint(transfer.request, sizeof transfer.request, &position, 8) &&
        protobuf_wire_write_varint(transfer.request, sizeof transfer.request, &position, 1) &&
        protobuf_wire_write_varint(transfer.request, sizeof transfer.request, &position, 18) &&
        protobuf_wire_write_varint(transfer.request, sizeof transfer.request, &position, used);
    if (!ok || used > sizeof transfer.request - position) {
        http_receive_free(transfer.response); transfer.response = NULL; return 0;
    }
    memcpy(transfer.request + position, root, used);
    transfer.request_size = (unsigned)(position + used);
    /* Never reuse a token in one worker lifetime, including after unsigned wrap. */
    if (transfer.sequence == UINT32_MAX) {
        http_receive_free(transfer.response); transfer.response = NULL; transfer.request_size = 0; return 0;
    }
    transfer.token = ++transfer.sequence;
    return transfer.token;
}

/* Borrow request bytes only for an active request. */
const unsigned char *lab_globe_fetch_request(unsigned token)
{
    return token && token == transfer.token ? transfer.request : NULL;
}

/* Expose the bounded native request length. */
unsigned lab_globe_fetch_request_size(unsigned token)
{
    return token && token == transfer.token ? transfer.request_size : 0;
}

/* Reserve directly in the response owner, without a second input buffer. */
unsigned char *lab_globe_fetch_reserve(unsigned token, unsigned size)
{
    return token && token == transfer.token ? http_receive_reserve(transfer.response, size) : NULL;
}

/* Accept a complete chunk only for the active request. */
bool lab_globe_fetch_commit(unsigned token, unsigned size)
{
    return token && token == transfer.token && http_receive_commit(transfer.response, size);
}

/* Parse the few singular fields of one strict response-schema message, skipping unknown protobuf tags.
 * This is bounded schema lookup, not a scan over coordinate records. */
static bool shore_fields(const unsigned char *data, size_t size, protobuf_wire_field_t fields[8],
                         unsigned required, unsigned known)
{
    size_t offset = 0;
    unsigned seen = 0, count = 0;
    while (offset < size) {
        protobuf_wire_field_t field;
        if (++count > 128 || !protobuf_wire_read_field(data, size, &offset, &field)) return false;
        if (field.field > 7) continue;
        unsigned bit = 1u << field.field;
        if (!(known & bit)) continue;
        if (!(required & bit) || (seen & bit)) return false;
        seen |= bit;
        fields[field.field] = field;
    }
    return seen == required;
}

static bool shore_response_entry(const protobuf_wire_field_t *entry, unsigned *seen,
                                 protobuf_wire_field_t *geometry)
{
    protobuf_wire_field_t fields[8] = {0}, value[8] = {0};
    if (!shore_fields(entry->data, entry->size, fields, 6, 6) ||
        fields[1].wire != PROTOBUF_BYTES || fields[2].wire != PROTOBUF_BYTES) return false;
    unsigned member = fields[1].size == 2 && !memcmp(fields[1].data, "ok", 2) ? 1 :
        fields[1].size == 7 && !memcmp(fields[1].data, "vectors", 7) ? 2 : 0;
    /* The shared server adapter adds presentation metadata to successful replies. Its length-delimited
     * value is irrelevant here: skip it without decoding host values, but still require both owned members. */
    if (!member) return true;
    if ((*seen & member) ||
        !shore_fields(fields[2].data, fields[2].size, value, member == 1 ? 6 : 130, 254) ||
        value[1].wire != PROTOBUF_VARINT || value[1].integer != (member == 1 ? 1u : 6u)) return false;
    if (member == 1 && (value[2].wire != PROTOBUF_VARINT || value[2].integer != 1)) return false;
    if (member == 2) {
        if (value[7].wire != PROTOBUF_BYTES) return false;
        *geometry = value[7];
    }
    *seen |= member;
    return true;
}

static bool shore_response(const unsigned char *bytes, size_t length)
{
    protobuf_wire_field_t envelope[8] = {0}, geometry = {0};
    if (!shore_fields(bytes, length, envelope, 6, 6) || envelope[1].wire != PROTOBUF_VARINT ||
        envelope[1].integer != 1 || envelope[2].wire != PROTOBUF_BYTES) return false;
    unsigned seen = 0, count = 0;
    bool kind = false;
    size_t offset = 0;
    while (offset < envelope[2].size) {
        protobuf_wire_field_t field;
        if (++count > 128 || !protobuf_wire_read_field(envelope[2].data, envelope[2].size, &offset, &field)) return false;
        if (field.field == 1) {
            if (kind || field.wire != PROTOBUF_VARINT || field.integer != 5) return false;
            kind = true;
        } else if (field.field == 6) {
            if (field.wire != PROTOBUF_BYTES || !shore_response_entry(&field, &seen, &geometry)) return false;
        } else if (field.field <= 7) return false;
    }
    return kind && seen == 3 && lab_globe_shore_admit(geometry.data, geometry.size, transfer.bounds);
}

/* Decode directly from owned response memory, then release it after atomic geometry admission. */
bool lab_globe_fetch_finish(unsigned token, unsigned status)
{
    if (!token || token != transfer.token) return false;
    bool ok = http_receive_finish(transfer.response, status) && status >= 200 && status < 300 &&
        shore_response(http_receive_data(transfer.response), http_receive_size(transfer.response));
    lab_globe_fetch_cancel(token);
    return ok;
}
