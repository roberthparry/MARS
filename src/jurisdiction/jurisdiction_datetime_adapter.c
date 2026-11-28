/**
 * @file jurisdiction_datetime_adapter.c
 * @brief Civil datetime interpolation with jurisdiction-owned Lingua integration.
 *
 * Borrows native datetime values. The default uses the installed Lingua presentation, while explicit ISO, date
 * and time modes remain independent of configuration. Case-sensitive codes use M for months and m for minutes; excel: selects
 * Excel's contextual month/minute interpretation. Calendar presentations dispatch to their owning native renderers.
 * A minimal perfect hash stores twenty-one native names in twenty-one slots. A bounded per-thread locale cache
 * owns immutable jurisdiction snapshots and closes database handles after loading; warm rendering uses no SQL.
 * Accessors may materialise the datetime's lazy calendar caches, so shared mutable values require synchronisation.
 */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#include "datetime-t.h"
#include "jurisdiction-t.h"

typedef struct {
    const char *name;
    const char *pattern;
    string_t *(*calendar)(const datetime_t *);
} datetime_fmt_style_t;

static uint32_t datetime_fmt_hash(const char *name, uint32_t seed)
{
    uint32_t hash = seed;
    for (const unsigned char *at = (const unsigned char *)name; *at; ++at)
        hash = (hash ^ *at) * UINT32_C(16777619);
    return hash ^ (hash >> 16u);
}

static const datetime_fmt_style_t *datetime_fmt_find(const char *style)
{
    static const int8_t displacement[21] = {
        0, 0, -2, -7, 0, 0, 1, 0, 2, 1, 0, -9, -11, -13, -16, -18, -19, 0, 0, 3, 0,
    };
    static const datetime_fmt_style_t styles[21] = {
        [0]  = {"muslim",    NULL,                        datetime_muslim_calendar_date_text},
        [1]  = {"christian", NULL,                        datetime_christian_calendar_date_text},
        [2]  = {"aztec",     NULL,                        datetime_aztec_calendar_date_text},
        [3]  = {"iso",       "%yyyy-%mm-%ddT@Hh:@mm:@ss", NULL},
        [4]  = {"chinese",   NULL,                        datetime_chinese_calendar_date_text},
        [5]  = {"M",         "%m",                       NULL},
        [6]  = {"time",      "@Hh:@mm:@ss",              NULL},
        [7]  = {"datetime",  "%yyyy-%mm-%ddT@Hh:@mm:@ss", NULL},
        [8]  = {"date",      "%yyyy-%mm-%dd",            NULL},
        [9]  = {"h",         "@Hh",                      NULL},
        [10] = {"jewish",    NULL,                        datetime_jewish_calendar_date_text},
        [11] = {"y",         "%yy",                      NULL},
        [12] = {"m",         "@m",                       NULL},
        [13] = {"ethiopian", NULL,                        datetime_ethiopian_calendar_date_text},
        [14] = {"hindu",     NULL,                        datetime_hindu_calendar_date_text},
        [15] = {"buddhist",  NULL,                        datetime_buddhist_calendar_date_text},
        [16] = {"d",         "%d",                       NULL},
        [17] = {"s",         "@s",                       NULL},
        [18] = {"cherokee",  NULL,                        datetime_cherokee_calendar_date_text},
        [19] = {"lingua",    NULL,                        NULL},
        [20] = {"mayan",     NULL,                        datetime_mayan_calendar_date_text},
    };
    _Static_assert(sizeof(styles) / sizeof(*styles) == sizeof(displacement), "Regenerate the minimal perfect hash.");
    if (!*style)
        return &styles[19];
    int place = displacement[datetime_fmt_hash(style, UINT32_C(2166136261)) % 21u];
    size_t slot = place < 0 ? (size_t)(-place - 1) : datetime_fmt_hash(style, (uint32_t)place) % 21u;
    return strcmp(style, styles[slot].name) ? NULL : &styles[slot];
}

typedef struct {
    char name[64];
    jurisdict_date_locale_t *locale;
    bool loaded;
} datetime_fmt_locale_t;

typedef struct {
    datetime_fmt_locale_t entries[8];
    datetime_fmt_locale_t *last;
    size_t next;
} datetime_fmt_locales_t;

static _Thread_local datetime_fmt_locales_t *datetime_fmt_locales;
static pthread_key_t datetime_fmt_locale_key;
static pthread_once_t datetime_fmt_locale_once = PTHREAD_ONCE_INIT;
static bool datetime_fmt_locale_ready;

static void datetime_fmt_locale_destroy(void *data)
{
    datetime_fmt_locales_t *cache = data;
    if (!cache)
        return;
    for (size_t i = 0; i < 8u; ++i)
        jurisdict_date_locale_free(cache->entries[i].locale);
    if (datetime_fmt_locales == cache)
        datetime_fmt_locales = NULL;
    free(cache);
}

/* Drop this thread's bounded presentation snapshots after configuration changes. */
void jurisdict_date_locale_cache_clear(void)
{
    if (datetime_fmt_locales) {
        pthread_setspecific(datetime_fmt_locale_key, NULL);
        datetime_fmt_locale_destroy(datetime_fmt_locales);
    }
}

static void datetime_fmt_locale_init(void)
{
    if (pthread_key_create(&datetime_fmt_locale_key, datetime_fmt_locale_destroy) != 0)
        return;
    if (atexit(jurisdict_date_locale_cache_clear) != 0) {
        pthread_key_delete(datetime_fmt_locale_key);
        return;
    }
    datetime_fmt_locale_ready = true;
}

static datetime_fmt_locale_t *datetime_fmt_locale_get(const char *name)
{
    if (!datetime_fmt_locales) {
        pthread_once(&datetime_fmt_locale_once, datetime_fmt_locale_init);
        if (!datetime_fmt_locale_ready)
            return NULL;
        datetime_fmt_locales_t *cache = calloc(1u, sizeof(*cache));
        if (!cache || pthread_setspecific(datetime_fmt_locale_key, cache) != 0) {
            free(cache);
            return NULL;
        }
        datetime_fmt_locales = cache;
    }
    datetime_fmt_locales_t *cache = datetime_fmt_locales;
    if (cache->last && !strcmp(cache->last->name, name))
        return cache->last;
    /* Only cache misses inspect the other seven entries; storage never grows with dates or print calls. */
    for (size_t i = 0; i < 8u; ++i) {
        if (cache->entries[i].loaded && !strcmp(cache->entries[i].name, name))
            return cache->last = &cache->entries[i];
    }
    datetime_fmt_locale_t *entry = &cache->entries[cache->next++ % 8u];
    jurisdict_date_locale_free(entry->locale);
    *entry = (datetime_fmt_locale_t){.loaded = true};
    memcpy(entry->name, name, strlen(name) + 1u);
    jurisdiction_t *engine = jurisdict_open(NULL);
    entry->locale = engine ? jurisdict_date_locale_load(engine, name) : NULL;
    jurisdict_close(engine);
    return cache->last = entry;
}

static bool datetime_fmt_locale_name(const char *name)
{
    size_t length = strlen(name);
    if (length < 5u || length >= 64u || name[0] < 'a' || name[0] > 'z' || name[1] < 'a' || name[1] > 'z')
        return false;
    size_t at = 2u;
    if (name[at] >= 'a' && name[at] <= 'z')
        ++at;
    if (name[at++] != '_')
        return false;
    for (; at < length; ++at) {
        unsigned char c = (unsigned char)name[at];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    }
    return true;
}

static bool datetime_fmt_validate(const fmt_options_t *options, fmt_error_t *error)
{
    if ((!options->excel && !datetime_fmt_find(options->presentation) &&
         !datetime_fmt_locale_name(options->presentation)) || options->precision >= 0) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported datetime presentation or precision"};
        return false;
    }
    return true;
}

static string_t *datetime_fmt_render(const void *data, const fmt_options_t *options, fmt_error_t *error)
{
    const datetime_t *value;
    memcpy(&value, data, sizeof(value));
    if (!value) {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_ARGUMENT, SIZE_MAX, SIZE_MAX, "datetime pointer is NULL"};
        return NULL;
    }
    if (options->excel)
        return fmt_excel_datetime(options, datetime_year(value), (unsigned)datetime_month(value), datetime_day(value),
                                   datetime_hour(value), datetime_minute(value), datetime_second(value), error);
    const datetime_fmt_style_t *style = datetime_fmt_find(options->presentation);
    if (!style || (!style->calendar && !style->pattern)) {
        datetime_fmt_locale_t *locale = datetime_fmt_locale_get(style ? "" : options->presentation);
        if (!locale || (!locale->locale && !style)) {
            if (error)
                *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "Lingua locale is unavailable"};
            return NULL;
        }
        if (locale->locale)
            return jurisdict_date_locale_text(locale->locale, value);
        /* A machine without an installed locale catalogue retains useful, deterministic datetime output. */
        style = datetime_fmt_find("iso");
    }
    if (style->calendar)
        return style->calendar(value);
    string_t *pattern = string_new_with(style->pattern);
    string_t *out = pattern ? datetime_format_text(value, pattern) : NULL;
    string_free(pattern);
    return out;
}

static const fmt_adapter_t datetime_fmt_adapter = {
    .validate = datetime_fmt_validate,
    .render = datetime_fmt_render,
    .numeric = false,
};

/* Borrow a civil datetime for the module's native calendar renderer. */
fmt_arg_t fmt_arg_datetime(const datetime_t *value)
{
    return fmt_arg_adapter(&datetime_fmt_adapter, &value, sizeof(value));
}
