/**
 * @file checks_readme.c
 * @brief Compile and run documented C programmes sequentially with JSON evidence.
 *
 * Uses the native README entry in the repository's global test configuration.
 * Compiler words are parsed without a shell. Each execution owns a disposable
 * working tree and separated output files. Interruptions unwind fixture ownership.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "checks_process.h"
#include "checks_fixtures.h"
#include "checks_readme.h"

struct options {
    string_t *cc;
    string_t *cflags;
    string_t *libs;
    string_t *archive;
    string_t *output;
    bool compile_only;
    double timeout;
};

static void checks_readme_free_options(struct options *options)
{
    string_free(options->cc);
    string_free(options->cflags);
    string_free(options->libs);
    string_free(options->archive);
    string_free(options->output);
}

static bool checks_readme_parse_options(struct options *options, int argc, char **argv)
{
    *options = (struct options){.cc = checks_text("cc"),
                                .cflags = checks_text("-D_GNU_SOURCE -std=gnu11 -Wall -Wextra -Werror"),
                                .archive = checks_text("build/release/libmars.a"),
                                .output = checks_text("build/readme-examples"),
                                .timeout = 60};
    for (int i = 0; i < argc; ++i) {
        string_t *argument = checks_text(argv[i]);
        if (checks_equal(argument, "--compile-only")) {
            options->compile_only = true;
            string_free(argument);
            continue;
        }
        string_offset_t equal = string_find(argument, "=");
        string_t *key = equal >= 0 ? checks_slice(argument, 0, (size_t)equal) : string_clone(argument);
        string_t *value =
            equal >= 0     ? checks_slice(argument, (size_t)equal + 1, string_byte_length(argument) - (size_t)equal - 1)
            : i + 1 < argc ? checks_text(argv[++i])
                           : NULL;
        string_t **destination = NULL;
        if (checks_equal(key, "--cc"))
            destination = &options->cc;
        else if (checks_equal(key, "--cflags"))
            destination = &options->cflags;
        else if (checks_equal(key, "--libs"))
            destination = &options->libs;
        else if (checks_equal(key, "--archive"))
            destination = &options->archive;
        else if (checks_equal(key, "--output"))
            destination = &options->output;
        bool ok = value != NULL;
        if (destination && value) {
            string_free(*destination);
            *destination = value;
            value = NULL;
        } else if (checks_equal(key, "--timeout") && value) {
            char *end = NULL;
            options->timeout = strtod(string_c_str(value), &end);
            ok = end && end != string_c_str(value) && !*end && isfinite(options->timeout) && options->timeout > 0;
        } else {
            ok = false;
        }
        string_free(value);
        string_free(key);
        string_free(argument);
        if (!ok)
            return false;
    }
    return options->libs != NULL;
}

static bool checks_readme_enabled(const json_t *config, const string_t *id)
{
    bool result = true;
    const json_t *value = config ? json_object_get(config, id) : NULL;
    if (!value)
        value = checks_member(config, "enabled");
    if (value)
        json_bool_value(value, &result);
    return result;
}

static bool checks_readme_artifact(const string_t *output, const string_t *id, const char *suffix, const string_t *text)
{
    string_t *name = string_sprintf("%s%s", string_c_str(id), suffix);
    string_t *path = checks_path(output, string_c_str(name));
    bool ok = checks_write(path, text);
    string_free(path);
    string_free(name);
    return ok;
}

static const char *checks_readme_execute_example(const string_t *root, const struct options *options,
                                                 const string_t *output, checks_fixtures_t *fixtures,
                                                 const checks_example_t *example, json_t *record)
{
    if (!checks_example_has_main(example))
        return "C example has no main()";
    const string_t *id = checks_example_id(example);
    const string_t *code = checks_example_code(example);
    if (!checks_readme_artifact(output, id, ".c", code))
        return "could not write programme source";
    string_t *source_name = string_sprintf("%s.c", string_c_str(id));
    string_t *source = checks_path(output, string_c_str(source_name));
    string_t *binary = checks_path(output, string_c_str(id));
    string_t *archive = checks_path(root, string_c_str(options->archive));
    string_t *includes = string_sprintf("-I%s/include", string_c_str(root));
    checks_strings_t *arguments = checks_strings_new();
    bool parsed = checks_shell_words(options->cc, arguments) && checks_strings_count(arguments) &&
                  checks_shell_words(options->cflags, arguments);
    checks_strings_add(arguments, includes);
    checks_strings_add(arguments, source);
    checks_strings_add(arguments, archive);
    parsed = parsed && checks_shell_words(options->libs, arguments);
    checks_strings_add(arguments, checks_text("-o"));
    checks_strings_add(arguments, string_clone(binary));
    string_t *stdout_text = NULL, *stderr_text = NULL;
    int status = -1;
    bool ran = parsed && checks_process_run(arguments, root, 0, &stdout_text, &stderr_text, &status);
    checks_strings_free(arguments);
    string_free(source_name);
    const char *error = NULL;
    if (!parsed)
        error = "invalid compiler argument quoting";
    else if (!ran)
        error = "could not execute compiler";
    else {
        checks_append(stdout_text, stderr_text);
        if (!checks_readme_artifact(output, id, ".compile.log", stdout_text))
            error = "could not write compiler log";
        else if (status)
            error = "compile/link failure";
    }
    string_free(stdout_text);
    string_free(stderr_text);
    if (!error && !options->compile_only) {
        char temporary[] = "/tmp/mars-readme-XXXXXX";
        if (!mkdtemp(temporary)) {
            error = "could not create programme working directory";
        } else {
            string_t *working = checks_text(temporary);
            arguments = checks_strings_new();
            checks_strings_add(arguments, string_clone(binary));
            bool ready = checks_fixtures_arguments(fixtures, checks_example_path(example), code, arguments);
            stdout_text = stderr_text = NULL;
            ran =
                ready && checks_process_run(arguments, working, options->timeout, &stdout_text, &stderr_text, &status);
            int execution_error = errno;
            if (!ran)
                error = execution_error == ETIMEDOUT ? "program timed out" : "could not execute programme";
            else {
                checks_json_integer(record, "status", status);
                if (!checks_readme_artifact(output, id, ".actual", stdout_text) ||
                    !checks_readme_artifact(output, id, ".stderr", stderr_text))
                    error = "could not write programme output";
                else if (status)
                    error = "program failed";
                else if (!checks_example_expected(example))
                    error = "missing documented output";
                else {
                    string_t *actual = checks_output_normalise(stdout_text);
                    string_t *expected = checks_output_normalise(checks_example_expected(example));
                    if (string_compare(actual, expected)) {
                        error = "documented output differs";
                        if (!checks_readme_artifact(output, id, ".expected", checks_example_expected(example)))
                            error = "could not write expected output";
                    }
                    string_free(actual);
                    string_free(expected);
                }
            }
            checks_strings_free(arguments);
            string_free(stdout_text);
            string_free(stderr_text);
            if (!checks_remove_tree(working) && !error)
                error = "could not remove programme working directory";
            string_free(working);
        }
    }
    string_free(binary);
    return error;
}

/* Run README examples and retain their compiler and execution evidence. */
int checks_readme(const string_t *root, int argc, char **argv)
{
    struct options options;
    if (!checks_readme_parse_options(&options, argc, argv)) {
        fprintf(stderr, "readme-examples: --libs is required; invalid option, value or timeout\n");
        checks_readme_free_options(&options);
        return 2;
    }
    string_t *output = checks_path(root, string_c_str(options.output));
    if (!checks_mkdir(output))
        checks_fatal("creating README output directory");
    string_t *config_path = checks_path(root, "tests/test_config.json");
    json_t *configuration = json_from_file(config_path);
    string_free(config_path);
    if (!configuration)
        checks_fatal("reading global test configuration");
    const json_t *config = checks_member(configuration, "tools/mars_checks/src/readme/checks_readme.c");
    config = checks_member(config, "readme_examples");
    checks_strings_t *paths = checks_files(root, "", ".md", true);
    json_t *records = json_new_array();
    checks_fixtures_t *fixtures = checks_fixtures_new(root);
    size_t checked = 0, failed = 0;
    int infrastructure_error = 0;
    for (size_t i = 0; i < checks_strings_count(paths) && !checks_process_interrupt_signal(); ++i) {
        const string_t *relative = checks_strings_get(paths, i);
        string_t *path = checks_path(root, string_c_str(relative));
        string_t *text = checks_read(path);
        string_free(path);
        if (!text) {
            infrastructure_error = 2;
            break;
        }
        /* Python's text mode normalises CRLF before parsing fences and source. */
        string_replace(text, "\r\n", "\n");
        string_replace(text, "\r", "\n");
        checks_examples_t *examples = checks_examples_parse(relative, text);
        string_free(text);
        for (size_t j = 0; j < checks_examples_count(examples) && !checks_process_interrupt_signal(); ++j) {
            const checks_example_t *example = checks_examples_get(examples, j);
            const string_t *id = checks_example_id(example);
            if (!checks_readme_enabled(config, id)) {
                string_printf("SKIP README %s\n", string_c_str(id));
                fflush(stdout);
                continue;
            }
            ++checked;
            json_t *record = json_new_object();
            checks_json_text(record, "id", id);
            checks_json_text(record, "path", relative);
            checks_json_integer(record, "line", (long)checks_example_line(example));
            const char *error = checks_readme_execute_example(root, &options, output, fixtures, example, record);
            if (error) {
                ++failed;
                string_t *message = checks_text(error);
                checks_json_text(record, "error", message);
                string_free(message);
                if (!checks_example_has_main(example))
                    string_printf("FAIL README %s: %s\n", string_c_str(id), error);
                else
                    string_printf("FAIL README %s: %s (%s:%zu)\n", string_c_str(id), error, string_c_str(relative),
                                  checks_example_line(example));
            } else {
                string_printf("PASS README %s\n", string_c_str(id));
            }
            fflush(stdout);
            if (!json_array_append(records, record))
                checks_fatal("recording README result");
            json_free(record);
        }
        checks_examples_free(examples);
    }
    checks_fixtures_free(fixtures);
    string_t *results_path = checks_path(output, "results.json");
    string_t *results = json_to_string_pretty(records, 2);
    if (!results)
        checks_fatal("serialising README results");
    string_append_char(results, '\n');
    if (!checks_write(results_path, results))
        infrastructure_error = 2;
    string_free(results);
    string_free(results_path);
    json_free(records);
    json_free(configuration);
    checks_strings_free(paths);
    string_free(output);
    checks_readme_free_options(&options);
    printf("README programs: %zu checked, %zu failed\n", checked, failed);
    if (infrastructure_error)
        fprintf(stderr, "README check could not inspect the repository or write results\n");
    if (checks_process_interrupt_signal())
        return 128 + checks_process_interrupt_signal();
    return infrastructure_error ? infrastructure_error : failed ? 1 : 0;
}
