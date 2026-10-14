/**
 * @file test_lab_wasm_build.c
 * @brief Optional WebAssembly assembly build graphs and their documented command.
 *
 * Dry runs verify release/debug paths, local delegation and independence from normal WASM linking.
 * The README case executes the real assembly target, then inspects a listing and its own dependencies.
 * All Make processes run sequentially with inherited jobserver settings removed in an isolated child.
 */
#include <stdio.h>
#include <stdlib.h>

#include "file-t.h"
#include "lab_process.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool wasm_make_environment(void)
{
    return !unsetenv("MAKEFLAGS") && !unsetenv("MFLAGS") && !unsetenv("MAKEOVERRIDES") &&
           !unsetenv("MAKEFILES") && !unsetenv("DEBUG");
}

static string_t *wasm_make(const char *const *args)
{
    string_t *output = NULL;
    int status = -1;
    if (lab_proc_run(args, NULL, 60000, 4u * 1024u * 1024u, &output, &status) && status == 0)
        return output;
    string_fprintf(stderr, "WASM Make failed (%d): %s\n", status, output ? string_c_str(output) : "no output");
    string_free(output);
    return NULL;
}

static bool wasm_build_plans(const char *directory)
{
    (void)directory;
    bool ok = wasm_make_environment();
    const char *configurations[] = {"DEBUG=0", "DEBUG=1"};
    const char *paths[] = {"build/release/wasm/src/lab_memory.s", "build/debug/wasm/src/lab_memory.s"};
    for (size_t i = 0; i < 2u && ok; ++i) {
        const char *args[] = {"make", "--no-print-directory", "-j1", "-n", "-B", "-o", "check-lab-wasm-deps",
                              configurations[i], "lab-wasm-asm", NULL};
        string_t *output = wasm_make(args);
        ok = output && string_find(output, paths[i]) >= 0 && string_find(output, ".s.d -MT ") >= 0 &&
             string_find(output, " -S ") >= 0 && string_find(output, " -c ") < 0 &&
             string_find(output, "--no-entry") < 0;
        string_free(output);
    }
    const char *local[] = {"make", "--no-print-directory", "-j1", "-n", "-B", "-C", "tools/mars_lab",
                           "DEBUG=1", "wasm-asm", NULL};
    string_t *output = ok ? wasm_make(local) : NULL;
    ok = ok && output && string_find(output, paths[1]) >= 0 && string_find(output, " -S ") >= 0;
    string_free(output);
    const char *normal[] = {"make", "--no-print-directory", "-j1", "-n", "-B", "-o", "check-lab-wasm-deps",
                            "DEBUG=0", "tools/mars_lab/build/release/wasm/lab_browser.wasm", NULL};
    output = ok ? wasm_make(normal) : NULL;
    ok = ok && output && string_find(output, " -S ") < 0 && string_find(output, " -c ") >= 0 &&
         string_find(output, "--no-entry") >= 0 && string_find(output, "lab_job_worker.o") >= 0;
    string_free(output);
    const char *codec[] = {"make", "--no-print-directory", "-j1", "-n", "-B", "-o", "check-lab-wasm-deps",
                           "DEBUG=0", "tools/mars_lab/build/release/wasm/lab_codec.wasm", NULL};
    output = ok ? wasm_make(codec) : NULL;
    ok = ok && output && string_find(output, "lab_job_worker.o") >= 0 &&
         string_find(output, "lab_codec.imports") >= 0 && string_find(output, "lab_dom.o") < 0;
    string_free(output);
    return ok;
}

static void test_lab_wasm_assembly_plans(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(wasm_build_plans), "assembly targets are optional and configuration-specific");
}

static bool wasm_readme(const char *directory)
{
    (void)directory;
    const char *args[] = {"make", "--no-print-directory", "-s", "-j1", "lab-wasm-asm", NULL};
    string_t *output = wasm_make_environment() ? wasm_make(args) : NULL;
    bool ok = output && string_view_equals_literal(string_view_all(output),
        "MARS Lab WebAssembly compile/link probe passed.\n"
        "MARS Lab WebAssembly assembly: tools/mars_lab/build/release/wasm/src/\n");
    file_t *listing = file_new_cstr("tools/mars_lab/build/release/wasm/src/lab_memory.s");
    file_t *dependency = file_new_cstr("tools/mars_lab/build/release/wasm/src/lab_memory.s.d");
    string_t *assembly = listing ? file_read_all_text(listing) : NULL;
    string_t *dependencies = dependency ? file_read_all_text(dependency) : NULL;
    ok = ok && assembly && dependencies && string_find(assembly, ".functype") >= 0 &&
         string_find(assembly, "i32.store8") >= 0 && string_find(dependencies, "lab_memory.s:") >= 0 &&
         string_find(dependencies, "tools/mars_lab/wasm/include/lab_memory.h") >= 0;
    string_free(dependencies);
    string_free(assembly);
    file_free(dependency);
    file_free(listing);
    string_free(output);
    return ok;
}

/* README example: docs/building.md, inspecting WebAssembly assembly. */
static void example_lab_wasm_assembly(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(wasm_readme), "documented command generates assembly and header dependencies");
    puts("MARS Lab WebAssembly compile/link probe passed.");
    puts("MARS Lab WebAssembly assembly: tools/mars_lab/build/release/wasm/src/");
}

/* Register ordinary build-graph checks before README output. */
void test_lab_wasm_build_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_wasm_assembly_plans, tests, NULL);
}

/* Generate and verify the documented listing after ordinary tests. */
void test_lab_wasm_build_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_lab_wasm_assembly, readme_examples, "wasm,readme,output");
}
