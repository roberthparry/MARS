/**
 * @file test_lab_mobile.c
 * @brief Native Lab QR geometry and private mobile metadata regressions.
 *
 * Checks unsupported input and byte-capacity boundaries, then compares every QR
 * module with a fixed fixture extracted from the legacy Python encoder. Metadata
 * cases isolate bind-environment changes in the suite's child fixture and use
 * only loopback or malformed hosts: no Tailscale command, network probe or sharing
 * configuration change is required. These are ordinary sequential suite tests.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>

#include "lab_mobile.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool empty_qr(const char *input)
{
    string_t *url = input ? string_new_with(input) : NULL;
    string_t *svg = input && !url ? NULL : lab_mobile_qr(url);
    bool ok = svg && !string_byte_length(svg);
    string_free(svg);
    string_free(url);
    return ok;
}

static void test_lab_mobile_qr_unsupported(void)
{
    TEST_ASSERT_TRUE(empty_qr(NULL) && empty_qr(""), "null and empty QR input yield owned empty strings");
    TEST_ASSERT_TRUE(empty_qr("http://example.invalid/caf\xc3\xa9") && empty_qr("http://example.invalid/\xe2\x98\x83"),
                     "non-ASCII QR input is rejected without emitting partial SVG");
    TEST_ASSERT_TRUE(empty_qr("http://example.invalid/a b") && empty_qr("http://example.invalid/\n"),
                     "spaces and control characters are outside the ASCII URL contract");
}

static void test_lab_mobile_qr_capacity(void)
{
    string_t *url = string_new();
    bool prepared = url != NULL;
    for (unsigned i = 0; prepared && i < 106; ++i)
        prepared = string_append_char(url, 'a') == 0;
    string_t *exact = prepared ? lab_mobile_qr(url) : NULL;
    bool accepted = exact && string_starts_with(exact, "<svg ") && string_ends_with(exact, "</svg>");
    bool extended = prepared && string_append_char(url, 'a') == 0;
    string_t *over = extended ? lab_mobile_qr(url) : NULL;
    bool rejected = over && !string_byte_length(over);
    string_free(over);
    string_free(exact);
    string_free(url);
    TEST_ASSERT_TRUE(accepted && rejected, "QR version 5/L accepts 106 ASCII bytes and rejects 107");
}

static bool coordinate(string_cursor_t *cursor, unsigned *out)
{
    unsigned value = 0, digits = 0;
    unsigned char ch;
    while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9') {
        if (++digits > 2)
            return false;
        value = value * 10 + ch - '0';
        if (string_cursor_next(cursor))
            return false;
    }
    if (!digits || value < 4 || value > 40)
        return false;
    *out = value - 4;
    return true;
}

static bool geometry_matches(const string_t *svg)
{
    /* Reference QR matrix for "http://127.0.0.1:8765/".
     * Bit x represents column x before the four-module SVG quiet-zone offset.
     * This fixture covers payload, padding, Reed–Solomon parity, mask and format bits. */
    static const uint64_t expected[37] = {
        [0] = UINT64_C(0x1fd333327f),  [1] = UINT64_C(0x105ddddc41),  [2] = UINT64_C(0x175777775d),
        [3] = UINT64_C(0x174eeeee5d),  [4] = UINT64_C(0x174444445d),  [5] = UINT64_C(0x1042222241),
        [6] = UINT64_C(0x1fd555557f),  [7] = UINT64_C(0x0017777700),  [8] = UINT64_C(0x046ccccdf7),
        [9] = UINT64_C(0x1ccccccc0c),  [10] = UINT64_C(0x1362222158), [11] = UINT64_C(0x1bc8888c16),
        [12] = UINT64_C(0x00d1111478), [13] = UINT64_C(0x14dbbbbeaa), [14] = UINT64_C(0x127ddddc73),
        [15] = UINT64_C(0x0bf77771b8), [16] = UINT64_C(0x12ccccc875), [17] = UINT64_C(0x12ccccca9b),
        [18] = UINT64_C(0x1fe2222441), [19] = UINT64_C(0x0a88888d1a), [20] = UINT64_C(0x1ad11116d2),
        [21] = UINT64_C(0x12fbbbbd27), [22] = UINT64_C(0x183ddddb78), [23] = UINT64_C(0x0b5777759d),
        [24] = UINT64_C(0x1a4ccccb70), [25] = UINT64_C(0x10acccca3a), [26] = UINT64_C(0x1b222222c9),
        [27] = UINT64_C(0x0ae8888a1e), [28] = UINT64_C(0x01f11110ed), [29] = UINT64_C(0x1d1bbbbd00),
        [30] = UINT64_C(0x1955dddd7f), [31] = UINT64_C(0x1b17777141), [32] = UINT64_C(0x1bfcccc95d),
        [33] = UINT64_C(0x035cccca5d), [34] = UINT64_C(0x154a22275d), [35] = UINT64_C(0x0ba0888d41),
        [36] = UINT64_C(0x18f111137f),
    };
    uint64_t actual[37] = {0};
    string_cursor_t *cursor = svg ? string_cursor_new(svg) : NULL;
    bool ok = cursor && string_cursor_consume(
                            cursor, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 45 45\" "
                                    "role=\"img\" aria-label=\"Mobile access QR code\">"
                                    "<rect width=\"45\" height=\"45\" fill=\"#fff\"/><path fill=\"#0f2f5f\" d=\"");
    while (ok && !string_cursor_match(cursor, "\"/></svg>")) {
        unsigned x, y;
        ok = string_cursor_consume(cursor, "M") && coordinate(cursor, &x) && string_cursor_consume(cursor, ",") &&
             coordinate(cursor, &y) && string_cursor_consume(cursor, "h1v1h-1z");
        if (ok) {
            uint64_t bit = UINT64_C(1) << x;
            ok = !(actual[y] & bit);
            actual[y] |= bit;
        }
    }
    ok = ok && string_cursor_consume(cursor, "\"/></svg>") && string_cursor_done(cursor);
    string_cursor_free(cursor);
    for (size_t row = 0; ok && row < sizeof expected / sizeof *expected; ++row)
        ok = actual[row] == expected[row];
    return ok;
}

static void test_lab_mobile_qr_geometry(void)
{
    string_t *url = string_new_with("http://127.0.0.1:8765/");
    string_t *first = url ? lab_mobile_qr(url) : NULL;
    string_t *second = url ? lab_mobile_qr(url) : NULL;
    bool ok = first && second && !string_compare(first, second) && geometry_matches(first);
    string_free(second);
    string_free(first);
    string_free(url);
    TEST_ASSERT_TRUE(ok, "ASCII QR output is deterministic and all 1369 modules match the legacy geometry");
}

static bool local_only_metadata(const char *header)
{
    string_t *host = header ? string_new_with(header) : NULL;
    json_t *details = header && !host ? NULL : lab_mobile_details(host, 8765);
    const string_t *title = json_string_value(test_lab_member(details, "title"));
    const string_t *hint = json_string_value(test_lab_member(details, "hint"));
    const string_t *url = json_string_value(test_lab_member(details, "url"));
    const string_t *qr = json_string_value(test_lab_member(details, "qr"));
    bool ok = details && title && string_view_equals_literal(string_view_all(title), "Local access") && hint &&
              string_byte_length(hint) && url && !string_byte_length(url) && qr && !string_byte_length(qr);
    const char *flags[] = {"control", "funnel", "tailscale"};
    for (size_t i = 0; ok && i < sizeof flags / sizeof *flags; ++i) {
        bool value = true;
        ok = json_bool_value(test_lab_member(details, flags[i]), &value) && !value;
    }
    /* All callers supply localhost, numeric or malformed hosts, never a valid .ts.net name. */
    ok = !lab_mobile_host_allowed(host) && ok;
    json_free(details);
    string_free(host);
    return ok;
}

static bool loopback_metadata(const char *directory)
{
    /* An empty executable search directory is an additional guard against invoking a live Tailscale CLI. */
    if (setenv("PATH", directory, 1))
        return false;
    const char *binds[] = {NULL, "127.0.0.1", "::1"};
    const char *hosts[] = {"localhost:8765", "127.0.0.1:8765", "[::1]:8765", "10.23.45.67:8765"};
    for (size_t i = 0; i < sizeof binds / sizeof *binds; ++i) {
        if (binds[i] ? setenv("MARS_LAB_BIND_HOST", binds[i], 1) : unsetenv("MARS_LAB_BIND_HOST"))
            return false;
        for (size_t j = 0; j < sizeof hosts / sizeof *hosts; ++j)
            if (!local_only_metadata(hosts[j]))
                return false;
    }
    return true;
}

static bool invalid_host_metadata(const char *directory)
{
    if (setenv("PATH", directory, 1) || setenv("MARS_LAB_BIND_HOST", "::", 1))
        return false;
    const char *hosts[] = {NULL,
                           "",
                           "[::1",
                           "localhost:0",
                           "localhost:65536",
                           "localhost:not-a-port",
                           "localhost\r\nX-Injected: value",
                           "http://localhost:8765/",
                           "localhost@other.invalid",
                           "localhost:8765/path",
                           "other.invalid",
                           "node.ts.net@localhost"};
    for (size_t i = 0; i < sizeof hosts / sizeof *hosts; ++i)
        if (!local_only_metadata(hosts[i]))
            return false;
    return true;
}

static void test_lab_mobile_loopback(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(loopback_metadata),
                     "unset and loopback-only binds never advertise a direct LAN URL or QR");
}

static void test_lab_mobile_invalid_hosts(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(invalid_host_metadata),
                     "malformed and unverified hosts remain local-only even under a wildcard bind");
}

/* Register mobile regressions alongside the other sequential native Lab helpers. */
void test_lab_mobile_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_mobile_qr_unsupported, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_mobile_qr_capacity, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_mobile_qr_geometry, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_mobile_loopback, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_mobile_invalid_hosts, tests, NULL);
}
