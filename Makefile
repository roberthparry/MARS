# ------------------------------------------------------------
# Build mode
# ------------------------------------------------------------
DEBUG ?= 0
RELEASE_OPT_FLAGS ?= -O2

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

CC := gcc
AR := ar rcs
INSTALL ?= install

PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

MARS_LAB_INSTALL_PREFIX ?= $(HOME)/.local
MARS_LAB_BINDIR ?= $(MARS_LAB_INSTALL_PREFIX)/bin
MARS_LAB_APPDIR ?= $(MARS_LAB_INSTALL_PREFIX)/share/applications
MARS_LAB_ICONDIR ?= $(MARS_LAB_INSTALL_PREFIX)/share/icons/hicolor/scalable/apps
MARS_LAB_LAUNCHER ?= $(MARS_LAB_BINDIR)/mars-lab
MARS_LAB_DESKTOP ?= $(MARS_LAB_APPDIR)/mars-lab.desktop
MARS_LAB_ICON ?= $(MARS_LAB_ICONDIR)/mars-lab.svg
MARS_LAB_ICON_CONCEPTS := $(wildcard packaging/linux/icon-concepts/*.svg)
TO_BE_ANNOUNCED_LAB_LAUNCHER ?= $(MARS_LAB_BINDIR)/to-be-announced-lab
TO_BE_ANNOUNCED_LAB_DESKTOP ?= $(MARS_LAB_APPDIR)/to-be-announced-lab.desktop
TO_BE_ANNOUNCED_LAB_ICON ?= $(MARS_LAB_ICONDIR)/to-be-announced-lab.svg

INCLUDES := -I. -Iinclude -Isrc -Itests -Itests/include

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
LDLIBS += -lmpfr -lmpc -lgmp

# ------------------------------------------------------------
# SQLCipher-backed SQLite storage
# ------------------------------------------------------------
SQLCIPHER_CFLAGS := $(shell pkg-config --cflags sqlcipher 2>/dev/null)
SQLCIPHER_LIBS   := $(shell pkg-config --libs   sqlcipher 2>/dev/null)

CFLAGS += $(SQLCIPHER_CFLAGS) -DSQLITE_HAS_CODEC
ifneq ($(SQLCIPHER_LIBS),)
    LDLIBS += $(SQLCIPHER_LIBS)
else
    LDLIBS += -lsqlcipher
endif

# ------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------
SRCS    := $(shell find src -name '*.c' | sort)
OBJS    := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

TEST_ALL_SRCS     := $(shell find tests -name 'test_*.c' ! -path 'tests/test_config/*' | sort)
TEST_SRCS         := $(shell find tests -name 'test_*.c' ! -path 'tests/test_config/*' | while read -r f; do d=$$(basename "$$(dirname "$$f")"); b=$$(basename "$$f"); if [ "$$b" = "test_$$d.c" ] || [ "$$b" = "$$d.c" ]; then printf '%s\n' "$$f"; fi; done | sort)
TEST_HELPER_SRCS  := $(filter-out $(TEST_SRCS),$(TEST_ALL_SRCS))
TEST_COMMON_SRCS  := $(shell find tests/test_config -name '*.c' 2>/dev/null | sort)
TEST_SHARED_HEADERS := $(wildcard tests/include/*.h)
TEST_SUITE_HEADERS  := $(shell find tests -mindepth 2 -maxdepth 2 -name 'test_*.h' | sort)
TEST_OBJS         := $(TEST_SRCS:tests/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_HELPER_OBJS  := $(TEST_HELPER_SRCS:tests/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_COMMON_HELPER_OBJS := $(TEST_COMMON_SRCS:tests/%.c=$(TEST_BUILD_DIR)/%.o)
BENCH_SRCS        := $(shell find bench -name 'bench_*.c' 2>/dev/null | sort)
BENCH_OBJS        := $(BENCH_SRCS:bench/%.c=$(BUILD_DIR)/bench/%.o)
BENCH_BINS        := $(patsubst bench/%.c,$(BUILD_DIR)/bench/%,$(BENCH_SRCS))
SCRATCH_SRCS      := $(shell find scratch -name '*.c' 2>/dev/null | sort)
SCRATCH_OBJS      := $(SCRATCH_SRCS:scratch/%.c=$(BUILD_DIR)/scratch/%.o)
SCRATCH_BINS      := $(patsubst scratch/%.c,$(BUILD_DIR)/scratch/%,$(SCRATCH_SRCS))
QFLOAT_TOOL_BIN   := $(BUILD_DIR)/tools/qfloat/gen_qfloat_tables

HEADERS      := $(wildcard include/*.h)

STATIC_LIB := $(BUILD_DIR)/libmars.a
SHARED_LIB := $(BUILD_DIR)/libmars.so

TEST_BINS  := $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SRCS))

.SECONDEXPANSION:
.SECONDARY: $(TEST_OBJS) $(TEST_HELPER_OBJS) $(TEST_COMMON_HELPER_OBJS)

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------
.PHONY: all clean test memtest debug release check-deps check-lab-deps install uninstall mars-lab to-be-announced-lab install-mars-lab uninstall-mars-lab help

all: $(STATIC_LIB) $(SHARED_LIB) $(TEST_BINS) $(BENCH_BINS) $(SCRATCH_BINS)

debug:
	$(MAKE) DEBUG=1 all

release:
	$(MAKE) DEBUG=0 all

# ------------------------------------------------------------
# Dependency checks
# ------------------------------------------------------------
check-deps:
	@missing=0; \
	packages=""; \
	check_dep() { \
	    name="$$1"; header="$$2"; lib="$$3"; package="$$4"; body="$$5"; \
	    if ! printf '%s\n' "#include <stdint.h>" "#include <$$header>" "int main(void) { $$body; return 0; }" \
	        | $(CC) -x c - -o /tmp/mars-check-dep $$lib >/dev/null 2>&1; then \
	        echo "Missing $$name development files."; \
	        echo "  Debian/Ubuntu: sudo apt install $$package"; \
	        packages="$$packages $$package"; \
	        missing=1; \
	    fi; \
	    rm -f /tmp/mars-check-dep; \
	}; \
	check_dep "GMP" "gmp.h" "-lgmp" "libgmp-dev" "mpz_t x; mpz_init(x); mpz_clear(x)"; \
	check_dep "MPFR" "mpfr.h" "-lmpfr -lgmp" "libmpfr-dev" "mpfr_t x; mpfr_init2(x, 53); mpfr_clear(x)"; \
	check_dep "MPC" "mpc.h" "-lmpc -lmpfr -lgmp" "libmpc-dev" "mpc_t x; mpc_init2(x, 53); mpc_clear(x)"; \
	check_dep "SQLCipher" "sqlcipher/sqlite3.h" "-lsqlcipher" "libsqlcipher-dev" "sqlite3 *db = 0; sqlite3_open(\":memory:\", &db); sqlite3_close(db)"; \
	if [ "$(ENABLE_UNISTRING)" = "1" ]; then \
	    check_dep "libunistring" "unistr.h" "-lunistring" "libunistring-dev" "(void)u8_strlen((const uint8_t *)\"x\")"; \
	fi; \
	if [ "$$missing" -ne 0 ]; then \
	    echo; \
	    echo "Install the missing development package(s), then rerun make."; \
	    echo "Debian/Ubuntu command:"; \
	    echo "  sudo apt install$$packages"; \
	    exit 1; \
	fi

check-lab-deps: check-deps
	@missing=0; \
	packages=""; \
	check_tool() { \
	    name="$$1"; command="$$2"; package="$$3"; \
	    if ! command -v "$$command" >/dev/null 2>&1; then \
	        echo "Missing $$name for MARS Lab TeX rendering."; \
	        echo "  Debian/Ubuntu: sudo apt install $$package"; \
	        packages="$$packages $$package"; \
	        missing=1; \
	    fi; \
	}; \
	check_tool "LaTeX" "latex" "texlive-latex-base"; \
	check_tool "dvisvgm" "dvisvgm" "dvisvgm"; \
	if [ "$$missing" -ne 0 ]; then \
	    echo; \
	    echo "Install the missing MARS Lab runtime tool(s), then rerun make."; \
	    echo "Debian/Ubuntu command:"; \
	    echo "  sudo apt install$$packages"; \
	    exit 1; \
	fi

# ------------------------------------------------------------
# Installation
# ------------------------------------------------------------
install: check-deps $(STATIC_LIB) $(SHARED_LIB)
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)"
	$(INSTALL) -d "$(DESTDIR)$(INCLUDEDIR)/mars"
	$(INSTALL) -m 644 $(STATIC_LIB) "$(DESTDIR)$(LIBDIR)/libmars.a"
	$(INSTALL) -m 755 $(SHARED_LIB) "$(DESTDIR)$(LIBDIR)/libmars.so"
	$(INSTALL) -m 644 $(HEADERS) "$(DESTDIR)$(INCLUDEDIR)/mars"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/libmars.a"
	rm -f "$(DESTDIR)$(LIBDIR)/libmars.so"
	@for h in $(notdir $(HEADERS)); do \
	    rm -f "$(DESTDIR)$(INCLUDEDIR)/mars/$$h"; \
	done
	-rmdir "$(DESTDIR)$(INCLUDEDIR)/mars"

# ------------------------------------------------------------
# Dependency tracking
# ------------------------------------------------------------
DEPFLAGS = -MT $@ -MMD -MP -MF $(dir $@).deps/$(subst /,_,$*).d
DEPS     := $(shell find build tests/build -name '*.d' 2>/dev/null)
-include $(DEPS)

# ------------------------------------------------------------
# Object build rules
# ------------------------------------------------------------
$(BUILD_DIR)/%.o: src/%.c Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: tests/%.c Makefile $(TEST_SHARED_HEADERS) $(TEST_SUITE_HEADERS)
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/bench/%.o: bench/%.c Makefile
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/scratch/%.o: scratch/%.c Makefile
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
define TEST_BIN_RULE
$(patsubst tests/%.c,$(TEST_BUILD_DIR)/%,$(1)): \
    $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%.o,$(1)) \
    $(STATIC_LIB) $(SHARED_LIB) \
    $(TEST_COMMON_HELPER_OBJS) \
    $(filter $(TEST_BUILD_DIR)/$(dir $(patsubst tests/%,%,$(1)))%.o,$(TEST_HELPER_OBJS))
	@mkdir -p $$(dir $$@)
	$(CC) -o $$@ \
	    $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%.o,$(1)) \
	    $(TEST_COMMON_HELPER_OBJS) \
	    $(filter $(TEST_BUILD_DIR)/$(dir $(patsubst tests/%,%,$(1)))%.o,$(TEST_HELPER_OBJS)) \
	    $(STATIC_LIB) $(LDLIBS)
endef

$(foreach src,$(TEST_SRCS),$(eval $(call TEST_BIN_RULE,$(src))))

$(BUILD_DIR)/bench/%: $(BUILD_DIR)/bench/%.o $(STATIC_LIB) $(SHARED_LIB)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $< $(STATIC_LIB) $(LDLIBS)

$(BUILD_DIR)/scratch/%: $(BUILD_DIR)/scratch/%.o $(STATIC_LIB) $(SHARED_LIB)
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

$(foreach bin,$(TEST_BINS),$(eval $(call TEST_ALIAS_RULES,$(notdir $(bin)),$(bin))))

define BENCH_ALIAS_RULES
.PHONY: $(1)
$(1): $(2)
	@$(2)
endef

$(foreach bin,$(BENCH_BINS),$(eval $(call BENCH_ALIAS_RULES,$(notdir $(bin)),$(bin))))

define SCRATCH_ALIAS_RULES
.PHONY: $(1) scratch/$(1)
$(1): $(2)
	@$(2)

scratch/$(1): $(2)
	@:
endef

$(foreach bin,$(SCRATCH_BINS),$(eval $(call SCRATCH_ALIAS_RULES,$(notdir $(bin)),$(bin))))

.PHONY: scratch
scratch: $(SCRATCH_BINS)

.PHONY: mars-lab to-be-announced-lab install-mars-lab uninstall-mars-lab install-to-be-announced-lab uninstall-to-be-announced-lab
mars-lab: check-lab-deps $(BUILD_DIR)/scratch/mars_lab
	@tools/mars-lab

.PHONY: to-be-announced-lab
to-be-announced-lab: $(BUILD_DIR)/scratch/to-be-announced_lab
	@tools/to-be-announced-lab

install-mars-lab: check-lab-deps tools/mars-lab packaging/linux/mars-lab.desktop.in packaging/linux/mars-lab.svg $(MARS_LAB_ICON_CONCEPTS)
	$(INSTALL) -d "$(MARS_LAB_BINDIR)" "$(MARS_LAB_APPDIR)" "$(MARS_LAB_ICONDIR)"
	rm -f "$(MARS_LAB_BINDIR)/mars-expr-lab" "$(MARS_LAB_APPDIR)/mars-expr-lab.desktop" "$(MARS_LAB_ICONDIR)/mars-expr-lab.svg" "$(MARS_LAB_ICONDIR)"/mars-expr-lab-*.svg
	@printf '%s\n' \
		'#!/bin/sh' \
		'export MARS_ROOT="$(CURDIR)"' \
		'log_dir="$${XDG_STATE_HOME:-$$HOME/.local/state}/mars-lab"' \
		'mkdir -p "$$log_dir"' \
		'printf "[%s] launch %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$0 $$*" >> "$$log_dir/launcher.log" 2>&1' \
		'open_browser=1' \
		'for arg in "$$@"; do [ "$$arg" = "--no-browser" ] && open_browser=0; done' \
		'unit="mars-lab-$$(date +%s%N)"' \
		'systemd-run --user --collect --unit="$$unit" --setenv=MARS_ROOT="$(CURDIR)" "$(CURDIR)/tools/mars-lab" --host :: --port 8765 --no-browser "$$@" >> "$$log_dir/launcher.log" 2>&1' \
		'status=$$?' \
		'printf "[%s] systemd-run exit %s unit %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$status" "$$unit" >> "$$log_dir/launcher.log" 2>&1' \
		'if [ "$$status" -eq 0 ] && [ "$$open_browser" -eq 1 ]; then' \
		'  sleep 0.8' \
		'  lab_url="$$(cd "$(CURDIR)" && python3 -c "import sys; sys.path.insert(0, \"tools\"); import mars_lab as m; print(m.browser_access_url(\"::\", 8765))" 2>/dev/null || true)"' \
		'  if [ -z "$$lab_url" ]; then lab_url="http://localhost:8765/"; fi' \
		'  if command -v xdg-open >/dev/null 2>&1; then xdg-open "$$lab_url" >/dev/null 2>&1 & elif command -v gio >/dev/null 2>&1; then gio open "$$lab_url" >/dev/null 2>&1 & fi' \
		'fi' \
		'exit "$$status"' \
		> "$(MARS_LAB_LAUNCHER)"
	chmod 755 "$(MARS_LAB_LAUNCHER)"
	$(INSTALL) -m 644 packaging/linux/mars-lab.svg "$(MARS_LAB_ICON)"
	@for icon in $(MARS_LAB_ICON_CONCEPTS); do \
		name=$$(basename "$$icon" .svg); \
		$(INSTALL) -m 644 "$$icon" "$(MARS_LAB_ICONDIR)/mars-lab-$$name.svg"; \
	done
	@sed -e 's|@MARS_LAUNCHER@|$(MARS_LAB_LAUNCHER)|g' packaging/linux/mars-lab.desktop.in > "$(MARS_LAB_DESKTOP)"
	chmod 755 "$(MARS_LAB_DESKTOP)"
	@if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$(MARS_LAB_APPDIR)" >/dev/null 2>&1 || true; fi
	@if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache "$(MARS_LAB_INSTALL_PREFIX)/share/icons/hicolor" >/dev/null 2>&1 || true; fi
	@if command -v kbuildsycoca6 >/dev/null 2>&1; then kbuildsycoca6 >/dev/null 2>&1 || true; elif command -v kbuildsycoca5 >/dev/null 2>&1; then kbuildsycoca5 >/dev/null 2>&1 || true; fi
	@echo "Installed MARS Lab desktop launcher:"
	@echo "  $(MARS_LAB_DESKTOP)"

install-to-be-announced-lab: tools/to-be-announced-lab packaging/linux/to-be-announced-lab.desktop.in packaging/linux/to-be-announced-lab.svg
	$(INSTALL) -d "$(MARS_LAB_BINDIR)" "$(MARS_LAB_APPDIR)" "$(MARS_LAB_ICONDIR)"
	@printf '%s\n' \
		'#!/bin/sh' \
		'export MARS_ROOT="$(CURDIR)"' \
		'log_dir="$${XDG_STATE_HOME:-$$HOME/.local/state}/to-be-announced-lab"' \
		'mkdir -p "$$log_dir"' \
		'printf "[%s] launch %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$0 $$*" >> "$$log_dir/launcher.log" 2>&1' \
		'open_browser=1' \
		'for arg in "$$@"; do [ "$$arg" = "--no-browser" ] && open_browser=0; done' \
		'unit="to-be-announced-lab-$$(date +%s%N)"' \
		'systemd-run --user --collect --unit="$$unit" --setenv=MARS_ROOT="$(CURDIR)" "$(CURDIR)/tools/to-be-announced-lab" --host :: --port 8766 --no-browser "$$@" >> "$$log_dir/launcher.log" 2>&1' \
		'status=$$?' \
		'printf "[%s] systemd-run exit %s unit %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$status" "$$unit" >> "$$log_dir/launcher.log" 2>&1' \
		'if [ "$$status" -eq 0 ] && [ "$$open_browser" -eq 1 ]; then' \
		'  sleep 0.8' \
		'  lab_url="$$(cd "$(CURDIR)" && python3 -c "import sys; sys.path.insert(0, \"tools\"); import to_be_announced_lab as t; print(t.to_be_announced_browser_access_url(\"::\", 8766))" 2>/dev/null || true)"' \
		'  if [ -z "$$lab_url" ]; then lab_host="$$(hostname -s 2>/dev/null | tr "[:upper:]" "[:lower:]")"; [ -n "$$lab_host" ] || lab_host="lenovo"; lab_url="http://$$lab_host.local:8766/to-be-announced/"; fi' \
		'  if command -v xdg-open >/dev/null 2>&1; then xdg-open "$$lab_url" >/dev/null 2>&1 & elif command -v gio >/dev/null 2>&1; then gio open "$$lab_url" >/dev/null 2>&1 & fi' \
		'fi' \
		'exit "$$status"' \
		> "$(TO_BE_ANNOUNCED_LAB_LAUNCHER)"
	chmod 755 "$(TO_BE_ANNOUNCED_LAB_LAUNCHER)"
	$(INSTALL) -m 644 packaging/linux/to-be-announced-lab.svg "$(TO_BE_ANNOUNCED_LAB_ICON)"
	@sed -e 's|@TO_BE_ANNOUNCED_LAUNCHER@|$(TO_BE_ANNOUNCED_LAB_LAUNCHER)|g' packaging/linux/to-be-announced-lab.desktop.in > "$(TO_BE_ANNOUNCED_LAB_DESKTOP)"
	chmod 755 "$(TO_BE_ANNOUNCED_LAB_DESKTOP)"
	@if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$(MARS_LAB_APPDIR)" >/dev/null 2>&1 || true; fi
	@if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache "$(MARS_LAB_INSTALL_PREFIX)/share/icons/hicolor" >/dev/null 2>&1 || true; fi
	@if command -v kbuildsycoca6 >/dev/null 2>&1; then kbuildsycoca6 >/dev/null 2>&1 || true; elif command -v kbuildsycoca5 >/dev/null 2>&1; then kbuildsycoca5 >/dev/null 2>&1 || true; fi
	@echo "Installed To-Be-Announced Lab desktop launcher:"
	@echo "  $(TO_BE_ANNOUNCED_LAB_DESKTOP)"

uninstall-mars-lab:
	rm -f "$(MARS_LAB_LAUNCHER)" "$(MARS_LAB_DESKTOP)" "$(MARS_LAB_ICON)" "$(MARS_LAB_ICONDIR)"/mars-lab-*.svg
	rm -f "$(MARS_LAB_BINDIR)/mars-expr-lab" "$(MARS_LAB_APPDIR)/mars-expr-lab.desktop" "$(MARS_LAB_ICONDIR)/mars-expr-lab.svg" "$(MARS_LAB_ICONDIR)"/mars-expr-lab-*.svg
	@if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$(MARS_LAB_APPDIR)" >/dev/null 2>&1 || true; fi
	@if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache "$(MARS_LAB_INSTALL_PREFIX)/share/icons/hicolor" >/dev/null 2>&1 || true; fi
	@if command -v kbuildsycoca6 >/dev/null 2>&1; then kbuildsycoca6 >/dev/null 2>&1 || true; elif command -v kbuildsycoca5 >/dev/null 2>&1; then kbuildsycoca5 >/dev/null 2>&1 || true; fi

uninstall-to-be-announced-lab:
	rm -f "$(TO_BE_ANNOUNCED_LAB_LAUNCHER)" "$(TO_BE_ANNOUNCED_LAB_DESKTOP)" "$(TO_BE_ANNOUNCED_LAB_ICON)"
	@if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$(MARS_LAB_APPDIR)" >/dev/null 2>&1 || true; fi
	@if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache "$(MARS_LAB_INSTALL_PREFIX)/share/icons/hicolor" >/dev/null 2>&1 || true; fi
	@if command -v kbuildsycoca6 >/dev/null 2>&1; then kbuildsycoca6 >/dev/null 2>&1 || true; elif command -v kbuildsycoca5 >/dev/null 2>&1; then kbuildsycoca5 >/dev/null 2>&1 || true; fi

.PHONY: gen_qfloat_tables gen_qfloat_constants
gen_qfloat_tables: $(QFLOAT_TOOL_BIN)
	@$(QFLOAT_TOOL_BIN) --exp-coef

gen_qfloat_constants: $(QFLOAT_TOOL_BIN)
	@$(QFLOAT_TOOL_BIN)

# ------------------------------------------------------------
# Help
# ------------------------------------------------------------
help:
	@echo "Targets:"
	@echo "  make debug                  Build debug binaries and tests"
	@echo "  make release                Build release binaries and tests"
	@echo "  make test                   Run all tests (release)"
	@echo "  make memtest                Run all tests under valgrind (release)"
	@echo "  make test_<name>            Build and run a single test (e.g. make test_expression) (release)"
	@echo "  make memtest_<name>         Build and run a single test under valgrind (release)"
	@echo "  make DEBUG=1 test           Run all tests (debug)"
	@echo "  make DEBUG=1 memtest        Run all tests under valgrind (debug)"
	@echo "  make DEBUG=1 test_<name>    Build and run a single test (e.g. make test_expression) (debug)"
	@echo "  make DEBUG=1 memtest_<name> Build and run a single test under valgrind (debug)"
	@echo "  make bench_<name>           Build and run a benchmark (e.g. make bench_integrator)"
	@echo "  make scratch                Build all scratch binaries"
	@echo "  make mars_lab               Build and run scratch/mars_lab.c"
	@echo "  make to-be-announced_lab              Build and run scratch/to-be-announced_lab.c"
	@echo "  make scratch/mars_lab       Build scratch/mars_lab.c"
	@echo "  make scratch/to-be-announced_lab      Build scratch/to-be-announced_lab.c"
	@echo "  make mars-lab               Launch the local MARS Lab"
	@echo "  make to-be-announced-lab              Launch the local To-Be-Announced Lab"
	@echo "  make install-mars-lab       Install a user desktop launcher for MARS Lab"
	@echo "  make uninstall-mars-lab     Remove the user desktop launcher for MARS Lab"
	@echo "  make install-to-be-announced-lab      Install a user desktop launcher for To-Be-Announced Lab"
	@echo "  make uninstall-to-be-announced-lab    Remove the user desktop launcher for To-Be-Announced Lab"
	@echo "  make check-deps             Check required external development libraries"
	@echo "  make check-lab-deps         Check development libraries and MARS Lab TeX tools"
	@echo "  make install                Install libraries and headers under PREFIX (default /usr/local)"
	@echo "  make uninstall              Remove installed libraries and headers from PREFIX"
	@echo "  make clean                  Remove all build artifacts"

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------
clean:
	rm -rf build tests/build
