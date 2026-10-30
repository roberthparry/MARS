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

# Keep compiler spill files on the build filesystem; explicit TMPDIR remains supported.
TMPDIR ?= $(abspath $(BUILD_DIR)/tmp)
export TMPDIR

CFLAGS += -D_GNU_SOURCE -pthread

CC := gcc
AR := ar rcs
INSTALL ?= install
TSC ?= tsc

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
ALMANAC_RULES_SOURCES := $(ALMANAC_RULES_SQL) $(wildcard $(ALMANAC_CHEBYSHEV_SQL)) $(wildcard $(ALMANAC_FRAME_ROTATION_SQL)) \
    $(wildcard $(ALMANAC_DB_SOURCE_DIR)/gshhs_f.b.zst)
JURISDICTION_DB_SOURCE_DIR ?= packaging/jurisdiction-db
HOLIDAY_DB_SOURCE_DIR ?= $(JURISDICTION_DB_SOURCE_DIR)
JURISDICTION_RULES_SQL ?= $(JURISDICTION_DB_SOURCE_DIR)/mars_holiday_rules.sql
HOLIDAY_RULES_SQL ?= $(JURISDICTION_RULES_SQL)
JURISDICTION_RULES_SOURCES := $(JURISDICTION_RULES_SQL) $(JURISDICTION_DB_SOURCE_DIR)/mars_country_jurisdictions.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_generated_first_class_rules.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_target_subdivisions.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_manual_first_class_rules.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_jurisdiction_location_defaults.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_jurisdiction_towns.sql $(JURISDICTION_DB_SOURCE_DIR)/mars_timezone_rules.sql
JURISDICTION_RULES_SOURCES += $(JURISDICTION_DB_SOURCE_DIR)/mars_calendar_local.sql
JURISDICTION_RULES_SOURCES += $(JURISDICTION_DB_SOURCE_DIR)/mars_jurisdiction_world_towns.sql
JURISDICTION_RULES_SOURCES += $(JURISDICTION_DB_SOURCE_DIR)/mars_calendar_locale_names.sql
JURISDICTION_RULES_SOURCES += $(JURISDICTION_DB_SOURCE_DIR)/mars_holiday_localized_names.sql
HOLIDAY_RULES_SOURCES := $(JURISDICTION_RULES_SOURCES)
TO_BE_ANNOUNCED_LAB_LAUNCHER ?= $(MARS_LAB_BINDIR)/to-be-announced-lab
TO_BE_ANNOUNCED_LAB_DESKTOP ?= $(MARS_LAB_APPDIR)/to-be-announced-lab.desktop
TO_BE_ANNOUNCED_LAB_ICON ?= $(MARS_LAB_ICONDIR)/to-be-announced-lab.svg

INCLUDES := -I. -Iinclude -Isrc -Itests -Itests/include -Itools/common

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
LDLIBS += -pthread
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

HEADERS      := $(wildcard include/*.h)

STATIC_LIB := $(BUILD_DIR)/libmars.a
SHARED_LIB := $(BUILD_DIR)/libmars.so

APP_RESOURCE_OBJ := $(BUILD_DIR)/tools/mars_paths.o $(BUILD_DIR)/tools/mars_settings.o $(BUILD_DIR)/tools/mars_keyring.o
$(BUILD_DIR)/tools/%.o: tools/common/%.c tools/common/mars_paths.h tools/common/mars_settings.h tools/common/mars_keyring.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test-only native protocol peers; OpenSSL is not an additional MARS link requirement.

$(TEST_BUILD_DIR)/shoreline/check_shoreline_precision: tests/shoreline/check_shoreline_precision.c $(STATIC_LIB)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(STATIC_LIB) $(LDLIBS)
HTTP_FIXTURE_BIN := $(TEST_BUILD_DIR)/http/fixtures/http_fixture
HTTP_FIXTURE_SRCS := $(sort $(wildcard tests/http/fixtures/*.c))
HTTP_FIXTURE_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
HTTP_FIXTURE_LIBS := $(shell pkg-config --libs openssl 2>/dev/null)
ifeq ($(strip $(HTTP_FIXTURE_LIBS)),)
HTTP_FIXTURE_LIBS := -lssl -lcrypto
endif

# The included Lab rules precede the root's targets; retain the root default.
.DEFAULT_GOAL := all
include tools/mars_lab/Makefile
include tools/to_be_announced_lab/Makefile
include tools/mars_config/Makefile
include tools/mars_checks/Makefile

$(CHECKS_BUILD_DIR)/mars-config.db: $(CONFIG_BUILD_DIR)/mars-config.db
	@mkdir -p $(@D)
	cp "$<" "$@"

$(CHECKS_BIN): | $(CHECKS_BUILD_DIR)/mars-config.db

TEST_BINS  := $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%,$(TEST_SRCS))
TEST_BINS += $(TBA_TEST_BIN) $(LAB_TEST_BIN) $(CONFIG_TEST_BIN) $(CHECKS_TEST_BIN)

# Bootstrap the maintenance applications before compiling downstream programs.
# Their common static library must remain outside this barrier: both tools link it.
# Order-only prerequisites enforce this even with parallel make, without forcing
# otherwise up-to-date outputs to rebuild because native-tools is phony.
.PHONY: native-tools
native-tools: $(CHECKS_BIN) $(CONFIG_PROGRAM)

$(SHARED_LIB) $(TEST_OBJS) $(TEST_HELPER_OBJS) $(TEST_COMMON_HELPER_OBJS) \
$(TEST_BINS) $(BENCH_OBJS) $(BENCH_BINS) $(SCRATCH_OBJS) $(SCRATCH_BINS) \
$(filter-out $(APP_RESOURCE_OBJ),$(LAB_OBJS)) $(LAB_TEST_OBJS) $(LAB_SERVER) \
$(filter-out $(APP_RESOURCE_OBJ),$(TBA_OBJS)) $(TBA_SERVER) \
$(CONFIG_TEST_OBJS) $(CHECKS_TEST_OBJS) $(CHECKS_TEST_FIXTURE_BIN): | native-tools

.SECONDEXPANSION:
.SECONDARY: $(TEST_OBJS) $(TEST_HELPER_OBJS) $(TEST_COMMON_HELPER_OBJS)

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------
.PHONY: all clean test memtest debug release release-evidence check-deps check-public-distribution check-compliance check-native-numeric-boundaries check-lab-deps install uninstall mars-lab mars-lab-stop mars-lab-restart to-be-announced-lab install-almanac-db uninstall-almanac-db install-jurisdiction-db uninstall-jurisdiction-db install-mars-lab uninstall-mars-lab help

# Documented scratch examples are opt-in; tests compile their sources directly.
all: check-deps native-tools check-public-distribution check-native-numeric-boundaries $(STATIC_LIB) $(SHARED_LIB) $(TEST_BINS) $(BENCH_BINS) native-lab native-tba-lab

debug:
	$(MAKE) DEBUG=1 all

release: check-compliance
	$(MAKE) DEBUG=0 all
	@if [ -x "$(MARS_LAB_LAUNCHER)" ]; then \
		$(MAKE) --no-print-directory DEBUG=0 configure-mars && \
		$(MAKE) --no-print-directory DEBUG=0 update-mars-db; \
	fi

release-evidence: release
	@tools/mars_checks/build/release/mars_checks release-evidence --library "$(RELEASE_LIBRARY)" --output "$(RELEASE_EVIDENCE)"

# Private reference material may remain on the developer's machine, but it
# must never enter the public repository index.
check-public-distribution: native-tools
	@"$(CHECKS_BIN)" public-distribution

check-compliance: check-public-distribution $(CHECKS_BIN)
	@"$(CHECKS_BIN)" compliance --quiet $(COMPLIANCE_ARGS)
	@"$(CHECKS_BIN)" markdown-api
	@"$(CHECKS_BIN)" source-policy

.PHONY: check-source-policy
check-source-policy: $(CHECKS_BIN)
	@"$(CHECKS_BIN)" source-policy

# qfloat and qcomplex are native double-double modules.  MPFR and MPC belong
# to the number backend and must not leak across this module boundary.
check-native-numeric-boundaries: $(QFLOAT_OBJS) $(QCOMPLEX_OBJS) | native-tools
	@if grep -ERn '#include[[:space:]]*[<"](mpfr|mpc)\.h|(^|[^[:alnum:]_])(mpfr_|mpc_)' \
		include/qfloat-t.h include/qcomplex-t.h src/qfloat src/qcomplex; then \
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
.PHONY: check-native-deps check-lab-typescript-deps check-tba-typescript-deps typescript-install-help build-temporary
build-temporary:
	@mkdir -p "$(TMPDIR)"

check-native-deps check-lab-wasm-deps check-tba-crypto-deps check-http-fixture-deps: | build-temporary

check-deps: check-native-deps check-lab-typescript-deps check-tba-typescript-deps check-lab-wasm-deps
	@echo "All dependency checks passed (C development libraries, native TypeScript compilers and WebAssembly runtime)."

# Both Labs use the same compiler probe, but retain independent tool overrides.
check-lab-typescript-deps: TYPESCRIPT_COMPILER = $(LAB_TSC)
check-lab-typescript-deps: TYPESCRIPT_SETTING = LAB_TSC
check-tba-typescript-deps: TYPESCRIPT_COMPILER = $(TBA_TSC)
check-tba-typescript-deps: TYPESCRIPT_SETTING = TBA_TSC
check-lab-typescript-deps check-tba-typescript-deps:
	@set -eu; \
	compiler="$(TYPESCRIPT_COMPILER)"; \
	if ! command -v "$$compiler" >/dev/null 2>&1; then \
	    echo "Missing native TypeScript compiler ($(TYPESCRIPT_SETTING)): $$compiler" >&2; \
	    $(MAKE) --no-print-directory typescript-install-help >&2; \
	    exit 1; \
	fi; \
	if ! version=$$("$$compiler" --version 2>&1); then \
	    echo "Unable to run native TypeScript compiler ($(TYPESCRIPT_SETTING)): $$compiler" >&2; \
	    printf '%s\n' "$$version" >&2; \
	    $(MAKE) --no-print-directory typescript-install-help >&2; \
	    exit 1; \
	fi; \
	major=$${version#Version }; major=$${major%%.*}; \
	case "$$major" in ''|*[!0-9]*) major=0 ;; esac; \
	if [ "$$major" -lt 7 ]; then \
	    echo "$(TYPESCRIPT_SETTING) requires the native TypeScript 7 compiler or newer: $$compiler" >&2; \
	    printf '%s\n' "Reported version: $$version" >&2; \
	    $(MAKE) --no-print-directory typescript-install-help >&2; \
	    exit 1; \
	fi; \
	printf '%s\n' "$(TYPESCRIPT_SETTING): $$version"

# Print instructions only: dependency checks never install or download tools.
typescript-install-help:
	@printf '%s\n' \
	    'The Debian/Ubuntu node-typescript package requires Node.js; use the native release instead.' \
	    'Node.js and npm are not required. Installation guide: docs/building.md#native-typescript-compiler' \
	    'Already installed? Use make TSC=/absolute/path/to/tsc, or put its directory on PATH.' \
	    'Official downloads: https://github.com/microsoft/typescript-go/releases/tag/typescript/v7.0.2'; \
	case "$$(uname -s):$$(uname -m)" in \
	    Linux:x86_64|Linux:amd64) \
	        arch=x64; digest=7ecad6f67377e831856367ab062ef394f21506a611405bf8ac0ff039348637d3 ;; \
	    Linux:aarch64|Linux:arm64) \
	        arch=arm64; digest=c83d931ac9dd7549cde6e71246aa9d6a9812843023df3e277fe3b5dcf41dd0ea ;; \
	    *) echo 'Select the archive matching your operating system and processor from the official downloads.'; exit 0 ;; \
	esac; \
	printf '%s\n' \
	    'For a new per-user installation on Debian/Ubuntu, run:' \
	    '  sudo apt install ca-certificates curl tar' \
	    '  (' \
	    '    set -eu' \
	    '    ts_install=$$(mktemp -d)' \
	    "    curl -fL https://github.com/microsoft/typescript-go/releases/download/typescript/v7.0.2/typescript-linux-$$arch.tgz -o \"\$$ts_install/compiler.tgz\"" \
	    "    printf '%s  %s\\n' '$$digest' \"\$$ts_install/compiler.tgz\" | sha256sum -c -" \
	    '    tar -xzf "$$ts_install/compiler.tgz" -C "$$ts_install"' \
	    '    mkdir -p "$$HOME/.local/lib/typescript" "$$HOME/.local/bin"' \
	    '    test ! -e "$$HOME/.local/lib/typescript/7.0.2"' \
	    '    mv -T "$$ts_install/package" "$$HOME/.local/lib/typescript/7.0.2"' \
	    '    ln -sT "$$HOME/.local/lib/typescript/7.0.2/lib/tsc" "$$HOME/.local/bin/tsc"' \
	    '  )' \
	    '  export PATH="$$HOME/.local/bin:$$PATH"' \
	    '  make check-deps'

# Library, installation and maintenance-tool targets do not need TypeScript.
check-native-deps:
	@missing=0; \
	probe_dir=$$(mktemp -d -t mars-check-deps.XXXXXX) || exit 1; \
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
	check_dep "POSIX threads" "pthread.h" "-pthread" "libc6-dev" "pthread_mutex_t m; pthread_mutex_init(&m, 0); pthread_mutex_destroy(&m)"; \
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

check-lab-deps: check-native-deps check-lab-typescript-deps check-lab-wasm-deps
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
	    echo "Install the missing MARS Lab installation/rendering tool(s), then rerun make."; \
	    echo "Debian/Ubuntu command:"; \
	    echo "  sudo apt install$$packages"; \
	    exit 1; \
	fi

# ------------------------------------------------------------
# Installation
# ------------------------------------------------------------
install: check-native-deps check-compliance $(STATIC_LIB) $(SHARED_LIB) $(LEGAL_DOCUMENTS)
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
DEPS     := $(shell find build tests/build tools/mars_lab/build -name '*.d' 2>/dev/null)
-include $(DEPS)

# ------------------------------------------------------------
# Object build rules
# ------------------------------------------------------------
$(BUILD_DIR)/%.o: src/%.c Makefile | build-temporary
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: tests/%.c Makefile $(TEST_SHARED_HEADERS) $(TEST_SUITE_HEADERS) | build-temporary
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/bench/%.o: bench/%.c Makefile | build-temporary
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/scratch/%.o: scratch/%.c Makefile | build-temporary
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
$(TEST_BUILD_DIR)/executor/test_executor: TEST_LINK_FLAGS = -Wl,--wrap=calloc -Wl,--wrap=thread_new

FILE_TEST_WRAPS := malloc calloc realloc strdup read write rename renameat2 unlink fsync fdatasync fchownat \
                   fdopen fdopendir readdir statx fstat fclose close fchmod ftello fseeko ferror
# Keep the comma literal separate from Make function argument separators.
comma := ,
$(TEST_BUILD_DIR)/file/test_file: TEST_LINK_FLAGS = $(foreach fn,$(FILE_TEST_WRAPS),-Wl$(comma)--wrap=$(fn))

# Standalone native protocol fixtures keep malformed peer behaviour outside the library under test.
.PHONY: check-http-fixture-deps
check-http-fixture-deps:
	@probe_dir=$$(mktemp -d -t mars-http-deps.XXXXXX) || exit 1; \
	trap 'rm -f "$$probe_dir/probe"; rmdir "$$probe_dir"' EXIT; \
	trap 'exit 1' HUP INT TERM; \
	if ! printf '%s\n' '#include <openssl/ssl.h>' \
	    'int main(void) { SSL_CTX *ctx = SSL_CTX_new(TLS_server_method()); SSL_CTX_free(ctx); return 0; }' \
	    | $(CC) $(HTTP_FIXTURE_CFLAGS) -x c - -o "$$probe_dir/probe" $(HTTP_FIXTURE_LIBS); then \
	    echo 'Native HTTPS test fixtures require OpenSSL development files (Debian/Ubuntu: libssl-dev).'; \
	    exit 1; \
	fi

$(HTTP_FIXTURE_BIN): $(HTTP_FIXTURE_SRCS) $(wildcard tests/http/fixtures/*.h) $(STATIC_LIB) Makefile | check-http-fixture-deps
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(HTTP_FIXTURE_CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $(HTTP_FIXTURE_SRCS) \
	    $(STATIC_LIB) $(LDLIBS) $(HTTP_FIXTURE_LIBS)

$(TEST_BUILD_DIR)/http/test_http.o $(CHECKS_BUILD_DIR)/fixtures/checks_fixtures.o \
$(CHECKS_BUILD_DIR)/tests/test_checks_readme.o: CFLAGS += -DMARS_HTTP_FIXTURE_PATH='"$(abspath $(HTTP_FIXTURE_BIN))"'
$(TEST_BUILD_DIR)/http/test_http $(CHECKS_TEST_BIN): $(HTTP_FIXTURE_BIN)
$(CHECKS_TEST_BIN): $(BUILD_DIR)/bench/expression/bench_expr_parse

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
$(BENCH_BINS) $(SCRATCH_BINS) \
$(FILE_COVERAGE_OBJS) $(FILE_COVERAGE_DIR)/test_file $(LAB_OBJS) $(LAB_SERVER) $(LAB_TEST_OBJS) \
$(CONFIG_OBJS) $(CONFIG_PROGRAM) $(CONFIG_TEST_OBJS) $(CHECKS_OBJS) $(CHECKS_BIN) $(CHECKS_TEST_OBJS): | check-native-deps

$(FILE_COVERAGE_DIR)/%.o: src/file/%.c include/file-t.h src/file/file_internal.h Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -O0 -g --coverage $(INCLUDES) -c $< -o $@

$(FILE_COVERAGE_DIR)/test_file: $(TEST_BUILD_DIR)/file/test_file $(FILE_COVERAGE_OBJS)
	$(CC) --coverage $(foreach fn,$(FILE_TEST_WRAPS),-Wl$(comma)--wrap=$(fn)) -o $@ \
	    $(TEST_BUILD_DIR)/file/test_file.o $(TEST_COMMON_HELPER_OBJS) $(FILE_TEST_HELPERS) \
	    $(FILE_COVERAGE_OBJS) $(STATIC_LIB) $(LDLIBS)

.PHONY: coverage-file
coverage-file: $(CHECKS_BIN) $(FILE_COVERAGE_DIR)/test_file
	rm -f $(FILE_COVERAGE_OBJS:.o=.gcda)
	$(FILE_COVERAGE_DIR)/test_file
	cd $(FILE_COVERAGE_DIR) && gcov -b -c -j $(notdir $(FILE_COVERAGE_OBJS))
	"$(CHECKS_BIN)" file-coverage $(FILE_COVERAGE_DIR)

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

README_EXAMPLE_CHECK = "$(CHECKS_BIN)" readme-examples --cc="$(CC)" --cflags="$(CFLAGS)" --libs="$(LDLIBS)" --archive="$(STATIC_LIB)"

.PHONY: test-readme-examples
test-readme-examples: $(STATIC_LIB) $(CHECKS_BIN) $(HTTP_FIXTURE_BIN)
	@$(README_EXAMPLE_CHECK)

test: check-public-distribution $(TEST_BINS)
	@rc=0; "$(CHECKS_BIN)" source-policy || rc=1; for t in $(TEST_BINS); do \
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
$(foreach bin,$(filter-out $(addprefix tests/build/release/almanac/,$(TEST_ALIAS_EXCLUDES)),$(TEST_BINS)),$(eval $(call TEST_ALIAS_RULES,$(notdir $(bin)),$(bin))))

.PHONY: test_almanac memtest_almanac
test_almanac: tests/build/release/almanac/test_almanac $(CONFIG_PROGRAM)
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
	    "$(CONFIG_PROGRAM)" almanac || exit $$?; \
	    exec stdbuf -oL -eL $<; \
	fi; \
	rm -f "$$tmp_out"; \
	exit 1

memtest_almanac: tests/build/release/almanac/test_almanac $(CONFIG_PROGRAM)
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
	    "$(CONFIG_PROGRAM)" almanac || exit $$?; \
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
mars-lab: native-lab
	@"$(LAB_SERVER)" $(ARGS)

mars-lab-stop:
	@pattern='([t]ools/(mars_lab|lab)/build/(release|debug)/mars_lab(_server)?|[b]uild/(release|debug)/tools/mars_lab_server)([[:space:]]|$$)'; \
	if pgrep -u "$$(id -u)" -f "$$pattern" >/dev/null; then \
		pkill -u "$$(id -u)" -f "$$pattern"; status=$$?; \
		if [ "$$status" -gt 1 ]; then echo "Could not stop MARS Lab." >&2; exit "$$status"; fi; \
		attempts=0; \
		while pgrep -u "$$(id -u)" -f "$$pattern" >/dev/null; do \
			if [ "$$attempts" -ge 100 ]; then \
				echo "MARS Lab is still shutting down; restart cancelled. Try again shortly." >&2; exit 1; \
			fi; \
			sleep 0.1; attempts=$$((attempts + 1)); \
		done; \
		echo "Stopped MARS Lab."; \
	else \
		echo "MARS Lab is not running."; \
	fi

mars-lab-restart: mars-lab-stop
	@$(MAKE) --no-print-directory mars-lab

.PHONY: to-be-announced-lab
to-be-announced-lab: native-tba-lab
	@"$(TBA_SERVER)" $(ARGS)

.PHONY: install-globe-db update-globe-db check-globe-db uninstall-globe-db
.PHONY: install-mars-db uninstall-mars-db
.PHONY: configure-mars
configure-mars: native-config
	@echo "Preparing encrypted MARS settings; the desktop key store may request an unlock."
	@"$(CONFIG_PROGRAM)" paths init
	@"$(CONFIG_PROGRAM)" settings migrate
	@echo "MARS configuration ready: mars.json locates encrypted mars.db; existing database paths and settings preserved."

.PHONY: update-mars-db
update-mars-db: check-native-deps native-config
	@"$(CONFIG_PROGRAM)" databases-ensure

install-mars-db: check-native-deps native-config
	@"$(CONFIG_PROGRAM)" databases

uninstall-mars-db: uninstall-globe-db uninstall-almanac-db uninstall-jurisdiction-db

install-globe-db: check-native-deps native-config
	@"$(CONFIG_PROGRAM)" globe-install

check-globe-db: check-native-deps native-config
	@"$(CONFIG_PROGRAM)" globe-check

update-globe-db: check-native-deps native-config
	@"$(CONFIG_PROGRAM)" globe-update

uninstall-globe-db:
	rm -f "$(HOME)/.mars/globe/globe.db" \
	    "$(HOME)/.mars/globe/globe.db-journal" \
	    "$(HOME)/.mars/globe/globe.db-wal" \
	    "$(HOME)/.mars/globe/globe.db-shm" \
	    "$(HOME)/.mars/config/globe-db.env"

install-almanac-db: check-native-deps native-config $(ALMANAC_RULES_SOURCES)
	@"$(CONFIG_PROGRAM)" almanac

uninstall-almanac-db:
	rm -f "$(HOME)/.mars/almanac/almanac.db" \
	    "$(HOME)/.mars/almanac/almanac.db-journal" \
	    "$(HOME)/.mars/almanac/almanac.db-wal" \
	    "$(HOME)/.mars/almanac/almanac.db-shm" \
	    "$(HOME)/.mars/config/almanac-db.env"

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
install-jurisdiction-db: check-native-deps native-config $(JURISDICTION_RULES_SOURCES)
	@"$(CONFIG_PROGRAM)" jurisdiction

uninstall-jurisdiction-db:
	rm -f "$(HOME)/.mars/jurisdiction/mars_jurisdiction_rules.db" \
	    "$(HOME)/.mars/jurisdiction/mars_jurisdiction_rules.db-journal" \
	    "$(HOME)/.mars/jurisdiction/mars_jurisdiction_rules.db-wal" \
	    "$(HOME)/.mars/jurisdiction/mars_jurisdiction_rules.db-shm" \
	    "$(HOME)/.mars/config/jurisdiction-db.env"

install-mars-lab: export MARS_CALENDAR_LIBRARY = $(abspath $(SHARED_LIB))
install-mars-lab: native-lab native-config check-lab-deps $(SHARED_LIB) $(JURISDICTION_RULES_SOURCES) packaging/linux/mars-lab.desktop.in packaging/linux/mars-lab.svg $(MARS_LAB_ICON_CONCEPTS)
	@echo "MARS Lab install [1/4]: installing desktop launcher and icons"
	$(INSTALL) -d "$(MARS_LAB_BINDIR)" "$(MARS_LAB_APPDIR)" "$(MARS_LAB_ICONDIR)"
	rm -f "$(MARS_LAB_BINDIR)/mars-expr-lab" "$(MARS_LAB_APPDIR)/mars-expr-lab.desktop" "$(MARS_LAB_ICONDIR)/mars-expr-lab.svg" "$(MARS_LAB_ICONDIR)"/mars-expr-lab-*.svg
	@printf '%s\n' \
		'#!/bin/sh' \
		'log_dir="$${XDG_STATE_HOME:-$$HOME/.local/state}/mars-lab"' \
		'mkdir -p "$$log_dir"' \
		'printf "[%s] launch %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$0 $$*" >> "$$log_dir/launcher.log" 2>&1' \
		'open_browser=1' \
		'for arg in "$$@"; do [ "$$arg" = "--no-browser" ] && open_browser=0; done' \
		'unit="mars-lab-$$(date +%s%N)"' \
		'systemd-run --user --collect --unit="$$unit" --setenv=MARS_CONFIG_FILE="$${MARS_CONFIG_FILE:-$${MARS_HOME:-$$HOME/.mars}/config/mars.json}" "$(CURDIR)/$(LAB_SERVER)" --host :: --port 8765 --no-browser "$$@" >> "$$log_dir/launcher.log" 2>&1' \
		'status=$$?' \
		'printf "[%s] systemd-run exit %s unit %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$status" "$$unit" >> "$$log_dir/launcher.log" 2>&1' \
		'if [ "$$status" -eq 0 ] && [ "$$open_browser" -eq 1 ]; then' \
		'  sleep 0.8' \
		'  lab_url="http://localhost:8765/"' \
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
	@echo "MARS Lab install [2/4]: refreshing desktop caches"
	@if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$(MARS_LAB_APPDIR)" >/dev/null 2>&1 || true; fi
	@if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache "$(MARS_LAB_INSTALL_PREFIX)/share/icons/hicolor" >/dev/null 2>&1 || true; fi
	@if command -v kbuildsycoca6 >/dev/null 2>&1; then kbuildsycoca6 >/dev/null 2>&1 || true; elif command -v kbuildsycoca5 >/dev/null 2>&1; then kbuildsycoca5 >/dev/null 2>&1 || true; fi
	@echo "MARS Lab install [3/4]: building jurisdiction database (live progress below)"
	@$(MAKE) install-jurisdiction-db
	@echo "MARS Lab install [4/4]: configuring weather (may request input)"
	@"$(CONFIG_PROGRAM)" weather
	@$(MAKE) --no-print-directory configure-mars
	@echo "Installed MARS Lab desktop launcher:"
	@echo "  $(MARS_LAB_DESKTOP)"

install-to-be-announced-lab: native-tba-lab packaging/linux/to-be-announced-lab.desktop.in packaging/linux/to-be-announced-lab.svg
	$(INSTALL) -d "$(MARS_LAB_BINDIR)" "$(MARS_LAB_APPDIR)" "$(MARS_LAB_ICONDIR)"
	@printf '%s\n' \
		'#!/bin/sh' \
		'log_dir="$${XDG_STATE_HOME:-$$HOME/.local/state}/to-be-announced-lab"' \
		'mkdir -p "$$log_dir"' \
		'printf "[%s] launch %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$0 $$*" >> "$$log_dir/launcher.log" 2>&1' \
		'open_browser=1' \
		'for arg in "$$@"; do [ "$$arg" = "--no-browser" ] && open_browser=0; done' \
		'unit="to-be-announced-lab-$$(date +%s%N)"' \
		'systemd-run --user --collect --unit="$$unit" --setenv=MARS_CONFIG_FILE="$${MARS_CONFIG_FILE:-$${MARS_HOME:-$$HOME/.mars}/config/mars.json}" "$(CURDIR)/$(TBA_SERVER)" --host :: --port 8766 --no-browser "$$@" >> "$$log_dir/launcher.log" 2>&1' \
		'status=$$?' \
		'printf "[%s] systemd-run exit %s unit %s\n" "$$(date "+%Y-%m-%d %H:%M:%S")" "$$status" "$$unit" >> "$$log_dir/launcher.log" 2>&1' \
		'if [ "$$status" -eq 0 ] && [ "$$open_browser" -eq 1 ]; then' \
		'  sleep 0.8' \
		'  lab_url="http://localhost:8766/to-be-announced/"' \
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
	@$(MAKE) --no-print-directory configure-mars
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

# ------------------------------------------------------------
# Help
# ------------------------------------------------------------
help:
	@printf '\n%s\n' "Build and cleanup"
	@printf '  %-40s %s\n' "make debug" "Build debug binaries and tests"
	@printf '  %-40s %s\n' "make release" "Build all standard targets in release mode (libraries, Labs, tests and benchmarks)"
	@echo "    If MARS Lab is installed, also install missing globe, almanac and jurisdiction databases."
	@echo "    Creates or migrates encrypted mars.db first; the desktop key store may prompt for an unlock."
	@echo "    Existing databases receive pending updates; their other data and settings are preserved."
	@printf '  %-40s %s\n' "make release-evidence" "Build a release and record its binary dependency evidence"
	@printf '  %-40s %s\n' "make native-lab" "Build the Lab server and WebAssembly assets"
	@printf '  %-40s %s\n' "make native-config" "Build tools/mars_config/build/<mode>/mars_config"
	@printf '  %-40s %s\n' "make native-checks" "Build tools/mars_checks/build/<mode>/mars_checks"
	@printf '  %-40s %s\n' "make lab-wasm-asm" "Generate optional Lab WebAssembly assembly listings"
	@printf '  %-40s %s\n' "make scratch" "Build optional standalone examples (not built by all)"
	@printf '  %-40s %s\n' "make clean" "Remove all build artefacts"
	@printf '\n%s\n' "MARS Lab"
	@printf '  %-40s %s\n' "make mars-lab" "Launch the local MARS Lab"
	@printf '  %-40s %s\n' "make mars-lab-stop" "Stop the local MARS Lab"
	@printf '  %-40s %s\n' "make mars-lab-restart" "Stop and relaunch the local MARS Lab"
	@printf '  %-40s %s\n' "make install-mars-lab" "Install a user desktop launcher for MARS Lab"
	@printf '  %-40s %s\n' "make uninstall-mars-lab" "Remove the user desktop launcher for MARS Lab"
	@printf '  %-40s %s\n' "make configure-mars" "Create tiny mars.json and encrypted mars.db; migrate existing settings using the desktop key store"
	@printf '\n%s\n' "Databases — all three"
	@printf '  %-40s %s\n' "make install-mars-db" "Install globe, almanac and jurisdiction databases sequentially"
	@printf '  %-40s %s\n' "make update-mars-db" "Install missing databases and update existing ones using saved settings"
	@printf '  %-40s %s\n' "make uninstall-mars-db" "Remove all three default databases and their configuration; preserve Lab history"
	@echo "    Stop the Lab before uninstalling. History and weather settings are preserved."
	@printf '\n%s\n' "Databases — individual"
	@printf '  %-40s %s\n' "make install-globe-db" "Build a fresh globe database with geography and photographic textures"
	@printf '  %-40s %s\n' "make update-globe-db" "Update packaged geography and textures, preserving river data"
	@printf '  %-40s %s\n' "make check-globe-db" "Verify globe integrity, shoreline availability and photographic textures"
	@printf '  %-40s %s\n' "make uninstall-globe-db" "Remove the default globe database and its configuration"
	@printf '  %-40s %s\n' "make install-almanac-db" "Build and configure the Almanac database only"
	@printf '  %-40s %s\n' "make uninstall-almanac-db" "Remove the default Almanac database and its configuration"
	@printf '  %-40s %s\n' "make install-jurisdiction-db [town]" "Build the private database and calendar_local for a town"
	@echo "    Use LOCATION='town, jurisdiction' for a qualified or multiword town."
	@echo "    Add CALENDAR_LANGUAGE=code to preselect a calendar language (also works non-interactively)"
	@printf '  %-40s %s\n' "make uninstall-jurisdiction-db" "Remove only the default jurisdiction database and its configuration"
	@printf '\n%s\n' "Tests and benchmarks"
	@printf '  %-40s %s\n' "make test" "Run all tests"
	@printf '  %-40s %s\n' "make test_<name>" "Build and run a single test (e.g. make test_expression)"
	@printf '  %-40s %s\n' "make test_lab_native" "Run native Lab tests"
	@printf '  %-40s %s\n' "make lab-browser-test" "Run Lab browser tests"
	@printf '  %-40s %s\n' "make test_http_live" "Test the HTTP client against the native loopback web server"
	@printf '  %-40s %s\n' "make test_webserver" "Run offline Linux web server tests and its README example"
	@printf '  %-40s %s\n' "make test-readme-examples" "Compile/run complete Markdown C examples"
	@printf '  %-40s %s\n' "make memtest" "Run all tests under valgrind"
	@printf '  %-40s %s\n' "make memtest_<name>" "Build and run a single test under valgrind"
	@printf '  %-40s %s\n' "make bench_<name>" "Build and run a benchmark (e.g. make bench_integrator)"
	@printf '\n%s\n' "Dependencies"
	@printf '  %-40s %s\n' "make check-deps" "Check development libraries and native TypeScript compilers"
	@printf '  %-40s %s\n' "make check-native-deps" "Check development libraries without browser build tools"
	@printf '  %-40s %s\n' "make check-lab-deps" "Check development libraries and MARS Lab TypeScript/TeX/WebAssembly tools"
	@printf '  %-40s %s\n' "make check-lab-wasm-deps" "Probe Clang's WebAssembly target and the LLVM linker"
	@printf '  %-40s %s\n' "make typescript-install-help" "Print native compiler installation commands without installing anything"
	@printf '\n%s\n' "Source and distribution checks"
	@printf '  %-40s %s\n' "make check-compliance" "Verify public-path, notice, SPDX and provenance safeguards"
	@printf '  %-40s %s\n' "make check-source-policy" "Audit inline sizes, expression registries and absence of Python"
	@printf '\n%s\n' "Library installation"
	@printf '  %-40s %s\n' "make install" "Install libraries and headers under PREFIX (default /usr/local)"
	@printf '  %-40s %s\n' "make uninstall" "Remove installed libraries and headers from PREFIX"
	@printf '%s\n' \
	    '    PREFIX is a Make variable: the installation base directory (default /usr/local).' \
	    '    Libraries go in PREFIX/lib; C headers go in PREFIX/include/mars.' \
	    '    Set it for one command: make install PREFIX="$$HOME/.local"' \
	    '    Or set an environment variable: export PREFIX="$$HOME/.local", then run make install.' \
	    '    A PREFIX value on the make command line overrides the environment variable.' \
	    '    Use the same value to remove it: make uninstall PREFIX="$$HOME/.local"' \
	    '    PREFIX controls library installation, not MARS Lab or its databases.'
	@printf '\n%s\n' "Private Lab"
	@printf '  %-40s %s\n' "make to-be-announced-lab" "Launch the local To-Be-Announced Lab"
	@printf '  %-40s %s\n' "make install-to-be-announced-lab" "Install a user desktop launcher for To-Be-Announced Lab"
	@printf '  %-40s %s\n' "make uninstall-to-be-announced-lab" "Remove the user desktop launcher for To-Be-Announced Lab"
	@echo ""
	@echo "Builds and tests default to release; add DEBUG=1 for debug (e.g. make DEBUG=1 test_expression)."

# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------
clean: clean-lab clean-tba-lab clean-config clean-checks
	rm -rf build tests/build
