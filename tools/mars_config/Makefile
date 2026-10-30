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
CONFIG_OBJS += $(APP_RESOURCE_OBJ)
CONFIG_SUPPORT_OBJS := $(filter-out $(CONFIG_BUILD_DIR)/app/cfg_main.o,$(CONFIG_OBJS))
CONFIG_PROGRAM := $(CONFIG_BUILD_DIR)/mars_config
CONFIG_TEST_SRCS := $(sort $(wildcard tools/mars_config/tests/*.c))
# PNG fixture generation requires zlib when its source is included in the test build.
CONFIG_TEST_PNG_LIBS := $(if $(filter %/test_cfg_texture_fixture.c,$(CONFIG_TEST_SRCS)),-lz)
CONFIG_TEST_OBJS := $(patsubst tools/mars_config/tests/%.c,$(CONFIG_BUILD_DIR)/tests/%.o,$(CONFIG_TEST_SRCS))
CONFIG_TEST_BIN := $(CONFIG_BUILD_DIR)/tests/test_mars_config

$(CONFIG_BUILD_DIR)/country_compose: tools/mars_config/country_compose.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(CONFIG_BUILD_DIR)/country_islands: tools/mars_config/country_islands.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(CONFIG_BUILD_DIR)/region_extract: tools/mars_config/region_extract.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(CONFIG_BUILD_DIR)/catalogue_towns: tools/mars_config/catalogue_towns.c $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(CONFIG_BUILD_DIR)/river_select: tools/mars_config/river_select.c $(CONFIG_SUPPORT_OBJS) $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/mars_config/include -Itools/mars_config/src/database $(LDFLAGS) \
	    -o $@ $< $(CONFIG_SUPPORT_OBJS) $(STATIC_LIB) $(LDLIBS)

# Optional offline preparation only; GEOS is not linked into mars_config or libmars.
$(CONFIG_BUILD_DIR)/river_water: tools/mars_config/river_water.c $(CONFIG_BUILD_DIR)/database/cfg_database_river_width.o $(STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/mars_config/src/database $(GEOS_CFLAGS) $(LDFLAGS) -o $@ $< \
	    $(CONFIG_BUILD_DIR)/database/cfg_database_river_width.o $(STATIC_LIB) $(LDLIBS) -l:libgeos_c.so.1

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
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_config/include -c $< -o $@

$(CONFIG_PROGRAM): $(CONFIG_OBJS) $(STATIC_LIB)
	$(CC) $(LDFLAGS) -o $@ $(CONFIG_OBJS) $(STATIC_LIB) $(LDLIBS)

# Private module headers are available only to their owning module's regression tests.
$(CONFIG_BUILD_DIR)/tests/test_cfg_calendar.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/calendar
$(CONFIG_BUILD_DIR)/tests/test_cfg_database.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/database
$(CONFIG_BUILD_DIR)/tests/test_cfg_rivers.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/database
$(CONFIG_BUILD_DIR)/tests/test_cfg_textures.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/database
$(CONFIG_BUILD_DIR)/tests/test_cfg_rendering.o: CONFIG_TEST_INTERNAL_INCLUDES := -Itools/mars_config/src/database

$(CONFIG_BUILD_DIR)/tests/%.o: tools/mars_config/tests/%.c tools/mars_config/tests/test_cfg_support.h tools/mars_config/Makefile Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/mars_config/include -Itools/mars_lab/include \
	    $(CONFIG_TEST_INTERNAL_INCLUDES) \
	    -DMARS_CONFIG_PROGRAM='"$(abspath $(CONFIG_PROGRAM))"' -DMARS_CONFIG_ROOT_DIR='"$(CURDIR)"' -c $< -o $@

$(CONFIG_TEST_BIN): $(CONFIG_TEST_OBJS) $(CONFIG_SUPPORT_OBJS) $(LAB_BUILD_DIR)/process/lab_process.o $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(CONFIG_PROGRAM)
	$(CC) $(LDFLAGS) -o $@ $(CONFIG_TEST_OBJS) $(CONFIG_SUPPORT_OBJS) \
	    $(LAB_BUILD_DIR)/process/lab_process.o $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDLIBS) $(CONFIG_TEST_PNG_LIBS)

.PHONY: native-config clean-config
native-config: $(CONFIG_PROGRAM) $(CONFIG_BUILD_DIR)/mars-config.db

MARS_ZONEINFO_DIR ?= $(firstword $(wildcard $(TZDIR) /usr/share/zoneinfo /usr/share/lib/zoneinfo /usr/lib/zoneinfo))
$(CONFIG_BUILD_DIR)/mars-config.db: $(CONFIG_PROGRAM) Makefile tools/mars_config/Makefile
	@"$(CONFIG_PROGRAM)" paths seed "$@" \
	    globe_sources "$(CURDIR)/packaging/globe-db" \
	    globe_textures "$(CURDIR)/packaging/globe-db/textures" \
	    jurisdiction_sources "$(CURDIR)/packaging/jurisdiction-db" \
	    almanac_sources "$(CURDIR)/packaging/almanac-db" \
	    lab_assets "$(CURDIR)/tools/mars_lab/assets" \
	    lab_scripts "$(CURDIR)/tools/mars_lab/build/typescript" \
	    lab_wasm "$(abspath $(LAB_BUILD_DIR))/wasm" \
	    lab_icons "$(CURDIR)/packaging/linux" \
	    lab_worker "$(abspath $(LAB_SERVER))" \
	    locale_sources "$(CURDIR)/tools/mars_config/data/locales" zoneinfo "$(MARS_ZONEINFO_DIR)" \
	    temporary "$(if $(TMPDIR),$(TMPDIR),/tmp)" system_documentation "$(if $(MARS_SYSTEM_DOC_DIR),$(MARS_SYSTEM_DOC_DIR),/usr/share/doc)" \
	    private_assets "$(CURDIR)/tools/to_be_announced_lab/assets" \
	    private_scripts "$(CURDIR)/tools/to_be_announced_lab/build/typescript" private_wasm "$(abspath $(TBA_BUILD_DIR))/wasm"

$(LAB_BUILD_DIR)/mars-config.db: $(CONFIG_BUILD_DIR)/mars-config.db
	@mkdir -p $(@D)
	cp "$<" "$@"

$(LAB_SERVER): | $(LAB_BUILD_DIR)/mars-config.db
$(CONFIG_TEST_BIN): | $(CONFIG_BUILD_DIR)/mars-config.db

$(TBA_BUILD_DIR)/mars-config.db: $(CONFIG_BUILD_DIR)/mars-config.db
	@mkdir -p $(@D)
	cp "$<" "$@"

$(TBA_SERVER): | $(TBA_BUILD_DIR)/mars-config.db
clean-config:
	rm -rf tools/mars_config/build

-include $(wildcard $(CONFIG_BUILD_DIR)/*/.deps/*.d)
endif
