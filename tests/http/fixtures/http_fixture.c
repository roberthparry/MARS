/**
 * @file http_fixture.c
 * @brief Native offline HTTP, gRPC and WebSocket fixture executable.
 *
 * Accepts a protocol name and inherited listening descriptor, optionally followed
 * by a certificate, private key and "identity" for mandatory client certificates.
 * Tests may prefix --lifetime-ms with a value from 100 to 15000 to shorten the
 * lifetime without changing the production test default or worker cap.
 * At most 16 detached workers serve connections, each with a 15-second total deadline. TLS uses
 * OpenSSL directly; public file and string APIs own certificate input and request
 * text, whilst intentional wire faults bypass the HTTP library under test.
 * The parent owns the process and stops all workers by terminating it.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/pem.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "http_fixture.h"

typedef void (*fixture_handler_t)(fixture_connection_t *);

typedef struct {
    fixture_connection_t connection;
    fixture_handler_t handler;
} fixture_worker_t;

/* Send a literal HTTP or protocol fragment. */
bool fixture_text(fixture_connection_t *connection, const char *text)
{
    return fixture_write(connection, text, strlen(text));
}

/* Export encoded text only at the transport boundary. */
bool fixture_string(fixture_connection_t *connection, const string_t *text)
{
    return !text || fixture_write(connection, string_c_str(text), string_byte_length(text));
}

/* Compare protocol tokens without inspecting their storage. */
bool fixture_equal(const string_t *text, const char *literal)
{
    return string_view_equals_literal(text ? string_view_all(text) : string_view_empty(), literal);
}

/* Parse decimal protocol fields with explicit bounds. */
bool fixture_decimal(const string_t *text, size_t maximum, size_t *value)
{
    string_cursor_t *cursor = string_cursor_new(text);
    bool ok = cursor && !string_cursor_done(cursor);
    size_t result = 0;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char digit;
        ok = string_cursor_peek_ascii(cursor, &digit) && digit >= '0' && digit <= '9';
        if (!ok || (size_t)(digit - '0') > maximum || result > (maximum - (size_t)(digit - '0')) / 10) {
            ok = false;
            break;
        }
        result = result * 10 + (size_t)(digit - '0');
        ok = string_cursor_next(cursor) == 0;
    }
    string_cursor_free(cursor);
    if (ok)
        *value = result;
    return ok;
}

/* Release the owned text fields after every request, including parser failures. */
void fixture_request_free(fixture_request_t *request)
{
    string_free(request->method);
    string_free(request->path);
    string_free(request->authorization);
    string_free(request->cookie);
    string_free(request->type);
    string_free(request->soap_action);
    string_free(request->custom);
    string_free(request->websocket_key);
    memset(request, 0, sizeof(*request));
}

static string_t *fixture_token(string_cursor_t *cursor, const char *delimiter)
{
    string_pos_t start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor) && !string_cursor_match(cursor, delimiter)) {
        if (string_cursor_next(cursor))
            return NULL;
    }
    string_view_t view = string_cursor_view_extract(start, cursor);
    return string_cursor_consume(cursor, delimiter) ? string_from_view(&view) : NULL;
}

static bool fixture_request_header(fixture_connection_t *connection, fixture_request_t *request,
                                    string_t *name, string_t *value)
{
    string_to_lower(name);
    string_t **destination = NULL;
    bool ok = true;
    /* Fixed ten-header dispatch bounds the comparisons for each parsed line. */
    if (fixture_equal(name, "content-length"))
        ok = fixture_decimal(value, 8388608, &request->length);
    else if (fixture_equal(name, "authorization"))
        destination = &request->authorization;
    else if (fixture_equal(name, "cookie"))
        destination = &request->cookie;
    else if (fixture_equal(name, "content-type"))
        destination = &request->type;
    else if (fixture_equal(name, "soapaction"))
        destination = &request->soap_action;
    else if (fixture_equal(name, "x-custom"))
        destination = &request->custom;
    else if (fixture_equal(name, "sec-websocket-key"))
        destination = &request->websocket_key;
    else if (fixture_equal(name, "connection")) {
        string_to_lower(value);
        request->close = fixture_equal(value, "close");
    } else if (fixture_equal(name, "expect")) {
        string_to_lower(value);
        if (fixture_equal(value, "100-continue"))
            ok = fixture_text(connection, "HTTP/1.1 100 Continue\r\n\r\n");
    }
    if (destination) {
        string_free(*destination);
        *destination = string_clone(value);
        ok = *destination != NULL;
    }
    return ok;
}

/* Parse bounded request text through string cursors and own the selected fields. */
bool fixture_request(fixture_connection_t *connection, fixture_request_t *request)
{
    memset(request, 0, sizeof(*request));
    unsigned char wire[65536];
    size_t used = 0;
    do {
        if (used == sizeof(wire) || !fixture_read(connection, wire + used, 1) || !wire[used])
            return false;
        ++used;
    } while (used < 4 || memcmp(wire + used - 4, "\r\n\r\n", 4));
    string_t *head = string_new();
    bool ok = head && string_append_utf8_exact(head, (const char *)wire, used) == 0;
    string_cursor_t *cursor = ok ? string_cursor_new(head) : NULL;
    ok = cursor != NULL;
    if (ok) {
        request->method = fixture_token(cursor, " ");
        request->path = fixture_token(cursor, " ");
        string_t *version = fixture_token(cursor, "\r\n");
        ok = request->method && request->path && version;
        request->close = !fixture_equal(version, "HTTP/1.1");
        string_free(version);
    }
    /* One sequential cursor pass consumes each bounded request head. */
    while (ok && !string_cursor_match(cursor, "\r\n")) {
        string_t *line = fixture_token(cursor, "\r\n");
        string_cursor_t *fields = line ? string_cursor_new(line) : NULL;
        string_t *name = fields ? fixture_token(fields, ":") : NULL;
        string_t *value = NULL;
        if (name) {
            string_view_t view = string_cursor_view_between(string_cursor_position(fields),
                                                            string_cursor_end_position(fields), fields);
            view = string_view_trim(view);
            value = string_from_view(&view);
        }
        ok = name && value && fixture_request_header(connection, request, name, value);
        string_free(value);
        string_free(name);
        string_cursor_free(fields);
        string_free(line);
    }
    ok = ok && string_cursor_consume(cursor, "\r\n") && string_cursor_done(cursor);
    string_cursor_free(cursor);
    string_free(head);
    if (!ok)
        fixture_request_free(request);
    return ok;
}

/* Main reserves a slot before creation; workers release it only after cleanup. */
static pthread_mutex_t fixture_workers_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned fixture_workers;

static bool fixture_worker_reserve(void)
{
    pthread_mutex_lock(&fixture_workers_mutex);
    bool available = fixture_workers < 16;
    if (available)
        ++fixture_workers;
    pthread_mutex_unlock(&fixture_workers_mutex);
    return available;
}

static void fixture_worker_release(void)
{
    pthread_mutex_lock(&fixture_workers_mutex);
    --fixture_workers;
    pthread_mutex_unlock(&fixture_workers_mutex);
}

static void *fixture_worker(void *argument)
{
    fixture_worker_t *worker = argument;
    fixture_connection_t *connection = &worker->connection;
    if (!connection->tls || fixture_tls_accept(connection))
        worker->handler(connection);
    if (connection->tls) {
        fixture_tls_shutdown(connection);
        SSL_free(connection->tls);
    }
    close(connection->fd);
    free(worker);
    fixture_worker_release();
    return NULL;
}

static int fixture_alpn(SSL *tls, const unsigned char **out, unsigned char *out_length,
                        const unsigned char *input, unsigned input_length, void *argument)
{
    (void)tls;
    (void)argument;
    static const unsigned char protocols[] = {2, 'h', '2'};
    unsigned char *selected = NULL;
    if (SSL_select_next_proto(&selected, out_length, protocols, sizeof(protocols), input, input_length) !=
        OPENSSL_NPN_NEGOTIATED)
        return SSL_TLSEXT_ERR_NOACK;
    *out = selected;
    return SSL_TLSEXT_ERR_OK;
}

static string_t *fixture_pem_text(const char *path)
{
    file_t *file = file_new_cstr(path);
    string_t *text = file ? file_read_all_text(file) : NULL;
    file_free(file);
    if (text && string_byte_length(text) > INT_MAX) {
        string_free(text);
        return NULL;
    }
    return text;
}

static SSL_CTX *fixture_tls_context(const char *certificate_path, const char *key_path, bool identity)
{
    string_t *certificate_text = fixture_pem_text(certificate_path), *key_text = fixture_pem_text(key_path);
    BIO *certificates = certificate_text ? BIO_new_mem_buf(string_c_str(certificate_text),
                                                          (int)string_byte_length(certificate_text)) : NULL;
    BIO *keys = key_text ? BIO_new_mem_buf(string_c_str(key_text), (int)string_byte_length(key_text)) : NULL;
    X509 *certificate = certificates ? PEM_read_bio_X509(certificates, NULL, NULL, NULL) : NULL;
    EVP_PKEY *key = keys ? PEM_read_bio_PrivateKey(keys, NULL, NULL, NULL) : NULL;
    SSL_CTX *context = SSL_CTX_new(TLS_server_method());
    bool ok = context && certificate && key && SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) == 1 &&
              SSL_CTX_use_certificate(context, certificate) == 1 && SSL_CTX_use_PrivateKey(context, key) == 1 &&
              SSL_CTX_check_private_key(context) == 1;
    if (ok && identity)
        ok = X509_STORE_add_cert(SSL_CTX_get_cert_store(context), certificate) == 1;
    X509_free(certificate);
    /* PEM chains are decoded by OpenSSL; all path access stays in file.h. */
    while (ok && (certificate = PEM_read_bio_X509(certificates, NULL, NULL, NULL))) {
        if (identity)
            ok = X509_STORE_add_cert(SSL_CTX_get_cert_store(context), certificate) == 1;
        if (!ok || SSL_CTX_add_extra_chain_cert(context, certificate) != 1) {
            X509_free(certificate);
            ok = false;
        }
    }
    EVP_PKEY_free(key);
    BIO_free(keys);
    BIO_free(certificates);
    string_free(key_text);
    string_free(certificate_text);
    if (!ok) {
        SSL_CTX_free(context);
        return NULL;
    }
    if (identity)
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    return context;
}

/* Run the requested peer on the listener already bound by its test parent. */
int main(int argc, char **argv)
{
    size_t lifetime_ms = 15000;
    string_t *option = argc > 1 ? string_new_with(argv[1]) : NULL;
    bool shortened = fixture_equal(option, "--lifetime-ms");
    string_free(option);
    if (shortened) {
        string_t *duration = argc > 2 ? string_new_with(argv[2]) : NULL;
        bool valid_duration = duration && fixture_decimal(duration, 15000, &lifetime_ms) && lifetime_ms >= 100;
        string_free(duration);
        if (!valid_duration)
            return EXIT_FAILURE;
        argc -= 2;
        argv += 2;
    }
    if (argc != 3 && argc != 5 && argc != 6) {
        string_fprintf(stderr, "Usage: http_fixture [--lifetime-ms 100..15000] http|grpc|websocket FD "
                               "[CERT KEY [identity]]\n");
        return EXIT_FAILURE;
    }
    fixture_handler_t handler = NULL;
    string_t *protocol = string_new_with(argv[1]), *descriptor = string_new_with(argv[2]);
    string_t *mode = argc == 6 ? string_new_with(argv[5]) : NULL;
    if (fixture_equal(protocol, "http"))
        handler = fixture_http;
    else if (fixture_equal(protocol, "grpc"))
        handler = fixture_grpc;
    else if (fixture_equal(protocol, "websocket"))
        handler = fixture_websocket;
    size_t listener = 0;
    bool valid = handler && descriptor && fixture_decimal(descriptor, INT_MAX, &listener) &&
                 (argc != 6 || fixture_equal(mode, "identity"));
    string_free(mode);
    string_free(descriptor);
    string_free(protocol);
    if (!valid)
        return EXIT_FAILURE;
    signal(SIGPIPE, SIG_IGN);
    SSL_CTX *context = NULL;
    if (argc >= 5) {
        context = fixture_tls_context(argv[3], argv[4], argc == 6);
        if (!context) {
            string_fprintf(stderr, "fixture: cannot initialise TLS certificate/key\n");
            return EXIT_FAILURE;
        }
        if (handler == fixture_grpc)
            SSL_CTX_set_alpn_select_cb(context, fixture_alpn, NULL);
    }
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes)) {
        SSL_CTX_free(context);
        return EXIT_FAILURE;
    }
    if (pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED)) {
        pthread_attr_destroy(&attributes);
        SSL_CTX_free(context);
        return EXIT_FAILURE;
    }
    for (;;) {
        int fd = accept((int)listener, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (!fixture_worker_reserve()) {
            close(fd);
            continue;
        }
        fixture_worker_t *worker = calloc(1, sizeof(*worker));
        if (!worker || !fixture_connection_init(&worker->connection, fd) ||
            !fixture_limit_lifetime(&worker->connection, (unsigned)lifetime_ms)) {
            close(fd);
            free(worker);
            fixture_worker_release();
            continue;
        }
        worker->handler = handler;
        if (context) {
            worker->connection.tls = SSL_new(context);
            if (!worker->connection.tls || SSL_set_fd(worker->connection.tls, fd) != 1) {
                SSL_free(worker->connection.tls);
                close(fd);
                free(worker);
                fixture_worker_release();
                continue;
            }
        }
        pthread_t thread;
        if (pthread_create(&thread, &attributes, fixture_worker, worker)) {
            SSL_free(worker->connection.tls);
            close(fd);
            free(worker);
            fixture_worker_release();
        }
    }
    pthread_attr_destroy(&attributes);
    /* The normal lifetime ends with the parent's SIGTERM; do not free a shared
     * TLS context while workers may still be using it after an accept failure. */
    return EXIT_FAILURE;
}
