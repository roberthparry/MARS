/**
 * @file lab_worker_plot_prepared.c
 * @brief Bounded native transport of solved curves and family choices between isolated requests.
 *
 * The existing server supplies a single JSON record through standard input. Keys
 * include authored input, selected axis, source mode and worksheet precision.
 * Completed equation evaluations can publish the same records before their first plot request.
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
    /* The standard-input alias is a system symlink; the file API still requires its target to be regular. */
    bool ok = input && text && file_open_follow(input, FILE_MODE_OPEN, FILE_ACCESS_READ);
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
    for (size_t i = 4; ok && i < 7; ++i)
        ok = item(record, i) != NULL;
    const json_t *results = json_array_get(record, 7);
    const json_t *roots = json_array_get(results, 1);
    const char *numeric = item(record, 7);
    if (!numeric)
        numeric = item(results, 0);
    size_t root_count = roots ? json_array_size(roots) : 0;
    ok = ok && numeric && root_count <= 256 && (!roots || json_array_size(results) == 2);
    lab_plot_prepared_t *prepared = ok ? calloc(1, sizeof(*prepared)) : NULL;
    if (prepared) {
        prepared->title = string_new_with(item(record, 4));
        prepared->TeX = string_new_with(item(record, 5));
        prepared->family = !strcmp(item(record, 6), "1");
        prepared->numeric = string_new_with(numeric);
        prepared->roots = root_count ? calloc(root_count, sizeof(*prepared->roots)) : NULL;
        prepared->count = json_array_size(record) - 8;
        ok = prepared->title && prepared->TeX && prepared->numeric && (!root_count || prepared->roots);
        for (size_t i = 0; ok && i < root_count; ++i) {
            const char *value = item(roots, i);
            prepared->roots[i] = value ? num_create_from_string(value) : NUM_NAN;
            ++prepared->root_count;
            ok = num_is_finite(prepared->roots[i]);
        }
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

/* Share completed equation evaluations with the same replay path used by later viewport changes. */
void lab_worker_plot_prepared_emit(const char *const keys[4], const char *title, const char *TeX,
                                   const expr_t *const *curves, string_t *const *labels, size_t count,
                                   bool family, const string_t *numeric, const number_t *roots, size_t root_count)
{
    if (!keys || !curves || !count || count > 16 || root_count > 256 || (root_count && !roots))
        return;
    json_t *record = json_new_array();
    json_t *results = json_new_array(), *values = json_new_array();
    bool packed = results && values && append(results, numeric ? string_c_str(numeric) : "");
    for (size_t i = 0; packed && i < root_count; ++i) {
        string_t *value = num_is_finite(roots[i]) ? num_to_string(roots[i]) : NULL;
        packed = value && append(values, string_c_str(value));
        string_free(value);
    }
    packed = packed && json_array_append(results, values);
    bool ok = record && keys[0] && keys[1] && keys[2] && keys[3] &&
              append(record, keys[0]) && append(record, keys[1]) && append(record, keys[2]) &&
              append(record, keys[3]) && append(record, title) && append(record, TeX) &&
              append(record, family ? "1" : "0") && packed && json_array_append(record, results);
    json_free(values);
    json_free(results);
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

/* Publish only when the process adapter has enabled its bounded preparation transport. */
void lab_worker_plot_prepared_write(int argc, char **argv, const char *title, const char *TeX,
                                    const expr_t *const *curves, string_t *const *labels, size_t count,
                                    bool family, const string_t *numeric, const number_t *roots, size_t root_count)
{
    if (argc != 20 || strcmp(argv[19], "--plot-cache"))
        return;
    const char *keys[] = {argv[1], argv[2], argv[3], argv[18]};
    lab_worker_plot_prepared_emit(keys, title, TeX, curves, labels, count, family, numeric, roots, root_count);
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
    for (size_t i = 0; i < prepared->root_count; ++i)
        num_destroy(&prepared->roots[i]);
    free(prepared->roots);
    free(prepared);
}
