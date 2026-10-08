# One Lab makefile: standalone commands delegate to the root build; when included
# by the root it supplies the source inventory and native compilation/linking rules.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
# Lab-local entry point; the root graph owns compiler/dependency configuration.
# Keeping one graph avoids duplicate library builds and divergent link flags.
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0

.PHONY: all test browser-test memtest run clean help
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
LAB_WORKERS := mars_lab equation_lab diffequation_lab matrix_lab integrator_lab datetime_lab almanac_lab almanac_event_lab holiday_lab ophelia
LAB_WORKER_BINS := $(addprefix $(BUILD_DIR)/scratch/,$(LAB_WORKERS))

# Native Lab build rules. Included by the root Makefile after library variables.
# The local Makefile delegates to this same graph, reusing MARS dependency checks.
$(LAB_BUILD_DIR)/%.o: tools/mars_lab/src/%.c tools/mars_lab/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/src \
	    -DMARS_LAB_ROOT_DIR='"$(CURDIR)"' \
	    -DMARS_LAB_ASSET_PATH='"$(CURDIR)/tools/mars_lab/assets/index.html"' \
	    -DMARS_LAB_WORKER_DIR='"$(abspath $(BUILD_DIR)/scratch)"' -c $< -o $@

$(LAB_SERVER): $(LAB_OBJS) $(STATIC_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(LAB_OBJS) $(STATIC_LIB) $(LDLIBS)

$(LAB_BUILD_DIR)/tests/%.o: tools/mars_lab/tests/%.c $(LAB_TEST_HEADERS) $(TEST_SHARED_HEADERS) tools/mars_lab/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/src -c $< -o $@

# Link the same native adapters as the application, excluding its main function.
$(LAB_TEST_BIN): $(LAB_TEST_OBJS) $(LAB_SUPPORT_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LAB_SERVER) $(LAB_WORKER_BINS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(LAB_TEST_OBJS) $(LAB_SUPPORT_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDLIBS)

.PHONY: native-lab lab-build clean-lab
native-lab: $(LAB_SERVER) $(LAB_WORKER_BINS)

$(LAB_BUILD_DIR)/tests/test_lab_browser: tools/mars_lab/tests/browser/test_lab_browser.c $(LAB_TEST_HEADERS) $(LAB_BUILD_DIR)/tests/test_lab_support.o $(TEST_COMMON_HELPER_OBJS) $(LAB_SUPPORT_OBJS) $(STATIC_LIB) $(LAB_WORKER_BINS)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/mars_lab/include -Itools/mars_lab/tests -o $@ $< \
	    $(LAB_BUILD_DIR)/tests/test_lab_support.o $(TEST_COMMON_HELPER_OBJS) $(LAB_SUPPORT_OBJS) $(STATIC_LIB) $(LDFLAGS) $(LDLIBS)

.PHONY: lab-browser-test
FIREFOX ?= firefox
lab-browser-test: $(LAB_BUILD_DIR)/tests/test_lab_browser
	TMPDIR="$(abspath $(LAB_BUILD_DIR))" MARS_LAB_BROWSER="$(FIREFOX)" $(LAB_BUILD_DIR)/tests/test_lab_browser

# Complete shared prerequisites in this graph before entering the local build.
# This also prevents simultaneous root/sub-make writes when callers use make -j.
lab-build: $(LAB_SERVER) $(LAB_WORKER_BINS)
	+$(MAKE) --no-print-directory -C tools/mars_lab DEBUG=$(DEBUG) all

clean-lab:
	rm -rf tools/mars_lab/build

endif
