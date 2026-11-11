/**
 * @file benchmarks_stats.c
 * @brief Read-only, paged-terminal-friendly benchmark statistics and portable exports.
 *
 * An indexed query snapshots one saved run before any potentially blocking terminal output. Backend and precision
 * groups use double table borders, colour-coded outcomes and compatible-run comparisons. UTF-8 cells wrap at whole
 * grapheme boundaries without truncating their contents. Unsupported cases are hidden unless explicitly requested.
 * Markdown and TSV exports contain no terminal escapes.
 */
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>

#include "array-t.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

enum { NAME, ARGUMENTS, RESULT, MEDIAN, MEAN, FASTEST, SLOWEST, CHANGE, SAMPLES, FIELD_COUNT };

static const char *const headings[FIELD_COUNT] = {
    "Operation", "Arguments", "Result", "Median µs", "Mean µs", "Fastest µs", "Slowest µs", "Change %", "Batches"
};

typedef struct {
    long run;
    const char *backend, *operation, *format;
    long bits;
    bool bits_selected, details, include_skipped;
} benchmark_stats_options_t;

typedef struct {
    string_t *cells[FIELD_COUNT];
    string_t *backend, *detail;
    int bits;
    bool outlier;
    const char *result_style, *change_style;
} benchmark_stats_row_t;

static void benchmark_stats_row_free(void *value)
{
    benchmark_stats_row_t *row = value;
    for (unsigned i = 0; i < FIELD_COUNT; ++i) string_free(row->cells[i]);
    string_free(row->backend);
    string_free(row->detail);
}

static bool benchmark_stats_options(int argc, char **argv, benchmark_stats_options_t *options)
{
    *options = (benchmark_stats_options_t){.backend = "", .operation = "", .format = "grid"};
    for (int i = 0; i < argc; ++i) {
        const char *key = argv[i];
        if (!strcmp(key, "--details")) options->details = true;
        else if (!strcmp(key, "--summary")) options->details = false;
        else if (!strcmp(key, "--include-skipped")) options->include_skipped = true;
        else if (!strncmp(key, "--format=", 9)) {
            options->format = key + 9;
            if (strcmp(options->format, "grid") && strcmp(options->format, "markdown") &&
                strcmp(options->format, "tsv")) return false;
        } else {
            if (++i == argc) return false;
            if (!strcmp(key, "--backend")) options->backend = argv[i];
            else if (!strcmp(key, "--operation")) options->operation = argv[i];
            else {
                char *end = NULL;
                errno = 0;
                long number = strtol(argv[i], &end, 10);
                if (errno || end == argv[i] || *end || number < 0) return false;
                if (!strcmp(key, "--run") && number > 0) options->run = number;
                else if (!strcmp(key, "--bits") && number <= 16384) {
                    options->bits = number;
                    options->bits_selected = true;
                } else return false;
            }
        }
    }
    return true;
}

static string_t *benchmark_stats_text(const char *text)
{
    string_t *source = string_new_with(text ? text : "-"), *out = string_new();
    string_cursor_t *cursor = source ? string_cursor_new(source) : NULL;
    bool ok = cursor && out;
    while (ok && !string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        string_pos_t start = string_cursor_position(cursor);
        string_cursor_next(cursor);
        if (rune_is_control(rune)) ok = string_append_format(out, "\\x%02x", rune_value(rune)) >= 0;
        else ok = !string_cursor_append_slice_between(out, start, string_cursor_position(cursor), cursor);
    }
    string_cursor_free(cursor);
    string_free(source);
    if (!ok) { string_free(out); return NULL; }
    return out;
}

static size_t benchmark_stats_rune_width(rune_t rune)
{
    uint32_t value = rune_value(rune);
    if (value >= 0x1f1e6 && value <= 0x1f1ff) return 2;
    int width = wcwidth((wchar_t)value);
    return width < 0 ? 1 : (size_t)width;
}

static size_t benchmark_stats_width(const string_t *text)
{
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor) return SIZE_MAX;
    size_t width = 0;
    while (!string_cursor_done(cursor)) {
        width += benchmark_stats_rune_width(string_cursor_peek(cursor));
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    return width;
}

static size_t benchmark_stats_terminal_width(void)
{
    struct winsize window = {0};
    if (!ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) && window.ws_col) return window.ws_col < 60 ? 60 : window.ws_col;
    const char *configured = getenv("COLUMNS");
    char *end = NULL;
    errno = 0;
    long width = configured ? strtol(configured, &end, 10) : 0;
    if (!errno && configured && end != configured && !*end && width >= 60 && width <= 1000) return (size_t)width;
    if (!ioctl(STDERR_FILENO, TIOCGWINSZ, &window) && window.ws_col) return window.ws_col < 60 ? 60 : window.ws_col;
    return 132;
}

static bool benchmark_stats_colour(void)
{
    const char *disabled = getenv("NO_COLOR"), *term = getenv("TERM"), *force = getenv("CLICOLOR_FORCE");
    return (!disabled || !*disabled) && (!term || strcmp(term, "dumb")) &&
        (isatty(STDOUT_FILENO) || (force && *force && strcmp(force, "0")) || (term && *term));
}

static string_t *benchmark_stats_metadata(sqlite_t *db, long *run)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(db,
        "SELECT id, started_utc, status, cpu, clock\n"
        "FROM benchmark_run\n"
        "WHERE id = CASE WHEN ?1 = 0 THEN (SELECT max(id) FROM benchmark_run) ELSE ?1 END");
    bool ok = query && sqlite_stmt_bind_int64(query, 1, *run) && sqlite_stmt_step(query) == SQLITE_STEP_ROW;
    string_t *header = NULL;
    if (ok) {
        *run = (long)sqlite_stmt_column_int64(query, 0);
        string_t *raw = string_sprintf("Benchmark run %ld — %s — %s (UTC); %s. Timing: %s.", *run,
            sqlite_stmt_column_text(query, 2), sqlite_stmt_column_text(query, 1), sqlite_stmt_column_text(query, 3),
            sqlite_stmt_column_text(query, 4));
        if (raw) header = benchmark_stats_text(string_c_str(raw));
        string_free(raw);
    }
    sqlite_stmt_finalize(query);
    return header;
}

static string_t *benchmark_stats_microseconds(sqlite_stmt_t *query, int column, bool measured)
{
    if (!measured || sqlite_stmt_column_is_null(query, column)) return string_new_with("-");
    return string_sprintf("%.5g", sqlite_stmt_column_double(query, column) / 1000);
}

static bool benchmark_stats_load(sqlite_t *db, const benchmark_stats_options_t *options, array_t *rows,
                                 size_t *widths)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(db,
        "SELECT backend, bits, operation, outcome, median_ns, mean_ns, minimum_ns, maximum_ns,\n"
        "       CASE WHEN baseline_median_ns > 0 AND outcome = 'pass'\n"
        "            THEN 100.0 * (median_ns / baseline_median_ns - 1) END,\n"
        "       sample_count, timing_outlier, arguments, finished_utc, iterations, resolution_ns, mad_ns, detail,\n"
        "       complex_mask\n"
        "FROM benchmark_result\n"
        "WHERE run_id = ?1 AND (?2 = '' OR backend = ?2)\n"
        "  AND (?3 = 0 OR bits = ?4) AND (?5 = '' OR operation = ?5)\n"
        "  AND (?6 OR outcome <> 'unsupported')\n"
        "ORDER BY CASE backend WHEN 'number-double' THEN 0 WHEN 'qfloat' THEN 1 WHEN 'qcomplex' THEN 2\n"
        "                      WHEN 'mpfr' THEN 3 WHEN 'mpc' THEN 4 ELSE 5 END, backend, bits, operation");
    bool ok = query && sqlite_stmt_bind_int64(query, 1, options->run) &&
        sqlite_stmt_bind_text(query, 2, options->backend) && sqlite_stmt_bind_int(query, 3, options->bits_selected) &&
        sqlite_stmt_bind_int64(query, 4, options->bits) && sqlite_stmt_bind_text(query, 5, options->operation) &&
        sqlite_stmt_bind_int(query, 6, options->include_skipped);
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        benchmark_stats_row_t row = {.bits = sqlite_stmt_column_int(query, 1)};
        const char *outcome = sqlite_stmt_column_text(query, 3);
        bool measured = !strcmp(outcome, "pass"), skipped = !strcmp(outcome, "unsupported");
        row.backend = benchmark_stats_text(sqlite_stmt_column_text(query, 0));
        row.cells[NAME] = benchmark_stats_text(sqlite_stmt_column_text(query, 2));
        row.cells[RESULT] = string_new_with(skipped ? "SKIPPED" : outcome);
        if (row.cells[RESULT]) string_to_upper(row.cells[RESULT]);
        row.result_style = measured ? "\033[1;92m" : skipped || !strcmp(outcome, "pending") ?
                           "\033[1;93m" : "\033[1;91m";
        for (unsigned i = MEDIAN; i <= SLOWEST; ++i)
            row.cells[i] = benchmark_stats_microseconds(query, 4 + (int)(i - MEDIAN), measured);
        row.cells[CHANGE] = string_new_with("-");
        if (!sqlite_stmt_column_is_null(query, 8)) {
            double change = sqlite_stmt_column_double(query, 8);
            unsigned trend = change < -5 ? 0 : change > 5 ? 2 : 1;
            static const char *const styles[] = {"\033[1;92m", "\033[1;38;2;255;140;0m", "\033[1;91m"};
            static const char *const markers[] = {"▲", "▬", "▼"};
            row.change_style = styles[trend];
            string_free(row.cells[CHANGE]);
            row.cells[CHANGE] = !strcmp(options->format, "grid") ?
                string_sprintf("%.4g %s%s", change, markers[trend], sqlite_stmt_column_int(query, 10) ? " †" : "") :
                string_sprintf("%.5g", change);
        }
        row.outlier = sqlite_stmt_column_int(query, 10) != 0;
        row.cells[SAMPLES] = measured ? string_sprintf("%d", sqlite_stmt_column_int(query, 9)) : string_new_with("-");
        string_t *inputs = benchmark_arguments_text(sqlite_stmt_column_text(query, 11),
            sqlite_stmt_column_text(query, 0), (unsigned)sqlite_stmt_column_int(query, 17));
        row.cells[ARGUMENTS] = inputs ? benchmark_stats_text(string_c_str(inputs)) : NULL;
        string_t *detail = string_sprintf("finished: %s; ",
            sqlite_stmt_column_is_null(query, 12) ? "pending" : sqlite_stmt_column_text(query, 12));
        string_free(inputs);
        if (detail && measured) string_append_format(detail,
            "calls/batch: %d; clock resolution: %d ns; MAD: %.5g µs; ", sqlite_stmt_column_int(query, 13),
            sqlite_stmt_column_int(query, 14), sqlite_stmt_column_double(query, 15) / 1000);
        if (detail) string_append_cstr(detail, sqlite_stmt_column_text(query, 16));
        row.detail = detail ? benchmark_stats_text(string_c_str(detail)) : NULL;
        string_free(detail);
        ok = row.backend && row.detail;
        for (unsigned i = 0; ok && i < FIELD_COUNT; ++i) {
            size_t width = row.cells[i] ? benchmark_stats_width(row.cells[i]) : SIZE_MAX;
            ok = width != SIZE_MAX;
            if (ok && width > widths[i]) widths[i] = width;
        }
        if (ok) ok = array_add(rows, &row);
        if (!ok) benchmark_stats_row_free(&row);
    }
    sqlite_stmt_finalize(query);
    return ok && step == SQLITE_STEP_DONE;
}

static void benchmark_stats_rule(const size_t *widths, unsigned count, const char *left, const char *line,
                                 const char *middle, const char *right)
{
    fputs(left, stdout);
    for (unsigned i = 0; i < count; ++i) {
        for (size_t j = 0; j < widths[i] + 2; ++j) fputs(line, stdout);
        fputs(i + 1 == count ? right : middle, stdout);
    }
    putchar('\n');
}

static bool benchmark_stats_grid_row(string_t *const *cells, const size_t *widths, const char *const *styles,
                                     unsigned count, bool numeric)
{
    string_cursor_t *cursors[FIELD_COUNT] = {0};
    bool ok = count <= FIELD_COUNT, more = true;
    for (unsigned i = 0; ok && i < count; ++i) ok = (cursors[i] = string_cursor_new(cells[i])) != NULL;
    while (ok && more) {
        more = false;
        fputs("║", stdout);
        for (unsigned i = 0; i < count; ++i) {
            size_t used = 0, start = string_cursor_position(cursors[i]);
            while (!string_cursor_done(cursors[i])) {
                size_t width = benchmark_stats_rune_width(string_cursor_peek(cursors[i]));
                if (used + width > widths[i]) break;
                used += width;
                string_cursor_next(cursors[i]);
            }
            size_t end = string_cursor_position(cursors[i]), padding = widths[i] - used;
            bool right = numeric && i >= MEDIAN;
            printf(" %*s", right ? (int)padding : 0, "");
            if (styles && styles[i]) fputs(styles[i], stdout);
            printf("%.*s", (int)(end - start), string_c_str(cells[i]) + start);
            if (styles && styles[i]) fputs("\033[0m", stdout);
            printf("%*s%s", right ? 0 : (int)padding, "", i + 1 == count ? " ║" : " │");
            more = more || !string_cursor_done(cursors[i]);
        }
        putchar('\n');
    }
    for (unsigned i = 0; i < count && i < FIELD_COUNT; ++i) string_cursor_free(cursors[i]);
    return ok;
}

static const char *benchmark_stats_style(const benchmark_stats_row_t *row, unsigned field)
{
    static const char *const styles[FIELD_COUNT] = {
        [NAME]   = "\033[96m",
        [MEDIAN] = "\033[1;93m",
        [MEAN]   = "\033[1;96m",
    };
    if (field == RESULT) return row->result_style;
    if (field == CHANGE) return row->change_style;
    return styles[field];
}

static bool benchmark_stats_panel(const benchmark_stats_row_t *row, size_t width, bool colour, bool details)
{
    size_t sizes[] = {13, width - 20};
    benchmark_stats_rule(sizes, 2, "╔", "═", "╤", "╗");
    bool ok = true;
    unsigned count = FIELD_COUNT + (details ? 1 : 0);
    for (unsigned i = 0; ok && i < count; ++i) {
        string_t *label = string_new_with(i == FIELD_COUNT ? "Details" : headings[i]);
        string_t *cells[] = {label, i == FIELD_COUNT ? row->detail : row->cells[i]};
        const char *styles[] = {colour ? "\033[1m" : NULL,
                                colour && i < FIELD_COUNT ? benchmark_stats_style(row, i) : NULL};
        ok = label && benchmark_stats_grid_row(cells, sizes, styles, 2, false);
        string_free(label);
        bool last = i + 1 == count;
        benchmark_stats_rule(sizes, 2, last ? "╚" : "╟", last ? "═" : "─", last ? "╧" : "┼", last ? "╝" : "╢");
    }
    return ok;
}

static void benchmark_stats_markdown(const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '|' || *p == '\\' || *p == '*' || *p == '_' || *p == '`' || *p == '[' || *p == ']') putchar('\\');
        if (*p == '<') fputs("&lt;", stdout);
        else if (*p == '>') fputs("&gt;", stdout);
        else if (*p == '&') fputs("&amp;", stdout);
        else putchar(*p);
    }
}

static bool benchmark_stats_print(const array_t *rows, const benchmark_stats_options_t *options, size_t *widths)
{
    bool tsv = !strcmp(options->format, "tsv"), markdown = !strcmp(options->format, "markdown");
    bool colour = !tsv && !markdown && benchmark_stats_colour(), ok = true;
    size_t available = benchmark_stats_terminal_width(), fixed = 1 + 3 * FIELD_COUNT;
    for (unsigned i = RESULT; i < FIELD_COUNT; ++i) fixed += widths[i];
    bool panels = !tsv && !markdown && (options->details || available < fixed + 22);
    if (!panels && !tsv && !markdown && widths[NAME] + widths[ARGUMENTS] + fixed > available) {
        size_t space = available - fixed, arguments_limit = space / 2;
        if (widths[NAME] < space - arguments_limit) arguments_limit = space - widths[NAME];
        if (widths[ARGUMENTS] > arguments_limit) widths[ARGUMENTS] = arguments_limit;
        if (widths[NAME] > space - widths[ARGUMENTS]) widths[NAME] = space - widths[ARGUMENTS];
    }
    string_t *headers[FIELD_COUNT] = {0};
    const char *header_styles[FIELD_COUNT];
    for (unsigned i = 0; i < FIELD_COUNT; ++i) {
        headers[i] = string_new_with(headings[i]);
        header_styles[i] = colour ? "\033[1m" : NULL;
        ok = ok && headers[i];
    }
    if (tsv) printf("backend\tbits\toperation\targuments\tresult\tmedian_us\tmean_us\tfastest_us\tslowest_us\tchange_percent"
                    "\tbatches\ttiming_outlier%s\n", options->details ? "\tdetails" : "");
    const char *backend = NULL;
    int bits = -1;
    for (size_t i = 0; ok && i < array_size(rows); ++i) {
        const benchmark_stats_row_t *row = array_get(rows, i);
        const char *next = string_c_str(row->backend);
        if (!tsv && (!backend || strcmp(backend, next) || bits != row->bits)) {
            if (backend && !markdown && !panels) benchmark_stats_rule(widths, FIELD_COUNT, "╚", "═", "╧", "╝");
            if (colour) fputs("\033[1;93m", stdout);
            printf("\n%sBackend: %s — ", markdown ? "## " : "", benchmark_backend_title(next));
            if (row->bits) printf("%d bits", row->bits);
            else fputs(!strcmp(next, "exact-fraction") ? "exact fractions" : "exact integers", stdout);
            if (colour) fputs("\033[0m", stdout);
            puts("\n");
            backend = next;
            bits = row->bits;
            if (markdown) {
                for (unsigned j = 0; j < FIELD_COUNT; ++j) printf("| **%s** ", headings[j]);
                fputs("| **Outlier** ", stdout);
                if (options->details) fputs("| **Details** ", stdout);
                puts("|");
                for (unsigned j = 0; j < FIELD_COUNT; ++j) fputs(j < MEDIAN ? "| :--- " : "| ---: ", stdout);
                fputs("| :--- ", stdout);
                if (options->details) fputs("| :--- ", stdout);
                puts("|");
            } else if (!panels) {
                benchmark_stats_rule(widths, FIELD_COUNT, "╔", "═", "╤", "╗");
                ok = benchmark_stats_grid_row(headers, widths, header_styles, FIELD_COUNT, false);
                benchmark_stats_rule(widths, FIELD_COUNT, "╠", "═", "╪", "╣");
            }
        } else if (!tsv && !markdown && !panels) benchmark_stats_rule(widths, FIELD_COUNT, "╟", "─", "┼", "╢");
        if (tsv) printf("%s\t%d", next, row->bits);
        if (tsv || markdown) {
            for (unsigned j = 0; j < FIELD_COUNT; ++j) {
                fputs(tsv ? "\t" : "| ", stdout);
                if (markdown && j == RESULT) fputs("**", stdout);
                if (tsv) fputs(string_c_str(row->cells[j]), stdout);
                else if (j == ARGUMENTS) {
                    string_t *inputs = benchmark_arguments_markdown(row->cells[j], benchmark_backend_exact(next));
                    ok = inputs && fputs(string_c_str(inputs), stdout) != EOF && ok;
                    string_free(inputs);
                } else benchmark_stats_markdown(string_c_str(row->cells[j]));
                if (markdown && j == RESULT) fputs("**", stdout);
                if (markdown) putchar(' ');
            }
            if (tsv) printf("\t%d", row->outlier);
            else printf("| %s ", row->outlier ? "yes" : "no");
            if (options->details) {
                fputs(tsv ? "\t" : "| ", stdout);
                if (tsv) fputs(string_c_str(row->detail), stdout);
                else benchmark_stats_markdown(string_c_str(row->detail));
                if (markdown) putchar(' ');
            }
            puts(markdown ? "|" : "");
        } else if (panels) ok = benchmark_stats_panel(row, available, colour, options->details);
        else {
            const char *styles[FIELD_COUNT] = {0};
            for (unsigned j = 0; colour && j < FIELD_COUNT; ++j) styles[j] = benchmark_stats_style(row, j);
            ok = benchmark_stats_grid_row(row->cells, widths, styles, FIELD_COUNT, true);
        }
    }
    if (backend && !markdown && !panels) benchmark_stats_rule(widths, FIELD_COUNT, "╚", "═", "╧", "╝");
    for (unsigned i = 0; i < FIELD_COUNT; ++i) string_free(headers[i]);
    return ok && !ferror(stdout);
}

/* Snapshot saved observations before printing, so slow pagers do not retain a database read transaction. */
int benchmark_stats(sqlite_t *db, int argc, char **argv)
{
    benchmark_stats_options_t options;
    if (!benchmark_stats_options(argc, argv, &options)) return 2;
    locale_t utf8 = newlocale(LC_CTYPE_MASK, "C.UTF-8", (locale_t)0);
    locale_t previous = utf8 ? uselocale(utf8) : (locale_t)0;
    if (!utf8 || !previous) { if (utf8) freelocale(utf8); return 1; }
    array_t *rows = array_create(sizeof(benchmark_stats_row_t), NULL, benchmark_stats_row_free);
    size_t widths[FIELD_COUNT];
    bool widths_ok = true;
    for (unsigned i = 0; i < FIELD_COUNT; ++i) {
        string_t *heading = string_new_with(headings[i]);
        widths[i] = heading ? benchmark_stats_width(heading) : SIZE_MAX;
        widths_ok = widths[i] != SIZE_MAX && widths_ok;
        string_free(heading);
    }
    bool ok = rows && widths_ok && sqlite_exec_cstr(db, "BEGIN");
    string_t *header = ok ? benchmark_stats_metadata(db, &options.run) : NULL;
    ok = header && benchmark_stats_load(db, &options, rows, widths) && ok;
    if (!sqlite_exec_cstr(db, ok ? "COMMIT" : "ROLLBACK")) ok = false;
    if (ok) {
        if (strcmp(options.format, "tsv")) {
            puts(string_c_str(header));
            puts("Times are microseconds per call. Batches are timed samples, not independent runs.");
            puts("Change compares the previous compatible, non-outlier run: ▲ faster; ▼ slower; ▬ within 5%. "
                 "† marks a retained slowdown outlier.");
            puts(options.include_skipped
                ? "SKIPPED means the backend does not support this case. '-' means no measured value."
                : "Unsupported cases are hidden; use --include-skipped to show them. '-' means no measured value.");
        }
        if (!array_size(rows) && strcmp(options.format, "tsv")) puts("No recorded benchmark cases match these filters.");
        else ok = benchmark_stats_print(rows, &options, widths);
    }
    string_free(header);
    array_destroy(rows);
    uselocale(previous);
    freelocale(utf8);
    if (!ok) fputs("Cannot read benchmark statistics: check the run identifier, storage and output.\n", stderr);
    return ok && !ferror(stdout) ? 0 : 1;
}
