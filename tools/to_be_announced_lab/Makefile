# Standalone entry point and root-included native build rules.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0
.PHONY: all run test browser-test clean
all:
	+$(MAKE) -C "$(MARS_ROOT)" DEBUG=$(DEBUG) native-tba-lab
run: all
	+$(MAKE) -C "$(MARS_ROOT)" DEBUG=$(DEBUG) to-be-announced-lab ARGS="$(ARGS)"
test:
	+$(MAKE) -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) test-tba-native
browser-test:
	+$(MAKE) -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) test-tba-browser
clean:
	+$(MAKE) -C "$(MARS_ROOT)" clean-tba-lab
else
TBA_BUILD_DIR := tools/to_be_announced_lab/build/$(notdir $(BUILD_DIR))
TBA_SRCS := $(sort $(wildcard tools/to_be_announced_lab/src/*/*.c))
TBA_OBJS := $(patsubst tools/to_be_announced_lab/src/%.c,$(TBA_BUILD_DIR)/%.o,$(TBA_SRCS))
TBA_SERVER := $(TBA_BUILD_DIR)/to_be_announced_lab
TBA_TEST_BIN := $(TBA_BUILD_DIR)/tests/test_tba_native
TBA_TEST_SUPPORT := tools/to_be_announced_lab/tests/test_tba_support.c
TBA_TEST_HEADERS := tools/to_be_announced_lab/tests/test_tba_support.h $(wildcard tools/to_be_announced_lab/wasm/include/*.h)
TBA_SHARED_OBJS := $(LAB_BUILD_DIR)/process/lab_process.o $(LAB_BUILD_DIR)/mobile/lab_mobile.o $(LAB_BUILD_DIR)/mobile/lab_mobile_qr.o $(LAB_BUILD_DIR)/wire/lab_wire.o
TBA_WASM := $(TBA_BUILD_DIR)/wasm/tba_browser.wasm
TBA_WASM_SRCS := $(sort $(wildcard tools/to_be_announced_lab/wasm/src/*.c))
TBA_WASM_POLICY_SRCS := $(addprefix tools/to_be_announced_lab/wasm/src/,tba_chart.c tba_model.c tba_period.c)
TBA_WASM_OBJS := $(patsubst tools/to_be_announced_lab/wasm/src/%.c,$(TBA_BUILD_DIR)/wasm/%.o,$(TBA_WASM_SRCS))
TBA_WASM_SHARED := $(LAB_BUILD_DIR)/wasm/src/lab_browser.o $(LAB_BUILD_DIR)/wasm/src/lab_memory.o
$(TBA_BUILD_DIR)/wasm/%.o: tools/to_be_announced_lab/wasm/src/%.c tools/to_be_announced_lab/Makefile | check-lab-wasm-deps
	@mkdir -p $(dir $@)
	"$(LAB_WASM_CC)" --target=wasm32-unknown-unknown -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	    -nostdlib -Wmissing-prototypes -MMD -MP -MF $(@:.o=.d) -Itools/to_be_announced_lab/wasm/include -c $< -o $@
$(TBA_WASM): $(TBA_WASM_OBJS) $(TBA_WASM_SHARED) tools/to_be_announced_lab/wasm/tba_browser.imports tools/to_be_announced_lab/Makefile
	"$(LAB_WASM_LD)" --no-entry --export-all --allow-undefined-file=tools/to_be_announced_lab/wasm/tba_browser.imports \
	    --initial-memory=16777216 --max-memory=16777216 $(TBA_WASM_OBJS) $(TBA_WASM_SHARED) -o $@
$(TBA_BUILD_DIR)/server/tba_server.o: CFLAGS += -DTBA_WASM_PATH='"$(abspath $(TBA_WASM))"'
$(TBA_BUILD_DIR)/%.o: tools/to_be_announced_lab/src/%.c tools/to_be_announced_lab/Makefile Makefile | check-deps
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include -Itools/mars_lab/include \
	    -DTBA_ROOT='"$(CURDIR)"' -DTBA_SERVER_PATH='"$(abspath $(TBA_SERVER))"' -c $< -o $@
$(TBA_SERVER): $(TBA_OBJS) $(TBA_SHARED_OBJS) $(STATIC_LIB) $(TBA_WASM)
	$(CC) $(LDFLAGS) -o $@ $(TBA_OBJS) $(TBA_SHARED_OBJS) $(STATIC_LIB) $(LDLIBS)
.PHONY: native-tba-lab clean-tba-lab
native-tba-lab: $(TBA_SERVER)
clean-tba-lab:
	rm -rf tools/to_be_announced_lab/build
$(TBA_BUILD_DIR)/tests/test_tba_native: tools/to_be_announced_lab/tests/test_tba_native.c $(TBA_TEST_SUPPORT) $(TBA_TEST_HEADERS) $(TBA_OBJS) $(TBA_SHARED_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(TBA_WASM) $(TBA_WASM_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include -Itools/mars_lab/include \
	    -Itools/to_be_announced_lab/wasm/include -Itools/to_be_announced_lab/tests -o $@ $< $(TBA_WASM_POLICY_SRCS) $(TBA_TEST_SUPPORT) \
	    $(filter-out $(TBA_BUILD_DIR)/app/tba_main.o,$(TBA_OBJS)) $(TBA_SHARED_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDFLAGS) $(LDLIBS)
.PHONY: test-tba-native
test-tba-native: $(TBA_BUILD_DIR)/tests/test_tba_native $(TBA_SERVER)
	$<
$(TBA_BUILD_DIR)/tests/test_tba_browser: tools/to_be_announced_lab/tests/browser/test_tba_browser.c $(TBA_TEST_SUPPORT) $(TBA_TEST_HEADERS) $(TBA_OBJS) $(TBA_SHARED_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(TBA_WASM)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include -Itools/mars_lab/include \
	    -Itools/to_be_announced_lab/tests -o $@ $< $(TBA_TEST_SUPPORT) \
	    $(filter-out $(TBA_BUILD_DIR)/app/tba_main.o,$(TBA_OBJS)) $(TBA_SHARED_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDFLAGS) $(LDLIBS)
.PHONY: test-tba-browser
test-tba-browser: $(TBA_BUILD_DIR)/tests/test_tba_browser $(TBA_SERVER)
	TMPDIR="$(abspath $(TBA_BUILD_DIR))" MARS_LAB_BROWSER="$(FIREFOX)" $<
-include $(wildcard $(TBA_BUILD_DIR)/*/.deps/*.d)
-include $(TBA_WASM_OBJS:.o=.d)
endif
