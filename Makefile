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
SQLCIPHER ?= sqlcipher

PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
DATADIR ?= $(PREFIX)/share
DOCDIR ?= $(DATADIR)/doc/mars

LEGAL_ROOT_DOCUMENTS := LICENSE THIRD_PARTY_NOTICES.md DEPENDENCIES.spdx
LEGAL_GUIDE_DOCUMENTS := docs/licensing.md docs/privacy.md docs/almanac-data-provenance.md docs/visual-asset-provenance.md docs/compliance-status.md
LEGAL_DOCUMENTS := $(LEGAL_ROOT_DOCUMENTS) $(LEGAL_GUIDE_DOCUMENTS)
RELEASE_LIBRARY := build/release/libmars.so
RELEASE_EVIDENCE ?= build/compliance/release-evidence.json

MARS_LAB_INSTALL_PREFIX ?= $(HOME)/.local
MARS_LAB_BINDIR ?= $(MARS_LAB_INSTALL_PREFIX)/bin
MARS_LAB_APPDIR ?= $(MARS_LAB_INSTALL_PREFIX)/share/applications
MARS_LAB_ICONDIR ?= $(MARS_LAB_INSTALL_PREFIX)/share/icons/hicolor/scalable/apps
MARS_LAB_LAUNCHER ?= $(MARS_LAB_BINDIR)/mars-lab
MARS_LAB_DESKTOP ?= $(MARS_LAB_APPDIR)/mars-lab.desktop
MARS_LAB_ICON ?= $(MARS_LAB_ICONDIR)/mars-lab.svg
MARS_LAB_ICON_CONCEPTS := $(wildcard packaging/linux/icon-concepts/*.svg)
ALMANAC_DB_SOURCE_DIR ?= packaging/almanac-db
ALMANAC_RULES_SQL ?= $(ALMANAC_DB_SOURCE_DIR)/mars_almanac.sql
ALMANAC_CHEBYSHEV_SQL ?= $(ALMANAC_DB_SOURCE_DIR)/mars_almanac_chebyshev.sql
ALMANAC_FRAME_ROTATION_SQL ?= $(ALMANAC_DB_SOURCE_DIR)/mars_almanac_frame_rotation.sql
ALMANAC_RULES_SOURCES := $(ALMANAC_RULES_SQL) $(wildcard $(ALMANAC_CHEBYSHEV_SQL)) $(wildcard $(ALMANAC_FRAME_ROTATION_SQL))
JURISDICTION_DB_SOURCE_DIR ?= packaging/jurisdiction-db
HOLIDAY_DB_SOURCE_DIR ?= $(JURISDICTION_DB_SOURCE_DIR)
JURISDICTION_RULES_SQL ?= $(JURISDICTION_DB_SOURCE_DIR)/mars_holiday_rules.sql
HOLIDAY_RULES_SQL ?= $(JURISDICTION_RULES_SQL)
JURISDICTION_RULES_SOURCES := $(JURISDICTION_RULES_SQL) $(JURISDICTION_DB_SOURCE_DIR)/mars_country_jurisdictions.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_generated_first_class_rules.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_target_subdivisions.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_manual_first_class_rules.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_jurisdiction_location_defaults.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_jurisdiction_towns.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_timezone_rules.sql
JURISDICTION_RULES_SOURCES += $(JURISDICTION_DB_SOURCE_DIR)/mars_calendar_local.sql
JURISDICTION_RULES_SOURCES += $(JURISDICTION_DB_SOURCE_DIR)/mars_calendar_locale_names.sql
JURISDICTION_RULES_SOURCES += $(JURISDICTION_DB_SOURCE_DIR)/mars_holiday_localized_names.sql
HOLIDAY_RULES_SOURCES := $(JURISDICTION_RULES_SOURCES)
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
# Streaming file compression and authenticated encryption
# ------------------------------------------------------------
ZSTD_CFLAGS := $(shell pkg-config --cflags libzstd 2>/dev/null)
ZSTD_LIBS   := $(shell pkg-config --libs libzstd 2>/dev/null)
SODIUM_CFLAGS := $(shell pkg-config --cflags libsodium 2>/dev/null)
SODIUM_LIBS   := $(shell pkg-config --libs libsodium 2>/dev/null)

CFLAGS += $(ZSTD_CFLAGS) $(SODIUM_CFLAGS)
LDLIBS += $(if $(strip $(ZSTD_LIBS)),$(ZSTD_LIBS),-lzstd)
LDLIBS += $(if $(strip $(SODIUM_LIBS)),$(SODIUM_LIBS),-lsodium)

# Verified HTTP/HTTPS transport (libcurl 7.86.0 or newer).
CURL_CFLAGS := $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS := $(shell pkg-config --libs libcurl 2>/dev/null)
CFLAGS += $(CURL_CFLAGS)
LDLIBS += $(if $(strip $(CURL_LIBS)),$(CURL_LIBS),-lcurl)

# ------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------
SRCS    := $(shell find src -name '*.c' | sort)
OBJS    := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)
QFLOAT_OBJS   := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(wildcard src/qfloat/*.c))
QCOMPLEX_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(wildcard src/qcomplex/*.c))

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
.PHONY: all clean test memtest debug release release-evidence check-deps check-public-distribution check-compliance check-native-numeric-boundaries check-jurisdiction-db-deps check-lab-deps install uninstall mars-lab mars-lab-stop mars-lab-restart to-be-announced-lab install-almanac-db uninstall-almanac-db install-jurisdiction-db uninstall-jurisdiction-db install-mars-lab uninstall-mars-lab help

all: check-public-distribution check-native-numeric-boundaries $(STATIC_LIB) $(SHARED_LIB) $(TEST_BINS) $(BENCH_BINS) $(SCRATCH_BINS)

debug:
	$(MAKE) DEBUG=1 all

release: check-compliance
	$(MAKE) DEBUG=0 all

release-evidence: release
	@tools/write_release_evidence.py --library "$(RELEASE_LIBRARY)" --output "$(RELEASE_EVIDENCE)"

# Private reference material may remain on the developer's machine, but it
# must never enter the public repository index.
check-public-distribution:
	@tools/check_public_distribution.py

check-compliance: check-public-distribution
	@tools/check_compliance.py --quiet
	@tools/check_markdown_api_coverage.py

# qfloat and qcomplex are native double-double modules.  MPFR and MPC belong
# to the number backend and must not leak across this module boundary.
check-native-numeric-boundaries: $(QFLOAT_OBJS) $(QCOMPLEX_OBJS)
	@if grep -ERn '#include[[:space:]]*[<"](mpfr|mpc)\.h|(^|[^[:alnum:]_])(mpfr_|mpc_)' \
		include/qfloat.h include/qcomplex.h src/qfloat src/qcomplex; then \
		echo "qfloat/qcomplex must not use MPFR or MPC."; \
		exit 1; \
	fi
	@if nm -u $(QFLOAT_OBJS) $(QCOMPLEX_OBJS) \
		| grep -Eq '[[:space:]](mpfr_|mpc_)'; then \
		echo "qfloat/qcomplex objects contain MPFR or MPC references."; \
		exit 1; \
	fi

# ------------------------------------------------------------
# Dependency checks
# ------------------------------------------------------------
check-deps:
	@missing=0; \
	probe_dir=$$(mktemp -d /tmp/mars-check-deps.XXXXXX) || exit 1; \
	trap 'rm -f "$$probe_dir/probe"; rmdir "$$probe_dir"' EXIT; \
	trap 'exit 1' HUP INT TERM; \
	packages=""; \
	check_dep() { \
	    name="$$1"; header="$$2"; lib="$$3"; package="$$4"; body="$$5"; \
	    if ! printf '%s\n' "#include <stdint.h>" "#include <$$header>" "int main(void) { $$body; return 0; }" \
	        | $(CC) -x c - -o "$$probe_dir/probe" $$lib >/dev/null 2>&1; then \
	        echo "Missing $$name development files."; \
	        echo "  Debian/Ubuntu: sudo apt install $$package"; \
	        packages="$$packages $$package"; \
	        missing=1; \
	    fi; \
	    rm -f "$$probe_dir/probe"; \
	}; \
	check_dep "GMP" "gmp.h" "-lgmp" "libgmp-dev" "mpz_t x; mpz_init(x); mpz_clear(x)"; \
	check_dep "libcurl >= 7.86.0" "curl/curl.h" "$(CURL_CFLAGS) $(if $(strip $(CURL_LIBS)),$(CURL_LIBS),-lcurl)" "libcurl4-openssl-dev" "_Static_assert(LIBCURL_VERSION_NUM >= 0x075600, \"libcurl too old\"); CURL *c = curl_easy_init(); curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, \"https\"); size_t n = 0; char b = 0; curl_ws_send(c, &b, 0, &n, 0, CURLWS_TEXT); curl_easy_cleanup(c)"; \
	check_dep "MPFR" "mpfr.h" "-lmpfr -lgmp" "libmpfr-dev" "mpfr_t x; mpfr_init2(x, 53); mpfr_clear(x)"; \
	check_dep "MPC" "mpc.h" "-lmpc -lmpfr -lgmp" "libmpc-dev" "mpc_t x; mpc_init2(x, 53); mpc_clear(x)"; \
	check_dep "SQLCipher" "sqlcipher/sqlite3.h" "-lsqlcipher" "libsqlcipher-dev" "sqlite3 *db = 0; sqlite3_open(\":memory:\", &db); sqlite3_close(db)"; \
	check_dep "Zstandard" "zstd.h" "$(ZSTD_CFLAGS) $(if $(strip $(ZSTD_LIBS)),$(ZSTD_LIBS),-lzstd)" "libzstd-dev" "ZSTD_CCtx *ctx = ZSTD_createCCtx(); ZSTD_inBuffer in = {0}; ZSTD_outBuffer out = {0}; ZSTD_compressStream2(ctx, &out, &in, ZSTD_e_end); ZSTD_freeCCtx(ctx)"; \
	check_dep "libsodium secretstream" "sodium.h" "$(SODIUM_CFLAGS) $(if $(strip $(SODIUM_LIBS)),$(SODIUM_LIBS),-lsodium)" "libsodium-dev" "crypto_secretstream_xchacha20poly1305_state state; unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES], header[crypto_secretstream_xchacha20poly1305_HEADERBYTES]; if (sodium_init() < 0) return 1; crypto_secretstream_xchacha20poly1305_keygen(key); return crypto_secretstream_xchacha20poly1305_init_push(&state, header, key)"; \
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

check-jurisdiction-db-deps: check-deps
	@missing=0; \
	packages=""; \
	check_tool() { \
	    name="$$1"; command="$$2"; package="$$3"; note="$$4"; \
	    if ! command -v "$$command" >/dev/null 2>&1; then \
	        echo "Missing $$name$$note."; \
	        echo "  Debian/Ubuntu: sudo apt install $$package"; \
	        packages="$$packages $$package"; \
	        missing=1; \
	    fi; \
	}; \
	check_tool "SQLCipher CLI" "$(SQLCIPHER)" "sqlcipher" " for jurisdiction database installation"; \
	if [ "$$missing" -ne 0 ]; then \
	    echo; \
	    echo "Install the missing jurisdiction database runtime tool(s), then rerun make."; \
	    echo "Debian/Ubuntu command:"; \
	    echo "  sudo apt install$$packages"; \
	    exit 1; \
	fi

check-lab-deps: check-jurisdiction-db-deps
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
	check_python() { \
	    if ! command -v python3 >/dev/null 2>&1; then \
	        echo "Missing Python 3.10 or later for MARS Lab."; \
	        echo "  Debian/Ubuntu: sudo apt install python3"; \
	        packages="$$packages python3"; \
	        missing=1; \
	    elif ! python3 -c 'import sys; raise SystemExit(sys.version_info < (3, 10))'; then \
	        echo "MARS Lab requires Python 3.10 or later."; \
	        echo "  Found: $$(python3 --version 2>&1)"; \
	        missing=1; \
	    fi; \
	}; \
	check_python; \
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
install: check-deps check-compliance $(STATIC_LIB) $(SHARED_LIB) $(LEGAL_DOCUMENTS)
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)"
	$(INSTALL) -d "$(DESTDIR)$(INCLUDEDIR)/mars"
	$(INSTALL) -d "$(DESTDIR)$(DOCDIR)/docs"
	$(INSTALL) -m 644 $(STATIC_LIB) "$(DESTDIR)$(LIBDIR)/libmars.a"
	$(INSTALL) -m 755 $(SHARED_LIB) "$(DESTDIR)$(LIBDIR)/libmars.so"
	$(INSTALL) -m 644 $(HEADERS) "$(DESTDIR)$(INCLUDEDIR)/mars"
	$(INSTALL) -m 644 $(LEGAL_ROOT_DOCUMENTS) "$(DESTDIR)$(DOCDIR)"
	$(INSTALL) -m 644 $(LEGAL_GUIDE_DOCUMENTS) "$(DESTDIR)$(DOCDIR)/docs"

uninstall:
	rm -f "$(DESTDIR)$(LIBDIR)/libmars.a"
	rm -f "$(DESTDIR)$(LIBDIR)/libmars.so"
	@for h in $(notdir $(HEADERS)); do \
	    rm -f "$(DESTDIR)$(INCLUDEDIR)/mars/$$h"; \
	done
	@for document in $(notdir $(LEGAL_ROOT_DOCUMENTS)); do \
	    rm -f "$(DESTDIR)$(DOCDIR)/$$document"; \
	done
	@for document in $(notdir $(LEGAL_GUIDE_DOCUMENTS)); do \
	    rm -f "$(DESTDIR)$(DOCDIR)/docs/$$document"; \
	done
	-rmdir "$(DESTDIR)$(INCLUDEDIR)/mars"
	-rmdir "$(DESTDIR)$(DOCDIR)/docs"
	-rmdir "$(DESTDIR)$(DOCDIR)"

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
$(TEST_BUILD_DIR)/matrix/test_matrix: TEST_LINK_FLAGS = -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=string_free -Wl,--wrap=expr_to_text_symbolic

FILE_TEST_WRAPS := malloc calloc realloc strdup read write rename renameat2 unlink fsync fdatasync fchownat \
                   fdopen fdopendir readdir statx fstat fclose close fchmod ftello fseeko
# Keep the comma literal separate from Make function argument separators.
comma := ,
$(TEST_BUILD_DIR)/file/test_file: TEST_LINK_FLAGS = $(foreach fn,$(FILE_TEST_WRAPS),-Wl$(comma)--wrap=$(fn))

# Instrument only the file module; keep normal library objects and counters separate.
FILE_COVERAGE_DIR := build/coverage/file
FILE_COVERAGE_SRCS := $(wildcard src/file/*.c)
FILE_COVERAGE_OBJS := $(patsubst src/file/%.c,$(FILE_COVERAGE_DIR)/%.o,$(FILE_COVERAGE_SRCS))
FILE_TEST_HELPERS = $(filter $(TEST_BUILD_DIR)/file/%.o,$(TEST_HELPER_OBJS))

# Gate each compiled or linked artefact, not just aggregate targets: sibling
# prerequisites could otherwise start compiling before the check in parallel.
# Order-only prerequisites avoid rebuilding current artefacts merely because
# the dependency check is phony. Direct object and executable targets are covered.
$(OBJS) $(TEST_OBJS) $(TEST_HELPER_OBJS) $(TEST_COMMON_HELPER_OBJS) \
$(BENCH_OBJS) $(SCRATCH_OBJS) $(STATIC_LIB) $(SHARED_LIB) $(TEST_BINS) \
$(BENCH_BINS) $(SCRATCH_BINS) $(QFLOAT_TOOL_BIN) \
$(FILE_COVERAGE_OBJS) $(FILE_COVERAGE_DIR)/test_file: | check-deps

$(FILE_COVERAGE_DIR)/%.o: src/file/%.c include/file.h src/file/file_internal.h Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -O0 -g --coverage $(INCLUDES) -c $< -o $@

$(FILE_COVERAGE_DIR)/test_file: $(TEST_BUILD_DIR)/file/test_file $(FILE_COVERAGE_OBJS)
	$(CC) --coverage $(foreach fn,$(FILE_TEST_WRAPS),-Wl$(comma)--wrap=$(fn)) -o $@ \
	    $(TEST_BUILD_DIR)/file/test_file.o $(TEST_COMMON_HELPER_OBJS) $(FILE_TEST_HELPERS) \
	    $(FILE_COVERAGE_OBJS) $(STATIC_LIB) $(LDLIBS)

.PHONY: coverage-file
coverage-file: $(FILE_COVERAGE_DIR)/test_file
	rm -f $(FILE_COVERAGE_OBJS:.o=.gcda)
	$(FILE_COVERAGE_DIR)/test_file
	cd $(FILE_COVERAGE_DIR) && gcov -b -c -j $(notdir $(FILE_COVERAGE_OBJS))
	python3 tools/report_file_coverage.py $(FILE_COVERAGE_DIR)

define TEST_BIN_RULE
$(patsubst tests/%.c,$(TEST_BUILD_DIR)/%,$(1)): \
    $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%.o,$(1)) \
    $(STATIC_LIB) $(SHARED_LIB) \
    $(TEST_COMMON_HELPER_OBJS) \
    $(filter $(TEST_BUILD_DIR)/$(dir $(patsubst tests/%,%,$(1)))%.o,$(TEST_HELPER_OBJS))
	@mkdir -p $$(dir $$@)
	$(CC) $$(TEST_LINK_FLAGS) -o $$@ \
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
VALGRIND_TRACK_ORIGINS ?= no
VALGRIND := valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --errors-for-leak-kinds=definite,indirect,possible \
    --error-exitcode=99 \
    --track-origins=$(VALGRIND_TRACK_ORIGINS)

README_EXAMPLE_CHECK = python3 tools/check_readme_examples.py --cc="$(CC)" --cflags="$(CFLAGS)" --libs="$(LDLIBS)" --archive="$(STATIC_LIB)"

.PHONY: test-readme-examples
test-readme-examples: $(STATIC_LIB)
	@$(README_EXAMPLE_CHECK)

test: check-public-distribution $(TEST_BINS)
	@rc=0; for t in $(TEST_BINS); do \
	    printf "  %-40s" "$$t ..."; \
	    if $$t > /dev/null 2>&1; then \
	        echo "PASS"; \
	    else \
	        echo "FAIL"; rc=1; \
	    fi; \
	done; $(README_EXAMPLE_CHECK) || rc=1; exit $$rc

memtest: $(TEST_BINS)
	@rc=0; for t in $(TEST_BINS); do \
	    echo "=== $$t ==="; \
	    $(VALGRIND) $$t || rc=1; \
	done; exit $$rc

define TEST_ALIAS_RULES
.PHONY: $(1) mem$(1)
$(1): $(2)
	@$(2)

mem$(1): $(2)
	$(VALGRIND) $(2)
endef

TEST_ALIAS_EXCLUDES := test_almanac memtest_almanac
.PHONY: test-http-live
test-http-live: $(TEST_BUILD_DIR)/http_live/test_http_live
	@$<

$(foreach bin,$(filter-out $(addprefix tests/build/release/almanac/,$(TEST_ALIAS_EXCLUDES)),$(TEST_BINS)),$(eval $(call TEST_ALIAS_RULES,$(notdir $(bin)),$(bin))))

.PHONY: test_almanac memtest_almanac
test_almanac: tests/build/release/almanac/test_almanac tools/configure_mars_lab_almanac_db.py
	@tmp_out=$$(mktemp); \
	tmp_status=$$(mktemp); \
	{ stdbuf -oL -eL $< 2>&1; echo $$? >"$$tmp_status"; } | tee "$$tmp_out"; \
	rc=$$(cat "$$tmp_status"); \
	rm -f "$$tmp_status"; \
	if [ "$$rc" -eq 0 ]; then \
	    rm -f "$$tmp_out"; \
	    exit 0; \
	fi; \
	if grep -q "Almanac tests require a configured almanac database." "$$tmp_out"; then \
	    rm -f "$$tmp_out"; \
	    python3 tools/configure_mars_lab_almanac_db.py || exit $$?; \
	    exec stdbuf -oL -eL $<; \
	fi; \
	rm -f "$$tmp_out"; \
	exit 1

memtest_almanac: tests/build/release/almanac/test_almanac tools/configure_mars_lab_almanac_db.py
	@tmp_out=$$(mktemp); \
	tmp_status=$$(mktemp); \
	{ $(VALGRIND) $< 2>&1; echo $$? >"$$tmp_status"; } | tee "$$tmp_out"; \
	rc=$$(cat "$$tmp_status"); \
	rm -f "$$tmp_status"; \
	if [ "$$rc" -eq 0 ]; then \
	    rm -f "$$tmp_out"; \
	    exit 0; \
	fi; \
	if grep -q "Almanac tests require a configured almanac database." "$$tmp_out"; then \
	    rm -f "$$tmp_out"; \
	    python3 tools/configure_mars_lab_almanac_db.py || exit $$?; \
	    exec $(VALGRIND) $<; \
	fi; \
	rm -f "$$tmp_out"; \
	exit 1

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

.PHONY: mars-lab mars-lab-stop mars-lab-restart to-be-announced-lab install-almanac-db uninstall-almanac-db install-jurisdiction-db uninstall-jurisdiction-db install-mars-lab uninstall-mars-lab install-to-be-announced-lab uninstall-to-be-announced-lab
mars-lab: check-lab-deps $(BUILD_DIR)/scratch/mars_lab
	@tools/mars-lab

mars-lab-stop:
	@if pgrep -u "$$USER" -f '[p]ython3 tools/mars_lab.py' >/dev/null; then \
		pkill -u "$$USER" -f '[p]ython3 tools/mars_lab.py'; \
		echo "Stopped MARS Lab."; \
	else \
		echo "MARS Lab is not running."; \
	fi

mars-lab-restart: mars-lab-stop
	@$(MAKE) --no-print-directory mars-lab

.PHONY: to-be-announced-lab
to-be-announced-lab: $(BUILD_DIR)/scratch/to-be-announced_lab
	@tools/to-be-announced-lab

install-almanac-db: check-jurisdiction-db-deps tools/configure_mars_lab_almanac_db.py $(ALMANAC_RULES_SOURCES)
	@python3 tools/configure_mars_lab_almanac_db.py

uninstall-almanac-db:
	rm -f "$(HOME)/.mars/almanac/almanac.db" "$(HOME)/.mars/config/almanac-db.env"

ifeq ($(firstword $(MAKECMDGOALS)),install-jurisdiction-db)
    ifneq ($(word 3,$(MAKECMDGOALS)),)
        $(error Supply one town or use LOCATION='town name' for names containing spaces)
    endif
    CALENDAR_LOCATION_GOAL := $(word 2,$(MAKECMDGOALS))
    ifneq ($(CALENDAR_LOCATION_GOAL),)
        ifneq ($(filter all clean test memtest test_% memtest_% debug release release-evidence install uninstall help scratch bench_% gen_% install-% uninstall-% check-% mars-lab% to-be-announced%,$(CALENDAR_LOCATION_GOAL)),)
            $(error A calendar location cannot be another Make target; run targets separately)
        endif
        ifneq ($(findstring /,$(CALENDAR_LOCATION_GOAL)),)
            $(error A calendar location must be a town name, not a file target)
        endif
        .PHONY: $(CALENDAR_LOCATION_GOAL)
        $(CALENDAR_LOCATION_GOAL):
	@:
    endif
endif

install-jurisdiction-db: export MARS_CALENDAR_LOCATION_ARGUMENT = $(if $(LOCATION),$(LOCATION),$(CALENDAR_LOCATION_GOAL))
install-jurisdiction-db: export MARS_CALENDAR_LANGUAGE_ARGUMENT = $(CALENDAR_LANGUAGE)
install-jurisdiction-db: export MARS_CALENDAR_LIBRARY = $(abspath $(SHARED_LIB))
install-jurisdiction-db: check-jurisdiction-db-deps $(SHARED_LIB) tools/configure_mars_lab_jurisdiction_db.py tools/jurisdiction_calendar.py tools/jurisdiction_calendar_languages.py $(JURISDICTION_RULES_SOURCES)
	@python3 tools/configure_mars_lab_jurisdiction_db.py

uninstall-jurisdiction-db:
	rm -rf "$(HOME)/.mars"

install-mars-lab: export MARS_CALENDAR_LIBRARY = $(abspath $(SHARED_LIB))
install-mars-lab: check-lab-deps $(SHARED_LIB) tools/mars-lab tools/configure_mars_lab_jurisdiction_db.py tools/jurisdiction_calendar.py tools/jurisdiction_calendar_languages.py tools/configure_mars_lab_weather.py $(JURISDICTION_RULES_SOURCES) packaging/linux/mars-lab.desktop.in packaging/linux/mars-lab.svg $(MARS_LAB_ICON_CONCEPTS)
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
	@$(MAKE) install-jurisdiction-db
	@python3 tools/configure_mars_lab_weather.py
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
	@echo "  make release-evidence       Build a release and record its binary dependency evidence"
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
	@echo "  make mars-lab-stop          Stop the local MARS Lab"
	@echo "  make mars-lab-restart       Stop and relaunch the local MARS Lab"
	@echo "  make to-be-announced-lab              Launch the local To-Be-Announced Lab"
	@echo "  make install-mars-lab       Install a user desktop launcher for MARS Lab"
	@echo "  make uninstall-mars-lab     Remove the user desktop launcher for MARS Lab"
	@echo "  make install-almanac-db     Build and configure the Almanac database only"
	@echo "  make uninstall-almanac-db   Remove the configured Almanac database"
	@echo "  make install-jurisdiction-db [town] Build the private database and calendar_local for a town"
	@echo "  make install-jurisdiction-db LOCATION='town, jurisdiction' Select a qualified or multiword town"
	@echo "  Add CALENDAR_LANGUAGE=code to preselect a calendar language (also works non-interactively)"
	@echo "  make uninstall-jurisdiction-db Remove ~/.mars, including the private jurisdiction database"
	@echo "  make install-to-be-announced-lab      Install a user desktop launcher for To-Be-Announced Lab"
	@echo "  make uninstall-to-be-announced-lab    Remove the user desktop launcher for To-Be-Announced Lab"
	@echo "  make check-deps             Check required external development libraries"
	@echo "  make test-http-live         Test the HTTP client against the native loopback web server"
	@echo "  make test_webserver        Run offline Linux web server tests and its README example"
	@echo "  make test-readme-examples   Compile/run complete Markdown C examples"
	@echo "  make check-compliance       Verify public-path, notice, SPDX and provenance safeguards"
	@echo "  make check-jurisdiction-db-deps Check runtime tools needed for jurisdiction database installation"
	@echo "  make check-lab-deps         Check development libraries and MARS Lab TeX tools"
	@echo "  make install                Install libraries and headers under PREFIX (default /usr/local)"
	@echo "  make uninstall              Remove installed libraries and headers from PREFIX"
	@echo "  make clean                  Remove all build artifacts"

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------
clean:
	rm -rf build tests/build
