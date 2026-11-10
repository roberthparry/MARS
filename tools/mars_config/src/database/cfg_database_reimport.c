/**
 * @file cfg_database_reimport.c
 * @brief Resumable one-time release imports of all packaged content.
 *
 * The encrypted settings store records the next stage as 1, 2 or 3. Completed stages advance that value;
 * a failed import leaves it in place. Empty state restores ordinary release updates. Globe reimport
 * retains the installed river network and prepared image blocks, whose original preparation copies may
 * have been removed, while replacing all packaged photographs and geography in one transaction. Almanac and
 * jurisdiction installations likewise build new files before publishing them. Settings and history survive.
 */
#include <stdio.h>

#include "string-t.h"
#include "mars_settings.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static int reimport_globe(void)
{
    string_t *key = cfg_database_setting(NULL, "MARS_GLOBE_DB_KEY", NULL);
    bool plain = key && !string_byte_length(key);
    string_free(key);
    if (!plain) {
        fputs("Queued fresh globe replacement requires a plaintext globe; existing data was retained.\n", stderr);
        return 1;
    }
    /* Recreate packaged tables, retaining large unrelated BLOB tables on their existing pages. */
    return cfg_globe_run("globe-reimport");
}

static int reimport_almanac(void)
{
    return cfg_database_run(false, NULL, NULL, NULL, NULL, false);
}

static int reimport_jurisdiction(void)
{
    return cfg_database_run(true, NULL, NULL, NULL, NULL, false);
}

/* One bounded, ordered installation workflow; lookup of the saved stage is direct. */
int cfg_database_reimport_pending(void)
{
    string_t *pending = mars_settings_read("MARS_RELEASE_DB_REIMPORT");
    if (!pending) return 1;
    size_t length = string_byte_length(pending);
    const char *value = string_c_str(pending);
    int stage = length == 1 && value[0] >= '1' && value[0] <= '3' ? value[0] - '1' : -1;
    string_free(pending);
    if (!length) return -1;
    if (stage < 0) {
        fputs("Invalid queued database re-import stage; no database changed.\n", stderr);
        return 1;
    }
    static const struct {
        const char *name;
        int (*run)(void);
        const char *next;
    } stages[] = {
        {"globe", reimport_globe, "2"},
        {"almanac", reimport_almanac, "3"},
        {"jurisdiction", reimport_jurisdiction, ""},
    };
    for (size_t i = (size_t)stage; i < sizeof stages / sizeof *stages; ++i) {
        printf("One-time fresh database import: %s (%zu/3).\n", stages[i].name, i + 1);
        if (stages[i].run()) return 1;
        string_t *update = string_sprintf("{\"MARS_RELEASE_DB_REIMPORT\":\"%s\"}", stages[i].next);
        bool saved = update && mars_settings_sync(update);
        string_free(update);
        if (!saved) {
            fputs("Import succeeded but its completion could not be saved; the request remains pending.\n", stderr);
            return 1;
        }
    }
    puts("One-time fresh database imports complete; subsequent releases use ordinary updates.");
    return 0;
}
