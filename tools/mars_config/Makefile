# Standalone entry points delegate to the repository's single library build graph.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0
.PHONY: all test clean
all:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" DEBUG=$(DEBUG) native-config
test:
	+$(MAKE) --no-print-directory -j1 -C "$(MARS_ROOT)" DEBUG=$(DEBUG) test_mars_config
clean:
	+$(MAKE) --no-print-directory -C "$(MARS_ROOT)" clean-config
else
CONFIG_BUILD_DIR := tools/mars_config/build/$(notdir $(BUILD_DIR))
CONFIG_SRCS := $(sort $(wildcard tools/mars_config/src/*/*.c))
CONFIG_OBJS := $(patsubst tools/mars_config/src/%.c,$(CONFIG_BUILD_DIR)/%.o,$(CONFIG_SRCS))
CONFIG_SUPPORT_OBJS := $(filter-out $(CONFIG_BUILD_DIR)/app/cfg_main.o,$(CONFIG_OBJS))
CONFIG_PROGRAM := $(CONFIG_BUILD_DIR)/mars_config
CONFIG_TEST_SRCS := $(sort $(wildcard tools/mars_config/tests/*.c))
CONFIG_TEST_OBJS := $(patsubst tools/mars_config/tests/%.c,$(CONFIG_BUILD_DIR)/tests/%.o,$(CONFIG_TEST_SRCS))
CONFIG_TEST_BIN := $(CONFIG_BUILD_DIR)/tests/test_mars_config

$(CONFIG_BUILD_DIR)/country_compose: tools/mars_config/country_compose.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(CONFIG_BUILD_DIR)/catalogue_towns: tools/mars_config/catalogue_towns.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

# Optional offline preparation only; GEOS is not linked into mars_config or libmars.
$(CONFIG_BUILD_DIR)/coastline_stage: tools/mars_config/coastline_stage.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

$(CONFIG_BUILD_DIR)/coastline_review: tools/mars_config/coastline_review.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

$(CONFIG_BUILD_DIR)/coastline_repair: tools/mars_config/coastline_repair.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

$(CONFIG_BUILD_DIR)/coastline_export: tools/mars_config/coastline_export.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(CONFIG_BUILD_DIR)/coastline_trial: tools/mars_config/coastline_trial.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(CONFIG_BUILD_DIR)/coastline_compose: tools/mars_config/coastline_compose.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

$(CONFIG_BUILD_DIR)/coastline_reconcile: tools/mars_config/coastline_reconcile.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

$(CONFIG_BUILD_DIR)/coastline_polar: tools/mars_config/coastline_polar.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

$(CONFIG_BUILD_DIR)/coastline_water: tools/mars_config/coastline_water.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

$(CONFIG_BUILD_DIR)/%.o: tools/mars_config/src/%.c tools/mars_config/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_config/include \
	    -DMARS_CONFIG_ROOT_DIR='"$(CURDIR)"' -c $< -o $@

$(CONFIG_PROGRAM): $(CONFIG_OBJS) $(STATIC_LIB)
	$(CC) $(LDFLAGS) -o $@ $(CONFIG_OBJS) $(STATIC_LIB) $(LDLIBS)

# Private module headers are available only to their owning module's regression tests.
$(CONFIG_BUILD_DIR)/tests/test_cfg_calendar.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/calendar
$(CONFIG_BUILD_DIR)/tests/test_cfg_database.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/database
$(CONFIG_BUILD_DIR)/tests/test_cfg_rivers.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/database
$(CONFIG_BUILD_DIR)/tests/test_cfg_rendering.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/database

$(CONFIG_BUILD_DIR)/tests/%.o: tools/mars_config/tests/%.c tools/mars_config/tests/test_cfg_support.h tools/mars_config/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_config/include -Itools/mars_lab/include \
	    $(CONFIG_TEST_INTERNAL_INCLUDES) \
	    -DMARS_CONFIG_PROGRAM='"$(abspath $(CONFIG_PROGRAM))"' -DMARS_CONFIG_ROOT_DIR='"$(CURDIR)"' -c $< -o $@

$(CONFIG_TEST_BIN): $(CONFIG_TEST_OBJS) $(CONFIG_SUPPORT_OBJS) $(LAB_BUILD_DIR)/process/lab_process.o $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(CONFIG_PROGRAM)
	$(CC) $(LDFLAGS) -o $@ $(CONFIG_TEST_OBJS) $(CONFIG_SUPPORT_OBJS) \
	    $(LAB_BUILD_DIR)/process/lab_process.o $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDLIBS)

.PHONY: native-config clean-config
native-config: $(CONFIG_PROGRAM)
clean-config:
	rm -rf tools/mars_config/build

-include $(wildcard $(CONFIG_BUILD_DIR)/*/.deps/*.d)
endif
