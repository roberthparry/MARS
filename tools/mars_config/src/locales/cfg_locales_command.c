/**
 * @file cfg_locales_command.c
 * @brief Command-line validation and safe publication of generated locale SQL.
 *
 * Dispatches checking, standard-output generation and staged file replacement.
 * The locale engine produces the complete SQL before this unit touches an output
 * destination. Paths and options use string_t; filesystem access uses file_t.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cfg_locales.h"
#include "cfg_locales_internal.h"

static string_t *cfg_locales_argument(const char *value)
{
    string_t *text = string_new();
    if (!text || !value || string_append_utf8_exact(text, value, strlen(value)) != 0) {
        string_free(text);
        return NULL;
    }
    return text;
}

static bool cfg_locales_option(const string_t *value, const char *name)
{
    return value && string_view_equals_literal(string_view_all(value), name);
}

static bool cfg_locales_publish(const string_t *path, const string_t *sql)
{
    file_t *destination = file_new(path);
    file_t *temporary = NULL;
    bool created = false;
    /* Exclusive creation bounds collisions without ever truncating another file. */
    for (unsigned attempt = 0; destination && !created && attempt < 32; ++attempt) {
        string_t *suffix = string_sprintf(".mars-locales-%ld-%u.tmp", (long)getpid(), attempt);
        string_t *staging = string_clone(path);
        bool ready = suffix && staging &&
                     string_append_utf8_exact(staging, string_c_str(suffix), string_byte_length(suffix)) == 0;
        file_free(temporary);
        temporary = ready ? file_new(staging) : NULL;
        string_free(staging);
        string_free(suffix);
        created = temporary && file_open(temporary, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
        if (!created && (!temporary || file_last_error(temporary) != EEXIST))
            break;
    }
    bool ok = created && file_write_text(temporary, sql) && file_sync(temporary, false);
    if (created && !file_close(temporary))
        ok = false;
    if (ok)
        ok = file_move(temporary, destination, true);
    if (created && !ok)
        (void)file_delete(temporary);
    file_free(temporary);
    file_free(destination);
    return ok;
}

static bool cfg_locales_check(const string_t *root, const string_t *sql)
{
    string_t *path = string_clone(root);
    const char *suffix = "/packaging/jurisdiction-db/mars_calendar_locale_names.sql";
    bool ready = path && string_append_utf8_exact(path, suffix, strlen(suffix)) == 0;
    string_t *expected = ready ? cfg_locales_read(path) : NULL;
    bool equal = expected && string_view_equals_view(string_view_all(sql), string_view_all(expected));
    string_free(expected);
    string_free(path);
    if (!equal)
        fputs("Calendar locale seed differs from the generator output or could not be read.\n", stderr);
    return equal;
}

/* Validate command-only arguments and publish only a complete successful generation. */
int cfg_locales_run(int argc, char **argv)
{
    bool check = false, valid = argc >= 0 && (argc == 0 || argv);
    string_t *output = NULL, *directory = NULL;
    for (int i = 0; valid && i < argc; ++i) {
        string_t *option = cfg_locales_argument(argv[i]);
        if (cfg_locales_option(option, "--check") && !check && !output) {
            check = true;
        } else if (cfg_locales_option(option, "--output") && !check && !output && i + 1 < argc) {
            output = cfg_locales_argument(argv[++i]);
            valid = output && string_byte_length(output) > 0;
        } else if (cfg_locales_option(option, "--data-dir") && !directory && i + 1 < argc) {
            directory = cfg_locales_argument(argv[++i]);
            valid = directory && string_byte_length(directory) > 0;
        } else {
            valid = false;
        }
        string_free(option);
    }
    if (!valid) {
        fputs("Usage: mars_config locales [--check | --output PATH] [--data-dir DIR]\n", stderr);
        string_free(output);
        string_free(directory);
        return 2;
    }
    const char *configured_root = getenv("MARS_ROOT");
    string_t *root = cfg_locales_argument(configured_root && *configured_root ? configured_root : MARS_CONFIG_ROOT_DIR);
    if (!directory && root) {
        directory = string_clone(root);
        const char *suffix = "/tools/mars_config/data/locales";
        if (directory && string_append_utf8_exact(directory, suffix, strlen(suffix)) != 0) {
            string_free(directory);
            directory = NULL;
        }
    }
    string_t *sql = root && directory ? cfg_locales_generate(root, directory) : NULL;
    bool ok = sql != NULL;
    if (!ok)
        fputs("Cannot generate calendar locales: invalid or unavailable pinned input.\n", stderr);
    else if (check) {
        ok = cfg_locales_check(root, sql);
        if (ok)
            ok = puts("Calendar locale seed matches Babel 2.17.0 / CLDR 46 and explicit supplements.") >= 0;
    } else if (output) {
        ok = cfg_locales_publish(output, sql);
        if (!ok)
            fputs("Cannot publish generated calendar locale SQL.\n", stderr);
    } else {
        /* Console output must not pass exact CLDR bytes through normalising formatting. */
        ok = fwrite(string_c_str(sql), 1, string_byte_length(sql), stdout) == string_byte_length(sql);
    }
    if (ok && !output)
        ok = fflush(stdout) == 0;
    string_free(sql);
    string_free(root);
    string_free(directory);
    string_free(output);
    return ok ? 0 : 1;
}
