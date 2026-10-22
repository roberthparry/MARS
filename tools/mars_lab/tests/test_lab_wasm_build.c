/**
 * @file test_lab_wasm_build.c
 * @brief WebAssembly header boundaries, optional assembly builds and their documented command.
 *
 * Dry runs verify release/debug paths, local delegation and independence from normal WASM linking.
 * The README case executes the real assembly target, then inspects a listing and its own dependencies.
 * All Make processes run sequentially with inherited jobserver settings removed in an isolated child.
 * Header probes ensure application APIs need no private grant and implementation headers reject
 * ungranted includes, including repeated includes and IntelliSense translation units.
 * Dependency checks reject absent maths headers and invalid archives with installation guidance.
 */
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

#include "file-t.h"
#include "lab_process.h"
#include "lab_serial.h"
#include "test_harness.h"
#include "test_lab_support.h"

typedef struct _serial_fixture_t {
    lab_serial_t serial;
    unsigned count;
    bool coherent;
} serial_fixture_t;

typedef struct _serial_worker_t {
    serial_fixture_t *fixture;
    bool succeeded;
} serial_worker_t;

static void serial_increment(void *context)
{
    serial_fixture_t *fixture = context;
    unsigned before = fixture->count;
    sched_yield();
    fixture->coherent = fixture->coherent && fixture->count == before;
    fixture->count = before + 1;
}

static void *serial_worker(void *context)
{
    serial_worker_t *worker = context;
    worker->succeeded = true;
    for (unsigned i = 0; i < 10000; ++i)
        if (!lab_serial_dispatch(&worker->fixture->serial, serial_increment, worker->fixture)) {
            worker->succeeded = false;
            break;
        }
    return NULL;
}

static void test_lab_serial_dispatch(void)
{
    serial_fixture_t fixture = {.serial = LAB_SERIAL_INITIALIZER, .coherent = true};
    serial_worker_t workers[4] = {{.fixture = &fixture}, {.fixture = &fixture},
                                  {.fixture = &fixture}, {.fixture = &fixture}};
    pthread_t threads[4];
    unsigned started = 0;
    while (started < 4 && !pthread_create(&threads[started], NULL, serial_worker, &workers[started])) ++started;
    bool ok = started == 4;
    for (unsigned i = 0; i < started; ++i) {
        ok = !pthread_join(threads[i], NULL) && ok;
        ok = workers[i].succeeded && ok;
    }
    ok = !lab_serial_dispatch(NULL, serial_increment, &fixture) &&
         !lab_serial_dispatch(&fixture.serial, NULL, &fixture) && ok;
    pthread_mutex_destroy(&fixture.serial.mutex);
    TEST_ASSERT_TRUE(ok && fixture.coherent && fixture.count == 40000,
                     "serial dispatch protects complete state transitions from four native contenders");
}

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
    const char *paths[] = {"build/release/wasm/src/lab_surface_math.s", "build/debug/wasm/src/lab_surface_math.s"};
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
         string_find(output, "--no-entry") >= 0 && string_find(output, "lab_job_worker.o") >= 0 &&
         string_find(output, " -isystem ") >= 0 && string_find(output, "libc.a") >= 0 &&
         string_find(output, "src/protobuf/protobuf_wire.c") >= 0 && string_find(output, "-fvisibility=hidden") >= 0;
    string_free(output);
    const char *codec[] = {"make", "--no-print-directory", "-j1", "-n", "-B", "-o", "check-lab-wasm-deps",
                           "DEBUG=0", "tools/mars_lab/build/release/wasm/lab_codec.wasm", NULL};
    output = ok ? wasm_make(codec) : NULL;
    ok = ok && output && string_find(output, "lab_job_worker.o") >= 0 &&
         string_find(output, "lab_codec.imports") >= 0 && string_find(output, "lab_dom.o") < 0 &&
         string_find(output, "libc.a") >= 0 && string_find(output, "lab_memory.o") < 0 &&
         string_find(output, "wasm/marslib/protobuf_wire.o") >= 0 && string_find(output, "libmars.a") < 0 &&
         string_find(output, "src/protobuf/protobuf.c") < 0;
    string_free(output);
    return ok;
}

static void test_lab_wasm_assembly_plans(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(wasm_build_plans), "assembly targets are optional and configuration-specific");
}

/* Compile one disposable header probe without linking or executing it. */
static bool wasm_header_probe(const char *path, const char *source, bool accepted, bool intellisense)
{
    file_t *file = file_new_cstr(path);
    string_t *text = string_new_with(source), *output = NULL;
    bool written = file && text && file_write_all_text(file, text);
    const char *compiler = getenv("LAB_WASM_CC");
    if (!compiler || !*compiler)
        compiler = "clang";
    const char *args[] = {compiler, "--target=wasm32-unknown-unknown", "-ffreestanding", "-nostdlib",
                         "-fsyntax-only", "-Werror", "-Itools/mars_lab/wasm/include",
                         "-Itools/mars_lab/wasm/src", intellisense ? "-D__INTELLISENSE__=1" : "-U__INTELLISENSE__",
                         path, NULL};
    int status = -1;
    bool ok = written && lab_proc_run(args, NULL, 10000, 128u * 1024u, &output, &status) &&
              ((status == 0) == accepted);
    if (!ok)
        string_fprintf(stderr, "WASM header probe failed (%d): %s\n", status,
                       output ? string_c_str(output) : "no output");
    string_free(output);
    string_free(text);
    file_free(file);
    return ok;
}

/* Check both public APIs and every private implementation-header access pattern. */
static bool wasm_header_boundaries(const char *directory)
{
    string_t *path = string_sprintf("%s/header_probe.c", directory);
    bool ok = path && wasm_header_probe(string_c_str(path),
        "#include \"lab_binding.h\"\n#include \"lab_workspace_dom.h\"\n", true, false);
    const char *headers[] = {"lab_binding_internal.h", "lab_flow_request_internal.h"};
    const char *grants[] = {"MARS_LAB_BINDING_INTERNAL_ACCESS", "MARS_LAB_FLOW_REQUEST_INTERNAL_ACCESS"};
    /* These two private headers are an explicit, bounded list, not a runtime lookup table. */
    for (size_t i = 0; ok && i < sizeof headers / sizeof *headers; ++i) {
        string_t *denied = string_sprintf("#include \"%s\"\n", headers[i]);
        string_t *allowed = string_sprintf("#define %s\n#include \"%s\"\n", grants[i], headers[i]);
        string_t *repeated = string_sprintf("#define %s\n#include \"%s\"\n#undef %s\n#include \"%s\"\n",
                                           grants[i], headers[i], grants[i], headers[i]);
        ok = denied && allowed && repeated &&
             wasm_header_probe(string_c_str(path), string_c_str(denied), false, false) &&
             wasm_header_probe(string_c_str(path), string_c_str(denied), false, true) &&
             wasm_header_probe(string_c_str(path), string_c_str(allowed), true, false) &&
             wasm_header_probe(string_c_str(path), string_c_str(repeated), false, true);
        string_free(repeated);
        string_free(allowed);
        string_free(denied);
    }
    string_free(path);
    return ok;
}

static void test_lab_wasm_header_boundaries(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(wasm_header_boundaries), "public APIs and private access grants remain separate");
}

/* Check failure diagnostics using an empty, isolated directory and a deliberately invalid archive. */
static bool wasm_math_dependency(const char *directory)
{
    string_t *include = string_sprintf("LAB_WASM_INCLUDE=%s", directory);
    const char *missing[] = {"make", "--no-print-directory", "-s", "-j1", "check-lab-wasm-deps",
                            include ? string_c_str(include) : "", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ok = include && wasm_make_environment() &&
              lab_proc_run(missing, NULL, 10000, 128u * 1024u, &output, &status) && status != 0 &&
              output && string_find(output, "sudo apt install wasi-libc") >= 0 &&
              string_find(output, "LAB_WASM_INCLUDE=") >= 0;
    string_free(output);
    string_free(include);

    string_t *path = string_sprintf("%s/invalid.a", directory);
    file_t *archive = path ? file_new(path) : NULL;
    string_t *invalid = string_new_with("This is not a WebAssembly archive.\n");
    string_t *option = path ? string_sprintf("LAB_WASM_LIBC=%s", string_c_str(path)) : NULL;
    const char *bad_archive[] = {"make", "--no-print-directory", "-s", "-j1", "check-lab-wasm-deps",
                                option ? string_c_str(option) : "", NULL};
    output = NULL;
    status = -1;
    ok = ok && archive && invalid && option && file_write_all_text(archive, invalid) &&
         lab_proc_run(bad_archive, NULL, 10000, 128u * 1024u, &output, &status) && status != 0 &&
         output && string_find(output, "WASM maths that link without host imports") >= 0 &&
         string_find(output, "sudo apt install clang lld wasi-libc") >= 0;
    string_free(output);
    string_free(option);
    string_free(invalid);
    file_free(archive);
    string_free(path);
    return ok;
}

static void test_lab_wasm_math_dependency(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(wasm_math_dependency), "maths dependencies are checked with actionable diagnostics");
}

static bool wasm_readme(const char *directory)
{
    (void)directory;
    const char *args[] = {"make", "--no-print-directory", "-s", "-j1", "lab-wasm-asm", NULL};
    string_t *output = wasm_make_environment() ? wasm_make(args) : NULL;
    bool ok = output && string_view_equals_literal(string_view_all(output),
        "MARS Lab WebAssembly compile/link probe passed.\n"
        "MARS Lab WebAssembly assembly: tools/mars_lab/build/release/wasm/src/\n");
    file_t *listing = file_new_cstr("tools/mars_lab/build/release/wasm/src/lab_surface_math.s");
    file_t *dependency = file_new_cstr("tools/mars_lab/build/release/wasm/src/lab_surface_math.s.d");
    string_t *assembly = listing ? file_read_all_text(listing) : NULL;
    string_t *dependencies = dependency ? file_read_all_text(dependency) : NULL;
    ok = ok && assembly && dependencies && string_find(assembly, ".functype") >= 0 &&
         string_find(assembly, "call\tsin") >= 0 && string_find(dependencies, "lab_surface_math.s:") >= 0 &&
         string_find(dependencies, "tools/mars_lab/wasm/include/lab_surface_math.h") >= 0;
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
    TEST_RUN_IN_GROUP(test_lab_serial_dispatch, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wasm_assembly_plans, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wasm_header_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_wasm_math_dependency, tests, NULL);
}

/* Generate and verify the documented listing after ordinary tests. */
void test_lab_wasm_build_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_lab_wasm_assembly, readme_examples, "wasm,readme,output");
}
