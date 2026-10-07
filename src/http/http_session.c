/* In-memory sessions and verified TLS client identities. */
#include <sodium.h>
#include <string.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

void http_secret_free(array_t *bytes)
{
    if (bytes && array_size(bytes))
        sodium_memzero(array_get(bytes, 0), array_size(bytes));
    array_destroy(bytes);
}

array_t *http_read_secret_file(const string_t *path)
{
    file_t *file = file_new(path);
    array_t *bytes = array_create(1, NULL, NULL);
    unsigned char buffer[8192];
    bool ok = file && bytes && file_open_read(file);
    while (ok) {
        size_t count = 0;
        ok = file_read(file, buffer, sizeof(buffer), &count);
        if (!ok || !count)
            break;
        ok = count <= 4194304 - array_size(bytes) && array_append_carray(bytes, buffer, count);
    }
    sodium_memzero(buffer, sizeof(buffer));
    if (file && file_is_open(file) && !file_close(file))
        ok = false;
    file_free(file);
    if (!ok || !array_size(bytes)) {
        http_secret_free(bytes);
        return NULL;
    }
    return bytes;
}

/* Replace the identity atomically, discarding connections and cookies from the old identity. */
bool http_client_set_identity_files(http_client_t *client, const string_t *certificate, const string_t *private_key,
                                    const string_t *password)
{
    if (!client || client->active || (!certificate != !private_key) ||
        (password && memchr(string_c_str(password), 0, string_byte_length(password))))
        return false;
    array_t *cert = certificate ? http_read_secret_file(certificate) : NULL;
    array_t *key = private_key ? http_read_secret_file(private_key) : NULL;
    string_t *pass = password ? string_clone(password) : NULL;
    CURL *fresh = curl_easy_init();
    if (!fresh || (certificate && (!cert || !key)) || (password && !pass)) {
        curl_easy_cleanup(fresh);
        array_destroy(cert);
        http_secret_free(key);
        string_free(pass);
        return false;
    }
    curl_easy_cleanup(client->easy);
    array_destroy(client->certificate);
    http_secret_free(client->private_key);
    string_free(client->key_password);
    client->easy = fresh;
    client->certificate = cert;
    client->private_key = key;
    client->key_password = pass;
    return true;
}

/* Clear the memory-only cookie jar without changing its enabled state. */
bool http_client_clear_cookies(http_client_t *client)
{
    return client && !client->active && curl_easy_setopt(client->easy, CURLOPT_COOKIELIST, "ALL") == CURLE_OK;
}

/* Enable cookies explicitly; disabling also destroys retained cookie state. */
bool http_client_set_cookies(http_client_t *client, bool enabled)
{
    if (!client || client->active)
        return false;
    if (!enabled && !http_client_clear_cookies(client))
        return false;
    client->cookies = enabled;
    return true;
}

/* Configure a bounded, same-origin buffered GET/HEAD redirect policy. */
bool http_client_set_redirects(http_client_t *client, unsigned max_hops)
{
    if (!client || client->active || max_hops > 16)
        return false;
    client->redirects = max_hops;
    return true;
}

bool http_apply_session(http_client_t *client)
{
    if (curl_easy_setopt(client->easy, CURLOPT_COOKIEFILE, client->cookies ? "" : NULL) != CURLE_OK)
        return false;
    if (!client->cookies && curl_easy_setopt(client->easy, CURLOPT_COOKIELIST, "ALL") != CURLE_OK)
        return false;
    if (!client->certificate)
        return true;
    struct curl_blob cert = {
        .data = array_get(client->certificate, 0), .len = array_size(client->certificate), .flags = CURL_BLOB_COPY};
    struct curl_blob key = {
        .data = array_get(client->private_key, 0), .len = array_size(client->private_key), .flags = CURL_BLOB_COPY};
    return curl_easy_setopt(client->easy, CURLOPT_SSLCERT_BLOB, &cert) == CURLE_OK &&
           curl_easy_setopt(client->easy, CURLOPT_SSLKEY_BLOB, &key) == CURLE_OK &&
           curl_easy_setopt(client->easy, CURLOPT_SSLCERTTYPE, "PEM") == CURLE_OK &&
           curl_easy_setopt(client->easy, CURLOPT_SSLKEYTYPE, "PEM") == CURLE_OK &&
           curl_easy_setopt(client->easy, CURLOPT_KEYPASSWD,
                            client->key_password ? string_c_str(client->key_password) : NULL) == CURLE_OK;
}

/* A retained jar may not grow beyond the client's header budget across requests. */
bool http_check_cookies(http_client_t *client)
{
    if (!client->cookies)
        return true;
    struct curl_slist *cookies = NULL;
    if (curl_easy_getinfo(client->easy, CURLINFO_COOKIELIST, &cookies) != CURLE_OK) {
        http_fail(client, HTTP_ERROR_TRANSPORT, "Cannot inspect retained cookies");
        return false;
    }
    size_t total = 0;
    bool ok = true;
    for (const struct curl_slist *item = cookies; item; item = item->next) {
        size_t size = strlen(item->data);
        if (size >= client->limits.max_header_bytes - total) {
            ok = false;
            break;
        }
        total += size + 1;
    }
    curl_slist_free_all(cookies);
    if (!ok) {
        curl_easy_setopt(client->easy, CURLOPT_COOKIELIST, "ALL");
        http_fail(client, HTTP_ERROR_LIMIT, "Retained cookie limit exceeded; session cleared");
    }
    return ok;
}
