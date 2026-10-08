/**
 * @file checks_main.c
 * @brief Command dispatcher for the native MARS repository checks.
 *
 * Resolves the source repository from the build-time root and dispatches the
 * repository checks, coverage reports and release-evidence commands. No library build or test is initiated
 * implicitly; readme-examples alone compiles the programmes it is asked to check.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checks_process.h"
#include "checks_controls.h"
#include "checks_coverage.h"
#include "checks_evidence.h"
#include "checks_policy.h"
#include "checks_readme.h"

#ifndef MARS_CHECKS_ROOT_DIR
#define MARS_CHECKS_ROOT_DIR "."
#endif

static void checks_app_usage(void)
{
    puts("Usage: mars_checks compliance [--allow-untracked] [--quiet]\n"
         "       mars_checks markdown-api\n"
         "       mars_checks file-coverage DIRECTORY\n"
         "       mars_checks release-evidence [--library FILE] [--output FILE] [--allow-dirty]\n"
         "       mars_checks source-policy\n"
         "       mars_checks public-distribution [--staged]\n"
         "       mars_checks readme-examples --libs LIBS [--cc CC] [--cflags FLAGS]\n"
         "           [--archive FILE] [--compile-only] [--timeout SECONDS] [--output DIRECTORY]");
}

static int checks_app_compliance_command(const string_t *root, int argc, char **argv)
{
    bool allow_untracked = false, quiet = false;
    for (int i = 0; i < argc; ++i) {
        string_t *option = checks_text(argv[i]);
        bool valid = true;
        if (checks_equal(option, "--allow-untracked"))
            allow_untracked = true;
        else if (checks_equal(option, "--quiet"))
            quiet = true;
        else
            valid = false;
        string_free(option);
        if (!valid)
            return 2;
    }
    return checks_compliance(root, allow_untracked, quiet);
}

static int checks_app_distribution_command(const string_t *root, int argc, char **argv)
{
    bool staged = false;
    for (int i = 0; i < argc; ++i) {
        string_t *option = checks_text(argv[i]);
        bool valid = checks_equal(option, "--staged");
        string_free(option);
        if (!valid)
            return 2;
        staged = true;
    }
    return checks_public_distribution(root, staged);
}

static int checks_app_markdown_command(const string_t *root, int argc, char **argv)
{
    (void)argv;
    return argc ? 2 : checks_markdown_api(root);
}

struct command {
    const char *name;
    int (*run)(const string_t *, int, char **);
};

static int checks_app_command_compare(const void *key, const void *element)
{
    return strcmp(key, ((const struct command *)element)->name);
}

/* Dispatch a single native repository command. */
int main(int argc, char **argv)
{
    static const struct command commands[] = {{"compliance", checks_app_compliance_command},
                                              {"file-coverage", checks_coverage},
                                              {"markdown-api", checks_app_markdown_command},
                                              {"public-distribution", checks_app_distribution_command},
                                              {"readme-examples", checks_readme},
                                              {"release-evidence", checks_evidence},
                                              {"source-policy", checks_policy}};
    if (argc < 2) {
        checks_app_usage();
        return 2;
    }
    for (int i = 1; i < argc; ++i) {
        string_t *option = checks_text(argv[i]);
        bool help = checks_equal(option, "--help") || checks_equal(option, "-h");
        string_free(option);
        if (help) {
            checks_app_usage();
            return 0;
        }
    }
    const struct command *command =
        bsearch(argv[1], commands, sizeof(commands) / sizeof(*commands), sizeof(*commands), checks_app_command_compare);
    if (!command) {
        checks_app_usage();
        return 2;
    }
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    if (!checks_process_interrupt_begin())
        checks_fatal("installing interruption handlers");
    int result = command->run(root, argc - 2, argv + 2);
    string_free(root);
    int interrupted = checks_process_interrupt_signal();
    checks_process_interrupt_end();
    if (interrupted)
        return 128 + interrupted;
    return result;
}
