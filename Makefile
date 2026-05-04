# ------------------------------------------------------------
# Build mode
# ------------------------------------------------------------
DEBUG ?= 0
RELEASE_OPT_FLAGS ?= -O2 -flto -march=native -mtune=native
QFLOAT_RELEASE_OPT_FLAGS ?= -O2 -flto

ifeq ($(DEBUG),1)
    BUILD_DIR      := build/debug
    TEST_BUILD_DIR := tests/build/debug
    CFLAGS         := -Wall -Wextra -Werror -g -O0 -fPIC -DDEBUG
    RELEASE_BUILD  := 0
else
    BUILD_DIR      := build/release
    TEST_BUILD_DIR := tests/build/release
    CFLAGS         := -Wall -Wextra -Werror $(RELEASE_OPT_FLAGS) -fPIC
    RELEASE_BUILD  := 1
endif

CFLAGS += -D_GNU_SOURCE
QFLOAT_RELEASE_CFLAGS := -Wall -Wextra -Werror $(QFLOAT_RELEASE_OPT_FLAGS) -fPIC -D_GNU_SOURCE

CC := gcc
AR := ar rcs

INCLUDES := -I. -Iinclude -Iinclude/internal -Itests -Itests/include

# ------------------------------------------------------------
# Optional libunistring
# ------------------------------------------------------------
ENABLE_UNISTRING ?= 1

ifeq ($(ENABLE_UNISTRING),1)
    UNISTRING_CFLAGS := $(shell pkg-config --cflags libunistring 2>/dev/null)
    UNISTRING_LIBS   := $(shell pkg-config --libs   libunistring 2>/dev/null)

    ifneq ($(UNISTRING_CFLAGS)$(UNISTRING_LIBS),)
        CFLAGS  += $(UNISTRING_CFLAGS) -DHAVE_UNISTRING
        LDLIBS  += $(UNISTRING_LIBS)
    else
        CFLAGS  += -DHAVE_UNISTRING
        LDLIBS  += -lunistring
    endif
endif

LDLIBS += -lm
LDLIBS += -lpthread

# ------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------
SRCS    := $(shell find src -name '*.c' | sort)
OBJS    := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

TEST_ALL_SRCS     := $(shell find tests -name 'test_*.c' | sort)
TEST_SRCS         := $(shell find tests -name 'test_*.c' | while read -r f; do d=$$(basename "$$(dirname "$$f")"); b=$$(basename "$$f"); if [ "$$b" = "test_$$d.c" ]; then printf '%s\n' "$$f"; fi; done | sort)
TEST_HELPER_SRCS  := $(filter-out $(TEST_SRCS),$(TEST_ALL_SRCS))
TEST_OBJS         := $(TEST_SRCS:tests/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_HELPER_OBJS  := $(TEST_HELPER_SRCS:tests/%.c=$(TEST_BUILD_DIR)/%.o)
BENCH_SRCS        := $(shell find bench -name 'bench_*.c' 2>/dev/null | sort)
BENCH_OBJS        := $(BENCH_SRCS:bench/%.c=$(BUILD_DIR)/bench/%.o)
BENCH_BINS        := $(patsubst bench/%.c,$(BUILD_DIR)/bench/%,$(BENCH_SRCS))
QFLOAT_TOOL_BIN   := $(BUILD_DIR)/tools/qfloat/gen_qfloat_tables

HEADERS      := $(wildcard include/*.h)

STATIC_LIB := $(BUILD_DIR)/libmars.a
SHARED_LIB := $(BUILD_DIR)/libmars.so

TEST_BINS  := $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SRCS))

.SECONDARY: $(TEST_OBJS) $(TEST_HELPER_OBJS)

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------
.PHONY: all clean test memtest debug release help

all: $(STATIC_LIB) $(SHARED_LIB) $(TEST_BINS) $(BENCH_BINS)

debug:
	$(MAKE) DEBUG=1 all

release:
	$(MAKE) DEBUG=0 all

# ------------------------------------------------------------
# Dependency tracking
# ------------------------------------------------------------
DEPFLAGS = -MT $@ -MMD -MP -MF $(dir $@).deps/$(subst /,_,$*).d
DEPS     := $(shell find build tests/build -name '*.d' 2>/dev/null)
-include $(DEPS)

# ------------------------------------------------------------
# Object build rules
# ------------------------------------------------------------
ifeq ($(DEBUG),0)
QFLOAT_RELEASE_CFLAGS += $(UNISTRING_CFLAGS)

ifeq ($(ENABLE_UNISTRING),1)
QFLOAT_RELEASE_CFLAGS += -DHAVE_UNISTRING
endif
endif

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: tests/%.c
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/bench/%.o: bench/%.c
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# ------------------------------------------------------------
# Libraries
# ------------------------------------------------------------
$(STATIC_LIB): Makefile $(OBJS)
	@mkdir -p $(dir $@)
	# Rebuild the archive from scratch so renamed object files cannot linger.
	rm -f $@
	$(AR) $@ $(OBJS)

$(SHARED_LIB): Makefile $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $(OBJS) $(LDLIBS)

# ------------------------------------------------------------
# Test binaries
# ------------------------------------------------------------
$(TEST_BUILD_DIR)/%: $(TEST_BUILD_DIR)/%.o $(STATIC_LIB) $(SHARED_LIB) $(TEST_HELPER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $< $(filter $(TEST_BUILD_DIR)/$(dir $*)%.o,$(TEST_HELPER_OBJS)) $(STATIC_LIB) $(LDLIBS)

$(BUILD_DIR)/bench/%: $(BUILD_DIR)/bench/%.o $(STATIC_LIB) $(SHARED_LIB)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(QFLOAT_TOOL_BIN): tools/qfloat/gen_qfloat_tables.c $(STATIC_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(STATIC_LIB) $(LDLIBS)

# ------------------------------------------------------------
# Test targets
# ------------------------------------------------------------
VALGRIND := valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes

test: $(TEST_BINS)
	@rc=0; for t in $(TEST_BINS); do \
	    printf "  %-40s" "$$t ..."; \
	    if $$t > /dev/null 2>&1; then \
	        echo "PASS"; \
	    else \
	        echo "FAIL"; rc=1; \
	    fi; \
	done; exit $$rc

memtest: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
	    echo "=== $$t ==="; \
	    $(VALGRIND) $$t; \
	done

define TEST_ALIAS_RULES
.PHONY: $(1) mem$(1)
$(1): $(2)
	@$(2)

mem$(1): $(2)
	$(VALGRIND) $(2)
endef

QFLOAT_TEST_BIN := $(TEST_BUILD_DIR)/qfloat/test_qfloat
TEST_ALIAS_BINS := $(filter-out $(QFLOAT_TEST_BIN),$(TEST_BINS))

ifeq ($(RELEASE_BUILD),1)
.PHONY: test_qfloat memtest_qfloat
test_qfloat:
	@$(MAKE) DEBUG=0 RELEASE_OPT_FLAGS="$(QFLOAT_RELEASE_OPT_FLAGS)" $(QFLOAT_TEST_BIN)
	@$(QFLOAT_TEST_BIN)

memtest_qfloat:
	@$(MAKE) DEBUG=0 RELEASE_OPT_FLAGS="$(QFLOAT_RELEASE_OPT_FLAGS)" $(QFLOAT_TEST_BIN)
	$(VALGRIND) $(QFLOAT_TEST_BIN)
else
$(eval $(call TEST_ALIAS_RULES,test_qfloat,$(QFLOAT_TEST_BIN)))
endif

$(foreach bin,$(TEST_ALIAS_BINS),$(eval $(call TEST_ALIAS_RULES,$(notdir $(bin)),$(bin))))

define BENCH_ALIAS_RULES
.PHONY: $(1)
$(1): $(2)
	@$(2)
endef

QFLOAT_GAMMA_BENCH_BIN := $(BUILD_DIR)/bench/qfloat/bench_qfloat_gamma_maths
BENCH_ALIAS_BINS := $(filter-out $(QFLOAT_GAMMA_BENCH_BIN),$(BENCH_BINS))

ifeq ($(RELEASE_BUILD),1)
.PHONY: bench_qfloat_gamma_maths
bench_qfloat_gamma_maths:
	@$(MAKE) DEBUG=0 RELEASE_OPT_FLAGS="$(QFLOAT_RELEASE_OPT_FLAGS)" $(QFLOAT_GAMMA_BENCH_BIN)
	@$(QFLOAT_GAMMA_BENCH_BIN)
else
$(eval $(call BENCH_ALIAS_RULES,bench_qfloat_gamma_maths,$(QFLOAT_GAMMA_BENCH_BIN)))
endif

$(foreach bin,$(BENCH_ALIAS_BINS),$(eval $(call BENCH_ALIAS_RULES,$(notdir $(bin)),$(bin))))

.PHONY: gen_qfloat_tables gen_qfloat_constants
ifeq ($(RELEASE_BUILD),1)
gen_qfloat_tables:
	@$(MAKE) DEBUG=0 RELEASE_OPT_FLAGS="$(QFLOAT_RELEASE_OPT_FLAGS)" $(QFLOAT_TOOL_BIN)
	@$(QFLOAT_TOOL_BIN) --exp-coef

gen_qfloat_constants:
	@$(MAKE) DEBUG=0 RELEASE_OPT_FLAGS="$(QFLOAT_RELEASE_OPT_FLAGS)" $(QFLOAT_TOOL_BIN)
	@$(QFLOAT_TOOL_BIN)
else
gen_qfloat_tables: $(QFLOAT_TOOL_BIN)
	@$(QFLOAT_TOOL_BIN) --exp-coef

gen_qfloat_constants: $(QFLOAT_TOOL_BIN)
	@$(QFLOAT_TOOL_BIN)
endif

# ------------------------------------------------------------
# Help
# ------------------------------------------------------------
help:
	@echo "Targets:"
	@echo "  make debug                  Build debug binaries and tests"
	@echo "  make release                Build release binaries and tests"
	@echo "  make test                   Run all tests (release)"
	@echo "  make memtest                Run all tests under valgrind (release)"
	@echo "  make test_<name>            Build and run a single test (e.g. make test_dval) (release)"
	@echo "  make memtest_<name>         Build and run a single test under valgrind (release)"
	@echo "  make DEBUG=1 test           Run all tests (debug)"
	@echo "  make DEBUG=1 memtest        Run all tests under valgrind (debug)"
	@echo "  make DEBUG=1 test_<name>    Build and run a single test (e.g. make test_dval) (debug)"
	@echo "  make DEBUG=1 memtest_<name> Build and run a single test under valgrind (debug)"
	@echo "  make bench_<name>           Build and run a benchmark (e.g. make bench_integrator)"
	@echo "  make clean                  Remove all build artifacts"

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------
clean:
	rm -rf build tests/build
