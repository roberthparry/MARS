# Standalone entry point and root-included native build rules.
ifeq ($(abspath $(firstword $(MAKEFILE_LIST))),$(abspath $(lastword $(MAKEFILE_LIST))))
.DEFAULT_GOAL := all
MARS_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEBUG ?= 0
.PHONY: all run test browser-test typescript typecheck clean
typescript:
	+$(MAKE) -C "$(MARS_ROOT)" DEBUG=$(DEBUG) tba-typescript
typecheck:
	+$(MAKE) -C "$(MARS_ROOT)" DEBUG=$(DEBUG) tba-typecheck
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
TBA_TS_CONFIG := tools/to_be_announced_lab/tsconfig.json
TBA_TEST_TS_CONFIG := tools/to_be_announced_lab/tsconfig.tests.json
TBA_TEST_TS_CHECKS := tools/to_be_announced_lab/build/typescript_checks/tests/browser/checks.js

# Browser and native transport assets belong exclusively to this application.
TBA_TSC ?= $(TSC)
TBA_WASM_CC ?= clang
TBA_WASM_LD ?= wasm-ld
.PHONY: tba-typescript tba-test-typescript tba-typecheck
tba-typescript: check-tba-typescript-deps
	"$(TBA_TSC)" --project $(TBA_TS_CONFIG)

tba-test-typescript: tba-typescript
	"$(TBA_TSC)" --project $(TBA_TEST_TS_CONFIG)

tba-typecheck: check-tba-typescript-deps
	"$(TBA_TSC)" --project $(TBA_TEST_TS_CONFIG) --noEmit

TBA_SRCS := $(sort $(wildcard tools/to_be_announced_lab/src/*/*.c))
TBA_OBJS := $(patsubst tools/to_be_announced_lab/src/%.c,$(TBA_BUILD_DIR)/%.o,$(TBA_SRCS))
TBA_SERVER := $(TBA_BUILD_DIR)/to_be_announced_lab
TBA_CRYPTO_CFLAGS := $(shell pkg-config --cflags libcrypto 2>/dev/null)
TBA_CRYPTO_LIBS := $(shell pkg-config --libs libcrypto 2>/dev/null)
ifeq ($(strip $(TBA_CRYPTO_LIBS)),)
TBA_CRYPTO_LIBS := -lcrypto
endif
.PHONY: check-tba-crypto-deps
check-tba-crypto-deps:
	@probe_dir=$$(mktemp -d /tmp/mars-tba-crypto.XXXXXX) || exit 1; \
	trap 'rm -f "$$probe_dir/probe"; rmdir "$$probe_dir"' EXIT; \
	trap 'exit 1' HUP INT TERM; \
	if ! printf '%s\n' '#include <openssl/evp.h>' '#include <openssl/rsa.h>' \
	    'int main(void) { EVP_PKEY *k = EVP_RSA_gen(2048); EVP_PKEY_free(k); return 0; }' \
	    | $(CC) $(TBA_CRYPTO_CFLAGS) -x c - -o "$$probe_dir/probe" $(TBA_CRYPTO_LIBS); then \
	    echo 'Private Lab encryption requires OpenSSL 3 development files (Debian/Ubuntu: libssl-dev).'; exit 1; \
	fi
$(TBA_BUILD_DIR)/stream/tba_stream.o: CFLAGS += $(TBA_CRYPTO_CFLAGS)
$(TBA_BUILD_DIR)/stream/tba_stream.o: | check-tba-crypto-deps
TBA_TEST_BIN := $(TBA_BUILD_DIR)/tests/test_tba_native
TBA_TEST_SUPPORT := tools/to_be_announced_lab/tests/test_tba_support.c
TBA_STREAM_TEST := tools/to_be_announced_lab/tests/test_tba_stream.c
TBA_TEST_HEADERS := tools/to_be_announced_lab/tests/test_tba_support.h $(wildcard tools/to_be_announced_lab/wasm/include/*.h)
TBA_WASM := $(TBA_BUILD_DIR)/wasm/tba_browser.wasm
TBA_WASM_SRCS := $(sort $(wildcard tools/to_be_announced_lab/wasm/src/*.c))
TBA_WASM_POLICY_SRCS := $(addprefix tools/to_be_announced_lab/wasm/src/,tba_chart.c tba_model.c tba_period.c)
TBA_WASM_OBJS := $(patsubst tools/to_be_announced_lab/wasm/src/%.c,$(TBA_BUILD_DIR)/wasm/%.o,$(TBA_WASM_SRCS))
TBA_CODEC_WASM := $(TBA_BUILD_DIR)/wasm/tba_codec.wasm
TBA_CODEC_OBJS := $(addprefix $(TBA_BUILD_DIR)/wasm/,tba_browser.o tba_memory.o tba_jobs.o tba_job_worker.o)
$(TBA_BUILD_DIR)/wasm/%.o: tools/to_be_announced_lab/wasm/src/%.c tools/to_be_announced_lab/Makefile | check-tba-wasm-deps
	@mkdir -p $(dir $@)
	"$(TBA_WASM_CC)" --target=wasm32-unknown-unknown -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	    -nostdlib -Wmissing-prototypes -MMD -MP -MF $(@:.o=.d) -Itools/to_be_announced_lab/wasm/include -c $< -o $@
$(TBA_WASM): $(TBA_WASM_OBJS) tools/to_be_announced_lab/wasm/tba_browser.imports tools/to_be_announced_lab/Makefile
	"$(TBA_WASM_LD)" --no-entry --export-all --allow-undefined-file=tools/to_be_announced_lab/wasm/tba_browser.imports \
	    --initial-memory=33554432 --max-memory=33554432 $(TBA_WASM_OBJS) -o $@
$(TBA_CODEC_WASM): $(TBA_CODEC_OBJS) tools/to_be_announced_lab/wasm/tba_codec.imports tools/to_be_announced_lab/Makefile
	"$(TBA_WASM_LD)" --no-entry --export-all --allow-undefined-file=tools/to_be_announced_lab/wasm/tba_codec.imports \
	    --initial-memory=33554432 --max-memory=33554432 $(TBA_CODEC_OBJS) -o $@
.PHONY: check-tba-wasm-deps
check-tba-wasm-deps:
	@command -v "$(TBA_WASM_CC)" >/dev/null && command -v "$(TBA_WASM_LD)" >/dev/null || { \
	    echo "Private Lab requires Clang and the LLVM WebAssembly linker." >&2; exit 1; }
$(TBA_BUILD_DIR)/server/tba_server.o: CFLAGS += -DTBA_WASM_PATH='"$(abspath $(TBA_WASM))"'
$(TBA_BUILD_DIR)/server/tba_server.o: CFLAGS += -DTBA_CODEC_PATH='"$(abspath $(TBA_CODEC_WASM))"'
$(TBA_BUILD_DIR)/%.o: tools/to_be_announced_lab/src/%.c tools/to_be_announced_lab/Makefile Makefile | check-native-deps
	@mkdir -p $(dir $@) $(dir $@).deps
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include \
	    -DTBA_ROOT='"$(CURDIR)"' -DTBA_SERVER_PATH='"$(abspath $(TBA_SERVER))"' -c $< -o $@
$(TBA_SERVER): $(TBA_OBJS) $(STATIC_LIB) $(TBA_WASM) $(TBA_CODEC_WASM) | tba-typescript
	$(CC) $(LDFLAGS) -o $@ $(TBA_OBJS) $(STATIC_LIB) $(LDLIBS) $(TBA_CRYPTO_LIBS)
.PHONY: native-tba-lab clean-tba-lab
native-tba-lab: $(TBA_SERVER)
clean-tba-lab:
	rm -rf tools/to_be_announced_lab/build
$(TBA_BUILD_DIR)/tests/test_tba_native: tools/to_be_announced_lab/tests/test_tba_native.c $(TBA_STREAM_TEST) $(TBA_TEST_SUPPORT) $(TBA_TEST_HEADERS) $(TBA_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(TBA_WASM) $(TBA_CODEC_WASM) $(TBA_WASM_SRCS) | tba-typescript
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include \
	    -Itools/to_be_announced_lab/wasm/include -Itools/to_be_announced_lab/tests -Wl,--wrap=clock_gettime -o $@ $< $(TBA_WASM_POLICY_SRCS) $(TBA_TEST_SUPPORT) $(TBA_STREAM_TEST) \
	    $(filter-out $(TBA_BUILD_DIR)/app/tba_main.o,$(TBA_OBJS)) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) $(TBA_CRYPTO_LIBS)
.PHONY: test-tba-native
test-tba-native: $(TBA_BUILD_DIR)/tests/test_tba_native $(TBA_SERVER)
	$<
$(TBA_BUILD_DIR)/tests/test_tba_browser: tools/to_be_announced_lab/tests/browser/test_tba_browser.c tools/to_be_announced_lab/Makefile $(TBA_TEST_SUPPORT) $(TBA_TEST_HEADERS) $(TBA_OBJS) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(TBA_WASM) $(TBA_CODEC_WASM) | tba-test-typescript
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -Itools/to_be_announced_lab/include \
	    -Itools/to_be_announced_lab/tests -DTBA_BROWSER_CHECKS_PATH='"$(abspath $(TBA_TEST_TS_CHECKS))"' \
	    -o $@ $< $(TBA_TEST_SUPPORT) \
	    $(filter-out $(TBA_BUILD_DIR)/app/tba_main.o,$(TBA_OBJS)) $(TEST_COMMON_HELPER_OBJS) $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) $(TBA_CRYPTO_LIBS)
.PHONY: test-tba-browser
test-tba-browser: $(TBA_BUILD_DIR)/tests/test_tba_browser $(TBA_SERVER)
	TMPDIR="$(abspath $(TBA_BUILD_DIR))" TBA_LAB_BROWSER="$(FIREFOX)" $<
-include $(wildcard $(TBA_BUILD_DIR)/*/.deps/*.d)
-include $(TBA_WASM_OBJS:.o=.d)
endif
