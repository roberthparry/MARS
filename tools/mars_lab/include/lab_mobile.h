/**
 * @file lab_mobile.h
 * @brief Private native Lab network metadata and QR generation.
 *
 * The server supplies its Host header and listening port. Numeric private hosts
 * must belong to a local interface. Tailnet DNS names must match this node's
 * verified Tailscale status and have no enabled Funnel configuration. These
 * helpers only inspect configuration; they never enable sharing or public access.
 */
#ifndef MARS_LAB_MOBILE_H
#define MARS_LAB_MOBILE_H

#include <stdint.h>

#include "json.h"

/**
 * @brief Return owned title/hint/url/qr/control/tailscale/funnel metadata, or NULL.
 *
 * Control and funnel are always false. With MARS_LAB_BIND_HOST set to 0.0.0.0 or
 * ::, desktop localhost/loopback requests discover an active private interface,
 * preferring LAN IPv4. IPv6 candidates require the dual-stack :: listener. Other
 * binds never trigger discovery; direct URLs must match the actual bind address.
 * An unset bind variable defaults to loopback. Malformed or unverified hosts yield
 * local-only metadata. No DNS resolution or network configuration is done.
 * Tailscale inspection uses two bounded read-only commands (1.5 seconds and 1 MiB
 * each). An existing HTTPS Serve root proxy to this port is preferred where found.
 * Release with json_free(). For page injection, use the transient state.mobile key.
 * @param host_header Borrowed request Host, including an optional port.
 * @param port Actual listening port used to verify proxy and direct URLs.
 * @return Owned metadata released with json_free, or NULL on allocation failure.
 */
json_t *lab_mobile_details(const string_t *host_header, uint16_t port);

/**
 * @brief Verify this node's exact tailnet DNS host with Funnel disabled.
 *
 * This augments, rather than replaces, the server's ordinary local Host allowlist.
 * Command failure, malformed output, enabled Funnel or any other DNS name fails
 * closed. Recheck per request; this cannot atomically prevent external Tailscale
 * configuration changes after inspection. No commands mutate external state.
 * @param host_header Borrowed request Host to verify.
 * @return True only for this machine's verified private tailnet host.
 */
bool lab_mobile_host_allowed(const string_t *host_header);

/**
 * @brief Return owned, trusted SVG for an ASCII URL using QR version 5, level L.
 *
 * Uses the original Python byte-mode, mask-zero GF(256)/Reed–Solomon algorithm.
 * Empty or unsupported input (including more than 106 bytes) yields an empty
 * string; allocation failure yields NULL. SVG contains only generated geometry,
 * never input markup. Release with string_free().
 * @param url Borrowed ASCII URL; NULL is treated as empty.
 * @return Owned SVG or empty string released with string_free; NULL on allocation failure.
 */
string_t *lab_mobile_qr(const string_t *url);

#endif
