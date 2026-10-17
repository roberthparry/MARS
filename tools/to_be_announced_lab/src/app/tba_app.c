/**
 * @file tba_app.c
 * @brief Process-local configuration for the private Lab.
 *
 * Owns only an URL prefix and compiled defaults. No request settings, client data
 * or results are loaded from or written to disk, including prior installations.
 */
#include <stdlib.h>
#include <string.h>

#include "tba_app.h"

struct tba_app {
    string_t *base;
    json_t *defaults;
};

/* Resolve the public prefix without touching stored state. */
tba_app_t *tba_app_new(const char *base)
{
    tba_app_t *app = calloc(1, sizeof *app);
    if (!app)
        return NULL;
    const char *prefix = base ? base : getenv("TBA_LAB_PUBLIC_PATH");
    if (!prefix)
        prefix = "/to-be-announced";
    app->base = string_new_with(!strcmp(prefix, "/") ? "" : prefix);
    app->defaults = tba_app_defaults();
    bool ok = app->base && (!*prefix || prefix[0] == '/') && string_find(app->base, "..") < 0 &&
              string_find(app->base, "?") < 0 && string_find(app->base, "#") < 0 &&
              json_type(app->defaults) == JSON_OBJECT;
    if (!ok) {
        tba_app_free(app);
        return NULL;
    }
    return app;
}

/* Release process-local configuration. */
void tba_app_free(tba_app_t *app)
{
    if (app) {
        string_free(app->base);
        json_free(app->defaults);
        free(app);
    }
}

/* Borrow the configured prefix. */
const char *tba_app_base(const tba_app_t *app)
{
    return app ? string_c_str(app->base) : "";
}

/* Return independent defaults, never prior client settings. */
json_t *tba_app_state(const tba_app_t *app)
{
    return app ? json_clone(app->defaults) : NULL;
}
