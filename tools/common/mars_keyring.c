/**
 * @file mars_keyring.c
 * @brief Native runtime adapter for the desktop's libsecret password service.
 *
 * Resolves the documented public libsecret ABI through the dynamic loader so headless build tools do not
 * require development headers or a running desktop. Only configuration initialisation creates a key;
 * ordinary startup never replaces an unavailable key. Keys use operating-system-backed libsodium randomness.
 */
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sodium.h>

#include "mars_keyring.h"

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t condition;
    void *cancellable;
    void (*cancel)(void *);
    bool finished;
} keyring_deadline_t;

static void *keyring_wait(void *argument)
{
    keyring_deadline_t *deadline = argument;
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += 30;
    pthread_mutex_lock(&deadline->lock);
    while (!deadline->finished) {
        int status = pthread_cond_timedwait(&deadline->condition, &deadline->lock, &until);
        if (status == ETIMEDOUT) {
            deadline->cancel(deadline->cancellable);
            break;
        }
        if (status) break;
    }
    pthread_mutex_unlock(&deadline->lock);
    return NULL;
}

/* Keep a stable service identity independent of the database or bootstrap file's location. */
string_t *mars_keyring_password(bool create)
{
    void *library = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
    if (!library || sodium_init() < 0) { if (library) dlclose(library); return NULL; }
    void *(*schema_new)(const char *, int, ...) = dlsym(library, "secret_schema_new");
    void (*schema_unref)(void *) = dlsym(library, "secret_schema_unref");
    void *(*service_get)(int, void *, void **) = dlsym(library, "secret_service_get_sync");
    void *(*attributes_build)(const void *, ...) = dlsym(library, "secret_attributes_build");
    void (*attributes_unref)(void *) = dlsym(library, "g_hash_table_unref");
    void *(*lookup)(void *, const void *, void *, void *, void **) =
        dlsym(library, "secret_service_lookup_sync");
    int (*store)(void *, const void *, void *, const char *, const char *, void *, void *, void **) =
        dlsym(library, "secret_service_store_sync");
    void *(*value_new)(const char *, ssize_t, const char *) = dlsym(library, "secret_value_new");
    const char *(*value_text)(void *) = dlsym(library, "secret_value_get_text");
    void (*value_unref)(void *) = dlsym(library, "secret_value_unref");
    void (*error_free)(void *) = dlsym(library, "g_error_free");
    void *(*cancellable_new)(void) = dlsym(library, "g_cancellable_new");
    void (*cancel)(void *) = dlsym(library, "g_cancellable_cancel");
    void (*unref)(void *) = dlsym(library, "g_object_unref");
    bool ready = schema_new && schema_unref && service_get && attributes_build && attributes_unref &&
                 lookup && store && value_new && value_text && value_unref && error_free &&
                 cancellable_new && cancel && unref;
    keyring_deadline_t deadline = {
        .lock = PTHREAD_MUTEX_INITIALIZER, .condition = PTHREAD_COND_INITIALIZER,
        .cancellable = ready ? cancellable_new() : NULL, .cancel = cancel
    };
    pthread_t timer;
    bool running = deadline.cancellable && !pthread_create(&timer, NULL, keyring_wait, &deadline);
    /* SECRET_SCHEMA_NONE and SECRET_SCHEMA_ATTRIBUTE_STRING are both zero in libsecret's public ABI. */
    void *schema = running ? schema_new("org.mars.Configuration", 0, "application", 0, NULL) : NULL;
    void *error = NULL;
    /* Use the desktop service explicitly: a sandboxed editor's password backend may be an isolated portal. */
    void *service = schema ? service_get(2, deadline.cancellable, &error) : NULL;
    void *attributes = service ? attributes_build(schema, "application", "mars", NULL) : NULL;
    void *secret = attributes ? lookup(service, schema, attributes, deadline.cancellable, &error) : NULL;
    const char *password = secret ? value_text(secret) : NULL;
    string_t *result = password && *password ? string_new_with(password) : NULL;
    if (secret) value_unref(secret);
    if (attributes && !result && !error && create) {
        unsigned char random[32];
        char encoded[65];
        randombytes_buf(random, sizeof random);
        sodium_bin2hex(encoded, sizeof encoded, random, sizeof random);
        void *value = value_new(encoded, -1, "text/plain");
        if (value && store(service, schema, attributes, "default", "MARS configuration database", value,
                           deadline.cancellable, &error)) result = string_new_with(encoded);
        if (value) value_unref(value);
        sodium_memzero(random, sizeof random); sodium_memzero(encoded, sizeof encoded);
    }
    if (error) error_free(error);
    if (attributes) attributes_unref(attributes);
    if (service) unref(service);
    if (schema) schema_unref(schema);
    if (running) {
        pthread_mutex_lock(&deadline.lock);
        deadline.finished = true;
        pthread_cond_signal(&deadline.condition);
        pthread_mutex_unlock(&deadline.lock);
        pthread_join(timer, NULL);
    }
    if (deadline.cancellable) unref(deadline.cancellable);
    pthread_cond_destroy(&deadline.condition);
    pthread_mutex_destroy(&deadline.lock);
    dlclose(library);
    return result;
}
