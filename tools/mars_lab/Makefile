# One Lab makefile: standalone commands delegate to the root build; when included
# by the root it supplies the source inventory and native compilation/linking rules.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
# Lab-local entry point; the root graph owns compiler/dependency configuration.
# Keeping one graph avoids duplicate library builds and divergent link flags.
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0

.PHONY: all test browser-test memtest run clean help check-wasm-deps wasm-asm typescript typecheck
typescript:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" lab-typescript

typecheck:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" lab-typecheck

check-wasm-deps:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" DEBUG=$(DEBUG) check-lab-wasm-deps

wasm-asm:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" DEBUG=$(DEBUG) lab-wasm-asm

all:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" DEBUG=$(DEBUG) native-lab

test:
	+$(MAKE) --no-print-directory -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) test_lab_native

browser-test:
	+$(MAKE) --no-print-directory -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) lab-browser-test

memtest:
	+$(MAKE) --no-print-directory -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) memtest_lab_native

run: all
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" DEBUG=$(DEBUG) mars-lab ARGS="$(ARGS)"

clean:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" clean-lab

help:
	@echo "MARS Lab: make [DEBUG=1] | make test | make browser-test | make memtest | make run ARGS='--no-browser' | make clean"
	@echo "Executable: build/release/mars_lab (or build/debug/mars_lab)"
	@echo "Optional assembly: make [DEBUG=1] wasm-asm"

else

# Source inventory shared by the root build and the Lab's local entry point.
# Paths are relative to the repository root; configurations never share objects.
LAB_CONFIG := $(notdir $(BUILD_DIR))
LAB_BUILD_DIR := tools/mars_lab/build/$(LAB_CONFIG)
LAB_SRCS := $(sort $(wildcard tools/mars_lab/src/*/*.c))
LAB_OBJS := $(patsubst tools/mars_lab/src/%.c,$(LAB_BUILD_DIR)/%.o,$(LAB_SRCS))
LAB_OBJS += $(APP_RESOURCE_OBJ)
LAB_SERVER := $(LAB_BUILD_DIR)/mars_lab
LAB_SUPPORT_OBJS := $(filter-out $(LAB_BUILD_DIR)/app/lab_main.o,$(LAB_OBJS))
LAB_TEST_SRCS := $(filter-out tools/mars_lab/tests/test_lab_cache_memory.c,$(sort $(wildcard tools/mars_lab/tests/*.c)))
LAB_TEST_HEADERS := $(wildcard tools/mars_lab/tests/*.h)
LAB_TEST_OBJS := $(patsubst tools/mars_lab/tests/%.c,$(LAB_BUILD_DIR)/tests/%.o,$(LAB_TEST_SRCS))
LAB_TEST_OBJS += $(LAB_BUILD_DIR)/tests/lab_serial_native.o
LAB_TEST_BIN := $(LAB_BUILD_DIR)/tests/test_lab_native

LAB_CACHE_MEMORY_BIN := $(LAB_BUILD_DIR)/tests/test_lab_cache_memory
LAB_CACHE_MEMORY_SRCS := tools/mars_lab/tests/test_lab_cache_memory.c \
    tools/mars_lab/wasm/src/lab_flag_cache.c tools/mars_lab/wasm/src/lab_country_cache.c \
    tools/mars_lab/wasm/src/lab_serial.c

$(LAB_CACHE_MEMORY_BIN): $(LAB_CACHE_MEMORY_SRCS) $(wildcard tools/mars_lab/wasm/include/*.h) $(STATIC_LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -UNDEBUG -Iinclude -Itools/mars_lab/wasm/include -o $@ $(LAB_CACHE_MEMORY_SRCS) $(STATIC_LIB) $(LDLIBS)

.PHONY: lab-cache-memtest
lab-cache-memtest: $(LAB_CACHE_MEMORY_BIN)
	valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 $(LAB_CACHE_MEMORY_BIN)

# Browser adapters are authored in strict TypeScript. TypeScript 7's native
# compiler needs neither Node.js nor npm; callers may supply its absolute path.
LAB_TSC ?= $(TSC)
LAB_TS_CONFIG := tools/mars_lab/tsconfig.json
LAB_TEST_TS_CONFIG := tools/mars_lab/tsconfig.tests.json

# Optional offline asset preparation only; ordinary builds use the bundled tiles.
LAB_GDAL_CFLAGS ?= $(shell pkg-config --cflags gdal 2>/dev/null)
LAB_GDAL_LIBS ?= -lgdal
$(LAB_BUILD_DIR)/satellite_coastmask: tools/mars_lab/satellite_mask.c tools/mars_lab/satellite_mask_main.c tools/mars_lab/include/satellite_mask.h $(STATIC_LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -Itools/mars_lab/include $(LAB_GDAL_CFLAGS) $(LDFLAGS) \
	    -o $@ tools/mars_lab/satellite_mask.c tools/mars_lab/satellite_mask_main.c $(STATIC_LIB) $(LDLIBS) $(LAB_GDAL_LIBS)

$(LAB_BUILD_DIR)/tests/test_satellite_mask: tools/mars_lab/tests/satellite/test_satellite_mask.c tools/mars_lab/satellite_mask.c tools/mars_lab/include/satellite_mask.h $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -Itools/mars_lab/include -Itests/include $(LAB_GDAL_CFLAGS) $(LDFLAGS) \
	    -o $@ tools/mars_lab/tests/satellite/test_satellite_mask.c tools/mars_lab/satellite_mask.c \
	    $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDLIBS) $(LAB_GDAL_LIBS)

LAB_JPEG_CFLAGS ?=
LAB_JPEG_LIBS ?= -ljpeg -lz
$(LAB_BUILD_DIR)/satellite_catalogue: tools/mars_lab/satellite_catalogue.c $(STATIC_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -lz

$(LAB_BUILD_DIR)/satellite_fetch: tools/mars_lab/satellite_fetch.c $(STATIC_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(LAB_BUILD_DIR)/satellite_batch: tools/mars_lab/satellite_batch.c $(STATIC_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(LAB_BUILD_DIR)/satellite_tiles: tools/mars_lab/satellite_tiles.c tools/mars_lab/satellite_mask.c tools/mars_lab/include/satellite_mask.h $(STATIC_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -Itools/mars_lab/include $(LAB_GDAL_CFLAGS) $(LDFLAGS) \
	    -o $@ tools/mars_lab/satellite_tiles.c tools/mars_lab/satellite_mask.c $(STATIC_LIB) $(LDLIBS) $(LAB_GDAL_LIBS)

$(LAB_BUILD_DIR)/texture_tiles: tools/mars_lab/texture_tiles.c $(STATIC_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(LAB_JPEG_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) $(LAB_JPEG_LIBS)

.PHONY: lab-typescript lab-test-typescript lab-typecheck
lab-typescript: check-lab-typescript-deps
	"$(LAB_TSC)" --project $(LAB_TS_CONFIG)

lab-typecheck: check-lab-typescript-deps
	"$(LAB_TSC)" --project $(LAB_TEST_TS_CONFIG) --noEmit

lab-test-typescript: lab-typescript
	"$(LAB_TSC)" --project $(LAB_TEST_TS_CONFIG)

LAB_WASM := $(LAB_BUILD_DIR)/wasm/lab_browser.wasm
LAB_CODEC_WASM := $(LAB_BUILD_DIR)/wasm/lab_codec.wasm
LAB_WASM_SRCS := $(sort $(wildcard tools/mars_lab/wasm/src/*.c))
LAB_WASM_OBJS := $(patsubst tools/mars_lab/wasm/src/%.c,$(LAB_BUILD_DIR)/wasm/src/%.o,$(LAB_WASM_SRCS))
LAB_WASM_WIRE_OBJ := $(LAB_BUILD_DIR)/wasm/marslib/protobuf_wire.o
LAB_WASM_OBJS += $(LAB_WASM_WIRE_OBJ)
LAB_WASM_OBJS += $(LAB_BUILD_DIR)/wasm/marslib/dictionary.o
LAB_WASM_OBJS += $(LAB_BUILD_DIR)/wasm/marslib/array.o
LAB_WASM_OBJS += $(LAB_BUILD_DIR)/wasm/marslib/http_receive.o
LAB_WASM_OBJS += $(LAB_BUILD_DIR)/wasm/marslib/webgl_mask.o
LAB_WASM_OBJS += $(LAB_BUILD_DIR)/wasm/marslib/webgl_renderer.o
LAB_WASM_OBJS += $(LAB_BUILD_DIR)/wasm/marslib/globe_view.o
LAB_WASM_OBJS += $(LAB_BUILD_DIR)/wasm/marslib/quaternion.o
LAB_WASM_ASM := $(LAB_WASM_OBJS:.o=.s)
# Debian/Ubuntu's wasi-libc keeps musl maths in libc.a; libm.a is an empty compatibility archive.
# Override both paths for another installation. Only referenced archive members are linked.
LAB_WASM_INCLUDE ?= /usr/include/wasm32-wasi
LAB_WASM_LIBC ?= /usr/lib/wasm32-wasi/libc.a
# Retain the memory ABI used by the codec and direct browser regression fixtures.
LAB_WASM_MEMORY_EXPORTS := --export=memset --export=memcpy --export=memmove --export=memcmp
LAB_WASM_CFLAGS := --target=wasm32-unknown-unknown -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fvisibility=default \
                   -nostdlib -nostdlibinc -Wmissing-prototypes -isystem "$(LAB_WASM_INCLUDE)" -Iinclude -Itools/mars_lab/wasm/include

# Link generic containers separately; do not pull in the native libmars archive.
$(LAB_BUILD_DIR)/wasm/marslib/quaternion.o: src/quaternion/quaternion.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/quaternion.s: src/quaternion/quaternion.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $@.d -MT $@ -S $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/webgl_%.o: src/webgl/webgl_%.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/webgl_%.s: src/webgl/webgl_%.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $@.d -MT $@ -S $< -o $@

# array_t uses owner-thread locking in non-shared WASM, without a WASI pthread dependency.
$(LAB_BUILD_DIR)/wasm/marslib/http_receive.o: src/http/http_receive.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/http_receive.s: src/http/http_receive.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $@.d -MT $@ -S $< -o $@

$(LAB_BUILD_DIR)/wasm/src/lab_globe_fetch.o $(LAB_BUILD_DIR)/wasm/src/lab_globe_fetch.s: LAB_WASM_CFLAGS += --target=wasm32-wasi

$(LAB_BUILD_DIR)/wasm/marslib/array.o: src/array/array.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -D_GNU_SOURCE -fvisibility=hidden -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/array.s: src/array/array.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -D_GNU_SOURCE -fvisibility=hidden -MMD -MP -MF $@.d -MT $@ -S $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/dictionary.o: src/dictionary/dictionary.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) -D_GNU_SOURCE -fvisibility=hidden -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/dictionary.s: src/dictionary/dictionary.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) -D_GNU_SOURCE -fvisibility=hidden -MMD -MP -MF $@.d -MT $@ -S $< -o $@

# Compile only the allocation-free wire core, not the owning native codec.
# Keep shared library helpers private so unreferenced functions can be discarded by the linker.
$(LAB_BUILD_DIR)/wasm/marslib/%.o: src/protobuf/%.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) -fvisibility=hidden -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/%.s: src/protobuf/%.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) -fvisibility=hidden -MMD -MP -MF $@.d -MT $@ -S $< -o $@

$(LAB_BUILD_DIR)/wasm/src/%.o: tools/mars_lab/wasm/src/%.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# Optional inspection output; separate dependency files must not replace the object dependencies.
$(LAB_BUILD_DIR)/wasm/src/%.s: tools/mars_lab/wasm/src/%.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) -MMD -MP -MF $@.d -MT $@ -S $< -o $@

.PHONY: lab-wasm-asm
lab-wasm-asm: $(LAB_WASM_ASM)
	@echo "MARS Lab WebAssembly assembly: $(LAB_BUILD_DIR)/wasm/src/"

-include $(LAB_WASM_OBJS:.o=.d)
-include $(LAB_WASM_ASM:%=%.d)

# Only the declared browser bridge may remain unresolved; never import missing libc or module functions.
# Export application definitions, not the library's hidden helpers and WASI entry points.
$(LAB_WASM): $(LAB_WASM_OBJS) $(LAB_WASM_LIBC) tools/mars_lab/wasm/lab_browser.imports tools/mars_lab/Makefile
	"$(LAB_WASM_LD)" --no-entry --export-dynamic --export-memory --allow-undefined-file=tools/mars_lab/wasm/lab_browser.imports \
	    $(LAB_WASM_MEMORY_EXPORTS) --initial-memory=69206016 --max-memory=100663296 \
	    $(LAB_WASM_OBJS) "$(LAB_WASM_LIBC)" -o $@

# Probe the actual compiler target and linker, not merely their presence on PATH.
# Keep this Lab-specific: native library builds do not need a WebAssembly toolchain.
LAB_WASM_CC ?= clang
LAB_WASM_LD ?= wasm-ld

# Workers need only the codec and queue policy, never DOM imports or UI state.
$(LAB_CODEC_WASM): $(addprefix $(LAB_BUILD_DIR)/wasm/src/,lab_browser.o lab_browser_storage.o lab_jobs.o lab_job_worker.o) $(LAB_WASM_WIRE_OBJ) $(LAB_WASM_LIBC) tools/mars_lab/wasm/lab_codec.imports tools/mars_lab/Makefile
	"$(LAB_WASM_LD)" --no-entry --export-dynamic --export-memory --allow-undefined-file=tools/mars_lab/wasm/lab_codec.imports \
	    $(LAB_WASM_MEMORY_EXPORTS) --initial-memory=33554432 --max-memory=33554432 \
	    $(filter %.o,$^) "$(LAB_WASM_LIBC)" -o $@
.PHONY: check-lab-wasm-deps
check-lab-wasm-deps:
	@set -eu; \
	for tool in "$(LAB_WASM_CC)" "$(LAB_WASM_LD)"; do \
	    if ! command -v "$$tool" >/dev/null 2>&1; then \
	        echo "Missing MARS Lab WebAssembly tool: $$tool" >&2; \
	        echo "Debian/Ubuntu: sudo apt install clang lld wasi-libc" >&2; \
	        exit 1; \
	    fi; \
	done; \
	if ! test -r "$(LAB_WASM_INCLUDE)/math.h" || ! test -r "$(LAB_WASM_INCLUDE)/string.h" \
	    || ! test -r "$(LAB_WASM_LIBC)"; then \
	    echo "Missing MARS Lab WebAssembly C library headers or archive (wasi-libc)." >&2; \
	    echo "LAB_WASM_INCLUDE=$(LAB_WASM_INCLUDE)" >&2; \
	    echo "LAB_WASM_LIBC=$(LAB_WASM_LIBC)" >&2; \
	    echo "Debian/Ubuntu: sudo apt install wasi-libc" >&2; \
	    echo "For another installation, set LAB_WASM_INCLUDE and LAB_WASM_LIBC." >&2; \
	    exit 1; \
	fi; \
	probe_dir=$$(mktemp -d -t mars-check-wasm.XXXXXX); \
	trap 'rm -f "$$probe_dir/probe.o" "$$probe_dir/array.o" "$$probe_dir/probe.wasm" "$$probe_dir/probe.log"; rmdir "$$probe_dir"' EXIT; \
	trap 'exit 1' HUP INT TERM; \
	if ! printf '%s\n' '#include <math.h>' '#include <string.h>' '#include "array-t.h"' \
	    'void mars_wasm_array_probe(void) { array_t *a = array_create(sizeof(int), 0, 0); array_destroy(a); }' \
	    'double mars_wasm_probe(double x, double y) { return sin(x) + cos(x) + atan2(y, x) + exp2(x) + log2(y); }' \
	    'int mars_wasm_memory_probe(char *dst, const char *src, unsigned length) {' \
	    'memset(dst, 0, length); memcpy(dst, src, length); memmove(dst, src, length);' \
	    'return memcmp(dst, src, length) + (int)strlen(src); }' \
	    | "$(LAB_WASM_CC)" --target=wasm32-unknown-unknown -ffreestanding -fno-builtin -fvisibility=default -nostdlib -nostdlibinc \
	        -isystem "$(LAB_WASM_INCLUDE)" -Iinclude -x c -c - \
	        -o "$$probe_dir/probe.o" >"$$probe_dir/probe.log" 2>&1; then \
	    echo "MARS Lab requires Clang's wasm32-unknown-unknown target and WASM maths headers." >&2; \
	    cat "$$probe_dir/probe.log" >&2; \
	    echo "Debian/Ubuntu: sudo apt install clang lld wasi-libc" >&2; \
	    exit 1; \
	fi; \
	if ! "$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -D_GNU_SOURCE -fvisibility=hidden \
	    -c src/array/array.c -o "$$probe_dir/array.o" >"$$probe_dir/probe.log" 2>&1; then \
	    echo "MARS Lab requires target C headers for its non-shared WASM containers; host libc headers are not supported." >&2; \
	    cat "$$probe_dir/probe.log" >&2; exit 1; \
	fi; \
	if ! "$(LAB_WASM_LD)" --no-entry --export-dynamic --export-memory "$$probe_dir/probe.o" "$$probe_dir/array.o" "$(LAB_WASM_LIBC)" \
	    -o "$$probe_dir/probe.wasm" >"$$probe_dir/probe.log" 2>&1; then \
	    echo "MARS Lab requires wasm-ld and WASM maths that link without host imports." >&2; \
	    cat "$$probe_dir/probe.log" >&2; \
	    echo "Debian/Ubuntu: sudo apt install clang lld wasi-libc" >&2; \
	    exit 1; \
	fi; \
	test -s "$$probe_dir/probe.wasm"; \
	echo "MARS Lab WebAssembly compile/link probe passed."

$(filter-out $(APP_RESOURCE_OBJ),$(LAB_OBJS)) $(LAB_SERVER) $(LAB_TEST_OBJS) $(LAB_TEST_BIN) $(LAB_BUILD_DIR)/tests/test_lab_browser: | check-lab-wasm-deps

# Native Lab build rules. Included by the root Makefile after library variables.
# The local Makefile delegates to this same graph, reusing MARS dependency checks.
$(filter-out $(APP_RESOURCE_OBJ),$(LAB_OBJS)) $(LAB_SERVER): | native-tools check-native-deps

$(LAB_BUILD_DIR)/%.o: tools/mars_lab/src/%.c tools/mars_lab/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/src -c $< -o $@

$(LAB_SERVER): $(LAB_OBJS) $(STATIC_LIB) $(LAB_WASM) $(LAB_CODEC_WASM) | lab-typescript
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(LAB_OBJS) $(STATIC_LIB) $(LDLIBS)

$(LAB_BUILD_DIR)/tests/%.o: tools/mars_lab/tests/%.c $(LAB_TEST_HEADERS) $(TEST_SHARED_HEADERS) tools/mars_lab/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/src -c $< -o $@

# Link the same native adapters as the application, excluding its main function.
$(LAB_BUILD_DIR)/tests/lab_serial_native.o: tools/mars_lab/wasm/src/lab_serial.c tools/mars_lab/wasm/include/lab_serial.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/mars_lab/wasm/include -c $< -o $@

$(LAB_BUILD_DIR)/tests/test_lab_wasm_build.o: CFLAGS += -Itools/mars_lab/wasm/include

$(LAB_TEST_BIN): $(LAB_TEST_OBJS) $(LAB_SUPPORT_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LAB_SERVER)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(LAB_TEST_OBJS) $(LAB_SUPPORT_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDLIBS)

.PHONY: native-lab clean-lab
native-lab: $(LAB_SERVER)

LAB_BROWSER_SRCS := tools/mars_lab/tests/browser/test_lab_browser.c tools/mars_lab/tests/browser/test_lab_browser_fixtures.c
$(LAB_BUILD_DIR)/tests/test_lab_browser: $(LAB_BROWSER_SRCS) tools/mars_lab/tests/browser/test_lab_browser_fixtures.h $(LAB_TEST_HEADERS) $(LAB_BUILD_DIR)/tests/test_lab_support.o $(TEST_COMMON_HELPER_OBJS) $(LAB_SUPPORT_OBJS) $(STATIC_LIB) $(LAB_SERVER) $(LAB_WASM)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/src -Itools/mars_lab/tests -o $@ $(LAB_BROWSER_SRCS) \
	    $(LAB_BUILD_DIR)/tests/test_lab_support.o $(TEST_COMMON_HELPER_OBJS) $(LAB_SUPPORT_OBJS) $(STATIC_LIB) $(LDFLAGS) $(LDLIBS)

.PHONY: lab-browser-test
FIREFOX ?= firefox
lab-browser-test: $(LAB_BUILD_DIR)/tests/test_lab_browser lab-test-typescript
	TMPDIR="$(abspath $(LAB_BUILD_DIR))" MARS_LAB_BROWSER="$(FIREFOX)" $(LAB_BUILD_DIR)/tests/test_lab_browser

clean-lab:
	rm -rf tools/mars_lab/build

endif

# Pure geographic view policy is shared with native globe consumers, without SQLite linkage.
$(LAB_BUILD_DIR)/wasm/marslib/globe_view.o: src/globe/globe_view.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(LAB_BUILD_DIR)/wasm/marslib/globe_view.s: src/globe/globe_view.c tools/mars_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" $(LAB_WASM_CFLAGS) --target=wasm32-wasi -fvisibility=hidden -MMD -MP -MF $@.d -MT $@ -S $< -o $@

# globe-t.h exposes database types whose system declarations require the WASI header ABI.
$(addprefix $(LAB_BUILD_DIR)/wasm/src/,lab_globe.o lab_globe.s): LAB_WASM_CFLAGS += --target=wasm32-wasi
