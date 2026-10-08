# One application makefile: local entry points delegate to the root graph.
# The root includes this file after defining BUILD_DIR, STATIC_LIB and test helpers.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0

.PHONY: all test run clean help
all:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" DEBUG=$(DEBUG) native-checks

test:
	+$(MAKE) --no-print-directory -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) test_mars_checks

run: all
	"$(MARS_ROOT)/tools/mars_checks/build/$(if $(filter 1,$(DEBUG)),debug,release)/mars_checks" $(ARGS)

clean:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" clean-checks

help:
	@echo "MARS checks: make [DEBUG=1] | make test | make run ARGS='compliance --allow-untracked'"

else

CHECKS_CONFIG := $(notdir $(BUILD_DIR))
CHECKS_BUILD_DIR := tools/mars_checks/build/$(CHECKS_CONFIG)
CHECKS_SRCS := $(sort $(wildcard tools/mars_checks/src/*/*.c))
CHECKS_OBJS := $(patsubst tools/mars_checks/src/%.c,$(CHECKS_BUILD_DIR)/%.o,$(CHECKS_SRCS))
CHECKS_SUPPORT_OBJS := $(filter-out $(CHECKS_BUILD_DIR)/app/checks_main.o,$(CHECKS_OBJS))
CHECKS_BIN := $(CHECKS_BUILD_DIR)/mars_checks
CHECKS_TEST_SRCS := $(sort $(wildcard tools/mars_checks/tests/*.c))
CHECKS_TEST_OBJS := $(patsubst tools/mars_checks/tests/%.c,$(CHECKS_BUILD_DIR)/tests/%.o,$(CHECKS_TEST_SRCS))
CHECKS_TEST_BIN := $(CHECKS_BUILD_DIR)/tests/test_mars_checks
CHECKS_TEST_FIXTURE_BIN := $(CHECKS_BUILD_DIR)/tests/fixtures/checks_process_fixture
CHECKS_HEADERS := $(wildcard tools/mars_checks/include/*.h tools/mars_checks/tests/*.h)

$(CHECKS_BUILD_DIR)/%.o: tools/mars_checks/src/%.c $(CHECKS_HEADERS) tools/mars_checks/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_checks/include \
	    -DMARS_CHECKS_ROOT_DIR='"$(CURDIR)"' -c $< -o $@

$(CHECKS_BIN): $(CHECKS_OBJS) $(STATIC_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(CHECKS_OBJS) $(STATIC_LIB) $(LDLIBS)

$(CHECKS_BUILD_DIR)/tests/%.o: tools/mars_checks/tests/%.c $(CHECKS_HEADERS) $(TEST_SHARED_HEADERS) tools/mars_checks/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_checks/include -Itools/mars_checks/tests \
	    -DMARS_CHECKS_ROOT_DIR='"$(CURDIR)"' \
	    -DMARS_CHECKS_BENCHMARK_PATH='"$(abspath $(BUILD_DIR)/bench/expression/bench_expr_parse)"' \
	    -DMARS_CHECKS_PROCESS_FIXTURE_PATH='"$(abspath $(CHECKS_TEST_FIXTURE_BIN))"' -c $< -o $@

$(CHECKS_TEST_FIXTURE_BIN): tools/mars_checks/tests/fixtures/checks_process_fixture.c tools/mars_checks/Makefile Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(CHECKS_TEST_BIN): $(CHECKS_TEST_OBJS) $(CHECKS_SUPPORT_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(CHECKS_TEST_FIXTURE_BIN)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(CHECKS_TEST_OBJS) $(CHECKS_SUPPORT_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDLIBS)

.PHONY: native-checks clean-checks
native-checks: $(CHECKS_BIN)

clean-checks:
	rm -rf tools/mars_checks/build/release tools/mars_checks/build/debug

# Crucially, neither the archive nor these objects depend on all, release or
# check-public-distribution: that check consumes CHECKS_BIN, which consumes the archive.
-include $(wildcard $(CHECKS_BUILD_DIR)/*/.deps/*.d $(CHECKS_BUILD_DIR)/tests/.deps/*.d)

endif
