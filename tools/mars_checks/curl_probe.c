/**
 * @file curl_probe.c
 * @brief Check the linked HTTP transport before a build or test uses optional libcurl protocols.
 *
 * This standalone bootstrap programme uses the same compiler and linker flags as MARS. It checks the
 * runtime, HTTPS, HTTP/2, thread-safe initialisation and both WebSocket schemes without opening a network
 * connection. A recent header or exported WebSocket function alone does not establish protocol support.
 * Failures print copyable upgrade commands; successful prerequisite checks remain quiet.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

_Static_assert(LIBCURL_VERSION_NUM >= 0x075600, "MARS requires libcurl headers >= 7.86.0");

static void probe_install_instructions(FILE *output)
{
    fputs("\nDebian/Ubuntu: run these commands on the affected computer.\n"
          "Try the distribution's development package first, then recheck its enabled features.\n\n"
          "sudo apt update\n"
          "sudo apt install --only-upgrade libcurl4-openssl-dev\n"
          "make check-curl-deps\n\n"
          "If WS/WSS are still unavailable, close MARS Lab and build curl 8.23.0 as follows.\n"
          "This installs under /usr/local and keeps the distribution's packages.\n\n"
          "sudo apt install build-essential pkg-config curl ca-certificates xz-utils "
          "libssl-dev libnghttp2-dev libpsl-dev zlib1g-dev\n"
          "(\n"
          "    set -eu\n"
          "    curl_build=$(mktemp -d)\n"
          "    trap 'rm -rf \"$curl_build\"' EXIT\n"
          "    curl -fL https://curl.se/download/curl-8.23.0.tar.xz -o \"$curl_build/curl.tar.xz\"\n"
          "    printf '%s  %s\\n' '8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5' "
          "\"$curl_build/curl.tar.xz\" | sha256sum -c -\n"
          "    tar -xJf \"$curl_build/curl.tar.xz\" -C \"$curl_build\"\n"
          "    cd \"$curl_build/curl-8.23.0\"\n"
          "    ./configure --prefix=/usr/local --libdir=/usr/local/lib \\\n"
          "        --with-openssl --with-nghttp2 --with-libpsl --enable-websockets \\\n"
          "        --enable-threaded-resolver --enable-shared --disable-static\n"
          "    make -j2\n"
          "    sudo make -j1 install\n"
          "    sudo ldconfig\n"
          ")\n"
          "export PKG_CONFIG_PATH=\"/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}\"\n"
          "make check-curl-deps\n\n"
          "After the feature check passes, rebuild and rerun the HTTP tests:\n\n"
          "make release\n"
          "make -j1 test_http\n\n"
          "Updating the curl command alone does not update the libcurl linked by MARS.\n"
          "Full instructions: docs/building.md#updating-libcurl-on-debian-or-ubuntu\n", output);
}

static bool probe_features(const curl_version_info_data *info)
{
    bool ok = info && info->version_num >= 0x075600;
    if (!ok)
        fputs("MARS requires a libcurl runtime >= 7.86.0.\n", stderr);
    static const struct { unsigned mask; const char *name; } features[] = {
        {CURL_VERSION_SSL, "TLS"},
        {CURL_VERSION_THREADSAFE, "thread-safe global initialisation"},
        {CURL_VERSION_HTTP2, "HTTP/2"}
    };
    /* These three capability flags and four schemes are a fixed bootstrap check, not a request-time lookup. */
    for (size_t i = 0; i < sizeof features / sizeof *features; ++i) {
        if (!info || !(info->features & features[i].mask)) {
            fprintf(stderr, "The linked libcurl lacks required %s support.\n", features[i].name);
            ok = false;
        }
    }
    static const char *const schemes[] = {"http", "https", "ws", "wss"};
    for (size_t i = 0; i < sizeof schemes / sizeof *schemes; ++i) {
        bool found = false;
        for (const char *const *protocol = info ? info->protocols : NULL; protocol && *protocol; ++protocol)
            if (!strcmp(*protocol, schemes[i])) {
                found = true;
                break;
            }
        if (!found) {
            fprintf(stderr, "The linked libcurl lacks required %s protocol support.\n", schemes[i]);
            ok = false;
        }
    }
    return ok;
}

/* Validate the actual linked runtime, or print installation help without contacting a service. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--install-help")) {
        probe_install_instructions(stdout);
        return ferror(stdout) ? 1 : 0;
    }
    bool quiet = argc == 2 && !strcmp(argv[1], "--quiet");
    if (argc > 2 || (argc == 2 && !quiet)) {
        fputs("Usage: curl_probe [--quiet|--install-help]\n", stderr);
        return 1;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fputs("Cannot initialise the linked libcurl runtime.\n", stderr);
        probe_install_instructions(stderr);
        return 1;
    }
    const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    bool ok = probe_features(info);
    CURL *easy = curl_easy_init();
    CURLU *url = curl_url();
    bool transport = easy && url && curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http,https,ws,wss") == CURLE_OK &&
        curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 2L) == CURLE_OK &&
        curl_url_set(url, CURLUPART_URL, "ws://localhost/", 0) == CURLUE_OK &&
        curl_url_set(url, CURLUPART_SCHEME, "wss", 0) == CURLUE_OK;
    if (!transport)
        fputs("The linked libcurl cannot configure WS/WSS transport.\n", stderr);
    ok = ok && transport;
    if (!quiet || !ok)
        fprintf(stderr, "libcurl runtime %s; headers %s.\n", info ? info->version : "unavailable", LIBCURL_VERSION);
    curl_url_cleanup(url);
    curl_easy_cleanup(easy);
    curl_global_cleanup();
    if (!ok)
        probe_install_instructions(stderr);
    return ok ? 0 : 1;
}
