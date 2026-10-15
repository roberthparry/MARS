/**
 * @file lab_worker_plot_prepared.c
 * @brief Bounded native transport of solved curves and family choices between isolated requests.
 *
 * The existing server supplies a single JSON record through standard input. Keys
 * include authored input, selected axis, source mode and worksheet precision.
 * Only successfully parsed native expressions are replayed; absent, oversized or
 * mismatched records fall back to solving. No files are persisted by this module.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "json-t.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_plot_prepared.h"

static const char *item(const json_t *array, size_t index)
{
    const string_t *text = json_string_value(json_array_get(array, index));
    return text ? string_c_str(text) : NULL;
}

static bool append(json_t *array, const char *value)
{
    string_t *text = string_new_with(value ? value : "");
    json_t *entry = text ? json_new_string(text) : NULL;
    bool ok = entry && json_array_append(array, entry);
    json_free(entry);
    string_free(text);
    return ok;
}

/* This is an optional optimisation, never a requirement for plotting. */
lab_plot_prepared_t *lab_worker_plot_prepared_read(int argc, char **argv)
{
    if (argc != 20 || strcmp(argv[19], "--plot-cache"))
        return NULL;
    file_t *input = file_new_cstr("/dev/stdin");
    string_t *text = string_new();
    bool ok = input && text && file_open_read(input);
    char buffer[4096];
    size_t got = 0;
    while (ok) {
        ok = file_read(input, buffer, sizeof(buffer), &got);
        if (!ok || !got)
            break;
        ok = string_byte_length(text) + got < 256 * 1024 && !string_append_chars(text, buffer, got);
    }
    file_free(input);
    json_t *record = ok && string_byte_length(text) ? json_from_text(text) : NULL;
    string_free(text);
    const size_t keys[] = {1, 2, 3, 18};
    ok = record && json_array_size(record) >= 9 && json_array_size(record) <= 24;
    for (size_t i = 0; ok && i < 4; ++i)
        ok = item(record, i) && !strcmp(item(record, i), argv[keys[i]]);
    for (size_t i = 4; ok && i < 8; ++i)
        ok = item(record, i) != NULL;
    lab_plot_prepared_t *prepared = ok ? calloc(1, sizeof(*prepared)) : NULL;
    if (prepared) {
        prepared->title = string_new_with(item(record, 4));
        prepared->TeX = string_new_with(item(record, 5));
        prepared->family = !strcmp(item(record, 6), "1");
        prepared->numeric = string_new_with(item(record, 7));
        prepared->count = json_array_size(record) - 8;
        ok = prepared->title && prepared->TeX && prepared->numeric;
        for (size_t i = 0; ok && i < prepared->count; ++i) {
            const json_t *curve = json_array_get(record, i + 8);
            ok = json_array_size(curve) == 2 && item(curve, 0) && item(curve, 1);
            if (ok) {
                prepared->curves[i] = expr_from_string(item(curve, 0), NULL);
                prepared->labels[i] = string_new_with(item(curve, 1));
                ok = prepared->curves[i] && prepared->labels[i];
            }
        }
    }
    json_free(record);
    if (!ok) {
        lab_worker_plot_prepared_free(prepared);
        return NULL;
    }
    return prepared;
}

/* Publish only serialised native graphs and presentation; no browser-side algebra is involved. */
void lab_worker_plot_prepared_write(int argc, char **argv, const char *title, const char *TeX,
                                    const expr_t *const *curves, string_t *const *labels, size_t count,
                                    bool family, const string_t *numeric)
{
    if (argc != 20 || strcmp(argv[19], "--plot-cache") || !count || count > 16)
        return;
    json_t *record = json_new_array();
    bool ok = record && append(record, argv[1]) && append(record, argv[2]) && append(record, argv[3]) &&
              append(record, argv[18]) && append(record, title) && append(record, TeX) &&
              append(record, family ? "1" : "0") && append(record, numeric ? string_c_str(numeric) : "");
    for (size_t i = 0; ok && i < count; ++i) {
        char *source = expr_to_string(curves[i], style_EXPRESSION);
        json_t *curve = json_new_array();
        ok = source && curve && append(curve, source) && append(curve, labels ? string_c_str(labels[i]) : "") &&
             json_array_append(record, curve);
        json_free(curve);
        free(source);
    }
    string_t *text = ok ? json_to_string(record) : NULL;
    if (text && string_byte_length(text) < 256 * 1024)
        printf("plot_prepared %s\n", string_c_str(text));
    string_free(text);
    json_free(record);
}

/* Release independent graph and string owners after a replay. */
void lab_worker_plot_prepared_free(lab_plot_prepared_t *prepared)
{
    if (!prepared)
        return;
    for (size_t i = 0; i < prepared->count; ++i) {
        expr_free(prepared->curves[i]);
        string_free(prepared->labels[i]);
    }
    string_free(prepared->title);
    string_free(prepared->TeX);
    string_free(prepared->numeric);
    free(prepared);
}
