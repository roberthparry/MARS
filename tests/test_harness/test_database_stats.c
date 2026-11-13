/**
 * @file test_database_stats.c
 * @brief Read-only test statistics snapshots formatted as aligned grids, Markdown or TSV.
 *
 * One query supplies every presentation. A display snapshot avoids repeating the metrics view's
 * median calculation. Automatic output uses the Unicode grid; Markdown and TSV are explicit portable exports.
 * Compact tables group cases by source; details retain every original metric. Unicode grids wrap whole
 * grapheme clusters and measure terminal columns using a thread-local UTF-8 locale. No test data is changed.
 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>

#include "string-t.h"
#include "array-t.h"
#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

enum {
    SOURCE, PATH, LAST_RUN, OUTCOME, LAST_MS, NORMAL_COUNT, ORDINARY_COUNT, LAST_NORMAL_MS, PREVIOUS_MS,
    TOOK_LONGER, CHANGE_MS, CHANGE_PERCENT, FASTEST_MS, SLOWEST_MS, MEAN_MS, MEDIAN_MS,
    OUTLIER_COUNT, LAST_OUTLIER, OUTLIER_UTC, OUTLIER_MS, MEMORY_UTC, FIELD_COUNT
};

static const struct {
    const char *key, *label;
    bool numeric;
} fields[FIELD_COUNT] = {
    [SOURCE]         = {"source",                  "Source",                  false},
    [PATH]           = {"path",                    "Test",                    false},
    [LAST_RUN]       = {"last_run_utc",            "Last run (UTC)",          false},
    [OUTCOME]        = {"last_outcome",            "Result",                  false},
    [LAST_MS]        = {"last_elapsed_ms",         "Last ms",                 true},
    [NORMAL_COUNT]   = {"normal_count",            "Samples",                 true},
    [ORDINARY_COUNT] = {"ordinary_count",          "Ordinary runs",           true},
    [LAST_NORMAL_MS] = {"last_normal_ms",          "Last ordinary ms",        true},
    [PREVIOUS_MS]    = {"previous_normal_ms",      "Previous ordinary ms",    true},
    [TOOK_LONGER]    = {"took_longer",             "Last ordinary slower",    false},
    [CHANGE_MS]      = {"change_ms",               "Change ms",               true},
    [CHANGE_PERCENT] = {"change_percent",          "Change %",                true},
    [FASTEST_MS]     = {"fastest_ms",              "Fastest ms",              true},
    [SLOWEST_MS]     = {"slowest_ms",              "Slowest ms",              true},
    [MEAN_MS]        = {"mean_ms",                 "Mean ms",                 true},
    [MEDIAN_MS]      = {"median_ms",               "Median ms",               true},
    [OUTLIER_COUNT]  = {"timing_outlier_count",     "Timing outliers",         true},
    [LAST_OUTLIER]   = {"last_normal_outlier",      "Last ordinary an outlier", false},
    [OUTLIER_UTC]    = {"last_timing_outlier_utc",   "Last outlier (UTC)",      false},
    [OUTLIER_MS]     = {"last_timing_outlier_ms",    "Last outlier ms",         true},
    [MEMORY_UTC]     = {"last_memory_test_utc",      "Last memory test (UTC)",  false},
};

typedef struct {
    string_t *cells[FIELD_COUNT];
    size_t widths[FIELD_COUNT];
    const char *change_style;
} stats_row_t;

static void row_free(void *value)
{
    stats_row_t *row = value;
    for (size_t i = 0; i < FIELD_COUNT; ++i) string_free(row->cells[i]);
}

static size_t rune_columns(rune_t rune)
{
    uint32_t value = rune_value(rune);
    /* A flag is one grapheme containing two regional indicators, displayed in two terminal columns. */
    if (value >= 0x1f1e6 && value <= 0x1f1ff) return 2;
    int width = wcwidth((wchar_t)value);
    return width < 0 ? 1 : (size_t)width;
}

static size_t columns(const string_t *text)
{
    size_t width = 0;
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor) return SIZE_MAX;
    while (!string_cursor_done(cursor)) {
        width += rune_columns(string_cursor_peek(cursor));
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    return width;
}

static string_t *safe_text(const char *text)
{
    string_t *source = string_new(), *out = string_new();
    bool ok = source && out && !string_append_utf8_exact(source, text, strlen(text));
    string_cursor_t *cursor = ok ? string_cursor_new(source) : NULL;
    ok = ok && cursor;
    while (ok && !string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        uint32_t value = rune_value(rune);
        string_pos_t start = string_cursor_position(cursor);
        string_cursor_next(cursor);
        if (rune_is_control(rune))
            ok = string_append_format(out, "\\x%02x", value) >= 0;
        else
            ok = !string_cursor_append_slice_between(out, start, string_cursor_position(cursor), cursor);
    }
    string_cursor_free(cursor); string_free(source);
    if (!ok) { string_free(out); out = NULL; }
    return out;
}

static size_t terminal_columns(void)
{
    struct winsize window = {0};
    if (!ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) && window.ws_col)
        return window.ws_col < 60 ? 60 : window.ws_col > 200 ? 200 : window.ws_col;
    const char *value = getenv("COLUMNS");
    char *end = NULL;
    unsigned long width = value ? strtoul(value, &end, 10) : 0;
    if (value && end && !*end && width >= 60 && width <= 200) return (size_t)width;
    if (!ioctl(STDERR_FILENO, TIOCGWINSZ, &window) && window.ws_col)
        return window.ws_col < 60 ? 60 : window.ws_col > 200 ? 200 : window.ws_col;
    return 132;
}

static bool terminal_emphasis(void)
{
    const char *disabled = getenv("NO_COLOR"), *term = getenv("TERM"), *force = getenv("CLICOLOR_FORCE");
    /* The native grid retains its styles through ANSI-aware pagers as well as direct terminal output. */
    return (!disabled || !*disabled) && (!term || strcmp(term, "dumb")) &&
           (isatty(STDOUT_FILENO) || (force && *force && strcmp(force, "0")) ||
            (term && *term));
}

static void markdown_cell(const char *text)
{
    static const char *const escapes[128] = {
        ['&']  = "&amp;",
        ['*']  = "\\*",
        ['<']  = "&lt;",
        ['>']  = "&gt;",
        ['[']  = "\\[",
        ['\\'] = "\\\\",
        [']']  = "\\]",
        ['_']  = "\\_",
        ['`']  = "\\`",
        ['|']  = "\\|",
    };
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        const char *escaped = *p < 128 ? escapes[*p] : NULL;
        if (escaped) fputs(escaped, stdout);
        else putchar(*p);
    }
}

static const char *outcome_style(const string_t *text)
{
    /* These three fixed labels are the complete supported set; never derive escapes from stored text. */
    const char *value = string_c_str(text);
    if (!strcmp(value, "PASS")) return "\033[1;92m";
    if (!strcmp(value, "FAIL")) return "\033[1;91m";
    if (!strcmp(value, "SKIPPED")) return "\033[1;93m";
    return NULL;
}

static const char *grid_cell_style(const stats_row_t *row, unsigned field)
{
    static const char *const styles[FIELD_COUNT] = {
        [SOURCE]    = "\033[1;93m",
        [PATH]      = "\033[96m",
        [MEAN_MS]   = "\033[1;96m",
        [MEDIAN_MS] = "\033[1;93m",
    };
    if (field == OUTCOME) return outcome_style(row->cells[field]);
    if (field == CHANGE_PERCENT) return row->change_style;
    return styles[field];
}

static void markdown_outcome(const string_t *text)
{
    fputs("**", stdout);
    markdown_cell(string_c_str(text));
    fputs("**", stdout);
}

static void grid_rule(const size_t *widths, size_t count, const char *left, const char *line,
                      const char *middle, const char *right)
{
    fputs(left, stdout);
    for (size_t i = 0; i < count; ++i) {
        for (size_t column = 0; column < widths[i] + 2; ++column) fputs(line, stdout);
        fputs(i + 1 == count ? right : middle, stdout);
    }
    putchar('\n');
}

static bool grid_row(string_t *const *cells, const size_t *widths, const bool *numeric, size_t count,
                     const char *const *styles)
{
    string_cursor_t *cursors[9] = {0};
    bool ok = count <= sizeof cursors / sizeof *cursors;
    for (size_t i = 0; ok && i < count; ++i) ok = (cursors[i] = string_cursor_new(cells[i])) != NULL;
    bool more = true;
    while (ok && more) {
        more = false;
        fputs("║", stdout);
        for (size_t i = 0; i < count; ++i) {
            size_t used = 0, start = string_cursor_position(cursors[i]);
            while (!string_cursor_done(cursors[i])) {
                size_t next = rune_columns(string_cursor_peek(cursors[i]));
                if (used + next > widths[i]) break;
                used += next;
                string_cursor_next(cursors[i]);
            }
            size_t end = string_cursor_position(cursors[i]);
            size_t padding = widths[i] - used;
            if (numeric && numeric[i]) printf(" %*s", (int)padding, "");
            else putchar(' ');
            if (styles && styles[i]) fputs(styles[i], stdout);
            printf("%.*s", (int)(end - start), string_c_str(cells[i]) + start);
            if (styles && styles[i]) fputs("\033[0m", stdout);
            if (!numeric || !numeric[i]) printf("%*s", (int)padding, "");
            fputs(i + 1 == count ? " ║" : " │", stdout);
            more = more || !string_cursor_done(cursors[i]);
        }
        putchar('\n');
    }
    for (size_t i = 0; i < count && i < sizeof cursors / sizeof *cursors; ++i) string_cursor_free(cursors[i]);
    return ok;
}

static bool print_details(const array_t *rows, bool markdown, size_t width, bool emphasis)
{
    size_t widths[] = {27, width - 34};
    for (size_t row = 0; row < array_size(rows); ++row) {
        const stats_row_t *entry = array_get(rows, row);
        putchar('\n');
        if (markdown) fputs("| **Metric** | **Value** |\n| :--- | :--- |\n", stdout);
        else {
            string_t *headers[] = {string_new_with("Metric"), string_new_with("Value")};
            const char *styles[] = {emphasis ? "\033[1m" : NULL, emphasis ? "\033[1m" : NULL};
            grid_rule(widths, 2, "╔", "═", "╤", "╗");
            bool ok = headers[0] && headers[1] && grid_row(headers, widths, NULL, 2, styles);
            string_free(headers[0]); string_free(headers[1]);
            if (!ok) return false;
            grid_rule(widths, 2, "╠", "═", "╪", "╣");
        }
        for (size_t i = 0; i < FIELD_COUNT; ++i) {
            if (markdown) {
                fputs("| ", stdout); markdown_cell(fields[i].label); fputs(" | ", stdout);
                if (i == OUTCOME) markdown_outcome(entry->cells[i]);
                else markdown_cell(string_c_str(entry->cells[i]));
                fputs(" |\n", stdout);
            } else {
                string_t *label = string_new_with(fields[i].label);
                string_t *cells[] = {label, entry->cells[i]};
                bool numeric[] = {false, fields[i].numeric};
                const char *styles[] = {NULL, emphasis ? grid_cell_style(entry, (unsigned)i) : NULL};
                bool ok = label && grid_row(cells, widths, numeric, 2, styles);
                string_free(label);
                if (!ok) return false;
                if (i + 1 == FIELD_COUNT) grid_rule(widths, 2, "╚", "═", "╧", "╝");
                else grid_rule(widths, 2, "╟", "─", "┼", "╢");
            }
        }
    }
    return true;
}

static bool print_summary(const array_t *rows, bool markdown, size_t width, bool emphasis)
{
    static const unsigned selected[] = {PATH, OUTCOME, LAST_MS, CHANGE_PERCENT, FASTEST_MS, SLOWEST_MS,
                                         MEAN_MS, MEDIAN_MS, NORMAL_COUNT};
    size_t widths[9] = {0}, fixed = 28;
    bool numeric[9] = {0}, ok = true;
    string_t *headers[9] = {0};
    const char *header_styles[9] = {0};
    for (size_t i = 0; i < 9; ++i) {
        headers[i] = string_new_with(fields[selected[i]].label);
        ok = ok && headers[i];
        widths[i] = strlen(fields[selected[i]].label);
        numeric[i] = fields[selected[i]].numeric;
        header_styles[i] = emphasis ? "\033[1m" : NULL;
        for (size_t j = 0; j < array_size(rows); ++j) {
            const stats_row_t *entry = array_get(rows, j);
            if (entry->widths[selected[i]] > widths[i]) widths[i] = entry->widths[selected[i]];
        }
        if (i) fixed += widths[i];
    }
    if (!ok || (width <= fixed + 24 && !markdown)) {
        for (size_t i = 0; i < 9; ++i) string_free(headers[i]);
        return ok && print_details(rows, false, width, emphasis);
    }
    if (widths[0] > 64) widths[0] = 64;
    if (!markdown && widths[0] > width - fixed) widths[0] = width - fixed;
    const char *source = NULL;
    for (size_t j = 0; ok && j < array_size(rows); ++j) {
        const stats_row_t *entry = array_get(rows, j);
        const char *next = string_c_str(entry->cells[SOURCE]);
        if (!source || strcmp(source, next)) {
            if (source && !markdown) grid_rule(widths, 9, "╚", "═", "╧", "╝");
            putchar('\n');
            if (emphasis) fputs(grid_cell_style(entry, SOURCE), stdout);
            fputs(markdown ? "## " : "Source: ", stdout);
            if (markdown) markdown_cell(next);
            else fputs(next, stdout);
            if (emphasis) fputs("\033[0m", stdout);
            putchar('\n');
            source = next;
            if (markdown) {
                putchar('\n');
                for (size_t i = 0; i < 9; ++i) {
                    fputs("| **", stdout); markdown_cell(string_c_str(headers[i])); fputs("** ", stdout);
                }
                fputs("|\n", stdout);
                for (size_t i = 0; i < 9; ++i) fputs(numeric[i] ? "| ---: " : "| :--- ", stdout);
                fputs("|\n", stdout);
            } else {
                grid_rule(widths, 9, "╔", "═", "╤", "╗");
                ok = grid_row(headers, widths, NULL, 9, header_styles);
                grid_rule(widths, 9, "╠", "═", "╪", "╣");
            }
        } else if (!markdown) grid_rule(widths, 9, "╟", "─", "┼", "╢");
        string_t *cells[9];
        for (size_t i = 0; i < 9; ++i) cells[i] = entry->cells[selected[i]];
        if (markdown) {
            for (size_t i = 0; i < 9; ++i) {
                fputs("| ", stdout);
                if (selected[i] == OUTCOME) markdown_outcome(cells[i]);
                else markdown_cell(string_c_str(cells[i]));
                putchar(' ');
            }
            fputs("|\n", stdout);
        } else {
            const char *styles[9] = {0};
            for (size_t i = 0; emphasis && i < 9; ++i) styles[i] = grid_cell_style(entry, selected[i]);
            ok = ok && grid_row(cells, widths, numeric, 9, styles);
        }
    }
    if (source && !markdown) grid_rule(widths, 9, "╚", "═", "╧", "╝");
    for (size_t i = 0; i < 9; ++i) string_free(headers[i]);
    return ok;
}

/* Presentation owns its snapshot and locale, and never changes a recorded result or timing. */
bool test_database_stats_print(test_database_t *database, const char *prefix, test_database_stats_format_t format,
                               bool details)
{
    if (format == TEST_DATABASE_STATS_AUTO)
        format = TEST_DATABASE_STATS_GRID;
    locale_t utf8 = newlocale(LC_CTYPE_MASK, "C.UTF-8", (locale_t)0);
    locale_t previous = utf8 ? uselocale(utf8) : (locale_t)0;
    if (!utf8 || !previous) { if (utf8) freelocale(utf8); return test_database_fail(database, "UTF-8 locale unavailable"); }
    array_t *rows = array_create(sizeof(stats_row_t), NULL, row_free);
    sqlite3_stmt *query = rows ? test_database_prepare(database,
        "SELECT source, path, last_run_utc, last_outcome, last_elapsed_ms, normal_count, ordinary_count,\n"
        "       last_normal_ms, previous_normal_ms, took_longer, change_ms, change_percent,\n"
        "       fastest_ms, slowest_ms, mean_ms, median_ms, timing_outlier_count, last_normal_outlier,\n"
        "       last_timing_outlier_utc, last_timing_outlier_ms, last_memory_test_utc\n"
        "FROM test_case_metrics\n"
        "WHERE run_count > 0 AND (?1 = '' OR substr(source, 1, length(?1)) = ?1)\n"
        "ORDER BY source, path") : NULL;
    bool ok = query && sqlite3_bind_text(query, 1, prefix ? prefix : "", -1, SQLITE_TRANSIENT) == SQLITE_OK;
    int status = SQLITE_DONE;
    while (ok && (status = sqlite3_step(query)) == SQLITE_ROW) {
        stats_row_t row = {0};
        const char *change_marker = NULL;
        if (sqlite3_column_type(query, CHANGE_PERCENT) != SQLITE_NULL) {
            static const struct { const char *style, *marker; } trends[] = {
                {"\033[1;92m", "▲"},
                {"\033[1;38;2;255;140;0m", "▬"},
                {"\033[1;91m", "▼"},
            };
            /* Use the unrounded metric: this display-only band does not alter the regression policy or timings. */
            double change = sqlite3_column_double(query, CHANGE_PERCENT);
            unsigned trend = change < -5 ? 0 : change > 5 ? 2 : 1;
            row.change_style = trends[trend].style;
            change_marker = trends[trend].marker;
        }
        for (size_t i = 0; ok && i < FIELD_COUNT; ++i) {
            int type = sqlite3_column_type(query, (int)i);
            if (i == CHANGE_PERCENT && format == TEST_DATABASE_STATS_GRID && change_marker)
                row.cells[i] = string_sprintf("%.5g %s", sqlite3_column_double(query, (int)i), change_marker);
            else if (type == SQLITE_FLOAT)
                row.cells[i] = string_sprintf(format == TEST_DATABASE_STATS_TSV ? "%.6g" : "%.5g",
                                              sqlite3_column_double(query, (int)i));
            else if (type == SQLITE_NULL) row.cells[i] = string_new_with("-");
            else row.cells[i] = safe_text((const char *)sqlite3_column_text(query, (int)i));
            if (row.cells[i] && i == OUTCOME && format != TEST_DATABASE_STATS_TSV) {
                if (!strcmp(string_c_str(row.cells[i]), "skip")) {
                    string_free(row.cells[i]);
                    row.cells[i] = string_new_with("SKIPPED");
                } else string_to_upper(row.cells[i]);
            }
            row.widths[i] = row.cells[i] ? columns(row.cells[i]) : SIZE_MAX;
            ok = row.widths[i] != SIZE_MAX;
        }
        if (ok) ok = array_add(rows, &row);
        if (!ok) row_free(&row);
    }
    sqlite3_finalize(query);
    ok = ok && status == SQLITE_DONE;
    if (ok && format == TEST_DATABASE_STATS_TSV) {
        for (size_t i = 0; i < FIELD_COUNT; ++i) printf("%s%s", i ? "\t" : "", fields[i].key);
        putchar('\n');
        for (size_t j = 0; j < array_size(rows); ++j) {
            const stats_row_t *row = array_get(rows, j);
            for (size_t i = 0; i < FIELD_COUNT; ++i) printf("%s%s", i ? "\t" : "", string_c_str(row->cells[i]));
            putchar('\n');
        }
    } else if (ok) {
        puts("\nTest statistics — durations in milliseconds; dates in UTC.");
        puts("Mean/median and samples exclude memory runs and timing outliers; fastest/slowest include ordinary outliers.");
        puts("Change % compares the last two ordinary runs. The latest run may be a memory run. '-' means unavailable.");
        if (!details) puts("Use --details for timestamps, memory-test dates, comparisons and outlier records.");
        if (!array_size(rows)) puts("No recorded tests match this source prefix.");
        else {
            bool markdown = format == TEST_DATABASE_STATS_MARKDOWN;
            bool emphasis = !markdown && terminal_emphasis();
            ok = details ? print_details(rows, markdown, terminal_columns(), emphasis) :
                           print_summary(rows, markdown, terminal_columns(), emphasis);
        }
    }
    array_destroy(rows);
    uselocale(previous); freelocale(utf8);
    return (ok && !ferror(stdout)) || test_database_fail(database, "Cannot read or print test statistics");
}
