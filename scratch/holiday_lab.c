#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datetime.h"
#include "holiday.h"
#include "ustring.h"

typedef struct holiday_lab_options_t {
    short start_year;
    month_t start_month;
    uint8_t start_day;
    short end_year;
    month_t end_month;
    uint8_t end_day;
    char jurisdiction[32];
} holiday_lab_options_t;

static bool parse_date_text(const char *text, short *year, month_t *month, uint8_t *day)
{
    int y;
    int m;
    int d;
    char tail;

    if (!text || !year || !month || !day)
        return false;
    if (sscanf(text, "%d-%d-%d%c", &y, &m, &d, &tail) != 3)
        return false;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    if (!datetime_valid_ymd((short)y, (month_t)m, (uint8_t)d))
        return false;

    *year = (short)y;
    *month = (month_t)m;
    *day = (uint8_t)d;
    return true;
}

static bool key_equals(const char *got, size_t got_len, const char *want)
{
    return strlen(want) == got_len && strncmp(got, want, got_len) == 0;
}

static void init_defaults(holiday_lab_options_t *options)
{
    options->start_year = 2026;
    options->start_month = DT_January;
    options->start_day = 1;
    options->end_year = 2026;
    options->end_month = DT_December;
    options->end_day = 31;
    options->jurisdiction[0] = '\0';
}

static bool apply_arg(holiday_lab_options_t *options, const char *arg)
{
    const char *equals = strchr(arg, '=');
    const char *value;
    size_t key_len;
    short y;
    month_t m;
    uint8_t d;

    if (!equals)
        return false;

    key_len = (size_t)(equals - arg);
    value = equals + 1;

    if (key_equals(arg, key_len, "start")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->start_year = y;
        options->start_month = m;
        options->start_day = d;
        return true;
    }
    if (key_equals(arg, key_len, "end")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->end_year = y;
        options->end_month = m;
        options->end_day = d;
        return true;
    }
    if (key_equals(arg, key_len, "jurisdiction")) {
        size_t value_len = strlen(value);

        if (value_len == 0 || value_len >= sizeof(options->jurisdiction))
            return false;
        memcpy(options->jurisdiction, value, value_len + 1u);
        return true;
    }

    return false;
}

static char *format_display_date(const datetime_t *dttm)
{
    string_t *format = NULL;
    string_t *formatted = NULL;
    const char *c_text = NULL;
    char *out = NULL;

    if (!dttm)
        return NULL;
    format = string_new_with("%Dddd %d%q %Mmmm %yyyy");
    formatted = format ? datetime_format_text(dttm, format) : NULL;
    c_text = formatted ? string_c_str(formatted) : NULL;

    if (c_text) {
        size_t len = strlen(c_text);
        out = malloc(len + 1u);
        if (out)
            memcpy(out, c_text, len + 1u);
    }

    string_free(formatted);
    string_free(format);
    return out;
}

static bool print_bank_holidays_from_database(const char *jurisdiction,
                                              const datetime_t *start,
                                              const datetime_t *end)
{
    holiday_t *holiday = NULL;
    array_t *events = NULL;
    bool produced = false;
    size_t i;

    if (!start || !end)
        return false;

    holiday = holiday_open(jurisdiction);
    if (!holiday)
        goto done;

    events = holiday_between(holiday, start, end);
    if (!events)
        goto done;

    for (i = 0u; i < array_size(events); ++i) {
        const holiday_event_t *event = array_get(events, i);
        char *display_date;

        if (!event)
            continue;
        display_date = format_display_date(event->holiday_date);
        printf("bank_holiday %s: %s\n",
               event->holiday_name ? event->holiday_name : "unavailable",
               display_date ? display_date : "unavailable");
        free(display_date);
        produced = true;
    }

done:
    array_destroy(events);
    holiday_close(holiday);
    return produced;
}

int main(int argc, char **argv)
{
    holiday_lab_options_t options;
    datetime_t *start = NULL;
    datetime_t *end = NULL;
    bool produced = false;
    int i;

    init_defaults(&options);
    for (i = 1; i < argc; ++i) {
        if (!apply_arg(&options, argv[i])) {
            fprintf(stderr, "Bad holiday argument: %s\n", argv[i]);
            return 2;
        }
    }

    start = datetime_init_ymd(datetime_alloc(),
                              options.start_year,
                              options.start_month,
                              options.start_day);
    end = datetime_init_ymd(datetime_alloc(),
                            options.end_year,
                            options.end_month,
                            options.end_day);
    if (!start || !end) {
        datetime_dealloc(start);
        datetime_dealloc(end);
        fprintf(stderr, "Failed to initialise holiday date range\n");
        return 1;
    }

    produced = print_bank_holidays_from_database(options.jurisdiction, start, end);
    printf("holiday_status %s\n", produced ? "ok" : "unavailable");

    datetime_dealloc(start);
    datetime_dealloc(end);
    return 0;
}
