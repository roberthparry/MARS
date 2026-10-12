# One Lab makefile: standalone commands delegate to the root build; when included
# by the root it supplies the source inventory and native compilation/linking rules.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
# Lab-local entry point; the root graph owns compiler/dependency configuration.
# Keeping one graph avoids duplicate library builds and divergent link flags.
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0

.PHONY: all test browser-test memtest run clean help check-wasm-deps wasm-asm
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
LAB_SERVER := $(LAB_BUILD_DIR)/mars_lab
LAB_SUPPORT_OBJS := $(filter-out $(LAB_BUILD_DIR)/app/lab_main.o,$(LAB_OBJS))
LAB_TEST_SRCS := $(sort $(wildcard tools/mars_lab/tests/*.c))
LAB_TEST_HEADERS := $(wildcard tools/mars_lab/tests/*.h)
LAB_TEST_OBJS := $(patsubst tools/mars_lab/tests/%.c,$(LAB_BUILD_DIR)/tests/%.o,$(LAB_TEST_SRCS))
LAB_TEST_BIN := $(LAB_BUILD_DIR)/tests/test_lab_native

LAB_WASM := $(LAB_BUILD_DIR)/wasm/lab_browser.wasm
LAB_CODEC_WASM := $(LAB_BUILD_DIR)/wasm/lab_codec.wasm
LAB_WASM_SRCS := $(sort $(wildcard tools/mars_lab/wasm/src/*.c))
LAB_WASM_OBJS := $(patsubst tools/mars_lab/wasm/src/%.c,$(LAB_BUILD_DIR)/wasm/src/%.o,$(LAB_WASM_SRCS))
LAB_WASM_ASM := $(LAB_WASM_OBJS:.o=.s)
LAB_WASM_CFLAGS := --target=wasm32-unknown-unknown -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
                   -nostdlib -Wmissing-prototypes -Itools/mars_lab/wasm/include

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
$(LAB_WASM): $(LAB_WASM_OBJS) tools/mars_lab/wasm/lab_browser.imports tools/mars_lab/Makefile
	"$(LAB_WASM_LD)" --no-entry --export-all --allow-undefined-file=tools/mars_lab/wasm/lab_browser.imports \
	    --initial-memory=67108864 --max-memory=67108864 \
	    $(LAB_WASM_OBJS) -o $@

# Probe the actual compiler target and linker, not merely their presence on PATH.
# Keep this Lab-specific: native library builds do not need a WebAssembly toolchain.
LAB_WASM_CC ?= clang
LAB_WASM_LD ?= wasm-ld

# Workers need only the codec and queue policy, never DOM imports or UI state.
$(LAB_CODEC_WASM): $(addprefix $(LAB_BUILD_DIR)/wasm/src/,lab_browser.o lab_memory.o lab_jobs.o) tools/mars_lab/wasm/lab_codec.imports tools/mars_lab/Makefile
	"$(LAB_WASM_LD)" --no-entry --export-all --allow-undefined-file=tools/mars_lab/wasm/lab_codec.imports \
	    --initial-memory=16777216 --max-memory=16777216 $(filter %.o,$^) -o $@
.PHONY: check-lab-wasm-deps
check-lab-wasm-deps:
	@set -eu; \
	for tool in "$(LAB_WASM_CC)" "$(LAB_WASM_LD)"; do \
	    if ! command -v "$$tool" >/dev/null 2>&1; then \
	        echo "Missing MARS Lab WebAssembly tool: $$tool" >&2; \
	        echo "Debian/Ubuntu: sudo apt install clang lld" >&2; \
	        exit 1; \
	    fi; \
	done; \
	probe_dir=$$(mktemp -d /tmp/mars-check-wasm.XXXXXX); \
	trap 'rm -f "$$probe_dir/probe.o" "$$probe_dir/probe.wasm" "$$probe_dir/probe.log"; rmdir "$$probe_dir"' EXIT; \
	trap 'exit 1' HUP INT TERM; \
	if ! printf '%s\n' 'int mars_wasm_probe(int value) { return value + 1; }' \
	    | "$(LAB_WASM_CC)" --target=wasm32-unknown-unknown -ffreestanding -nostdlib -x c -c - \
	        -o "$$probe_dir/probe.o" >"$$probe_dir/probe.log" 2>&1; then \
	    echo "MARS Lab requires Clang with the wasm32-unknown-unknown target." >&2; \
	    cat "$$probe_dir/probe.log" >&2; \
	    echo "Debian/Ubuntu: sudo apt install clang lld" >&2; \
	    exit 1; \
	fi; \
	if ! "$(LAB_WASM_LD)" --no-entry --export=mars_wasm_probe "$$probe_dir/probe.o" \
	    -o "$$probe_dir/probe.wasm" >"$$probe_dir/probe.log" 2>&1; then \
	    echo "MARS Lab requires a working LLVM WebAssembly linker (wasm-ld)." >&2; \
	    cat "$$probe_dir/probe.log" >&2; \
	    echo "Debian/Ubuntu: sudo apt install clang lld" >&2; \
	    exit 1; \
	fi; \
	test -s "$$probe_dir/probe.wasm"; \
	echo "MARS Lab WebAssembly compile/link probe passed."

$(LAB_OBJS) $(LAB_SERVER) $(LAB_TEST_OBJS) $(LAB_TEST_BIN) $(LAB_BUILD_DIR)/tests/test_lab_browser: | check-lab-wasm-deps

# Native Lab build rules. Included by the root Makefile after library variables.
# The local Makefile delegates to this same graph, reusing MARS dependency checks.
$(LAB_OBJS) $(LAB_SERVER): | native-tools check-deps

$(LAB_BUILD_DIR)/%.o: tools/mars_lab/src/%.c tools/mars_lab/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/src \
	    -DMARS_LAB_ROOT_DIR='"$(CURDIR)"' \
	    -DMARS_LAB_ASSET_PATH='"$(CURDIR)/tools/mars_lab/assets/index.html"' \
	    -DMARS_LAB_SERVER_PATH='"$(abspath $(LAB_SERVER))"' -c $< -o $@

$(LAB_BUILD_DIR)/server/lab_server.o: CFLAGS += -DMARS_LAB_WASM_PATH='"$(abspath $(LAB_WASM))"'
$(LAB_BUILD_DIR)/server/lab_server.o: CFLAGS += -DMARS_LAB_CODEC_PATH='"$(abspath $(LAB_CODEC_WASM))"'

$(LAB_SERVER): $(LAB_OBJS) $(STATIC_LIB) $(LAB_WASM) $(LAB_CODEC_WASM)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(LAB_OBJS) $(STATIC_LIB) $(LDLIBS)

$(LAB_BUILD_DIR)/tests/%.o: tools/mars_lab/tests/%.c $(LAB_TEST_HEADERS) $(TEST_SHARED_HEADERS) tools/mars_lab/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/src -c $< -o $@

# Link the same native adapters as the application, excluding its main function.
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
lab-browser-test: $(LAB_BUILD_DIR)/tests/test_lab_browser
	TMPDIR="$(abspath $(LAB_BUILD_DIR))" MARS_LAB_BROWSER="$(FIREFOX)" $(LAB_BUILD_DIR)/tests/test_lab_browser

clean-lab:
	rm -rf tools/mars_lab/build

endif
