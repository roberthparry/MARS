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

#include "json-t.h"

/**
 * @brief Return owned title/hint/url/qr/control/tailscale/funnel metadata, or NULL.
 *
 * Control and funnel are always false. A verified private Tailscale HTTPS proxy
 * is preferred for local requests, including a loopback listener. Otherwise, with
 * MARS_LAB_BIND_HOST set to 0.0.0.0 or ::, desktop localhost/loopback requests
 * discover an active private interface, preferring LAN IPv4. IPv6 candidates
 * require the dual-stack :: listener. Other binds never trigger interface discovery;
 * direct URLs must match the actual bind address. An unset bind variable defaults
 * to loopback. Malformed or unverified hosts yield local-only metadata.
 * No DNS resolution or network configuration is done.
 * Tailscale inspection uses its local Unix-socket HTTP API (1.5 seconds and 1 MiB
 * per response), without launching a command-line process. An existing HTTPS Serve
 * root proxy to this port is preferred even when the page was opened through localhost.
 * Release with json_free(). For page injection, use the transient state.mobile key.
 * @param host_header Borrowed request Host, including an optional port.
 * @param port Actual listening port used to verify proxy and direct URLs.
 * @return Owned metadata released with json_free, or NULL on allocation failure.
 */
json_t *lab_mobile_details(const string_t *host_header, uint16_t port);

/**
 * @brief Select private mobile access using caller-supplied local-daemon snapshots.
 * @param[in] host_header Borrowed request Host with optional port; never treated as daemon identity.
 * @param[in] port Actual Lab listening port, used to check the HTTPS proxy target.
 * @param[in] status Borrowed trusted Tailscale status object; NULL means unavailable.
 * @param[in] serve Borrowed trusted Serve configuration object; NULL means unavailable.
 * @return Owned metadata released with json_free(), or NULL on allocation failure. Snapshot inputs are not changed.
 * No daemon I/O occurs. Online identity, exact host, local interfaces, proxy and Funnel checks still apply.
 * Snapshots must come from the local daemon, never from an HTTP request or another untrusted source.
 */
json_t *lab_mobile_details_from_status(const string_t *host_header, uint16_t port,
                                      const json_t *status, const json_t *serve);

/**
 * @brief Verify this node's exact tailnet DNS host with Funnel disabled.
 *
 * This augments, rather than replaces, the server's ordinary local Host allowlist.
 * Local API failure, malformed output, enabled Funnel or any other DNS name fails
 * closed. Recheck per request; this cannot atomically prevent external Tailscale
 * configuration changes after inspection. No commands mutate external state.
 * @param host_header Borrowed request Host to verify.
 * @return True only for this machine's verified private tailnet host.
 */
bool lab_mobile_host_allowed(const string_t *host_header);

/**
 * @brief Verify one exact tailnet Host against trusted local-daemon snapshots without I/O.
 * @param[in] host_header Borrowed request Host with optional port.
 * @param[in] status Borrowed trusted local status object, or NULL to reject verification.
 * @param[in] serve Borrowed trusted local Serve configuration, or NULL to reject verification.
 * @return True only for this online node's exact tailnet DNS name with Funnel disabled.
 * Inputs are not changed or retained. The caller must obtain snapshots from the local daemon.
 */
bool lab_mobile_host_allowed_from_status(const string_t *host_header, const json_t *status, const json_t *serve);

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
