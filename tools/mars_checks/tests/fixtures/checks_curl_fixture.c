/**
 * @file checks_curl_fixture.c
 * @brief Model feature-limited libcurl runtimes in isolated dependency-check processes.
 *
 * Linker wrapping changes the reported runtime capabilities or rejects a WebSocket URL. All other
 * operations use the installed transport. No network access, installed library changes or user settings
 * are involved. MARS_TEST_CURL selects one bounded failure scenario for the child process only.
 */
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

curl_version_info_data *__real_curl_version_info(CURLversion age);
CURLUcode __real_curl_url_set(CURLU *url, CURLUPart part, const char *value, unsigned flags);

/* Keep all real runtime properties except the single capability under test. */
curl_version_info_data *__wrap_curl_version_info(CURLversion age)
{
    const char *mode = getenv("MARS_TEST_CURL");
    curl_version_info_data *real = __real_curl_version_info(age);
    if (!mode || !real || !strcmp(mode, "current"))
        return real;
    static curl_version_info_data info;
    static const char *const no_ws[] = {"http", "https", "wss", NULL};
    static const char *const no_wss[] = {"http", "https", "ws", NULL};
    info = *real;
    if (!strcmp(mode, "old")) { info.version_num = 0x075500; info.version = "7.85.0"; }
    if (!strcmp(mode, "no-ws")) info.protocols = no_ws;
    if (!strcmp(mode, "no-wss")) info.protocols = no_wss;
    if (!strcmp(mode, "no-tls")) info.features &= ~CURL_VERSION_SSL;
    if (!strcmp(mode, "no-threads")) info.features &= ~CURL_VERSION_THREADSAFE;
    if (!strcmp(mode, "no-http2")) info.features &= ~CURL_VERSION_HTTP2;
    return &info;
}

/* Exercise URL validation separately from the runtime's advertised capabilities. */
CURLUcode __wrap_curl_url_set(CURLU *url, CURLUPart part, const char *value, unsigned flags)
{
    const char *mode = getenv("MARS_TEST_CURL");
    if (mode && !strcmp(mode, "url") && part == CURLUPART_SCHEME && !strcmp(value, "wss"))
        return CURLUE_UNSUPPORTED_SCHEME;
    return __real_curl_url_set(url, part, value, flags);
}
