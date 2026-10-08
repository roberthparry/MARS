/**
 * @file cfg_database_sql.c
 * @brief Bounded recursive import of packaged SQL through the native SQLCipher API.
 *
 * Reads fixed-size chunks using file.h, preserving exact UTF-8 SQL in string_t.
 * SQLCipher's public completeness lexer handles quoted delimiters, comments and
 * trigger bodies. Large literal VALUES inserts use savepoint-protected batches.
 * A line is limited to 1 MiB, other pending SQL to 16 MiB and .read
 * nesting to 32 files. These bounds do not limit the total source-file size.
 * Relative .read arguments resolve against the repository, matching the scripts.
 * MARS_CONFIG_IMPORT_TRACE=1 enables per-source wall/CPU timings. Diagnostic names
 * come only from a fixed packaged-source list; arbitrary paths and SQL are omitted.
 */
#define _POSIX_C_SOURCE 200809L
#include <sqlcipher/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cfg_database_internal.h"

static const size_t cfg_database_line_limit = 1024 * 1024;
static const size_t cfg_database_statement_limit = 16 * 1024 * 1024;

typedef struct cfg_database_lexer {
    unsigned char quote;
    bool block_comment;
    bool line_comment;
    bool has_sql;
    bool terminator;
} cfg_database_lexer;

static bool cfg_database_sql_file(sqlite_t *db, const string_t *root, const string_t *path, unsigned depth,
                                  cfg_database_values *parent);

/* Select one candidate through a perfect hash, then verify exact allowlist membership. */
const char *cfg_database_trace_label(const string_t *path)
{
    static const char *const sources[28] = {
        [ 0] = "packaging/jurisdiction-db/mars_timezone_rules.sql",
        [ 1] = "packaging/jurisdiction-db/mars_generated_first_class_rules.sql",
        [ 4] = "packaging/jurisdiction-db/mars_calendar_local.sql",
        [ 8] = "packaging/almanac-db/mars_almanac_chebyshev.sql",
        [ 9] = "packaging/jurisdiction-db/mars_target_subdivisions.sql",
        [11] = "packaging/jurisdiction-db/mars_calendar_locale_names.sql",
        [12] = "packaging/jurisdiction-db/mars_jurisdiction_towns.sql",
        [13] = "packaging/almanac-db/mars_almanac_frame_rotation.sql",
        [14] = "packaging/jurisdiction-db/mars_manual_first_class_rules.sql",
        [17] = "packaging/jurisdiction-db/mars_holiday_rules.sql",
        [24] = "packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql",
        [25] = "packaging/jurisdiction-db/mars_country_jurisdictions.sql",
        [26] = "packaging/almanac-db/mars_almanac.sql",
        [27] = "packaging/jurisdiction-db/mars_holiday_localized_names.sql"
    };
    if (!path)
        return "unlisted SQL source";
    string_view_t view = string_view_all(path);
    size_t length = string_view_length(view);
    unsigned char discriminator = 0;
    if (length < 37 || length > 65 || !string_view_peek_ascii(view, 32, &discriminator))
        return "unlisted SQL source";

    /* Collision-free for the fixed catalogue above; regression tests cover every entry.
     * Unknown paths can collide, so the final exact comparison must not be omitted. */
    size_t slot = (length + 3 * discriminator) % 28;
    const char *label = sources[slot];
    return label && string_view_equals_literal(view, label) ? label : "unlisted SQL source";
}

static double cfg_database_trace_seconds(clockid_t clock)
{
    struct timespec time;
    return clock_gettime(clock, &time) == 0 ? (double)time.tv_sec + (double)time.tv_nsec / 1e9 : -1;
}

static bool cfg_database_sql_space(unsigned char ch)
{
    return ch == ' ' || (ch >= '\t' && ch <= '\r');
}

static void cfg_database_lex_line(cfg_database_lexer *lexer, const string_t *line)
{
    string_view_t view = string_view_all(line);
    for (size_t i = 0; i < string_view_length(view); ++i) {
        unsigned char ch = 0, next = 0;
        bool ascii = string_view_peek_ascii(view, i, &ch);
        string_view_peek_ascii(view, i + 1, &next);
        if (lexer->line_comment) {
            if (ch == '\n')
                lexer->line_comment = false;
        } else if (lexer->block_comment) {
            if (ch == '*' && next == '/') {
                lexer->block_comment = false;
                ++i;
            }
        } else if (lexer->quote) {
            if (ch == lexer->quote) {
                if (lexer->quote != ']' && next == lexer->quote)
                    ++i;
                else
                    lexer->quote = 0;
            }
        } else if (ch == '-' && next == '-') {
            lexer->line_comment = true;
            ++i;
        } else if (ch == '/' && next == '*') {
            lexer->block_comment = true;
            ++i;
        } else if (!ascii || !cfg_database_sql_space(ch)) {
            lexer->has_sql = true;
            lexer->terminator = ch == ';';
            if (ch == '\'' || ch == '"' || ch == '`' || ch == '[')
                lexer->quote = ch == '[' ? ']' : ch;
        }
    }
}

static string_t *cfg_database_read_argument(const string_t *line)
{
    string_cursor_t *cursor = string_cursor_new(line);
    if (!cursor)
        return NULL;
    string_cursor_skip_spaces(cursor);
    bool ok = string_cursor_consume(cursor, ".read") &&
              (string_cursor_match(cursor, " ") || string_cursor_match(cursor, "\t"));
    string_cursor_skip_spaces(cursor);
    const char *quote = string_cursor_consume(cursor, "\"") ? "\"" : string_cursor_consume(cursor, "'") ? "'" : NULL;
    string_pos_t start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor) &&
           !(quote ? string_cursor_match(cursor, quote)
                   : (string_cursor_match(cursor, " ") || string_cursor_match(cursor, "\t") ||
                      string_cursor_match(cursor, "\r") || string_cursor_match(cursor, "\n"))))
        string_cursor_next(cursor);
    string_t *argument = string_cursor_slice_between(start, string_cursor_position(cursor), cursor);
    if (quote)
        ok = string_cursor_consume(cursor, quote) && ok;
    string_cursor_skip_spaces(cursor);
    ok = ok && argument && string_byte_length(argument) && string_cursor_done(cursor);
    string_cursor_free(cursor);
    if (!ok) {
        string_free(argument);
        return NULL;
    }
    return argument;
}

static bool cfg_database_sql_line(sqlite_t *db, const string_t *root, string_t *pending, cfg_database_values *values,
                                  cfg_database_lexer *lexer, const char *bytes, size_t length, unsigned depth)
{
    string_t *line = string_new();
    bool ok = line && !string_append_utf8_exact(line, bytes, length);
    string_view_t view = string_view_all(line);
    unsigned char first = 0;
    size_t start = 0;
    while (string_view_peek_ascii(view, start, &first) && cfg_database_sql_space(first))
        ++start;
    bool directive = string_view_peek_ascii(view, start, &first) && first == '.';
    if (ok && directive && !values->prefix && !lexer->has_sql && !lexer->block_comment && !lexer->line_comment) {
        string_t *argument = cfg_database_read_argument(line);
        ok = argument && cfg_database_sql_file(db, root, argument, depth + 1, values);
        string_free(argument);
        string_clear(pending);
    } else if (ok) {
        ok = length <= cfg_database_statement_limit - string_byte_length(pending) &&
             !string_append_utf8_exact(pending, bytes, length);
        bool handled = false;
        if (ok)
            ok = cfg_database_values_line(db, values, pending, line, &handled);
        if (ok && handled && !values->prefix)
            *lexer = (cfg_database_lexer){0};
        if (ok && !handled)
            cfg_database_lex_line(lexer, line);
        if (ok && !handled && lexer->terminator && !lexer->quote && !lexer->block_comment &&
            sqlite3_complete(string_c_str(pending))) {
            ok = cfg_database_execute(db, values, string_c_str(pending));
            string_clear(pending);
            *lexer = (cfg_database_lexer){0};
        } else if (ok && !handled && !lexer->quote && !lexer->block_comment && cfg_database_values_header(pending)) {
            values->prefix = string_clone(pending);
            ok = values->prefix != NULL;
        } else if (ok && !handled && !lexer->has_sql && !lexer->block_comment) {
            /* Discard complete comments between statements, never comments inside SQL. */
            string_clear(pending);
        }
    }
    string_free(line);
    return ok;
}

static bool cfg_database_sql_file(sqlite_t *db, const string_t *root, const string_t *path, unsigned depth,
                                  cfg_database_values *parent)
{
    if (depth >= 32)
        return false;
    const char *trace_option = getenv("MARS_CONFIG_IMPORT_TRACE");
    bool trace = trace_option && trace_option[0] == '1' && trace_option[1] == '\0';
    const char *label = trace ? cfg_database_trace_label(path) : NULL;
    double wall_start = trace ? cfg_database_trace_seconds(CLOCK_MONOTONIC) : 0;
    double cpu_start = trace ? cfg_database_trace_seconds(CLOCK_PROCESS_CPUTIME_ID) : 0;
    if (trace) {
        fprintf(stderr, "Import begin depth=%u source=%s\n", depth, label);
        fflush(stderr);
    }
    string_t *resolved = string_starts_with(path, "/")
                             ? string_clone(path)
                             : string_sprintf("%s/%s", string_c_str(root), string_c_str(path));
    file_t *file = resolved ? file_new(resolved) : NULL;
    string_t *pending = string_new();
    cfg_database_values values = {.trace = trace};
    cfg_database_lexer lexer = {0};
    char *line = malloc(cfg_database_line_limit);
    bool ok = file && pending && line && file_open_read(file);
    char chunk[16384];
    size_t used = 0, count = 0, bytes_read = 0;
    while (ok) {
        ok = file_read(file, chunk, sizeof(chunk), &count);
        if (!ok || !count)
            break;
        bytes_read += count;
        /* One bounded pass over stream bytes locates line endings without decoding SQL. */
        for (size_t i = 0; i < count && ok; ++i) {
            if (!chunk[i] || used == cfg_database_line_limit) {
                ok = false;
                break;
            }
            line[used++] = chunk[i];
            if (chunk[i] == '\n') {
                ok = cfg_database_sql_line(db, root, pending, &values, &lexer, line, used, depth);
                used = 0;
            }
        }
    }
    if (ok && used)
        ok = cfg_database_sql_line(db, root, pending, &values, &lexer, line, used, depth);
    if (values.prefix)
        ok = false;
    /* SQLite accepts a final statement without a terminating semicolon. */
    if (ok && string_byte_length(pending))
        ok = cfg_database_execute(db, &values, string_c_str(pending));
    if (file && file_is_open(file))
        ok = file_close(file) && ok;
    free(line);
    cfg_database_values_free(db, &values);
    string_free(pending);
    file_free(file);
    string_free(resolved);
    if (trace) {
        double wall_end = cfg_database_trace_seconds(CLOCK_MONOTONIC);
        double cpu_end = cfg_database_trace_seconds(CLOCK_PROCESS_CPUTIME_ID);
        double wall_elapsed = wall_start >= 0 && wall_end >= 0 ? wall_end - wall_start : -1;
        double cpu_elapsed = cpu_start >= 0 && cpu_end >= 0 ? cpu_end - cpu_start : -1;
        double parser_cpu = cpu_elapsed >= values.execution_cpu ? cpu_elapsed - values.execution_cpu : -1;
        fprintf(stderr,
                "Import %s depth=%u source=%s bytes=%zu wall=%.3fs cpu=%.3fs "
                "sql_cpu=%.3fs parser_io_cpu=%.3fs sql_calls=%zu (includes nested reads)\n",
                ok ? "done" : "failed", depth, label, bytes_read, wall_elapsed, cpu_elapsed, values.execution_cpu,
                parser_cpu, values.execution_calls);
        fflush(stderr);
    }
    if (parent) {
        parent->execution_cpu += values.execution_cpu;
        parent->execution_calls += values.execution_calls;
    }
    return ok;
}

/* Import without adding, splitting or committing the source's transactions. */
bool cfg_database_import(sqlite_t *db, const string_t *root, const string_t *path)
{
    return db && root && path && cfg_database_sql_file(db, root, path, 0, NULL);
}
