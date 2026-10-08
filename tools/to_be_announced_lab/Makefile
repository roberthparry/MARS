# Standalone entry point and root-included native build rules.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0
.PHONY: all run test clean
all:
	+$(MAKE) -C "$(MARS_ROOT)" DEBUG=$(DEBUG) native-tba-lab
run: all
	+$(MAKE) -C "$(MARS_ROOT)" DEBUG=$(DEBUG) to-be-announced-lab ARGS="$(ARGS)"
test:
	+$(MAKE) -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) test-tba-native
clean:
	+$(MAKE) -C "$(MARS_ROOT)" clean-tba-lab
else
TBA_BUILD_DIR := tools/to_be_announced_lab/build/$(notdir $(BUILD_DIR))
TBA_SRCS := $(sort $(wildcard tools/to_be_announced_lab/src/*/*.c))
TBA_OBJS := $(patsubst tools/to_be_announced_lab/src/%.c,$(TBA_BUILD_DIR)/%.o,$(TBA_SRCS))
TBA_SERVER := $(TBA_BUILD_DIR)/to_be_announced_lab
TBA_TEST_BIN := $(TBA_BUILD_DIR)/tests/test_tba_native
TBA_SHARED_OBJS := $(LAB_BUILD_DIR)/process/lab_process.o $(LAB_BUILD_DIR)/mobile/lab_mobile.o $(LAB_BUILD_DIR)/mobile/lab_mobile_qr.o
$(TBA_BUILD_DIR)/%.o: tools/to_be_announced_lab/src/%.c tools/to_be_announced_lab/Makefile Makefile | check-deps
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include -Itools/mars_lab/include \
	    -DTBA_ROOT='"$(CURDIR)"' -DTBA_WORKER='"$(abspath $(BUILD_DIR)/scratch/to-be-announced_lab)"' -c $< -o $@
$(TBA_SERVER): $(TBA_OBJS) $(TBA_SHARED_OBJS) $(STATIC_LIB)
	$(CC) $(LDFLAGS) -o $@ $(TBA_OBJS) $(TBA_SHARED_OBJS) $(STATIC_LIB) $(LDLIBS)
.PHONY: native-tba-lab clean-tba-lab
native-tba-lab: $(TBA_SERVER) $(BUILD_DIR)/scratch/to-be-announced_lab
clean-tba-lab:
	rm -rf tools/to_be_announced_lab/build
$(TBA_BUILD_DIR)/tests/test_tba_native: tools/to_be_announced_lab/tests/test_tba_native.c $(TBA_OBJS) $(TBA_SHARED_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include -Itools/mars_lab/include -o $@ $< \
	    $(filter-out $(TBA_BUILD_DIR)/app/tba_main.o,$(TBA_OBJS)) $(TBA_SHARED_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDFLAGS) $(LDLIBS)
.PHONY: test-tba-native
test-tba-native: $(TBA_BUILD_DIR)/tests/test_tba_native
	$<
-include $(wildcard $(TBA_BUILD_DIR)/*/.deps/*.d)
endif
